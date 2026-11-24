"""Collect complete-core mapping and retain access logic when excluding storage."""

from collections import defaultdict
import csv
from decimal import Decimal
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent
STORAGE = {
    "instructions": r"tile__\d+/ctrl_mem/reg_file/regs_reg\[",
    "constants": r"tile__\d+/const_mem/reg_file/regs_reg\[",
    "routing_prologue": r"tile__\d+/ctrl_mem/prologue_count_reg_routing_crossbar_reg\[",
    "fu_crossbar_prologue": r"tile__\d+/ctrl_mem/prologue_count_reg_fu_crossbar_reg\[",
}
FU = ["add_subtract", "multiply", "divide_remainder_sqrt", "logic", "shift", "compare", "select", "grant", "phi", "pe_load_store", "return", "address_generation", "nop"]
FILES = [
    "area.rpt", "units.rpt", "setup.rpt", "constraints.rpt", "qor.rpt",
    "check.rpt", "timing_check.rpt", "design.rpt", "leaves.tsv", "SUCCESS", "exit", "dc.log",
]


def component(path):
    parts = path.split("/")
    if not parts[0].startswith("tile__"):
        return parts[0] if parts[0] in {"controller", "ctrl_ring", "bypass_queue"} else "core_local_control"
    if len(parts) < 3:
        return "tile_local_control"
    if parts[1].startswith(("tile_in_channel", "tile_out_or_link")):
        return "tile_channels"
    return parts[1] if parts[1] in {"ctrl_mem", "const_mem", "element", "register_cluster", "routing_crossbar", "fu_crossbar"} else "tile_local_control"


