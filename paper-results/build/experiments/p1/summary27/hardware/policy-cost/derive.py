"""Account policy-specific area from archived P1 subtrees and SRAM macros."""

import csv
import hashlib
import json
import math
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[5]
HARDWARE = HERE.parent
SOURCE = HARDWARE / "layers.json"
LAYERS = json.loads(SOURCE.read_text())
CONFIGS = json.loads((HARDWARE / "configs.json").read_text())["configurations"]
PAIRS = list(csv.DictReader((HARDWARE.parent / "pairs.csv").open()))
POLICIES = {"Manual": "per-task", "Hybrid": "per-segment", "Auto": "per-graph"}
GEMMINI_CDC = {
    "configAck_sink",
    "configNodeOut_config_source",
    "configNodeOut_patch_source",
    "publicationControl_sink",
    "observeNodeOut_reply_source",
    "out_back_front_q",
}
SOURCES = {SOURCE, HARDWARE / "configs.json", HARDWARE.parent / "pairs.csv"}
CHANGES = []
DETAILS = []


def report_rows(layer):
    return LAYERS["reports"][layer["source"]["report_key"]]["rows"]


def record_change(layer, mode, logic, sram, reason, **evidence):
    CHANGES.append(
        {
            "configuration": layer["configuration"],
            "mode": mode,
            "layer_id": layer["id"],
            "removed_logic": logic,
            "removed_sram": sram,
            "reason": reason,
            "report_key": layer["source"]["report_key"],
            **evidence,
        }
    )
    return logic, sram


def remove(layer, mode, reason):
    return record_change(
        layer,
        mode,
        layer["logic_area_library_units"],
        layer["sram_area_library_units"],
        reason,
        included_paths=layer["source"]["include_paths"],
    )


def gemmini_cdc(layer, mode):
    selected = [r for r in report_rows(layer) if r["path"] in GEMMINI_CDC]
    if len(selected) != len(GEMMINI_CDC):
        raise SystemExit("Incomplete Gemmini CDC subtree evidence")
    logic = sum(r["total"] - r["sram"] for r in selected)
    sram = sum(r["sram"] for r in selected)
    return record_change(
        layer,
        mode,
        logic,
        sram,
        "Remove automatic endpoint-control subtrees; retain native output bridge and mixed local logic",
        included_paths=sorted(GEMMINI_CDC),
    )


def manual_cut(layer):
    role = layer["role"]
    if layer["ip"] == "shared_control" or role in ("link_adapter", "adapter_cdc"):
        return remove(layer, "Manual", "Remove automatic control subtree")
    if role != "endpoint_mmio_cdc_control":
        return 0.0, 0.0
    if layer["ip"].startswith("cgra"):
        return remove(
            layer,
            "Manual",
            "Remove automatic CGRA endpoint; retain native CPU interface",
        )
    return gemmini_cdc(layer, "Manual")


def profile_cut(layer, config):
    """Keep mapped logic and SRAM type; pack unused fields out of the SRAM word."""
    memory = config["autolink"]["profile_storage"]
    if "macro" not in memory:
        return 0.0, 0.0
    n = config["cgra_count"]
    resnet = config["id"].startswith("resnet")
    limits = (
        {"transfers": 6 * n, "edges": 6 * n}
        if resnet
        else {name: n for name in ("transfers", "edges", "regions", "jobs", "stages")}
    )
    rtl = (
        ROOT
        / "build/experiments/p1/trim/saed-patch-v001"
        / config["id"]
        / "autolink-ClockSinkDomain_5/src/ClockSinkDomain_5.sv"
    )
    SOURCES.add(rtl)
    pattern = re.compile(
        r"(description_run_(transfers|edges|regions|jobs|stages)_(\d+)(?:_\w+)?) <= _descriptions_R0_data\[(\d+)(?::(\d+))?\]"
    )
    fields = []
    for match in pattern.finditer(rtl.read_text()):
        name, group, index, high, low = match.groups()
        if group in limits and int(index) >= limits[group]:
            fields.append({"field": name, "high": int(high), "low": int(low or high)})
    removed_bits = sum(f["high"] - f["low"] + 1 for f in fields)
    new_width = memory["width"] - removed_bits
    old_count = memory["macro_instances_per_module"]
    new_count = math.ceil(new_width / memory["macro_width"]) * memory["depth_banks"]
    saved = layer["sram_area_library_units"] * (old_count - new_count) / old_count
    return record_change(
        layer,
        "Hybrid",
        0.0,
        saved,
        "Pack unused descriptor fields; recount same-type SRAM slices; retain all logic",
        rtl=str(rtl.relative_to(ROOT)),
        fields=fields,
        old_width=memory["width"],
        new_width=new_width,
        macro=memory["macro"],
        old_macros=old_count,
        new_macros=new_count,
        depth_banks=memory["depth_banks"],
        depth_saving=0,
    )


