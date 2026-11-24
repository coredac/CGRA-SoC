#!/usr/bin/env python3
"""Check two physically separate Add and ReLU ASIC cores and their memory copy."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=ROOT / "build/experiments/asic/fixed"
    )
    parser.add_argument(
        "--verilator", type=Path, default=ROOT / "chipyard/.conda-env/bin/verilator"
    )
    parser.add_argument(
        "--sram",
        action="store_true",
        help="Use the physical SRAM interface functional model",
    )
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    sources = [
        ROOT / "rtl/asic/Fixed.sv",
        ROOT / "rtl/asic/FixedMemory.sv",
        ROOT / "tests/asic/fixed/Bench.sv",
        ROOT / "tests/asic/fixed/main.cpp",
    ]
    if args.sram:
        sources.append(ROOT / "tests/asic/fixed/Sram.sv")
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
        "--top-module",
        "FixedBench",
        "--Mdir",
        str(output / "obj"),
        *(["-DASIC_SRAM"] if args.sram else []),
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
    with (output / "run.log").open("w") as log:
        run = subprocess.run(
            [str(output / "obj/VFixedBench")],
            stdout=log,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
    rows = [
        json.loads(line)
        for line in (output / "run.log").read_text().splitlines()
        if line.startswith("{")
    ]
    result = {
        "passed": run.returncode == 0 and len(rows) == 30,
        "exit": run.returncode,
        "physical_sram_interface": args.sram,
        "scope": "Two independent fixed-function cores, each with 64 KiB synchronous local RAM. Testbench performs the actual inter-core read/write copy; no CPU, adapter or TileLink DMA is instantiated.",
        "cycle_scope": "Core start through done separately from actual fixture-port copy cycles. Host preload, reference checks and final reads excluded from compute intervals.",
        "cases": len(rows),
        "checked_outputs": sum(row["checked"] for row in rows),
        "build_command": command,
        "sha256": {
            str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in [*sources, Path(__file__).resolve()]
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
