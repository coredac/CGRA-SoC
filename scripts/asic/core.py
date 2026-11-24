#!/usr/bin/env python3
"""Check the ASIC local-memory bridge with the existing small workload fixtures."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import gpt
import vector

ROOT = Path(__file__).resolve().parents[2]
FAMILIES = {"all": -1, "gcn": 0, "norm": 2, "softmax": 3, "gpt_vector": 4}


def cases():
    result = []

    def add(name, unit, inputs, expected, **fields):
        record = dict(
            name=name,
            unit=unit,
            operation=0,
            fused=1,
            phase=0,
            count=len(expected),
            rows=0,
            edges=0,
            divisor=1,
            src0=0,
            src1=4096,
            src2=8192,
            src3=12288,
            dst=16384,
            inputs=inputs,
            expected=expected,
        )
        record.update(fields)
        result.append(record)

    for item in gpt.fixtures():
        unit = item["unit"] + 2
        inputs = list(
            zip((0, 4096, 8192), (item["source"], item["extra"], item["beta"]))
        )
        for phase in range(item["phases"]):
            add(
                f"{item['name']}-phase{phase}",
                unit,
                inputs if phase == 0 else [],
                item["expected"] if phase + 1 == item["phases"] else [],
                operation=item["operation"],
                fused=int(item["phases"] == 1),
                phase=phase,
                count=len(item["source"]),
                rows=item["row"],
                divisor=item["divisor"],
            )
    for run, item in enumerate(vector.fixtures()):
        (
            rowptr,
            columns,
            weights,
            features,
            sums,
            activated,
            first,
            relu,
            main,
            skip,
            residual,
        ) = item["arrays"]
        inputs = list(zip((0, 4096, 8192, 12288), (rowptr, columns, weights, features)))
        add(
            f"gcn{run}-fused",
            0,
            inputs,
            activated,
            rows=len(rowptr) - 1,
            edges=len(columns),
        )
        add(
            f"gcn{run}-aggregate",
            0,
            inputs,
            sums,
            fused=0,
            rows=len(rowptr) - 1,
            edges=len(columns),
        )
        add(f"gcn{run}-local-relu", 1, [], activated, src0=16384, dst=16384)
        add(f"resnet{run}-relu", 1, [(0, first)], relu)
        add(f"resnet{run}-fused", 1, [(0, main), (4096, skip)], residual, operation=2)
        add(
            f"resnet{run}-add",
            1,
            [(0, main), (4096, skip)],
            [a + b for a, b in zip(main, skip)],
            operation=1,
        )
        add(f"resnet{run}-local-relu", 1, [], residual, src0=16384, dst=16384)
    add("last-word-relu", 1, [(32767, [-7])], [0], src0=32767, dst=32767)
    return result


def private_cases(family, words):
    layouts = {
        0: (0, 128, 1024, 2048, words - 2048),
        2: (0, 128, 256, 0, words - 128),
        3: (0, 0, 0, 0, words - 2),
        4: (0, 1024, 0, 0, words - 1024),
    }
    addresses = dict(zip((0, 4096, 8192, 12288, 16384), layouts[family]))
    result = []
    for record in cases():
        if record["unit"] != family:
            continue
        record["inputs"] = [
            (addresses[address], values) for address, values in record["inputs"]
        ]
        for field in ("src0", "src1", "src2", "src3", "dst"):
            record[field] = addresses[record[field]]
        # An independent core ignores the runtime unit selector.
        record["unit"] = 7
        result.append(record)
    return result


def fixture_text(records):
    fields = (
        "name",
        "unit",
        "operation",
        "fused",
        "phase",
        "count",
        "rows",
        "edges",
        "divisor",
        "src0",
        "src1",
        "src2",
        "src3",
        "dst",
    )
    lines = [str(len(records))]
    for record in records:
        inputs = [(address, values) for address, values in record["inputs"] if values]
        lines.append(
            " ".join(str(record[field]) for field in fields)
            + f" {len(inputs)} {len(record['expected'])}"
        )
        for address, values in inputs:
            lines.append(f"{address} {len(values)} " + " ".join(map(str, values)))
        lines.append(" ".join(map(str, record["expected"])))
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=ROOT / "build/experiments/asic/core"
    )
    parser.add_argument(
        "--verilator", type=Path, default=ROOT / "chipyard/.conda-env/bin/verilator"
    )
    parser.add_argument("--family", choices=FAMILIES, default="all")
    parser.add_argument("--words", type=int, default=32768)
    parser.add_argument("--sram-model", type=Path)
    args = parser.parse_args()
    minimum = {
        "all": 32768,
        "gcn": 8192,
        "norm": 2048,
        "softmax": 2048,
        "gpt_vector": 4096,
    }
    if (
        args.words < minimum[args.family]
        or args.words > 32768
        or args.words & (args.words - 1)
    ):
        parser.error(
            "words must be a power of two that holds the selected family fixtures, at most 32768"
        )
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    family = FAMILIES[args.family]
    records = cases() if family < 0 else private_cases(family, args.words)
    fixture = output / "fixtures.txt"
    fixture.write_text(fixture_text(records))
    sources = [
        ROOT / "rtl/asic/Core.sv",
        ROOT / "rtl/asic/vector/Gcn.sv",
        ROOT / "rtl/asic/vector/Vector.sv",
        *(
            ROOT / "rtl/asic/gpt" / name
            for name in ("div.sv", "norm.sv", "softmax.sv", "vector.sv")
        ),
        ROOT / "tests/asic/core/main.cpp",
    ]
    defines = []
    if args.sram_model:
        sources.extend([ROOT / "rtl/asic/CoreMemory.sv", args.sram_model.resolve()])
        defines.extend(["+define+ASIC_SRAM", "-Wno-PINCONNECTEMPTY", "-Wno-BLKSEQ"])
    command = [
        str(args.verilator),
        "--cc",
        "--exe",
        "--build",
        "-j",
        "4",
        "-Wall",
        "-Wno-DECLFILENAME",
        "-Wno-UNUSEDSIGNAL",
        f"-GFAMILY={family}",
        f"-GWORDS={args.words}",
        "--top-module",
        "AsicCore",
        "--Mdir",
        str(output / "obj"),
        *defines,
        *map(str, sources),
    ]
    env = dict(os.environ)
    env["PATH"] = str(args.verilator.parent) + os.pathsep + env.get("PATH", "")
    with (output / "build.log").open("w") as log:
        compiled = subprocess.run(
            command, stdout=log, stderr=subprocess.STDOUT, env=env
        )
    if compiled.returncode:
        print(f"Build failed; see {output / 'build.log'}", file=sys.stderr)
        return compiled.returncode
    run_command = [str(output / "obj/VAsicCore"), str(fixture), str(args.words)]
    with (output / "run.log").open("w") as log:
        run = subprocess.run(
            run_command, stdout=log, stderr=subprocess.STDOUT, timeout=120
        )
    rows = [
        json.loads(line)
        for line in (output / "run.log").read_text().splitlines()
        if line.startswith("{")
    ]
    result = {
        "passed": run.returncode == 0
        and len(rows) == len(records)
        and all(row["passed"] for row in rows),
        "exit": run.returncode,
        "scope": "Real ASIC kernel bridge and synchronous local memory. No CPU, AutoLink adapter or TileLink DMA in this component test.",
        "family": args.family,
        "spm_bytes": args.words * 4,
        "sram_model": str(args.sram_model) if args.sram_model else None,
        "cycle_scope": "Accepted core start through core done, including local-to-kernel loads and kernel-to-local output stores. Host preload and checking excluded.",
        "cases": len(rows),
        "checked_outputs": sum(row["checked"] for row in rows),
        "build_command": command,
        "run_command": run_command,
        "sha256": {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in [*sources, Path(__file__).resolve(), fixture]
        },
        "records": rows,
    }
    (output / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps({key: result[key] for key in ("passed", "cases", "checked_outputs")})
    )
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
