# Frontier Development Guide

## Shared Guidelines (Cross-Agent Source of Truth)

`docs/AI_SHARED_GUIDELINES.md` is authoritative for cross-agent policy: branch/worktree discipline, test requirements, logging policy, UserTalk test constraints, issue hygiene, shared priority usage. **Read it before non-trivial work.** If this file conflicts with it on a cross-agent rule, follow the shared guidelines.

## Technical Decision-Making Principles

**Default to proper, maintainable, long-term solutions.** Quick fixes accumulate as technical debt. Unless the user explicitly requests a quick fix for time constraints, recommend the approach that solves the problem correctly rather than suppressing symptoms.

---

## Quick Reference

Build/test/migration command reference: see `docs/QUICK_REFERENCE.md`. Key paths:
- Build CLI: `make -C frontier-cli` (or `make build` from repo root)
- Unit tests: `./tools/run_headless_tests.sh` (or `make unit`)
- Integration tests: `cd tests && make test-integration` (or `make integration`)
- ODB sync check: `./tools/verify_virgin_root_sync.sh --full` (or `make verify-odb-sync` from repo root, or `cd tests && make verify-odb-sync`)
- Migrate v6→v7: `./frontier-cli/frontier-cli --migrate <db>.root`
- PR monitoring: `./tools/monitor_pr_review.sh <PR_NUMBER>`

Test output dirs: `tests/tmp/{unit,integration,migration,results}/`. Scratch files: `/tmp` or `tests/tmp/` both work.

The unit and integration commands above are also declared in the `## /auto Test Manifest` section below — keep both in sync.

### Documentation Index

All docs live in `docs/`. Notable entry points:
- **Getting started / CLI**: `GETTING_STARTED.md`, `CLI_USAGE_GUIDE.md`
- **Implementation**: `VERB_IMPLEMENTATION_GUIDE.md`, `ODB_SCRIPT_EDITING.md`, `TESTING_GUIDE.md`, `LOGGING_STANDARDS.md`
- **Debugging / architecture**: `VERB_RESOLUTION_ARCHITECTURE.md`, `DEBUGGING_GUIDE.md`, `AGENT_DEBUGGING_GUIDE.md` (protocol-driven debugging playbook for AI agents), `ARCHITECTURAL_ANTIPATTERNS.md`
- **Workflow**: `DOIT_WORKFLOW.md`, `WORKTREE_WORKFLOW.md`
- **UserTalk reference**: `docs/usertalk/docserver/` (75+ verb categories)

---

## Communication Standards

### Documentation Audiences

This project has a deeply technical partner (Dave Winer). Apply global doc-writing defaults from `~/.claude/CLAUDE.md`. Project-specific:

- **Dave Winer** is a technical stakeholder — use the "technical stakeholders" audience level (architecture, integration approaches, trade-off rationale). He doesn't need code-level details but appreciates technical depth.
- **Planning docs** (`planning/`) target internal technical audience — full detail.
- **Phase references** are shared vocabulary across all audiences.

### Privacy & Entity References

- **NEVER mention specific people or entities** (partnerships, companies, individuals) unless the user explicitly asks
- This includes commit messages, PR descriptions, code comments, and documentation
- Keep communications focused on technical details, not partnerships or strategic relationships

---

## Permanent Branches (Never Merge to Develop)

- `archive/codex-sessions` — AI session transcripts (~950k lines)
- `archive/portable-refactoring` — Refactoring experiments (348 files, 98k lines)
- `archive/carbon-migration` — Legacy documentation (85 files)

Full details: `docs/PERMANENT_BRANCHES.md`.

---

## Strategic Roadmap & Project Context

Planning source of truth: `planning/INDEX.md`, `planning/phase_overview.md`, `planning/phase6/CRDT_FOUNDATION_ROADMAP.md`.

**Key context:**
- Frontier has "guest databases" — any databases opened that aren't system root
- Current "target" is generally a window (database or editor window)
- Legacy Frontier source: `/Users/jake/dev/tedchoward/Frontier`
- v7 database format should NOT contain font/style info (except within stored RTF objects)
- **Threading model**: GIL — real POSIX threads serialized by a single mutex. Only the GIL holder can access C globals. Yield points at `langbackgroundtask()` and `thread.sleepTicks()`. See ADR-014.

