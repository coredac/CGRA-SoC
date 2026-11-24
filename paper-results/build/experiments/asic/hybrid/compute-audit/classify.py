"""Attribute original mapped leaf area while retaining storage access logic."""

import csv
import hashlib
import json
import re
from collections import defaultdict
from pathlib import Path
import sys

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[4]
sys.path.insert(0, str(OUT.parent / "area-audit"))
from collect import read, select

RULES = {
    "cgra": [
        (
            "pe_configuration_bits",
            r"cgra/cgra_inst/cgra/tile__\d+/ctrl_mem/reg_file/regs_reg\[.*",
        ),
        (
            "constant_storage_bits",
            r"cgra/cgra_inst/cgra/tile__\d+/const_mem/reg_file/regs_reg\[.*",
        ),
        (
            "adapter_configuration_bits",
            r"linkAdapter/(?:elements|jobPacketCount|jobExpectedCompletions|jobRearmCount|jobSetupCount|jobNext)_\d+_reg(?:\[.*)?",
        ),
    ],
    "asic": [
        (
            "local_data_bits",
            r"AsicCore/gcn/(?:features|weights|columns|rowptr)_reg\[.*",
        ),
        ("local_data_bits", r"AsicCore/norm/(?:source|gain|offset|result)_reg\[.*"),
        ("local_data_bits", r"AsicCore/softmax/(?:source|exponent|result)_reg\[.*"),
        (
            "local_data_bits",
            r"AsicCore/gpt_vector/(?:source|other|result)_store/banks\[\d+\]\.memory/words_reg\[.*",
        ),
        (
            "adapter_configuration_bits",
            r"adapter/(?:descriptors_\d+_\w+|nextJobs_\d+)_reg(?:\[.*)?",
        ),
    ],
}


def category(backend, row):
    if row["ref"].startswith("SRAM"):
        return (
            "configuration_sram"
            if row["path"].startswith("linkAdapter/")
            else "data_sram"
        )
    if row["sequential"] == "true":
        for label, pattern in RULES[backend]:
            if re.fullmatch(pattern, row["path"]):
                return label
    return "retained_compute_control"


def summarize(backend):
    totals = defaultdict(lambda: {"cells": 0, "area": 0.0})
    core = defaultdict(lambda: {"cells": 0, "area": 0.0})
    retained = defaultdict(lambda: {"cells": 0, "area": 0.0})
    ring = defaultdict(lambda: {"cells": 0, "area": 0.0})
    seen = set()
    duplicate = 0
    root = "cgra/cgra_inst/cgra/" if backend == "cgra" else "AsicCore/"
    for row in csv.DictReader((OUT / f"{backend}.tsv").open(), delimiter="\t"):
        path = row["path"]
        duplicate += path in seen
        seen.add(path)
        label = category(backend, row)
        area = float(row["area"])
        if "/ctrl_ring/" in path:
            part = (
                "packet_buffer_bits"
                if row["sequential"] == "true"
                and re.fullmatch(r"regs_reg\[.*", path.rsplit("/", 1)[-1])
                else "remaining_ring_logic_and_state"
            )
            ring[part]["cells"] += 1
            ring[part]["area"] += area
        for table in [totals] + ([core] if path.startswith(root) else []):
            table[label]["cells"] += 1
            table[label]["area"] += area
        if label != "retained_compute_control":
            continue
        if backend == "cgra" and path.startswith(root):
            local = path[len(root) :]
            if local.startswith("tile__"):
                group = local.split("/")[1]
                if group.startswith(("tile_in_channel", "tile_out_or_link")):
                    group = "tile_channels"
                elif group not in {
                    "const_mem",
                    "ctrl_mem",
                    "element",
                    "fu_crossbar",
                    "register_cluster",
                    "routing_crossbar",
                }:
                    group = "tile_local_control"
            else:
                group = local.split("/")[0]
                if group not in {"bypass_queue", "controller", "ctrl_ring", "data_mem"}:
                    group = "core_local_control"
        else:
            group = (
                path.split("/")[1]
                if path.startswith(root)
                else "endpoint_adapters_dma_and_shell"
            )
            if path.startswith(root) and group not in {
                "memory",
                "gcn",
                "norm",
                "softmax",
                "gpt_vector",
                "vector_unit",
            }:
                group = "core_local_control"
        retained[group]["cells"] += 1
        retained[group]["area"] += area
    return {
        "categories": dict(totals),
        "core_categories": dict(core),
        "retained_breakdown": dict(retained),
        "control_ring": dict(ring),
        "duplicate_paths": duplicate,
        "total_area": sum(v["area"] for v in totals.values()),
    }


