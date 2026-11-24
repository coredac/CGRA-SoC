# Dedicated-kernel experiment

This is a supplementary CGRA-versus-ASIC experiment. The project's primary P1 performance experiment remains [the completed 27-case matrix](../build/experiments/p1/summary27/README.md): three workloads × 1/2/4 CGRAs × Manual/Hybrid/Auto, with 54 cold/warm records. Selecting Hybrid for this supplementary comparison does not replace or reduce that primary matrix. Its performance coverage is complete; the recorded large runs disable numerical checking and have only one sample per phase, as documented in the P1 report.

This experiment replaces the non-matrix kernels with synthesizable dedicated RTL. Gemmini's matrix and convolution operations remain outside the standalone benches. The first shapes are GCN ring-chords16 with 128 projected features, the existing small projected ResNet block, and GPT N2/H128/two heads/FF512. Inputs and affine parameters are loaded at runtime; the DUT does not contain reference outputs.

The hypothesis is that sharing CGRA resources across operators and workloads can reduce installed hardware cost at similar end-to-end performance. This is not an assumed outcome. Dedicated hardware may also reuse units, fuse phases and retain intermediates on chip. Existing GCN aggregation includes ReLU, and existing ResNet has Add+ReLU; fusion is not an exclusive CGRA capability.

The standalone results below are arithmetic and kernel-level evidence only. The ASIC benches do not instantiate the SoC adapter, configuration cache, AutoLink fabric, TileLink DMA or CPU submission path. A separate system implementation includes these components and has passed complete small-workload comparisons for GCN, ResNet and GPT. Its CPU-cycle results are reported below; integrated area and timing qualification remain separate work.

The fixed independent-operator ASIC fusion/locality experiment and the general five-unit ASIC comparison are distinct baselines. Earlier ResNet warm advantages of 6.70% and 8.97% use split Add/ReLU hardware and on-chip exchange; the current general-ASIC Hybrid table uses fused AddReLU and host intermediates. Completing that table does not complete the independent-operator experiment across all three workloads. [The reconciliation](../build/experiments/asic/hybrid/reconciliation.md) records these scope changes and the complete area breakdown.

The [matched split/on-chip ResNet follow-up](../build/experiments/asic/hybrid/split-onchip/results.json) now gives both backends two cached regional Hybrid submissions, with ASIC Add→peer-SPM transfer→ReLU performed automatically in the second region. For the 1024-output projected block, three warm samples average 41,298.33 CGRA cycles versus 44,593.33 ASIC cycles, a 7.39% reduction. Both processes exit 0 and all 8192 outputs across eight cold/warm executions pass. The simulator and original split interface hashes are verified; the two ASICs retain 64 KiB each, matching 128 KiB CGRA payload. These are simulation cycles under a common nominal clock, not timing-qualified physical latency. They are separate from the general-ASIC performance and area point.

The [independent ASIC implementation](../build/experiments/asic/hybrid/independent/README.md) now instantiates six private endpoints: Add 32 / ReLU 32 / GCN 32 / Norm 8 / Softmax 8 / GPT-vector 16 KiB, totaling 128 KiB. Each has its own native control, AutoLink adapter, DMA and publication window. Descriptor capacities are 4/4/2/8/6/8, totaling 32. Family phases and native operation variants retain their existing hardware sharing. The [compact Add/ReLU RTL probe](../build/experiments/asic/hybrid/private-spm-probe/results.json), [family-core checks](../build/experiments/asic/independent/core/validation.json), and [private SRAM model checks](../build/experiments/asic/hybrid/independent/memory/results.json) validate the new private capacities. Local addresses are repacked for these capacities; arithmetic-unit local arrays remain additional storage and are charged separately. Equal external-SPM capacity does not establish equal total data storage.

A compute-unit area view keeps necessary arithmetic pipeline/state registers on both sides and separates configuration caches, bulk data storage and adapters. Full endpoint cost still includes every required component. The [hierarchy attribution](../build/experiments/asic/hybrid/area-audit/results.json) alone cannot separate flattened GCN/Norm arrays. The subsequent [leaf-cell audit](../build/experiments/asic/hybrid/compute-audit/results.json) identifies named storage flops in the original mapped designs and reports the remaining computation and control. CGRA elaboration parameters change hardware area; runtime kernel configuration on the same instantiated design does not.

### Computation and control excluding bulk storage cells

The leaf-cell audit removes data/configuration SRAM macros and named bulk configuration, constant and data-array flops. It retains arithmetic, pipeline/FSM state, operand register files, operational FIFO registers, prologue counters, validity state, interconnect and all read/write address, decode and mux logic. Core subtotals exclude endpoint adapters and DMA; endpoint subtotals include them. This is an accounting boundary on the existing mapped implementation, not a synthesized memory-free accelerator. Retained access logic still depends on the original array sizes. Both original endpoints fail 2 ns, so these are diagnostic areas rather than timing-qualified PPA.

| Boundary, library area units | CGRA | General five-unit ASIC | CGRA / ASIC |
| --- | ---: | ---: | ---: |
| Core computation and control, bulk storage cells excluded | 1,178,924.43 | 385,209.52 | 3.060 |
| Endpoint computation and control, bulk storage cells excluded | 1,264,551.83 | 402,725.45 | 3.140 |

CGRA's core subtotal contains 194,912.54 units in FU execution/selection, 166,795.66 in routing and FU crossbars, 70,099.87 in operand register clusters, 264,385.39 in PE configuration-memory access/local control, 71,800.04 in constant-memory access, 303,709.63 in the control ring, 69,080.32 in data-SPM access and 38,141.98 elsewhere. The control ring includes 62,169.59 units of operational packet-buffer flops; the remaining 241,540.04 includes queue access, selection, routing, arbitration and control. These queues remain charged as operational control, rather than being removed as bulk configuration caches.

The ASIC core subtotal contains GCN 128,104.97, normalization 31,718.34, archived Softmax 2,593.36, GPT vector 169,213.24, streaming vector 416.87, data-SPM access 52,112.90 and core control 1,049.84. Unit subtotals retain their local-array access circuits; they are not arithmetic-only areas. The two installed resource inventories differ: sixteen reusable CGRA PEs versus five bounded dedicated units, with common Gemmini excluded. The current implementation does not demonstrate smaller CGRA computation-plus-control area.

Configuration storage has two distinct implementations: the CGRA adapter's packet/patch cache uses SRAM macros, while PE instruction storage uses flip-flops. The audit removes 98,410.29 units of PE configuration flops, 37,494.69 of constant flops and 2,585.86 of adapter configuration flops, in addition to SRAM. ASIC removes 197,656.01 units of local data-array flops and 6,279.94 of descriptor flops, in addition to data SRAM. Thus ASIC configuration is included and classified explicitly; it is not assumed absent. The named leaf cells reconcile both complete mapped-area reports with less than 0.003 library units of rounding difference.

The [earlier ASIC inventory audit](../build/experiments/asic/hybrid/asic-inventory/README.md) distinguished the original shared five-family endpoint from the requested independent ASIC collection. Its estimate from copying five more shells was only a sensitivity check. The subsequent six-endpoint implementation uses actual specialization, private memories, descriptor partitions and generated adapters/DMA; its new mapped inventory supersedes that estimate. CGRA architecture stays fixed, and no area advantage is assumed before the measured comparison.

At the user's request, performance reruns were stopped after the corrected Softmax system's first cold sample passed 256 outputs at 276,911 cycles. That incomplete run is not a new performance result. Existing validated warm records remain the performance evidence for their archived hardware; they are not relabelled as measurements of a new independent-ASIC topology or the corrected Softmax implementation.

## Independent ASIC rerun

The archived initial [six-endpoint rerun](../build/experiments/asic/hybrid/independent/results.md) completed actual mapping of Add, ReLU, GCN, Norm, Softmax and GPT-vector endpoints with 128 KiB aggregate private SPM and 32 resident descriptors. Each endpoint includes its own adapter, DMA, publication interface and sequencer. The isolated rebuilt SoC [passes 402 output checks and six AutoLink launches](../build/experiments/asic/hybrid/independent/system/functional.json), including private-memory boundaries and Add-publication-to-ReLU DMA. Hardware counters require exactly one native start and no CPU start for each AutoLink launch. The first failed functional attempt used CPU dispatch and supplied a causal Softmax row with a noncausal expected result; it is preserved under `system/attempt1` and excluded. No workload performance was rerun.

| Area boundary, SAED14 library units | CGRA after control optimization | Six independent ASICs |
| --- | ---: | ---: |
| Complete endpoint subtotal, including all storage | 6,326,272.63 | 1,992,057.29 |
| Endpoint subtotal excluding data SRAM macros only | 3,628,260.88 | 643,051.42 |
| Endpoint computation and control, all named bulk storage bits excluded | 1,264,481.23 | 439,237.12 |
| Core computation and control, also excluding endpoint adapters/DMA | 1,178,924.43 | 374,766.72 |

Independent ASIC endpoint computation/control is 9.07% larger than the previous shared endpoint's 402,725.45, but CGRA remains 2.879 times larger on that boundary. ASIC endpoint shells contribute 64,470.40; specialization and smaller private-memory access structures offset part of the duplicated shell cost. This implementation does not establish a smaller-CGRA area claim. Subtotals exclude common upstream AutoLink routing, RoCC selection, Rocket and Gemmini; they are not whole-SoC areas.

The subtraction removes SRAM cells and identified bulk data/configuration-array flops, while retaining access/decode/mux logic, operating queues, state and pipeline registers. Both have 128 KiB external payload capacity, but the unchanged CGRA SRAM mapping uses 256 KiB physical data SRAM plus 288 KiB configuration SRAM; equal payload does not imply equal macro area. ASIC arithmetic-local arrays are additional to its private SPM budget.

In that archived flow, endpoint controls use 2 ns, 0.1 ns I/O delay, 0.1 ns input transition, 1 fF output load and enclosed library wire loads. Unchanged GCN/Norm/GPT-vector arithmetic checkpoints retain their 10 ns mappings; corrected Softmax retains its 2 ns mapping. Add and ReLU setup/hold meet 2 ns, while GCN/Norm/Softmax/GPT-vector worst setup slack is −7.1451/−7.6056/−0.4323/−13.2421 ns. All six have positive reported hold slack, and minimum-capacitance violations remain. These are verified mapped inventories and timing diagnostics, not timing-closed PPA. Archived performance stays attached to its original hardware.

## ASIC timing closure and control-area audit

The [closure follow-up](../build/experiments/asic/hybrid/closure/state.json) changes native ASIC RTL rather than excluding failing paths. GCN registers its product before accumulation and partitions feature/weight storage into 128-word banks without changing capacity or access cycles. Norm separates array reads, delta, square, accumulation, affine multiplication, division and rounding. GPT-vector separates array reads, GELU arithmetic, division preparation and rounding; its three 1024-word arrays use the same eight-bank implementation in simulation and synthesis. All changes retain the original integer truncation and overflow semantics. These are sequential multicycle datapaths, not one-result-per-cycle pipelines. Their extra execution cycles invalidate reuse of the archived performance as measurements of these new revisions; the original performance records remain unchanged.

Native arithmetic is first optimized within complete endpoints. The final mappings reuse those verified native subtrees with exact leaf-inventory checks, optimize the changed Core and endpoint control at 1.6 ns, and report every endpoint path against the original 2 ns target. A registered rolling load address removes source selection and addition from the SRAM half-cycle read path without adding load cycles. Compile-time capability guards remove unused Vector-only address branches from fixed GCN, Norm, Softmax and GPT-vector endpoints. Generic and fixed-family SRAM-model tests confirm unchanged legal execution cycles for these integration changes. No reset false paths or functional multicycle exceptions are introduced.

GCN now meets the actual 2 ns target with +0.028916 ns setup slack and zero setup violations. Its Core registers busy on the existing state transitions and uses that registered signal for the read-address selector. Idle host reads, native loads and Vector reads retain their active addresses; only disabled-read addresses change. A completed Ultra-mapped native GCN subtree is reused with an exact leaf-inventory check, while the changed Core and endpoint control are freshly optimized. Full-endpoint timing includes every native, control and SRAM path. All six independent ASICs meet 2 ns setup with zero violations. Add/ReLU reuse unchanged implementations; Norm, Softmax and GPT-vector are remapped for the same updated Core, with setup margins of +0.087255, +0.202466 and +0.061328 ns respectively. Electrical constraints and physical signoff remain separate from setup acceptance.

The updated Core passes 32,973 functional cases and 59,272 output checks, including cycle-by-cycle busy checks and reset during execution. All output records and execution cycles match the previous Core. The final independent-endpoint SoC passes 402 checked values, six AutoLink launches, peer DMA and every private-SPM boundary check with the updated Core. All build and validation stages exit zero. The [functional evidence](../build/experiments/asic/hybrid/closure/system-next/validation.json) records exact prepared, generated and compiled RTL identities. Workload performance experiments were not rerun.

The [control-area audit](../build/experiments/asic/hybrid/closure/control-audit.md) identifies both large CGRA components as native VectorCGRA logic. The control ring has seventeen three-port routers, two virtual channels, 198-bit messages and 136 depth-two queues, including input and egress queues. Its 303,709.63 area includes 62,169.59 of operational message-buffer bits. Buffers and inverters occupy 152,962.71, or 50.36%; this is observed mapping overhead, not proof that all of it is architecturally necessary or caused by the wire-load model.

Each PE reads and writes a 48-by-111-bit control store. Across sixteen PEs, 85,248 configuration bits were already excluded, while their read/write muxes and decoders remain. The audit finds another 23,040 bits in crossbar prologue-initial-value tables that are written only during configuration and had incorrectly been counted as runtime counters. Excluding these 26,597.376 area units changes PE access/control from 264,385.39 to 237,788.02. Actual execution counters and operational queues remain counted. The ring and corrected PE access/control together account for 43.74% of the retained endpoint computation/control area. Corrected CGRA endpoint computation/control is 1,237,883.85 and core computation/control is 1,152,327.05; full physical area does not change. The six timing-qualified ASIC endpoints total 399,175.45 for endpoint computation/control and 334,722.23 for core computation/control; the corresponding CGRA/ASIC ratios are 3.1011 and 3.4426. These mapped implementations do not show smaller CGRA computation/control area. The audit preserves the original reports and records exact leaf patterns and source hashes.

