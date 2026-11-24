#!/usr/bin/env python3
"""Freeze a standalone ASIC top for the existing SAED14 DC environment."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--top", required=True)
    parser.add_argument("--rtl", nargs="+", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("output must be a new directory")
    if not args.top.isidentifier():
        parser.error("top must be a Verilog module identifier")
    args.output.mkdir(parents=True)
    sources = []
    hashes = {}
    for index, path in enumerate(args.rtl):
        name = f"{index:02d}_{path.name}"
        shutil.copyfile(path, args.output / name)
        sources.append(name)
        hashes[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    shutil.copyfile(Path(__file__).with_name("synth.tcl"), args.output / "synth.tcl")
    (args.output / "job.tcl").write_text(
        f"set top {{{args.top}}}\n"
        + "set source_list [list "
        + " ".join("{" + name + "}" for name in sources)
        + "]\n"
    )
    (args.output / "manifest.json").write_text(
        json.dumps(
            {
                "top": args.top,
                "rtl_sha256": hashes,
                "clock_period_ns": 10,
                "scope": "Standalone kernel, including declared local storage; no SoC DMA or interconnect.",
                "storage": "RTL arrays mapped to standard cells; no SRAM macro substitution.",
                "command": "/tmp/cgra-area-20261030/dc.sh -f synth.tcl",
            },
            indent=2,
        )
        + "\n"
    )
    files = sorted(path for path in args.output.iterdir() if path.is_file())
    (args.output / "inputs.sha256").write_text(
        "".join(
            f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n"
            for path in files
        )
    )
    print(args.output)


if __name__ == "__main__":
    main()
