# Multi-ASIC area scenario

This requested scenario uses the final modified multi-ASIC core areas, not complete endpoint costs. Every AE number below is conditional on retaining historical end-to-end cycle ratios. That assumption has not been validated: the expanded ASIC native schedules and GPT timing repair change cycle counts. These numbers must not be presented as measured AE of the modified hardware.

| ASIC core | Native role | Primitive lanes | Area including local storage |
| --- | --- | ---: | ---: |
| Add | Elementwise addition | 16 | 34,553.86 |
| ReLU | ReLU activation | 16 | 34,550.53 |
| GCN | Sparse graph feature aggregation with optional native ReLU | 16 | 176,676.04 |
| Norm | LayerNorm stages and normalization | 16 | 65,169.30 |
| Softmax | Softmax stages | 16 | 34,774.30 |
| GPT-vector | GELU approximation, requantization, residual add and saturation | 16 | 217,356.74 |
| SAD (optional) | Sum of absolute differences, independently provisioned sequencer | 16 | 35,790.17 |

CGRA area is 544,538.07; six ASICs total 563,080.75; seven including SAD total 598,870.93 SAED14 library area units. Corresponding CGRA area reductions are 3.29% and 9.07%.

Each family contains native control/local-array logic and one sixteen-lane PrimitiveBank exposing integer arithmetic, logical operations, comparisons, selection and address arithmetic. Add/ReLU native streams use lane zero, GPT kernels retain scalar native schedules, and GCN schedules its sixteen channels through the bank. Installed lanes do not establish equal useful throughput to the sixteen CGRA PEs.

The area boundary includes compute, native control, local/configuration arrays and their access, arbitration, and pipeline/runtime registers. It excludes endpoint adapters and their descriptor caches, DMA, external SPM and its access, SoC interconnect, and common CPU/GEMM. This is a core-area scenario, not complete hardware cost.

| Workload | Historical speedup | Conditional AE, six ASICs | Conditional AE, seven ASICs |
| --- | ---: | ---: | ---: |
| ResNet | 1.0798 | 1.1166 | 1.1875 |
| GPT-2 | 1.0211 | 1.0559 | 1.1230 |
| GCN | 1.0045 | 1.0387 | 1.1047 |
| Arithmetic mean | 1.0351 | 1.0704 | 1.1384 |

Each AE multiplies historical speedup by total installed ASIC core area divided by CGRA core area. The same full six- or seven-family inventory is charged to every workload in its scenario, including inactive cores. The arithmetic mean gives each workload equal weight, but the historical speedups themselves use different old ASIC baselines.

SAD is optional and is not exercised by the three listed workloads. Adding it represents broader independently provisioned hardware support, not a necessary area charge for those workloads. Keep the six-family case visible and identify the seven-family case as extra provisioning.

All seven selected ASIC cores meet the reported 2 ns synthesis constraints. The CGRA genuine internal path still has -0.759389 ns slack. There is no full-workload performance rerun on either modified core set, so no same-frequency physical AE conclusion is established.

[Final seven-family area](../hybrid/workload-trial/results.md), [six-family comparison](../hybrid/revised-area/results.md), [expanded native scheduling](../hybrid/capability-trial/README.md), [historical performance](borrowed-area.json), [this scenario provenance](multi-asic.json), [CSV](multi-asic.csv).
