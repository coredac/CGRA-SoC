"""Attribute existing mapped area by preserved hierarchy, without resynthesis."""

import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
BASE = ROOT / "build/experiments/asic"


def read(path):
    text = path.read_text().split("Hierarchical area distribution", 1)[1]
    text = re.sub(r"(\S+)\n\s+(?=\d)", r"\1 ", text)
    pattern = (
        r"\s*(\S+)\s+([\d.]+)\s+[\d.]+\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+(\S+)\s*"
    )
    rows = {}
    for line in text.splitlines():
        match = re.fullmatch(pattern, line)
        if match:
            rows[match[1]] = {
                "area": float(match[2]),
                "local_area": sum(float(match[i]) for i in (3, 4, 5)),
                "local_sram_area": float(match[5]),
                "design": match[6],
            }
    return rows


def select(rows, pattern):
    chosen = {path: row for path, row in rows.items() if re.fullmatch(pattern, path)}
    return {
        "count": len(chosen),
        "area": sum(r["area"] for r in chosen.values()),
        "instances": chosen,
    }


def main():
    cgra_path = (
        BASE / "cgra-cost-complete/reports/checkpoint/endpoint/recovered/area.rpt"
    )
    asic_path = BASE / "cost-complete/checkpoints/asic-sram-20261113-v1/sta2/area.rpt"
    cgra, asic = read(cgra_path), read(asic_path)
    tile = r"cgra/cgra_inst/cgra/tile__\d+/"
    result = {
        "method": "Existing mapped hierarchy attribution; no new synthesis or timing qualification",
        "units": "SAED14 library area units",
        "cgra": {
            "fu_subtrees": select(cgra, tile + r"element/fu__\d+"),
            "elements_including_selection": select(cgra, tile + "element"),
            "register_clusters": select(cgra, tile + "register_cluster"),
            "control_memories": select(cgra, tile + "ctrl_mem"),
            "constant_memories": select(cgra, tile + "const_mem"),
        },
        "asic": {
            "units_including_local_arrays": select(
                asic, r"AsicCore/(gcn|norm|softmax|gpt_vector|vector_unit)"
            ),
            "gpt_vector_stores": select(asic, r"AsicCore/gpt_vector/\w+_store"),
            "pure_compute_area": None,
        },
        "limitations": [
            "FU subtrees include each FU's internal handshake/control. This is not an arithmetic-gates-only result.",
            "Element and FU categories overlap; do not add them together.",
            "ASIC GCN/Norm arrays are flattened into unit cells. A symmetric memory-free compute total cannot be inferred by subtracting SRAM macros.",
            "These are the general ASIC and current CGRA checkpoints, not the independent Add/ReLU hardware's cost/performance point.",
            "Both mapped endpoints fail the common clock targets; neither provides qualified physical-frequency area/performance.",
        ],
        "sources": {
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (cgra_path, asic_path)
        },
    }
    checks = {
        "cgra_fu_count": result["cgra"]["fu_subtrees"]["count"] == 16 * 13,
        "cgra_element_count": result["cgra"]["elements_including_selection"]["count"]
        == 16,
        "asic_unit_count": result["asic"]["units_including_local_arrays"]["count"] == 5,
    }
    for name, rows in (("cgra", cgra), ("asic", asic)):
        total = next(iter(rows.values()))["area"]
        local = sum(row["local_area"] for row in rows.values())
        checks[name + "_local_area_reconciles"] = abs(total - local) < 1.0
        result[name]["hierarchy_area_residual"] = total - local
    result["checks"] = checks
    result["passed"] = all(checks.values())
    (OUT / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "passed": result["passed"],
                "checks": checks,
                "cgra_fu_area": result["cgra"]["fu_subtrees"]["area"],
            }
        )
    )
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
