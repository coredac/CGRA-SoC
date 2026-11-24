"""Compare completed modified cores without mixing historical or partial areas."""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def main():
    paths = [ROOT / name / "results.json" for name in ["cgra", "asic"]]
    if not all(path.exists() for path in paths):
        print("Waiting for complete CGRA and six-family ASIC mappings.")
        return
    cgra, asic = [json.loads(path.read_text()) for path in paths]
    families = asic["families"]
    if not cgra["complete"] or len(families) != 6 or not all(row["complete"] for row in families.values()):
        print("All seven complete-core mappings must finish before comparison.")
        return
    fixed_path = ROOT / "gpt-fix/mapped/results.json"
    if not fixed_path.exists():
        print("Waiting for the selected GPT timing repair; original expanded GPT fails 2 ns.")
        return
    fixed = json.loads(fixed_path.read_text())
    if not fixed["complete"] or not fixed["no_violated_constraints"]:
        print("The selected GPT timing repair has not passed mapped constraints.")
        return
    original_gpt = families["gpt_vector"]
    if fixed["original_source_sha"] != original_gpt["source_sha256"] or fixed["new_source_sha"] != fixed["source_sha256"]:
        print("GPT repair does not match the recorded before/after source lineage.")
        return
    families["gpt_vector"] = fixed
    full = sum(row["total_cell_area"] for row in families.values())
    retained = sum(row["area_excluding_local_array_storage"] for row in families.values())
    cgra_retained = cgra["area_excluding_static_storage"]
    result = {
        "scope": "Modified 16-PE CGRA versus six independent modified ASIC families with 16 primitive lanes each; compute/control/local access, excluding adapters/DMA/external data SPM.",
        "area_unit": "SAED14 library area units",
        "clock_target_ns": 2,
        "cgra_including_local_storage": cgra["total_cell_area"],
        "asic_sum_including_local_storage": full,
        "cgra_excluding_static_storage": cgra_retained,
        "asic_sum_excluding_local_storage": retained,
        "cgra_over_asic_including_local_storage": cgra["total_cell_area"] / full,
        "cgra_over_asic_excluding_storage": cgra_retained / retained,
        "cgra_minus_asic_excluding_storage": cgra_retained - retained,
        "cgra_worst_setup_slack_ns": cgra["worst_setup_slack_ns"],
        "cgra_worst_reported_data_path": cgra["worst_reported_data_path"],
        "asic_setup_slack_ns": {name: row["worst_setup_slack_ns"] for name, row in families.items()},
        "selected_gpt_result": str(fixed_path.relative_to(ROOT)),
        "original_gpt_source_sha256": original_gpt["source_sha256"],
        "selected_gpt_source_sha256": fixed["source_sha256"],
        "original_gpt_area_excluding_storage": original_gpt["area_excluding_local_array_storage"],
        "original_gpt_setup_slack_ns": original_gpt["worst_setup_slack_ns"],
        "gpt_timing_repair_area_delta": fixed["total_cell_area"] - original_gpt["total_cell_area"],
        "all_reported_constraints_met": cgra["no_violated_constraints"] and all(row["no_violated_constraints"] for row in families.values()),
        "limits": [
            "Six independent expanded ASIC endpoints install 96 primitive lanes; this is not equal total arithmetic resources or verified equal throughput.",
            "Area excluding storage is a breakdown, not full implementation cost; local storage access and runtime state remain charged.",
            "No simulator or performance rerun; old results remain associated with original hardware.",
            "Ideal-clock wire-load mapping is not physical signoff; report actual setup failures separately.",
        ],
    }
    (ROOT / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    rows = [("Modified CGRA", cgra["total_cell_area"], cgra_retained, cgra["worst_setup_slack_ns"])]
    rows += [(name, row["total_cell_area"], row["area_excluding_local_array_storage"], row["worst_setup_slack_ns"]) for name, row in families.items()]
    lines = ["# Complete modified-core results", "", "Area units are SAED14 library area units. Both boundaries exclude adapters, DMA and external data SPM, including its access. Local array/configuration access and runtime state remain counted.", "", "| Core | Including local storage | Excluding named storage cells | Worst setup slack (ns), target 2 ns |", "| --- | ---: | ---: | ---: |"]
    lines += [f"| {name} | {full_area:,.2f} | {core_area:,.2f} | {slack:+.6f} |" for name, full_area, core_area, slack in rows]
    lines += [f"| Six ASICs combined | {full:,.2f} | {retained:,.2f} | Per-core values above |", "", f"CGRA / six-ASIC area ratio is {result['cgra_over_asic_excluding_storage']:.6f} after named storage subtraction, and {result['cgra_over_asic_including_local_storage']:.6f} including local storage. This compares the modified designs; the ASIC sum installs 96 primitive lanes versus the CGRA's 16 PEs. Equal useful throughput is not established.", "", f"The selected GPT-vector includes the local timing repair; its predecessor had {original_gpt['worst_setup_slack_ns']:+.6f} ns setup slack. The native operation-0 schedule gains three cycles per element. Mapped area changes by {result['gpt_timing_repair_area_delta']:+.2f} units.", "", "See [scope and validation limits](README.md), [CGRA details](cgra/results.json), [original ASIC details](asic/results.json) and [selected GPT repair](gpt-fix/mapped/results.json). No simulator or performance run was performed."]
    lines += ["", f"All six selected ASICs meet the reported 2 ns synthesis constraints. CGRA does not: its raw worst slack includes the forwarded clock output incorrectly treated as a data output; the worst reported genuine data path still has {cgra['worst_reported_data_path']['slack_ns']:+.6f} ns slack. Raw QoR reports {cgra['violating_setup_paths']} violating setup paths and 114 fanout violations. No false-path exception, corrected-constraint STA or new mapping is claimed.", "", "The worst genuine data path runs from PE 1's instruction-address register through instruction selection, the routing crossbar, operand selection, multiplication and the FU crossbar into an operand register. This lies within the PE execution path; DMA, adapters and the external SPM are outside this synthesis boundary.", "", "| CGRA retained component | Area |", "| --- | ---: |"]
    lines += [f"| {name} | {area:,.2f} |" for name, area in sorted(cgra["retained_group_area"].items(), key=lambda item: -item[1])]
    (ROOT / "results.md").write_text("\n".join(lines) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
