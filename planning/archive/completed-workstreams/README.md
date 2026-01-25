# Completed Workstreams Archive

This directory contains planning documents for **completed** major workstreams. These documents are kept for historical reference and to understand the decision-making process that led to the current implementation.

## Contents

### Networking Infrastructure

**TCP Phase 1A/1B/3 Complete** (2026-01-19 to 2026-01-24)

- **TCP_PHASE1A_PREFLIGHT.md** - Pre-flight checklist for TCP Phase 1A implementation
  - Status: ✅ COMPLETE (merged in PRs #327, #329, #330)
  - Outcome: 11 TCP verbs implemented (Phase 1A: 6 verbs, Phase 1B: 5 verbs, Phase 3: 2 verbs)
  - Reference: Active networking docs in `planning/phase4/networking/`
  - Tests: 150+ integration tests, comprehensive C unit tests
  - Impact: Production-ready TCP networking layer supporting client-server architecture

### Infrastructure & Process Improvements

**Hierarchical OPML Export** (2026-01-19)

- **HIERARCHICAL_OPML_DESIGN.md** - Design document for hierarchical OPML test export structure
  - Status: ✅ COMPLETE (merged in PR #326)
  - Outcome: Converted monolithic 14,002-line OPML to 26-file hierarchical structure
  - Impact: Eliminates merge conflicts when multiple developers add tests
  - Implementation: `tools/export_tests_to_opml.py`

## What Gets Archived Here?

Documents move here when:
- ✅ The workstream is **fully complete** and merged to develop
- ✅ The implementation is **stable** and in production use
- ✅ The document is primarily **historical reference** (not active planning)

Documents stay in active planning when:
- ❌ Work is ongoing (even if partially complete)
- ❌ Document is actively referenced for next phases
- ❌ Contains architecture/patterns still being refined

## Related Archives

- **planning/archive/completed-phases/** - Completed phase planning (Phase 1, 2, 3 milestones)
- **planning/archive/reference/** - Historical reference documents (design patterns, decisions)
- **planning/archive/experimental/** - Tried approaches that were superseded
- **planning/_STATUS_ARCHIVE.md** - Historical status entries before 2026-01-05

## Active Planning

For current work, see:
- **planning/_CURRENT_STATUS.md** - Recent achievements and current focus
- **planning/_CURRENT_TODO_LIST.md** - Priority-ordered work queue
- **planning/INDEX.md** - Navigation for all active planning
- **planning/phase4/** - Phase 4 active workstreams (threading, networking, global state)
