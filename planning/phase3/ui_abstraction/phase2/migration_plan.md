# Phase 2 Migration Plan

Status
- State: Draft
- Phase: 2
- Last Updated: 2025-09-29
- Notes: Focuses on fencing core and replacing conditionals with adapters.

Related Docs
- planning/ui_abstraction/phase2/analysis.md
- planning/ui_abstraction/phase2/architecture.md
- planning/ui_abstraction/phase2/migration_playbook.md
- planning/ui_abstraction/phase2/UIServices_stub.md
- planning/no_ui_linkage_policy.md
- planning/phase_gates.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

## Table of Contents
- [Principles](#principles)
- [Steps](#steps)
- [Enforcement](#enforcement)
- [Exit Criteria](#exit-criteria)

## Principles
- Prefer incremental, vertical slices that move one feature path to `UIServices` at a time.
- Avoid broad rewrites; contain changes and keep behavior identical.

## Steps
1. Fence the core
   - Introduce a `FRONTIER_CORE` build for `libfrontier_core`.
   - Add compile-time guards to reject inclusion of platform UI headers in core.
   - Ensure the core link has no UI frameworks.
2. Define `UIServices v0`
   - Start with logging, backgrounding, alerts, file dialogs, clipboard, and menu enable/disable.
   - Provide a headless adapter that logs and returns deterministic errors where UI is unavailable.
3. Replace high-traffic call sites
   - Swap scattered `#if FRONTIER_HEADLESS` blocks that only differ by logging/alerts to `UIServices` calls.
   - Migrate background/yield logic to `UIServices` (`ops.c` delay paths, etc.).
4. Remove UI link from CLI
   - Update `frontier-cli` to link `libfrontier_core` + headless adapter only.
   - Validate core scripts and DB ops still work in headless mode.
5. Expand surface as needed
   - Introduce additional endpoints (automation bridge, text editing hooks) when refactoring those modules.
6. Platform adapters
   - Implement macOS/Windows adapters using native frameworks behind the interface.

## Enforcement
- CI job: grep the core binary for known UI symbols; fail on presence.
- Lint: forbid `Windows.h`, AppKit, Carbon includes in core files.

## Exit Criteria
- No `#if FRONTIER_HEADLESS` in core hot paths; conditionals reside in adapters.
- `frontier-cli` has no UI frameworks in link flags and passes tests.
- Native shells run through the same core and `UIServices` boundary.
