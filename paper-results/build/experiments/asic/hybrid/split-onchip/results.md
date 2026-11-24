# Matched Hybrid ResNet result

Both backends passed four RTL executions and checked 4096 outputs each (8192 total). Both simulator processes exited normally with code 0. The 72 frozen simulator, ELF, source and compiler-dependency hashes still match after execution.

| Backend | Cold cycles | Warm cycles | Warm mean | Regional submissions |
| --- | ---: | --- | ---: | ---: |
| CGRA fused | 103060 | 42177, 40888, 40830 | 41298.33 | 2 |
| Independent Add/ReLU ASIC | 53975 | 45571, 44138, 44071 | 44593.33 | 2 |

CGRA uses 7.39% fewer warm cycles (1.0798× ASIC/CGRA speedup). ASIC remains faster for the single cold execution. The selected warm metric excludes first-use configuration cost but retains per-inference staging, conversion, copies, computation and final output visibility.

| Warm mean interval | CGRA cycles | ASIC cycles |
| --- | ---: | ---: |
| conv1 | 3642.00 | 3653.00 |
| activation | 13392.67 | 10319.33 |
| branches | 4216.67 | 4220.00 |
| branch_load | 5684.67 | 3899.67 |
| pair | 7723.00 | 13133.00 |
| output | 4813.00 | 7616.67 |

The second region takes 7723 cycles on CGRA and 13133 cycles on ASIC. CGRA fuses AddReLU; ASIC automatically runs Add, copies 4096 bytes directly between its private SPMs, then runs ReLU. Both use one CPU submission for this region, so this measurement demonstrates an advantage with equal CPU launch counts. It does not establish a CPU-launch-count advantage.

End-to-end gain also includes interface/output-transfer differences: CGRA spends more time in the first activation and branch-load intervals, but less in final output transfer. The total improvement must not be attributed entirely to fusion. The inherited raw log fields `add`, `peer` and `relu` do not separately time the automatic ASIC stages: `add` covers the whole second region and `peer`/`relu` are zero placeholders. Use `pair` for the measured combined interval.

Both endpoints retain the original aggregate 128 KiB data capacity; ASIC has two private 64 KiB SPMs. The 1024-output block is unchanged. First-stage ReLU and second-stage AddReLU CGRA programs reside in disjoint control slots, so both can retain static configuration. The ASIC retains its job descriptors in adapters.

This result uses the original split simulator and matching runtime ABI, not the later general fused ASIC. No new silicon timing or area result is implied. Both use the simulator’s nominal 500 MHz clock; physical timing closure remains a separate requirement.

See [configuration and reproduction](README.md), [raw results](results.json), [input hashes](inputs.json), [ABI comparison](abi.json), [CGRA log](cgra/run.log), and [ASIC log](asic/run.log).
