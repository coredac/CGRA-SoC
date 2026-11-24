# GPT comparison with the compact-layout reference

This revision uses review-v20 for CGRA entries. The single-CGRA data SPM is projected at 32 KiB, and every AE value uses its updated per-task reference area. The ASIC projection retains its previous absolute area, cycles, three auxiliary cores and shared 128 KiB SPM. These are modeled comparisons, not newly synthesized or timing-closed implementations.

| Composition | per-task AE | per-segment AE | per-graph AE |
| --- | ---: | ---: | ---: |
| 1 GEMM + 1 CGRA | 1.000 | 0.894 | 0.849 |
| 1 GEMM + 4 CGRA | 1.007 | 1.573 | 1.666 |
| 1 GEMM + 3 ASIC cores (estimate) | 1.013 | 1.016 | 0.977 |

| Composition | Policy | Cycles (million) | Area (million library units) | Global speedup | AE |
| --- | --- | ---: | ---: | ---: | ---: |
| 1 GEMM + 1 CGRA | per-task | 18.2709 | 8.7324 | 1.0000 | 1.0000 |
| 1 GEMM + 1 CGRA | per-segment | 16.5398 | 10.7867 | 1.1047 | 0.8943 |
| 1 GEMM + 1 CGRA | per-graph | 16.2623 | 11.5503 | 1.1235 | 0.8494 |
| 1 GEMM + 4 CGRA | per-task | 12.6891 | 12.4897 | 1.4399 | 1.0067 |
| 1 GEMM + 4 CGRA | per-segment | 5.1173 | 19.8187 | 3.5704 | 1.5732 |
| 1 GEMM + 4 CGRA | per-graph | 4.4345 | 21.6021 | 4.1202 | 1.6655 |
| 1 GEMM + 3 ASIC cores | per-task | 18.6592 | 8.4384 | 0.9792 | 1.0133 |
| 1 GEMM + 3 ASIC cores | per-segment | 16.6725 | 9.4190 | 1.0959 | 1.0160 |
| 1 GEMM + 3 ASIC cores | per-graph | 16.5915 | 9.8426 | 1.1012 | 0.9770 |

The following values normalize each composition independently to its best AE. They describe the policy ranking within a composition, not absolute efficiency across compositions.

| Composition | per-task relative AE | per-segment relative AE | per-graph relative AE |
| --- | ---: | ---: | ---: |
| 1 GEMM + 1 CGRA | 1.0000 | 0.8943 | 0.8494 |
| 1 GEMM + 4 CGRA | 0.6044 | 0.9446 | 1.0000 |
| 1 GEMM + 3 ASIC cores | 0.9974 | 1.0000 | 0.9616 |

The within-composition winner is unchanged: per-task for one CGRA, per-graph for four CGRAs, and nominally per-segment for the ASIC estimate. The latter is only 0.265% above per-task and should be treated as a near tie. For four CGRAs and ASICs, the internal relative-AE bars are unchanged because all policies share the same normalization-factor change. Single-CGRA segment/graph relative AE decreases from 0.9275/0.8903 to 0.8943/0.8494.

The ASIC composition has four compute IPs: one GEMM plus Norm, Softmax and GPT-vector. Its three auxiliary cores share the original modeled 128 KiB SPM and one upstream command endpoint. Its SPM was not resized to match the projected CGRA data memory, so this is not an equal-SRAM ASIC comparison. The old N2-to-N64 cycle extrapolation, peripheral/control scaling and physical-timing limitations remain unchanged.

All speedups here use the common GPT 1-CGRA per-task latency reference; the main speedup chart uses the per-task reference at each CGRA count. Original experiments and the preceding table remain unchanged.

[refresh.py](refresh.py) reproduces [results.json](results.json), [cases.csv](cases.csv), [comparison.json](comparison.json) and [validation.json](validation.json).