def cache_cut(layer, config, mode):
    """GCN Hybrid needs no patches; Auto needs half as many distinct jobs."""
    ip = next(c for c in config["cgras"] if c["id"] == layer["ip"])
    memories = next(
        e["value"] for e in ip["evidence"] if e["field"] == "mapped_memories"
    )
    rows = {r["path"]: r for r in report_rows(layer)}
    saved = 0.0
    evidence = []
    for memory in memories:
        if memory["module"] not in ("packets_ext", "patches_ext"):
            continue
        if mode == "Hybrid" and memory["module"] != "patches_ext":
            continue
        path = "linkAdapter/" + memory["module"].removesuffix("_ext")
        row = rows[path]
        old_count = memory["macro_instances_per_module"]
        new_depth = 0 if mode == "Hybrid" else memory["depth"] // 2
        new_count = (
            math.ceil(new_depth / memory["macro_depth"])
            * memory["width_slices_per_lane"]
        )
        saved += row["sram"] * (old_count - new_count) / old_count
        evidence.append(
            {
                "path": path,
                "macro": memory["macro"],
                "old_macros": old_count,
                "new_macros": new_count,
                "old_depth": memory["depth"],
                "new_depth": new_depth,
            }
        )
    reason = (
        "Hybrid patches resolved into packets by CPU"
        if mode == "Hybrid"
        else "Auto reuses jobs across two bands; halve job banks, rounded to macro depth"
    )
    return record_change(
        layer, mode, 0.0, saved, reason + "; retain all cache logic", memories=evidence
    )


def hybrid_cut(layer, config):
    name, role = config["id"], layer["role"]
    if name.startswith("gpt2"):
        return 0.0, 0.0
    if name.startswith("resnet"):
        return (
            profile_cut(layer, config) if role == "profile_descriptions" else (0.0, 0.0)
        )
    if layer["ip"] == "gemmini0":
        if role in ("link_adapter", "adapter_cdc"):
            return remove(
                layer,
                "Hybrid",
                "GCN Hybrid controls Gemmini through native CPU commands",
            )
        if role == "endpoint_mmio_cdc_control":
            return gemmini_cdc(layer, "Hybrid")
    if (
        role == "stage_scheduler"
        and layer["label"] == f"scheduler/stages_{config['cgra_count']}"
    ):
        return remove(
            layer,
            "Hybrid",
            "GCN Hybrid uses stages 0..n-1; retain all common scheduler logic",
        )
    if role == "profile_descriptions":
        return profile_cut(layer, config)
    if role == "link_adapter" and layer["ip"].startswith("cgra"):
        return cache_cut(layer, config, "Hybrid")
    return 0.0, 0.0


def auto_cut(layer, config):
    if (
        config["id"].startswith("gcn")
        and layer["role"] == "link_adapter"
        and layer["ip"].startswith("cgra")
    ):
        return cache_cut(layer, config, "Auto")
    return 0.0, 0.0


