# Repository Guidelines

<!-- 2025-11-08 Codex: Clarified planning backlog workflow (ADRs/issues) and priority usage. -->

This document is a concise contributor guide for Frontier’s C/C toolchain and test harness. Use it to navigate the repo, build locally, and submit focused changes that keep tests green.

## TBD Decisions (to revisit later)
- Windows/Linux build paths and toolchains (MSVC/CMake/GNU) — TBD.
- CI provider and required jobs (build, tests, sanitizers) — TBD.
- Coverage tooling and enforcement; target remains ~90% — TBD.
- Static analysis toolchain (`clang-tidy`, `scan-build`) and configs — TBD.
- Release checklist, tagging, and changelog flow (SemVer) — TBD.
- Performance benchmarks and budgets — TBD.
- Security review and threat model for web server features — TBD.
- Large-file policy (e.g., Git LFS) and automated `.root` sanitization — TBD.

## Project Structure & Module Organization
- `Common/headers`, `Common/source` — Core Frontier C sources and headers.
- `portable/` — Cross‑platform shims and runtime stubs used by tests/CLI.
- `frontier-cli/` — Standalone CLI (C) for UserTalk execution and DB ops.
- `tests/` — Cross‑platform C test suite and Makefile targets.
- `app_resources/`, `databases/`, `samples/` — Assets and sample data.
- `build_*` — Platform build scaffolding (Xcode, GNU, VC). Do not edit generated artifacts.

## Build, Test, and Development Commands
- Build CLI: `make -C frontier-cli` (multi‑arch `arm64,x86_64`).
- CLI info: `make -C frontier-cli test` (prints target/archs).
- Run all tests: `make -C tests test`.
- Sanitized tests: `SANITIZE=1 make -C tests` (ASan/UBSan).
- Clean tests: `make -C tests clean`.
- Example direct tests: `cd tests && ./db_format_tests` or `./runtime_tests`.

## Prerequisites & Platforms
- Toolchains: Xcode 26.0.1 (Build 17A400); Apple clang 17.0.0.
- OS support: macOS primary; Windows/Linux plans are TBD. Headless builds aim to be platform‑agnostic.

## Workflow & Versioning
- Branching: Work on feature branches and merge into `develop`; releases merge `develop` → `main`.
- Branch names: `feature/<scope-short-desc>`, `fix/<issue-or-scope>`, `refactor/...`, `test/...`, `docs/...`, `chore/...`.
- Versioning: SemVer for releases; during refactor maintain database version code only.

## Coverage & Analysis
- Coverage: No tooling yet; target ~90% once enabled. Aim for tests covering new/changed code now.
- Static analysis: Not configured; optional local runs via `scan-build` or `clang-tidy` are encouraged.

## Coding Style & Naming Conventions
- Language: C (tests `-std=c99`), CLI `-std=c17`.
- Indentation: 4 spaces, no tabs; 100‑column soft limit.
- Braces: K&R style (same line), consistent include order: system, project, local.
- Filenames: C sources `snake_case.c/h`; tests use `test_*.c` or `*_tests.c`.
- Warnings: Keep `-Wall -Wextra` clean; prefer small, focused functions.

## Testing Guidelines
- Add unit/integration tests under `tests/components/` or `tests/examples/`.
- Use the provided framework (`tests/framework/test_framework.h`).
- Run locally: `make -C tests test` and with sanitizers.
- Prefer deterministic tests and avoid filesystem/network writes unless required.

## Commit & Pull Request Guidelines
- Commits: Imperative mood; optional scope tags (e.g., `feat:`, `fix:`, `Headless:`). Example: `fix: correct 64‑bit header conversion`.
- PRs must include: purpose/impact, key files touched, test plan with command output, related docs (e.g., `planning/...`) and linked issues.
- Keep changes surgical; update docs/tests when modifying `Common/source` or `portable/`.
- Merging: Preserve individual commits (no squash). Prefer merging feature branches into `develop` with a merge commit; rebase only to resolve conflicts without rewriting intent.

## Security & Configuration Tips
- Do not commit secrets or proprietary databases; keep `databases/` to samples.
- Headless builds: prefer `FRONTIER_HEADLESS`/portable paths when possible.
- Multi‑arch: verify both `arm64` and `x86_64` where applicable (CLI builds both).
- Web server context: assume hostile inputs; avoid unsafe defaults, validate/sanitize request data, and restrict dangerous verbs in headless modes.

## Test Data Policy
- `.root` files used by the app: place under `databases/Guest Databases/...`.
- Test‑only `.root` files: place under an appropriate subdirectory in `tests/`.
- Sanitize for personal data/config before commit (sanitization will be performed by in‑app UserTalk tooling once modern builds run).

