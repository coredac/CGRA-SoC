# Issue #4 T4: Gemmini Producer Adapter and Publication Completion Prompt

## Recovery rule

Before continuing this task after any interruption, context compaction, restart, or agent handoff, read this file completely, then read the T1, T2, and T3 prompts and inspect root, Chipyard, Gemmini, and nested gemmini-rocc-tests status/diffs. Confirm T1/T2/T3 commits and preserve all unrelated dirty/generated files. Do not rely on remembered context and do not overwrite this prompt after implementation begins.

## Repository and workflow

Work in `/home/jjqin/CGRA-SoC`. Obey root `AGENTS.md`, README support boundaries, CGRA-SoC Issue #4 and all seven discussion comments, and narrower repository conventions. Preserve unrelated work; never reset, clean, restore, or reformat it. Generated files must come from their generator and are not committed by default. A required Chipyard change must be validated and committed in Chipyard first, followed by the root submodule pointer and root-owned integration. Do not push or create a pull request.

One implementation subagent must complete T4, then a newly started independent reviewer must audit it against this prompt, Issue #4, T1/T2/T3 evidence, README/AGENTS, dirty-tree boundaries, and fresh executable validation. Resolve findings before committing. Do not commit before review. After review passes, create scoped local commits in ownership order; never push.

## Established architecture and committed baseline

Issue #4 uses consumer-pull payload movement and event-driven control. The CPU may configure and submit work, but it must not copy payload bytes or stage the intermediate result in DRAM. Gemmini int32 results remain in the private accumulator and are locally published into the production external SPAD. Payload and control remain separate.

Committed evidence and interfaces:

- T1: Chipyard `6ca6b381`, root `7535751`. Existing `gemmini_extended_mvout_spad(dst_row, 4, 0xA0000000, 16, rows)` publishes raw int32 accumulator rows unchanged. The external destination unit is 16 bytes, full-width rows are 64 bytes, destination stride is 4, and a full 16x16 result is 1024 bytes.
- T1 completion rule: StoreController/ROB completion and `completion_io.completed` are too early. Producer-safe completion is the final external `StreamWriter` TileLink D acknowledgement, or an exactly equivalent manager-side acknowledged publication count. `gemmini_fence()` was only a serialized software validation boundary and must not become endpoint busy polling.
- T2: Chipyard `9d8859fd`, root `db4f6e2`. The production external SPAD is one coherent 64 KiB manager at `0x60000000`; output slots are `0x6000f800`/row 3968 and `0x6000fc00`/row 4032, each 1024 bytes. The generated Scala/C contract owns these values. CGRA DMA can route reads to this manager.
- T3: Chipyard `3dbe5fae`, root `d9d03b8`. `SpmTransferEndpoint` owns `FREE -> PRODUCING -> READY -> READING -> FREE` and exposes typed Decoupled `requestIn/out`, `readyIn/out`, `readStartIn`, `releaseIn`, and `errorOut`. Its focused suite passes 7/7 tests after independent review.

The ReservationStation STORE_SPAD footprint still ignores full-width destination stride 4. T4 must keep Gemmini publication strictly serialized and must not claim concurrent/overlapping Gemmini access safety.

## T4 objective

Implement and executable-test a thin Gemmini producer adapter in the forked Chipyard integration. Connect the T3 endpoint producer seam so a reserved request context is paired with one real Gemmini full-width publication into the requested T2 slot, and submit the matching `READY(job_id, slot, actual_bytes, status)` only after the last TileLink D acknowledgement belonging to that publication.

T4 stops at producer-safe READY. It must not launch CGRA DMA, assert `readStartIn`, consume `readyOut` as a real CGRA adapter, issue `RELEASE`, or launch CGRA compute.

## Producer command and correlation contract

The generic T3 REQUEST intentionally contains only job, slot, and maximum bytes; it does not contain Gemmini-private accumulator source coordinates. Do not silently hard-code accumulator source row zero or reinterpret generic fields as a Gemmini command.

First inspect the real Gemmini `spad_writer` TileLink path, diplomacy source-ID allocation, and wrapper hierarchy. Implement the narrowest sound producer-side contract:

1. The adapter consumes and retains one `requestOut` context only when no publication is active.
2. Slot and size are checked against `GemminiExternalSpadGenerated`; the destination must be exactly the requested reserved slot and the current full-width row layout uses stride 4.
3. The publication itself may be submitted by the CPU or an existing producer-local Gemmini command path, consistent with Issue #4 allowing CPU task submission. In that case the adapter must arm before the command and correlate the real `spad_writer` traffic by writer identity, destination range, byte count, and ordering. It must not treat unrelated ordinary external-SPAD writes as progress.
4. If a reusable producer-local launch input is necessary, define a typed producer-specific command carrying the missing accumulator source/row count rather than adding magic constants to REQUEST. Keep it separate from the generic endpoint protocol. Do not add a CPU payload path.
5. Do not modify Gemmini systolic-array or accumulator internals. A Gemmini-core change is allowed only if executable evidence proves that the final writer D acknowledgement cannot be observed soundly in the Chipyard integration; report that evidence before broadening scope.

