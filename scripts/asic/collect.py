#!/usr/bin/env python3
"""Collect frozen standalone ASIC synthesis jobs without affecting running jobs."""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import re


def inspect(path):
    manifest = json.loads((path / "manifest.json").read_text())
    row = {"top": manifest["top"], "state": "running"}
    if not (path / "exit").exists():
        return row
    code = int((path / "exit").read_text())
    row.update(state="failed", exit=code)
    if code or not (path / "out/SUCCESS").exists():
        return row
    for line in (path / "inputs.sha256").read_text().splitlines():
        expected, name = line.split("  ", 1)
        if hashlib.sha256((path / name).read_bytes()).hexdigest() != expected:
            row["error"] = f"Changed frozen input: {name}"
            return row
    with (path / "out/result.tsv").open() as handle:
        area = next(csv.DictReader(handle, delimiter="\t"))
    if area["top"] != manifest["top"]:
        row["error"] = "Mapped top differs from frozen manifest"
        return row
    timing = (path / "out/timing.rpt").read_text()
    slack = [
        float(value)
        for value in re.findall(r"slack\s+\((?:MET|VIOLATED)\)\s+([-+\d.eE]+)", timing)
    ]
    if not slack:
        row["error"] = "Missing timing slack"
        return row
    row.update(
        state="complete",
        cell_area_library_units=float(area["cell_area_library_units"]),
        leaf_cells=int(area["leaf_cells"]),
        clock_period_ns=float(area["clock_period_ns"]),
        worst_reported_slack_ns=min(slack),
        timing_met=min(slack) >= 0,
        rtl_sha256=manifest["rtl_sha256"],
        mapped_sha256=hashlib.sha256((path / "out/mapped.v").read_bytes()).hexdigest(),
    )
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    rows = [
        inspect(args.root / top)
        for top in ("Gcn", "Vector", "GptNorm", "GptSoftmax", "GptVector")
    ]
    failed = any(row["state"] == "failed" for row in rows)
    complete = all(row["state"] == "complete" for row in rows)
    report = {
        "complete": complete,
        "state": "failed" if failed else "complete" if complete else "running",
        "scope": "Standalone dedicated tops with standard-cell local arrays; no SoC or SRAM-normalized CGRA cost comparison.",
        "records": rows,
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"state": report["state"], "output": str(args.output)}))
    return 1 if failed else 0 if complete else 2


if __name__ == "__main__":
    raise SystemExit(main())