The selected independent ASIC inventory has the following area boundaries. Values are SAED14 library area units. All ASIC endpoints pass synthesis setup at 2 ns; CGRA still fails that target, so this is not a matched-frequency physical PPA result. ASIC physical data SRAM totals 128 KiB, while CGRA contains 256 KiB physical data SRAM and another 288 KiB configuration SRAM; equal 128 KiB external payload capacity does not make physical memory capacity equal.

| Area boundary | Six independent ASICs | CGRA | CGRA / ASIC |
| --- | ---: | ---: | ---: |
| Complete endpoints | 1,944,937.70 | 6,326,272.63 | 3.2527 |
| Excluding data SRAM macros | 595,931.83 | 3,628,260.88 | 6.0884 |
| Computation/control, excluding named bulk data/configuration storage | 399,175.45 | 1,237,883.85 | 3.1011 |
| Core computation/control only | 334,722.23 | 1,152,327.05 | 3.4426 |

The storage exclusions retain access muxes, decoders, operational queues, operand registers, pipeline registers and controller state. The core-only boundary additionally excludes endpoint host/DMA/adapter logic. Those shells contribute 85,556.80 for CGRA and 64,453.22 for the six ASICs; their 21,103.59 difference is only 2.52% of the endpoint computation/control gap. Most of this measured gap is inside the cores, including the native VectorCGRA control structures. Remaining min-capacitance and max-fanout violations are reported separately; no physical signoff is claimed. The current SoC functional proof is complete in `system-next`. The [final report](../build/experiments/asic/hybrid/closure/results.md) and machine-readable results bind the area and timing inventory to current source and functional evidence.

### Remaining area gap and baseline requirements

The [area-gap audit](../build/experiments/asic/hybrid/closure/area-gap.json) regroups the existing mapped leaves and verifies every endpoint sum. It performs no synthesis, simulation or performance rerun. The computation/control gap is 838,708.41 area units: ASIC would need 210.11% additional area to reach the current CGRA total, assuming CGRA remains unchanged.

| CGRA computation/control group | Area |
| --- | ---: |
| Control ring | 303,709.63 |
| PE configuration access and control | 237,788.02 |
| Functional units and selection | 194,912.54 |
| Routing and FU crossbars | 166,795.66 |
| Operand registers and constant-store access | 141,899.91 |
| Data-SPM access | 69,080.32 |
| Endpoint adapters, DMA and shell | 85,556.80 |
| Remaining controllers, channels and queues | 38,140.98 |

The six ASIC endpoints already include separate SPMs, DMA and adapters. GPT-vector alone retains 192,374.23 area units of access circuitry for its source, other and result arrays after storage-flop exclusion, accounting for 48.19% of all ASIC computation/control area. ASIC native-module area therefore cannot be treated as arithmetic-only area when comparing it with CGRA FU subtrees.

The main capability difference is parallelism: the installed CGRA has sixteen scalar PEs, whereas several ASIC arithmetic paths execute sequentially over multiple cycles. Passing the same 2 ns setup target does not establish equal throughput or initiation interval. Additional ASIC lanes, pipeline registers or memory ports require a stated throughput requirement and a matching resource budget; neither a sixteen-fold multiplier nor the area ratio establishes that budget. Archived performance belongs to earlier ASIC revisions and cannot establish throughput for the current mapped implementations.

Independence is currently per accelerator family. GPT-vector shares hardware across GELU, requantization and residual addition. A per-operator or concurrent-service baseline would require specialized independent implementations if that is the intended requirement. As a coarse sensitivity calculation, even adding two complete copies of the current multifunction GPT-vector endpoint raises ASIC computation/control only to 830,386.92, still below CGRA. This duplication is not itself a valid baseline: specialization and any permitted reuse must be accounted for.

Broader tensor-shape support or upstream system interconnect may add cost only where required by the comparison contract, with corresponding CGRA costs included. Equal physical SPM capacity changes the storage-inclusive comparison; it does not by itself close the computation/control gap. Existing evidence supports no claim that these changes would make CGRA smaller.

### Core area excluding data-SPM access

The requested [additional subtraction](../build/experiments/asic/hybrid/ring-trial/area.json) removes the remaining data-SPM module access and pipeline cells from both core computation/control totals. CGRA loses 69,080.32 units and the six ASICs lose 43,631.70. The remaining areas are 1,083,246.74 and 291,090.53 respectively, a 3.7213 ratio. Both sides already exclude endpoint DMA/adapters and identified bulk storage cells. This boundary still includes PE instruction access, constant access, operand registers and native arithmetic-local-array access; it is not an arithmetic-gates-only result. ASIC also has banked SPM access logic, so this is a symmetric scope change rather than removal of a CGRA-only feature.

An isolated [control-ring trial](../build/experiments/asic/hybrid/ring-trial/results.json) replaces each switch's encoded wide packet mux with a one-hot selector. The candidate retains the round-robin arbiter, two virtual channels, queue capacity, packet format and register boundaries, including input-zero selection when no request is granted. The active VectorCGRA source is unchanged; the proposed patch and generated artifacts live only in `build/experiments/asic/hybrid/ring-trial`, with no commit. Discard that directory to remove the trial; do not reset existing source changes.

Both switches translate with the real 198-bit packet type. The selector passes 896 per-bit algebra cases, and a reset-constrained bounded miter proves translated output equivalence at the next two steps for arbitrary input packets, requests and downstream readiness. All 51 candidate switches elaborate and translate in the seventeen-node ring; Verilator lint reports no errors or combinational-loop warnings, retaining generated loop-index width warnings. Under identical Yosys generic mapping, switch cell count excluding scope metadata falls from 8,253 to 2,223, with six registers in both. This 73.06% generic-cell reduction is not a SAED14 area reduction or a full-ring timing result. No simulator, workload or SoC integration validation was run, as requested; existing performance and mapped-area results do not describe this trial.

A subsequent [control review](../build/experiments/asic/hybrid/control-review/README.md) maps the original and candidate switches separately with the same SAED14 libraries and 2 ns constraints. Area is 383.083196 versus 387.833996: the one-hot candidate is 1.24% larger, despite the generic-cell reduction. Both isolated switches meet constraints. This is not a full-ring remap, and the candidate remains unapplied. Switches account for only 10.38% of the archived ring area; instruction-store access, queues, routing and buffering remain the larger optimization targets. The review explains which structures are still required during warm execution and which configuration storage has already been excluded.

### Isolated control simplification

The [control simplification trial](../build/experiments/asic/hybrid/control-trial/README.md) replaces the seventeen-router control ring with a round-robin addressed bus and seventeen registered mailboxes, and reduces each PE's incoming command and FU-return queues from two entries to one. It preserves all packet ports, legal destinations, runtime commands and the 48-by-111-bit instruction store. The selected PE variant retains the original instruction access logic; a decoded-read alternative produced no useful area saving. These changes reduce buffering and transport concurrency, so old workload performance cannot be assigned to the trial.

The selected standalone PE control maps to 12,171.949073 area units versus 12,815.260648 originally. Both contain 6,768 static configuration cells occupying 6,310.4832 units. Excluding those cells gives 5,861.465873 versus 6,504.777448, a 9.8898% reduction. Both meet 2 ns with +1.308921 ns worst setup slack and no reported constraint violations. The same-flow original ring maps to 138,178.748464 units with +0.367287 ns slack; the candidate bus maps to 6,314.035230 units with +0.672539 ns slack, a 95.4305% area reduction. Neither final ring constraint report lists a violation. Summing one network and sixteen standalone PE controls gives 242,255.19 versus 100,097.49 units with static configuration storage excluded, a 58.68% reduction. This is a replicated-component subtotal, not a full-CGRA area or timing result; the archived in-context ring area of 303,709.63 is a different mapping boundary.

The isolated assembled full CGRA RTL passes lint with the same nine preexisting datapath circular-combinational warnings and no new warning signals in that set. Local read-equivalence and candidate packet-safety checks pass; no simulator or performance test is run. The bus admits at most one packet per cycle, and each non-bypass mailbox or PE queue can accept continuous same-destination traffic only every other cycle. Active VectorCGRA source hashes, its Git revision and the preexisting tracked diff remain unchanged. All generators, candidates and reports remain uncommitted under the build experiment directory.

### Parallelism and memory-port matching

The frozen mapped CGRA has sixteen scalar PEs and seven memory-connected PEs, not four: tile IDs 0, 4, 8 and 12 on the west edge, plus 1, 2 and 3 on the south edge. The corner is counted once. Its generated data-memory controller declares seven tile read-request and seven tile write-request interfaces across eight banks; controller/NoC and DMA interfaces are additional. These interface counts do not guarantee seven useful accesses every cycle, or simultaneous full-rate reads and writes from each PE: instruction scheduling, dependencies, bank conflicts and backpressure still apply.

Each current ASIC endpoint's external data-SPM interface has one read address/data path and one write address/data path. Internal arithmetic-local arrays provide separate operand access and must also be included in a bandwidth comparison. Six independent endpoints can have concurrent activity subject to their task dependencies; their ports cannot be treated as six interchangeable lanes for one fixed-family kernel. Add currently fetches its two inputs on alternating reads, whereas ReLU uses one input per result.

An equal-resource sensitivity should specify operation types and widths, arithmetic initiation intervals, accessible memory bandwidth and capacity, and concurrent-task assumptions. Sixteen PE instances do not imply sixteen ASIC copies per family, sixteen one-cycle divisions, or sixteen arithmetic operations plus seven memory operations in the same cycle. An equal-throughput comparison instead sizes each backend to a common useful-result rate for the same kernel or graph; the present archived end-to-end cycle data do not establish such a rate for the revised ASIC hardware. Added ASIC lanes, registers and ports must follow that requirement rather than a target area. The storage-excluded core-area view remains a breakdown; full hardware cost must still charge any extra memory ports and access logic needed to sustain the specified rate.

### Sixteen-lane GCN trial

The temporary [GCN trial](../build/experiments/asic/hybrid/parallel-trial/README.md) widens aggregation from one arithmetic lane to sixteen while sharing graph indices, weights and control. Structural synthesis finds sixteen multipliers, unchanged 82,464-bit array capacity, and 2,302 non-array register bits versus 373 in the original. Multiply and accumulate remain separate phases; the scalar load and result interfaces remain unchanged. Feature operands are captured in groups of seven, seven and two from sixteen striped asynchronous banks. This is an arithmetic-parallelism trial, not a physically matched seven-port memory subsystem or an equal-throughput result.

The candidate is isolated under `build/experiments/asic/hybrid/parallel-trial`, with no active RTL change or commit. Lint, translation, structural synthesis and source/address review pass. No simulator or performance rerun was performed; functional equivalence remains unverified, and historical performance cannot be assigned to the candidate. Only GCN is widened; the other five dedicated units remain unchanged. Source hashes, resource counts and validation limits are recorded in [the structural audit](../build/experiments/asic/hybrid/parallel-trial/resources-summary.json).

The [mapped trial results](../build/experiments/asic/hybrid/parallel-trial/results.json) give 139,671.35 versus 153,036.14 library area units including local arrays, an increase of 9.57%. Removing identified array storage cells gives 69,465.35 versus 82,830.15, an increase of 19.24%; these totals retain local access logic and exclude external SPM, adapters and DMA. Array storage area remains 70,205.99 in both. Multiplier area grows from 650.02 to 10,341.29. Both final constraint reports have no violations at 2 ns, with worst setup slack +0.075145 ns and +0.000161 ns respectively. The candidate margin is only 0.161 ps, and its critical path is shared-weight register to product register through a multiplier. This is ideal-clock synthesis, not physical signoff or verified throughput. It does not replace the integrated six-ASIC area sum.

### Flexibility coverage

A static inspection of the frozen mapped CGRA RTL and current ASIC sources distinguishes installed arithmetic from software-accessible capability. The frozen CGRA contains sixteen scalar PEs with selectable integer addition/subtraction, multiplication, unsigned division/remainder/integer square root, logic, shifts, comparisons, selection and address-generation units, plus dataflow control and routing. This inventory does not establish successful mapping of every possible combination or tensor shape. Floating-point units present in other repository configurations are not part of this measured design.

| Capability | Current independent ASIC implementation | Candidate extension and scope |
| --- | --- | --- |
| Division | Norm, Softmax and GPT-vector each instantiate `GptDivider`; operands and supported signedness are wired by the caller | Expose general operand selection and full-width results where a public primitive interface is required; reuse existing dividers where scheduling permits |
| Integer square root and remainder | Norm already implements integer square root internally; `GptDivider` exposes a quotient but no remainder output | Expose existing square-root machinery and add a remainder result path if required; account for interfaces and scheduling rather than charging duplicate complete units |
| Elementwise primitives | Add/ReLU are dedicated; GPT-vector exposes fixed GELU, requantization and residual-add operations with INT8 clamping | A shared primitive engine could expose subtraction, multiplication, logic, variable shifts, comparison, selection and full-width results; this is an expanded baseline, not evidence of omitted original cost |
| Runtime dimensions | Norm is fixed at H128; Softmax processes two causal columns; GCN exposes runtime node count but uses compile-time channel count and bounded arrays | Parameterize supported lengths, strides and masks within an explicit capacity budget; arbitrary supported shapes require independent validation |
| Operator composition | Existing adapters support cached descriptors and AutoLink chaining; family arithmetic follows fixed state machines | Programmable local operation sequences and intermediates add flexibility beyond existing job chaining, together with their control and storage costs |