def csv_write(name, rows):
    with (HERE / name).open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def report(records):
    lines = [
        "# Policy-specific area accounting",
        "",
        "These are derived area estimates from the old P1 synthesis breakdown, not new synthesis or new performance measurements. Original results and the approved v6 figure remain unchanged.",
        "",
        "## Area",
        "",
        "Units: million library area units. All rows retain the original native compute, data SRAM, DMA and data-transfer interfaces. Only automatic-control subtrees and explicitly identified SRAM banks/slices are removed.",
        "",
        "| Configuration | Per-task | Per-segment | Per-graph | Original installed |",
        "| --- | ---: | ---: | ---: | ---: |",
    ]
    for config in CONFIGS:
        rows = [r for r in records if r["configuration"] == config["id"]]
        values = " | ".join(f"{r['area']/1e6:.4f}" for r in rows)
        lines.append(
            f"| {config['id']} | {values} | {rows[0]['installed_area']/1e6:.4f} |"
        )
    lines += [
        "",
        "## Area efficiency",
        "",
        "AE = (cycles × area of per-task with one CGRA) / (cycles × area of the current policy/configuration). The baseline area is also specialized. Speedup keeps its original per-task baseline at the same CGRA count.",
        "",
        "| Configuration | Per-task AE | Per-segment AE | Per-graph AE | Best AE policy |",
        "| --- | ---: | ---: | ---: | --- |",
    ]
    for config in CONFIGS:
        rows = [r for r in records if r["configuration"] == config["id"]]
        values = " | ".join(f"{r['normalized_ae']:.4f}" for r in rows)
        best = max(rows, key=lambda r: r["normalized_ae"])
        lines.append(f"| {config['id']} | {values} | {best['policy']} |")
    lines += [
        "",
        "## Interpretation",
        "",
        "Manual-only specialization substantially reduces area. ResNet still favors per-graph AE at each count. GPT-2 favors per-task AE with one CGRA and per-graph with two/four; the two-CGRA margin is small and should not support a strong optimality claim. GCN Auto can use less area than Hybrid because tile-id patch reuse halves the number of distinct cached jobs. No monotonic policy-area ordering was imposed.",
        "",
        "## Accounting rules",
        "",
        "- Per-task: remove shared AutoLink scheduler/profile control, automatic adapters and their CDC, and CGRA automatic endpoints. Keep native CPU packet submission, core configuration, DMA, SPM and native Gemmini output bridges. Gemmini mixed endpoint logic is retained except six independently identified control subtrees.",
        "- ResNet per-segment: retain all 5n stages and all job caches. Compact the descriptor SRAM from 7n to 6n dependencies, keeping the original macro type and all logic. Macro slices change 164/324/643 to 152/299/593 for 1/2/4 CGRAs. Reducing profile counts 9/5/3 to 8/4/2 saves no depth bank because the macros have at least 16 rows.",
        "- GPT-2: per-segment and per-graph cache the same 31 profiles and full job schedule. Their control areas remain equal.",
        "- GCN per-segment: remove the unused Gemmini automatic adapter/control paths and highest stage subtree. Remove patch SRAM because the CPU resolves parameters before capture. Where the profile memory uses SRAM, remove fields for the unused stage/edge and count the remaining macro slices. Shared arbitration and all mixed logic remain.",
        "- GCN per-graph: retain the Gemmini automatic path; halve distinct CGRA jobs through existing tile-id patch reuse. Recount packet/patch banks with the original 64-row macro, including upward rounding for four CGRAs. Keep all per-job slot limits and peripheral logic.",
        "",
        "## Limits",
        "",
        "The specialized hardware has not been generated or simulated. AE assumes native bypass wiring, narrower cached descriptors and reduced job banks preserve the archived execution cycles and clock. Subtree subtraction does not account for remapping or placement. The original synthesis itself uses SRAM/capacity adaptations that differ from simulation, and its component reports fail the 10 ns target. These results are component-accounted estimates, not timing-closed PPA. Whole-SoC CPU, caches and global interconnect remain outside the original area scope.",
        "",
        "## Suggested figure caption",
        "",
        "Execution-policy comparison with policy-specific area accounting. Entries denote policy(CGRA count), where 1/2/3 denote per-task/per-segment/per-graph. Speedup is normalized to per-task at the same CGRA count; AE is normalized to per-task with one CGRA. Area is estimated from synthesized component breakdowns and SRAM macro counts; AE assumes specialization preserves the archived execution cycles at equal clock frequency.",
        "",
        "## Files",
        "",
        "- [Area and AE data](area.csv): all 27 rows, including original area, deductions and revised normalization.",
        "- [Layer accounting](layers.csv): 3,873 disjoint layer/policy rows.",
        "- [Method and evidence](method.json): source hashes, every deduction and SRAM macro counts.",
        "- [Speedup and modeled AE](speedup-ae.pdf).",
        "- [Policy-specific area breakdown](area.pdf).",
        "",
    ]
    (HERE / "results.md").write_text("\n".join(lines))


