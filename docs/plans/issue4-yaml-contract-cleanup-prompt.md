# Issue #4 YAML Contract Cleanup Prompt

## Recovery rule

After any interruption or context compaction, reread this file, the root `AGENTS.md`, and the current repository status before continuing. Preserve all unrelated and generated dirt. Do not commit or push.

## Goal

Reduce `configs/soc/cgra_soc.yaml` to source-of-truth inputs rather than derived Gemmini external-SPAD values.

Keep under `memory.gemmini_external_spad`:

- `base_address`
- `size_bytes` (temporary until Gemmini sizing is moved to YAML in future work)
- `output_slots.count`
- `output_slots.size_bytes`

Remove from YAML and derive in `scripts/generate_gemmini_external_spad.py`:

- `validation_telemetry_address = base_address + size_bytes`
- `production_control_address = validation_telemetry_address + 4096`
- `control_page_size_bytes = 4096`
- `spad_row_bytes = 16` from the current fixed Gemmini integration contract
- `full_width_row_stride = 4` from the current fixed 16x16 int8/input and int32/accumulator Gemmini integration contract

Keep the generated Scala and C ABI byte-for-byte unchanged if possible. The Scala elaboration checks against the actual Gemmini configuration remain authoritative and must continue rejecting a mismatch.

## Scope

- Modify the root YAML, generator, and generator unit tests only as needed.
- Regenerate intentional versioned Scala/C interfaces through the generator; do not hand-edit them.
- Do not address the separate holistic-review findings about launch sequencing or producer arming.
- Do not change README/AGENTS or Gemmini/VectorCGRA sources.
- Do not commit or push.

## Validation

- Generator unit tests cover derived addresses/constants and prove legacy derived YAML keys are not required.
- Generator `--check` passes.
- Generated interface contents remain unchanged; if they change, explain why and run proportional hardware validation.
- Black check for changed Python, YAML parse, and root/Chipyard/Gemmini/VectorCGRA/nested-test `git diff --check` pass.
- A fresh independent reviewer verifies scope, derivation correctness, generated-file policy, and dirty-tree preservation.
