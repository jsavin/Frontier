# Planning Index

> **Active Plan:** Follow the [Carbon Dependency Retirement workstream](carbon_migration/README.md). Legacy phase docs with open work now live under `planning/phase*/`; completed notes stay in the archive.

Status
- State: Carbon Migration In Progress
- Current Workstream: [Carbon Dependency Retirement](carbon_migration/README.md)
- Last Updated: 2025-10-30
- Notes: Older phase docs remain for historical context. Any document still marked Draft/Planned/In Progress has been restored to `planning/phase*/` so it can continue evolving.

Purpose
- Provide a single entry point to the phase directories and the most relevant planning documents.

Related Docs
- planning/phase_overview.md
- planning/Frontier_Refactoring_Plan.md
- planning/phase_gates.md
- planning/DECISIONS.md
- planning/carbon_migration/README.md *(current plan)*

Change Log
- 2025-10-30: Reactivated in-progress phase docs (moved back to `planning/phase*/`) and highlighted the Carbon plan as the primary workstream.
- 2025-10-29: Added Carbon migration plan and redirected status/index content to the new workstream.
- 2025-10-12: Reorganized planning materials by phase and refreshed cross-links.

## Active Workstreams
- **Carbon Migration** – `planning/carbon_migration/`
  - [Plan overview](carbon_migration/README.md)
  - [Inventory](carbon_migration/inventory.md)
  - [Phases](carbon_migration/phases.md)
  - [Decision log](carbon_migration/decision_log.md)
  - [Status log](carbon_migration/status_log.md)
- **Legacy Follow-ups**
  - Phase 2 open items – `planning/phase2/`
  - Phase 3 headless runtime plans – `planning/phase3/`
  - Phase 4 text modernization backlog – `planning/phase4/`
  - Phase 5 parser/tooling roadmap – `planning/phase5/`

## Archive
The archive now holds completed/retired material only.

- **Phase 1 — Foundations & Toolchain** (`planning/archive/phase1/`)
- **Phase 2 — Completed Records** (`planning/archive/phase2/`)
- **Phase 3 — Completed Records** (`planning/archive/phase3/`)

## Cross-Cutting Docs
- ADR Index: `planning/adr/`
- Open Issues & Backlog: `planning/issues/`
- Legacy Glossary: `planning/legacy_glossary.md`
- Third-Party Dependencies: `planning/third_party_dependencies.md`
- Future Enhancements: `planning/TODO_future_improvements.md`
- Code Patterns Catalog: `planning/docs/code_patterns.md`

## Developer Quickstart
- Headless/Tests: `planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md`

## Next Steps (High Level)
1. Finish the documentation pivot (update/deprecate older Phase 2/3 docs, keep `_CURRENT_STATUS.md` synced).
2. Execute Phase 1 of the Carbon plan (header hygiene so portable builds preprocess cleanly).
3. Begin runtime modernization (remove remaining Carbon helpers in `langhash.c`, `strings.c`, `memory.c`) per `carbon_migration/phases.md`.
