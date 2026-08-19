# Issue #4 T3: Event-Driven Transfer Endpoint and Slot Ownership Prompt

## Recovery rule

Before continuing this task after any interruption, context compaction, restart, or agent handoff, read this file completely, then read the T1 and T2 prompts and inspect root, Chipyard, Gemmini, and nested gemmini-rocc-tests status/diffs. Confirm T1/T2 commits and preserve all unrelated dirty/generated files. Do not rely on remembered context and do not overwrite this prompt after implementation begins.

## Repository and workflow

Work in `/home/jjqin/CGRA-SoC`. Obey root `AGENTS.md`, README boundaries, Issue #4 and its discussion, and narrower repository conventions. Preserve unrelated work; never reset, clean, restore, or reformat it. Generated files must come from their generator and are not committed by default. If Chipyard changes are required, validate and commit Chipyard first, then update the root pointer/integration. Do not push or create a pull request.

One implementation subagent must complete T3, then a newly started independent reviewer must audit it against this prompt, Issue #4, T1/T2 evidence, README/AGENTS, dirty-tree boundaries, and fresh tests. Resolve findings before committing. Do not commit before review. After review passes, create scoped local commits in ownership order; never push.

## Established architecture and evidence

Issue #4 requires direct programmable accelerator-SPM transfers with no CPU payload copying or intermediate DRAM staging. The approved architecture remains:

1. Payload movement is consumer-pull.
2. Gemmini int32 results remain in its private accumulator and are locally published into reserved external-SPAD slots.
3. Payload and control are separate. The reusable control vocabulary is `REQUEST(job_id, slot, max_bytes)`, `READY(job_id, slot, actual_bytes, status)`, and `RELEASE(job_id, slot)`.
4. Control is event driven and loosely coupled. No CPU or endpoint busy polling.
5. Integration belongs primarily in forked Chipyard wrappers/adapters; core-IP changes require proven necessity.

T1 proved existing `gemmini_extended_mvout_spad` full-width publication works with destination stride 4, and that StoreController/ROB completion is too early. T2, committed as Chipyard `9d8859fd` and root `db4f6e2`, established one production 64 KiB external SPAD at `0x60000000`, two 1024-byte output slots at `0x6000f800`/row 3968 and `0x6000fc00`/row 4032, a real CGRA-DMA-readable TileLink manager, stable backpressured read responses, and generated Scala/C layout interfaces. T2 remains serialized. The ReservationStation STORE_SPAD hazard footprint still ignores stride 4, so no task may claim overlapping/concurrent Gemmini publication safety without a later fix or strict ownership exclusion.

## T3 objective

Implement and executable-test a reusable, event-driven transfer endpoint core in the forked Chipyard integration. It owns the two static slots and enforces the protocol/state contract, but deliberately remains disconnected from Gemmini publication completion and CGRA DMA launch in T3.

T3 must provide the exact seam for later thin adapters:

- T4 will connect the producer side to the real Gemmini publication path and generate `READY` only from the final external writer D acknowledgement.
- T5 will connect the consumer side to CGRA DMA start/completion.
- T3 tests may drive abstract producer/consumer events through a test harness, but production logic must not contain fake completions, timers, busy polling, or a DRAM/CPU data path.

## Required endpoint interface

Use typed Chisel bundles and Decoupled/ready-valid event channels rather than magic register fields or loose booleans. Names may follow local style, but semantics must be explicit and documented.

Required flow:

1. `requestIn`: consumer submits `REQUEST(job_id, slot, max_bytes)`.
2. `requestOut`: after reserving the slot, endpoint forwards the same request to the producer adapter and holds it stable under backpressure.
3. `readyIn`: producer adapter submits `READY(job_id, slot, actual_bytes, status)` only after producer-safe completion. T3 treats this as an abstract event; it must not synthesize it from ROB/store completion or busy.
4. `readyOut`: endpoint forwards the stored READY event to the consumer and holds it stable under backpressure.
5. `readStartIn`: consumer adapter reports that the matching pull has actually started/been accepted; successful jobs then enter `READING`.
6. `releaseIn`: consumer adapter submits `RELEASE(job_id, slot)` only after the matching pull completes; the endpoint returns the slot to `FREE`.
7. `errorOut`: invalid commands/events produce a typed error event with job, slot, and reason. Errors must not silently mutate unrelated slot state or deadlock the endpoint.

The core must expose a read-only state/debug snapshot suitable for assertions and validation, but later control logic must consume events rather than poll this snapshot.

## Slot and protocol contract

Use the T2 generated parameters; do not duplicate slot count/size magic numbers. The current endpoint owns exactly two static slots. Use a parameterized implementation where practical, while elaboration requires must match the generated production contract.

Each slot follows:

`FREE -> PRODUCING -> READY -> READING -> FREE`

Required semantics:

