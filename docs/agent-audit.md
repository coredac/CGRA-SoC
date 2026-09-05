# Agent workflow audit

Reviewed on 2026-09-05 against the [GPT-6 Astra model guide](https://developers.openai.com/api/docs/guides/latest-model). The changes apply its guidance on initiative, instruction conflicts, delegation, writing, and proportional verification.

## Scope and changes

The initial audit covered root AGENTS.md, repository skill locations, the GitHub Actions workflow and setup script, runner interfaces, and the nested VTR instruction entrypoint. Repository `.agents/` and `.codex/` were empty; there was no existing repository SKILL.md or model configuration to migrate. Before committing, a follow-up added Chipyard's legacy AGENT.md to the audit. Personal skills and other submodule instructions remain under their owners' control.

| Surface | Result |
| --- | --- |
| Root instructions | Removed the routine-design clarification gate; consolidated style, test, and GitHub rules; made independent delegation and shared build serialization explicit. |
| Hardware context | Moved configuration sources, support boundaries, and interface contracts to [contracts.md](contracts.md), loaded for relevant hardware tasks. Kept pending AES YAML changes separate from the adaptation commit. |
| Chipyard instructions | Migrated AGENT.md to the standard [AGENTS.md](../chipyard/AGENTS.md), linked shared mandatory rules, retained Chipyard-specific contracts, and replaced stale absolute paths and generator references. |
| Skill | Added [cgra-validate](../.agents/skills/cgra-validate/SKILL.md) for reference-test selection, runner selection, rebuild rules, and completion evidence. Documentation edits do not trigger it. |
| Skill discovery | Allowed versioned `.agents/skills/` while keeping local agent state ignored. Used the standard uppercase SKILL.md name and required metadata. |
| CI | Markdown-only push and PR changes run diff checks and skip the simulator job. Other files, initial pushes, and manual dispatch retain the hardware suite. Classification failure fails the existing simulator check before setup. |
| Existing work | Kept the original simulator commands in the adaptation commit and preserved pending YAML selections and rebuild flags in the working tree. The Chipyard pointer advances only for its instruction update; hardware edits and generated outputs remain outside this change. |

The initial rewrite reduced root AGENTS.md from 13,820 to 5,955 bytes (56.9%); hardware details and runner commands remain available on demand. A follow-up style review restored explicit mandatory wording for branch chains, naming, file/function scope, whitespace, and redundant tests, so the current file is larger than that initial revision. These sizes measure the root instruction footprint, not total task tokens or model speed. No model performance comparison was run.

## Design basis

The coding-style rules are repository requirements. The Astra guide supports explicit style instructions and proportional verification; it does not prescribe assertion policy, naming conventions, or formatters. The style review retained every coding constraint and clarified that the C++/Python `assert` prohibition is unconditional, while the Python `raise` restriction applies only to unnecessary statements. Branch simplification still depends on readability, and test economy does not waive required validation.

[Codex instruction discovery](https://learn.chatgpt.com/docs/agent-configuration/agents-md) supports scoped repository guidance. [Skill authoring](https://learn.chatgpt.com/docs/build-skills) specifies `.agents/skills`, clear activation descriptions, and loading details when needed. The new skill contains repository validation knowledge without adding a general planning or review loop.

CI uses a [job condition](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-jobs-with-conditions) so a documentation change completes the workflow with the hardware job skipped. Push comparisons use the before/head range; PR comparisons use the merge base. Disabling rename detection ensures moving source code to a Markdown filename still selects hardware tests.

## Validation

- Parsed workflow YAML, checked every run block with `bash -n`, and confirmed the original simulator steps were preserved.
- Executed the classifier in temporary Git histories for 13 cases: Markdown, skill instructions, C, YAML, workflow, skill script, whitespace failure, source renamed to Markdown, diverged PR base, code PR, manual dispatch, initial push, and invalid base.
- Validated skill metadata, reviewed its routing against documentation, C-only, kernel, and YAML-change tasks, and checked runner `--help`, Markdown structure, local links, Git ignore behavior, and `git diff --check`.
- No hosted Actions run or simulator build was launched; hardware execution commands were unchanged.
