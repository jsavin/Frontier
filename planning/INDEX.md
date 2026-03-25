# Planning Index

**Status**: Phase 4 Active — CLI Extensibility, Distribution Workflow, Test Coverage
**Last Updated**: 2026-03-25
**Entry Point**: This document provides navigation to all active planning docs.

**Keep This Synced With**: `_CURRENT_STATUS.md` and `_CURRENT_TODO_LIST.md`

## Current Focus (Mar 2026)

**CLI Extensibility & Distribution** (ACTIVE)
- CLI arguments bridge to UserTalk via `system.environment.args` (PRs #488, #489)
- `--browser` flag for AI-driven web testing via `sys.openUrl` (PR #490)
- Clean Virgin.root and `make clean-root` target (PRs #491, #493)
- PSTRING compile-time validation (PR #492)
- ODB script editing workflow via protocol

**HTTP Server & mainResponder** (WORKING)
- Full HTTP request pipeline operational (PRs #462-#480)
- TCP accept -> GIL callback -> mainResponder.respond -> serve page

**Phase 4 Global State Elimination** (QUEUED)
- databasedata elimination complete (Phases 1-10)
- P0a (Hash table context migration) queued
- Reference: `planning/phase4/INDEX.md`

Purpose
- Provide a single entry point to the phase directories and the most relevant planning documents.

Related Docs
- planning/phase_overview.md
- planning/Frontier_Refactoring_Plan.md
- planning/phase_gates.md
- planning/DECISIONS.md
- planning/phase3/carbon_migration/README.md *(current plan)*

Change Log
- 2025-10-30: Reactivated in-progress phase docs (moved back to `planning/phase*/`) and highlighted the Carbon plan as the primary workstream.
- 2025-10-29: Added Carbon migration plan and redirected status/index content to the new workstream.
- 2025-10-12: Reorganized planning materials by phase and refreshed cross-links.

## Active Workstreams

### Phase 4: Threading & Networking (Current Priority)

**Networking** - `planning/phase4/networking/`
- ✅ TCP Phase 1A/1B/3 complete (13 verbs, PRs #327, #329, #330)
- 🚀 TCP Phase 2 next (Buffered I/O, 4 verbs) - HTTP client milestone
- [Networking INDEX](phase4/networking/INDEX.md) - Phase status and execution plan
- [TCP Verbs Analysis](phase4/networking/TCP_VERBS_ANALYSIS.md) - Complete API reference
- [Implementation Plan](phase4/networking/IMPLEMENTATION_PLAN.md) - Phased execution strategy

**Threading & Global State** - `planning/phase4/threading/`
- ✅ Thread Registry complete (PR #317)
- ✅ Deterministic testing foundation complete (PR #318, Phase 1)
- ⏸️ P0a (Hash table context migration) queued for weeks 7-9
- [Threading INDEX](phase4/threading/README.md) - 5-phase roadmap
- [P0a Critical Thread Safety](phase4/p0a-critical-thread-safety/README.md) - Launch blocking work

**Phase 4 Overview** - `planning/phase4/`
- [Phase 4 INDEX](phase4/INDEX.md) - Complete Phase 4 roadmap (P0a through P2)
- [Comprehensive Plan](phase4/00-overview/comprehensive_plan.md) - Detailed execution plan
- [Executive Summary](phase4/00-overview/executive_summary.md) - High-level overview

### Phase 3: Verb Implementation & Runtime Modernization

**Verb Porting** - `planning/phase3/verb_implementations/`
- File verbs: 100% complete (86/86) ✅
- Lang verbs: 16% (ongoing)
- Overall: 37% (264/710 verbs)
- [Verb Implementation Status](phase3/verb_implementations/verb_implementation_status.md)

**UI Abstraction** - `planning/phase3/ui_abstraction/`
- Headless runtime separation from UI layer
- [UI Abstraction Overview](phase3/ui_abstraction/ui_abstraction_overview.md)

**Database & Migration** - `planning/phase3/`
- ✅ v6→v7 migration complete
- ✅ Database context refactoring complete
- [ODB Engine V7 Migration Plan](phase3/ODB_ENGINE_V7_MIGRATION_PLAN.md)
- [.root7 Extension Retirement](root7_retirement_plan.md) — Migration naming, runtime cleanup, doc updates

### Phase 6: CRDT & Collaborative ODB (Future)

**CRDT Foundation** - `planning/phase6/`
- Multi-user collaborative editing of ODB objects via CRDTs
- [Phase 6 INDEX](phase6/INDEX.md) — Overview and document links
- [CRDT Foundation Roadmap](phase6/CRDT_FOUNDATION_ROADMAP.md) — Strategic vision (months 6-18)
- [External Atomicity & Collaboration](phase6/EXTERNAL_ATOMICITY_AND_COLLABORATION_ROADMAP.md) — Migration path

### Phase 7: Polyglot Scripting (Future)

**Multi-Language Support** - `planning/phase7/polyglot/`
- JavaScript and Python as first-class scripting languages
- Abstract language interface (C vtable pattern)
- Compiled extensions (Go, Rust via shared libraries)
- [Vision & Architecture](phase7/polyglot/00-polyglot-vision.md) — Overview and prior art
- [Language Interface Spec](phase7/polyglot/01-language-interface-spec.md) — C vtable definition
- [JavaScript Integration](phase7/polyglot/02-javascript-integration.md) — JS engine analysis
- [Python Integration](phase7/polyglot/03-python-integration.md) — Double-GIL problem and solutions
- [Compiled Extensions](phase7/polyglot/04-compiled-extensions.md) — Go, Rust plugin model

### Phase 5: Parser & Tooling

**Text Modernization** - `planning/phase5/text_modernization/`
- UTF-8 adoption, Pascal string elimination, hashtable algorithm rework, database format v8
- 7-phase roadmap from bridging helpers through full rollout
- [Text Modernization Roadmap](phase5/text_modernization/README.md) — **master document**

**Future Modernization** - `planning/phase5/`
- Bison 3 migration planning
- [Phase 5 Overview](phase5/PHASES.md)

## Archive

The archive holds completed/retired material for historical reference.

**Completed Workstreams** - `planning/archive/completed-workstreams/`
- TCP Phase 1A preflight (✅ complete, PRs #327, #329, #330)
- Hierarchical OPML export design (✅ complete, PR #326)
- [Completed Workstreams README](archive/completed-workstreams/README.md)

**Completed Phases** - `planning/archive/completed-phases/`
- Phase 1: Foundations & Toolchain
- Phase 2: Core modernization records
- Phase 3: Carbon migration, logging infrastructure, kernel verb porting

**Reference Material** - `planning/archive/reference/`
- Historical design patterns and decisions
- Superseded approaches (kept for context)

**Status History** - `planning/_STATUS_ARCHIVE.md`
- Historical status entries before 2026-01-05

## Cross-Cutting Documentation

### Architecture Decision Records (ADRs)
- **ADR Index**: `planning/architectural_decision_records/README.md`
- **Recent ADRs**:
  - ADR-010: Deterministic Thread Testing (3-phase roadmap)
  - ADR-011: Search Path Priority in Name Resolution
  - ADR-005: Parameter State Thread-Safety (thread-local pattern)
- **Patterns**: Explicit Context Passing (`architectural_decision_records/explicit-context-passing/`)

### Strategic Roadmaps
- **CRDT Foundation**: `planning/phase6/CRDT_FOUNDATION_ROADMAP.md` - Collaborative ODB foundation
- **Multi-User Collaboration**: `planning/phase6/EXTERNAL_ATOMICITY_AND_COLLABORATION_ROADMAP.md`
- **Database Corruption Prevention**: `planning/DATABASE_CORRUPTION_PREVENTION.md`
- **Integration Test Gaps**: `planning/INTEGRATION_TEST_GAPS.md` - Prioritized test coverage gaps (persistence, concurrency, verbs)

### Reference Documentation
- **Legacy Glossary**: `planning/legacy_glossary.md` - Historical Frontier terminology
- **Third-Party Dependencies**: `planning/third_party_dependencies.md`
- **Future Enhancements**: `planning/TODO_future_improvements.md`
- **Code Patterns**: `planning/docs/code_patterns.md`
- **Docs Conventions**: `planning/DOCS_CONVENTIONS.md`

## Developer Quickstart
- Headless/Tests: `planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md`

## Next Steps (High Level)

### Immediate
1. **Integration Test Gap Investigation**: Triage 66 pre-existing test failures identified during gap analysis
2. **CI/CD for clean-root**: Integrate `make clean-root` into pre-release pipeline
3. **Manila Guest Database Testing**: Full installation and serving end-to-end

### Short-Term
4. **GUI Application Prototype**: Protocol layer + table browser (planning complete)
   - Reference: `planning/gui/ARCHITECTURE.md`, `planning/gui/PROTOCOL.md`

### Medium-Term
5. **Phase 4 P0a**: Hash Table Context Migration (launch blocking)
   - Reference: `planning/phase4/p0a-critical-thread-safety/README.md`

### Ongoing
6. **Verb Implementation Coverage**: 68% (482/710 verbs) — all core processors complete
   - Reference: `planning/phase3/verb_implementations/`

## Navigation Tips

- **Start Here**: `planning/_CURRENT_STATUS.md` for recent work and current focus
- **Work Queue**: `planning/_CURRENT_TODO_LIST.md` for priority-ordered tasks
- **Phase Planning**: `planning/phase4/INDEX.md` for comprehensive Phase 4 roadmap
- **Historical Context**: `planning/archive/` for completed work and decisions
- **Implementation Guides**: See `docs/` directory for verb implementation, testing, CLI usage
