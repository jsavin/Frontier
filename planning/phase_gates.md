# Phase Gates

Status
- State: In Progress
- Phases: 1 → 5
- Last Updated: 2025-10-12
- Notes: Exit criteria synced with the reorganised phase plan.

Related Docs
- `planning/phase_overview.md`
- `planning/INDEX.md`
- `planning/Frontier_Refactoring_Plan.md`

Change Log
- 2025-10-12: Updated phase checkpoints to reflect the new structure.
- 2025-09-29: Initial draft.

## Phase 1 — Foundations & Toolchain
- **Build:** Clean builds on supported compilers/architectures without legacy dependencies (QuickTime removed, compatibility layer validated).
- **Tests:** Minimal runtime/test harness executes (core, db_format, parser smoke cases).
- **Docs:** Compiler/toolchain remediation recorded in `phase1/` docs; change log entries completed.

## Phase 2 — Core Architecture & Database
- **Data Structures:** 64-bit/ARM-safe structures implemented and verified against sample databases.
- **Database:** Dual-read/conversion strategy defined; migration tooling tested with representative data.
- **Performance:** Hash/handle modernization benchmarks recorded; regression thresholds documented.

## Phase 3 — Headless Runtime & Automation
- **CLI:** Headless CLI runs scripts and database operations without UI linkages (AppKit/Carbon/Win32 free).
- **Adapters:** `UIServices` adapters (headless + at least one native) in place; high-traffic call sites migrated.
- **Tests:** Automated runtime/CLI tests pass; headless stub matrix reviewed and updated.
- **Docs:** Developer quickstart/headless guides (`phase3/`) current.

## Phase 4 — String & Text Modernization
- **Strategy:** UTF-8 migration roadmap approved with phased rollout and compatibility plan (`phase4/utf8_transition_plan.md`).
- **Helpers:** Core string helper cleanup (safe conversions, removal of legacy casts) implemented.
- **Dependencies:** Text/rich text modernization backlog prioritised; impacts on DB/XML/AppleEvents documented.

## Phase 5 — Toolchain & Parser Evolution
- **Parser:** Regeneration procedure validated; Bison 3 migration go/no-go decision documented.
- **Tooling:** Language tooling and generated artefacts tracked with reproducible scripts.
- **Docs:** Parser/toolchain notes (`phase5/`) updated with status and rollback strategy.

Use this checklist alongside the per-phase READMEs to confirm readiness before promoting work across phases.