def main():
    result = {backend: summarize(backend) for backend in RULES}
    reports = {
        "cgra": ROOT
        / "build/experiments/asic/cgra-cost-complete/reports/checkpoint/endpoint/recovered/area.rpt",
        "asic": ROOT
        / "build/experiments/asic/cost-complete/checkpoints/asic-sram-20261113-v1/sta2/area.rpt",
    }
    hierarchy = {k: read(p) for k, p in reports.items()}
    checks = {}
    for backend in RULES:
        report_area = next(iter(hierarchy[backend].values()))["area"]
        residual = result[backend]["total_area"] - report_area
        result[backend]["report_area"] = report_area
        result[backend]["area_residual"] = residual
        checks[f"{backend}_area_reconciles"] = abs(residual) < 1.0
        checks[f"{backend}_unique_paths"] = result[backend]["duplicate_paths"] == 0
    tile = r"cgra/cgra_inst/cgra/tile__\d+/"
    names = [
        "add",
        "mul",
        "div",
        "logic",
        "shift",
        "cmp",
        "sel",
        "grant",
        "phi",
        "mem",
        "return",
        "gep",
        "nah",
    ]
    result["fu_breakdown"] = {
        name: select(hierarchy["cgra"], tile + f"element/fu__{i}")
        for i, name in enumerate(names)
    }
    result["cgra_config_hierarchy"] = {
        name: select(hierarchy["cgra"], tile + name)
        for name in ["ctrl_mem", "ctrl_mem/reg_file", "const_mem", "const_mem/reg_file"]
    }
    result["scope"] = (
        "Original complete CGRA and general five-unit ASIC checkpoints; not the split Add/ReLU baseline or newly pipelined Softmax."
    )
    result["units"] = "SAED14 library area units; not place-and-route area."
    result["definition"] = (
        "Compute plus control excluding identified bulk storage cells. Remove SRAM macros and named configuration/data array flops only. Retain array decode/read/write mux logic, operand register files, FIFO/queue registers, FSMs, pipeline registers, prologue counters, interconnect, validity state, and adapters/DMA unless reporting the separate core subtotal."
    )
    result["limitations"] = [
        "This accounting boundary is not a synthesized memory-free accelerator. Access logic remains and depends on the original storage sizes.",
        "Control includes mutable prologue counters and configuration-cache validity metadata. Bulk configuration payload arrays are separated.",
        "Installed compute resources differ: reusable 16-PE CGRA versus the five dedicated ASIC units, excluding their common Gemmini.",
        "Both original mapped endpoints fail 2 ns. These areas do not qualify matched-frequency PPA.",
        "Complete endpoint and core subtotals have distinct boundaries; neither should be silently substituted for the other.",
        "Separating array access logic from shared arithmetic/control would require a different boundary and additional structural evidence; these results retain that logic on both sides.",
    ]
    result["classification_regex"] = RULES
    result["checks"] = checks
    result["passed"] = all(checks.values())
    sources = list(reports.values()) + [
        OUT / "asic.tsv",
        OUT / "cgra.tsv",
        OUT / "inventory.tcl",
        ROOT
        / "build/experiments/asic/cost-complete/checkpoints/asic-sram-20261113-v1/AsicAccelerator/out/mapped.ddc",
        ROOT
        / "build/experiments/asic/cgra-cost-complete/reports/checkpoint/endpoint/recovered/mapped.ddc",
    ]
    result["sha256"] = {
        str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sources
    }
    (OUT / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: {
                    "total": result[k]["total_area"],
                    "retained_endpoint": result[k]["categories"][
                        "retained_compute_control"
                    ],
                    "retained_core": result[k]["core_categories"][
                        "retained_compute_control"
                    ],
                }
                for k in RULES
            },
            indent=2,
        )
    )
    print("checks:", checks)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