**PR Workflow** ⚠️:
1. Create feature branch, commit work
2. **BEFORE FIRST PUSH**: Run full test suite (unit + integration)
3. Push feature branch (NEVER push to origin/develop directly)
4. Use pull-request agent to create PR
5. Start `./tools/monitor_pr_review.sh <PR_NUMBER>`
6. **ALWAYS discuss bot feedback with user before addressing**
7. **NEVER merge PRs without explicit user approval**

---

## Working with Agents

Global agent discipline (always use sub-agents, parallelize, pass working dir) lives in `~/.claude/CLAUDE.md`. Frontier-specific agents:

| Agent | Use When |
|-------|----------|
| **system-architect** | C domain, runtime architecture, memory management |
| **usertalk-engineer** | UserTalk scripting, verb implementations in UserTalk domain |
| **odb-database-expert** | Database format, corruption issues, migration |
| **logging-expert** | Logging infrastructure, standards compliance |
| **frontier-sdet** | Test infrastructure, test strategy |

### Error Type Delegation

- UserTalk syntax errors → `usertalk-engineer`
- Integration test framework issues → `frontier-sdet`
- Architecture/design decisions → `system-architect`
- Test content verification → `frontier-sdet` or `usertalk-engineer`

Agents must verify fixes work end-to-end (full chain: name resolution → dispatch → implementation), not just one piece. Agent selection by phase + parallel patterns: `docs/DOIT_WORKFLOW.md`.

---

## /gate Configuration

Reviewer selection lives in `.claude/gate.yaml`. Schema: `~/.claude/skills/gate/gate-config-schema.md`. Authoring guide: `~/.claude/skills/gate/gate-config-guide.md`.

Current tier summary:
- `bar-raiser` — always
- `security` — always (binary parsing, network protocol, CLI input — broad sensitive surface)
- `concurrency` — auto (triggered by GIL, pthread, signal, langbackgroundtask paths/patterns)
- `swiftui` — excluded (pure C codebase, no Swift)

Threat model and trigger details are in `.claude/gate.yaml` itself.

After editing `.claude/gate.yaml`, validate with `/gate --validate` (or `python3 ~/.claude/skills/gate/validate.py`).

---

## /auto Test Manifest

Test commands and contract used by the `/auto` skill.

Status semantics for the entries below:
- `required` — must run and pass; blocks push and merge
- `skippable` — run if infra is available; otherwise note skip in PR description
- `manual-only` — do not run autonomously; list in PR for reviewer
- `none` — this layer does not exist in this project

See `~/.claude/skills/auto/SKILL.md` (a local Claude Code skill install — not in this repo) for the full table.

- **Unit**: `./tools/run_headless_tests.sh` (required)
- **Integration**: `cd tests && make test-integration` (required)
- **E2E / UI**: none (headless CLI project)
- **Smoke / Browser**: none
- **Lint**: none
- **Base branch**: `develop`
- **Merge strategy**: `--squash` (branch deletion is manual — see `/auto` skill Phase 6 for the exit-worktree-then-merge sequence; `--delete-branch` is incompatible with merging from inside a worktree)

Both required test layers must pass before push and before merge. Frontier has no E2E/UI/Lint layers — those entries exist for cross-project portability of `/auto`.

The unit and integration commands above are also listed in the Quick Reference section near the top of this file — keep both in sync.

---

## ODB Script Editing Rules

**Edit `databases/Virgin.root`** for changes that should persist in builds. `Virgin.root` is the source of truth — `make dist` copies it to `dist/Frontier.root`. Edits to `databases/Frontier.root` are local only and overwritten.

