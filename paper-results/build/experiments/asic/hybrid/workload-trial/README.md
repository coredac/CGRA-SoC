# Additional autonomous workload endpoint

This local experiment adds SAD to the previously measured six independent expanded ASIC families. It reuses the frozen modified CGRA area and the six ASIC measurements, including the selected GPT timing repair. The main area column includes local/configuration storage on both sides; adapters, DMA and external data SPM remain excluded.

The [candidate audit](candidates.md) selects SAD and explains why FIR4x4, AXPY, Histogram, GEMV and Pool are not added as clean new CGRA-supported workloads. Existing YAML names are insufficient evidence of distinct, correct functions. No kernel configuration, active VectorCGRA source, ASIC source or simulator is changed by this experiment.

SAD is a seventh autonomous endpoint with the same public sixteen-lane primitive interface as the six expanded ASICs. Its native controller shares that bank for wrapped subtraction, negation, reduction and accumulation. It is an independently provisioned workload engine, not mandatory additional hardware: existing expanded ASIC primitive interfaces already expose the arithmetic needed for software sequencing. The combined collection is not a minimum-area shared ASIC design or an equal-throughput comparison. Seven banks install 112 primitive lanes versus sixteen CGRA PEs.

The SAD mapping uses the previous SAED14 RVT TT 0.8 V/25 C flow: 2 ns ideal clock, 0.1 ns I/O delay and input transition, load 1, maximum transition 0.2 ns, maximum fanout 16 and enclosed wire-load estimates. Only the top-level name changes in the synthesis script. No timing exceptions are added. Source, constraints and report hashes bind each measurement to its inputs.

The new endpoint has no bulk local data array. All captured operands, accumulator state, pipeline registers and control are charged in both area columns. Synthesis and static/formal checks do not establish whole-system functionality or performance. The unchanged CGRA still has a genuine internal setup violation of 0.759389 ns at 2 ns; this experiment does not repair it. No simulator or performance test is run.

Run `python build/experiments/asic/hybrid/workload-trial/launch.py` once after source validation, then `python build/experiments/asic/hybrid/workload-trial/collect.py` to collect and merge the existing job. The launcher refuses a second launch when `job.json` is present. The collector writes `sad/results.json` and the combined `results.json` and `results.md` without overwriting the previous six-family comparison.

## Measured result

The [completed table](results.md) gives SAD 35,790.173373 units with +0.058382 ns worst setup slack and no reported violations at 2 ns. Its primitive bank occupies 33,638.9496 units; native control and operand state occupy 2,151.2244. The seven independent cores total 598,870.926939 units including local storage, compared with unchanged CGRA 544,538.068098. The measured area saving for this optional installed collection is 9.0725%, versus 3.2931% for the previous six. CGRA timing remains unresolved; no performance-per-area conclusion is claimed.
