# Issue #4 T7: End-to-End Gemmini-to-CGRA Result Validation Prompt

## Recovery rule

Before continuing after any interruption, context compaction, restart, or handoff, read this file completely, then read the T6, T5, T4, T3, T2, and T1 prompts and inspect root, Chipyard, Gemmini, VectorCGRA, and nested gemmini-rocc-tests status/diffs. Confirm the committed T1-T6 chain and preserve every unrelated dirty/generated file. Do not rely on remembered context and do not overwrite this prompt after implementation begins.

## Repository and workflow

Work in `/home/jjqin/CGRA-SoC`. Obey root `AGENTS.md`, README support boundaries, Issue #4 and all seven discussion comments, `docs/plans/cgra-dma-tilelink-refactor-prompt.md`, and narrower repository conventions. Preserve unrelated work; never reset, clean, restore, or reformat it. Generated files must be changed through their generator and are not committed by default. Commit an owning submodule before its parent pointer. Do not push or create a pull request.

One newly started implementation subagent must complete T7, followed by a newly started independent reviewer. Resolve every review finding before scoped local commits. Do not commit before review and never push.

## Committed baseline

- T1 Chipyard `6ca6b381`, root `7535751`: existing Gemmini full-width accumulator publication works with destination stride 4; producer safety requires the final dedicated writer TileLink D handshake.
- T2 Chipyard `9d8859fd`, root `db4f6e2`: one coherent 64 KiB external SPAD at `0x60000000`, with two 1024-byte reserved publication slots.
- T3 Chipyard `3dbe5fae`, root `d9d03b8`: typed two-slot endpoint owns `FREE -> PRODUCING -> READY -> READING -> FREE`.
- T4 Chipyard `42bc95f4`, root `92125a2`: producer READY occurs only after the matching final `spad_writer` D handshake.
- T5 Chipyard `0f5fdaf7`, root `d13d651`: consumer-pull adapter reuses the real CGRA TileLink DMA; readStart is the first real Get acceptance and RELEASE/completion follows matching real `CMD_DMA_DONE`. CGRA local SPM is now eight 16-word banks, 128 words/512 bytes.
- T6 Chipyard `09718179`, root `30fab41`: a generic 16-entry launch-sequence gate retains full generated `CMD_LAUNCH` packets, correlates exact typed T5 completion identity, and reports LaunchAccepted only when the final retained packet really enters the existing packet FIFO. DMA > CPU > automatic-launch arbitration and async crossings are executable-tested. T6 deliberately does not claim compute completion or result correctness.
- The supported workload remains one 128-byte ReLU chunk. T7 must not claim a complete 1024-byte multi-chunk matrix workload.

## T7 objective

Complete the minimal Issue #4 prototype for one supported 128-byte chunk: produce known int32 Gemmini accumulator data, publish it into the reserved external SPAD without CPU payload copying or DRAM staging, pull it into CGRA SPM through the T5 automatic DMA, launch the real generated ReLU sequence only after matching T5 completion through the T6 gate, observe a matching real CGRA `CMD_COMPLETE`, move the CGRA result to an ordinary validation buffer only through the existing CGRA DMA MVOUT path, and compare all 32 int32 words against ReLU expectations.

T7 must distinguish three events: T5 payload completion, T6 LaunchAccepted, and actual CGRA compute completion. It must not call LaunchAccepted compute complete. The final output observer may be CPU software after a real CGRA DMA MVOUT into ordinary memory; CPU must not copy the input payload, write CGRA SPM, read CGRA SPM through a debug path, or stage the Gemmini intermediate through DRAM.

## Approved production control decision

The user approved a small production MMIO typed control block after the mandatory investigation. Place it at `0x60011000` and keep the existing validation-only telemetry page at `0x60010000` unchanged so T1-T6 validation ABI/tests do not move. Derive both pages from the authoritative generated external-SPAD contract and prove address-map uniqueness through fresh elaboration.

