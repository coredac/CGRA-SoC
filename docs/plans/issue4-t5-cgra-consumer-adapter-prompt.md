# Issue #4 T5: CGRA Consumer-Pull DMA Adapter Prompt

## Recovery rule

Before continuing after any interruption, context compaction, restart, or handoff, read this file completely, then read the T4, T3, T2, and T1 prompts and inspect root, Chipyard, Gemmini, VectorCGRA, and nested gemmini-rocc-tests status/diffs. Confirm T1–T4 commits and preserve every unrelated dirty/generated file. Do not rely on remembered context and do not overwrite this prompt after implementation begins.

## Repository and workflow

Work in `/home/jjqin/CGRA-SoC`. Obey root `AGENTS.md`, README support boundaries, CGRA-SoC Issue #4 and all seven discussion comments, `docs/plans/cgra-dma-tilelink-refactor-prompt.md`, and narrower repository conventions. Preserve unrelated work; never reset, clean, restore, or reformat it. Generated files must be changed through their generator and are not committed by default. Commit an owning submodule before its parent pointer. Do not push or create a pull request.

One implementation subagent must complete T5, followed by a newly started independent reviewer. Resolve every review finding before scoped local commits. Do not commit before review and never push.

## Committed baseline and fixed architecture

- Consumer-pull payload movement; CPU may configure/submit but cannot copy payload or stage it through DRAM.
- T2 production external SPAD: 64 KiB at `0x60000000`; 1024-byte slot 0 at `0x6000f800`, slot 1 at `0x6000fc00`; generated Scala/C contract is authoritative.
- T3 endpoint: Chipyard `3dbe5fae`, root `d9d03b8`; typed event protocol and `FREE -> PRODUCING -> READY -> READING -> FREE` ownership.
- T4 producer: Chipyard `42bc95f4`, root `92125a2`; the endpoint is instantiated with the external-SPAD integration, and READY derives only from the dedicated Gemmini `spad_writer` final TileLink `d.fire`. Publication remains strictly serialized.
- Existing CGRA DMA is already a dedicated 128-bit TileLink client named `cgra-dma`. It accepts semantic six-packet MVIN/MVOUT sequences, emits physical 16-byte Get requests, writes read responses into CGRA SPM, and returns `CMD_DMA_DONE` with an 8-bit tag only after the CGRA-local DMA operation completes. It does not use Rocket `io.mem`.
- The generated CGRA exposes 128 SPM words of 32 bits, so only 512 bytes fit at once. The supported CGRA + Gemmini DMA demo remains a 128-byte chunk-0 smoke test; do not claim a complete 1024-byte multi-chunk workload in T5.

## T5 objective

Implement and executable-test a thin CGRA consumer adapter in forked Chipyard. It accepts one typed local pull descriptor, sends the generic endpoint REQUEST, waits for producer READY, automatically issues the existing CGRA DMA MVIN sequence from the selected external-SPAD slot into the selected CGRA SPM word range, reports `readStartIn` when that real DMA read begins, and reports `releaseIn` only after the matching real `CMD_DMA_DONE` completion.

T5 completes data movement and slot release but deliberately does not launch CGRA compute. T6 will gate compute launch on the consumer completion event.

## Typed consumer descriptor

Keep producer-independent endpoint fields unchanged. Define a separate typed CGRA-local descriptor, at minimum:

- `jobId` (32 bits), `slot` (32 bits), and exact requested `bytes` (32 bits), forwarded as endpoint REQUEST.
- `spmWordAddress`, using the generated CGRA DMA SPM address width.
- `dmaTag`, using the generated 8-bit tag width.

Do not encode these values in loose booleans or magic MMIO offsets inside production logic. Validate before endpoint reservation:

- nonzero job ID and a generated slot index;
- nonzero byte count aligned to both the 16-byte CGRA DMA beat and the 64-byte Gemmini full-width publication row;
- byte count no larger than one 1024-byte producer slot and no larger than the current 512-byte CGRA SPM capacity;
- `spmWordAddress + bytes / 4` within generated `spmWords`;
- exact slot base derived from `GemminiExternalSpadGenerated`, not software-provided physical addresses;
- tag fits generated width and cannot alias another active automatic transfer.

Malformed descriptors must produce a typed consumer error/completion without reserving a slot or issuing DMA. Keep producer READY status distinct from consumer protocol/DMA errors.

## Required event and DMA semantics

