# Performance and area evidence

## Figure 1(b)

[The nine source rows](../build/experiments/policy-check/gpt-three-way-v2/cases.csv) contain cycles, area and AE for GPT-2. The ASIC rows use a three-core proxy based on earlier measurements. [The source description](../build/experiments/policy-check/gpt-three-way-v2/README.md) records the assumptions.

| Composition | Published per-task / per-segment / per-graph | Source AE divided by each row's per-task AE |
| --- | --- | --- |
| GEMM + 1 CGRA | 1.00 / 0.89 / 0.85 | 1.00 / 0.89 / 0.85 |
| GEMM + 4 CGRAs | 1.00 / 1.57 / 1.67 | 1.00 / 1.56 / 1.65 |
| GEMM + 3 ASIC cores | 1.00 / 1.02 / 0.98 | 1.00 / 1.00 / 0.96 |

The last two columns in the paper match the source AE rounded to two decimals using the common GPT single-CGRA reference. The paper prints 1.00 for every per-task cell. Applying the within-composition normalization specified by the caption produces the rightmost column above. Full precision preserves the ASIC per-segment advantage of roughly 0.26% over per-task.

## Figure 4(a)

[The figure data](../mainres-1c.csv) give the speedup and AE for all 27 bars of each measure, with each workload normalized to its single-CGRA per-task case. The PDF path heights of all 54 bars match the [standalone plot](../main-figure/mainres-1c.pdf). It retains an earlier axis-label wording. [The plot source](../mainres-1c.tex) and [the conversion from the original plotting template](../main-figure/rebase.py) preserve the calculations. The arithmetic means of the nine per-segment and per-graph speedups are 2.1303 and 2.7328. The corresponding AE means are 1.1492 and 1.4678. The mean of the nine per-graph/per-segment speedup ratios is 1.4078.

[The baseline measurements](../build/experiments/p1/summary27/results.json) contain all 27 historical cold/warm pairs. The 54 cycle readings trace to 52 distinct SHA-256-verified files, including run logs. Each point has a single cold and warm observation. The final figure uses 12 archived cycle values, 9 archived GCN values from the earlier design, 5 segment sensitivity estimates and 1 earlier cycle value under an unchanged-execution assumption. The per-row cycle and area status in the CSV identifies the evidence for each displayed value. The large runs used checked=0.

## Figure 4(b)

[The stacked-bar data](../build/experiments/p1/summary27/figures/review-v20/area-relative.csv) give the component-area fractions relative to the per-graph total for each of the three single-CGRA configurations. All 69 nonzero segment heights in the local source PDF agree with the published vector paths. The plot uses a projected 32 KiB data SPM for the single-CGRA GPT configuration. Component accounting and policy assumptions are preserved in [the area method](../build/experiments/p1/summary27/figures/review-v20/area-method.json) and [model inputs](../build/experiments/p1/summary27/figures/review-v20/method.json).

[Raw synthesis reports](saed14-selected-reports.tar.gz) cover 83 selected component jobs. All 415 files were read from the compressed archive and verified against the [synthesis index](../build/experiments/p1/summary27/hardware/layers.json) and [per-file checksums](SHA256SUMS.members). Archive entries retain the original server paths relative to /tmp. Historical manifests elsewhere in the archive can contain original absolute paths. Their local files retain the repository-relative directory structure under this archive.
