# CGRA-SoC Repository Instructions

## Scope

This repository is the top-level integration and runtime workspace for VectorCGRA, OpenFPGA, Gemmini, and Chipyard. It owns system configuration, integration scripts, tests, documentation, and future runtime development.

The `VectorCGRA/`, `OpenFPGA/`, `chipyard/`, and `firesim/` submodules retain their own conventions. Default to changing CGRA-SoC. Change a submodule only when the task belongs to that repository or the integration cannot be fixed at the top level.

Preserve unrelated user changes in the working tree. Do not reset submodules or generated artifacts to clean the repository. For a required submodule change, commit and validate the owning repository first, then update its submodule pointer and any integration code in CGRA-SoC.

## Sources of Truth

The single-CGRA configuration is layered:

- `configs/arch/arch.yaml` owns CGRA structure and functional-unit choices.
- `configs/soc/cgra_soc.yaml` owns the SoC interface and memory settings.
- `configs/kernels/kernel_*_4x4.yaml` owns kernel metadata and execution counts.

Do not restore the deprecated mixed kernel schema or add fallback reads for its old fields. Keep multi-CGRA architecture and SoC settings in their matching files under `configs/arch/` and `configs/soc/`.

The main generation entry points are:

- `scripts/generate_single_cgra.py`
- `scripts/generate_multi_cgra.py`
- `scripts/cgra_fast_api.py`
- `scripts/openfpga/generate.py`

Use the scripts' `--help` output and the current YAML schema instead of copying arguments from old plans or logs.

## Current Support

### CGRA

Single-CGRA fast APIs support FIR, ReLU, GEMV, Histogram, and AXPY. GEMM and SAD are unsupported. Track kernel bugs and validation limitations in GitHub Issues rather than recording them here.

Multi-CGRA tests support homogeneous mesh, 2x2 and 4x4 systolic, scalar FIR, and vector FIR configurations. Their packet headers are fixed, preencoded test inputs. Automatic multi-CGRA control-packet generation is unsupported; do not replace those headers with an ad hoc generator.

The generated fast API is local single-CGRA only. Do not silently extend it to multi-CGRA targets without first defining the packet-encoding contract.

### CGRA + FPGA

The current OpenFPGA integration is a TileLink MMIO fabric flow. It supports AND2, AND2/OR2, bin2bcd, and gcd6 demos with frame-based k4 fabrics. A Chipyard configuration enabling CGRA and OpenFPGA together is not currently supported. Do not describe the existing OpenFPGA-only configs as concurrent CGRA + FPGA systems.

### CGRA + Gemmini

`CGRAMinimalGemminiRocketConfig` combines CGRA and Gemmini. CGRA uses `custom0`; Gemmini uses `custom3`. Keep the opcodes distinct.

The CPU-mediated Gemmini GEMM to CGRA ReLU demo is supported. The DMA test is a chunk-0 smoke test, not a complete multi-chunk workload. Do not assume a direct Gemmini-buffer-to-CGRA-memory connection.

## Generated and Frozen Files

Generated outputs must be changed through their generators, not edited as a lasting fix. Frozen packet artifacts must also remain untouched unless a task explicitly asks to replace them. These files include:

- `tests/generated/`
- `tests/include/cgra_layout.h`
- generated RTL under VectorCGRA and Chipyard resources
- generated Chipyard Scala parameter and OpenFPGA metadata files
- precomputed `*_packets.h` test headers

The active generated RTL and layout describe whichever single- or multi-CGRA generator ran most recently. Regenerate the intended design before rebuilding or testing. `.clang-format-ignore` is the authoritative list of generated C files excluded from formatting.

Do not commit generated files by default, including outputs from scripts, PyMTL, Chisel, and directories such as `generated/`. Commit a generated output only when it is required for the repository to build or run, is an intentional versioned interface, or the user explicitly requests it. When it must be committed, include the source or generator change when applicable and keep the reason clear in the handoff.

## Editing and Formatting

- Format root-owned C and headers with the root `.clang-format` (LLVM, LF).
- Format Python under `scripts/` with Black using `pyproject.toml`.
- Do not reformat submodules or generated files.
- Keep shell runners simple; do not add wrapper layers for commands already expressed directly in the README.
- Keep README flow names as `CGRA`, `CGRA + FPGA`, and `CGRA + Gemmini`.
- Keep README user-facing and concise. Put implementation contracts here rather than expanding the README.
- Avoid manually wrapping code, configuration, comments, or commands solely to satisfy a column width. Keep the current formatter configuration authoritative and otherwise wrap only when syntax or readability requires it.
- Keep each natural-language Markdown paragraph and list item on one physical line. Do not hard-wrap README.md, AGENTS.md, or GitHub Issue bodies to a fixed column width.

## GitHub Workflow

- Create all issues in CGRA-SoC, including issues whose implementation belongs to VectorCGRA or cgra-chipyard.
- Do not create issues in the forked submodule repositories unless the user explicitly requests it.
- Record the affected module in the Issue body. Do not add module prefixes such as `[VectorCGRA]` to Issue titles.
- Use `Summary`, `Module`, `Current`, `Expected`, and `Acceptance Criteria` sections for Issue bodies. Do not impose an arbitrary content-length limit or hard-wrap prose.
- Keep bugs, known limitations, TODOs, and planned work in GitHub Issues rather than README or AGENTS.md.
- Use one CGRA-SoC issue to track cross-repository work instead of duplicating the issue across repositories.
- Do not invent Issue numbers or links before GitHub access is available.

Direct commits are currently allowed. Do not create a pull request unless the user asks for one or the repository policy changes. If pull requests are introduced, merge the owning submodule change first and then update the CGRA-SoC submodule pointer through its integration pull request.

Keep commits scoped and commit messages short and descriptive. Use the author's configured Git identity. Do not add Codex, Claude Code, another AI tool, or an AI-generated `Co-authored-by` trailer to the commit author, message, or body.

Update AGENTS.md in the same change only when repository ownership, workflow, support boundaries, generated-file policy, or another durable instruction has actually changed. Do not update it for routine implementation details.

## Validation

Use validation proportional to the change:

1. For a CGRA kernel change, check the matching VectorCGRA from-YAML reference test first.
2. Regenerate the intended RTL and API when their sources change.
3. Run the matching top-level runner.

The top-level runners are:

- `run-chipyard-cgra-test.sh`
- `run-chipyard-openfpga-demo.sh`
- `run-chipyard-cgra-gemmini-demo.sh`

Use `--rebuild` after generated RTL, Scala integration, the RoCC wrapper, or the Chipyard configuration changes. If only C test code changes and the matching simulator is current, a rebuild is unnecessary. The first Gemmini run after a configuration change requires a rebuild so elaboration regenerates matching Gemmini parameters.

For documentation-only changes, check Markdown structure, links, and `git diff --check`; do not launch a simulator build.

## Interface Contracts

- The CGRA is a RoCC accelerator and receives raw packets through `custom0`.
- OpenFPGA is a TileLink MMIO peripheral, not a RoCC accelerator.
- Single- and multi-CGRA systems share the raw CPU/RoCC packet interface.
- `tests/include/cgra_runtime.h` is the minimal direct packet-send layer.
- Supported multi-CGRA hot paths send their preencoded packets directly.
- Regenerate after switching between scalar and vector layouts; do not assume a fixed packet width.