1. Accept at most one automatic pull descriptor and retain it under backpressure.
2. Submit endpoint `requestIn` exactly once. Do not issue CGRA DMA before matching `readyOut` has status 0 and a valid `actualBytes` equal to the exact requested length.
3. If producer READY status is nonzero, consume it, do not issue DMA, and drive the matching failed-job `releaseIn` only after T3 has delivered that READY. Return a typed consumer failure to the local requester.
4. On successful READY, issue the existing semantic CGRA DMA MVIN packet sequence with source address equal to the generated slot base, destination SPM word address from the descriptor, byte count equal to READY.actualBytes, and the retained tag.
5. Arbitrate with the existing CPU-issued CGRA DMA path. Preserve its one-command-at-a-time contract; an automatic pull must backpressure rather than assert or corrupt a CPU DMA command, and vice versa. Record ownership of the active DMA operation.
6. Drive endpoint `readStartIn` exactly once when the matching automatic DMA has actually started. Prefer the first real CGRA DMA read request handshake (`readReq.fire` or the equivalent first TileLink Get acceptance), not merely descriptor acceptance or packet construction.
7. Drive endpoint `releaseIn` exactly once only when a matching `CMD_DMA_DONE` tag for the automatic MVIN has been accepted. The completion must mean the final response was written into CGRA SPM. Do not use CPU `DMA_WAIT`, CGRA busy, a timer, packet enqueue completion, or a counted estimate as release.
8. Route CPU-owned `CMD_DMA_DONE` through the existing `dmaDoneValid`/`DMA_WAIT` behavior unchanged. Route adapter-owned completion to the consumer adapter without leaving a stale software completion that blocks later DMA.
9. Hold all Decoupled event payloads stable under backpressure. Maintain job/slot/tag identity and reject unexpected/mismatched/duplicate start or completion events without releasing the slot.
10. Publish a typed local `completionOut` carrying job, slot, actual bytes, tag, and consumer status. It must be exactly once and stable under backpressure. This is the T6 compute-launch seam; production control must consume the event rather than poll debug state.

## Cross-hierarchy and configuration requirements

- Connect the T4 endpoint consumer seam to the one CGRA accelerator only in the combined CGRA + Gemmini configurations that own the T2/T4 integration.
- Use a normal typed diplomacy/bundle bridge and an explicit safe clock crossing if the endpoint and CGRA tile are in different clock domains. Do not use illegal cross-module references or hide the interface with ad hoc global wiring. If existing Chipyard conventions prove a different mechanism is required, document it and keep the typed seam.
- It is acceptable to relocate the T3 endpoint into a shared control LazyModule if necessary, but there must remain exactly one owner of slot state and one T4 producer connection. Preserve all T3 assertions/tests.
- Use the existing VectorCGRA DMA engine and Chipyard semantic packet sequencer. Do not reimplement a second payload DMA, expose CGRA SPM as a new manager, or modify VectorCGRA RTL unless executable evidence proves the existing start/completion seam is unavailable; report that evidence before broadening scope.
- Validation-only MMIO injection/telemetry may submit the typed pull and observe completion, but production correctness cannot depend on CPU polling. Do not add a production busy-wait ABI.
- Do not launch CGRA compute, update README/AGENTS/Issue #4, implement multi-chunk scheduling, interrupts, or a dynamic slot allocator in T5.

## Required executable validation

Add focused Chisel tests for the consumer adapter and a fresh full-system real-path validation. At minimum prove:

1. Successful 64-byte and 128-byte pulls from both slots into distinct legal CGRA SPM word ranges.
2. No DMA issue before matching success READY; producer READY backpressure is stable and consumed exactly once.
3. `readStartIn` occurs only on the first real accepted automatic DMA read, not on descriptor/REQUEST/READY/packet enqueue.
4. `releaseIn` and local completion occur only after matching real `CMD_DMA_DONE`, never on intermediate 16-byte Get/D responses, DMA adapter idle transitions, or CPU wait behavior.
5. The bytes actually written into CGRA SPM equal the int32 data published in the selected external-SPAD slot. A later CPU-issued CGRA DMA MVOUT may be used only as a validation observer after automatic completion; it is not the payload path being claimed.
6. A stalled local completion remains stable/exactly once while the released slot can be reused only according to endpoint ownership.
7. Producer failure READY causes no DMA and follows the failed READY direct-release path.
8. Invalid slot, zero/misaligned/oversized length, 1024 bytes exceeding current CGRA SPM, SPM-range overflow, duplicate descriptor, wrong tag, wrong slot/job READY, and unexpected DMA completion do not falsely start/read/release.
9. An automatic pull and CPU DMA command arbitrate safely; both complete correctly in their respective ownership paths with no stale `dmaDoneValid`.
10. Rerun T3 and T4 focused suites, T4 final-D full-system test, T2 external-SPAD/CGRA reachability, and the existing `relu_dma.c` chunk-0 smoke regression as appropriate after integration.

Any Scala/config/TL/bundle-crossing change requires a fresh matching combined or dedicated validation simulator rebuild. Do not use stale simulator evidence. Compile changed C with `-Wall -Wextra`, run root clang-format checks, and run `git diff --check` in root, Chipyard, Gemmini, VectorCGRA, and nested tests. Record exact commands/results and simulator/source provenance.

## Review requirements

The independent reviewer must audit descriptor validation, exact generated slot/SPM arithmetic, producer-status handling, no early DMA, real read-start causality, exact matching `CMD_DMA_DONE` release, CPU/automatic DMA arbitration and ownership, no stale completion, endpoint state/order compatibility, cross-clock/cross-hierarchy correctness, completion backpressure/exactly-once behavior, absence of CPU/DRAM payload fallback and busy polling, no compute launch, configuration scope, regression provenance, generated-file policy, and preservation of all unrelated dirty files.

## Commit and handoff

After implementation, fresh validation, independent review, and fixes, commit each owning repository first and update parent pointers in order. Use short descriptive messages, configured author identity, and no AI attribution. Exclude all generated build/test collateral and unrelated dirty files. Do not push.

The T5 handoff must list the exact typed descriptor/completion contracts, bridge/crossing mechanism, DMA arbitration/ownership rules, precise readStart/release events, files, tests, review findings/resolutions, commits, preserved dirty files, the current 512-byte/128-byte-smoke limitation, and the exact T6 compute-launch gating seam.