The production MMIO block may stage and enqueue only typed control information: one T5 pull descriptor, one T6 launch header, the declared bounded sequence of full-width launch packets, and typed result/error/completion events. A submission completes once bounded hardware storage accepts it; it must not wait for Gemmini publication, CGRA DMA, launch, or compute completion. Payload bytes never traverse this MMIO block.

Use single-active compute ownership to correlate the existing untagged real `CMD_COMPLETE`: retain the typed job identity before launch, enter the launched state only after the final automatic launch packet is really accepted by the existing packet FIFO, and attribute the first subsequent real `CMD_COMPLETE` to that sole active job. Reject or backpressure overlapping automatic/CPU launch operations that would make ownership ambiguous. This rule is sufficient for T7's one-job prototype; tagged concurrent compute completion remains future work.

## Mandatory read-only design investigation before implementation

T6 production currently directly consumes T5 completion, while its launch header/packet producer seam is intentionally tied off and its result/error outputs are sunk. T5 production descriptor submission is also not yet a user-facing runtime control path. Before adding a production ABI, inspect the existing custom0 raw/config/launch path, generated fast API, external-SPAD integration, RoCC response/wait behavior, `CMD_COMPLETE` handling, and the typed T5/T6 bundle bridges.

Report the narrow production control alternatives with concrete code evidence before implementation if choosing among them materially changes the ABI or topology. In particular compare: a small production MMIO submission/control block; a new semantic RoCC command sequence that captures typed descriptor/header/packet data; and reuse of an existing typed runtime/control seam if one already exists. Do not silently promote validation telemetry into a production ABI or add magic registers without an explicit typed contract.

The selected control path must let the CPU configure and enqueue the descriptor and launch sequence, then return once bounded hardware storage accepts them. It must not hold a RoCC/MMIO command until DMA or compute finishes. Hardware event causality, not CPU polling, must order publication, DMA, and launch. Final software synchronization may consume a real completion/result event or the existing matching CGRA wait mechanism, but production correctness cannot depend on repeatedly polling validation counters.

## Required typed runtime/control contract

- Submit one T5 pull descriptor carrying `jobId`, `slot`, exact requested bytes, `spmWordAddress`, and `dmaTag`.
- Submit one T6 launch header carrying the same identity plus `packetCount`, followed by exactly that many full-width legal generated `CMD_LAUNCH` packets.
- Preserve all launch packet bits and destination-tile fields; do not interpret or alter per-tile FU operations, routing, or control-memory configuration.
- Return or expose typed LaunchAccepted, launch protocol error, and final compute completion/result events with enough retained identity to reject stale, wrong, duplicate, or reordered events.
- If existing `CMD_COMPLETE` lacks job identity, correlate it only through a sound single-active-operation ownership rule established before launch, and document/assert that rule. Do not infer completion from CGRA busy, elapsed cycles, accepted packet count, or CPU wait retirement.
- Keep producer/consumer/launch/compute status domains distinct.

## Required event ordering and ownership

1. CPU may configure CGRA and enqueue the typed pull and launch sequence before Gemmini publication completes.
2. The producer remains strictly serialized and publishes raw int32 rows only into its reserved slot.
3. No CGRA DMA begins before matching producer READY; no launch packet reaches the shared packet FIFO before matching successful T5 completion.
4. A real LaunchAccepted occurs only after all retained packets are accepted by the existing packet path.
5. Compute completion occurs only on the matching real `CMD_COMPLETE` observed after LaunchAccepted under the single-active-operation rule.
6. Output DMA MVOUT may begin only after compute completion and must use the existing semantic six-packet DMA path and matching `CMD_DMA_DONE`/DMA_WAIT behavior.
7. Slot RELEASE remains the T5 DMA-input completion event; T7 compute does not hold or retroactively reclaim the producer slot.
8. CPU raw/config traffic and CPU/automatic DMA ownership behavior from T5/T6 must remain compatible and deterministic.
9. All Decoupled payloads and software-visible queued submissions/results must be stable and exactly once under backpressure.

