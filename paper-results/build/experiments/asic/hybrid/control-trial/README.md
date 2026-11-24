# Isolated CGRA control simplification

This experiment changes copies under this directory only. Active VectorCGRA sources, its Git revision and the existing dirty worktree are preserved. No commit, simulator run or performance rerun is part of the experiment. `source-state.json` records the initial source identities. The candidate retains runtime configuration and all 48 control slots; it is not a fixed-program fabric.

## Structural changes

The ring candidate replaces seventeen three-port routers, virtual-channel queues, shortest-path routing, credit counters and adapters with one round-robin addressed bus and seventeen destination mailboxes. All seventeen packet ports, all legal destinations, all 198 packet bits and backpressure remain. Packet capacity falls from 272 to 17 slots and state from 55,012 to 3,400 bits. The original ring may update `vc_id` on wraparound; the candidate preserves it and no longer uses it for routing.

The bus admits at most one packet per cycle. A mailbox cannot drain and refill on the same edge, so continuous traffic to one destination is limited to one packet every two cycles. A blocked destination is skipped when choosing another eligible source. These are deliberate changes to transport concurrency, buffering and latency, not proof that the removed resources were unused in every workload.

The selected PE candidate preserves its 48-by-111-bit instruction store, static prologue tables, original instruction access logic, execution counters and supported commands. Its incoming command and FU-return queues each shrink from two entries to one non-bypass slot. Full queues cannot accept a simultaneous replacement on dequeue; this can add backpressure. Stored control contents, word width and legal-address read/write behavior are retained.

An additional trial replaced the binary-indexed instruction read with shared address decode and an OR selection tree, using explicit per-word write enables. Final mapping showed no access-area saving: that combined variant occupies 12,175.01 units versus 12,171.95 for queue trimming alone. Its early intermediate area was larger, but the final cleanup removed that intermediate increase. The combined trial and its original assembled copy are retained under `pe/candidate` and `decoded`; the selected assembly uses `pe/queue`.

## Local integration

`assemble.py` applies the bus and queue-only PE candidate to a frozen full `IntegratedCgraWithDmaRTL_single` copy and writes `IntegratedTrial.sv`. It replaces the ring and PE-control module bodies and inserts uniquely named PE helpers, leaving the active generated RTL untouched. `assembly.json` binds the assembled file to the frozen baseline and candidate hashes. Regenerate the assembled copy after any candidate change. Select `--pe candidate` only to reconstruct the decoded-read alternative.

The twenty-four standalone baseline ring modules match the frozen integrated design after ignoring generated labels, localparam names, comments and whitespace; see `ring-baseline-check.json`. Original and selected-candidate full-design Verilator lint both exit zero. The candidate retains the same nine preexisting datapath circular-combinational warnings; no new reported signal appears in that warning set. Width-expansion warnings fall from 187 to 164, with two truncation warnings unchanged. This is a structural integration check, not proof that the inherited warnings are harmless or that workloads remain correct.

## Validation and area interpretation

The PE legal-address read rewrite passes a combinational equivalence check for arbitrary store contents. The ring candidate passes temporal-induction checks for one-hot valid arbitration, selected-packet integrity, admission into the destination mailbox and stable stalled outputs. Neither establishes whole-CGRA functional or cycle equivalence. There is no simulator or performance result for the new architecture.

Standalone original/candidate mappings use the same SAED14 RVT TT library, 2 ns clock and I/O constraints. Ring reports are in [ring/results.json](ring/results.json); PE reports are in [pe/results.json](pe/results.json). Ring operational mailbox storage remains counted. PE area is reported both with and without identified static configuration storage; runtime queues, counters and pipeline registers remain counted. Full-area and storage-excluded views must not be mixed.

The selected single-PE implementation changes full area from 12,815.260648 to 12,171.949073, a 5.0199% reduction. Each version contains exactly 6,768 identified static configuration cells occupying 6,310.4832 units. Excluding them gives 6,504.777448 versus 5,861.465873, a 9.8898% reduction. All three PE variants have +1.308921 ns worst setup slack and no reported constraint violations. Queue trimming accounts for essentially all the reduction; the decoded-read rewrite adds no measured benefit.

The matched standalone ring comparison is complete: original area 138,178.748464 versus 6,314.035230 for the bus, a 95.4305% reduction. Worst setup slack is +0.367287 ns versus +0.672539 ns at 2 ns; neither final constraint report lists a violation. Both jobs have a successful completion marker and zero process exit status. The original standalone mapping took 2,171 seconds. Its area differs from the archived in-context 303,709.63 figure, which is why the two mapping boundaries must not be mixed.

| Matched standalone component | Original area | Selected area | Reduction |
| --- | ---: | ---: | ---: |
| Control packet network, including operational storage | 138,178.75 | 6,314.04 | 95.43% |
| One PE control, excluding static configuration storage | 6,504.78 | 5,861.47 | 9.89% |
| One network plus sixteen representative PE controls | 242,255.19 | 100,097.49 | 58.68% |

The last row is a replicated-component subtotal. It is not a resynthesized whole-CGRA area, and it cannot be subtracted from an unrelated whole-design mapping to claim an exact new core area.

An isolated component's mapping boundary differs from its archived in-context mapping. Summing one ring plus sixteen standalone PE controls is a replicated-component estimate, not a new measured full-CGRA area. The assembled whole CGRA has been linted but has not been resynthesized. Passing a component's 2 ns constraint does not close all paths in the assembled design, and the original 108.32万 core-area figure cannot be replaced by a component-only result.

## Reproduction

From the repository root, run `python build/experiments/asic/hybrid/control-trial/assemble.py --pe queue` after the component generators. Lint the resulting copy with `chipyard/.conda-env/bin/verilator --lint-only -Wno-fatal --top-module IntegratedCgraWithDmaRTL_single build/experiments/asic/hybrid/control-trial/IntegratedTrial.sv`. This command performs no simulation or simulator build. Run `verify.py` to check source isolation and assembly hashes. Each component directory retains its generator, static checks, synthesis constraints, job metadata, logs and reports.