def build():
    records = []
    for config in CONFIGS:
        name = config["id"]
        layers = [r for r in LAYERS["layers"] if r["configuration"] == name]
        original = LAYERS["configurations"][name]["total_area_library_units"]
        if not math.isclose(
            sum(r["total_area_library_units"] for r in layers), original, abs_tol=1e-5
        ):
            raise SystemExit("Original layer sum mismatch")
        for mode, policy in POLICIES.items():
            total = 0.0
            for layer in layers:
                cut = {
                    "Manual": lambda: manual_cut(layer),
                    "Hybrid": lambda: hybrid_cut(layer, config),
                    "Auto": lambda: auto_cut(layer, config),
                }[mode]
                removed_logic, removed_sram = cut()
                logic = layer["logic_area_library_units"] - removed_logic
                sram = layer["sram_area_library_units"] - removed_sram
                if logic < -1e-6 or sram < -1e-6:
                    raise SystemExit("Negative retained area")
                retained = logic + sram
                total += retained
                DETAILS.append(
                    {
                        "configuration": name,
                        "mode": mode,
                        "layer_id": layer["id"],
                        "ip": layer["ip"],
                        "role": layer["role"],
                        "logic_category": layer["logic_category"],
                        "sram_category": layer["sram_category"],
                        "original_area": layer["total_area_library_units"],
                        "logic": logic,
                        "sram": sram,
                        "removed_logic": removed_logic,
                        "removed_sram": removed_sram,
                        "area": retained,
                    }
                )
            pair = next(
                r
                for r in PAIRS
                if r["workload"] == config["workload"]
                and int(r["cgras"]) == config["cgra_count"]
                and r["mode"] == mode
            )
            if abs(original - total) < 1e-5:
                total = original
            records.append(
                {
                    "configuration": name,
                    "workload": config["workload"],
                    "cgras": config["cgra_count"],
                    "mode": mode,
                    "policy": policy,
                    "cycles": int(pair["warm_cycles"]),
                    "installed_area": original,
                    "area": total,
                    "removed_area": original - total,
                    "reduction_percent": 100 * (original - total) / original,
                }
            )
    for row in records:
        baseline = next(
            r
            for r in records
            if r["workload"] == row["workload"]
            and r["cgras"] == 1
            and r["mode"] == "Manual"
        )
        manual = next(
            r
            for r in records
            if r["configuration"] == row["configuration"] and r["mode"] == "Manual"
        )
        row.update(
            baseline_cycles=baseline["cycles"],
            baseline_area=baseline["area"],
            speedup=manual["cycles"] / row["cycles"],
            normalized_area=row["area"] / baseline["area"],
            normalized_ae=baseline["cycles"]
            * baseline["area"]
            / (row["cycles"] * row["area"]),
            area_basis="hierarchical_subtraction_and_sram_capacity_model",
        )
    if len(records) != 27:
        raise SystemExit("Expected 27 rows")
    csv_write("area.csv", records)
    csv_write("efficiency.csv", records)
    csv_write("layers.csv", DETAILS)
    for config in CONFIGS:
        source = ROOT / "build/experiments/p1/trim" / config["id"] / "source/p1.c"
        if source.exists():
            SOURCES.add(source)
    for n, version in ((1, "v002"), (2, "v001"), (4, "v001")):
        SOURCES.add(
            ROOT / f"build/experiments/p1/gpt2/cgra{n}/{version}/source/p1_plan.h"
        )
    method = {
        "status": "derived_estimate_not_resynthesis",
        "original_results_modified": False,
        "scope": "Old P1 27 rows only; selected accelerator components, not whole SoC; library area units.",
        "manual": "Remove shared automatic control, automatic adapters/CDC and CGRA auto endpoints. Remove six proven Gemmini endpoint-control subtrees; retain native output bridge and mixed residual logic.",
        "hybrid_resnet": "Same 5n stages and job cache as Auto. Model descriptor packing for 6n rather than 7n dependencies. Profiles 8/n rather than 8/n+1 still require one 16-entry macro bank. Scheduler, register and peripheral logic retained.",
        "hybrid_gpt2": "Both policies cache the same 31 profiles and full schedule; no area difference inferred from launch count.",
        "hybrid_gcn": "CPU-driven Gemmini needs no automatic adapter. Retain n of n+1 stage subtrees and common scheduler logic; remove unused patch SRAM; model narrower descriptor SRAM where SRAM-backed. All native paths retained.",
        "auto_gcn": "Half as many jobs through tile-id patch reuse. Recount packet and patch SRAM banks with original macro types. Keep per-job slot capacities and all logic. Auto profile-depth reduction gives no SRAM bank reduction; register-backed profiles kept.",
        "retained": "Native arithmetic, core control/configuration, native DMA, CPU command interfaces, SPM windows, output bridges, packed conversion, all data SRAM, local TileLink and all unseparated logic.",
        "ae": "Archived cycles unchanged. AE=(per-task one-CGRA cycles * derived area)/(current cycles * derived area), assuming equal clocks and unchanged native datapath timing after specialization.",
        "limitations": [
            "No specialized RTL was generated, simulated or synthesized. Native bypass wiring and new memory packing are assumed implementable; resulting cycles/timing are unvalidated.",
            "Subtree subtraction preserves existing cell mapping and mixed residuals; it does not predict reoptimization or physical placement.",
            "Capacity estimates use integer counts of original SRAM macro types, not area proportional to logical bits or number of launches.",
            "Retaining unseparated logic avoids unsupported savings but is not a formal upper bound on a new physical implementation.",
            "Original synthesis capacity/SRAM adaptations differ from the simulation RTL; archived component timing fails the 10 ns target. These estimates do not establish timing-closed PPA.",
            "No assumed ordering of per-task, per-segment and per-graph area is enforced.",
        ],
        "sources": [
            {
                "path": str(p.relative_to(ROOT)),
                "sha256": hashlib.sha256(p.read_bytes()).hexdigest(),
            }
            for p in sorted(SOURCES)
        ],
        "changes": CHANGES,
        "checks": {
            "rows": len(records),
            "layer_policy_rows": len(DETAILS),
            "nonnegative": True,
            "original_layer_totals_match": True,
            "native_data_paths_unchanged": True,
        },
        "records": records,
    }
    (HERE / "method.json").write_text(json.dumps(method, indent=2) + "\n")
    report(records)
    for row in records:
        print(
            row["configuration"],
            row["mode"],
            f"area={row['area']/1e6:.6f}M",
            f"AE={row['normalized_ae']:.4f}",
        )


if __name__ == "__main__":
    build()