## Integration boundaries

- Prefer forked Chipyard wrappers/adapters and root-owned runtime/tests. Do not modify Gemmini systolic-array/accumulator RTL or VectorCGRA tile/FU/SPM RTL unless executable evidence proves a required event is unavailable and that evidence is reported first.
- Reuse the T2 external SPAD, T3 endpoint, T4 final-D producer event, T5 real DMA path, T6 launch buffer/arbiter, existing generated ReLU packets, existing CGRA expected-completion machinery, and existing DMA MVOUT observer.
- Do not add a second payload DMA, shadow buffer, CPU memcpy, DRAM intermediate, SPM peek, fake completion, timer, busy polling, kernel-specific production hardware packet constants, multi-chunk scheduler, interrupt subsystem, or dynamic slot allocator.
- Validation-only telemetry may prove causality, but the implemented production control path and event ordering must not depend on it.
- Do not update README or AGENTS for routine T7 details unless support or durable workflow boundaries genuinely change.

## Required executable validation

1. Focused tests for any new runtime/control adapter, completion correlator, queue, or crossing, including input/output backpressure, exact identity, duplicate/wrong events, producer/consumer/launch/compute failures, and reuse for a second job.
2. A fresh full-system validation that uses real Gemmini accumulator values containing negative, zero, and positive int32 data; publishes exactly 128 bytes; automatically pulls to a legal CGRA SPM range; launches the real generated ReLU sequence through T6; waits for one real `CMD_COMPLETE`; performs real CGRA DMA MVOUT; and verifies all 32 output words.
3. Prove with real hardware events that launch does not occur before T5 completion and output DMA does not occur before compute completion. Do not use validation counter polling as the ordering mechanism being claimed.
4. Exercise at least one failure or mismatch path end to end enough to prove no false compute launch/output completion.
5. Rerun T6/T5/T4/T3 focused suites, T6/T5 full real-path tests, relevant T1/T2 regressions, existing production `relu_dma.c`, old CPU raw/config/launch `relu.c`, and relevant VectorCGRA ReLU from-YAML reference as proportional to the changed layers.
6. Regenerate the intended single-CGRA design before fresh rebuilds. Every changed production/validation elaboration branch requires a current-source rebuild; stale simulators are not evidence.
7. Compile changed C with `-Wall -Wextra`, run root clang-format checks, runner shell syntax checks, and `git diff --check` in root, Chipyard, Gemmini, VectorCGRA, and nested gemmini-rocc-tests.

## Decision and review requirements

If there is no sound existing way to correlate `CMD_COMPLETE` with the typed job without changing the runtime ABI, or if production submission requires a materially different control topology, stop after the mandatory investigation and discuss the concrete alternatives before implementation. Do not choose a broad ABI by convenience.

The independent reviewer must audit the production submission ABI, bounded nonblocking acceptance, exact typed correlation, no CPU payload path, final-D/real-Get/real-DMA-DONE/real-launch/real-CMD_COMPLETE causality, output DMA ordering, single-active compute ownership, CPU traffic compatibility, crossing/reset, backpressure/exactly once, failure paths, configuration scope, old-test compatibility, generated-file policy, dirty-tree preservation, and fresh validation provenance.

## Commit and handoff

After implementation, fresh validation, independent review, and fixes, commit Chipyard first and then the root pointer/prompt/runtime/tests/runner changes. Use short messages, configured identity, and no AI attribution. Exclude generated/build collateral and unrelated dirty files. Do not push.

The T7 handoff must list the exact production control ABI, typed events, event ordering, compute-completion correlation, output observer path, files, tests, review findings/resolutions, commits, preserved dirt, and the precise remaining limitation that only one 128-byte chunk is validated.
