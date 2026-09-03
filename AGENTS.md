# CGRA-SoC Repository Instructions

## Mandatory Style Rules

These rules are mandatory for all agent output and hand-written code in this repository:

- All agent-written prose must be concise and use plain language. This applies to chat messages, documentation, code comments, annotations, and GitHub Issue text.
- Keep implementations simple and direct. Do not add fallback paths, compatibility branches, speculative abstractions, or defensive checks that are not required by the current contract. Avoid large `if`/`else` chains; split responsibilities, use early returns, or use a small dispatch structure when that makes the logic clearer.
- Add only checks and tests that protect required behavior or reproduce a real regression. Do not add redundant checks, duplicate tests, or speculative edge-case tests. This rule does not permit skipping the validation required by the `Validation` section.
- Use concise, descriptive names. Do not use long chains of underscore-separated words for variables, functions, directories, or files when a shorter name remains clear in context.
- Preserve clear module boundaries. Keep each file and function focused, and do not collect unrelated functions in one file or build a single oversized function.
- Do not add unnecessary blank lines. Use whitespace only to separate meaningful logical sections and follow the repository formatter where one applies.
- Match the established style of the surrounding code before introducing a new pattern. Inspect nearby code and existing modules first; if the applicable convention or the simplest correct design is still unclear, ask the user before proceeding.

## Scope

This repository is the top-level integration and runtime workspace for VectorCGRA, OpenFPGA, Gemmini, and Chipyard. It owns system configuration, integration scripts, tests, documentation, and future runtime development.

The `VectorCGRA/`, `OpenFPGA/`, `chipyard/`, and `firesim/` submodules retain their own conventions. Default to changing CGRA-SoC. Change a submodule only when the task belongs to that repository or the integration cannot be fixed at the top level.

Preserve unrelated user changes in the working tree. Do not reset submodules or generated artifacts to clean the repository. For a required submodule change, commit and validate the owning repository first, then update its submodule pointer and any integration code in CGRA-SoC.

## Sources of Truth

The single-CGRA configuration is layered:

- `configs/arch/arch.yaml` owns CGRA structure and functional-unit choices.
- `configs/soc/cgra_soc.yaml` owns the SoC interface and memory settings.
- `configs/soc/autolink/gc.yaml` owns the two-IP CGRA and Gemmini SoC settings and automatic task.
- `configs/soc/autolink/gca.yaml` owns the three-IP CGRA, Gemmini, and AES SoC settings and automatic tasks.
- `configs/kernels/kernel_*_4x4.yaml` owns kernel metadata and execution counts.

Do not restore the deprecated mixed kernel schema or add fallback reads for its old fields. Keep multi-CGRA architecture and SoC settings in their matching files under `configs/arch/` and `configs/soc/`.

The main generation entry points are:

- `scripts/generate_single_cgra.py`
- `scripts/generate_multi_cgra.py`
- `scripts/cgra_fast_api.py`
- `scripts/generate_auto_links.py`
- `scripts/generate_cgra_link_control.py`
- `scripts/generate_gemmini_ext_spm.py`
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

`CGRAMinimalGemminiRocketConfig` combines CGRA and Gemmini. `CGRAMinimalGemminiAESRocketConfig` and `CGRAMinimalGemminiAESAutoLinkRocketConfig` also add AES. CGRA uses `custom0`, AES uses `custom1`, and Gemmini uses `custom3`. Keep the opcodes distinct.

The CPU-mediated Gemmini GEMM to CGRA ReLU demos remain supported. Manual and automatic configurations use the same Gemmini external SPM. `CGRAMinimalGemminiAutoLinkRocketConfig` adds one automatic 128-byte transfer from that SPM to CGRA local SPM. The CGRA DMA reads the Gemmini SPM through TileLink and the system bus; there is no shared staging buffer or intermediate DRAM copy.

The three-IP Manual and Automatic configurations validate one strict sequential pipeline: AES decrypts 256 bytes into the Gemmini shared external SPM, Gemmini runs GEMM with B preloaded by the CPU and publishes 128 bytes at the SPM tail, CGRA pulls the data into its local SPM and runs ReLU, and AES reads the CGRA read-only SPM window and encrypts 128 bytes to DRAM. Manual mode has the CPU start each IP in order. Automatic mode has the CPU preload B, capture the native Gemmini command sequence, and configure the CGRA and root AES jobs; AutoLink uses fixed `aes -> gemmini`, `gemmini -> cgra`, and `cgra -> aes` routes and returns Gemmini, CGRA, and AES destination results. The older Gemmini-to-CGRA and Gemmini-to-CGRA-to-AES demos remain supported.

AutoLink carries control only. TileLink carries payload data. Routes and copy tasks are fixed during elaboration; runtime programming is unsupported. Hybrid mode, overlap, multiple chunks, multiple kernels, and concurrent producers are unsupported.

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

`run-chipyard-cgra-gemmini-demo.sh` defaults to `CGRAMinimalGemminiAutoLinkRocketConfig` and `relu_spm_auto.c`. Validate the automatic Gemmini-to-CGRA path with `./run-chipyard-cgra-gemmini-demo.sh --rebuild`.

Validate the CPU-controlled three-IP path with `CONFIG=CGRAMinimalGemminiAESRocketConfig TEST_SRC=tests/cgra-gemmini/aes_gemm_relu_manual.c ./run-chipyard-cgra-gemmini-demo.sh --rebuild`.

Validate the automatic three-IP path with `CONFIG=CGRAMinimalGemminiAESAutoLinkRocketConfig TEST_SRC=tests/cgra-gemmini/aes_gemm_relu_auto.c ./run-chipyard-cgra-gemmini-demo.sh --rebuild`.

For documentation-only changes, check Markdown structure, links, and `git diff --check`; do not launch a simulator build.

## Interface Contracts

- The CGRA is a RoCC accelerator and receives raw packets through `custom0`.
- OpenFPGA is a TileLink MMIO peripheral, not a RoCC accelerator.
- AutoLink supplements TileLink with dependency, copy, and compute control messages. It does not carry payload data.
- The automatic Gemmini-to-CGRA path uses a CGRA TileLink DMA master to pull data from Gemmini's four-bank external SPM.
- The automatic Gemmini path captures CPU-issued native RoCC commands in a parameterized wrapper buffer and replays them after `requestCompute`; it does not synthesize a fixed Gemmini job or modify the Gemmini IP.
- The AES root job starts only after its output watch is armed and its destination and byte count match the watched range, then reports output only after output and completion writes drain.
- The downstream AES job starts streaming on `requestCopy`, reports copy completion only after all input is read, treats `requestCompute` as a continuation barrier, and reports compute completion only after output and completion writes drain.
- The system bus can read and write the Gemmini shared external SPM.
- `CgraLinkEndpoint` uses MMIO for AutoLink configuration and results. CGRA launch packet contents use RoCC.
- Single- and multi-CGRA systems share the raw CPU/RoCC packet interface.
- `tests/include/cgra_runtime.h` is the minimal direct packet-send layer.
- Supported multi-CGRA hot paths send their preencoded packets directly.
- Regenerate after switching between scalar and vector layouts; do not assume a fixed packet width.
