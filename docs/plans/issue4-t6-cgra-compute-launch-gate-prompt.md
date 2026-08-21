# Issue #4 T6: Event-Driven CGRA Compute Launch Gate Prompt

## Recovery rule

Before continuing after any interruption, context compaction, restart, or handoff, read this file completely, then read the T5, T4, T3, T2, and T1 prompts and inspect root, Chipyard, Gemmini, VectorCGRA, and nested gemmini-rocc-tests status/diffs. Confirm the committed T1–T5 chain and preserve every unrelated dirty/generated file. Do not rely on remembered context and do not overwrite this prompt after implementation begins.

## Repository and workflow

Work in `/home/jjqin/CGRA-SoC`. Obey root `AGENTS.md`, README support boundaries, Issue #4 and all seven discussion comments, `docs/plans/cgra-dma-tilelink-refactor-prompt.md`, and narrower repository conventions. Preserve unrelated work; never reset, clean, restore, or reformat it. Generated files must be changed through their generator and are not committed by default. Commit an owning submodule before its parent pointer. Do not push or create a pull request.

One newly started implementation subagent must complete T6, followed by a newly started independent reviewer. Resolve every review finding before scoped local commits. Do not commit before review and never push.

## Committed baseline and fixed architecture

- Consumer-pull payload movement; CPU may configure and submit work but cannot copy payload bytes or stage the intermediate through DRAM.
- T2 production external SPAD: 64 KiB at `0x60000000`, two 1024-byte publication slots at `0x6000f800` and `0x6000fc00`.
- T3 endpoint owns `FREE -> PRODUCING -> READY -> READING -> FREE` with typed REQUEST/READY/readStart/RELEASE events.
- T4 producer READY is caused only by the dedicated Gemmini `spad_writer` final TileLink D handshake.
- T5 is committed as Chipyard `0f5fdaf7` and root `d13d651`. It accepts a typed consumer descriptor, waits for matching successful READY, reuses the existing semantic CGRA DMA MVIN sequencer, emits readStart only on the first real TileLink Get acceptance, emits RELEASE/completion only on matching real `CMD_DMA_DONE`, and preserves CPU/automatic DMA ownership.
- The user approved increasing the CGRA local SPM from two to eight 16-word banks. The physical and generated capacity is now 128 words/512 bytes; existing ReLU/FIR from-YAML tests, production/validation `relu_dma`, and T1–T5 regressions pass.
- A T5 transfer remains at most 512 bytes; the supported existing CGRA + Gemmini workload remains the 128-byte chunk-0 smoke. T6 must not claim a complete 1024-byte multi-chunk workload.

## T6 objective

Implement and executable-test a thin, event-driven compute-launch gate in forked Chipyard. It must retain one typed launch intent, correlate it with the matching T5 typed consumer completion, and permit exactly one real existing CGRA compute launch only after successful payload DMA completion. It must never launch on descriptor acceptance, producer READY, first DMA read, DMA idle/busy, elapsed time, CPU polling, or a failed/mismatched completion.

T6 proves control gating and real launch causality. It deliberately does not claim the complete Gemmini GEMM-to-CGRA ReLU data/result demo; T7 will validate the complete computation and output.

## Approved launch-sequence decision

The user approved a generic bounded launch-sequence gate rather than gating only one packet. Add an explicit `packetCount` and a launch-only packet stream/FIFO in the Chipyard CGRA wrapper immediately before the existing shared `packetFifo`. The proposed capacity is 16 packets, sufficient for the currently supported generated launch sequences (ReLU 10, FIR 11, Histogram 5, AXPY 13, GEMV 9) while remaining a parameterized finite contract.

- Buffer only legal `CMD_LAUNCH` packets. Reject any configuration, raw control, or other command presented on the automatic launch stream.
- Preserve every packet bit, including its existing destination-tile field. The gate must not interpret or modify per-tile FU operations, routing, or control-memory configuration; those remain configured earlier through the existing path.
- CPU configuration/raw packets keep their existing path. The new launch buffer drains through deterministic arbitration into the existing CGRA `packetFifo` only after matching successful T5 completion.
- Locate the storage/control in the Chipyard `CGRAAcceleratorImp` clock domain, not in Gemmini external SPAD and not inside VectorCGRA tile/core RTL. Cross typed sequence inputs and the T5 completion safely into that domain.
- A 16-entry by current approximately 189-bit packet buffer is about 3024 storage bits before metadata/control. Keep the implementation simple and synthesizable; do not add a new compute datapath.
- This is a typed hardware seam plus validation-only injection. It does not establish a new production software runtime ABI in T6.

## Required typed contract

Define a typed local launch intent rather than loose booleans or magic validation registers. At minimum retain:

- `jobId` (32 bits), `slot` (32 bits), exact `bytes` (32 bits), `spmWordAddress` using the generated CGRA DMA width, and `dmaTag` using the generated 8-bit width, matching the T5 descriptor/completion identity;
- the existing CGRA launch operation in a producer-independent form. Do not hard-code ReLU packet constants in generic production logic. Prefer retaining an already assembled legal launch packet or another typed representation that reuses the existing raw-packet/packet-FIFO path without duplicating compute logic;
- an exactly-once typed local launch result/error carrying the retained identity and a defined status, suitable for T7/runtime integration.

If the existing CGRA wrapper cannot retain and later inject a generic launch operation without a kernel-specific hardware constant or materially changing the CPU raw-packet ABI, stop after concrete source/executable evidence and present the narrow alternatives before broadening the design.

## Required semantics

