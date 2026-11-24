# Current-revision supplementary backend measurements

All six regional-Hybrid configurations pass: 24 executions and 26,624 checked outputs. The separate ASIC AUTO interface test passes both inputs. Primary P1's 27-case matrix is unchanged.

Warm execution is the primary steady-state metric: configuration is resident, while execution, transfers and completion remain timed. Cold execution is retained separately as a configuration-cost diagnostic; the arbitrary one-cold/three-warm sampling mix is not the primary performance metric.

| Workload | Backend | Warm mean cycles | Warm range |
| --- | --- | ---: | ---: |
| ResNet | CGRA | 69,734.00 | 69,336–70,501 |
| ResNet | ASIC | 59,205.33 | 58,759–59,985 |
| GCN | CGRA | 93,864.33 | 93,057–95,221 |
| GCN | ASIC | 94,282.67 | 93,550–95,722 |
| GPT | CGRA | 231,670.67 | 229,535–235,201 |
| GPT | ASIC | 236,560.67 | 234,202–240,165 |

| Workload | CGRA fewer warm cycles |
| --- | ---: |
| ResNet | -17.78% |
| GCN | +0.44% |
| GPT | +2.07% |

Negative values mean CGRA uses more cycles. All setup, transfers, computation and final visibility are timed; numerical checking and printing are excluded. Input order is 0,1,0,1 with no CPU-cache flush. The three warm samples characterize this deterministic sequence, not independent statistical trials.

Separate startup diagnostic (the recorded finite sequence, not an assumed deployment workload):

| Workload | Backend | Cold cycles | Cold + 3 warm cycles |
| --- | --- | ---: | ---: |
| ResNet | CGRA | 127,210 | 336,412 |
| ResNet | ASIC | 64,473 | 242,089 |
| GCN | CGRA | 179,722 | 461,315 |
| GCN | ASIC | 100,905 | 383,753 |
| GPT | CGRA | 569,754 | 1,264,766 |
| GPT | ASIC | 276,349 | 986,031 |

Both backends use the same documented logical regions and cached programs. GCN uses an aggregation+ReLU region, GPT uses 18 non-matrix regions, and ResNet uses ReLU and fused AddReLU regions. CPU-submitted Gemmini work and all host conversions remain included. This is handwritten regional Hybrid, not the P1 compiler-policy experiment or whole-graph Auto.

This general fused-ASIC/host-intermediate ResNet comparison does not replace the earlier split Add/ReLU ASIC with on-chip exchange, where CGRA has measured warm-cycle advantages. Backend fusion, data placement and submission policy differ. See the [historical reconciliation](../reconciliation.md); do not combine that split design's performance with this general design's area.

These are fresh RTL CPU-cycle measurements on the current frozen general-ASIC revision. The nominal simulated endpoint clock is 500 MHz; physical timing closure must be checked separately. The 10 ns synthesis target is a different condition. Area, 10 ns timing and 2 ns STA must retain their distinct labels; no achievable physical latency is inferred from these cycles alone.

Complete endpoint cost uses the same SAED14 RVT TT0.8V25C logic and SRAM libraries. Both totals include all supported compute units, local memory, configuration, adapters, DMA and publication. Shared Rocket, Gemmini and upstream AutoLink are excluded.

| Backend | Standard-cell area, excluding SRAM macros | SRAM area | Complete cell area (library units) | SRAM macros | Setup slack at 10 ns | Setup slack at 2 ns |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| CGRA | 1,403,042.66 | 4,923,300.56 | 6,326,343.22 | 800 | -23.3800 ns | -31.3800 ns |
| ASIC | 606,661.40 | 1,349,005.88 | 1,955,667.27 | 256 | -31.2429 ns | -39.2429 ns |

The standard-cell column includes compute, control, adapters, DMA, configuration registers and storage arrays implemented in standard cells, including ASIC unit-local arrays. It is not pure arithmetic area or area with every memory removed. Complete area includes data SPM and configuration SRAM. The [SPM comparison contract](../reconciliation.md#spm-comparison-contract) distinguishes the existing shared-SPM ASIC from a prospective independent-operator design.

CGRA area is 3.235× ASIC area in this implementation. Neither backend meets 10 ns or 2 ns setup timing. Final STA removes optimizer-created temporary path exceptions and uses unchanged mapped netlists; coverage checks report no unconstrained endpoints or missing input delays. These are pre-layout mapped estimates, with interrupted optimization in the CGRA endpoint and the ASIC GptVector child. They do not establish lower CGRA cost, physical 500 MHz operation, placed-and-routed area, power or energy.

[CGRA cost and STA](../../cgra-cost-complete/results.json), [ASIC cost and STA](../../cost-complete/results.json), [independent acceptance review](../../cost-review/review.json). Memory transformations have directed cycle-equivalence checks; full mapped-SoC gate-level verification and formal equivalence are not claimed.

[Raw results and provenance](results.json), [samples](samples.csv), [region audit](../audit/regions.json), [hardware manifest](../../cost-complete/revision/manifest.json). The earlier frozen-simulator measurements remain separate under the parent directory. Every selected run has a normal successful exit, output checks, unchanged input hashes and a recorded raw-log hash. Interrupted attempts are retained separately.

Source report SHA256: `5d240cc89a1862524b5d8e380d327c7f59d16ddb76dd0e4763644bfb3edc3419`. Regenerate with `python3 build/experiments/asic/hybrid/current/summarize.py`.
CGRA cost report SHA256: `d480ad3cbdabc39b0131a8c2ca38053f0119f16db699e425e252da2d0be6c527`.
ASIC cost report SHA256: `b089567a85e8f7b05528d692692f4f1aa22b13b3e053e60174db111c0ce42d32`.
