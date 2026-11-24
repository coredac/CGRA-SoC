"""Summarize completed measurements of the frozen current hardware revision."""

import hashlib
import json
from pathlib import Path
import statistics
import sys

OUT = Path(__file__).resolve().parent


def main():
    path = OUT / "results.json"
    if not path.is_file():
        print("The current-revision measurement is not complete.")
        return 1
    result = json.loads(path.read_text())
    if not result["passed"]:
        print("The current-revision report has not passed validation.")
        return 1
    cost_paths = {
        "CGRA": OUT.parents[1] / "cgra-cost-complete/results.json",
        "ASIC": OUT.parents[1] / "cost-complete/results.json",
    }
    costs = {
        name: json.loads(source.read_text()) for name, source in cost_paths.items()
    }
    if not all(
        item["complete_mapped_endpoint"] and item["sta_complete"]
        for item in costs.values()
    ):
        print("Complete endpoint area and STA reports are required.")
        return 1
    lines = [
        "# Current-revision supplementary backend measurements",
        "",
        f"All six regional-Hybrid configurations pass: {result['execution_samples']} executions and {result['checked_outputs']:,} checked outputs. The separate ASIC AUTO interface test passes both inputs. Primary P1's 27-case matrix is unchanged.",
        "",
        "Warm execution is the primary steady-state metric: configuration is resident, while execution, transfers and completion remain timed. Cold execution is retained separately as a configuration-cost diagnostic; the arbitrary one-cold/three-warm sampling mix is not the primary performance metric.",
        "",
        "| Workload | Backend | Warm mean cycles | Warm range |",
        "| --- | --- | ---: | ---: |",
    ]
    comparison = []
    startup = []
    for workload in ("ResNet", "GCN", "GPT"):
        pair = {}
        for backend in ("CGRA", "ASIC"):
            samples = [
                row
                for row in result["samples"]
                if row["workload"] == workload and row["backend"] == backend
            ]
            warm = [row["total_cycles"] for row in samples if row["phase"] == "warm"]
            cold = next(
                row["total_cycles"] for row in samples if row["phase"] == "cold"
            )
            mean, total = statistics.mean(warm), sum(warm) + cold
            pair[backend] = mean, total
            lines.append(
                f"| {workload} | {backend} | {mean:,.2f} | {min(warm):,}–{max(warm):,} |"
            )
            startup.append(f"| {workload} | {backend} | {cold:,} | {total:,} |")
        comparison.append(
            (
                workload,
                100 * (1 - pair["CGRA"][0] / pair["ASIC"][0]),
                100 * (1 - pair["CGRA"][1] / pair["ASIC"][1]),
            )
        )
    lines += [
        "",
        "| Workload | CGRA fewer warm cycles |",
        "| --- | ---: |",
    ]
    lines.extend(f"| {name} | {warm:+.2f}% |" for name, warm, total in comparison)
    lines += [
        "",
        "Negative values mean CGRA uses more cycles. All setup, transfers, computation and final visibility are timed; numerical checking and printing are excluded. Input order is 0,1,0,1 with no CPU-cache flush. The three warm samples characterize this deterministic sequence, not independent statistical trials.",
        "",
        "Separate startup diagnostic (the recorded finite sequence, not an assumed deployment workload):",
        "",
        "| Workload | Backend | Cold cycles | Cold + 3 warm cycles |",
        "| --- | --- | ---: | ---: |",
        *startup,
        "",
        "Both backends use the same documented logical regions and cached programs. GCN uses an aggregation+ReLU region, GPT uses 18 non-matrix regions, and ResNet uses ReLU and fused AddReLU regions. CPU-submitted Gemmini work and all host conversions remain included. This is handwritten regional Hybrid, not the P1 compiler-policy experiment or whole-graph Auto.",
        "",
        "This general fused-ASIC/host-intermediate ResNet comparison does not replace the earlier split Add/ReLU ASIC with on-chip exchange, where CGRA has measured warm-cycle advantages. Backend fusion, data placement and submission policy differ. See the [historical reconciliation](../reconciliation.md); do not combine that split design's performance with this general design's area.",
        "",
        "These are fresh RTL CPU-cycle measurements on the current frozen general-ASIC revision. The nominal simulated endpoint clock is 500 MHz; physical timing closure must be checked separately. The 10 ns synthesis target is a different condition. Area, 10 ns timing and 2 ns STA must retain their distinct labels; no achievable physical latency is inferred from these cycles alone.",
        "",
        "Complete endpoint cost uses the same SAED14 RVT TT0.8V25C logic and SRAM libraries. Both totals include all supported compute units, local memory, configuration, adapters, DMA and publication. Shared Rocket, Gemmini and upstream AutoLink are excluded.",
        "",
        "| Backend | Standard-cell area, excluding SRAM macros | SRAM area | Complete cell area (library units) | SRAM macros | Setup slack at 10 ns | Setup slack at 2 ns |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for name, cost in costs.items():
        lines.append(
            f"| {name} | {cost['standard_cell_area_library_units']:,.2f} | {cost['sram_area_library_units']:,.2f} | {cost['cell_area_library_units']:,.2f} | {cost['sram_instances']} | {cost['timing10']['worst_setup_slack_ns']:.4f} ns | {cost['timing2']['worst_setup_slack_ns']:.4f} ns |"
        )
    ratio = (
        costs["CGRA"]["cell_area_library_units"]
        / costs["ASIC"]["cell_area_library_units"]
    )
    lines += [
        "",
        "The standard-cell column includes compute, control, adapters, DMA, configuration registers and storage arrays implemented in standard cells, including ASIC unit-local arrays. It is not pure arithmetic area or area with every memory removed. Complete area includes data SPM and configuration SRAM. The [SPM comparison contract](../reconciliation.md#spm-comparison-contract) distinguishes the existing shared-SPM ASIC from a prospective independent-operator design.",
        "",
        f"CGRA area is {ratio:.3f}× ASIC area in this implementation. Neither backend meets 10 ns or 2 ns setup timing. Final STA removes optimizer-created temporary path exceptions and uses unchanged mapped netlists; coverage checks report no unconstrained endpoints or missing input delays. These are pre-layout mapped estimates, with interrupted optimization in the CGRA endpoint and the ASIC GptVector child. They do not establish lower CGRA cost, physical 500 MHz operation, placed-and-routed area, power or energy.",
        "",
        "[CGRA cost and STA](../../cgra-cost-complete/results.json), [ASIC cost and STA](../../cost-complete/results.json), [independent acceptance review](../../cost-review/review.json). Memory transformations have directed cycle-equivalence checks; full mapped-SoC gate-level verification and formal equivalence are not claimed.",
        "",
        "[Raw results and provenance](results.json), [samples](samples.csv), [region audit](../audit/regions.json), [hardware manifest](../../cost-complete/revision/manifest.json). The earlier frozen-simulator measurements remain separate under the parent directory. Every selected run has a normal successful exit, output checks, unchanged input hashes and a recorded raw-log hash. Interrupted attempts are retained separately.",
        "",
        f"Source report SHA256: `{hashlib.sha256(path.read_bytes()).hexdigest()}`. Regenerate with `python3 build/experiments/asic/hybrid/current/summarize.py`.",
    ]
    lines.extend(
        f"{name} cost report SHA256: `{hashlib.sha256(source.read_bytes()).hexdigest()}`."
        for name, source in cost_paths.items()
    )
    (OUT / "results.md").write_text("\n".join(lines) + "\n")
    print("Wrote", OUT / "results.md")
    return 0


if __name__ == "__main__":
    sys.exit(main())
