# Planning Index

**Status**: Phase 4 Active (TCP Phase 2 Next, P0a Queued)
**Last Updated**: 2026-01-25
**Entry Point**: This document provides navigation to all active planning docs.

**Keep This Synced With**: `_CURRENT_STATUS.md` and `_CURRENT_TODO_LIST.md`

## Current Focus (Jan 2026)

**TCP Networking Phase 2** (🚀 STARTING)
- Phase 1A/1B/3 complete (13 verbs implemented, PRs #327, #329, #330)
- Phase 2: Buffered I/O (4 verbs) - HTTP client milestone
- Reference: `planning/phase4/networking/INDEX.md`

**Phase 4 Global State Elimination** (⏸️ QUEUED for weeks 7-9)
- Thread registry complete (PR #317)
- Deterministic thread testing foundation complete (PR #318)
- P0a (Hash table context migration) queued after TCP Phase 2
- Reference: `planning/phase4/INDEX.md`

**Database & Path Resolution** (✅ COMPLETE)
- System.paths migration fixes (PR #336)
- Path entry name matching fixes (PR #337)
- Builtins priority fixes (PR #342)

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

### Phase 5: Parser & Tooling

**Future Modernization** - `planning/phase5/`
- Bison 3 migration planning
- UTF-8 transition roadmap
- String/text modernization
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
- **CRDT Foundation**: `planning/CRDT_FOUNDATION_ROADMAP.md` - Collaborative ODB foundation
- **Multi-User Collaboration**: `planning/EXTERNAL_ATOMICITY_AND_COLLABORATION_ROADMAP.md`
- **Database Corruption Prevention**: `planning/DATABASE_CORRUPTION_PREVENTION.md`

### Reference Documentation
- **Legacy Glossary**: `planning/legacy_glossary.md` - Historical Frontier terminology
- **Third-Party Dependencies**: `planning/third_party_dependencies.md`
- **Future Enhancements**: `planning/TODO_future_improvements.md`
- **Code Patterns**: `planning/docs/code_patterns.md`
- **Docs Conventions**: `planning/DOCS_CONVENTIONS.md`

## Developer Quickstart
- Headless/Tests: `planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md`

## Next Steps (High Level)

### Immediate (Weeks 5-6)
1. **TCP Networking Phase 2**: Buffered I/O (4 verbs) → HTTP client milestone
   - Reference: `planning/phase4/networking/IMPLEMENTATION_PLAN.md`

### Short-Term (Weeks 7-9)
2. **Phase 4 P0a**: Hash Table Context Migration (launch blocking)
   - Migrate hash table operations from global state to thread-local
   - Reference: `planning/phase4/p0a-critical-thread-safety/README.md`

### Medium-Term (Weeks 10-12)
3. **Phase 4 P0b**: Continue Global State Elimination
   - Outline context migration
   - External object processing audit
   - Reference: `planning/phase4/INDEX.md`

### Ongoing
4. **Verb Implementation Coverage**: Continue porting verbs to headless runtime
   - Current: 37% (264/710 verbs)
   - Priority: Lang verbs, string verbs, table verbs
   - Reference: `planning/phase3/verb_implementations/`

## Navigation Tips

- **Start Here**: `planning/_CURRENT_STATUS.md` for recent work and current focus
- **Work Queue**: `planning/_CURRENT_TODO_LIST.md` for priority-ordered tasks
- **Phase Planning**: `planning/phase4/INDEX.md` for comprehensive Phase 4 roadmap
- **Historical Context**: `planning/archive/` for completed work and decisions
- **Implementation Guides**: See `docs/` directory for verb implementation, testing, CLI usage
