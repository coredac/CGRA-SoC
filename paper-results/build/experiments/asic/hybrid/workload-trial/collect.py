"""Audit SAD mapped cells and merge an optional seventh endpoint scenario."""

from collections import defaultdict
import csv
from decimal import Decimal
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent
FILES = ["area.rpt", "units.rpt", "setup.rpt", "constraints.rpt", "qor.rpt", "check.rpt", "timing_check.rpt", "design.rpt", "leaves.tsv", "SUCCESS", "exit", "started", "finished", "dc.log", "inputs.sha256"]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    job = json.loads((ROOT / "job.json").read_text())
    folder = ROOT / "sad"
    receipt = folder / "reports.json"
    if not receipt.exists():
        ready = subprocess.run(["ssh", job["host"], f"test -f {job['remote']}/exit"], capture_output=True)
        if ready.returncode:
            print("SAD mapping is still running.")
            return
        subprocess.run(["scp", *[f"{job['host']}:{job['remote']}/{name}" for name in FILES], str(folder)], check=True)
        receipt.write_text(json.dumps({"job": job, "files": {name: digest(folder / name) for name in FILES}}, indent=2) + "\n")
    evidence = json.loads(receipt.read_text())
    inputs = {line.split()[1]: line.split()[0] for line in (folder / "inputs.sha256").read_text().splitlines()}
    audit = {
        "exit_ok": (folder / "exit").read_text().strip() == "0",
        "source_matches": digest(folder / "source/all.v") == job["source_sha256"] == inputs["all.v"],
        "constraints_match": digest(folder / "synth.tcl") == job["constraints_sha256"] == inputs["synth.tcl"],
        "reports_match": evidence["job"] == job and all(digest(folder / name) == value for name, value in evidence["files"].items()),
        "timing_check_clean": not re.search(r"Warning:|Error:", (folder / "timing_check.rpt").read_text()),
    }
    groups = defaultdict(Decimal)
    seen = set()
    duplicate = False
    sequential = 0
    with (folder / "leaves.tsv").open() as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            duplicate |= row["path"] in seen
            seen.add(row["path"])
            group = "primitive_bank" if row["path"].startswith("primitives/") else "native_control_and_operand_state"
            groups[group] += Decimal(row["area"])
            sequential += row["sequential"] == "true"
    qor = (folder / "qor.rpt").read_text()
    total = Decimal(re.search(r"^\s*Cell Area:\s*([\d.]+)", qor, re.M)[1])
    leaf_total = sum(groups.values())
    audit["unique_leaves"] = not duplicate
    audit["leaf_area_reconciles"] = abs(total - leaf_total) <= max(Decimal("0.001"), total * Decimal("0.0000001"))
    (folder / "audit.json").write_text(json.dumps(audit, indent=2) + "\n")
    if not all(audit.values()):
        print("SAD evidence audit failed; see sad/audit.json.")
        return
    setup = (folder / "setup.rpt").read_text()
    result = {
        "complete": True,
        "total_cell_area": float(total),
        "local_array_storage_area": 0,
        "area_excluding_local_array_storage": float(total),
        "storage_rule": "No bulk local array exists. All operand, accumulator, pipeline and controller registers remain counted.",
        "group_area": {name: float(value) for name, value in groups.items()},
        "sequential_cell_count": sequential,
        "leaf_area_sum": float(leaf_total),
        "worst_setup_slack_ns": min(map(float, re.findall(r"slack \((?:MET|VIOLATED)\)\s+(-?[\d.]+)", setup))),
        "violating_setup_paths": int(float(re.search(r"No. of Violating Paths:\s*([\d.]+)", qor)[1])),
        "no_violated_constraints": "no violated constraints" in (folder / "constraints.rpt").read_text(),
        "source_sha256": job["source_sha256"],
        "constraints_sha256": job["constraints_sha256"],
        "evidence_audit": audit,
    }
    (folder / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    baseline_path = ROOT.parent / "revised-area/results.json"
    baseline = json.loads(baseline_path.read_text())
    families_path = ROOT.parent / "revised-area/asic/results.json"
    fixed_path = ROOT.parent / "revised-area/gpt-fix/mapped/results.json"
    families = json.loads(families_path.read_text())["families"]
    families["gpt_vector"] = json.loads(fixed_path.read_text())
    families["sad"] = result
    combined = baseline["asic_sum_including_local_storage"] + result["total_cell_area"]
    retained = baseline["asic_sum_excluding_local_storage"] + result["area_excluding_local_array_storage"]
    cgra = baseline["cgra_including_local_storage"]
    summary = {
        "area_unit": "SAED14 library area units",
        "clock_ns": 2,
        "baseline_sha256": digest(baseline_path),
        "family_report_sha256": digest(families_path),
        "selected_gpt_report_sha256": digest(fixed_path),
        "families": {name: {key: row[key] for key in ["total_cell_area", "area_excluding_local_array_storage", "worst_setup_slack_ns", "no_violated_constraints", "source_sha256"]} for name, row in families.items()},
        "cgra_including_local_storage": cgra,
        "six_asic_including_local_storage": baseline["asic_sum_including_local_storage"],
        "sad_including_local_storage": result["total_cell_area"],
        "seven_asic_including_local_storage": combined,
        "seven_asic_excluding_local_storage": retained,
        "cgra_area_saving_vs_seven_percent": 100 * (1 - cgra / combined),
        "cgra_area_saving_vs_six_percent": 100 * (1 - cgra / baseline["asic_sum_including_local_storage"]),
        "sad_setup_slack_ns": result["worst_setup_slack_ns"],
        "sad_constraints_met": result["no_violated_constraints"],
        "cgra_internal_setup_slack_ns": baseline["cgra_worst_reported_data_path"]["slack_ns"],
        "limits": [
            "Seventh ASIC is optional independent autonomous SAD provisioning; existing primitive interfaces already contain the arithmetic needed for software sequencing.",
            "Seven expanded endpoints install 112 primitive lanes versus 16 CGRA PEs; equal useful throughput is not established.",
            "CGRA area reuses the unchanged modified core; SAD mapping on that modified control network is not newly validated.",
            "No simulator or performance run; synthesis and local static/formal checks are not whole-system validation.",
            "CGRA still fails 2 ns. This is an area scenario, not timing-qualified performance per area.",
        ],
    }
    (ROOT / "results.json").write_text(json.dumps(summary, indent=2) + "\n")
    lines = ["# Existing six families plus an autonomous SAD endpoint", "", "Both sides exclude adapters, DMA and external data SPM. Area is in SAED14 library units; the selected main column includes local/configuration storage.", "", "| Provisioning | Including local storage | Excluding named storage |", "| --- | ---: | ---: |", f"| Modified CGRA, unchanged | {cgra:,.2f} | {baseline['cgra_excluding_static_storage']:,.2f} |", f"| Original selected six ASICs | {baseline['asic_sum_including_local_storage']:,.2f} | {baseline['asic_sum_excluding_local_storage']:,.2f} |", f"| Additional independent SAD | {result['total_cell_area']:,.2f} | {result['total_cell_area']:,.2f} |", f"| Seven ASICs combined | {combined:,.2f} | {retained:,.2f} |", "", f"CGRA area saving changes from {summary['cgra_area_saving_vs_six_percent']:.2f}% to {summary['cgra_area_saving_vs_seven_percent']:.2f}% when the optional SAD endpoint is installed. SAD worst setup slack is {result['worst_setup_slack_ns']:+.6f} ns at 2 ns; all reported constraints met: {result['no_violated_constraints']}. The CGRA still has {summary['cgra_internal_setup_slack_ns']:+.6f} ns internal setup slack.", ""]
    lines += [f"- {limit}" for limit in summary["limits"]]
    lines += ["", "| ASIC core | Including local storage | Worst setup slack, ns |", "| --- | ---: | ---: |"]
    lines += [f"| {name} | {row['total_cell_area']:,.2f} | {row['worst_setup_slack_ns']:+.6f} |" for name, row in families.items()]
    lines += ["", "See [candidate audit](candidates.md), [SAD source and checks](sad/source/README.md), [mapped SAD detail](sad/results.json), and [unchanged six-family baseline](../revised-area/results.md)."]
    (ROOT / "results.md").write_text("\n".join(lines) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