def main():
    job = json.loads((ROOT / "job.json").read_text())
    hashes = {
        "source_sha256": hashlib.sha256((ROOT / "source/CgraCompute.v").read_bytes()).hexdigest(),
        "constraints_sha256": hashlib.sha256((ROOT / "synth.tcl").read_bytes()).hexdigest(),
    }
    if any(value != job[key] for key, value in hashes.items()):
        print("Local source or constraints differ from launched job; no result published.")
        return
    receipt = ROOT / "reports.json"
    if not receipt.exists():
        ready = subprocess.run(["ssh", job["host"], f"test -f {job['remote']}/SUCCESS"], capture_output=True)
        if ready.returncode:
            print("Complete mapped result is not available yet.")
            return
        subprocess.run(["scp", *[f"{job['host']}:{job['remote']}/{name}" for name in FILES], str(ROOT)], check=True)
        receipt.write_text(json.dumps({"job": job, "files": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in FILES}}, indent=2) + "\n")
    evidence = json.loads(receipt.read_text())
    if evidence["job"] != job or any(hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != digest for name, digest in evidence["files"].items()):
        print("Report provenance differs from recorded job; no result published.")
        return
    if (ROOT / "exit").read_text().strip() != "0":
        print("Mapping did not exit successfully; no area result published.")
        return
    groups = defaultdict(Decimal)
    functional_units = defaultdict(Decimal)
    storage = defaultdict(lambda: {"cells": 0, "area": Decimal(0)})
    coverage = defaultdict(lambda: defaultdict(int))
    counts = defaultdict(int)
    leaf_total = Decimal(0)
    seen = set()
    matched = []
    with (ROOT / "leaves.tsv").open() as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            path = row["path"]
            if path in seen:
                print(f"Duplicate leaf: {path}; no result published.")
                return
            seen.add(path)
            area = Decimal(row["area"])
            leaf_total += area
            label = next((name for name, pattern in STORAGE.items() if row["sequential"] == "true" and re.match(pattern, path)), None)
            if label:
                storage[label]["cells"] += 1
                storage[label]["area"] += area
                coverage[label][path.split("/")[0]] += 1
                matched.append({**row, "category": label})
                continue
            part = component(path)
            groups[part] += area
            counts[part] += 1
            if part == "element":
                unit = re.match(r"tile__\d+/element/fu__(\d+)/", path)
                label = FU[int(unit[1])] if unit else "fu_selection_and_control"
                functional_units[label] += area
    with (ROOT / "storage.tsv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=["path", "ref", "area", "sequential", "category"], delimiter="\t")
        writer.writeheader()
        writer.writerows(matched)
    qor = (ROOT / "qor.rpt").read_text()
    setup = (ROOT / "setup.rpt").read_text()
    paths = []
    for block in setup.split("  Startpoint:")[1:]:
        paths.append({
            "startpoint": block.splitlines()[0].strip(),
            "endpoint": re.search(r"Endpoint:\s*(.*)", block)[1],
            "slack_ns": float(re.search(r"slack \((?:MET|VIOLATED)\)\s+(-?[\d.]+)", block)[1]),
        })
    data_paths = [path for path in paths if "clock source" not in path["startpoint"]]
    total = Decimal(re.search(r"^\s*Cell Area:\s*([\d.]+)", qor, re.M)[1])
    excluded = sum((row["area"] for row in storage.values()), Decimal(0))
    expected_tiles = {f"tile__{number}" for number in range(16)}
    tolerance = max(Decimal("0.001"), total * Decimal("0.0000001"))
    audit = {
        "report_matches_leaves": abs(total - leaf_total) <= tolerance,
        "area_reconciliation_tolerance": float(tolerance),
        "storage_tile_counts": {name: dict(coverage[name]) for name in STORAGE},
        "storage_covers_all_tiles": all(set(coverage[name]) == expected_tiles for name in STORAGE),
        "mapped_storage_within_declared_bits": all(max(coverage[name].values(), default=0) <= bound for name, bound in [("instructions", 5328), ("constants", 2030), ("routing_prologue", 1152), ("fu_crossbar_prologue", 288)]),
    }
    (ROOT / "audit.json").write_text(json.dumps(audit, indent=2) + "\n")
    if not all(audit[key] for key in ["report_matches_leaves", "storage_covers_all_tiles", "mapped_storage_within_declared_bits"]):
        print("Mapped-cell accounting needs review; see audit.json. No result published.")
        return
    result = {
        "scope": "Complete modified CGRA compute core, external data_mem instance and DMA shell excluded; named static storage cells subtracted after mapping.",
        "complete": True,
        "area_unit": "SAED14 library area units",
        "clock_ns": 2,
        "total_cell_area": float(total),
        "static_storage_area": float(excluded),
        "area_excluding_static_storage": float(total - excluded),
        "leaf_area_sum": float(leaf_total),
        "report_minus_leaf_area": float(total - leaf_total),
        "retained_group_area": {name: float(value) for name, value in groups.items()},
        "retained_group_cell_counts": dict(counts),
        "functional_unit_area": {name: float(value) for name, value in functional_units.items()},
        "static_storage": {name: {"cells": row["cells"], "area": float(row["area"])} for name, row in storage.items()},
        "static_storage_patterns": STORAGE,
        "worst_setup_slack_ns": min(map(float, re.findall(r"slack \((?:MET|VIOLATED)\)\s+(-?[\d.]+)", setup))),
        "violating_setup_paths": int(float(re.search(r"No. of Violating Paths:\s*([\d.]+)", qor)[1])),
        "no_violated_constraints": "no violated constraints" in (ROOT / "constraints.rpt").read_text(),
        "worst_startpoint": re.search(r"Startpoint:\s*(.*)", setup)[1],
        "worst_endpoint": re.search(r"Endpoint:\s*(.*)", setup)[1],
        "reported_setup_paths": paths,
        "worst_reported_data_path": min(data_paths, key=lambda path: path["slack_ns"]),
        "source_sha256": job["source_sha256"],
        "constraints_sha256": job["constraints_sha256"],
        "report_provenance": "reports.json",
        "limits": [
            "Runtime prologue counters, operand registers, pipeline state and queues remain charged.",
            "All instruction/constant read-write selection and native compute routing remain charged.",
            "Ideal-clock wire-load synthesis is not physical signoff.",
            "No simulator or performance rerun; old workload results do not validate the modified control transport.",
            "Raw worst setup includes a forwarded-clock output constrained as data; genuine internal data paths also fail 2 ns. No timing exception or remapping is applied to these frozen results.",
        ],
    }
    (ROOT / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
