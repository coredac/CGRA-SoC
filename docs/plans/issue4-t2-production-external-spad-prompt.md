# Issue #4 T2: Production Gemmini External-SPAD Address and Slot Contract Prompt

## Recovery rule

Before continuing this task after any process interruption, context compaction, restart, or agent handoff, read this file completely, then read `docs/plans/issue4-t1-external-spad-validation-prompt.md` and inspect the current root, Chipyard, Gemmini, and nested gemmini-rocc-tests status and diffs. Do not rely on remembered context. Preserve both prompts unchanged once implementation begins.

## Repository and workflow

Work in `/home/jjqin/CGRA-SoC`. Obey the root `AGENTS.md`, README support boundaries, and any narrower instructions discovered in scope. Preserve every unrelated dirty or untracked file. Never reset, clean, restore, or reformat unrelated work. Generated files must be changed through their generators and must not be committed by default. If a required change belongs to Chipyard, validate and commit Chipyard first, then update the CGRA-SoC submodule pointer and root-owned integration. Do not push or create a pull request.

This task must be implemented by one subagent and then reviewed by a newly started independent subagent. The reviewer must check the result against this prompt, Issue #4 and its discussion, T1 evidence, README/AGENTS constraints, existing user changes, and fresh validation evidence. Resolve findings before committing. Do not commit before review. After review passes, create the smallest coherent owning-repository commit first and then the root commit; never push.

## Issue #4 and approved architecture

Issue #4 requires a programmable direct accelerator-SPM transfer, initially Gemmini GEMM to CGRA ReLU, without CPU payload copying or intermediate DRAM staging. The CPU may configure and submit work but must not move the payload. The approved architecture is:

1. The consumer pulls payload data. The CGRA-side DMA will eventually read a producer-visible Gemmini external SPAD.
2. Gemmini int32 results remain in the private accumulator. Gemmini locally publishes full-width rows into reserved external-SPAD output slots; the accumulator is not exposed as a TileLink manager.
3. Payload and control are separate. Later tasks implement `REQUEST(job_id, slot, max_bytes)`, `READY(job_id, slot, actual_bytes, status)`, and `RELEASE(job_id, slot)`.
4. Control is loosely coupled and event driven. Later READY must derive from final publication acknowledgement, not CPU/endpoint busy polling.
5. Integration belongs primarily in the forked Chipyard wrapper/interconnect. Avoid Gemmini systolic-array, accumulator, or VectorCGRA compute/SPM changes.

Static ping-pong slots use ownership `FREE -> PRODUCING -> READY -> READING -> FREE`. This task defines the physical backing and layout only; it does not implement ownership transitions or the control protocol.

## Proven T1 facts that must be preserved

T1 is committed as Chipyard `6ca6b381` and root `7535751`. Fresh full rebuild and simulation passed 64-byte, 128-byte, and 1024-byte full-width publications. Existing Gemmini RTL works unchanged.

- Existing command: `gemmini_extended_mvout_spad(dst_row, 4, 0xA0000000, 16, rows)` for the current DIM=16, int8 element, int32 accumulator configuration.
- `0xA0000000` selects accumulator, non-accumulate, full-width row read.
- External-SPAD destination unit is one ordinary 16-byte Gemmini SPAD row. A 64-byte full-width accumulator row therefore requires destination stride 4.
- A complete 16x16 int32 result occupies 1024 contiguous bytes.
- Do not use Gemmini StoreController/ROB completion or `completion_io.completed` as publication READY. Store completion is earlier than external visibility. Consumer safety is reached only after the external StreamWriter's final TileLink D acknowledgement or an equivalent manager-side acknowledged-row count.
- `gemmini_fence()` was safe for serialized T1 because Gemmini global busy includes StreamWriter busy, but future endpoint logic must not busy-poll it.
- ReservationStation STORE_SPAD hazard tracking ignores the stride-4 footprint. T2 remains serialized and must not claim concurrency safety. Later ownership/protocol work must prevent overlap or add a localized regression/fix.
- T1's external-SPAD memory and telemetry are explicitly validation-only. Refactor/reuse proven logic where sound, but do not silently call validation telemetry a production endpoint.

## T2 objective

Turn the T1-only external-SPAD backing into a narrowly scoped production integration for `CGRAMinimalGemminiRocketConfig`, and define one authoritative physical address/layout contract for two reserved full-matrix output slots. The normal combined CGRA + Gemmini configuration must elaborate with Gemmini external-SPAD mode enabled and with the memory reachable by normal TileLink clients, including the existing CGRA DMA client. Other Gemmini configurations must remain unchanged.

Use this production address contract unless an actual diplomacy/address-map conflict is proven:

