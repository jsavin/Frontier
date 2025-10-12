# ADR 0002 — UI Boundary via Ports and Adapters

Status
- State: Accepted
- Date: 2025-09-29

Related Docs
- planning/ui_abstraction/phase2/analysis.md
- planning/ui_abstraction/phase2/architecture.md
- planning/ui_abstraction/phase2/migration_plan.md

Change Log
- 2025-09-29: ADR accepted; sections initialized.

Context
- Core code contains scattered `#if FRONTIER_HEADLESS` and UI types in shared headers.
- Headless builds and testability suffer when core links UI frameworks.

Decision
- Introduce a narrow `UIServices` interface and adopt a ports-and-adapters boundary.
- Core depends only on `UIServices`; headless and native UIs implement adapters.

Consequences
- Conditional compilation moves into adapters; core becomes UI-agnostic.
- Build splits into `libfrontier_core` + adapters; CLI links core + headless adapter.

References
- planning/ui_abstraction/phase2/analysis.md
- planning/ui_abstraction/phase2/architecture.md
- planning/ui_abstraction/phase2/migration_plan.md
