# GPT-vector timing repair mapping

The complete repaired `ExpandedAsic` FAMILY=5 core was mapped at the byte-identical constraints and synthesis settings of the [original six-family baseline](../../asic/README.md). [job.json](job.json) binds the source and constraint hashes; [results.json](results.json) is the single audited GPT-vector row for explicit selection in the parent comparison.

| Metric | Before | After |
| --- | ---: | ---: |
| Full cell area | 217,237.700918 | 217,356.737307 |
| Local-array storage area | 91,658.649600 | 91,658.649600 |
| Storage-excluded area | 125,579.051318 | 125,698.087707 |
| Worst setup slack at 2 ns | −0.234015 ns | +0.002648 ns |
| Violating setup endpoints | 4 | 0 |
| Mapped sequential cells | 104,219 | 104,291 |

Area increases by 119.036389 SAED14 library area units. All 24 banks and 98,304 storage cells remain; their decoder/read/write access logic remains counted. The repaired core includes 88,972.0056 area of memory-module access, 33,758.3412 of primitive-bank logic, 2,958.1056 of other native datapath/control/access, and 9.6348 of top arbitration/control. Printed leaf attributes differ from report totals by 0.000507 area.

The [source change](../source/manifest.json) adds three native operation-zero cycles per element: two for the SQUARE partial-product pipeline and one for the split BIAS carry. It introduces 97 logical pipeline bits before optimization; the net mapped sequential count increases by 72. Those pipeline registers are retained in the storage-excluded area. Other native operation paths, native arrays and primitive-bank RTL remain unchanged.

The [final constraints report](gpt_vector/constraints.rpt) has no violated constraints. The [timing report](gpt_vector/setup.rpt) now has the worst path from native `value_reg[1]` to `value_reg[31]`, through the native value-plus-second addition. The repaired BIAS carry path has +0.007090 ns slack. Source/constraint hash checks, full leaf-area reconciliation, all 24 storage-array inventories and `check_timing` pass. Ideal-clock wire-load synthesis is not physical timing signoff; the remaining setup margin is small.

Replacing the original GPT-vector row yields six-core totals of 563,080.753566 full cell area, 177,284.582400 local-array storage and 385,796.171166 storage-excluded area. All six selected cores meet the 2 ns synthesis constraints. This is six independent 16-lane expanded ASICs, not an equal-total-lane or equal-throughput comparison with one 16-PE CGRA.

The original reports remain intact. No simulator or performance run was performed, and the historical performance numbers do not validate the added native latency. No active RTL, VectorCGRA source or commit was changed by this mapping task.
