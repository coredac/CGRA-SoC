# Modified compute cores

This experiment selects the local CGRA control simplification and the six-family ASIC arithmetic expansion for a complete compute-core synthesis comparison. It does not update active VectorCGRA or ASIC RTL, commit changes, regenerate simulators, or rerun performance workloads.

Both sides use SAED14 RVT TT 0.8 V 25 C, a 2 ns ideal clock, 0.1 ns input/output delay, 0.1 ns input transition, output load 1, maximum transition 0.2 ns, maximum fanout 16, enclosed wire-load mode, and `compile -map_effort medium -area_effort high`. No reset exceptions, false paths or multicycle exceptions are added. Completion of synthesis does not establish timing closure.

The CGRA boundary contains all sixteen PEs, their arithmetic, operand registers, constant and instruction access, routing, the modified control bus and core controller. Only the external `data_mem` instance is removed; each of its signals becomes an explicit directional top-level port. The DMA engine and endpoint adapter are outside this core. Source extraction and checks are in [cgra/source](cgra/source/).

Each ASIC result maps one complete `ExpandedAsic` family with its native arithmetic, control and local arrays, the sixteen-lane primitive bank, command arbitration and runtime state. DMA, external SPM and endpoint adapters are outside this boundary. All six families must finish before reporting their measured sum. A standalone primitive-bank result multiplied by six is not that sum.

The original expanded GPT-vector result fails the 2 ns setup target by 0.234015 ns on native bias-add and square-multiply paths. Its local repair under [gpt-fix/source](gpt-fix/source/) adds 97 logical pipeline bits and three cycles per native operation-zero element. Bias addition and partial-product recombination pass combinational SAT checks; a modular multiplication identity and bounded arithmetic checks cover the decomposition. Whole-FSM equivalence and simulation are not claimed. The selected comparison uses [gpt-fix/mapped/results.json](gpt-fix/mapped/results.json) only after successful mapping and reported constraint closure, while retaining the original family result in `asic/results.json`. The repaired whole GPT result now passes with +0.002648 ns worst setup slack and no reported violations: 217,356.737307 total units, 125,698.087707 after storage subtraction, and a 119.036389-unit increase over its predecessor. Actual mapped sequential cells increase by 72. The selected six-ASIC totals are 563,080.753566 including local storage and 385,796.171166 after storage subtraction, with all six meeting the synthesis constraints.

Two area columns distinguish complete mapped cells from the same cells with named bulk local/configuration storage bits subtracted. Subtraction requires an exact storage path and a sequential-cell attribute. Read/write selection, counters, operand registers, queues and pipeline registers remain charged. This is compute plus control and local access, not an arithmetic-gates-only area. External SPM storage and its access circuitry are excluded on both sides.

Six independent expanded ASIC cores install ninety-six primitive lanes in total; the shared CGRA has sixteen PEs. Several ASIC native schedules remain scalar, and the modified CGRA bus and one-entry queues reduce transport concurrency. These are modified-design area measurements, not proof of equal throughput or unchanged performance. Existing performance results remain archived against their original hardware.

Local scripts, frozen source hashes, remote job paths, mapped reports and cell classifications are retained beside each result. Only final reports with successful tool exit and mapped-leaf checks count as measurements. Wire-load synthesis is not physical signoff, and no whole-system functional or performance validation is claimed.

Collect the existing jobs and produce the comparison from the repository root; these commands do not launch new synthesis or simulation:

```sh
python build/experiments/asic/hybrid/revised-area/cgra/collect.py
python build/experiments/asic/hybrid/revised-area/asic/collect.py
python build/experiments/asic/hybrid/revised-area/compare.py
```

## Completed comparison

The [final comparison](results.md) uses the selected repaired GPT core and all sixteen CGRA PEs. CGRA occupies 544,538.068098 units including local storage, or 413,303.567298 after named static storage subtraction. The six independent ASICs total 563,080.753566 and 385,796.171166 respectively. CGRA is 3.2931% smaller with local storage included and 7.1300% larger after its subtraction.

CGRA does not meet 2 ns: raw worst slack is −0.962234 ns on the forwarded SPM clock, which the generic output constraint treats as data. A genuine internal PE path still fails by 0.759389 ns, traversing instruction selection, routing, operand selection, multiplication and FU output selection. The raw QoR reports 15,156 violating setup paths and 114 fanout violations. All six selected ASICs pass the reported synthesis constraints. No corrected-constraint STA or additional mapping is claimed. This comparison cannot establish an equal-frequency performance or area-efficiency win.