**Always use `--protocol` mode for ODB edits, never `-e`.** Protocol supports multi-step operations without shell escaping issues. Protocol mode opens the system root read-write by default (issue #127 restored the legacy default); use `--lock-opened-roots` for inspection-only sessions — in-memory mutations evaluate normally, but the on-exit save is suppressed.

**Always use `script.newScriptObject` / `op.newOutlineObject`** to install scripts — never raw `op.insert`. These verbs handle line ending normalization (LF/CRLF → CR).

**Quality gates for every script edit:**
1. Trim whitespace: `string.trimWhiteSpace(s)` before installing
2. Verify compilation: read back with `string()`, then call the verb
3. Write integration tests for new/modified verbs
4. Keep `.ut` files in sync with ODB changes
5. For kernel verbs: verify verb is registered in headless build before writing glue

Full protocol workflow, indentation rules, braces/semicolons in outline vs string format, complete verb-addition checklist: `docs/ODB_SCRIPT_EDITING.md`.

**Runtime ODB<->.ut sync (`--ut-sync-dir`).** When the live `.ut` mirror is active, edit/debug in protocol mode (ODB is authoritative; `.ut` is an outbound text projection refreshed on shutdown) rather than hand-editing `.ut` files. Agent workflow + caveats (compile-gated import rejects broken `.ut`, two-sided-edit conflict refusal via `.ut-sync-state`, `repl.syncscan()` reconcile): `docs/usertalk/UT_SYNC_WORKFLOW.md`.

---

## UserTalk Critical Facts

Cross-agent UserTalk invariants and integration-test parser constraints live in `docs/AI_SHARED_GUIDELINES.md`. Key gotchas:

- Double quotes for strings — UserTalk uses `"string"` not `'string'`
- typeof() returns OSType codes (e.g. `'TEXT'`) not descriptive strings
- Absolute paths required — UserTalk table paths must be fully qualified (e.g. `@workspace.foo`)

### UserTalk language primer (load before .ut or yaml-script work)

Before writing inline UserTalk in yaml integration tests, editing `.ut` files, or debugging UserTalk runtime behavior, load `docs/usertalk/CLAUDE_PRIMER.md` (~250 lines). It's the language tour with idioms and failure-mode decoder — what the docserver verb reference doesn't teach. Deeper docs in `docs/usertalk/` (records_and_tables, operators_and_idioms, strings_and_text, verb_invocation_patterns, testing_patterns, debugging_workflow) are linked from the primer and load on demand.

---

## C Coding Style

- **Language**: C — tests `-std=c99`, CLI `-std=c17`
- **Indentation**: **Tabs** for all C code. Frontier's outline editor translates plaintext↔outlines using tab-based indentation; spaces-indented C is hostile to that workflow. Enforced by `.editorconfig` + pre-commit hook (rejects new spaces-indented lines on staged C files). **Agents: do NOT use `--no-verify` to bypass.** A mass retab of pre-existing files is tracked separately.
- **Column width**: 100-col soft limit
- **Braces**: K&R (opening brace same line)
- **Include order**: system, then project, then local
- **Filenames**: new sources `snake_case.c/h`; legacy `CamelCase.c` / `dot.compound.c` retained as-is for history; tests `test_*.c` or `*_tests.c`
- **Warnings**: keep `-Wall -Wextra` clean; prefer small focused functions

---

## Critical Testing Constraints

Cross-agent test requirements (including integration test expectations for verb changes) live in `docs/AI_SHARED_GUIDELINES.md`. Frontier-specific patterns: `docs/TESTING_GUIDE.md`.

**Integration test file metadata**: New test files that open guest databases must set `needs_guest_dbs: true` at the YAML root. Tests with port conflicts or REPL dependencies must set `sequential: true`. See `docs/TESTING_GUIDE.md` "File-Level Metadata".

---

## Architectural Anti-Patterns

Quick reference (1-line summaries):
- **Name resolution vs verb dispatch**: don't add EFP searches to `langexternalgettable()` — only in `langhandlercall()`
- **Hash table lookup**: use `hashtablelookupnode()` when you only need the node
- **Global mutable state**: eliminate before launch; use thread-local or explicit context
- **Address values**: always use `setexemptaddressvalue()`, never modify handle memory directly
- **Mode stack**: use context guards, don't rely on push/pop being restored
- **Context guard completeness**: guards must save/restore ALL globals the guarded operation clears
- **Tmp stack ownership**: call `exemptfromtmpstack()` after storing heap values in persistent tables

Full anti-patterns + when-to-read triggers (hash table / verb resolution / migration / context guard / use-after-free / data-structure deep-dives): `docs/ARCHITECTURAL_ANTIPATTERNS.md`.

---

## Debugging & Investigation

- LLDB / git bisect / verb resolution / `loadfromhandle fail` false alarms / DB crashes / protocol-based UserTalk debugger: `docs/DEBUGGING_GUIDE.md`
- Verb lookup, `parentOf()` / `typeOf()` / `defined()` semantics, search order: `docs/VERB_RESOLUTION_ARCHITECTURE.md`

---

## Logging Standards

Cross-agent logging policy: `docs/AI_SHARED_GUIDELINES.md`. Implementation-level details: `docs/LOGGING_STANDARDS.md`.
