# Existing six families plus an autonomous SAD endpoint

Both sides exclude adapters, DMA and external data SPM. Area is in SAED14 library units; the selected main column includes local/configuration storage.

| Provisioning | Including local storage | Excluding named storage |
| --- | ---: | ---: |
| Modified CGRA, unchanged | 544,538.07 | 413,303.57 |
| Original selected six ASICs | 563,080.75 | 385,796.17 |
| Additional independent SAD | 35,790.17 | 35,790.17 |
| Seven ASICs combined | 598,870.93 | 421,586.34 |

CGRA area saving changes from 3.29% to 9.07% when the optional SAD endpoint is installed. SAD worst setup slack is +0.058382 ns at 2 ns; all reported constraints met: True. The CGRA still has -0.759389 ns internal setup slack.

- Seventh ASIC is optional independent autonomous SAD provisioning; existing primitive interfaces already contain the arithmetic needed for software sequencing.
- Seven expanded endpoints install 112 primitive lanes versus 16 CGRA PEs; equal useful throughput is not established.
- CGRA area reuses the unchanged modified core; SAD mapping on that modified control network is not newly validated.
- No simulator or performance run; synthesis and local static/formal checks are not whole-system validation.
- CGRA still fails 2 ns. This is an area scenario, not timing-qualified performance per area.

| ASIC core | Including local storage | Worst setup slack, ns |
| --- | ---: | ---: |
| add | 34,553.86 | +0.104329 |
| relu | 34,550.53 | +0.104401 |
| gcn | 176,676.04 | +0.000325 |
| norm | 65,169.30 | +0.104401 |
| softmax | 34,774.30 | +0.037377 |
| gpt_vector | 217,356.74 | +0.002648 |
| sad | 35,790.17 | +0.058382 |

See [candidate audit](candidates.md), [SAD source and checks](sad/source/README.md), [mapped SAD detail](sad/results.json), and [unchanged six-family baseline](../revised-area/results.md).
