"""Estimate equal-buffer segment storage and refill cost from frozen P1 data."""

import csv
import hashlib
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
HW = HERE.parent
ROOT = HERE.parents[5]
BASE = HW / "policy-cost"
CONFIGS = json.loads((HW / "configs.json").read_text())["configurations"]
LAYERS = json.loads((HW / "layers.json").read_text())
AREAS = list(csv.DictReader((BASE / "area.csv").open()))
TIMES = json.loads((HERE / "evidence/segment-timing.json").read_text())
DETAILS = []


def write_csv(name, rows):
    with (HERE / name).open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def capacity(config):
    n = config["cgra_count"]
    if config["id"].startswith("resnet"):
        return {"profiles": 2, "native": 6 * n, "cgra": 4}
    if config["id"].startswith("gcn"):
        return {"profiles": 2, "native": 0, "cgra": 128 // n}
    plan = json.loads((HERE / f"evidence/p1_segments_{n}.json").read_text())
    segments = plan["segments"]
    return {
        "profiles": 2 * max(s["profiles"] for s in segments),
        "native": 2 * max(s["native_jobs"] for s in segments),
        "cgra": 2 * max(c["jobs"] for s in segments for c in s["cgra"]),
    }


def memory_cut(layer, memory, path, depth):
    rows = LAYERS["reports"][layer["source"]["report_key"]]["rows"]
    row = next(r for r in rows if r["path"] == path)
    old = memory["macro_instances_per_module"]
    new = math.ceil(depth / memory["macro_depth"]) * memory["width_slices_per_lane"]
    saving = row["sram"] * (old - new) / old
    DETAILS.append(
        dict(
            configuration=layer["configuration"],
            layer_id=layer["id"],
            path=path,
            macro=memory["macro"],
            old_macros=old,
            new_macros=new,
            old_depth=memory["depth"],
            new_depth=depth,
            saved_area=saving,
        )
    )
    return saving


def area_cut(config, slots):
    if config["id"].startswith("gcn"):
        return 0.0
    saving = 0.0
    layers = [
        l
        for l in LAYERS["layers"]
        if l["configuration"] == config["id"] and l["role"] == "link_adapter"
    ]
    for layer in layers:
        native = layer["ip"] == "gemmini0"
        ip = (
            config["gemmini"]
            if native
            else next(c for c in config["cgras"] if c["id"] == layer["ip"])
        )
        memories = next(
            e["value"] for e in ip["evidence"] if e["field"] == "mapped_memories"
        )
        adapter = ip["adapter"]
        targets = (
            {
                "commands_ext": (
                    "linkBusy_adapter/commands",
                    slots["native"] * adapter["commands_per_job"],
                )
            }
            if native
            else {
                "packets_ext": (
                    "linkAdapter/packets",
                    slots["cgra"] * adapter["packets_per_job"],
                )
            }
        )
        if native and adapter["patch_backing"] == "sram":
            targets["GemminiPatch"] = (
                "linkBusy_adapter/binding",
                slots["native"] * adapter["patches_per_job"],
            )
        for memory in memories:
            if memory["module"] in targets:
                path, depth = targets[memory["module"]]
                saving += memory_cut(layer, memory, path, depth)
    return saving


def latency(config, timing):
    """Original execution boundary: initial segment ready before timing starts."""
    old = timing["resident_cycles"]
    execute = timing["execution_cycles"]
    capture = timing["capture_cycles"]
    outside = old - execute
    if timing.get("full_schedule_fits_two_buffers"):
        return old, old, old, "retain_complete_two_segment_schedule", capture
    if config["id"].startswith("resnet"):
        count = timing["segments"]
        remaining = capture * (count - 1) / count
        point = old + (count - 1) * max(0, (capture - execute) / count)
        low = outside + max(execute, remaining)
        return (
            math.ceil(low),
            math.ceil(old + remaining),
            math.ceil(point),
            "uniform_segment_model",
            capture,
        )
    # Host plan: 36 reloads versus 32 unique CGRA jobs; 3293/2999 packets.
    # Inflate ALL capture work by the larger job-count ratio as a conservative
    # linear-cost scenario. Native commands and profile loads do not duplicate.
    reload_cost = math.ceil(capture * 36 / 32)
    return (
        old,
        old + reload_cost,
        "",
        "overlap_sensitivity_no_segment_timestamps",
        reload_cost,
    )


def main():
    estimates = []
    updated = []
    for config in CONFIGS:
        name = config["id"]
        base = next(
            r for r in AREAS if r["configuration"] == name and r["mode"] == "Hybrid"
        )
        timing = next(
            t
            for t in TIMES
            if t["workload"] == base["workload"]
            and int(t["cgras"]) == config["cgra_count"]
        )
        slots = capacity(config)
        saving = area_cut(config, slots)
        original = float(base["area"])
        area = original - saving
        low, high, point, kind, reload_cost = latency(config, timing)
        old = int(base["cycles"])
        estimates.append(
            dict(
                configuration=name,
                cgras=config["cgra_count"],
                old_hybrid_area=original,
                modeled_area=area,
                saved_area=saving,
                saving_percent=100 * saving / original,
                profile_slots=slots["profiles"],
                native_slots=slots["native"],
                cgra_slots_per_ip=slots["cgra"],
                old_cycles=old,
                cycles_low=low,
                cycles_high=high,
                cycles_point=point,
                low_extra_percent=100 * (low / old - 1),
                high_extra_percent=100 * (high / old - 1),
                point_extra_percent=100 * (point / old - 1) if point else "",
                timing_model=kind,
                original_capture_cycles=timing["capture_cycles"],
                modeled_reload_cycles=reload_cost,
                full_reload_cycles_low=old
                - timing["execution_cycles"]
                + max(timing["execution_cycles"], reload_cost),
                full_reload_cycles_high=old + reload_cost,
            )
        )
        for source in (r for r in AREAS if r["configuration"] == name):
            hybrid = source["mode"] == "Hybrid"
            row_area = area if hybrid else float(source["area"])
            row_low = low if hybrid else int(source["cycles"])
            row_high = high if hybrid else int(source["cycles"])
            manual = next(
                r for r in AREAS if r["configuration"] == name and r["mode"] == "Manual"
            )
            baseline = float(source["baseline_cycles"]) * float(source["baseline_area"])
            updated.append(
                dict(
                    configuration=name,
                    policy=source["policy"],
                    area=row_area,
                    original_cycles=int(source["cycles"]),
                    cycles_low=row_low,
                    cycles_high=row_high,
                    speedup_low=int(manual["cycles"]) / row_high,
                    speedup_high=int(manual["cycles"]) / row_low,
                    ae_low=baseline / (row_high * row_area),
                    ae_high=baseline / (row_low * row_area),
                    status=(
                        "modeled_segment_refill"
                        if hybrid
                        else "unchanged_prior_area_model_and_archived_cycles"
                    ),
                )
            )
    write_csv("hybrid.csv", estimates)
    write_csv("comparison27.csv", updated)
    write_csv("memory.csv", DETAILS)
    paths = [
        HW / "configs.json",
        HW / "layers.json",
        BASE / "area.csv",
        BASE / "method.json",
        HERE / "estimate.py",
    ]
    paths += sorted((HERE / "evidence").glob("*"))
    paths += [ROOT / t["archived_log"] for t in TIMES]
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    (HERE / "sources.json").write_text(json.dumps(hashes, indent=2) + "\n")
    for row in estimates:
        print(
            f"{row['configuration']}: area {row['old_hybrid_area']/1e6:.4f} -> {row['modeled_area']/1e6:.4f} (-{row['saving_percent']:.2f}%), cycles {row['old_cycles']} -> {row['cycles_point'] or str(row['cycles_low'])+'..'+str(row['cycles_high'])}"
        )


if __name__ == "__main__":
    main()