- Accept a request only for a `FREE` slot, a nonzero job ID (reserve zero as invalid), and a nonzero `max_bytes` no larger than 1024 bytes and aligned to the current 16-byte CGRA DMA beat.
- Active job IDs must be unique across slots. A duplicate job request is rejected without changing either slot.
- Slot reservation and job metadata become authoritative when `requestIn` handshakes. `requestOut` may be backpressured; the endpoint must retain and stably forward the request exactly once.
- A READY event must match a reserved slot/job and may be accepted only after its request has been forwarded to the producer.
- For `status == 0`, `actual_bytes` must be nonzero, 16-byte aligned, and `<= max_bytes`. For nonzero producer status, allow `actual_bytes == 0`; preserve the producer status to the consumer.
- A valid READY transitions `PRODUCING -> READY` and is forwarded exactly once. Backpressure cannot change payload or state metadata.
- `readStartIn` must match job/slot, require a delivered successful READY, and transition `READY -> READING` exactly once.
- `releaseIn` must match job/slot. Successful transfers release only from `READING`. Failed READY events may be released directly from `READY` after the error READY was delivered, because no payload read is required.
- RELEASE clears all job/length/status metadata and returns the slot to `FREE`.
- Invalid slot/state/job/length, duplicate job, duplicate/out-of-order event, or mismatched release must be rejected with a defined error code and no unauthorized transition.
- A stalled `requestOut`, `readyOut`, or `errorOut` must obey Decoupled payload stability. Do not overwrite a pending error with a later error; either backpressure offending inputs or define a lossless queue.
- Events for the two independent slots may make progress without global busy waiting. Define deterministic arbitration if a shared output can present events from both slots.

## Status and error representation

Use explicit constants/enums in one authoritative Scala definition. At minimum distinguish invalid slot, invalid length/alignment, slot busy, duplicate job, job mismatch, state/order mismatch, and actual length exceeding the request. Producer `READY.status` is payload status and must not be conflated with endpoint protocol errors.

Use widths that are sufficient and stable for later integration:

- `job_id`: 32 bits
- `max_bytes` / `actual_bytes`: 32 bits
- `status`: 32 bits
- slot field: sufficient for generated slot count, with explicit validity checking for non-power-of-two future values

Do not add a dynamic allocator, multiple outstanding jobs per slot, interrupts, or software ABI/MMIO register assignments in this task.

## Required implementation boundaries

- Implement the reusable endpoint/core and a focused validation harness in forked Chipyard. Keep test-only injection/observation separate from the production interface.
- Do not connect to Gemmini StoreController/ROB completion, Gemmini busy, or a timer.
- Do not modify Gemmini RTL, VectorCGRA RTL/SPM, the existing CGRA DMA datapath, or the production external-SPAD memory data path in T3 unless an executable test proves a protocol-core integration necessity and it is reported first.
- Do not automatically issue `gemmini_extended_mvout_spad`, launch CGRA DMA, launch CGRA compute, or move payload bytes.
- Do not add CPU busy-wait control. A validation harness may inspect/poll test-only diagnostics solely to prove event behavior, but that is not a production synchronization mechanism.
- Do not change README/AGENTS or Issue #4 for this intermediate task.

## Required executable validation

Add focused tests that exercise real ready/valid backpressure and all state transitions. Prefer a Chisel unit/test harness if the current Chipyard test infrastructure supports it; otherwise add a dedicated validation configuration whose test-only controls are unmistakably separate from production. Source-only reasoning is insufficient.

At minimum validate:

1. Happy path for both slots, including distinct job IDs and variable valid lengths (for example 128 and 1024 bytes).
2. `requestOut` and `readyOut` held for multiple cycles under backpressure with stable payload and exactly-once delivery.
3. Independent slot progress: one stalled slot does not prevent a valid event for the other unless documented output arbitration requires a bounded turn.
4. Duplicate job, busy slot, invalid/zero/misaligned/oversized length, mismatched READY, READY-before-request-forward, duplicate READY/read-start/release, and mismatched release.
5. Producer error READY with `actual_bytes == 0`, forwarding status and direct release without entering `READING`.
6. Successful state sequence and metadata clearing on release.
7. A stalled `errorOut` does not lose or overwrite errors.
8. Assertions for legal state transitions, stable Decoupled outputs, job uniqueness, bounds, and no transition on rejected events.

Run relevant Scala compile/elaboration/test commands, warnings/format checks for any root-owned test code, and `git diff --check` in root, Chipyard, Gemmini, and nested tests. If a full SoC validation config or Scala integration changes, use a fresh matching rebuild; do not use a stale simulator. Record exact commands/results.

## Review requirements

The independent reviewer must explicitly audit protocol semantics, slot/job/length validation, exact state transitions, error losslessness, ready-valid stability, two-slot independence/arbitration, separation of producer status from protocol errors, absence of fake completion/busy polling/data movement, configuration scope, test sufficiency, generated-file policy, dirty-tree preservation, and fresh validation provenance.

If a reusable endpoint cannot be connected later without a materially different cross-hierarchy mechanism, or the proposed interface conflicts with the approved consumer-pull design, report the evidence before broadening scope. Ordinary Chisel/test-harness choices that preserve this contract do not require user confirmation.

## Commit and handoff

After implementation, fresh validation, independent review, and fixes, commit Chipyard first and root pointer/test/prompt second if root changes exist. Use short descriptive messages and configured author identity; no AI attribution. Do not include unrelated or generated build collateral. Do not push.

The T3 handoff must list the exact interface/state/error contract, files, tests, review findings/resolutions, commits, preserved dirty files, and the precise T4 producer-adapter seam. Continue to T4 under the user's standing authorization only after T3 is committed.
