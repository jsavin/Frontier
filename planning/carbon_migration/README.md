# Carbon Dependency Retirement Plan

**Last updated:** 2025-10-29  \
**Maintainers:** @you (TPM), @assistant (implementation)

## Why this plan exists
Frontier’s headless/runtime work keeps tripping over pockets of Classic Mac OS / Carbon APIs (QuickDraw, AppleEvents, TEC, Moveable Handles, etc.). Incremental shims slowed us down. The new directive is simple: **remove all Carbon dependencies and replace them with modern, platform-neutral code**. This document is the authoritative plan for that migration.

## Objectives
- Eliminate every Carbon-era header, macro, and API from the core build (headless and desktop).
- Provide modern equivalents (or deliberate removals) for each feature so the runtime still functions.
- Keep the project buildable throughout the migration by landing work in small, reviewable slices.
- Document every major decision so we never revisit the same debate.

## Scope
In scope:
1. Core runtime (language engine, database, serializer, CLI/tooling).
2. Shared headers that currently define Carbon shims (`standard.h`, `langhash.c`, `strings.c`, `land.h`, etc.).
3. Tests and utilities that rely on classic APIs.

Out of scope (for now):
- UI re-hosting (we can stub UI entry points while we modernize backend code).
- Non-macOS platform quirks (unless they block Carbon removal directly).

## Guiding principles
- **No more bandaids.** Either refactor to a modern abstraction or remove the feature entirely.
- **One subsystem at a time.** Each milestone should target a coherent area (strings, AppleEvents, QuickDraw, etc.) and leave it Carbon-free.
- **Test before celebrate.** The portable/headless build (`make -C tests test_migration`, runtime smoke tests) must pass before we call a milestone done.
- **Document decisions.** Use `decision_log.md` for every notable call (e.g., “replace TEC with ICU-lite helper”).

## How we measure progress
- `inventory.md` tracks every known offending symbol/header. Rows move from "pending" → "in progress" → "removed".
- `phases.md` describes the execution order for subsystems and links to the commits/PRs that completed each phase.
- `_CURRENT_STATUS.md` (at `planning/_CURRENT_STATUS.md`) provides the high-level status we report upstream.
- `status_log.md` records dated check-ins so anyone can reconstruct the history quickly.

## Links
- [Inventory](inventory.md)
- [Phases](phases.md)
- [Decision log](decision_log.md)
- [Status log](status_log.md)
