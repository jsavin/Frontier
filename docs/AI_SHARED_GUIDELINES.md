# AI Shared Guidelines (Cross-Agent)

Last Updated: 2026-02-10

This file is the source of truth for cross-agent workflow and engineering policy shared by Codex, Claude Code, and related automation agents in this repository.

## Scope and Priority

- Use this document for repository-level engineering rules that apply regardless of agent tooling.
- Keep agent-runtime specifics in `AGENTS.md` (Codex) and `CLAUDE.md` (Claude Code).
- If a cross-agent policy in another file conflicts with this one, this file wins.

## Pre-Work Verification and Branch Discipline

Before non-trivial work:

```bash
pwd && git branch --show-current && git status --short
```

- Trivial work (single-line typo/docs fix) may proceed on `develop`.
- Non-trivial work (feature, bug fix, multi-file edits) must use a dedicated branch and worktree.
- Never push directly to `origin/develop`.
- If your runtime requires a branch naming prefix, apply it while preserving a feature/fix semantic name.

Recommended setup:

```bash
cd /Users/jake/dev/jsavin/Frontier
git worktree add ../Frontier-<feature-name> -b feature/<feature-name>
cd ../Frontier-<feature-name>
```

## Required Test and Validation Policy

Run relevant tests before opening or updating a PR. For runtime/verb/database work, prefer full validation:

```bash
./tools/run_headless_tests.sh
cd tests && make test-integration
cd tests && make test-all
SANITIZE=1 make -C tests
```

Minimum expectations:

- New or modified behavior is covered by tests.
- Verb behavior changes include integration tests in `tests/integration/test_cases/`.
- Tests should be deterministic and avoid unnecessary filesystem/network side effects.

## Logging and Diagnostics Policy

- Use Frontier structured logging for diagnostics (`log_trace`, `log_debug`, `log_info`, `log_warn`, `log_error`).
- Do not use ad hoc `fprintf(stderr, ...)` for diagnostic logging.
- User-facing terminal output may use `fputs` when appropriate.

## UserTalk and Integration Test Constraints

### UserTalk runtime facts

1. Strings use double quotes (`"text"`).
2. `typeof()` returns OSType codes (for example `'TEXT'`) and must not be changed to return descriptive strings.
3. File/database verb operations require absolute paths.

### UserTalk integration YAML constraints

- Do not place inline `//` comments inside UserTalk `{ ... }` blocks.
- Keep indentation consistent inside blocks; avoid stray blank lines with mismatched indentation.
- Isolate test state under `system.temp.*`, never by mutating `system.*` tables directly.
- Always clean up temporary test objects (for example `delete(@system.temp.someTestTable)`).

## Migration and Data-Safety Invariants

- During v6 -> v7 migrations, only `binaryType`, `PICT`, and `CARD` may be copied verbatim.
- All other non-scalars must be decoded and reserialized.
- For v7+ non-scalar saves/migrations (outlines, WPText, menus, scripts, tables, etc.), strip UI metadata (cursor/window/font/UI fields).

## Issue/PR Hygiene and Priority

- Sanitize PII before creating issues/PR descriptions: absolute local paths, usernames, hostnames, emails, tokens, secrets.
- Prefer repository-relative or `$PROJECT_ROOT/...` paths in public issue text.
- Use shared priority labels consistently:
  - `P0`: must do first / launch-blocking
  - `P1`: pre-production critical
  - `P2`: future / nice-to-have

## Knowledge Capture and Status Updates

When technical behavior or format details become clearer:

- Update the most relevant long-lived reference docs (for example `docs/database_architecture.md`, phase planning docs, or ADRs).
- Track future ADR-style decisions and upcoming issues in `planning/TODO_future_improvements.md`.
- Update `planning/_CURRENT_STATUS.md` with latest status and explicit next steps so future sessions can resume quickly.
