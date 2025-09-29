# Phase 1 — Stub and Gate

Status
- State: In Progress
- Phase: 2 (Phase 1 of UI abstraction)
- Last Updated: 2025-09-29
- Notes: Continue `FRONTIER_HEADLESS/PORTABLE` gating for headless builds.

Related Docs
- planning/ui_abstraction/PHASES.md
- planning/ui_abstraction/phase2/migration_plan.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

Objective: Maintain velocity by getting headless builds linking and running tests using targeted stubs and `#if` gates, without large-scale refactors.

Scope
- Continue to use `FRONTIER_HEADLESS` and `FRONTIER_PORTABLE` to conditionally exclude UI-only code.
- Provide/extend shims for AppleEvents, dialogs, menus, windowing, timers, and legacy types via portable headers.
- Keep legacy UI modules largely intact; do not attempt broad header surgery yet.

Deliverables
- Reliable headless/CLI build with no incidental dependency on AppKit/Carbon/Win32.
- Documented inventory of stubs and code paths gated behind `FRONTIER_HEADLESS`.
- Minimal changes to unblock tests and automation.

Recommended Targets
- `frontier-cli`: link only against core modules; remove `-framework AppKit`/`-framework Carbon` once dependency leaks are addressed.
- Headless runtime library (static or shared) usable by tests.

Key Macros/Files (as of current tree)
- Macros: `FRONTIER_HEADLESS`, `FRONTIER_PORTABLE`.
- Portable headers: `portable/standard_portable.h`, `portable/portable_types.h`, `portable/shelltypes_portable.h`.
- Headless stubs: `Common/headers/headless_stubs.h`.
- Gated sources: examples include `Common/source/ops.c`, `Common/source/opedit.c`, `Common/source/langsystem7.c`, `Common/headers/file.h`.

Milestones
1. Ensure headless build requires no UI frameworks in the link step.
2. Consolidate stubbed AppleEvent and dialog behavior to be consistent (error reporting, logging).
3. Inventory remaining UI touchpoints invoked from core paths executed by CLI and tests.

Exit Criteria
- Headless CLI runs core evaluation and DB operations with no UI linkage.
- CI artifacts confirm the headless binary has no unresolved UI symbols.

Notes
- Prefer small, surgical guards close to existing conditionals. Defer structural moves to Phase 2.
