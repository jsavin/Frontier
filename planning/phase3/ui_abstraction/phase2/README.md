# Phase 2 — Architectural Separation

Status
- State: Draft
- Phase: 2
- Last Updated: 2025-09-29
- Notes: Analysis/design/migration documents included in this folder.

Related Docs
- planning/ui_abstraction/phase2/analysis.md
- planning/ui_abstraction/phase2/architecture.md
- planning/ui_abstraction/phase2/migration_plan.md
- planning/ui_abstraction/phase2/patterns_and_choices.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

Goal: Replace widespread stubs and `#if FRONTIER_HEADLESS` gating with a clean runtime ↔ UI boundary so the core links and runs with no UI framework dependencies, and UI shells (native or remote) can evolve independently.

This folder contains the detailed analysis and plan:
- `analysis.md`: Why stubs don’t scale; coupling inventory; recommended architecture; enforcement tactics.
- `architecture.md`: Proposed ports-and-adapters design, `UIServices` interface, and module split.
- `migration_plan.md`: Sequenced steps to move conditionals into adapters and fence the core.
- `patterns_and_choices.md`: MVC vs. MVVM discussion and guidance per platform.
