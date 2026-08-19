# Issue #4 T1: Gemmini Full-Width Accumulator-to-External-SPAD Validation Prompt

## Recovery rule

Before continuing this task after any process interruption, context compaction, or restart, read this file completely, then inspect the current root, Chipyard, Gemmini, and nested gemmini-rocc-tests status and diffs. Do not rely on remembered context and do not overwrite this prompt with a later plan.

## Repository and workflow

Work in `/home/jjqin/CGRA-SoC` and obey the root `AGENTS.md` plus any narrower repository instructions discovered in scope. Preserve every unrelated dirty or untracked file. Never reset, clean, restore, or reformat unrelated work. Generated files must be produced through their generators and must not be committed by default. If a required change belongs to a submodule, validate and commit the owning repository first, then update its parent submodule pointer and root integration. Do not push or create a pull request.

This task must be implemented by one subagent and then reviewed by a newly started independent subagent. The reviewer must check the implementation against this prompt, CGRA-SoC Issue #4 and its discussion, README/AGENTS constraints, existing user changes, and the actual validation evidence. Resolve review findings before committing. One task may be committed after it passes review, but nothing may be pushed; the user will perform a final combined review.

## Issue #4 goal and agreed architecture

Issue #4 targets programmable direct transfers between accelerator-local memories, initially Gemmini GEMM to CGRA ReLU, without CPU payload copying or an intermediate DRAM buffer. CPU involvement is limited to initial configuration/submission and final synchronization.

The following five design decisions are approved and must not be reopened during T1:

1. Cross-IP payload movement uses consumer-pull. The CGRA consumer endpoint will eventually issue TileLink reads from a producer-visible Gemmini external SPAD and write the data into the CGRA local SPM using the existing CGRA DMA path.
2. Gemmini's int32 GEMM result correctly remains in its private accumulator. The accumulator will not be exposed as a general TileLink manager. Gemmini locally publishes a completed full-width result from the accumulator into a reserved output region of its external SPAD; this local publish is not a push into CGRA.
3. Payload and synchronization are separate. The planned reusable control protocol is `REQUEST(job_id, slot, max_bytes)`, `READY(job_id, slot, actual_bytes, status)`, and `RELEASE(job_id, slot)`. T1 does not implement this protocol.
4. Control must be loosely coupled and event driven. The eventual design must use a precise local publish-completion event and DMA completion rather than CPU or endpoint busy polling. T1 must identify the completion condition for the tested publish path.
5. The primary integration work belongs in the forked Chipyard wrappers/interconnect. Gemmini systolic-array and accumulator changes are forbidden for T1. VectorCGRA compute/SPM changes are out of scope. A small Gemmini StoreController/reservation-tracking correction is allowed only if the validation proves it necessary for correct full-width strided publication.

The planned buffer policy after T1 is static reserved slots, preferably ping-pong, with ownership `FREE -> PRODUCING -> READY -> READING -> FREE`. A full 16x16 int32 result occupies 1024 bytes, so two output slots occupy 2 KiB of the current 64 KiB Gemmini SPAD. T1 validates the data representation and path but does not implement slot ownership.

## T1 objective

Determine with executable evidence whether the existing Gemmini implementation can publish raw full-width int32 accumulator contents into TileLink external SPAD without modifying the systolic array or accumulator, and establish the exact command, address, stride, layout, and completion semantics needed by the later producer endpoint.

The expected candidate path is Gemmini `MVOUT_SPAD` / `gemmini_extended_mvout_spad`. For the current `DIM=16`, `elem_t=int8_t`, and `acc_t=int32_t`, one full-width accumulator row is 64 bytes while one ordinary scratchpad row is 16 bytes. The current hypothesis is that a full-width source requires destination stride 4 so adjacent accumulator rows occupy four external-SPAD rows each. This is a hypothesis to prove, not an assumption to encode without evidence.

## Required investigation

Read the actual Gemmini Controller, Scratchpad, StoreController, reservation-station hazard tracking, external-SPAD TileLink client wiring, command encoding, and existing `mvin_mvout_spad`/`matmul_spad` tests. Confirm from code how the full-width source bit is selected, how `dst_stride` is interpreted, what unit destination addresses use, how many bytes each row writes, and exactly when the command/completion signal means the final external-SPAD write is globally observable to another TileLink client.