1. Accept at most one launch intent and retain it stably under backpressure. Reject zero job, invalid slot, invalid/misaligned/oversized length, generated SPM overflow, and identity fields that cannot match a legal T5 completion.
2. Correlate all of `jobId`, `slot`, `actualBytes`, `spmWordAddress`, and `dmaTag`. A producer or consumer error completion must not launch compute.
3. T5 `completionOut` must be consumed through the production event connection, not validation telemetry or software polling. Preserve T5 completion backpressure and exactly-once behavior.
4. Allow CPU to configure the CGRA and submit/arm the launch operation before payload completion, but accepting that command must not hold the CPU in a busy-wait until DMA completion. Retain it in a bounded buffer and return control once safely captured.
5. Inject the retained launch through the existing CGRA packet path only after the matching successful T5 completion handshakes. Do not create a second compute engine or bypass the packet FIFO.
6. Define the precise launch event as the real acceptance of the retained launch operation by the existing CGRA packet path. A local result may report “launch accepted”; it must not claim compute complete unless it is tied to a matching real `CMD_COMPLETE` event, which is optional and should remain a separate typed event.
7. CPU-issued unrelated raw/config/control packets must retain existing behavior. Define deterministic arbitration between CPU packet traffic and the automatic launch; neither may overwrite or duplicate the other. Backpressure rather than assert/corrupt when the shared path is occupied.
8. Hold every Decoupled payload stable under backpressure. Reject wrong, duplicate, early, or stale completions and launch intents without false launch.
9. Preserve exactly one endpoint/consumer owner and the T5 RELEASE semantics. Slot release remains caused by DMA completion; compute launch occurs after release/completion and must not retroactively hold the producer slot.
10. Do not use CGRA busy polling, a timer, CPU notification loops, or MMIO telemetry for production correctness.

## Integration boundary

- Connect the T5 production `completionOut` seam only in the combined CGRA + Gemmini configurations that own the external SPAD and one CGRA.
- Use typed diplomacy/bundle bridges and explicit safe clock crossing where the gate and CGRA packet path use different clock domains. Do not use cross-module references or global hidden wiring.
- Reuse the existing CGRA raw packet assembly, packet FIFO, expected-completion machinery, and launch command semantics where sound. Preserve `custom0` for CGRA and `custom3` for Gemmini.
- Validation-only MMIO may inject a typed launch intent and observe counters/results, but production launch causality must not depend on MMIO.
- Do not modify Gemmini RTL, VectorCGRA compute/SPM RTL, the T2 memory datapath, or the T4/T5 payload/completion mechanisms unless executable evidence proves the required existing seam is unavailable and that evidence is reported first.
- Do not implement multi-chunk scheduling, interrupts, a dynamic slot allocator, a new software runtime ABI, README/AGENTS changes, or the complete end-to-end result check in T6.

## Required executable validation

Add focused Chisel tests and a fresh matching full-system validation. At minimum prove:

1. A matching successful 64-byte and 128-byte T5 completion permits exactly one retained launch intent for both slots/legal distinct SPM ranges.
2. No launch occurs before T5 completion, including after REQUEST, producer READY, readStart, intermediate DMA beats, or a stalled completion.
3. Failed producer/consumer completion, wrong job/slot/bytes/SPM/tag, duplicate completion, duplicate intent, and malformed intent produce typed failure/no false launch.
4. Launch intent and T5 completion can arrive in either order; the gate retains the first valid side and launches only after both matching sides are present.
5. Stall the real CGRA packet acceptance and local result output for multiple cycles; prove stable payload and exactly-once launch.
6. Interleave CPU config/raw packet traffic and prove deterministic arbitration with no lost, duplicated, or reordered automatic launch.
7. Full-system validation must configure an existing supported CGRA kernel, arm/retain its real launch operation before T5 completes, run a real Gemmini publication plus automatic CGRA DMA, and prove from real hardware events that `CMD_LAUNCH` is accepted only after matching T5 completion. T6 may stop after real launch acceptance or a matching `CMD_COMPLETE`; T7 owns full output correctness.
8. Rerun T5, T4, and T3 focused suites; T5 full real path; the existing `relu_dma.c`; and relevant T1/T2 regressions as proportional to changed integration.

Any Scala/config/crossing/generated-interface change requires fresh production and/or dedicated validation rebuilds that exercise every elaboration branch changed. Regenerate the intended single-CGRA design before rebuilding. Compile changed C with `-Wall -Wextra`, run root clang-format checks, and run `git diff --check` in root, Chipyard, Gemmini, VectorCGRA, and nested tests. Do not use stale simulator evidence.

## Review requirements

The independent reviewer must audit launch-intent typing and validation; exact T5 completion identity/status correlation; absence of early launch; real existing packet-path acceptance; CPU/automatic packet arbitration; no CPU busy waiting; safe crossing/reset; backpressure/exactly once; failure and duplicate behavior; endpoint release ordering; production/validation separation; no kernel-specific production constant, CPU/DRAM payload fallback, new compute engine, or compute-before-data race; configuration scope; old test compatibility; generated-file policy; dirty-tree preservation; and fresh validation provenance.

## Commit and handoff

After implementation, fresh validation, independent review, and fixes, commit Chipyard first and then the root pointer/prompt/tests/runner changes. Use short messages, configured identity, and no AI attribution. Exclude generated/build collateral and unrelated dirty files. Do not push.

The T6 handoff must list the exact typed launch/result contracts, retained operation representation, matching and arbitration rules, precise real launch event, clock crossing, files, tests, findings/resolutions, commits, preserved dirt, and the exact T7 full-demo seam.