For broader workload coverage, prioritize runtime shape support and reusable full-width primitive interfaces before adding programmable local sequencing. Do not replicate every primitive in every endpoint unless independent concurrent service requires it. A finite expanded operator bank can remain dedicated hardware; a programmable sequencer or instruction engine should be identified as a different architectural baseline. Both must implement reachable runtime functionality rather than unused logic that synthesis can remove.

The existing CGRA division/remainder/square-root subtrees together occupy 11,829.05 library area units across sixteen PEs. This is measured CGRA subtree area, not a prediction of ASIC extension cost or equivalent throughput. It shows that this arithmetic block alone is small relative to the current 817,604.82 core computation/control gap. Additional functionality has no measured area increment yet, and no area crossover is established. This audit changes no RTL and runs no synthesis, simulator or performance experiment.

### Temporary six-family arithmetic expansion

The isolated [six-family trial](../build/experiments/asic/hybrid/capability-trial/README.md) adds a reachable sixteen-lane primitive interface to each of Add, ReLU, GCN, Norm, Softmax and GPT-vector. It covers full-width addition/subtraction, multiplication, unsigned division/remainder/integer square root, bitwise and logical operations, shifts, signed comparisons, selection, ReLU, pass/increment and two-dimensional address arithmetic. Native and primitive commands are mutually exclusive. Add/ReLU use lane zero for their native stream; GCN uses the shared bank for sixteen-channel multiply and accumulate phases; Norm/Softmax/GPT-vector share lane zero's divider with their native computation. Specialized native logic remains where its width or function differs.

This is a new arithmetic-capability baseline, not a drop-in implementation with the old performance. Scalar compatibility interfaces and several native schedules remain sequential; the primitive interface does not make every native kernel sixteen-wide. Six independent expanded endpoints contain ninety-six primitive lanes in total, versus sixteen PEs in one shared CGRA. The experiment therefore does not establish equal total resources, equal useful throughput or full CGRA dataflow/predicate programmability. Its local checks and bank-only synthesis cannot establish the area or timing of all six integrated endpoints. Active ASIC/CGRA sources and frozen performance records remain unchanged; there is no commit or simulator rerun.

The standalone sixteen-lane bank maps to 33,673.447781 SAED14 library area units. Its worst setup slack is +0.013344 ns at 2 ns, and the final constraint report lists no violations. This is ideal-clock synthesis of the bank only, not timing closure or measured total area of the six expanded endpoints. All six variants pass lint and structural synthesis; an inductive proof covers the shared native divider's busy/done/result behavior. Whole-endpoint functionality, new square-root/remainder operations and performance remain unverified.

### Revised complete compute cores

The [selected revised comparison](../build/experiments/asic/hybrid/revised-area/README.md) maps the complete modified sixteen-PE CGRA and all six expanded ASIC compute cores with the same SAED14 library and 2 ns constraints. Both exclude endpoint adapters, DMA and external data SPM, including its access. Local arrays remain in synthesis; only named sequential bulk storage cells are subtracted afterward. Array access, operand registers, runtime counters, queues, routing and computation remain charged. These local experiments do not update active VectorCGRA or ASIC sources, commit changes, or rerun simulator/performance workloads.

The [original expanded ASIC measurements](../build/experiments/asic/hybrid/revised-area/asic/results.json) total 562,961.717177 library area units including local storage and 385,677.134777 excluding its storage cells. Add, ReLU, GCN, Norm and Softmax meet the reported 2 ns constraints; GCN has only +0.000325 ns setup margin. GPT-vector has four violating endpoints and −0.234015 ns worst slack in native square-multiply and bias-add logic. Its [local pipeline repair](../build/experiments/asic/hybrid/revised-area/gpt-fix/source/manifest.json) adds 97 logical register bits and three cycles per native operation-zero element. It passes lint, structural checks and bounded arithmetic checks, including combinational SAT proofs for bias addition and partial-product recombination. Whole-FSM equivalence and new performance are not established. The repaired whole GPT core now maps to 217,356.737307 units, or 125,698.087707 after the unchanged storage subtraction, with +0.002648 ns worst setup slack and no reported violations. Its mapped sequential-cell count increases by 72 and area by 119.036389 units. Selecting that repaired row gives six-ASIC totals of 563,080.753566 including local storage and 385,796.171166 excluding storage cells; all six meet the synthesis constraints. The completed [modified-core comparison](../build/experiments/asic/hybrid/revised-area/results.md) gives CGRA 544,538.068098 units including local storage and 413,303.567298 after subtracting 131,234.5008 units of named static storage cells. CGRA is 3.2931% smaller including local storage, but 7.1300% larger after storage subtraction, a gap of 27,507.396132 units. This is one sixteen-PE CGRA against six independent expanded ASICs with ninety-six installed primitive lanes; equal useful throughput is not established.

The modified CGRA still fails 2 ns. Raw worst slack is −0.962234 ns on the forwarded SPM clock output, which the generic output-delay rule incorrectly treats as data. The worst reported genuine internal path is −0.759389 ns, from PE 1 instruction-address state through instruction selection, routing, operand selection, multiplication and FU output selection to an operand register. The raw report contains 15,156 violating setup paths and 114 fanout violations; removing the clock-output artifact alone does not close timing. No corrected-constraint STA or remapping is claimed. The retained area is dominated by FU logic (114,210.2088), instruction access/control (93,878.8716), routing crossbars (82,770.3912), operand registers (45,412.1868), constant access (30,097.9164) and FU crossbars (19,307.9172). The modified control bus itself is only 6,347.3796 units. These component areas already exclude the named static storage cells.

### Additional autonomous SAD endpoint

The [additional-workload audit](../build/experiments/asic/hybrid/workload-trial/candidates.md) selects SAD, whose existing eight-pair CGRA fixture returns 24. The FIR4x4 YAML is an aliased square-sum rather than a general FIR; AXPY, Histogram and GEMV have skipped incorrect outputs; the Pool declaration targets a separate accelerator. Those candidates are not counted as newly supported CGRA workloads.

The new local `ExpandedSad` shares one unchanged sixteen-lane PrimitiveBank between public primitives and an autonomous masked, multiblock SAD sequence. It preserves wrapped 32-bit subtraction, absolute value and summation. All operands, accumulator and pipeline/control registers remain counted; no bulk local storage is subtracted. Static translation/lint/structure checks pass, 4,222 software arithmetic cases pass, and a 108-step bounded check of the actual controller with an abstract ADD/SUB/PASS bank passes. These checks do not establish whole-endpoint equivalence or modified-CGRA workload execution. No simulator or performance test ran.

The [completed seven-endpoint table](../build/experiments/asic/hybrid/workload-trial/results.md) adds 35,790.173373 SAED14 library area units for SAD: 33,638.9496 in the primitive bank and 2,151.2244 in native control and operand state. The SAD result meets 2 ns with +0.058382 ns worst setup slack, zero violating setup paths, no reported constraint violations and a clean timing check. Source and constraint hashes and leaf-area reconciliation pass. The six original rows remain unchanged. Seven independent expanded ASICs total 598,870.926939 units including local storage, versus unchanged CGRA 544,538.068098, yielding 9.0725% smaller CGRA area under this provisioning scenario. Excluding named storage gives 421,586.344539 versus 413,303.567298 units.

This seventh endpoint is optional independent provisioning, not an unavoidable arithmetic-support cost: the existing ASIC primitive interfaces can already express SAD with software sequencing. The expanded collection installs 112 primitive lanes versus sixteen CGRA PEs, without equal useful throughput evidence. The unchanged CGRA still fails 2 ns internally by 0.759389 ns; the new area total is not a timing-qualified performance-per-area result. All sources remain local experiment files, with no active VectorCGRA/ASIC update or commit.

## Area with and without data SPM

The [area extraction](../build/experiments/asic/hybrid/area-audit/spm.json) reuses existing mapped reports, verifies their hierarchical sums and retains the matched Hybrid ResNet performance record with its source hash. No fresh synthesis or hardware change is represented by this extraction. Values are SAED14 library area units, not a verified square-micrometre conversion.

| Installed backend | Including data SPM | Excluding data SPM module | Removed data SPM module |
| --- | ---: | ---: | ---: |
| Archived CGRA | 6,326,343.22 | 3,559,251.16 | 2,767,092.07 |
| Independent Add + ReLU ASIC, ResNet only | 1,381,973.36 | 9,701.04 | 1,372,272.32 |
| General five-unit ASIC, three small workloads | 1,955,667.27 | 554,548.50 | 1,401,118.78 |

The exclusion removes the named data-memory module, including its SRAM, metadata and internal decode/mux/pipeline logic. It retains everything outside that module: configuration caches, endpoint adapters, DMA, publication managers and compute-unit local arrays. The exact paths are `cgra/cgra_inst/cgra/data_mem`, `AsicFixed/memory` in each fixed endpoint, and `AsicCore/memory` in the general endpoint. This is component accounting, not synthesis of a functional memory-free accelerator. All three installed variants have 128 KiB aggregate externally addressable payload, but different physical padding and additional storage.

CGRA packet/patch configuration SRAM alone occupies 2,225,288.81 units and remains in the SPM-excluded column. Removing all SRAM macros instead gives 1,403,042.66 for CGRA, 32,967.49 for the fixed pair and 606,661.40 for the general ASIC; this alternative leaves memory-interface standard cells but removes configuration SRAM, so it is a different boundary and still not pure compute. Neither accounting view demonstrates smaller CGRA area in these implementations.

The fixed pair supports ResNet Add/ReLU only and must not be presented as the hardware inventory for all three workloads. Its performance record is the 41,298.33 versus 44,593.33 warm-cycle result above; general ASIC area belongs to the separate general-ASIC performance experiment. The initial six-private-endpoint inventory and its subsequent setup-closure follow-up are reported above. The follow-up meets ASIC endpoint setup at 2 ns; the archived figures in this section predate those changes. CGRA and general ASIC checkpoints have interrupted optimization and fail the 10 ns and 2 ns targets; fixed endpoints use a different 2 ns optimization flow. These reused areas are implementation diagnostics, not a common-frequency, timing-qualified cost/performance result.

### Area and timing diagnosis

The [area-gap audit](../build/experiments/asic/hybrid/area-audit/diagnosis.json) attributes the 4,370,675.95-unit difference between CGRA and the general five-unit ASIC as follows. These disjoint categories sum to both full endpoint areas.

| Component, library area units | CGRA | General ASIC | Difference | Share of difference |
| --- | ---: | ---: | ---: | ---: |
| Data SPM SRAM macros | 2,698,011.75 | 1,349,005.88 | 1,349,005.88 | 30.86% |
| Configuration SRAM macros | 2,225,288.81 | 0.00 | 2,225,288.81 | 50.91% |
| Standard cells, including register storage | 1,403,042.66 | 606,661.40 | 796,381.26 | 18.22% |
| Total | 6,326,343.22 | 1,955,667.27 | 4,370,675.95 | 100% |

SRAM accounts for 81.78% of the difference. CGRA's 35-bit data words occupy two 32-bit SRAM slices, turning 128 KiB payload plus metadata into 256 KiB physical SRAM. Its 32-job, 192-packet cache contains 6144×198-bit packet and 6144×67-bit patch arrays, mapped to another 288 KiB physical SRAM. ASIC descriptors remain charged as standard cells; zero configuration SRAM does not mean absent configuration. Width padding and cache capacity are implementation choices, not unavoidable properties of all CGRAs.

The CGRA standard-cell partition includes 362,795.68 units in PE control memories/local control, 303,709.63 in the configuration/control ring, 109,294.73 in constant stores, 148,887.54 in FU subtrees, 70,099.87 in register clusters, 32,434.64 in FU crossbars, 134,361.02 in routing crossbars and 241,459.55 elsewhere. FU subtrees include their internal control. ASIC standard cells also contain 24,668 bytes of local data arrays. These partitions do not establish a symmetric arithmetic-only comparison.

The [timing-path audit](../build/experiments/asic/hybrid/area-audit/timing-causes.json) records the following archived estimates. Negative slack is the missed deadline under the current analysis constraints.

| Mapped implementation | Setup slack at 2 ns | Setup slack at 10 ns | Worst-path data arrival |
| --- | ---: | ---: | ---: |
| CGRA | −31.38 ns | −23.38 ns | 33.37 ns |
| General ASIC | −39.2429 ns | −31.2429 ns | 41.2324 ns |
| Fixed Add endpoint | +0.0001 ns | Not used | 1.9845 ns |
| Fixed ReLU endpoint | +0.0003 ns | Not used | 1.9826 ns |

CGRA's archived critical path starts at a native tile's control-memory read address, propagates through opcode/valid decode, the Select FU's output-valid logic, the FU crossbar's ready logic and the Add FU's ready propagation, then reaches the register-cluster write enable. This is a native VectorCGRA control/handshake chain, not a serial Select-to-Add arithmetic datapath or a SoC-adapter path. The archived general-ASIC path is Softmax source selection, subtract/clamp, polynomial multiplications, square, shift and total accumulation within one cycle. Its division is iterative, but that archived exponential approximation is combinational. SRAM-input paths also fail at 2 ns, with slacks −26.75 ns and −13.1429 ns respectively. Later pipeline changes require separate timing and cycle measurements.

The estimates are strongly load-model dependent. The same protected Softmax child has 5.2528 ns arrival under standalone top wire-load model `8000`, but 41.2324 ns under the integrated endpoint's model `2000000`; CGRA uses model `8000000`. These identifiers are library model names, not measured physical wire lengths. The hierarchy fixes mapped children with `dont_touch`, preventing full parent-level reoptimization under the changed loads. Low-effort 10 ns mapping and interrupted optimization add further limitations. The 2 ns results reanalyze the same netlists, rather than resynthesizing for 2 ns.

First correct and normalize the hierarchical load/optimization methodology, preferably with placement-aware estimates, then evaluate pipelines on the remaining critical logic and memory paths. Fixing the load model alone does not establish 2 ns closure; the archived standalone Softmax also exceeds 2 ns. Do not infer physical Fmax or a required pipeline-stage count from period plus WNS. Fixed Add/ReLU have no explicit path exceptions and zero reported setup violations, but only tiny margins and no physical signoff. Any functional pipeline change requires new cycle measurements; the retained ResNet warm result remains a simulation-cycle result for its original hardware.

