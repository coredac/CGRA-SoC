"""Recalculate the archived GPT projection for its three used auxiliary cores."""

import csv
import hashlib
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent


def dump(name, data):
    (HERE / name).write_text(json.dumps(data, indent=2) + "\n")


def main():
    source = HERE / "seven-core-scenario.json"
    report = json.loads(source.read_text())
    old_cases = json.loads(source.read_text())["cases"]
    core_path = Path(report["sources"]["seven_cores"]["path"])
    cores = json.loads(core_path.read_text())["families"]
    selected = ["norm", "softmax", "gpt_vector"]
    area = report["area_model"]
    del area["new_seven_core_subtotal"]
    area["new_core_subtotal"] = sum(cores[k]["total_cell_area"] for k in selected)
    area["additional_shared_control"] = (len(selected) / 5 - 1) * area[
        "old_shared_control_and_peripheral"
    ]
    area["new_cluster_endpoint"] = (
        area["old_endpoint"]
        - area["old_five_unit_subtotal"]
        + area["new_core_subtotal"]
        + area["additional_shared_control"]
    )
    area["equation"] = (
        "new cluster = old endpoint - five old units + three selected cores + (3/5 - 1)*(old shared memory peripheral + local controller + endpoint shell)"
    )
    report["ASIC_inventory"] = ["Norm", "Softmax", "GPT-vector"]
    report["additional_ASIC_cores"] = 3
    report["total_fixed_function_compute_IPs"] = 4
    report["inventory_note"] = (
        "GPT uses these three auxiliary services plus one GEMM. Add, ReLU, GCN and SAD are excluded from this workload-specific projection. The installed-seven-core scenario is archived separately."
    )
    report["topology"] = (
        "Three local ASIC cores share a 128 KiB SPM and one upstream command endpoint; GEMM is the other endpoint. Four compute IPs do not mean four independent AutoLink endpoints."
    )
    report["assumptions"][2:5] = [
        "Removing unused cores adds no latency relative to the old proxy; cluster scheduling, shared-SPM arbitration and buffering are not revalidated.",
        "The existing linear peripheral/control model is extrapolated from five to three cores; SRAM payload remains 128 KiB. No redesigned cluster synthesis exists.",
        "The three selected core areas include their synthesized native/local/configuration logic and local storage; old unit areas are removed to avoid double counting.",
    ]
    report["main_result_alignment"][
        "correction"
    ] = "CGRA main-result values are unchanged. The GPT ASIC estimate now includes only its three used auxiliary cores plus GEMM; its old cycle proxies are unchanged."
    layers = Path(report["sources"]["p1_layers"]["path"])
    layers = layers.parent.parent / "review-v19" / "layers.csv"
    report["sources"]["p1_layers"] = {
        "path": str(layers),
        "sha256": hashlib.sha256(layers.read_bytes()).hexdigest(),
    }
    report["projection_source"] = {
        "path": str(source),
        "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
    }
    reference = report["cases"][0]
    for case in report["cases"][6:]:
        case["composition"] = "1 GEMM + 3 ASIC cores"
        case["area_library_units"] = (
            area["shared_GEMM_SPM_fabric_by_policy"][case["policy"]]
            + area["new_cluster_endpoint"]
        )
        case["relative_area"] = (
            case["area_library_units"] / reference["area_library_units"]
        )
        case["AE"] = case["speedup"] / case["relative_area"]
        case["status"] = (
            "cross-size cross-revision proxy estimate; three auxiliary cores with one shared upstream endpoint"
        )
    task, segment, graph = report["cases"][6:]
    ratio = segment["AE"] / task["AE"]
    report["ASIC_sensitivity"] = {
        "segment_AE_gain_over_task": ratio - 1,
        "segment_AE_gain_over_graph": segment["AE"] / graph["AE"] - 1,
        "extra_segment_cycles_before_tying_task": segment["cycles"] * (ratio - 1),
    }
    checks = {
        "CGRA main-result entries unchanged": report["cases"][:6] == old_cases[:6],
        "ASIC cycle proxies unchanged": all(
            a["cycles"] == b["cycles"]
            for a, b in zip(report["cases"][6:], old_cases[6:])
        ),
        "four fixed-function compute IPs": report["GEMM_count"]
        + report["additional_ASIC_cores"]
        == report["total_fixed_function_compute_IPs"],
        "input hashes match": all(
            hashlib.sha256(Path(s["path"]).read_bytes()).hexdigest() == s["sha256"]
            for s in report["sources"].values()
        ),
    }
    for case in report["cases"]:
        checks[case["composition"] + " " + case["policy"] + " AE formula"] = (
            math.isclose(
                case["AE"],
                reference["cycles"]
                * reference["area_library_units"]
                / (case["cycles"] * case["area_library_units"]),
            )
        )
    dump("results.json", report)
    with (HERE / "cases.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(reference))
        writer.writeheader()
        writer.writerows(report["cases"])
    dump("validation.json", {"passed": all(checks.values()), "checks": checks})
    print(f"{sum(checks.values())}/{len(checks)} checks passed")


if __name__ == "__main__":
    main()
