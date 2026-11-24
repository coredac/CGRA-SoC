"""Apply the GPT single-CGRA compact-layout SRAM projection."""

import csv
import hashlib
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
PRIOR = HERE.parent / "review-v19"
HW = HERE.parents[1] / "hardware"
LAYOUT = HERE.parents[3] / "policy-check/sram-alignment/layout.json"


def read(path):
    with path.open() as stream:
        return list(csv.DictReader(stream))


def write(name, rows):
    with (HERE / name).open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def main():
    old_metrics = read(PRIOR / "metrics.csv")
    old_layers = read(PRIOR / "layers.csv")
    metrics = [dict(r) for r in old_metrics]
    layers = [dict(r) for r in old_layers]
    layout = json.loads(LAYOUT.read_text())
    inventory = json.loads((HW / "layers.json").read_text())
    target = next(r for r in layout["cases"] if r["cgras"] == 1)
    layer = next(
        r
        for r in inventory["layers"]
        if r["configuration"] == "gpt2-rows-cgra1" and r["role"] == "core_data_mem"
    )
    source = layer["source"]
    report = inventory["reports"][source["report_key"]]
    memory = next(r for r in report["rows"] if r["path"] == source["include_paths"][0])
    macro = "SRAM2RW128x32"
    old_count = memory["macros"][macro]
    new_count = (
        old_count
        * target["capacity_each_kib"]
        / layout["single_core_original"]["capacity_kib"]
    )
    unit_area = memory["sram"] / old_count
    new_sram = new_count * unit_area
    adjustments = []
    for row in layers:
        if row["layer_id"] != layer["id"]:
            continue
        removed = float(row["sram"]) - new_sram
        adjustments.append(
            {
                "layer_id": row["layer_id"],
                "mode": row["mode"],
                "old_sram": float(row["sram"]),
                "new_sram": new_sram,
                "removed_sram": removed,
            }
        )
        row["sram"] = new_sram
        row["removed_sram"] = float(row["removed_sram"]) + removed
        row["area"] = float(row["logic"]) + new_sram
    removed = adjustments[0]["removed_sram"]
    for row in metrics:
        if row["workload"] != "GPT-2" or row["cgras"] != "1":
            continue
        row["area_library_units"] = float(row["area_library_units"]) - removed
        row["area_million_library_units"] = row["area_library_units"] / 1e6
        row["area_status"] = "compact_layout_data_SRAM_projection"
    reference = next(
        r
        for r in metrics
        if (r["workload"], r["cgras"], r["policy"]) == ("GPT-2", "1", "per-task")
    )
    product = float(reference["area_library_units"]) * int(reference["cycles_low"])
    for row in metrics:
        if row["workload"] != "GPT-2":
            continue
        area = float(row["area_library_units"])
        row["ae_low"] = product / (area * int(row["cycles_high"]))
        row["ae_high"] = product / (area * int(row["cycles_low"]))
        row["condition"] = (
            "GPT single-CGRA data SRAM is projected at 32 KiB using a compact layout; its peripheral logic and all cycles are unchanged. AE uses the updated per-task1 reference."
        )
    modes = {"per-task": "Manual", "per-segment": "Hybrid", "per-graph": "Auto"}
    checks = {
        "27 settings and 3873 layers": len(metrics) == 27 and len(layers) == 3873,
        "three single-CGRA memory layers adjusted": len(adjustments) == 3,
        "integer macro inventory": new_count.is_integer(),
        "projection fits 32 KiB": target["required_words"] * 4 <= 32 * 1024,
        "all cycles and speedups unchanged": all(
            a[k] == b[k]
            for a, b in zip(old_metrics, metrics)
            for k in ("cycles_low", "cycles_high", "speedup_low", "speedup_high")
        ),
        "non-GPT metrics unchanged": all(
            a == b for a, b in zip(old_metrics, metrics) if a["workload"] != "GPT-2"
        ),
        "all other layers unchanged": all(
            a == b for a, b in zip(old_layers, layers) if a["layer_id"] != layer["id"]
        ),
        "deductions reconcile": all(
            math.isclose(
                float(r["original_area"]),
                sum(float(r[k]) for k in ("area", "removed_logic", "removed_sram")),
                abs_tol=1e-5,
            )
            for r in layers
        ),
    }
    for row in metrics:
        if row["workload"] != "GPT-2":
            continue
        total = sum(
            float(r["area"])
            for r in layers
            if r["configuration"] == f"gpt2-rows-cgra{row['cgras']}"
            and r["mode"] == modes[row["policy"]]
        )
        checks[f"GPT{row['cgras']} {row['policy']} area"] = math.isclose(
            total, float(row["area_library_units"]), abs_tol=1e-5
        )
    write("metrics.csv", metrics)
    write("layers.csv", layers)
    inputs = [PRIOR / "metrics.csv", PRIOR / "layers.csv", HW / "layers.json", LAYOUT]
    method = {
        "status": "compact_layout_area_projection_not_new_synthesis_or_simulation",
        "scope": "Only the three GPT single-CGRA core_data_mem SRAM areas change. All other component areas and every cycle/speedup remain unchanged. All GPT AE values use the new reference area.",
        "data_SPM_KiB_each": {"1": 32, "2": 16, "4": 8},
        "data_SPM_KiB_total": {"1": 32, "2": 32, "4": 32},
        "macro": macro,
        "macro_unit_area": unit_area,
        "old_macro_count": old_count,
        "projected_macro_count": int(new_count),
        "macro_count_note": "Preserve the archived banking/port implementation's macro multiplicity; scale capacity using integer counts of the same macro type.",
        "layer_changes": adjustments,
        "reference_area": reference["area_library_units"],
        "conditions": layout["conditions"][:2]
        + [
            "Data-memory peripheral/address logic is retained at its old cost; address remapping is not implemented or simulated.",
            "Cycle invariance under relocation and physical common-clock timing closure are unverified.",
            "Prior GCN shared-template and segment-refill modeling assumptions remain unchanged.",
        ],
        "sources": {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
        "checks": checks,
    }
    (HERE / "method.json").write_text(json.dumps(method, indent=2) + "\n")
    changes = [
        {
            "cgras": int(b["cgras"]),
            "policy": b["policy"],
            "old_area": float(a["area_library_units"]),
            "new_area": float(b["area_library_units"]),
            "old_ae": float(a["ae_low"]),
            "new_ae": float(b["ae_low"]),
        }
        for a, b in zip(old_metrics, metrics)
        if b["workload"] == "GPT-2"
    ]
    (HERE / "changes.json").write_text(json.dumps(changes, indent=2) + "\n")
    print(f"{sum(checks.values())}/{len(checks)} checks passed")
    print(json.dumps(changes, indent=2))


if __name__ == "__main__":
    main()