- external-SPAD base: `0x60000000`
- external-SPAD size: 64 KiB (`0x10000` bytes), matching the current Gemmini scratchpad capacity
- output slot count: 2
- output slot size: 1024 bytes each (one complete 16x16 int32 result)
- reserve the top 2 KiB of the external SPAD so ordinary Gemmini rows remain below the publication region
- slot 0 physical base: `0x6000f800`; Gemmini destination row: `0x0f80` (3968)
- slot 1 physical base: `0x6000fc00`; Gemmini destination row: `0x0fc0` (4032)

The implementation must derive/check physical addresses and Gemmini row addresses from shared parameters rather than scattering independent magic numbers. Enforce power-of-two/alignment/capacity/non-overlap invariants at elaboration or compile time as appropriate. Keep the whole 64 KiB backing addressable so Gemmini external-SPAD semantics remain general; only the top 2 KiB is reserved by the Issue #4 producer contract.

## Required implementation

1. Refactor or supersede the T1 validation-only memory/config naming so there is a production external-SPAD memory/integration used by `CGRAMinimalGemminiRocketConfig`. Preserve a dedicated validation configuration if needed for T1 telemetry and regression, but do not duplicate two divergent SRAM implementations.
2. Enable `use_shared_ext_mem` and `use_tl_ext_mem` only for the combined production configuration that needs this path. Preserve current Gemmini dataflow/capacities/bus width and do not alter unrelated Gemmini configs.
3. Present the external SPAD as a real TileLink manager at the contract address. Gemmini ordinary SPAD reads/writes and `spad_writer` publication must still reach the same physical SRAM. Normal system clients must be able to issue supported reads; the later CGRA DMA consumer must not require a private bypass or a DRAM alias.
4. Keep a physically coherent SRAM model with explicit arbitration/serialization and safe TileLink acknowledgement timing. Reuse the T1 width adaptation/buffering that passed firtool combinational-loop checks. Do not acknowledge writes before the model has accepted/committed the write.
5. Expose the address/slot layout to root-owned software/tests through a small intentional interface owned by CGRA-SoC, or generate it from a single source if a suitable existing generator exists. Do not commit Chisel-generated Gemmini headers or other build collateral. The interface must include base, size, slot count, slot size, physical slot addresses, Gemmini row addresses, and row stride 4.
6. Keep T1 telemetry optional and validation-only. Production correctness must not depend on CPU reading telemetry registers.

## Required validation

Use fresh current-source evidence. At minimum:

1. Compile/elaborate the Chipyard changes and run firtool/CheckCombLoops through a full rebuild of `CGRAMinimalGemminiRocketConfig`.
2. Run a focused executable publication test on the production configuration that publishes into both reserved slot bases and verifies full-width layout and guard/non-overlap behavior through the real TileLink-visible backing. It may use CPU reads only as a T2 observer; CPU payload copying/staging must not be presented as the final data path.
3. Preserve and rerun the dedicated T1 validation or an equivalent regression after any refactor, with 64/128/1024-byte checks.
4. Add focused compile/elaboration assertions or software static assertions for slot derivation, alignment, row units, and capacity.
5. Run warnings-enabled C compilation, root clang-format checks for changed C/headers, and `git diff --check` in root, Chipyard, Gemmini, and nested gemmini-rocc-tests.
6. Confirm from the elaborated topology or an executable reachability check that the existing CGRA DMA TileLink client can route reads to `0x60000000..0x6000ffff`. Do not implement the consumer protocol or claim an end-to-end consumer transfer in T2.

Do not use a stale simulator built before the current Scala/configuration sources. Record the exact rebuild and test commands and exit results.

## Out of scope

Do not implement REQUEST/READY/RELEASE registers or FSMs, slot ownership transitions, Gemmini publish triggering, final-D-ack READY generation, automatic CGRA DMA triggering, CGRA launch gating, interrupts, multiple outstanding jobs, dynamic allocation, the complete GEMM-to-ReLU demo, or future other-IP-to-Gemmini transfers. Do not fix ReservationStation concurrency unless T2's serialized validation itself fails because of it. Do not update README or AGENTS.md for this intermediate integration task. Do not update Issue #4 unless the user asks.

## Decision and review requirements

If the proposed address range conflicts with a real manager, the external memory cannot be reached by the CGRA DMA without changing the architecture, or production reuse of the T1 1R1W model is not sound, stop and report the exact evidence before broadening the design. Ordinary implementation details that preserve the approved architecture do not require user confirmation.

The independent reviewer must explicitly audit address-map uniqueness, slot arithmetic, Gemmini row units and stride, shared physical backing, TileLink routing to the CGRA DMA, write acknowledgement timing, absence of CPU/DRAM fallback in the claimed path, configuration scope, preservation of T1, generated-file policy, dirty-tree preservation, and fresh validation provenance.

## Handoff

The T2 report must state the exact production topology and address contract, files changed, tests and evidence, reviewer findings/resolutions, commits, unrelated changes preserved, and constraints passed to T3. Stop after T2 review/commit, then continue to the next saved task prompt under the user's standing authorization; do not push.
