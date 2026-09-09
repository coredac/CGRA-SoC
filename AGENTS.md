# CGRA-SoC repository instructions

## Working approach

Complete the requested change through relevant validation. Resolve routine choices from nearby code and the current contract, and state assumptions that affect the result. Ask only when missing information changes scope, interfaces, or an irreversible action; continue independent work while awaiting an answer. Existing authorization remains valid.

User instructions take precedence over skill guidance. Load skills only for their stated task, and read linked material as needed. If a skill blocks authorized work, link the file and quote the blocking instruction. A skill does not authorize extra external actions.

Batch independent reads and checks. Use subagents for bounded, independent research, implementation, or review when parallel work materially helps; assign separate file ownership and integrate their results. Serialize generators and simulator builds that share RTL, headers, or build directories.

Keep plans proportional to the task. For long work, retain the goal, decisions, changed files, validation results, and next step so work can resume without repeating completed checks. Report what changed, what passed, and any remaining blocker in concise, plain language.

## Ownership and context

CGRA-SoC owns system configuration, integration scripts, tests, documentation, and runtime development. Change `VectorCGRA/`, `OpenFPGA/`, `chipyard/`, or `firesim/` only when the task belongs there or cannot be fixed at the top level; follow the owning repository's conventions.

Inspect the working diff before editing and preserve unrelated changes. Do not reset submodules or generated artifacts to clean the tree. Validate and commit required submodule changes in the owning repository before updating the top-level pointer.

Read [hardware contracts](docs/contracts.md) before changing configurations, generators, runtime interfaces, or hardware tests; read only the relevant sections. It owns configuration sources, supported systems, and interface constraints. Use current YAML and each script's `--help` to select arguments. Setup is in [docs/Setup.md](docs/Setup.md).

## Mandatory code and prose rules

These rules apply to all agent output and hand-written code in this repository.

- Keep implementations simple and direct. Do not add fallback paths, compatibility branches, speculative abstractions, or defensive checks that the current contract does not require.
- Avoid large `if`/`else` chains. Split responsibilities, use early returns, or use a small dispatch structure when that makes the logic clearer.
- Inspect nearby code and existing modules before introducing a new pattern, and match their established style. Resolve routine choices under the working approach above.
- Use short, descriptive names for variables, functions, directories, and files. Do not use long underscore-separated names when a shorter name remains clear.
- Keep module boundaries clear and each file and function focused. Do not collect unrelated functions in one file or build an oversized function.
- Do not add `assert` in C++ or Python. In Python, do not add unnecessary `raise` statements.
- Format changed root-owned C and headers with `.clang-format` (LLVM, LF), and Python under `scripts/` with Black using `pyproject.toml`. Do not reformat submodules, generated files, or unrelated code.
- Let formatters control code layout. Do not add unnecessary blank lines; use whitespace to separate logical sections. Do not manually wrap code, configuration, comments, or commands just to meet a column width.
- Keep prose concise and plain, including chat messages, documentation, comments, annotations, and GitHub Issue text. Keep each Markdown paragraph and list item on one physical line. Use lists and tables when they make the content easier to read.
- Keep shell runners direct; avoid wrappers around commands already documented in README.
- Keep README user-facing. Put implementation contracts in `docs/contracts.md`.

## Generated and frozen files

Fix generated outputs through their generators. This includes `tests/generated/`, `tests/include/cgra_layout.h`, generated RTL, Chipyard Scala parameters, and OpenFPGA metadata. Precomputed `*_packets.h` headers are frozen; replace them only when explicitly requested. `.clang-format-ignore` identifies generated C files excluded from formatting.

The active RTL and layout come from the last single- or multi-CGRA generation. Select and regenerate the intended design before hardware validation; switching scalar/vector layouts can change packet width.

Keep generated outputs uncommitted unless required to build or run, intentionally versioned as an interface, or explicitly requested. When committing one, include its source or generator change where applicable and explain why the output is versioned.

## Validation

Add checks and tests only to protect required behavior or reproduce a real regression. Do not add redundant checks, duplicate tests, or speculative edge-case tests. This does not waive the required validation below. Run the checks needed for the changed path; repeat or broaden them only after a relevant edit, failure, or unresolved concern.

| Change | Required validation |
| --- | --- |
| Documentation or instruction-only skill | Check Markdown structure, links, and `git diff --check`; no simulator build. |
| Workflow or tooling | Check syntax and exercise changed behavior; run a hardware path when its generation or execution behavior changes. |
| CGRA kernel | Run the matching VectorCGRA from-YAML reference test first, regenerate affected RTL/API, then run the matching top-level test. |
| Hardware integration or runtime | Regenerate affected outputs and run the matching top-level runner. |

For hardware work, use [cgra-validate](.agents/skills/cgra-validate/SKILL.md) for runner selection and rebuild rules. Required validation stays required when a skill is unavailable; report an unavailable tool or failed check explicitly.

## GitHub and maintenance

Track bugs, known limitations, TODOs, and planned work in CGRA-SoC Issues. Use one issue for cross-repository work; create submodule issues only if requested. Issue bodies use `Summary`, `Module`, `Current`, `Expected`, and `Acceptance Criteria`; name the module in the body, not as a title prefix. Use verified issue numbers and links.

Direct commits are allowed. Keep commits scoped, use short messages and the configured author identity, and omit AI attribution or AI-generated co-author trailers. Create a PR only when requested. For cross-repository PRs, merge the owning submodule change before the integration pointer update.

Commit validated Chipyard changes in scoped steps. CGRA-SoC does not need a matching commit for each Chipyard commit; defer its submodule pointer update until related root-owned changes or a small feature are ready to commit together.

Update these instructions and linked contracts only for durable changes in ownership, workflow, support, interfaces, or generated-file policy. Keep reusable repository skills in `.agents/skills/<name>/SKILL.md`; keep local agent state untracked.
