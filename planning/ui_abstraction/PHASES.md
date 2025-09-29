# UI Abstraction Phases

Status
- State: In Progress
- Phase: 2
- Last Updated: 2025-09-29
- Notes: Phase 1 (stub/gate) and Phase 2 (separate) defined here.

Related Docs
- planning/ui_abstraction/ui_abstraction_overview.md
- planning/ui_abstraction/phase1/README.md
- planning/ui_abstraction/phase2/analysis.md
- planning/ui_abstraction/phase2/architecture.md
- planning/ui_abstraction/phase2/migration_plan.md
- planning/ui_abstraction/phase2/patterns_and_choices.md
- planning/ui_abstraction/phase2/carbon_dependency_audit.md
- planning/ui_abstraction/phase2/quicktime_retirement.md
- planning/ui_abstraction/phase2/langsystem7_headless_refactor.md

This project proceeds in two major phases to separate Frontier's runtime from platform UI code while maintaining momentum on testing and headless execution.

## Phase 1 — Stub and Gate (Build/Link Unblock)
- Goal: Get headless builds compiling, linking, and testable by stubbing UI APIs and gating UI code paths with compiler directives.
- Approach: Continue using `FRONTIER_HEADLESS` and `FRONTIER_PORTABLE` to exclude UI, provide shims for AppleEvents, dialogs, menus, windowing, and any Mac/Win UI types.
- Output: A reliable headless runtime path suitable for CLI and automated tests; minimal code churn in legacy UI modules.

## Phase 2 — Architectural Separation (Runtime ↔ UI Boundary)
- Goal: Fully decouple the core runtime from UI frameworks via explicit interfaces (ports/adapters), enabling multiple shells: headless, native UIs, and future web UI. Hash Table modernization shifts to Phase 3 to follow UI separation.
- Approach: Introduce a narrow UI services interface consumed by core; move conditional compilation into adapters; restructure the build into `libfrontier_core` + platform shells.
- Output: Clean layering, simpler builds (core has no UI link), reduced divergence of behavior between headless and UI, and a foundation for additional clients.

See `phase1/` and `phase2/` for detailed plans and analysis.
Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).
