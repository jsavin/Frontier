# Planning Archive

This directory contains historical planning documents, completed workstreams, and reference material. All documents here are kept for historical context and learning, not active planning.

## Archive Structure

### Completed Workstreams (`completed-workstreams/`)

Planning documents for **successfully implemented and merged** major initiatives.

**Recent additions** (2026-01-25):
- TCP Phase 1A preflight checklist (✅ PRs #327, #329, #330)
- Hierarchical OPML export design (✅ PR #326)

See: `completed-workstreams/README.md`

### Completed Phases (Phase-Specific Subdirectories)

Phase milestone documentation from earlier development cycles.

**Contents**:
- `phase1/` - Foundations & toolchain bootstrap (completed)
- `phase2/` - Core architecture/database initiative records (completed)
- `phase3/` - Headless runtime initiative records (completed)
  - Carbon migration, logging infrastructure, kernel verb porting

### Reference Material (`reference/`)

Historical reference documents that provide context for design decisions and patterns.

**Typical contents**:
- Superseded design approaches (kept for learning)
- Historical analysis documents
- Process documentation from early development

See: `reference/README.md`

### Experimental Approaches (`experimental/`)

Planning documents for approaches that were explored but not adopted, or deferred for later.

**Purpose**: Document why certain approaches weren't used, preserve ideas for future consideration.

See: `experimental/README.md`

## What Gets Archived?

### Archive When Work is COMPLETE

**Move to Archive When**:
- ✅ **ALL phases of a multi-phase workstream complete** and merged to develop
  - Example: TCP Phase 1A/1B/3 all complete → archive preflight checklist
  - Example: Phase 1 done but Phase 2 planned → KEEP active until Phase 2 done
- ✅ **Single-phase workstream complete** and merged to develop
  - Example: OPML design implemented → archive design document
- ✅ **Implementation is stable** and in production use
- ✅ **Document is purely historical reference** (not referenced by active planning)
- ✅ **Approach was tried but superseded** (experimental/deferred)

### Keep in Active Planning When

- ❌ **Any phase still in progress** or planned for current/next quarter
- ❌ **Document is actively referenced** for ongoing or planned work
- ❌ **Contains architecture/patterns** still being refined or evolved
- ❌ **Roadmap is strategic** for current direction (CRDT, multi-user, thread-safety, etc.)
- ❌ **Status ambiguous** - when in doubt, keep active until explicitly superseded

## Quick Reference: Where Is Everything?

| What Are You Looking For? | Where To Find It |
|---------------------------|------------------|
| **Current work status** | `planning/_CURRENT_STATUS.md` |
| **Work queue** | `planning/_CURRENT_TODO_LIST.md` |
| **Active planning** | `planning/INDEX.md` |
| **Phase 4 roadmap** | `planning/phase4/INDEX.md` |
| **Completed work (recent)** | `planning/archive/completed-workstreams/` |
| **Old phase docs** | `planning/archive/phase1/`, `phase2/`, `phase3/` |
| **Historical context** | `planning/archive/reference/` |
| **Status history** | `planning/_STATUS_ARCHIVE.md` |
| **Architecture decisions** | `planning/architectural_decision_records/` |

## Archive Maintenance

When archiving documents:

1. **Add context**: Update README with what was completed and when
2. **Update references**: Fix cross-references in active planning docs
3. **Preserve history**: Don't delete, just move
4. **Link back**: Active docs should link to archived context when relevant

## Recent Archive Activity

**2026-01-25**: Planning directory reorganization
- Created archive structure (completed-workstreams, reference, experimental)
- Archived TCP Phase 1A preflight checklist (work complete)
- Archived hierarchical OPML design (implementation complete)
- Updated planning/INDEX.md to reflect completed work
- Added comprehensive READMEs for archive navigation

## Related Documentation

- **Active Planning**: `planning/INDEX.md` - Navigation for all active planning
- **Current Status**: `planning/_CURRENT_STATUS.md` - Recent achievements and focus
- **Work Queue**: `planning/_CURRENT_TODO_LIST.md` - Priority-ordered tasks
