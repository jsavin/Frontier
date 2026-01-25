# Planning Directory Reorganization - 2026-01-25

## Summary

Comprehensive reorganization of the `planning/` directory to reflect completed work (TCP Phase 1A/1B/3, hierarchical OPML, thread registry), improve navigation, and create a clear archive structure for historical documents.

## Changes Made

### 1. Archive Structure Created

**New archive subdirectories**:
- `archive/completed-workstreams/` - Successfully implemented major initiatives
- `archive/reference/` - Historical reference and superseded designs
- `archive/experimental/` - Tried approaches (not adopted or deferred)

**Existing archives preserved**:
- `archive/phase1/`, `archive/phase2/`, `archive/phase3/` - Phase milestone docs

### 2. Documents Archived

**Completed workstreams moved to archive**:

1. **TCP_PHASE1A_PREFLIGHT.md** → `archive/completed-workstreams/`
   - Status: ✅ COMPLETE (PRs #327, #329, #330)
   - Outcome: 13 TCP verbs implemented (Phase 1A: 6, Phase 1B: 5, Phase 3: 2)
   - Rationale: Pre-flight checklist is now obsolete, work successfully completed

2. **HIERARCHICAL_OPML_DESIGN.md** → `archive/completed-workstreams/`
   - Status: ✅ COMPLETE (PR #326)
   - Outcome: Converted 14,002-line OPML to 26-file hierarchical structure
   - Rationale: Design implemented, tool in production use

### 3. Active Planning Documentation Updated

**planning/INDEX.md**:
- Updated status to reflect TCP Phase 1A/1B/3 completion
- Added current focus section (TCP Phase 2, P0a queued)
- Reorganized workstream sections with completion status
- Added comprehensive archive navigation
- Improved cross-references to active work

**planning/README.md**:
- Updated status and current focus
- Added archive structure explanation
- Updated directory layout table
- Added recent changes to change log

**planning/phase4/networking/INDEX.md**:
- Marked TCP Phase 1A/1B/3 as COMPLETE ✅
- Updated phase summary table with completion status
- Added completed verbs list (13 verbs)
- Added validation milestones section with completion status
- Added "Completed Work" section with PR references
- Added reference to archived preflight document

### 4. Archive Documentation Created

**archive/README.md**:
- Comprehensive archive navigation guide
- Clear criteria for what gets archived vs. stays active
- Quick reference table for finding documents
- Archive maintenance guidelines
- Recent archive activity log

**archive/completed-workstreams/README.md**:
- Lists completed major initiatives
- Provides context for TCP Phase 1A/1B/3
- Provides context for hierarchical OPML export
- Explains archival criteria

**archive/reference/README.md**:
- Explains purpose of reference material
- Lists types of content (historical, superseded, learning value)
- Distinguishes active vs. archived reference docs

**archive/experimental/README.md**:
- Purpose: document tried but not adopted approaches
- Rationale for keeping experimental work
- Currently empty (will be populated as needed)

## Rationale

### Why Archive TCP Phase 1A Preflight?

**Completion Status**:
- TCP Phase 1A/1B/3 fully implemented and merged (PRs #327, #329, #330)
- 150+ integration tests passing
- 13 verbs in production use
- Network server platform ready

**Document Purpose**:
- Pre-flight checklist was for **starting** Phase 1A implementation
- All checklist items completed successfully
- Document now serves as **historical reference** only

**Value Preserved**:
- Kept in archive for understanding planning process
- Shows pre-implementation verification approach
- Useful for future similar workstreams

### Why Archive Hierarchical OPML Design?

**Completion Status**:
- Design fully implemented (PR #326)
- Tool (`tools/export_tests_to_opml.py`) in production use
- 26-file structure operational
- Merge conflicts eliminated as designed

**Document Purpose**:
- Design document was for **proposing** the approach
- Implementation complete and stable
- Document now serves as **historical reference** only

**Value Preserved**:
- Explains rationale for current structure
- Documents design decisions
- Useful for understanding why structure exists

## Impact

### Improved Navigation

**Before**:
- Mixture of active and completed planning in same directories
- No clear archive structure
- Hard to distinguish current work from historical context

**After**:
- Clear separation: active planning vs. completed work
- Structured archive with purpose-specific subdirectories
- Easy to find current work (INDEX.md, _CURRENT_STATUS.md)
- Easy to find historical context (archive/ with READMEs)

### Better Context

**Before**:
- Completion status unclear for TCP Phase 1A/1B/3
- No obvious place for completed planning docs
- Archive structure ad-hoc

**After**:
- TCP completion status explicit (✅ COMPLETE in multiple places)
- Clear archive categories (workstreams, reference, experimental)
- Comprehensive READMEs explain archive purpose and navigation

### Cleaner Active Planning

**Before**:
- 180+ files in planning/ (mixture of active and archived)
- Obsolete docs mixed with current work

**After**:
- Active planning clearly marked
- Completed work properly archived
- Archive structure scalable for future work

## Future Maintenance

### When to Archive Documents

**Archive when**:
- ✅ Workstream fully complete and merged
- ✅ Implementation stable in production
- ✅ Document primarily historical reference

**Keep active when**:
- ❌ Work ongoing (even if partially complete)
- ❌ Document actively referenced for next phases
- ❌ Contains architecture/patterns being refined

### Archive Workflow

1. Complete work and merge to develop
2. Move planning doc to appropriate archive subdirectory
3. Update archive README with completion context
4. Update active planning docs to remove obsolete references
5. Add cross-references where historical context is relevant

## Files Modified

### Created:
- `planning/archive/completed-workstreams/README.md`
- `planning/archive/reference/README.md`
- `planning/archive/experimental/README.md`
- `planning/REORGANIZATION_2026_01_25.md` (this file)

### Modified:
- `planning/INDEX.md` - Comprehensive update for completion status
- `planning/README.md` - Updated status and structure
- `planning/archive/README.md` - Complete rewrite with navigation
- `planning/phase4/networking/INDEX.md` - Marked phases complete

### Moved:
- `planning/HIERARCHICAL_OPML_DESIGN.md` → `archive/completed-workstreams/`
- `planning/phase4/networking/TCP_PHASE1A_PREFLIGHT.md` → `archive/completed-workstreams/`

## Next Steps

This reorganization establishes patterns for future archive work:

1. **TCP Phase 2 planning** - Will move to archive when Phase 2 complete
2. **Thread testing Phase 2** - Will move to archive when Phase 2 complete
3. **Reference material** - Can be moved to `archive/reference/` as superseded
4. **Experimental approaches** - Can be moved to `archive/experimental/` as tried

## Document History

- **Created**: 2026-01-25
- **Purpose**: Document planning directory reorganization
- **Context**: After TCP Phase 1A/1B/3 completion, hierarchical OPML implementation
- **Next Review**: When next major workstream completes