### Constraint and path follow-up

The [constraint follow-up](../build/experiments/asic/hybrid/timing-rerun/README.md) keeps the original CGRA architectural RTL and 2 ns clock unchanged. Reanalyzing the same mapped netlist with enclosed hierarchical wire-load estimates instead of one top-level model improves worst setup slack from −31.38 to −21.28 ns without changing area. The worst path becomes the SoC reset input to its response register. Reset remains timed; no false-path, multicycle or reset case-analysis exceptions were added. The improvement quantifies model sensitivity, not physical clock closure.

Completed register-origin reports under enclosed mode expose further failures: SoC `dmaSeqActive` to `respData` −19.04 ns; SoC `dmaSeqActive` to adapter `replayIndex` −15.14 ns; VectorCGRA integrated DMA state to data-SPM SRAM input −7.51 ns; and SoC DMA request-byte offset to read-response alignment data −0.56 ns. The SRAM path has a falling-edge capture and approximately 1 ns budget. Thus masking reset alone cannot establish closure. Buffer/inverter increments account for about 14.29 ns of the 21.03 ns DMA-active-to-response arrival, but these are load-dependent wire-load estimates, not intrinsic gate delays.

The earlier 900-second and 480-second trials were incomplete. A subsequent [completed top-level constraint optimization](../build/experiments/asic/hybrid/timing-rerun/constrained.json) runs with explicit 0.1 ns input transition, 1 fF output load, 0.2 ns maximum transition and maximum fanout 16. It exits 0 with a mapped checkpoint and all reports: setup improves from −21.3017 to −9.0419 ns; hold remains +0.0258 ns. Every one of 3747 child hierarchy rows and all 800 SRAM instances stay unchanged. The top-local area delta is −70.596, giving the updated storage-excluded CGRA endpoint area above. Reset, DMA and functional paths remain timed; no false paths, multicycle exceptions or reset case analysis were added.

After optimization, the worst response-register group is −7.0916 ns, adapter group −7.9984 ns, DMA byte alignment −0.5603 ns and native DMA-to-SRAM group −7.5113 ns. Group startpoints may change, so these are group-worst comparisons rather than identical-path deltas. The three SoC categories originate in `example/CGRA.scala` command/response and DMA-alignment logic, with adapter control in `socgen/cgra/CgraLinkAdapter.scala`. Their current handshake semantics do not justify multicycle exceptions.

The delay calculation separates the large model penalty: an apparent 7.56 ns inverter step contains 0.003047 ns intrinsic cell delay and 7.5545 ns incoming wire delay under the automatic 8000000 wire-load model. A [separate ideal-interconnect diagnostic](../build/experiments/asic/hybrid/timing-rerun/ideal.json), using the same original DDC without recompilation or path exceptions, gives +1.5987/+0.6834/+1.5553 ns slack for the three SoC groups. This diagnoses model sensitivity, not physical closure. Even with zero estimated wire RC, overall slack is −0.5186 ns: native tile control-memory address, routing/register selection, Mem FU address addition and data-memory arbitration reach a falling-edge SRAM input in 1.5207 ns against a 1.0021 ns requirement. Reset-to-SRAM remains −0.3428 ns in that diagnostic. The residual issue spans native VectorCGRA memory logic and the SAED SRAM mapping's half-cycle interface, and cannot be solved by masking reset or DMA paths.

### Softmax arithmetic correction

The current `rtl/asic/gpt/softmax.sv` replaces the long exponential-approximation expression with real registered arithmetic stages, and separates numerator preparation from iterative division. Exhaustive clamped-distance bounds permit narrower intermediate registers without changing the previous INT32 wrap, shift, clamp or rounding behavior. This is a multicycle FSM, not an II=1 streaming pipeline. The [comparison](../build/experiments/asic/softmax-pipeline/comparison.json) verifies 16,470 cases and 43,911 outputs; the preserved old binary also passes the expanded fixtures. The Core bridge additionally passes 32,971 cases and 57,224 outputs. Each two-column Softmax row gains 24 cycles: fused 73→97 and staged 75→99. Other common unit cases retain their previous cycles.

The [matched standalone synthesis](../build/experiments/asic/softmax-pipeline/synthesis/comparison.json) recompiles both old and new RTL at 2 ns with the same SAED14 library and enclosed automatic wire-load flow. Old/new areas are 2,986.43/1,918.26 library units, setup slacks −0.342653/+0.004394 ns and hold slacks +0.034753/+0.037985 ns. Setup violations fall from 44 to zero. New critical timing is in the iterative divider, from denominator to quotient; arithmetic staging and proven width reduction lower combinational area while sequential area increases. The new unit retains 33 minimum-capacitance output violations with no downstream load specified. These are standalone ideal-clock pre-layout results, not full endpoint closure or a physical 500 MHz claim. The full-endpoint area tables above describe the archived pre-correction implementation.

## System comparison

Both systems must use the same Rocket, Gemmini, AutoLink protocol, bus width, clock plan, memory service, tensor precision and input/output placement. Replace only the non-matrix backend and its required adapter. Keep the small workload shapes above. Dedicated RTL has runtime operation, phase, shape/count, scalar parameters, addresses and job descriptors; it does not require CGRA instruction or routing packets. Measure each design's real configuration traffic rather than padding the ASIC descriptor to match a CGRA program.

The common endpoint protocol is `AutoEndpointIO`. `CgraLinkAdapter` also depends on native CGRA packet fields, completion counts, patches and DMA, so it is not a drop-in ASIC shell. Pool and AES streaming adapters start on copy acceptance and support one data input; they cannot directly serve GCN's CSR/feature inputs or GPT's banked input/affine parameters. The ASIC integration uses a buffered endpoint with runtime descriptor storage, real TileLink input transfers, output publication and completion after the required transfers drain. AutoLink's dependency, watch, copy and compute semantics remain common. Adapter, configuration storage, DMA, local memory and interconnect area all count toward system cost.

`configs/soc/autolink/asic.yaml` installs both backends in one simulator, with the same Rocket, two Gemmini DIM64 engines and AutoLink fabric. The ASIC endpoint has 32 cached descriptors and a 128 KiB local memory. Each descriptor selects the unit, operation, phase, shape, scalar divisor, four input word addresses, output word address and next descriptor. The current endpoint supports one buffered slot and explicit addresses, with raw INT32 input/output; it does not automatically relocate tiles or pack INT8 output. Unit-local arrays and the common memory both count toward its storage cost. Installed-backend area must exclude the other backend even though the validation simulator contains both.

The native API is [asic.h](../tests/include/asic.h). [System tests](../tests/asic/system/README.md) distinguish non-matrix interface tests from complete workloads. GCN uses actual Gemmini projection published to on-chip SPM, followed by an ASIC DMA read. Initial complete ResNet and GPT paths use actual Gemmini results with CPU representation conversion and DRAM intermediates, all within the timing interval. Those paths establish functionality before measuring on-chip locality improvements.

The current dedicated hardware is bounded to the small shapes: GCN has capacity for 16 nodes, 256 edges and 128 channels; LayerNorm handles H128 rows; Softmax handles two-column causal rows; the GPT vector unit handles up to 1024 words. Runtime inputs, weights and parameters remain variable. Larger shapes need an explicit implementation and validation; these results do not establish arbitrary-shape support.

Use the following experiment matrix to separate backend cost from orchestration and locality:

| Backend | Per-operator submission | Chained submission |
| --- | --- | --- |
| CGRA | CPU submits and waits at each logical operator boundary | CPU submits one chain; cached jobs execute in order and retain compatible intermediates in local SPM |
| Dedicated ASIC units | CPU submits and waits at each logical operator boundary | The same AutoLink machinery sequences units; supported local links retain intermediates without CPU submission |

The primary paired comparison uses the same submission policy on both backends. CGRA chained versus ASIC per-operator may also be shown, labelled as a combined architecture-and-orchestration comparison. Keep the chained ASIC result visible so savings provided by AutoLink itself are not attributed exclusively to CGRA. Disabling a local link for a materialization ablation must be explicit and must not force DRAM traffic into the main ASIC baseline.

Existing `cgra_job_sequence` and `cgra_job_full` can execute multiple phases under one outer job and publish only the final output. This is launch coalescing with local storage retention, not automatically one fused arithmetic datapath. Direct data edges between stages on the same physical endpoint are unsupported by the current graph contract; represent a compatible local chain inside that endpoint's job sequence. Arithmetic fusion, such as Add+ReLU, is a separate switch and must preserve intermediate rounding and saturation.

### Evaluation coverage and strategy matrix

The supplementary general-ASIC regional-Hybrid performance matrix is complete: three supported workloads × CGRA/ASIC, with one cold and three warm executions per configuration, for 24 executions and 26,624 checked outputs. All six configurations pass. [Results, cost and all samples](../build/experiments/asic/hybrid/current/results.md) retain both favorable and unfavorable outcomes. The project's primary P1 27-case matrix is unchanged.

| Workload | Shape | CGRA warm CPU cycles | ASIC warm CPU cycles | CGRA fewer cycles |
| --- | --- | ---: | ---: | ---: |
| GCN | 16 nodes, 76 edges, 64→128 features | 93,864.33 | 94,282.67 | +0.44% |
| ResNet | 16×16×8 input, 8×8×16 output projected block | 69,734.00 | 59,205.33 | -17.78% |
| GPT | N2/H128, two heads, FF512 block | 231,670.67 | 236,560.67 | +2.07% |

All six configurations were freshly measured on one frozen current-source general five-unit ASIC/CGRA simulator. No archived cycle result is reused in this table. The [region manifest](../build/experiments/asic/hybrid/audit/regions.json) documents matching logical boundaries on both backends. GCN has one aggregation+ReLU region; GPT has 18 non-matrix regions and ten CPU-submitted Gemmini GEMMs. ResNet uses two cached, self-contained backend programs and two matching AutoLink submissions; its three Gemmini convolutions and all host representation conversions remain CPU-orchestrated and timed. AddReLU is fused on both backends. Software submission and native-start fields are accounting from the program, not additional hardware measurements.

This is explicitly handwritten regional Hybrid: the CPU controls region boundaries and hardware sequences eligible work inside each region. It is not a compiler-generated `soc.region` plan, the P1 compiler-policy experiment, or full-graph cross-IP Auto. Original labels and evidence remain unchanged. The current runner verifies source, generated RTL, simulator, ELF and raw-log hashes, normal process exits, all numerical outputs and the recorded submission/transfer fields. Its separate two-input ASIC AUTO interface test also passes, checking 256 additional outputs. The earlier archived measurements and their 185-file audit remain available separately.

Warm execution is the selected primary steady-state metric. Results are means of the deterministic input sequence 1,0,1 after cold input0, with no CPU cache flush between samples; they do not establish statistical significance. This general-ASIC variant shows fewer CGRA warm cycles for GCN and GPT and more for ResNet. Cold execution is retained separately to show configuration overhead. The recorded cold-plus-three-warm sequence is not a specified deployment mix and is not used to overturn the warm-performance comparison.

Manual and Auto remain optional control-policy ablations for this supplementary comparison. The policy definitions are:

- Manual: the CPU submits native tasks individually, retaining valid configuration caches.
- Hybrid: the CPU submits fixed, documented contiguous graph regions, with hardware sequencing inside each region.
- Auto: hardware follows graph dependencies across the workload, using actual profile batches when capacity requires them; preparation, final output and drains remain measured.

Cache-off, host-materialized versus on-chip transfer, fused versus split operators, and tile granularity are separate controls. The split Add/ReLU hardware below is a separate sensitivity baseline; it cannot replace the five-unit ASIC inventory in a cross-workload cost comparison. [Coverage status](../build/experiments/asic/coverage/status.json) separates the completed backend matrix from remaining cost work and the completed primary experiment.

Complete-endpoint mapped area is now available for both backends in the same SAED14 RVT TT0.8V25C logic and SRAM libraries. Each total includes the native compute hardware, local memory, configuration storage, adapter, DMA, publication interface and endpoint queues; shared Rocket, Gemmini and upstream AutoLink logic are excluded. The ASIC inventory includes all five units, not the separate Add/ReLU-only baseline.

| Complete endpoint | Standard-cell area | SRAM area | Total cell area | SRAM macros |
| --- | ---: | ---: | ---: | ---: |
| CGRA | 1,403,042.66 | 4,923,300.56 | 6,326,343.22 | 800 |
| General five-unit ASIC | 606,661.40 | 1,349,005.88 | 1,955,667.27 | 256 |

