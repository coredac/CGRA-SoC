"""Align the GPT three-composition estimate with the compact-layout model."""

import csv
import hashlib
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parent / "gpt-three-way/results.json"
MAIN = HERE.parents[1] / "p1/summary27/figures/review-v20"


def record(path):
    return {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def dump(name, data):
    (HERE / name).write_text(json.dumps(data, indent=2) + "\n")


def relative(cases):
    groups = {}
    for case in cases:
        groups.setdefault(case["composition"], []).append(case)
    return {
        composition: {c["policy"]: c["AE"] / max(r["AE"] for r in group) for c in group}
        for composition, group in groups.items()
    }


def write_readme(report, comparison):
    lines = [
        "# GPT comparison with the compact-layout reference",
        "",
        "This revision uses review-v20 for CGRA entries. The single-CGRA data SPM is projected at 32 KiB, and every AE value uses its updated per-task reference area. The ASIC projection retains its previous absolute area, cycles, three auxiliary cores and shared 128 KiB SPM. These are modeled comparisons, not newly synthesized or timing-closed implementations.",
        "",
        "| Composition | per-task AE | per-segment AE | per-graph AE |",
        "| --- | ---: | ---: | ---: |",
    ]
    for start in (0, 3, 6):
        group = report["cases"][start : start + 3]
        label = group[0]["composition"] + (" (estimate)" if start == 6 else "")
        lines.append(
            "| " + label + " | " + " | ".join(f"{c['AE']:.3f}" for c in group) + " |"
        )
    lines += [
        "",
        "| Composition | Policy | Cycles (million) | Area (million library units) | Global speedup | AE |",
        "| --- | --- | ---: | ---: | ---: | ---: |",
    ]
    for c in report["cases"]:
        lines.append(
            f"| {c['composition']} | {c['policy']} | {c['cycles']/1e6:.4f} | {c['area_library_units']/1e6:.4f} | {c['speedup']:.4f} | {c['AE']:.4f} |"
        )
    lines += [
        "",
        "The following values normalize each composition independently to its best AE. They describe the policy ranking within a composition, not absolute efficiency across compositions.",
        "",
        "| Composition | per-task relative AE | per-segment relative AE | per-graph relative AE |",
        "| --- | ---: | ---: | ---: |",
    ]
    for name, values in comparison["new_relative_AE"].items():
        lines.append(
            "| " + name + " | " + " | ".join(f"{v:.4f}" for v in values.values()) + " |"
        )
    lines += [
        "",
        "The within-composition winner is unchanged: per-task for one CGRA, per-graph for four CGRAs, and nominally per-segment for the ASIC estimate. The latter is only 0.265% above per-task and should be treated as a near tie. For four CGRAs and ASICs, the internal relative-AE bars are unchanged because all policies share the same normalization-factor change. Single-CGRA segment/graph relative AE decreases from 0.9275/0.8903 to 0.8943/0.8494.",
        "",
        "The ASIC composition has four compute IPs: one GEMM plus Norm, Softmax and GPT-vector. Its three auxiliary cores share the original modeled 128 KiB SPM and one upstream command endpoint. Its SPM was not resized to match the projected CGRA data memory, so this is not an equal-SRAM ASIC comparison. The old N2-to-N64 cycle extrapolation, peripheral/control scaling and physical-timing limitations remain unchanged.",
        "",
        "All speedups here use the common GPT 1-CGRA per-task latency reference; the main speedup chart uses the per-task reference at each CGRA count. Original experiments and the preceding table remain unchanged.",
        "",
        "[refresh.py](refresh.py) reproduces [results.json](results.json), [cases.csv](cases.csv), [comparison.json](comparison.json) and [validation.json](validation.json).",
    ]
    (HERE / "README.md").write_text("\n".join(lines) + "\n")


def main():
    old = json.loads(SOURCE.read_text())
    report = json.loads(SOURCE.read_text())
    with (MAIN / "metrics.csv").open() as stream:
        metrics = {
            (int(r["cgras"]), r["policy"]): r
            for r in csv.DictReader(stream)
            if r["workload"] == "GPT-2"
        }
    report["status"] = (
        "Compact-layout CGRA area projection and unchanged ASIC proxy; not newly measured or timing-closed PPA"
    )
    report["reference"] = (
        "P1 GPT N64/H128/heads2/FF512, 1 GEMM + 1 CGRA, per-task; projected 32 KiB CGRA data SPM"
    )
    for name, filename in (
        ("p1_metrics", "metrics.csv"),
        ("p1_layers", "layers.csv"),
        ("p1_area_method", "method.json"),
    ):
        report["sources"][name] = record(MAIN / filename)
    report["projection_source"] = record(SOURCE)
    report["main_result_alignment"] = {
        **record(MAIN / "method.json"),
        "selection": "ae_low; cycles_high; same shared GPT per-task1 AE reference",
        "correction": "CGRA values match review-v20. ASIC area and cycles are unchanged; only its relative area and AE use the new reference area.",
    }
    report["assumptions"].append(
        "GPT single-CGRA data SRAM is projected at 32 KiB using the multicore compact-layout equations; address relocation and cycle invariance are not validated. Its peripheral logic is not re-synthesized. The ASIC cluster retains its original shared 128 KiB SPM."
    )
    for case in report["cases"][:6]:
        count = int(case["composition"].split()[3])
        row = metrics[count, case["policy"]]
        case["cycles"] = float(row["cycles_high"])
        case["area_library_units"] = float(row["area_library_units"])
        case["status"] = (
            "review-v20 conservative endpoint with compact-layout area projection and unchanged archived/model cycles"
        )
    reference = report["cases"][0]
    for case in report["cases"]:
        case["speedup"] = reference["cycles"] / case["cycles"]
        case["relative_area"] = (
            case["area_library_units"] / reference["area_library_units"]
        )
        case["AE"] = case["speedup"] / case["relative_area"]
    task, segment, graph = report["cases"][6:]
    ratio = segment["AE"] / task["AE"]
    report["ASIC_sensitivity"] = {
        "segment_AE_gain_over_task": ratio - 1,
        "segment_AE_gain_over_graph": segment["AE"] / graph["AE"] - 1,
        "extra_segment_cycles_before_tying_task": segment["cycles"] * (ratio - 1),
    }
    comparison = {
        "old_reference_area": old["cases"][0]["area_library_units"],
        "new_reference_area": reference["area_library_units"],
        "old_relative_AE": relative(old["cases"]),
        "new_relative_AE": relative(report["cases"]),
        "AE_changes": [
            {
                "composition": b["composition"],
                "policy": b["policy"],
                "old_AE": a["AE"],
                "new_AE": b["AE"],
            }
            for a, b in zip(old["cases"], report["cases"])
        ],
    }
    checks = {
        "nine points": len(report["cases"]) == 9,
        "all cycles and global speedups unchanged": all(
            a["cycles"] == b["cycles"] and math.isclose(a["speedup"], b["speedup"])
            for a, b in zip(old["cases"], report["cases"])
        ),
        "ASIC absolute area and cycles unchanged": all(
            a["area_library_units"] == b["area_library_units"]
            and a["cycles"] == b["cycles"]
            for a, b in zip(old["cases"][6:], report["cases"][6:])
        ),
        "ASIC model and topology unchanged": all(
            old[k] == report[k]
            for k in ("area_model", "latency_model", "topology", "ASIC_inventory")
        ),
        "input hashes match": all(
            hashlib.sha256(Path(s["path"]).read_bytes()).hexdigest() == s["sha256"]
            for s in report["sources"].values()
        ),
    }
    for case in report["cases"][:6]:
        count = int(case["composition"].split()[3])
        checks[f"{count} CGRA {case['policy']} matches review-v20 AE"] = math.isclose(
            case["AE"], float(metrics[count, case["policy"]]["ae_low"])
        )
    for case in report["cases"]:
        checks[f"{case['composition']} {case['policy']} AE formula"] = math.isclose(
            case["AE"],
            reference["cycles"]
            * reference["area_library_units"]
            / (case["cycles"] * case["area_library_units"]),
        )
    for name, values in comparison["new_relative_AE"].items():
        before = comparison["old_relative_AE"][name]
        checks[name + " winner unchanged"] = max(values, key=values.get) == max(
            before, key=before.get
        )
    dump("results.json", report)
    dump("comparison.json", comparison)
    dump("validation.json", {"passed": all(checks.values()), "checks": checks})
    with (HERE / "cases.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(reference))
        writer.writeheader()
        writer.writerows(report["cases"])
    write_readme(report, comparison)
    print(f"{sum(checks.values())}/{len(checks)} checks passed")


if __name__ == "__main__":
    main()
