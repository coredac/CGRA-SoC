# CGRA data SPM capacity alignment

This audit concerns CGRA data SPM only. GEMM storage and adapter configuration buffers are separate. All three policies at a fixed workload and CGRA count use the same data-SPM capacity.

| Source | 1 CGRA | 2 CGRA | 4 CGRA |
| --- | ---: | ---: | ---: |
| GPT area inputs | 1 × 128 KiB | 2 × 16 KiB | 4 × 8 KiB |
| GPT performance RTL | 1 × 128 KiB | 2 × 32 KiB | 4 × 32 KiB |
| ResNet data SPM, both sources | 1 × 64 KiB | 2 × 32 KiB | 4 × 32 KiB |
| GCN data SPM, both sources | 1 × 64 KiB | 2 × 64 KiB | 4 × 32 KiB |

The GPT area configurations were trimmed to power-of-two capacities covering their recorded peak addresses. Their manifests show per-core peak usage of 16896/2608/1568 words and selected capacity of 32768/4096/2048 words for 1/2/4 CGRAs. The trimmed simulations were deferred. Archived GPT 2/4-CGRA performance manifests instead specify 8192 32-bit words per core. Therefore six GPT policy points pair cycles with a smaller data SPM in the area model.

Restoring the macro inventory to the original performance capacities adds 0.674503 million library area units to each GPT 2-CGRA setting and 2.023509 million to each 4-CGRA setting. The data memory uses the SRAM2RW128x32 macro family. This preview leaves cycles, data-memory peripheral logic, adapters and other costs unchanged.

| CGRA count | per-task AE | per-segment AE | per-graph AE |
| --- | ---: | ---: | ---: |
| 1 | 1.000 | 0.928 | 0.890 |
| 2 | 1.470 | 1.560 | 1.520 |
| 4 | 1.067 | 1.758 | 1.876 |

These are SRAM-macro-only adjustments, not new synthesis results. A full correction must account for address decoding, muxing and interface-width changes, using equivalent archived components or local synthesis. Matching original capacities also does not impose equal aggregate capacity across compositions: GPT retains 128/64/128 KiB total. Other simulation/synthesis mismatches and timing closure are outside this adjustment.

Values, formulas, input hashes and checks are in [preview.json](preview.json), with tabular data in [preview.csv](preview.csv). Accepted figures and original measurements were not modified.

The capacity-restoration preview is an alternative accounting model, not a mandatory correction if workload-specific trimming remains the intended evaluation. A further source check found a layout asymmetry: the original single-CGRA implementation places affine parameters at word 16384, forcing a 16896-word address extent and a 128 KiB allocation. The multicore implementation computes a compact layout instead. Applying its layout equations to one lane predicts 4688 words (18.3125 KiB), rounded to 32 KiB; the existing two/four-lane equations yield 16/8 KiB per core, also 32 KiB aggregate. This does not establish that an implemented single-CGRA design already uses 32 KiB. [layout.json](layout.json) records the projection and required address-relocation validation. If adopted, the new baseline area must propagate to every GPT AE normalization.
