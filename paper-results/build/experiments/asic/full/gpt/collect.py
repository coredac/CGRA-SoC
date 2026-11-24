"""Collect the four GPT configurations without mixing missing or failed samples."""

import hashlib
import json
from pathlib import Path
import statistics
import sys

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[4]


def fields(line):
    result = {}
    for word in line.split():
        if "=" not in word:
            continue
        key, value = word.split("=", 1)
        result[key] = int(value) if value.lstrip("-").isdigit() else value
    return result


def collect(name):
    folder = OUT / "runs" / name
    meta = json.loads((folder / "run.json").read_text())
    lines = (folder / "run.log").read_text().splitlines()
    samples = [
        fields(line) for line in lines if line.startswith("GPT COMPARE backend=")
    ]
    hardware = [
        fields(line) for line in lines if line.startswith("GPT COMPARE ASIC hardware ")
    ]
    passed = (
        meta.get("exit") == 0 and "GPT COMPARE: PASS" in lines and len(samples) == 4
    )
    passed = passed and [row["sample"] for row in samples] == [0, 1, 2, 3]
    passed = passed and [row["input"] for row in samples] == [0, 1, 0, 1]
    passed = passed and all(
        row["checked"] == 256 and row["mismatches"] == row["status"] == 0
        for row in samples
    )
    passed = passed and all(
        row["configured_jobs"] == (20 if row["sample"] == 0 else 0) for row in samples
    )
    passed = passed and (
        len(hardware) == 4 if name.endswith("asic") else len(hardware) == 0
    )
    warm = [row["total_cycles"] for row in samples if row["sample"] > 0]
    summary = {"count": len(warm)}
    if warm:
        summary.update(
            mean=statistics.mean(warm),
            median=statistics.median(warm),
            minimum=min(warm),
            maximum=max(warm),
        )
    return {
        "passed": passed,
        "execution": meta,
        "samples": samples,
        "asic_hardware_counters": hardware,
        "warm_total_cycles": summary,
        "log_sha256": hashlib.sha256((folder / "run.log").read_bytes()).hexdigest(),
    }


def main():
    names = ["operator-cgra", "operator-asic", "chain-cgra", "chain-asic"]
    runs = {name: collect(name) for name in names}
    result = {
        "passed": all(run["passed"] for run in runs.values()),
        "scope": "Complete small GPT block: N2, hidden128,2heads, FF512; archived ASIC0 SoC, validated dual-lane CGRA maps. Four configurations; one cold plus three warm runs alternating two fixture inputs.",
        "cycle_scope": "CPU rdcycle interval includes cold configuration, packing, DMA, all kernels and final output staging; reference comparisons and printed output excluded. Pipeline excludes configuration interval.",
        "strategy_labels": {
            "operator": "Each nonmatrix phase submitted separately through AutoLink",
            "chain": "Local cached-kernel chains for LN, Softmax and FC1/GELU; CPU orchestrates the block. Not full graphAuto.",
        },
        "locality": "Operator already keeps LN/Softmax intermediates in local SPM. Chain reduces 51 to 18 submissions; FC1/GELU alone avoids a 1024-word output/reload roundtrip (8192 bytes). Both backends receive this benefit; full block remains CPU-orchestrated.",
        "counter_scope": "ASIC counters are hardware deltas. Configured jobs, submissions, Gemmini GEMMs and backend DMA bytes are instrumented software accounting. No new CGRA hardware counters invented.",
        "limitations": [
            "Functional RTL cycle comparison; no physical timing or full hardware cost claim.",
            "Warm samples alternate two existing inputs; not three distinct new inputs or independent statistical trials.",
            "Simulator retains validated homogeneous hardware; isolated hetero FU and mixed-width SRAM candidates are not part of this SoC run.",
            "Full-size workloads and additional scheduling strategies are outside these four configurations.",
            "CGRA includes power-of-two strength reduction and two-lane maps; ASIC vector retains its tested generic division implementation. ASIC could also add a power-of-two fast path.",
        ],
        "runs": runs,
        "comparisons": {},
        "inputs": json.loads((OUT / "inputs.json").read_text()),
        "report_sources_sha256": {
            name: hashlib.sha256((OUT / name).read_bytes()).hexdigest()
            for name in ["collect.py", "METHOD.md", "source.diff"]
        },
    }
    for mode in ["operator", "chain"]:
        cgra = runs[mode + "-cgra"]["warm_total_cycles"]["mean"]
        asic = runs[mode + "-asic"]["warm_total_cycles"]["mean"]
        result["comparisons"][mode] = {
            "warm_cgra_mean_cycles": cgra,
            "warm_asic_mean_cycles": asic,
            "asic_over_cgra_speedup": asic / cgra,
            "cgra_fewer_cycles_percent": (1 - cgra / asic) * 100,
        }
    (OUT / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {"passed": result["passed"], "comparisons": result["comparisons"]}, indent=2
        )
    )
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
