"""Prepare audited modeled metrics and component layers for figure review."""

import csv
import hashlib
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
HW = HERE.parents[1] / "hardware"


def read(path):
    return list(csv.DictReader(path.open()))


def write(name, rows):
    with (HERE / name).open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def main():
    inputs = [
        HW / "current-review27.csv",
        HW / "policy-cost/layers.csv",
        HW / "segment-buffer/memory.csv",
        HW / "policy-cost/method.json",
        HW / "gcn-reuse/method.json",
    ]
    current = read(inputs[0])
    layers = read(inputs[1])
    cuts = read(inputs[2])
    changes = json.loads(inputs[3].read_text())["changes"]
    data = []
    for row in current:
        row = dict(row)
        manual = next(
            r
            for r in current
            if r["workload"] == row["workload"]
            and r["cgras"] == row["cgras"]
            and r["policy"] == "per-task"
        )
        reference = next(
            r
            for r in current
            if r["workload"] == row["workload"]
            and r["cgras"] == "1"
            and r["policy"] == "per-task"
        )
        area = float(row["area_library_units"])
        product = int(reference["cycles_low"]) * float(reference["area_library_units"])
        row.update(
            speedup_low=int(manual["cycles_low"]) / int(row["cycles_high"]),
            speedup_high=int(manual["cycles_low"]) / int(row["cycles_low"]),
            ae_low=product / (area * int(row["cycles_high"])),
            ae_high=product / (area * int(row["cycles_low"])),
            condition=(
                "GCN segment AE is conditional on old cycles and zero unpriced band-binding area"
                if row["workload"] == "GCN" and row["policy"] == "per-segment"
                else "component-area model at equal clock"
            ),
        )
        data.append(row)
    for row in layers:
        row["logic"] = float(row["logic"])
        row["sram"] = float(row["sram"])
        prior_sram = row["sram"]
        if row["mode"] == "Hybrid":
            row["sram"] -= sum(
                float(c["saved_area"]) for c in cuts if c["layer_id"] == row["layer_id"]
            )
            if (
                row["configuration"].startswith("gcn")
                and row["ip"].startswith("cgra")
                and row["role"] == "link_adapter"
            ):
                selected = [c for c in changes if c["layer_id"] == row["layer_id"]]
                row["sram"] += sum(
                    c["removed_sram"] for c in selected if c["mode"] == "Hybrid"
                )
                row["sram"] -= sum(
                    c["removed_sram"] for c in selected if c["mode"] == "Auto"
                )
        row["removed_sram"] = float(row["removed_sram"]) + prior_sram - row["sram"]
        row["area"] = row["logic"] + row["sram"]
    checks = [
        ("27 settings", len(data) == 27),
        ("3873 layers", len(layers) == 3873),
        (
            "nonnegative layers",
            all(r["logic"] >= 0 and r["sram"] >= -1e-6 for r in layers),
        ),
        (
            "layer deductions reconcile",
            all(
                math.isclose(
                    float(r["original_area"]),
                    r["area"] + float(r["removed_logic"]) + r["removed_sram"],
                    abs_tol=1e-5,
                )
                for r in layers
            ),
        ),
    ]
    prefixes = {"ResNet": "resnet", "GPT-2": "gpt2-rows", "GCN": "gcn"}
    modes = {"per-task": "Manual", "per-segment": "Hybrid", "per-graph": "Auto"}
    for row in data:
        config = f"{prefixes[row['workload']]}-cgra{row['cgras']}"
        total = sum(
            l["area"]
            for l in layers
            if l["configuration"] == config and l["mode"] == modes[row["policy"]]
        )
        checks.append(
            (
                config + " " + row["policy"] + " area reconciles",
                math.isclose(total, float(row["area_library_units"]), abs_tol=1e-5),
            )
        )
        checks.append(
            (
                config + " " + row["policy"] + " ordered intervals",
                row["speedup_low"] <= row["speedup_high"]
                and row["ae_low"] <= row["ae_high"],
            )
        )
    write("metrics.csv", data)
    write("layers.csv", layers)
    metadata = {
        "status": "modeled_sensitivity_figure_not_new_measured_PPA",
        "speedup": "Original per-task cycles at the same CGRA count divided by current cycles.",
        "ae": "Per-task1 cycles times per-task1 modeled area divided by current cycles times current modeled component area.",
        "bars": "Accepted review-v18 bars use speedup_low and ae_low. This audit revision changes deduction metadata only and generates no replacement figure.",
        "revision": "Correct cumulative removed_sram after segment-buffer and GCN template adjustments. Retained logic, SRAM, total area and all 27 metrics remain unchanged from review-v7.",
        "gcn_dagger": "Segment AE is a conditional projection using the archived original-design cycle count and shared-template area subtotal. Added band-origin logic and latency are excluded; actual AE can be lower.",
        "area_scope": "Accelerator components only. CPU, CPU caches, global interconnect and Pool require separately verified source reports; do not infer zeros for missing components.",
        "sources": {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
        "checks": checks,
    }
    (HERE / "method.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"{sum(ok for _, ok in checks)}/{len(checks)} checks passed")
    for name, ok in checks:
        if not ok:
            print("FAILED", name)


if __name__ == "__main__":
    main()
