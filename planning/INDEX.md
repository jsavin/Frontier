# Planning Index

Status
- State: In Progress
- Phase: Multi-Phase Roadmap
- Last Updated: 2025-09-29
- Notes: UI separation is Phase 2; Hash Tables moved to Phase 3.

Purpose
- Single source of truth for active phases, status, and key documents.

Related Docs
- planning/Frontier_Refactoring_Plan.md
- planning/phase_gates.md
- planning/ui_abstraction/PHASES.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

Current Phases
- Phase 1: Headless/CLI enablement and testing
  - Plan: planning/1.0_phase1_cli_implementation_plan.md — CLI entry, parser, executor
  - Summary: planning/1.1_phase1_implementation_summary.md — status and deliverables
  - Tests: planning/0.5.23_runtime_test_plan.md — headless/runtime test coverage
  - Parser: planning/phase1/parser_regeneration_with_bison.md — regenerate parser with modern Bison
- Phase 2: UI Abstraction (ports/adapters)
  - Overview: planning/ui_abstraction/ui_abstraction_overview.md — why and goals
  - Phases: planning/ui_abstraction/PHASES.md — stub/gate then separate
  - Analysis: planning/ui_abstraction/phase2/analysis.md — limits of stubs/#if
  - Architecture: planning/ui_abstraction/phase2/architecture.md — UIServices and adapters
  - Migration Plan: planning/ui_abstraction/phase2/migration_plan.md — sequencing
  - Patterns: planning/ui_abstraction/phase2/patterns_and_choices.md — MVC/MVVM guidance
- Phase 3: Hash Table Modernization
  - Strategy: planning/0.5.16_hash_table_modernization_strategy.md — design and migration
  - TODO: planning/TODO_future_improvements.md — priorities and notes
- Phase 4: String & Rich Text Modernization
  - Overview: planning/phase4/PHASES.md — goals, milestones, gates
  - String + Text Plan: planning/phase4/string_and_text_modernization.md — Pascal string retirement, UTF text, WPText/Paige deprecation
- Phase 5: Parser/Bison 3 Migration (Gated)
  - Plan: planning/phase5/bison3_migration_plan.md — stage for Bison 3, keep 2.3 compatibility and commit generated sources

Cross-Cutting Docs
- Frontier Refactoring Plan: planning/Frontier_Refactoring_Plan.md
- Carbon/UI Audit: planning/ui_abstraction/phase2/carbon_dependency_audit.md
- langsystem7 Headless Refactor: planning/ui_abstraction/phase2/langsystem7_headless_refactor.md
- Portable Handle Runtime: planning/0.5.21_portable_handle_runtime.md
- QuickTime Retirement Scope: planning/ui_abstraction/phase2/quicktime_retirement.md

Developer Quickstart
- Headless/Tests: planning/DEVELOPER_QUICKSTART_HEADLESS.md

Policies & Guides
- Phase Gates: planning/phase_gates.md
- No-UI Linkage Policy: planning/no_ui_linkage_policy.md
- Headless Stubbed Behavior Matrix: planning/headless_stubbed_behavior_matrix.md

ADRs
- 0001 — Move Hash Tables to Phase 3: planning/adr/0001-hash-tables-phase-3.md
- 0002 — UI Boundary via Ports/Adapters: planning/adr/0002-ui-boundary-ports-and-adapters.md
