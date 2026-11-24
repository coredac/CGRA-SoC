# GPT execution matrix

This matrix runs the complete existing small GPT block with N=2, hidden width 128, 2 heads and FF width 512. It uses the archived validated ASIC0 SoC simulator and the already validated dual-lane CGRA schedules. No active RTL, shared header or simulator is regenerated. The heterogeneous DIV and mixed-width SRAM candidates are separate preflights and are not included in this SoC binary.

Each backend and mode receives a fresh simulator process. Its sample 0 is cold, followed by three warm samples with fixture input IDs 0, 1, 0, 1. All 256 final INT8 outputs are checked against the corresponding stored reference after every sample. These are two alternating inputs, not three distinct new fixtures or independent statistical trials.

Both modes submit through AutoLink. In `operator`, the CPU submits each nonmatrix phase separately. LN and Softmax intermediates already remain in local SPM in this mode; they do not incur a host roundtrip between every phase. In `chain`, cached local chains reduce submissions from 51 to 18. FC1→GELU additionally avoids a 1024-word output transfer and reload: 8192 fewer host-transfer bytes. Both backends receive this benefit. Measured ASIC core starts remain 51 in both modes: 18 counts submissions, not native arithmetic launches. The complete GPT block remains CPU-orchestrated with other DRAM staging; `chain` is not full graphAuto.

The CPU `rdcycle` interval includes initial configuration when cold, input packing, DMA, all Gemmini and nonmatrix kernels, and final output staging. The reference check and printed output are outside the interval. `setup_cycles` ends after configuration; `pipeline_cycles` covers the remainder. Cold samples configure 20 jobs; warm samples must configure zero. We retain every raw sample and report mean, median and range for the three warm totals.

ASIC active cycles, starts, memory bytes, CPU starts and configuration commands are hardware counter deltas. Job configuration count, AutoLink submissions, Gemmini GEMMs and backend DMA bytes are instrumented software accounting. No new CGRA hardware counter is inferred from those totals.

The only source changes from validated `preflight/system/lanes.c` separate sample ID from fixture input ID, configure and flush only on sample 0, repeat four times, and print sample IDs plus scope. `source.diff` records these changes. `inputs.json` hashes the source, ELF and all 52 recorded dependencies/artifacts; each run manifest hashes the archived simulator and ELF and records its command, environment, exit and elapsed time.

The results are functional RTL cycle measurements. They do not establish physical clock closure, silicon runtime, power or complete hardware cost. No performance outcome is required for a run to be reported.

The CGRA mapping uses validated power-of-two strength reduction and two-lane requantization/residual kernels. The ASIC vector baseline retains its existing generic divisor implementation; a dedicated power-of-two fast path could also improve that baseline. This comparison measures the tested implementations and does not establish that the observed arithmetic optimization is exclusive to CGRA.