## Agent‑Specific Notes
- Follow this file’s guidance across the repo; place new cross-platform code in `portable/` when feasible.
- Do not reformat unrelated files; avoid editing generated `build_*` outputs.
- When adding files, mirror existing naming and include patterns.
- Always use built-in shell-based tools for debugging on-disk files and formats instead of writing your own code to parse the file. Only write code for this purpose if built-in shell-based utilities are unavailable, and only after asking the user first to install the tool(s) you need.
- Frontier logging is reliable in headless builds now—exhaust the existing `[headless]`/`[wp-plain]` instrumentation or add lightweight stderr logs before reaching for LLDB. Keep the tmux/LLDB workflow as a fallback, not the default.
- When new technical details surface (format quirks, runtime behavior, etc.), document them immediately in the most relevant markdown reference (e.g., `docs/database_architecture.md`) so future sessions can pick up the thread quickly.
- Before writing any dated comments or doc updates, double-check the system clock to avoid stale timestamps.
- During v6→v7 migrations only three external types may be copied verbatim: `binaryType`, `PICT`, and (when encountered) `CARD`. Every other non-scalar (menus, outlines, scripts, WPTexts, tables, etc.) must be fully decoded and reserialized so bugs are not masked by opaque blobs.
- Treat the kernel↔UserTalk bridge doc (`planning/phase3/carbon_migration/kernel_userTalk_bridge.md`) as canonical for token/search-path behavior; consult/update it whenever you learn new details about runtime dispatch or table hydration.
- Before archiving or mass-moving documents, verify each file’s status block. Only move docs whose `State` is `Completed`, `Done`, or `Deprecated`; if the status indicates ongoing work, leave it in place (or restore it) so in-flight plans stay editable.
- When touching the database migrator, remember the current implementation only updates the header; ensure follow-up work reserializes tables so migrated roots become fully 64-bit.
- Treat `system.macintosh.*` as UI-only AppleEvent glue. Headless/CLI builds should never depend on it; if you see headless code reaching into that hierarchy, stop and refactor the caller to use platform-neutral hooks.
- Paige is strictly a **conversion tool**: use it only to deserialize legacy WPText blobs and emit UTF‑8 RTF during migration. Do not add new runtime features, UI code, or editing paths that depend on Paige once data is in the v7 format.
- Whenever you hit inscrutable behavior in a legacy/third-party component (especially anything that used to depend on Carbon/QuickDraw), verify we aren’t still linking a test stub or placeholder implementation before debugging the symptoms. Document any such findings and prefer swapping in the real portable code paths over chasing UI-era bugs.
- When a logging/instrumentation approach keeps failing, escalate quickly to interactive debugging (LLDB via tmux, watchpoints, etc.) and state that plan explicitly so we stop burning tokens on a stuck instrumentation path.
- Time is not a constraint for this project; favor stability, long-term maintainability, and code longevity over speed. Work in 30-minute focused blocks without interrupting the user for granular updates; only pause for critical decisions that truly need user input.
- When starting a new session, ask the user if you should try to pick up from where the previous session ended. If the user says so, you can either do what the user asks, or propose the following: 1) check the `README.md`, planning docs (in the planning directory), 2) review recent commits to understand where we're at in the project, 3) read `planning/_CURRENT_STATUS.md` to pick up context from the most recent sessions.
- If the user or Codex PR Bot says follow-up fixes are already being handled, stop and confirm before making new changes—avoid duplicating or racing their work.
- In general, avoid creating or maintaining shims that potentially mask underlying issues unless specifically asked to do so by the user.
- Always run tests before pushing PRs to origin. If you encounter new or unexpected test failures, stop and ask the user what to do.
- Whenever making changes to code, make sure to summarize the change with a dated comment near the top of the file, attributed to Codex.
- Frontier’s database allocator (headers/trailers, variance fields, avail list merging) follows the Boundary Tag Method from Knuth’s *The Art of Computer Programming* (per Dave Winer); keep that lineage in mind when debugging allocation logic or documenting format quirks.
- Track all future ADR-style decisions and upcoming issues inside `planning/TODO_future_improvements.md`; do not recreate `planning/adr` or `planning/issues`.
- Use the shared priority ladder defined there: `P0` (must / do first), `P1` (pre-prod critical), `P2` (future/nice-to-have). Mirror those labels when filing GitHub issues or internal notes so urgency stays consistent across tools.
- After every turn, update `planning/_CURRENT_STATUS.md` with the latest status summary and explicit next steps so the next session can resume immediately.
- When migrating or saving non-scalar data (outlines, WPTexts, menus, scripts, tables, etc.) in v7+, strip cursor/window/font/UI metadata entirely; zero those fields and plan to store per-user preferences elsewhere so headless builds stay multi-user safe.
- Prefer evidence over inference when interpreting identifiers or historical formats. If a name looks familiar (e.g., “WS” or “Word”), confirm its meaning via code/docs/logs before pursuing a theory to avoid chasing unrelated artifacts.
- For modern BE64 paths, avoid format forks inside shared functions: fork legacy vs modern logic into separate functions/files so the modern code stays branch-free and clean; keep legacy-only code isolated.

## Sandbox & Approvals
- Escalation: Always request escalated execution when needed (e.g., writing outside workspace, network access, package installs, GUI commands, or when sandboxing blocks progress).
- Granting access: If escalation is required, ask the user to run `/approvals` to grant full access so the agent can run tests and necessary build steps.
- When writing PR descriptions, include three sections: summary of changes (what/why per file or area), a "so what" explaining the impact, and clear next steps if follow-up work is expected.
