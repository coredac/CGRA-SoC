# Complete modified-core results

Area units are SAED14 library area units. Both boundaries exclude adapters, DMA and external data SPM, including its access. Local array/configuration access and runtime state remain counted.

| Core | Including local storage | Excluding named storage cells | Worst setup slack (ns), target 2 ns |
| --- | ---: | ---: | ---: |
| Modified CGRA | 544,538.07 | 413,303.57 | -0.962234 |
| add | 34,553.86 | 34,553.86 | +0.104329 |
| relu | 34,550.53 | 34,550.53 | +0.104401 |
| gcn | 176,676.04 | 106,475.73 | +0.000325 |
| norm | 65,169.30 | 49,892.86 | +0.104401 |
| softmax | 34,774.30 | 34,625.12 | +0.037377 |
| gpt_vector | 217,356.74 | 125,698.09 | +0.002648 |
| Six ASICs combined | 563,080.75 | 385,796.17 | Per-core values above |

CGRA / six-ASIC area ratio is 1.071300 after named storage subtraction, and 0.967069 including local storage. This compares the modified designs; the ASIC sum installs 96 primitive lanes versus the CGRA's 16 PEs. Equal useful throughput is not established.

The selected GPT-vector includes the local timing repair; its predecessor had -0.234015 ns setup slack. The native operation-0 schedule gains three cycles per element. Mapped area changes by +119.04 units.

See [scope and validation limits](README.md), [CGRA details](cgra/results.json), [original ASIC details](asic/results.json) and [selected GPT repair](gpt-fix/mapped/results.json). No simulator or performance run was performed.

All six selected ASICs meet the reported 2 ns synthesis constraints. CGRA does not: its raw worst slack includes the forwarded clock output incorrectly treated as a data output; the worst reported genuine data path still has -0.759389 ns slack. Raw QoR reports 15156 violating setup paths and 114 fanout violations. No false-path exception, corrected-constraint STA or new mapping is claimed.

The worst genuine data path runs from PE 1's instruction-address register through instruction selection, the routing crossbar, operand selection, multiplication and the FU crossbar into an operand register. This lies within the PE execution path; DMA, adapters and the external SPM are outside this synthesis boundary.

| CGRA retained component | Area |
| --- | ---: |
| element | 114,210.21 |
| ctrl_mem | 93,878.87 |
| routing_crossbar | 82,770.39 |
| register_cluster | 45,412.19 |
| const_mem | 30,097.92 |
| fu_crossbar | 19,307.92 |
| controller | 9,597.95 |
| tile_channels | 8,516.01 |
| ctrl_ring | 6,347.38 |
| tile_local_control | 2,722.70 |
| bypass_queue | 427.75 |
| core_local_control | 14.30 |
