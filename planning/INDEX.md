# Planning Index

Status
- State: In Progress
- Phases: Foundations → Architecture → Headless Runtime → Text Modernization → Tooling
- Last Updated: 2025-10-12
- Notes: Headless runtime is operational; CLI build/linking cleanup remains. UTF-8 migration planning queued after headless stabilization.

Purpose
- Provide a single entry point to the phase directories and the most relevant planning documents.

Related Docs
- planning/phase_overview.md
- planning/Frontier_Refactoring_Plan.md
- planning/phase_gates.md
- planning/DECISIONS.md

Change Log
- 2025-10-12: Reorganized planning materials by phase and refreshed cross-links.

## Phase 1 — Foundations & Toolchain (`planning/phase1/`)
- Initial Analysis: `planning/phase1/0.1_initial_analysis.md`
- Compiler Compatibility: `planning/phase1/0.4.1_compiler_compatibility_plan.md`
- Minimal Compilation + Test Harness: `planning/phase1/0.5.1_minimal_viable_compilation.md`, `planning/phase1/0.5.3_testing_strategy.md`
- QuickTime & Legacy Cleanup: `planning/phase1/0.4.6_quicktime_elimination_plan.md`

## Phase 2 — Core Architecture & Database (`planning/phase2/`)
- Database Versioning & Paths: `planning/phase2/0.5.14_database_versioning_strategy.md`, `planning/phase2/database_path_canonicalization.md`
- 64-bit Data Structure Plan & Execution: `planning/phase2/0.5.15_64bit_data_structure_analysis.md`, `planning/phase2/0.5.19_phase1_migration_implementation_complete.md`
- Portable Handle Runtime: `planning/phase2/0.5.21_portable_handle_runtime.md`

## Phase 3 — Headless Runtime & Automation (`planning/phase3/`)
- CLI Execution Plan & Summary: `planning/phase3/1.0_phase1_cli_implementation_plan.md`, `planning/phase3/1.1_phase1_implementation_summary.md`
- Runtime Test Plan: `planning/phase3/0.5.23_runtime_test_plan.md`
- Headless Developer Guides: `planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md`, `planning/phase3/headless_stubbed_behavior_matrix.md`
- System Verb Bootstrapping & UI Abstraction: `planning/phase3/system_verbs_bootstrap_plan.md`, `planning/phase3/ui_abstraction/`
- Frontier.root Headless Bring-up & Kernel Glue Integration: `planning/phase3/frontier_root_headless_plan.md`
- Legacy Pascal Layouts & Modernization: `planning/phase3/pascal_runtime_modernization.md`, `planning/phase3/headless_legacy_table_loader.md`
- Headless Daemon Vision & Service Core: `planning/phase3/headless_daemon_vision.md`

## Phase 4 — String & Text Modernization (`planning/phase4/`)
- Modernization Overview: `planning/phase4/PHASES.md`
- UTF-8 Transition Plan: `planning/phase4/utf8_transition_plan.md`
- String & Text Modernization Strategy: `planning/phase4/string_and_text_modernization.md`

## Phase 5 — Toolchain & Parser Evolution (`planning/phase5/`)
- Bison 3 Migration Plan: `planning/phase5/bison3_migration_plan.md`
- Parser Regeneration Playbook: `planning/phase5/parser_regeneration_with_bison.md`

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
1. Complete CLI build/linking cleanup and extend coverage toward the headless service core (Phase 3/3b).
2. Finalize UTF-8 migration work breakdown (Phase 4).
3. Schedule parser/Bison upgrades once UTF-8/text modernization milestones are stable (Phase 5).
