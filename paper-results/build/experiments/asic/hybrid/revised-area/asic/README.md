# Expanded ASIC whole-core area before the GPT timing repair

All six independent `ExpandedAsic` families were mapped from the frozen [capability-trial RTL](../../capability-trial/rtl/ExpandedAsic.sv). The family parameter is the only specialization of the common translated source. [job.json](job.json) binds source and constraint hashes; [results.json](results.json) contains audited measurements and exact storage inventories.

The comparison includes computation, native control, primitive-bank arbitration, pipeline/runtime registers and local-array access logic. It excludes endpoint adapters, DMA, external SPM and SoC interconnect. Arrays remain present during synthesis. The storage-excluded figure subtracts only the mapped sequential cells of named bulk arrays; it retains their decoder, read selection and write-enable circuitry.

| Family | Full cell area | Local-array storage | Storage-excluded area | Worst setup slack (ns) | Violating setup endpoints |
| --- | ---: | ---: | ---: | ---: | ---: |
| add | 34,553.855376 | 0.000000 | 34,553.855376 | +0.104329 | 0 |
| relu | 34,550.525372 | 0.000000 | 34,550.525372 | +0.104401 | 0 |
| gcn | 176,676.036229 | 70,200.307200 | 106,475.729029 | +0.000325 | 0 |
| norm | 65,169.297921 | 15,276.441600 | 49,892.856321 | +0.104401 | 0 |
| softmax | 34,774.301361 | 149.184000 | 34,625.117361 | +0.037377 | 0 |
| gpt_vector | 217,237.700918 | 91,658.649600 | 125,579.051318 | -0.234015 | 4 |
| Total | 562,961.717177 | 177,284.582400 | 385,677.134777 | — | 4 |

Area is in SAED14 library area units. All jobs use SAED14 RVT TT 0.8 V/25 C, a 2 ns clock, 0.1 ns input/output delay, 0.1 ns input transition, output load 1 library unit, maximum transition 0.2 ns, maximum fanout 16, enclosed wire-load mode and `compile -map_effort medium -area_effort high`. This is ideal-clock, wire-load synthesis; it is not physical timing signoff.

Five families meet the synthesis constraints. The original expanded GPT-vector family has four real setup violations: product→biased[31/30/29] at −0.234015/−0.154168/−0.050129 ns and delta→square[31] at −0.070336 ns. Their total negative slack is −0.508648 ns. They belong to the native BIAS constant addition and SQUARE multiplication, not reset, DMA or configuration paths. The isolated [timing repair](../gpt-fix/source/manifest.json) has now been mapped at identical constraints; its [audited result](../gpt-fix/mapped/results.json) meets setup with +0.002648 ns slack and replaces only the GPT-vector row in the parent comparison. This file and results.json retain the before measurement.

GCN retains 20 bulk arrays and 75,296 mapped one-bit storage cells: 18 banks of 4,096 cells, 544 row-pointer cells and 1,024 column-index cells. Only four column-index bits are consumed, so synthesis removes the other 28 bits of each declared entry. Norm retains four arrays/16,384 cells; Softmax three arrays/160 cells; GPT-vector 24 banks/98,304 cells. Add and ReLU have no bulk arrays. Every subtracted cell is listed in each family’s `storage.tsv`.

| Native memory module attribution | Storage area | Access logic retained in main comparison |
| --- | ---: | ---: |
| GCN 16 feature + 2 weight banks | 68,743.987200 | 66,366.722400 |
| GPT-vector source + other + result stores | 91,658.649600 | 89,013.564000 |

GCN’s 106,475.729029 storage-excluded area includes a 33,521.3784 primitive bank, 66,366.7224 bank-access logic, approximately 5,610.2064 remaining native logic/graph-index access and 977.4216 top arbitration/control. GPT-vector’s 125,579.051318 includes a 33,719.6688 primitive bank, 89,013.564 bank-access logic, 2,836.1832 other native logic and 9.6348 top control. The tiny differences between these sums and reported totals are printed-attribute rounding. Norm and Softmax have flat native arrays, so their access logic is not claimed as a separately measured hierarchy.

Each core installs 16 callable arithmetic lanes, making 96 installed lanes across six independent ASICs versus 16 PEs in one CGRA. Native throughput is not aligned: Add/ReLU native streams use lane 0; GPT native kernels remain scalar; GCN schedules its 16 channels through the bank. The primitive interface is arithmetic capability, not CGRA token, predicate or programmable routing semantics. This baseline is not an equal-throughput proof.

All six source/constraint hashes, leaf-area sums, storage-array inventories and timing-constraint checks pass the evidence audit. Reports retain tool warnings about unused family/DesignWare ports, unused truncated arithmetic bits and constant nets connected to multiple pins. No simulation or performance rerun was performed. Historical performance remains evidence for the historical implementations; it does not validate modified scheduling or the timing repair’s extra cycles. No active RTL, VectorCGRA or committed source was changed.