If the current hierarchy cannot distinguish this publication from other traffic, or an exact command-to-D-ack association requires a materially different generic protocol, stop and report the concrete evidence and alternatives before changing the approved architecture.

## Required completion semantics

- Track only accepted external `spad_writer` writes belonging to the armed slot/publication.
- Count completion on TileLink D handshakes, never merely A acceptance, SRAM commit, StoreController response, ROB completion, Gemmini busy/fence, elapsed cycles, or CPU polling.
- A full result requires exactly 16 acknowledged 64-byte full-line writes for 1024 bytes. If variable row counts are supported, derive actual bytes from acknowledged full-width rows and require a whole number of 64-byte rows no larger than `max_bytes` and the 1024-byte slot.
- Hold the generated READY stable under backpressure and deliver it exactly once to T3 `readyIn`.
- Preserve job and slot identity from the armed request. Successful READY uses status 0 and the exact acknowledged byte count.
- Define typed, nonzero producer status values for malformed destination/order/size, an unsupported request, or observed TileLink denial/corruption where those signals are available. Failure READY uses a sound `actual_bytes` value and must not expose partially published data as successful.
- Do not allow a second publication request until the first publication has reached its final D ack and its READY has handshaken into the endpoint. This strict serialization is also the T4 mitigation for the known ReservationStation stride-footprint limitation.
- Add assertions for READY causality, one active job, exact acknowledged byte bounds, stable backpressured READY, no early READY, no count from unrelated writer traffic, and no duplicate completion.

## Integration boundary

- Instantiate/connect the T3 endpoint and producer adapter only in the combined CGRA + Gemmini configurations that own the T2 external SPAD. Other Gemmini/Chipyard configurations must remain unchanged.
- Keep validation-only command injection, D-channel stalling, counters, and MMIO controls visibly separate from production correctness. Production must not depend on CPU polling telemetry.
- Reuse the T2 physical SRAM and TileLink path; do not create a shadow buffer, DRAM alias, CPU memcpy, or private fake acknowledgement path.
- Do not modify VectorCGRA RTL/SPM or the existing CGRA DMA datapath in T4.
- Do not update README/AGENTS or Issue #4 for this intermediate task.

## Required executable validation

Use a focused Chisel test where possible and a fresh matching full-system validation for the real publication path. Source-only reasoning is insufficient.

At minimum validate:

1. Arm slot 0 with one job, run a real one-row/64-byte full-width publication, and prove READY is absent before its D acknowledgement and contains the correct job/slot/actual/status afterward.
2. Run a real full 16x16/1024-byte publication into slot 1 and prove READY occurs only after all 16 writer D handshakes. Verify all payload bytes through the real external-SPAD backing and preserve guard/non-overlap checks.
3. Stall the adapter-to-endpoint READY path for multiple cycles and prove payload stability and exactly-once delivery.
4. Insert D-channel backpressure in a validation-only configuration and prove the completion event waits for the final `d.fire`, not `d.valid`, A acceptance, or memory commit.
5. Interleave or inject unrelated ordinary external-SPAD writes and prove they do not advance the armed publication count. If diplomacy makes simultaneous injection impossible, add a focused lower-level executable test of the writer filter/source association and document the full-system serialization.
6. Reject or return producer failure for wrong slot range, unsupported/misaligned size, unexpected writer address/order, duplicate request, and denied/corrupt response where observable, without emitting a false success.
7. Rerun the T3 endpoint focused suite and the T1 64/128/1024-byte full-width regression after integration.

Any Scala/configuration/TL integration change requires a fresh matching `CGRAMinimalGemminiRocketConfig` or dedicated validation rebuild; do not use a stale simulator. Compile changed software with warnings enabled, check root-owned C/header formatting if changed, and run `git diff --check` in root, Chipyard, Gemmini, and nested gemmini-rocc-tests. Record exact commands and exit results.

## Review requirements

The independent reviewer must explicitly audit: exact final-D-ack causality; writer-identity and destination/byte-count correlation; no counting of unrelated writes; no StoreController/ROB/busy/timer completion; strict serialization and ReservationStation-risk containment; generated slot/stride use; READY identity, status, stability, and exactly-once behavior; endpoint state compatibility; production versus validation separation; absence of CPU/DRAM payload fallback; configuration scope; regressions; generated-file policy; dirty-tree preservation; and fresh simulator provenance.

## Commit and handoff

After implementation, fresh validation, independent review, and fixes, commit Chipyard first and then the root pointer/prompt/tests or runner changes. Use short descriptive messages, the configured author identity, and no AI attribution. Exclude build/test collateral and every unrelated dirty/generated file. Do not push.

The T4 handoff must list the exact producer command/correlation contract, writer source-identification mechanism, row/address/count semantics, failure statuses, files, tests, review findings/resolutions, commits, preserved dirty files, and the precise T5 consumer-adapter seam. Continue to T5 only after T4 is committed.
