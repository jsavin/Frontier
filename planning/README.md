# Frontier Planning Documentation

**Status**: Phase 4 Active (TCP Phase 2 Next, P0a Queued)
**Last Updated**: 2026-01-25

**Quick Navigation**:
- **Current Work**: See `_CURRENT_STATUS.md` for recent achievements and focus
- **Work Queue**: See `_CURRENT_TODO_LIST.md` for priority-ordered tasks
- **Planning Index**: See `INDEX.md` for comprehensive navigation
- **Archive**: See `archive/README.md` for completed work and historical context

## Current Focus (Jan 2026)

**TCP Networking Phase 2** (🚀 STARTING)
- Phase 1A/1B/3 complete (13 verbs, PRs #327, #329, #330)
- Phase 2: Buffered I/O (4 verbs) - HTTP client milestone

**Phase 4 Global State Elimination** (⏸️ QUEUED)
- Thread registry complete (PR #317)
- Deterministic testing foundation complete (PR #318)
- P0a queued for weeks 7-9 after TCP Phase 2

**Database & Path Resolution** (✅ COMPLETE)
- System.paths migration fixes (PR #336)
- Path entry matching fixes (PR #337)
- Builtins priority fixes (PR #342)

## Related Documentation

**Active Planning**:
- `INDEX.md` - Comprehensive planning navigation
- `_CURRENT_STATUS.md` - Recent achievements and current focus
- `_CURRENT_TODO_LIST.md` - Priority-ordered work queue
- `phase4/INDEX.md` - Phase 4 roadmap (threading, networking, global state)
- `phase6/CRDT_FOUNDATION_ROADMAP.md` - Collaborative ODB foundation strategy

**Architecture Decisions**:
- `architectural_decision_records/` - ADRs for major decisions
- `phase6/EXTERNAL_ATOMICITY_AND_COLLABORATION_ROADMAP.md` - Multi-user collaboration
- `DATABASE_CORRUPTION_PREVENTION.md` - Safety guidelines

**Historical Context**:
- `archive/` - Completed workstreams and reference material
- `_STATUS_ARCHIVE.md` - Status history before 2026-01-05

## Change Log

- **2026-01-25**: Planning directory reorganization
  - Updated INDEX.md to reflect TCP Phase 1A/1B/3 completion
  - Created archive structure (completed-workstreams, reference, experimental)
  - Archived TCP Phase 1A preflight and hierarchical OPML design
  - Updated networking INDEX with completion status
- 2025-10-30: Reactivated unfinished phase docs, pointed navigation to Carbon plan
- 2025-10-12: Reorganized legacy docs into phase subdirectories
- 2025-09-29: Initial planning structure

## Directory Layout

| Directory | Purpose | Status |
|-----------|---------|--------|
| `phase4/` | **Active.** Threading, networking, global state elimination | Current priority |
| `phase3/` | Verb implementation, UI abstraction, database migration | Ongoing |
| `phase5/` | Future: Parser modernization, UTF-8, string handling | Planning |
| `architectural_decision_records/` | ADRs for major architectural decisions | Active reference |
| `archive/` | Completed workstreams, historical reference, experimental | Read-only |
| Root files | Status, TODO, INDEX, strategic roadmaps | Active navigation |

**Archive Structure**:
- `archive/completed-workstreams/` - Successfully implemented major initiatives
- `archive/completed-phases/` - Phase 1, 2, 3 milestone documentation
- `archive/reference/` - Historical context and superseded designs
- `archive/experimental/` - Tried approaches (not adopted or deferred)

See `archive/README.md` for complete archive navigation.

## Writing Guidelines

1. Include a status block (`State`, `Phase`, `Last Updated`, `Notes`) at the top of each planning doc.
2. Add yourself to the change log when you update content.
3. Prefer short, phase-scoped documents over monolithic plans; cross-link related notes.
4. Use ADRs (`planning/adr/`) for architectural decisions that affect multiple phases.
5. Run `python3 scripts/check_doc_links.py` before submitting large doc refactors.

For a high-level summary of goals and risks, start with `planning/Frontier_Refactoring_Plan.md`. For day‑to‑day navigation, use `planning/INDEX.md`.
