# Repository Guidelines

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
- Follow this file’s guidance across the repo; place new cross‑platform code in `portable/` when feasible.
- Do not reformat unrelated files; avoid editing generated `build_*` outputs.
- When adding files, mirror existing naming and include patterns.
- When starting a new session, first check the README.md, planning docs (in the planning directory), and recent commits to understand where we're at in the project. You can also read the last hundred or so lines of the most recent couple of files in codex_sessions to pick up context from the most recent sessions.

## Sandbox & Approvals
- Escalation: Always request escalated execution when needed (e.g., writing outside workspace, network access, package installs, GUI commands, or when sandboxing blocks progress).
- Granting access: If escalation is required, ask the user to run `/approvals` to grant full access so the agent can run tests and necessary build steps.