Enable the existing Gemmini TileLink external-SPAD mode only in a dedicated validation configuration or otherwise narrowly scoped test configuration. Do not silently change every Gemmini configuration. Instantiate/connect the required external scratchpad memory using existing Gemmini/Radiance-style mechanisms where possible; do not invent a private testing bypass that cannot represent the target path.

## Required executable validation

Add a focused, reproducible validation that exercises the real Gemmini store-to-external-SPAD path. Validate at least:

1. A 128-byte small transfer with nontrivial signed int32 values.
2. One complete 16-element int32 accumulator row, proving the row byte layout.
3. A complete 16x16 int32 result, exactly 1024 bytes, with values chosen so truncation to int8 is detectable.
4. The destination layout, alignment, row progression, and untouched guard regions around the destination.
5. The completion boundary required before an independent TileLink reader may consume the bytes.

Where practical, make the test first create or load known full-width accumulator values, publish them with the candidate `MVOUT_SPAD` command, and compare all external-SPAD bytes/words against the expected int32 values. A test that checks only scaled/truncated `elem_t` output is insufficient. A source-code-only argument is insufficient unless the full SoC simulation is genuinely blocked by an external tool/environment failure; in that case preserve the strongest lower-level executable evidence and report the blocker exactly.

The test must detect incorrect `dst_stride`, truncation, gaps, overwrites, and premature completion. It must not use CPU memcpy, ordinary Gemmini `mvout` through DRAM, CGRA SPM peek, a fake success response, or a fallback data path as evidence for the target mechanism.

## Decision outcomes

T1 must end with one of these evidence-backed conclusions:

- Existing path works unchanged: record the exact command fields, source/destination address units, required stride, byte layout, and completion event. Do not modify Gemmini RTL merely for style.
- Existing path needs a localized correctness fix: modify only the proven StoreController/reservation/completion logic, add a regression that fails before the fix, and explain why this does not expose the accumulator or specialize Gemmini for CGRA.
- Existing path cannot meet the architecture: stop before T2, document the precise blocking behavior and alternatives, and ask the user before broadening scope to accumulator exposure or another producer-push design.

## Out of scope

Do not implement the external-SPAD production address map, ping-pong allocator, REQUEST/READY/RELEASE endpoint protocol, CGRA consumer DMA trigger, CGRA launch gate, full Gemmini-to-CGRA demo, future other-IP-to-Gemmini path, interrupt delivery, multiple outstanding jobs, or dynamic allocation. Do not modify README or AGENTS.md for routine T1 details. Do not create another GitHub issue or change Issue #4 unless the user requests it.

## Validation and acceptance

Run validation proportional to every changed layer. At minimum, run focused unit/regression tests for the command/store path, compile the validation software with warnings enabled, elaborate/rebuild the dedicated external-SPAD configuration if Scala/configuration changes, and execute the validation on the rebuilt simulator. If the combined `CGRAMinimalGemminiRocketConfig` or its integration changes, use `./run-chipyard-cgra-gemmini-demo.sh --rebuild` or a narrowly added equivalent that unquestionably rebuilds the matching configuration. Do not use an old simulator as evidence after configuration or Scala changes.

Run `git diff --check` in every changed repository and inspect final status/diffs to ensure unrelated changes were preserved and generated artifacts were not accidentally committed. The independent reviewer must explicitly audit full-width correctness, stride/address units, completion timing, use of the real external-SPAD path, absence of DRAM/CPU fallback, scope discipline, generated-file policy, and test sufficiency.

## Commit and handoff

After implementation, primary verification, independent review, and any review fixes, commit the smallest coherent change in the owning repository first. If a nested submodule is changed, commit there first and propagate pointers outward in order. Commit messages must be short and descriptive, use the configured author identity, and contain no AI attribution. Do not include unrelated dirty/generated files. Do not push.

The final T1 report must state the proven outcome; exact command, stride, address unit, byte layout, and completion semantics; files and repositories changed; every test command and result; review findings and resolutions; commits created; unrelated pre-existing changes left untouched; and remaining constraints that T2 must follow. Stop after T1 and wait for user approval before starting T2.
