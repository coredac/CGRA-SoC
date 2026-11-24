#!/usr/bin/env python3
"""Build and test dedicated GPT ASIC kernels against the existing N2 reference."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import block_math as bm
import gpt2_data


def fixtures():
    cases = []

    def add(name, unit, source, expected, extra=(), beta=(), row=0, op=0, div=1):
        for phases in {0: (1, 7), 1: (1, 3), 2: (1,)}[unit]:
            mode = "fused" if phases == 1 else "staged"
            cases.append(
                dict(
                    name=f"{name}-{mode}",
                    unit=unit,
                    source=list(source),
                    expected=list(expected),
                    extra=list(extra),
                    beta=list(beta),
                    row=row,
                    operation=op,
                    divisor=div,
                    phases=phases,
                )
            )

    for run in range(2):
        data, _ = gpt2_data.reference(heads=2, length=2, seed=0, run=run)
        value = {key: list(item[1]) for key, item in data.items()}
        for layer, source in ((1, "x"), (2, "expected_u")):
            for row in range(2):
                part = slice(row * 128, (row + 1) * 128)
                add(
                    f"run{run}-ln{layer}-row{row}",
                    0,
                    value[source][part],
                    value[f"expected_ln{layer}"][part],
                    value[f"gamma{layer}"],
                    value[f"beta{layer}"],
                )
        for row in range(4):
            part = slice(row * 2, (row + 1) * 2)
            add(
                f"run{run}-softmax-row{row}",
                1,
                value["scores"][part],
                value["probability"][part],
                row=row % 2,
            )
        for name, divisor in bm.DIVISORS.items():
            add(
                f"run{run}-requant-{name}",
                2,
                value[f"{name}_accum"],
                value[f"expected_{name}"],
                op=1,
                div=divisor,
            )
        concat = value["expected_concat"]
        pv_expected = [
            concat[row * 128 + head * 64 + column]
            for head in range(2)
            for row in range(2)
            for column in range(64)
        ]
        add(f"run{run}-requant-pv", 2, value["pv_accum"], pv_expected, op=1, div=127)
        add(f"run{run}-gelu", 2, value["expected_fc1"], value["expected_gelu"])
        add(
            f"run{run}-residual1",
            2,
            value["x"],
            value["expected_u"],
            value["expected_attention"],
            op=2,
        )
        add(
            f"run{run}-residual2",
            2,
            value["expected_u"],
            value["expected_y"],
            value["expected_fc2"],
            op=2,
        )
    for name, values in (
        ("constant", [-17] * 128),
        ("full-range", [-128, 127] * 64),
    ):
        gamma, beta = [64] * 128, [-3] * 128
        expected, _, _ = bm.layernorm([values], gamma, beta)
        add(f"arithmetic-ln-{name}", 0, values, expected[0], gamma, beta)
    values = list(range(-128, 128))
    expected, _ = bm.gelu([values])
    add("arithmetic-gelu", 2, values, expected[0])
    values = [
        -2147483648,
        -257,
        -129,
        -128,
        -127,
        -3,
        -2,
        -1,
        0,
        1,
        2,
        3,
        127,
        128,
        129,
        257,
        2147483647,
    ]
    # Preserve INT32 overflow at the new vector pipeline boundaries.
    wrap_input = [
        -2147483648,
        -2147483647,
        -1073741825,
        -1073741824,
        -1073741823,
        -536870913,
        -536870912,
        -536870911,
        -131073,
        -131072,
        -131071,
        -41,
        -40,
        -39,
        -1,
        0,
        1,
        39,
        40,
        41,
        131071,
        131072,
        131073,
        536870911,
        536870912,
        536870913,
        1073741823,
        1073741824,
        1073741825,
        2147483647,
    ]
    wrap_output = [
        0,
        1,
        -1,
        0,
        1,
        -1,
        127,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        -1,
        0,
        1,
        39,
        40,
        41,
        -1,
        0,
        1,
        -1,
        127,
        0,
        -1,
        0,
        1,
        -1,
    ]
    add(
        "arithmetic-gelu-wrap",
        2,
        [wrap_input[i % len(wrap_input)] for i in range(1024)],
        [wrap_output[i % len(wrap_output)] for i in range(1024)],
    )
    add(
        "arithmetic-residual-wrap",
        2,
        [2147483647, -2147483648, 127, -128] * 256,
        [-128, 127, 127, -128] * 256,
        [1, -1, 1, -1] * 256,
        op=2,
    )
    for divisor in (4, 127, 256):
        expected, _ = bm.quantize([values], divisor)
        add(f"arithmetic-requant-{divisor}", 2, values, expected[0], op=1, div=divisor)
    for row in range(2):
        values = [-8192, 8192]
        add(
            f"arithmetic-softmax-row{row}",
            1,
            values,
            bm.probabilities(values, row),
            row=row,
        )
    # Exhaust the clamped exponent domain and both low-bit rounding cases.
    for distance in range(4098):
        for odd in range(2):
            values = [17, 17 - 2 * distance - odd]
            if distance % 2:
                values.reverse()
            add(
                f"arithmetic-softmax-distance{distance}-odd{odd}",
                1,
                values,
                bm.probabilities(values, 1),
                row=1,
            )
    for name, values in (
        ("minimum", [-2147483648, -2147483647]),
        ("maximum", [2147483647, 2147475454]),
        ("signed-span", [-1073741824, 1073741823]),
        ("masked-maximum", [-2147483648, 2147483647]),
    ):
        rows = (0,) if name == "masked-maximum" else (0, 1)
        for row in rows:
            add(
                f"arithmetic-softmax-{name}-row{row}",
                1,
                values,
                bm.probabilities(values, row),
                row=row,
            )
    return cases


def write_fixture(cases, path):
    lines = [str(len(cases))]
    for case in cases:
        lines.append(
            " ".join(
                map(
                    str,
                    (
                        case["name"],
                        case["unit"],
                        len(case["source"]),
                        case["row"],
                        case["operation"],
                        case["divisor"],
                        case["phases"],
                    ),
                )
            )
        )
        for field in ("source", "extra", "beta", "expected"):
            lines.append(" ".join(map(str, case[field])))
    path.write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=ROOT / "build/experiments/asic/gpt"
    )
    parser.add_argument(
        "--verilator", type=Path, default=ROOT / "chipyard/.conda-env/bin/verilator"
    )
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = sorted((ROOT / "rtl/asic/gpt").glob("*.sv"))
    testbench = ROOT / "tests/asic/gpt/main.cpp"
    fixture = out / "fixture.txt"
    cases = fixtures()
    write_fixture(cases, fixture)
    command = [
        str(args.verilator),
        "--cc",
        "--exe",
        "--build",
        "-j",
        "4",
        "--top-module",
        "GptAsic",
        "--Mdir",
        str(out / "obj"),
        "-Wall",
        "-Wno-DECLFILENAME",
        "-Wno-UNUSEDSIGNAL",
        *map(str, source),
        str(testbench),
    ]
    env = dict(os.environ)
    env["PATH"] = str(args.verilator.parent) + os.pathsep + env.get("PATH", "")
    with (out / "build.log").open("w") as log:
        compiled = subprocess.run(
            command, stdout=log, stderr=subprocess.STDOUT, env=env
        )
    if compiled.returncode:
        print(f"Build failed; see {out / 'build.log'}", file=sys.stderr)
        return compiled.returncode
    with (out / "run.log").open("w") as log, (out / "errors.log").open("w") as errors:
        result = subprocess.run(
            [str(out / "obj/VGptAsic"), str(fixture)], stdout=log, stderr=errors
        )
    records = [json.loads(line) for line in (out / "run.log").read_text().splitlines()]
    report = {
        "passed": result.returncode == 0
        and len(records) == len(cases)
        and all(row["passed"] for row in records),
        "exit": result.returncode,
        "shape": {"length": 2, "hidden": 128, "heads": 2, "ff": 512},
        "coverage": {
            "workload_cases": 52,
            "arithmetic_cases": len(cases) - 52,
            "outputs_checked": sum(row["checked"] for row in records),
        },
        "scope": "Standalone dedicated non-Gemmini kernels; not a full GPT or SoC run.",
        "cycles": "Clock edges from command acceptance through done, including one start edge per command; preload/readback excluded.",
        "transfers": "Actual 32-bit fixture-port reads/writes, not measured DMA or DRAM traffic. Staged/fused retain the same local arrays.",
        "limitations": "Variable division uses 32 restoring steps. Technology timing remains to be checked. Local arrays have combinational reads, no SRAM mapping claim.",
        "sha256": {
            (
                str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p)
            ): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [
                *source,
                testbench,
                fixture,
                Path(__file__).resolve(),
                ROOT / "scripts/block_math.py",
                ROOT / "scripts/gpt2_data.py",
                ROOT / "scripts/attention_data.py",
            ]
        },
        "records": records,
    }
    (out / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {"passed": report["passed"], "cases": len(records), "output": str(out)}
        )
    )
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