Values are library area units, not placed-and-routed area or measured chip cost. The current CGRA endpoint is 3.235× the ASIC area; SRAM accounts for 81.78% of the difference. CGRA retains its eight 4096×35-bit native banks, 6144×198-bit packet cache and 6144×67-bit patch cache. Its native memory uses two 32-bit macro slices per 35-bit word. The ASIC common 128 KiB memory uses 256 128×32-bit SRAM macros, while unit-local asynchronous arrays remain standard cells to preserve their access semantics. Both memory transformations passed cycle-level checks against their original implementations. This implementation does not support a lower-CGRA-area claim. Standard-cell area excluding SRAM macros is reported separately and is 2.313× larger for CGRA in this mapping. This includes configuration registers and register-based local arrays; it is not pure arithmetic area. [The SPM comparison contract](../build/experiments/asic/hybrid/reconciliation.md#spm-comparison-contract) separates the measured shared-SPM ASIC from the proposed independently provisioned private-SPM baseline.

The CGRA result comes from an independently reopened and verified mapped checkpoint after interrupted optimization. The ASIC outer endpoint and Core finish normally, but their GptVector child also comes from an independently verified interrupted-optimization checkpoint. These are complete mapped area diagnostics, not a matched pair of fully timing-converged implementations. [CGRA reports](../build/experiments/asic/cgra-cost-complete/README.md), [ASIC reports](../build/experiments/asic/cost-complete/README.md) and the [independent acceptance review](../build/experiments/asic/cost-review/review.json) retain source, library, netlist and report hashes, failed attempts and structural checks. No power estimate, physical routing or full mapped-SoC gate-level verification is claimed.

Both backends fail the 10 ns synthesis target. The simulator's nominal 500 MHz clock corresponds to 2 ns; separate STA reads the same mapped netlists at 10 ns and 2 ns with unchanged I/O constraints. Optimizer-created temporary false paths are removed before final STA. CGRA setup slack is −23.38 ns at 10 ns and −31.38 ns at 2 ns; ASIC setup slack is −31.2429 ns and −39.2429 ns respectively. Neither backend reports unconstrained-endpoint or missing-input-delay warnings in the final coverage checks. The [CGRA results](../build/experiments/asic/cgra-cost-complete/results.json) and [ASIC results](../build/experiments/asic/cost-complete/results.json) retain both clock conditions and SRAM-interface timing. CPU-cycle comparisons are valid under the common simulation clock model, but do not establish physical 500 MHz operation or a realizable common-frequency speedup.

Five original Scala shell sources and generated endpoint RTL are missing from the earlier frozen simulator archive. An isolated current-source general-ASIC revision was therefore built and frozen. Its ASIC AUTO interface smoke and all six workload/backend configurations pass, with 24 workload executions and 26,624 checked outputs. [The new run](../build/experiments/asic/hybrid/current/README.md) retains its own source and binary hashes. The complete CGRA synthesis closure is equivalent to this revision's preprocessed RTL, including unchanged scratchpad and packet/patch capacities. Earlier frozen performance remains archived separately; the current table uses only the new measurements.

### GCN scheduling and return-path investigation

The completed chained GCN baseline is 7200 warm cycles slower than ASIC: 4252.33 cycles (59.06%) come from its input/output transfers and 2775.33 (38.55%) from non-matrix execution. Both already use one outer CPU submission, so this gap is primarily transport and schedule cost. The input/output buffers are each one contiguous aligned 8192-byte transfer; the supported read-only CGRA window does not permit direct Gemmini writes into CGRA memory.

An isolated II9 schedule removes redundant moves and uses a separate zero-token broadcast for ReLU. It retains the same frozen hardware, three lanes and 12 PEs. All five matching from-YAML reference cases and two complete native inputs passed (4096 checked outputs). Native cycles fall from 43044 to 39744 per input, a 7.67% reduction. Static packets fall from 249 to 228 and cached packets per row from 124 to 121; this does not change installed cache capacity or establish an area reduction.

A separate complete-system test reuses the idle Gemmini accumulator to return raw INT32 data after aggregation. It copies the same 2048 words through a 32-by-64 view, includes the load/store/fence in output timing and preserves the caller output buffer. Both backends receive this option. Eight executions passed all 16384 outputs: CGRA warm total averages 97245.67 cycles versus 101434.33 with native return, while ASIC averages 94639.67 versus 94234.33 with native return. Output phases average 5276 versus 9333 for CGRA and 7357.33 versus 6876.33 for ASIC. Therefore the combined candidate uses Gemmini return for CGRA and retains native return for ASIC.

The combined system comparison passed all eight executions and 16384 output checks, giving 16 new system executions and 32768 checks across both experiments. CGRA warm samples are 95221, 93315 and 93057 cycles (mean 93864.33); the same-ELF ASIC samples are 95719, 93576 and 93546 (mean 94280.33). CGRA lowers its previous warm total by 7.46% and uses 0.44% fewer cycles than ASIC on this deterministic sequence: near parity rather than a substantial speed advantage. Its non-matrix phase averages 45537.33 versus ASIC 46237.67, while the slower input copy remains.

Cold cycles are 179722 for CGRA versus 100903 for ASIC. Including the cold sample and three warm samples, CGRA totals 461315 cycles versus ASIC 383744, or 20.21% more. Configuration amortization therefore remains a limitation; the small warm lead does not establish an advantage for short runs.

[Complete comparison and phase breakdown](../build/experiments/asic/gcn-opt/results.md), raw logs, generator sources and hardware/binary hashes are under `build/experiments/asic/gcn-opt/`. These changes add no RTL or installed hardware. Gemmini reuse is evaluated only after projection and aggregation complete, not under concurrent workload overlap. The baseline's shared byte-wise input staging remains unchanged, and neither these measurements nor the native schedule establishes complete-backend area or physical timing qualification.

### Repeated supported system sweep

The currently executable matrix is complete: 37 cases, 148 execution samples and 188,384 checked outputs, with every simulator exiting successfully. [Complete tables and samples](../build/experiments/asic/full/results.md) and the [comparison figure](../build/experiments/asic/full/results.svg) retain both favorable and unfavorable results. This covers the supported block shapes and controls, not full-size models. The selected regional-Hybrid backend comparison is reported above; Manual/Auto remain optional controls.

The subsequent sweep runs complete workloads at currently supported shapes, without waiting for new hardware integration or physical qualification. Each configuration has one configuration-cold execution followed by three warm executions with fixture inputs 0, 1, 0, 1. One-time setup is keyed to the sample number, not input identity. CPU caches remain warm, and per-launch packet replay and descriptor use remain measured. These repeats characterize this deterministic input/cache sequence, not independent random trials.

GCN, GPT and the fused ResNet control have completed all 40 executions with numerical checks, successful simulator exits and unchanged input/source/binary hashes. They use the same archived general-ASIC hardware. Warm means are:

| Workload | Mode | CGRA CPU cycles | ASIC CPU cycles | CGRA fewer cycles |
| --- | --- | ---: | ---: | ---: |
| GCN, 16 nodes, 64→128 features | per-job | 104,933.3 | 94,513.3 | -11.02% |
| GCN, 16 nodes, 64→128 features | chain | 101,434.3 | 94,234.3 | -7.64% |
| GPT, N2/H128/two heads/FF512 | operator | 262,565.7 | 264,525.3 | +0.74% |
| GPT, N2/H128/two heads/FF512 | chain | 231,670.7 | 236,360.3 | +1.98% |
| ResNet, 16×16×8 to 8×8×16 | Fused AddReLU, native submits, host intermediates | 71,600.0 | 58,661.0 | −22.06% |

GPT chain mode reduces submissions from 51 to 18 and backend DMA payload from 41,216 to 33,024 bytes on both backends. Operator mode already retains LayerNorm and Softmax intermediates locally; the additional transfer reduction comes from joining FC1 requantization and GELU. This is local chaining with CPU orchestration of the complete block, not full-graph Auto. CGRA uses the validated two-lane mappings and power-of-two strength reduction, while the ASIC vector retains its generic divisor implementation; a corresponding ASIC fast path is possible. The measured difference therefore applies to these implementations rather than establishing an exclusive architectural advantage.

The fused ResNet control preserves two native CPU submissions and fused AddReLU on both backends. CGRA resends native configuration packets on each call, while ASIC starts resident descriptors. The warm mean gap includes that command traffic, computation and transfers; its phase counters do not isolate pure arithmetic or fusion benefit. It is a separate control from the split-ASIC cached/SPM experiments below. Cold-start results also remain separate and continue to favor ASIC for these general-backend workloads.

Exact samples, phase counters, commands, source hashes and process exits are retained under `build/experiments/asic/full/{gcn,gpt,resnet_fused}/`. The split ResNet size/cache/locality and AddReLU micro sweep is recorded separately under `build/experiments/asic/full/resnet/`. These are functional RTL CPU-cycle comparisons; current complete-endpoint area and a common physical frequency remain unqualified.

The split-ASIC ResNet sweep completed twelve configurations and 48 executions, including both supported sizes, CPU-packet versus cached CGRA submission, and host versus SPM intermediates. Warm values below are means of three samples. Medium is 16×16×8 input with 8×8×16 output; spatial is 32×32×8 input with 16×16×16 output. Both execute the complete projected residual block.

| Shape | Intermediates | CGRA cached | CGRA CPU packets | Split ASIC | Cached CGRA fewer cycles |
| --- | --- | ---: | ---: | ---: | ---: |
| medium | host | 70,412.7 | 73,068.3 | 71,835.0 | +1.98% |
| medium | SPM | 41,177.0 | 43,657.3 | 44,134.3 | +6.70% |
| spatial | host | 265,990.7 | 268,840.0 | 274,477.3 | +3.09% |
| spatial | SPM | 147,975.7 | 150,899.0 | 162,556.3 | +8.97% |

For the spatial block, keeping intermediates on chip reduces cached-CGRA warm cycles by 44.37% and ASIC warm cycles by 40.78%. Both retain the real SPM copies and representation conversion costs. CGRA uses two non-matrix submissions and no Add-to-ReLU peer copy; the split ASIC uses three submissions and a 16 KiB peer transfer. The fused general-ASIC control above remains a separate result.

The AddReLU sweep completed fifteen cases and sixty executions. N denotes the element count. Each size has a configuration-cold sample and three warm samples; CPU caches are retained between sizes.

| N | CGRA cached | CGRA CPU packets | Split ASIC | Cached CGRA fewer cycles |
| ---: | ---: | ---: | ---: | ---: |
| 8 | 1,433.3 | 2,216.3 | 1,140.7 | -25.66% |
| 32 | 3,460.7 | 4,138.7 | 3,249.3 | -6.50% |
| 128 | 11,756.7 | 12,554.3 | 12,286.7 | +4.31% |
| 512 | 44,962.3 | 45,691.7 | 47,490.0 | +5.32% |
| 1024 | 89,075.3 | 90,294.7 | 94,425.3 | +5.67% |

The measured warm crossover occurs between 32 and 128 elements with cached CGRA submission, and between 128 and 512 elements with CPU-packet submission. These boundaries apply to the tested sizes and interfaces. Including one cold plus three warm executions reverses most warm-only advantages: spatial cached/SPM ResNet retains a 0.51% cycle reduction, while chained GPT uses 28.30% more cycles than ASIC. The complete four-run totals are retained alongside warm means; no setup cost is discarded.

### Mapping preflight before the policy matrix

The November 12 preflight keeps the archived 4×4 Attention hardware fixed and checks schedules before launching the full policy matrix. The two-lane GCN mapping uses ten PEs, preserves the 128-channel feature stride and per-channel accumulation order, and passes all five existing from-YAML cases. Across both actual GCN inputs, all 4096 final values pass; native aggregation latency drops from 127,024 to 63,872 cycles per input. The three-lane candidate folds ReLU into each reduction path and uses twelve PEs. All five reference cases and both complete input sequences pass, with 43,044 native cycles per input (66.1% below the original map). Its 249 static packets are loaded once; each of 16 cached row jobs uses 124 packets, within the existing 192-packet capacity. Its complete-SoC evaluation is recorded below.

The GPT candidate specializes width-128 LayerNorm indexing and proven nonnegative division by powers of two. Signed variable division remains unchanged. All reference cases pass. Native measurements across two actual inputs and both LayerNorm layers check 2088 values and reduce aggregate cycles from 224,912 to 90,992. Separate signed-boundary checks for divisors 64, 128 and 256 each reduce 21-element requantization from 1880 to 494 cycles; all 256 INT8 activation inputs pass, with fused GELU/requantization reducing 23,847 to 7206 cycles. The full-block candidate retains generic division for PV's divisor 127. The ASIC LayerNorm already performs constant strength reduction; this optimization corrects CGRA mapping overhead rather than demonstrating an exclusive CGRA capability.

A paired GPT layout control separates inputs, gain, bias, output and statistics into different SPM banks. All 4176 checked values pass, but aggregate LayerNorm cycles change only from 224,912 to 224,908. This negligible result is retained; bank separation is not the selected optimization.

The three-lane GCN complete-SoC check uses the same ELF in fresh CGRA and ASIC processes, with one cold input followed by a changed warm input. All 8192 outputs across both backends pass. Cold cycles are 189,690 for CGRA and 100,871 for ASIC; warm cycles are 102,885 and 95,686 respectively. CGRA remains 7.52% slower in warm total cycles. Its warm non-matrix interval is 48,971 versus 46,241, while input-copy and final-output intervals add further overhead. Thus the native speedup does not establish an end-to-end GCN advantage. Logs, executable, generated interface headers and source hashes are under `build/experiments/asic/preflight/system/`.

The optimized GPT complete-block comparison also passes both inputs on both backends (1024 final outputs), with identical non-matrix DMA payload counts and 18 outer submissions per run. CGRA cold/warm totals are 571,975/280,717 cycles, versus ASIC 274,684/240,453. CGRA remains 16.75% slower when warm; its cold setup costs 265,698 cycles versus ASIC 10,466. The valid kernel gains therefore narrow the system gap without establishing overall GPT superiority. These chained submissions are not the missing whole-graph Auto policy.

The subsequent two-lane GPT mapping uses existing memory-connected PEs 0 and 1 for independent vector elements, without adding hardware. Fixed-divisor quantization, generic divisor 127 and residual kernels pass paired native tests with 4608 outputs per version. Each cached program uses at most 138 packets, below the existing 192-packet capacity. The final complete-block comparison again uses one identical ELF for both backends and checks 1024 final outputs without errors. Warm totals are 235,328 CGRA cycles and 239,804 ASIC cycles: CGRA uses 1.87% fewer cycles in this single changed-input sample. Cold totals are 568,251 and 274,713, including setup of 308,046 and 10,649 respectively. Both execute 18 outer submissions and ten Gemmini GEMMs, with 18,560 input DMA bytes and 14,464 output DMA bytes. Thus this measured difference comes from mapping improvements, not fewer submissions or measured DMA savings. Cold CGRA remains slower; repeated warm samples and common physical timing qualification are still needed. Final paired records are in `build/experiments/asic/preflight/system/lanes-results.json`; earlier single-lane results remain in `results.json`.

A resident GCN control experiment moves only the fixed per-iteration count and lower-bound configuration into one-time setup; REARM clears constant-queue validity, so constant loads remain. Both versions pass 32 row tasks across two changed inputs. Cached packets per task drop from 124 to 100; measured configuration replay through completion falls from 44,836 to 44,452 cycles per input, while kernel cycles remain 43,044. This 0.86% replay reduction is not an observed full-SoC gain and is insufficient evidence that the 7.52% system gap closes. Evidence is under `build/experiments/asic/preflight/gcn/resident/`.

A fourth copy of the present GCN lane is not legal on the frozen mesh: the unused PEs 10, 11, 14 and 15 have no direct SPM ports. Each lane requires distinct gather-load and reduction-store PEs; four copies require eight memory-connected PEs, but the mesh provides seven (0, 1, 2, 3, 4, 8 and 12). Packet capacity would permit four lanes, so memory connectivity is the binding constraint. Reusing a port or routing through an occupied PE requires a different schedule with measured arbitration and data movement. No four-lane performance result is claimed.

These native measurements run from first per-PE launch acceptance through last completion acceptance. They exclude configuration, CPU orchestration and DMA. Candidate schedules and reference evidence are under `build/experiments/asic/preflight/gcn/` and `build/experiments/asic/preflight/gpt-schedule/`; the paired layout control is under `build/experiments/asic/preflight/gpt/`. Complete small-workload runs use the frozen original general-ASIC SoC and isolated headers generated from its original YAML, preserving the current active split-system build. Neither these kernel improvements nor logical configuration-capacity savings establish physical timing closure or lower total hardware area.

The hardware preflight separately generates a heterogeneous integrated CGRA with DIV/SQRT only on PEs 0, 1, 4 and 5 through the existing `tile_overrides.fu_types` mechanism. Elaboration confirms four `ExclusiveDivRTL` instances instead of sixteen; PE count, other FU lists, control capacity, 128 KiB payload SPM, links and DMA remain unchanged. The newly translated Verilator model passes 35 reference jobs and 1953 outputs across ResNet, three-lane GCN and GPT. An additional 12 two-lane GPT jobs check 1536 outputs on the same new library, including generic divisor 127. No old homogeneous model substitutes for these checks. Evidence is under `build/experiments/asic/preflight/hetero/`. These tests leave external DMA idle; the complete-SoC timings above still use the original homogeneous SoC. Reduced unit count is not a measured total-area saving, and the heterogeneous candidate still needs full-system timing and area qualification.

The historical native area report attributes 11,829.048 library area units to all sixteen divider subtrees, only 0.295% of its 4,014,065.131572 total. Removing twelve corresponds to at most 8,903.0436 historical subtree units (0.222% of that total), before any unmeasured remapping effects. Thus divider pruning is functionally valid but does not by itself establish a meaningful total-area advantage. The dominant known storage overhead is the existing 35-bit word mapping into two 32-bit SRAM slices; changing that representation requires preserving metadata, port semantics and latency. Exact hierarchy selections and report hashes are recorded in `build/experiments/asic/cost-audit/audit.json`.

A further isolated storage candidate splits each actual 4096×35 bank into a 32-bit payload array and a 3-bit metadata array padded to a 4-bit macro. Both slices use the same logical addresses and enables, with the existing delayed-write and forwarding wrapper unchanged; this avoids packed-tag read-modify-write. The complete DataMemWrapper ready/valid, routing, reset and backpressure behavior is compared against the original native bank. Both the vendor macro models and a collision-poison model pass 21,529 cycles and 15,703 response checks each, including 257 current-write collisions, 770 previous-write collisions, 1730 held-response cycles and eleven reset cycles. The existing SyncRam reference tests also pass (three cases). Requests settle before falling-edge chip-select sampling; this is functional cycle equivalence, not physical timing closure.

Across eight banks, the candidate uses 256 SRAM2RW128x32 and 256 SRAM2RW128x4 macros instead of 512 SRAM2RW128x32 macros. Physical capacity falls from 256 KiB to 144 KiB while logical payload and metadata capacity remain unchanged. Verified same-corner library areas give a macro-only sum of 2,100,676.197888 versus 2,698,011.718656 units, a 22.14% reduction in this SRAM contribution. This arithmetic excludes changes in muxes, decode, wiring and timing repair; it is not a measured complete-CGRA area reduction or proof of lower cost than ASIC. The original mapper and active RTL are unchanged. Candidate source, generated wrapper, vendor provenance and validation logs are under `build/experiments/asic/preflight/memory/`; full integration and matched endpoint PPA remain outstanding.

### Timing and counters

Use one continuous CPU `rdcycle` interval for the primary result. Start before input staging and any configuration required by that run; stop after final output is visible at the same caller-owned destination and all required DMA/completion acknowledgements have drained. Exclude fixture construction, reference checking and printing. Do not construct total time by adding overlapping compute and DMA intervals.

- Cold execution includes native parameters, CGRA programs or ASIC descriptors, adapter job capture, graph/run configuration, parameter loading, input staging, submission, execution, waits and final output transfer.
- Warm execution repeats the same configured graph with different runtime input, retains only configuration that actually remains valid, and still counts staging, submissions, transfers, waits and output visibility. Apply this retention policy to both systems.
- Workload switching executes GCN → ResNet → GPT → GCN on each fixed installed design, without rebuilding hardware or resetting between runs. Count any necessary program replacement, descriptor selection, parameter transfer and data movement. This measures switching in addition to demonstrating functional reuse.
- Record CPU submissions, outer AutoLink jobs, native accelerator starts and CGRA per-PE launch packets separately. A seven-phase LayerNorm may have one outer submission and seven internal starts; neither number replaces the other.
- Report configuration bytes and transactions, payload DMA bytes, TileLink payload bytes, DRAM payload bytes and internal-link bytes separately. Local SPM accesses are not zero-cost and are not DRAM transactions. Derive counters from accepted handshakes or actual issued commands, not fixed overhead estimates.
- Report total cycles, setup/capture intervals and execution/drain intervals, plus overlap counters where available. All time values must name their clock domain. Only claim a common-frequency speedup after both mapped designs meet that frequency; otherwise report qualified clock periods and elapsed time.

### Workload boundaries and acceptance

GCN measures projection followed by aggregation and activation; retain projected data on chip where supported and compare an aggregation/activation chain against separate commands. ResNet measures convolution outputs, activation and residual Add+ReLU with the same skip tensor and convolution work in both systems. GPT measures the same matrix operations plus LayerNorm, Softmax, requantization, GELU and residual paths; group compatible non-matrix phases without dropping dependencies across Gemmini. Internal division and square-root latency must be reflected in timing-closed ASIC hardware.

Start with operator versus compatible operator-chain granularity, then vary rows/tiles under the same workload shape and memory budget. Hold arithmetic precision and the amount of work fixed. Count all dedicated units required to support the three workloads in one installed system, counting shared units once; compare against one fixed reusable CGRA design. Keep storage implementation and capacity explicit before presenting any area-normalized result.

The system result is ready only after the ASIC endpoint, configuration path and DMA are integrated; both backends pass matching small-shape top-level tests; cold, warm and switching runs produce numerical matches; counters identify actual submission and transfer savings; and the reported frequency is supported by synthesis timing. The standalone 92 ASIC cases and 76 CGRA phase jobs do not satisfy these system acceptance criteria.

## Separate fixed Add and ReLU cores

### Clock and latency qualification

The active split SoC connects CGRA, Add and ReLU to the same Rocket/RoCCGroup clock. Its simulation source has a 2 ns period (500 MHz). The existing standalone synthesis reports use a 10 ns constraint (100 MHz), so those reports do not qualify the active simulation frequency. All system tables here report logical RTL CPU cycles; they are not timing-closed 500 MHz latency or proof of a physical speedup.

The initial fixed cores did not execute a whole tensor in one cycle. Before the retiming changes below, under the standalone bench's inclusive accepted-start-through-done convention, Add takes `2*N+2` edges and ReLU takes `N+2`: 258/130 edges for 128 elements and 2050/1026 for 1024. Memory reads are synchronous. The arithmetic between register boundaries is a 32-bit wrapping adder or a sign-select; memory, address generation and control also belong to the timing path. ASIC active-cycle counters additionally include adapter/DMA busy intervals and must not be interpreted as pure arithmetic latency.

A common-frequency comparison requires both backends, their memory implementations, adapters and DMA to meet the same target under matching libraries, PVT and timing constraints. Any failed path must be pipelined or the shared clock lowered, with affected RTL simulations rerun. Extra cycles must follow the implemented register boundaries rather than an arbitrary multiplier on measured results.

The clock audit maps each fixed core's full 64 KiB memory to 128 `SRAM2RW128x32` macros using the existing SAED14 TT library. A delayed-write/forwarding wrapper preserves read-first behavior; all 30 existing cases and their complete cycle records match the original RTL, checking 9,620 outputs. The macro has rising-edge data timing but falling-edge CSB setup/hold constraints. Those half-cycle checks remain enabled. At a 2 ns target, the Add core mapping reports approximately −2.69 ns worst setup slack and the ReLU core −1.35 ns. Add's worst path runs from the SRAM read-bank register to pending write data; ReLU's runs from the index register to an SRAM chip select. These mappings fail the target despite correct logical cycle counts.

The complete current ASIC endpoint candidates also fail 2 ns. Their exact generated dependency closures include the native adapter/configuration cache, DMA, publication manager, TileLink fragmenter, memory arbiter and async queue boundary; only the already-tested fixed-core memory implementation is substituted. All leaves resolve to real library cells and each endpoint retains 128 SRAM macros. The Add endpoint reports −6.31 ns worst setup slack and the ReLU endpoint −6.42 ns. The critical endpoint paths run from the copy-request queue through copy-status checks and DMA address/length generation, so arithmetic-only retiming cannot qualify these designs. These runs use 0.1 ns input/output delays, SAED14 RVT TT 0.8 V 25 C, ideal clocks and medium-effort mapping. Mapping success is distinct from setup, hold and design-rule closure; constraint reports retain the violations.

| Mapped candidate | Target period | Worst reported setup slack | Target met |
| --- | ---: | ---: | --- |
| Fixed Add core and 64 KiB SRAM | 2 ns | −2.6859 ns | No |
| Fixed ReLU core and 64 KiB SRAM | 2 ns | −1.3508 ns | No |
| Complete Add ASIC endpoint | 2 ns | −6.3123 ns | No |
| Complete ReLU ASIC endpoint | 2 ns | −6.4217 ns | No |

An archived CGRA core mapping has the same original pickled RTL hash as the active core, but its historical SRAM implementation was only statically validated. That mapping already reports −23.38 ns slack at 10 ns, with a 28.38 ns path ending at a falling-edge SRAM constraint. It cannot qualify the current enclosing CGRA adapter or establish a common operating frequency. These are pre-layout wire-load estimates, not placed-and-routed timing. Raw evidence, manifests and clock-source hashes are retained in `build/experiments/asic/clock-audit/`; the candidate memory implementation is under `build/experiments/asic/split/timing/`. That initial audit did not modify the active RTL or simulator.

### Retimed implementation

The current fixed ASIC sources use the same memory pipeline in behavioral simulation and `ASIC_SRAM` synthesis. Registered bank selection, synchronous SRAM access, grouped read selection and final readout replace the large combinational memory path. Host reads accepted at edge 0 return valid data after edge 4; host writes commit at edge 3. Independent writes remain accepted every cycle. The adapter waits for pending writes before reads, compute launch and DMA completion. Pending status follows the registered write-valid stages instead of reducing 128 bank-enable signals; exactly one bank is selected per accepted write, so this changes the control path without changing any cycle boundary.

Add computes parallel eight-bit partial sums, then selects carries in its second arithmetic stage. Address counters use parallel nibble carry logic; completion compares against a captured final index. Add retains initiation interval two and ReLU one. Measured standalone execution is `2*N+12` and `N+11` cycles, including fill and final write drain. These are actual register stages, not a multiplier applied to old cycle counts. Both behavioral and SRAM-interface modes pass 30 cases and 9,620 checked outputs with identical cycle records.

AutoLink copy handling captures the request, checks bounded local offsets, then issues DMA. DMA transfer size is registered. Address, remaining-byte and payload counters use short carry-select chains; range sums and publication addresses also use parallel carry logic. These combinational rewrites retain the execution schedule. Configuration fields, native commands, publication and copy interfaces remain available. Eight Scala tests cover memory arbitration, pending write drain, adapter copy/error behavior, counter carry/borrow boundaries and wide range sums.

The final generated Add and ReLU endpoints pass synthesis setup/hold at a common 2 ns target, using SAED14 RVT TT 0.8 V 25 C cells, 0.1 ns input/output delays, and 128 real SRAM macros per endpoint. This check includes the fixed datapath, local memory, native adapter, DMA and publication interface. The clock and SRAM constraints were unchanged through optimization.

| Complete endpoint | Setup slack | Hold slack | Target period |
| --- | ---: | ---: | ---: |
| Fixed Add ASIC | +0.0001 ns | +0.0080 ns | 2 ns |
| Fixed ReLU ASIC | +0.0003 ns | +0.0092 ns | 2 ns |

These are pre-layout ideal-clock, wire-load results with very small setup margins. Maximum-capacitance and maximum-transition violations are zero, but minimum-capacitance violations remain (2,013 Add; 1,069 ReLU), as does the optimizer's default zero-leakage objective violation. No constraints were waived to obtain closure. This is synthesis setup/hold closure, not physical signoff. Full reports and hashes are in `build/experiments/asic/retimed/endpoints-v5/results.json`; final reports are under `AsicAccelerator/incremental/out` and `AsicAccelerator_1/incremental2/out`. CGRA has not passed the corresponding physical timing qualification, so the cycle comparisons below remain conditional on a common operating frequency.

The retimed system passed 24 cold/warm runs and checked 7,328 outputs, including the full small ResNet block and five AddReLU sizes. The sweep used the pending-bank reduction version; the final registered-valid rewrite is cycle-equivalent because its valid bit equals the OR of the exhaustive one-hot bank enables. Both final standalone memory modes match every earlier cycle/data record, and rebuilt small ResNet regressions after the registered-valid rewrite and the short-carry rewrite each pass four more runs with all six measurement/counter lines identical (`regression.json` and `regression-v5.json`). Small ResNet takes 67,416/15,140 cold/warm CPU cycles on CGRA and 12,826/12,231 on ASIC. Thus ASIC remains faster for the full small block. The following warm micro measurements include identical input/output work; the pair interval contains actual ASIC peer DMA and both submissions.

| INT32 elements | CGRA total | Retimed ASIC total | CGRA pair | Retimed ASIC pair |
| ---: | ---: | ---: | ---: | ---: |
| 8 | 2,201 | 1,106 | 1,265 | 203 |
| 32 | 4,151 | 3,233 | 1,354 | 503 |
| 128 | 12,495 | 12,013 | 2,123 | 1,703 |
| 512 | 45,824 | 47,279 | 5,045 | 6,503 |
| 1024 | 90,202 | 94,557 | 8,739 | 12,926 |

At 512 and 1024 elements, CGRA uses fewer total cycles: 3.1% and 4.6% fewer than the retimed ASIC. The corresponding pair intervals use 22.4% and 32.4% fewer cycles. At 1024 elements the ASIC performs 4,096 bytes of peer SPM traffic taking 9,755 cycles, whereas the fused CGRA has no peer transfer. This is on-chip accelerator-to-accelerator DMA, not a measured DRAM round trip. These results describe the specified fixed operator bank; the earlier fused ASIC comparison remains valid separately. They do not establish a physical 500 MHz CGRA advantage while CGRA timing remains unqualified.

After the short-carry rewrite, a fresh isolated 1024-element invocation also passes all four cold/warm runs and 4,096 outputs. Its warm CGRA/ASIC totals are 90,114/94,678 cycles, with pair intervals 8,744/12,913 cycles: CGRA uses 4.8% fewer total cycles and 32.3% fewer pair cycles. This invocation has a different preceding cache/execution history from the five-size sweep, so these paired results are recorded separately in `results-v5.json`. Together with the rebuilt small ResNet test, the final-source regression checks 4,608 outputs in eight runs with zero errors.

Current retiming evidence is retained in `build/experiments/asic/retimed/`. Each row is one changed-input warm run, not a repeated-trial mean. The original clock audit and initial performance tables below describe the older implementation. Physical timing and system measurements must be matched to their exact source manifests.

### Medium residual block

The same retimed v5 SoC also runs a 16×16×8 → 8×8×16 projected residual block, increasing the final element count from 128 to 1024. The three Gemmini convolutions, separate first ReLU, fused-CGRA versus split-ASIC residual pair, host representation conversions and timing boundaries are unchanged. `scripts/asic/resnet_system.py --case medium` generates the fixture; `RESNET_DATA` selects it in `split.c`. The original small fixture remains byte-identical. This changes software data and runtime counts only, so the simulator and endpoint timing results remain applicable.

Both backends use the same ELF and run separately from fresh simulator processes, each executing a configuration-cold input followed by a changed-input warm run. All four runs pass, checking 4,096 outputs and the ASIC payload/start counters. The warm measurements are:

| Interval | CGRA cycles | ASIC cycles |
| --- | ---: | ---: |
| First activation, including conversions and input/output DMA | 28,748 | 23,406 |
| Branch input staging and DMA | 19,575 | 17,056 |
| Residual AddReLU, including ASIC peer DMA | 8,956 | 12,909 |
| Final output DMA and fence | 4,828 | 7,604 |
| Complete block, including remaining stages | 72,264 | 71,171 |

CGRA uses 30.6% fewer cycles in the residual pair, but 1.5% more cycles for the complete block in this single warm sample. The first activation and branch loading offset the pair and final-output savings. ASIC peer DMA moves 4,096 bytes in 9,755 cycles. Cold totals are 124,656 CGRA and 74,279 ASIC, including their different configuration costs. This supports a faster fused interval and closely spaced measured warm totals; it does not establish statistical equivalence, whole-block speedup, or lower hardware cost.

Evidence, source/simulator hashes, the identical ELF and the paired logs are under `build/experiments/asic/medium/`. Historical P1 results for 32×32×16 → 16×16×32 are retained separately: they use strip scheduling, Auto/Hybrid capture and publication paths, different hardware capacities and packet layouts, and different timing boundaries. Their totals cannot be reused directly as the CGRA denominator for this experiment. CGRA physical timing remains unqualified, as above.

### Spatial scaling and configuration reuse

The scale experiment keeps the retimed v5 hardware fixed and increases the residual block from 1024 to 4096 outputs, holding input/output channels at 8/16. Operand regions start at 0, 4096, 8192 and 12288 words, reusing first-activation storage after its last consumer. A medium-shape control with this layout passes four runs and 4096 outputs: warm CGRA/ASIC totals are 72,589/71,502 cycles and pair intervals are 9,004/12,919. This does not show an advantage from bank separation. Host buffers also grew, so total-cycle changes cannot be attributed solely to bank placement.

The first 4096-output attempt submitted all 16×16 output positions as one native convolution. It exceeded Gemmini's 128-row accumulator workspace and, for two convolutions, its scratch workspace. Both backends produced the same 1812 incorrect outputs. These failed measurements remain in `build/experiments/asic/scale/failed.json` and are excluded from performance comparisons. The corrected shared convolution routine selects legal row strips, checks native geometry and storage, and fences each strip. This case uses eight output rows per strip, including the required halos; each convolution makes two submissions on both backends.

Configuration caches, adapters and DMA are already present in both RTL backends. CPU-packet and cached CGRA modes select software submission paths on the same hardware; they do not remove or add cache hardware. Area comparisons must include the complete endpoints in either mode. The primary on-chip comparison below enables both backends' existing configuration reuse; CPU-packet mode is retained as an overhead control.

A separate same-ELF experiment uses the existing CGRA configuration cache and AutoLink compute trigger, with identical input/output DMA and memory placement. Cold setup includes capture and graph loading. Warm execution includes job selection, trigger, completion consumption and graph drain. All six runs pass and check 6144 outputs. The 1024-output measurements are:

| Mode | Cold block cycles | Warm block cycles | Warm AddReLU cycles |
| --- | ---: | ---: | ---: |
| Fixed Add/ReLU ASICs, cached descriptors | 75,082 | 71,472 | 12,918 |
| CGRA, CPU configuration packets | 125,254 | 72,448 | 9,030 |
| CGRA, cached configuration | 128,785 | 70,014 | 7,667 |

Cached CGRA uses 2.0% fewer warm whole-block cycles than the ASIC pair and 40.6% fewer pair cycles; it uses 3.4% fewer whole-block cycles than CPU packet submission. Cold execution remains slower. These are individual changed-input samples, not repeated-trial means or proof of a statistically stable 2% advantage. Evidence and frozen sources/ELF are in `build/experiments/asic/cache/`. The three modes use the same binary within this experiment; older layouts and binaries remain separate records.

The corrected 4096-output block passes all six runs, checks 24,576 final outputs, and matches the ASIC transfer/start counters. The three modes again use one ELF with separate fresh simulator processes. The shared convolution function executes six legal native tiles per block, preserving identical scheduling and timing boundaries across backends.

| Mode | Cold block cycles | Warm block cycles | Warm AddReLU cycles |
| --- | ---: | ---: | ---: |
| Fixed Add/ReLU ASICs, cached descriptors | 284,356 | 281,469 | 51,337 |
| CGRA, CPU configuration packets | 328,107 | 276,368 | 30,710 |
| CGRA, cached configuration | 331,176 | 272,855 | 29,208 |

At 4096 outputs, ordinary CGRA uses 1.8% fewer warm whole-block cycles than the ASIC pair; cached CGRA uses 3.1% fewer. The cached fused interval uses 43.1% fewer cycles and avoids a 16,384-byte peer copy costing the ASIC 38,939 cycles. Its first activation still costs 107,475 versus 92,872 ASIC cycles, and branch input loading costs 86,630 versus 76,361; these boundaries offset much of the fused interval's benefit. Cold execution remains slower on CGRA. These individual samples support a measured warm advantage for this schedule, not guaranteed superiority for other shapes or a physical common-frequency speedup.

Final spatial logs, sources, ELF and hashes are in `build/experiments/asic/scale/tiled/`. The overall index is `build/experiments/asic/scale/results.json`: 16 valid runs and 34,816 checked outputs across the layout control, medium cache comparison and corrected spatial comparison. Failed unsplit-convolution records remain explicitly excluded.

### Hardware cost audit

The available hardware reports do not demonstrate lower CGRA area. The two current fixed ASIC endpoints together occupy 1,381,973.36 SAED14 library area units: 1,349,005.88 in SRAM macros and 32,967.49 in standard cells. This includes both 64 KiB memories, fixed cores, adapters, descriptor storage, DMA, publication managers and endpoint queues. Shared Rocket, Gemmini, upstream AutoLink fabric and physical implementation are outside this endpoint total. SRAM accounts for about 97.6% of that measured area.

The historical CGRA native subtree has 4,014,065.13 area units, including 512 SRAM macros for eight 4096×35-bit banks. The chosen mapping uses two 32-bit macro slices per 35-bit word; the extra physical capacity comes from width padding, not read-port replication. It is an implementation cost to investigate, not a universal CGRA requirement. The behavioral native RTL hash matches the current design, but the historical macro substitution was only statically checked and did not close timing even at 10 ns. Its area is diagnostic evidence, not a common-frequency comparison.

The historical full CGRA endpoint report (5,441,829.54 area units) also has different wrapper, DMA and adapter sources. Current packet and patch storage declares 6144×198 and 6144×67 bits, totaling 203,520 logical bytes; the old trimmed packet-only report cannot represent this configuration. A fresh complete CGRA mapping with functional SRAM validation and the same timing target is still required. Area units are retained as reported because the reports do not explicitly establish square micrometres.

Cross-workload resource sharing also needs a matching installed baseline. The general ASIC design has one instance each of GCN, Vector, Norm, Softmax and GPT Vector, with shared 128 KiB SPM and 24,668 bytes of declared unit-local arrays; its complete mapped area is unavailable. Comparing the two Add/ReLU-only endpoints against a CGRA supporting all three workload families cannot establish that cost claim. Full inventory, report paths and source hashes are in `build/experiments/asic/scale/cost.json`.

The measured performance opportunities are configuration reuse and eliminating the residual pair's peer copy. Across the other workload families, candidates include keeping GCN aggregation outputs near their consumers and chaining GPT normalization, requantization and activation phases. Existing ASIC GCN/ReLU fusion and GPT local job chains already provide some of these benefits, so those baselines must retain their capabilities; no new scaled GCN or GPT performance result is claimed here. The on-chip comparison below evaluates direct Gemmini publication and hardware INT8/INT32 conversion while preserving native output semantics, legal strip halos and buffer lifetimes. CGRA memory metadata packing and cache capacity are cost candidates, but changing either requires regeneration, numerical validation and fresh timing/area reports. The present results support fusion and reuse benefits against this fixed operator-bank baseline, while leaving total area savings and physical frequency qualification unresolved.

### On-chip intermediate exchange

The on-chip variant uses the same frozen retimed v5 RTL, including both configuration caches, adapters, DMA engines and the existing Gemmini accumulator conversion path. It changes software scheduling and tensor placement. Convolution outputs are published as INT8 into the reserved upper half of Gemmini SPM. Before each endpoint input, Gemmini expands signed bytes into INT32 staging; endpoint DMA reads that staging directly. The first ReLU result is copied back into staging and narrowed to INT8 for the next convolution. Both ASIC and CGRA execute the same conversions and preserve the same arithmetic; the first ReLU's 0–127 range makes hardware narrowing identical to the previous host conversion. Final results still return to host memory.

The publication regions and staging region are disjoint and remain outside native convolution scratch. Native convolution tiles, conversions and DMA operations fence or wait before storage reuse. The on-chip variant uses four conversion operations per block: three expansions and one narrowing. Their command, transfer and completion time is included in activation or branch-loading intervals. The original host-intermediate path remains selectable for a same-ELF control. All primary comparisons enable CGRA configuration caching and retain ASIC cached descriptors; this is not a comparison of different cache hardware.

The 128-output functional smoke passes all four cold/warm runs, checking 512 final outputs and the ASIC DMA/start counters. Its CGRA warm total is 10,096 cycles and ASIC warm total is 9,461; both modes run sequentially in that smoke process, so the independent medium-shape comparison is the performance result. Evidence is under `build/experiments/asic/onchip/`.

The 1024-output same-ELF comparison then passes all eight runs with 8192 checked outputs and matching ASIC counters. Each row below has a separate fresh simulator process, one cold input and one changed-input warm input. Both paths include all conversion, DMA, completion and visibility work inside CPU timing.

| Backend and intermediate path | Cold block cycles | Warm block cycles | Warm first activation | Warm branch loading |
| --- | ---: | ---: | ---: | ---: |
| ASIC, host materialization | 76,641 | 72,854 | 24,696 | 17,077 |
| ASIC, on-chip exchange | 48,165 | 45,069 | 10,094 | 3,893 |
| Cached CGRA, host materialization | 129,905 | 71,243 | 28,509 | 19,635 |
| Cached CGRA, on-chip exchange | 100,295 | 42,041 | 13,343 | 5,682 |

On-chip exchange with hardware format conversion reduces warm cycles by 41.0% for CGRA and 38.1% for ASIC relative to each backend's host path. The CGRA advantage over ASIC changes from 2.2% with host intermediates to 6.7% with on-chip exchange. The remaining fused-pair intervals are 7,671 CGRA versus 12,918 ASIC cycles; ASIC's 4096-byte peer copy costs 9770 cycles. The first activation and branch loading still favor ASIC, so the whole-block advantage is smaller than the fused interval's advantage. Cold ASIC remains faster because CGRA capture/setup costs more.

For each on-chip run, endpoint DMA reads no intermediate payload from host memory and writes only the 4096-byte final output there. It moves 16,384 bytes for intermediate endpoint transfers, and the four Gemmini conversions move 20,480 bytes of payload. ASIC additionally moves the 4096-byte residual-pair intermediate. These software payload totals exclude native convolution publication and working-memory traffic; they are not total physical bus transactions. Both placement and conversion implementation change together, so this comparison measures their combined benefit rather than isolating either effect.

All 12 smoke and medium runs pass, checking 8704 outputs. `build/experiments/asic/onchip/results.json` records the paired results, zero process exit codes and source/ELF manifests. The medium experiment is a single sample per phase and covers one residual block. Hardware is unchanged, so previous ASIC setup/hold qualification still applies and CGRA physical common-frequency qualification remains outstanding.

### Fixed operator-bank baseline

`configs/soc/autolink/split.yaml` defines a second baseline with two physically separate ASIC endpoints. Add contains only wrapping INT32 addition; ReLU contains only signed activation. Their operation is fixed during elaboration and cannot be changed by runtime descriptors. Each owns 64 KiB of synchronous local storage, matching the CGRA's 128 KiB in total. Both retain the native descriptor cache, adapter, AutoLink interface, publication window and TileLink DMA. Equal storage capacity does not establish equal area: both ASIC adapters and DMA engines must be counted.

`tests/asic/system/split.c` compares those endpoints with one fused CGRA AddReLU program. The ASIC path submits Add, waits for completion, reads its actual publication window into ReLU's separate SPM through DMA, then submits ReLU. The CGRA executes one program and has no inter-endpoint copy. This is a fixed operator-bank baseline with separate native submissions, not a restriction shared by every ASIC design. The earlier fused-ASIC measurements remain a separate comparison.

The first scope is the complete small ResNet block and an AddReLU sweep of 8, 32, 128, 512 and 1024 INT32 values. The full block executes all three convolutions on Gemmini and checks every final output. The sweep checks changed inputs and signed overflow behavior. Both backends stage identical host inputs and return identical host outputs; the ASIC adds exactly one N-word peer transfer. CPU timing includes setup when configuration-cold, staging, configuration packets, execution, waits, transfers and the final visibility fence. Counter reads, reference checks and printing are outside that interval. Warm repeated micro runs reuse each backend's retained configuration; the CGRA uses `CGRA_REPEAT` only when the same job remains resident.

The fixed-core RTL bench passed 30 cases and checked 9,620 outputs, including negative intermediate sums, independent memories, descriptor capture, in-place operation and memory boundaries. The adapter and memory-port tests passed three cases covering both 64 KiB and 128 KiB endpoints. Fixture-port copy cycles from the standalone bench are not SoC DMA cycles. Reproduction commands and argument selection are in the [system test instructions](../tests/asic/system/README.md).

The rebuilt split SoC passed all four full-block runs, checking 512 final outputs. Full small ResNet takes 67,416 cold / 15,140 warm CPU cycles on CGRA and 11,243 cold / 10,646 warm on the two fixed ASICs. The warm residual AddReLU interval is 2,507 versus 1,168 cycles; the ASIC interval includes a measured 727-cycle peer copy of 512 bytes. Whole-block native submissions are two versus three. Both backends read 1,536 host payload bytes and write 1,024; ASIC hardware counters additionally confirm the peer input transfer and one Add plus two ReLU starts. Thus this small block still favors the split ASIC despite its extra launch and transfer. Cold configuration is included, and processor caches are not reset between backend runs.

The five-size micro sweep passed another 20 cold/warm runs and checked 6,816 outputs. The following warm CPU-cycle measurements include input staging and common host DMA in the total. The pair interval includes submission, execution, waits and the ASIC peer copy; it excludes common host input/output DMA. Each row is one changed-input warm run, not a repeated-trial mean.

| INT32 elements | CGRA total | Split ASIC total | CGRA pair | Split ASIC pair |
| ---: | ---: | ---: | ---: | ---: |
| 8 | 2,201 | 1,010 | 1,265 | 148 |
| 32 | 4,151 | 2,944 | 1,354 | 352 |
| 128 | 12,495 | 10,957 | 2,123 | 1,168 |
| 512 | 45,824 | 43,153 | 5,045 | 4,432 |
| 1024 | 90,202 | 86,332 | 8,739 | 8,807 |

At 1024 elements, the pair intervals are within 1%: CGRA uses one native submission and no peer transfer, while the ASIC uses two submissions and copies 4096 bytes between local memories. The measured ASIC peer-copy interval is 5,655 cycles. CGRA's total remains 4.5% higher, so this experiment establishes the launch/traffic savings but no end-to-end speedup. Host staging dominates the largest micro totals and remains explicitly reported. Neither this result nor equal aggregate SPM capacity establishes an area advantage. The split baseline has only been measured for AddReLU and small ResNet; the GCN/GPT results below belong to the earlier general ASIC design. All 24 split-system runs, 7,328 checked outputs, hardware counters and source/build hashes are recorded under `build/experiments/asic/split/`, in `results.json`, `inputs.sha256.json`, `system.log` and `micro.log`.

## Measurements

The primary system comparison passed 16 runs and checked 17,920 final outputs: eight GCN runs, four ResNet runs and four GPT runs. Each workload uses two different inputs for configuration-cold and warm execution. GCN and GPT use the same AutoLink submission path on both backends. ResNet uses fused AddReLU and two native submissions on each backend. All matrix products and convolutions execute on actual Gemmini hardware.

| Workload and policy | CGRA cold cycles | ASIC cold cycles | CGRA warm cycles | ASIC warm cycles |
| --- | ---: | ---: | ---: | ---: |
| GCN, separate jobs | 229,699 | 96,852 | 187,299 | 93,834 |
| GCN, chained jobs | 218,039 | 96,326 | 181,570 | 93,548 |
| ResNet, fused AddReLU | 66,194 | 11,145 | 15,074 | 10,717 |
| GPT, chained phases | 854,106 | 275,609 | 567,131 | 238,992 |

The current small-shape mappings do not show a CGRA performance advantage. Warm CGRA/ASIC cycle ratios are 1.94 for chained GCN, 1.41 for ResNet and 2.37 for chained GPT. The cold runs include backend configuration but do not imply processor or shared Gemmini cache resets. Both use a common simulated CPU-cycle domain; neither this table nor standalone synthesis establishes the integrated ASIC's physical clock frequency. Machine-readable records, logs and source hashes are in `build/experiments/asic/system/results.json` and `inputs.sha256.json`.

GCN chaining reduces CGRA root submissions from 16 to one while retaining 16 native row jobs; ASIC submissions reduce from two to one while retaining two native operator jobs. Both transfer the actual 8 KiB projected matrix from Gemmini SPM and return an 8 KiB result. CGRA's warm non-matrix interval changes from 133,462 to 129,912 cycles, while ASIC changes from 46,448 to 46,213. The total-cycle change also includes staging/cache variation, so it is not entirely submission overhead.

GPT chaining uses 18 root submissions and the same 18,560 input and 14,464 output DMA bytes on either backend. The ASIC hardware counters confirm 51 native starts in both cold and warm runs, 120 configuration commands when cold and zero when warm. CGRA's separate-phase cold diagnostic also passed at 883,660 cycles, 51 root submissions, 22,656 input bytes and 18,560 output bytes. Its extra 8 KiB of traffic materializes the FC1 requantization/GELU boundary; it is not the paired ASIC performance comparison. CGRA still replays complete cached controls when phases overlap control memory. Optimizing that replay and measuring packed on-chip ResNet/GPT transport are distinct follow-up experiments, rather than assumed benefits in these results.

- A kernel launch is an accepted start command at the tested RTL interface. It is not a CPU instruction count, an AutoLink launch or a per-PE configuration packet count.
- Kernel cycles run from accepted start through final completion. Fixture generation and reference checking are excluded. Loading, computation and result transfers must have separate counts when their intervals differ.
- Interface bytes count observed handshakes at the test interface. They are not DRAM bytes. A testbench that materializes a boundary models an external consumer; an actual SoC DMA path has not been measured by that bench.
- Staged and fused variants use the same arithmetic and input tensors. Report both requested intermediate outputs and final outputs, preserving negative and saturated values as required by the existing integer contract.
- Hardware reuse across workloads means running the same physical design with different runtime commands/data. It does not mean adding results from different CGRA configurations. Shared dedicated units are counted once in an installed hardware inventory.
- Whole-SoC CPU cycles in the earlier P1 archives cannot be divided by standalone ASIC cycles to claim speedup. A comparison needs matched tensors, measurement boundaries, interfaces, memory service and timing constraints.

## First implementation

The vector path contains a CSR weighted aggregation unit and a streaming ReLU/Add/Add+ReLU unit. The GPT path contains dedicated normalization, causal Softmax and elementwise arithmetic. Python runners generate fixtures using the existing workload references, then execute compiled RTL and check the outputs. Any missing coverage and any idealized memory service must remain explicit in the result file.

The first staged-versus-fused ASIC tests isolate launch and boundary-transfer effects. They do not establish that CGRA has lower area or latency. A unified CGRA baseline must execute the matching kernels on one frozen fabric; old workload-specific hardware capacities cannot stand in for that experiment.

Run the two ASIC benches from the repository root:

```sh
python3 scripts/asic/vector.py
python3 scripts/asic/gpt.py
```

Results and exact source hashes are written to `build/experiments/asic/vector/results.json` and `build/experiments/asic/gpt/result.json`. The vector bench checks repeated inputs and backpressure; the GPT bench checks both workload inputs and bounded arithmetic corner cases. Run descriptions and legal interfaces are in [vector RTL](../rtl/asic/vector/README.md) and [GPT tests](../tests/asic/gpt/README.md).

The vector bench also tests `GcnVector`, which directly connects two dedicated units through ready/valid. Its one outer command causes two internal starts. It transmits intermediate values over the internal link without testbench materialization, providing a connected ASIC baseline in addition to separate launches and fused arithmetic. This distinguishes benefits from local connectivity from benefits specific to reconfigurable resource sharing.

## Small-shape results

The first ASIC run passed 92 cases and checked 37,723 outputs. The following historical cycles include command acceptance and final completion; staged commands have no software dispatch gap. Its GPT units used combinational division. The current RTL replaces division with a real iterative divider, so the historical LayerNorm and Softmax cycles are not current results or evidence of a realizable clock frequency.

| ASIC kernel | Separate launches | Fused launch | Connected units |
| --- | ---: | ---: | ---: |
| GCN aggregation + ReLU, 16 × 128 | 35,347 | 33,297 | 33,298 |
| ResNet Add + ReLU, 128 values | 260 | 130 | Not measured |
| LayerNorm, one H128 row | 537 | 531 | Not measured |
| Causal Softmax, one two-column row | 9 | 7 | Not measured |

GCN's connected ASIC uses one outer command and two internal starts, transfers 8,192 bytes over its internal link, and avoids the staged bench's 16,384 bytes of intermediate output/input traffic. ResNet fusion avoids 1,024 intermediate interface bytes. GPT staged and fused modes already retain intermediates locally; their traffic is identical. These results support measuring launch granularity and connectivity separately from resource sharing.

The frozen CGRA comparison passed 76 kernel/phase jobs across the same three workload families. All reports use the same 4 × 4 fabric with 48 control slots and a 128 KiB synchronous scratchpad. The separate workload invocations establish reuse of the same hardware design, but do not measure switching latency between workloads. GPT coverage here is LayerNorm and Softmax; the ASIC bench additionally covers the remaining non-matrix vector kernels.

Use the repository Python environment, which supplies CFFI, for the frozen simulator:

```sh
chipyard/.conda-env/bin/python scripts/asic/cgra.py --workload gcn
chipyard/.conda-env/bin/python scripts/asic/cgra.py --workload resnet
chipyard/.conda-env/bin/python scripts/asic/cgra.py --workload gpt
```

The runner loads the archived simulator in `build/experiments/p1/gpt2/cgra1/v002/reference`; it does not regenerate RTL. Reports in `build/experiments/asic/cgra/` record hardware hashes, logical starts and per-PE launch packets separately. The GPT replay restores control slot zero when returning from resident Softmax phases to a full kernel configuration, and passes two input sequences without resetting the model.

The next cost comparison must count the installed dedicated units with shared units counted once, charge compatible storage implementations/capacities, and include controller/interconnect cost. A directly connected ASIC remains a required baseline. The current standalone cycle counts have different launch granularities and memory interfaces, so they do not establish CGRA speedup or an end-to-end result.

## Synthesis

Stage a stable, tested top in a new directory:

```sh
python3 scripts/asic/stage.py --top Vector --rtl rtl/asic/vector/Vector.sv --output build/experiments/asic/synthesis/Vector
```

The staged directory contains frozen RTL, source hashes, the DC script and a manifest. In the established remote SAED14 environment, run `sha256sum -c inputs.sha256` and `/tmp/cgra-area-20261030/dc.sh -f synth.tcl` from that directory. The script rejects unresolved leaves and writes mapped RTL, area and timing reports. A success marker means mapping completed, not that the target frequency was met.

The clock target is 10 ns with 0.1 ns input/output delay and the SAED14 RVT TT 0.8 V 25 C standard-cell library, matching the earlier component synthesis target. Local RTL arrays are charged as standard cells; this first mapping does not replace them with SRAM macros. Areas are library area units for standalone kernels, including their declared storage but excluding SoC DMA/interconnect. They must not be compared directly against SRAM-adapted P1 totals as a normalized cost result.

Fresh synthesis of the iterative-divider RTL completed for `GptNorm` (50,632.028333 library area units, worst reported slack 0.00 ns, MET after rounding) and `GptSoftmax` (2,755.330742 units, +4.74 ns). The reports use ideal clocks and wire-load estimates, not placed-and-routed timing. `GptVector` was deferred after the primary system comparisons completed: its mapping process consumed CPU but produced no new mapping milestone for about 15 minutes, and no mapped area or timing result was available. Snapshots, partial logs and completed reports are retained under `build/experiments/asic/synthesis/iterative/`. This partial component result does not establish total backend cost or an integrated clock frequency.
