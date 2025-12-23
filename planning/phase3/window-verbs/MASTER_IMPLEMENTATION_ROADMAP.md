# Master Implementation Roadmap
## Headless Completeness & Code Cleanup

## Status
- State: Ready to Begin Implementation
- Phase: 3
- Last Updated: 2025-12-21
- Notes: Coordinates dead code removal and headless verb implementation workstreams

**Created**: 2025-12-21
**Purpose**: Overall sequencing and coordination of all implementation work
**Status**: Ready to begin

---

## Executive Summary

This roadmap coordinates two parallel workstreams:
1. **Dead Code Removal** (Entry-level / Haiku) - Remove explicit dead code markers
2. **Headless Verb Implementation** (Mid-level / Sonnet) - Make key verbs work headless

**Total Duration**: 4-5 weeks
**Team Composition**: 1 entry-level + 1 mid-level engineer (or sequential work)

---

## Work Packages Overview

| Package | Complexity | Duration | Can Start | Blocks |
|---------|-----------|----------|-----------|--------|
| **WP1**: Dead Code Phase 1 | ⭐ Haiku | 1 week | Immediately | Nothing |
| **WP2**: Dead Code Phase 2 | ⭐ Haiku | 1 week | After WP1 | Nothing |
| **WP3**: Window Verbs Phase 1-2 | ⭐⭐ Sonnet | 1 week | Immediately | Nothing |
| **WP4**: Window Verbs Phase 3-5 | ⭐⭐⭐ Sonnet | 2 weeks | After WP3 | Nothing |
| **WP5**: Menu Verbs (Optional) | ⭐⭐⭐ Sonnet | 1-2 weeks | After WP4 | User approval |
| **WP6**: Documentation | ⭐ Haiku | 0.5 weeks | After all | WP1-5 |

---

## Parallel Work Strategy

### Option A: Two Engineers (Fastest - 3 weeks)

**Week 1**:
- **Haiku**: WP1 - Dead Code Phase 1 (xxx blocks, OBSOLETE, NEVER)
- **Sonnet**: WP3 - Window Verbs Phase 1-2 (target mgmt, simple properties)

**Week 2**:
- **Haiku**: WP2 - Dead Code Phase 2 (oldMACVERSION, WIN95VERSION)
- **Sonnet**: WP4 - Window Verbs Phase 3-4 (ODB properties, getFile)

**Week 3**:
- **Haiku**: Assist with testing, start WP6 documentation
- **Sonnet**: WP4 - Window Verbs Phase 5 (menu script detection, noops)

**Week 4** (if menu verbs approved):
- **Haiku**: WP6 - Documentation
- **Sonnet**: WP5 - Menu Verbs Implementation

---

### Option B: One Engineer (Sequential - 5 weeks)

**Weeks 1-2**: Entry-level work (Haiku)
- Week 1: WP1 - Dead Code Phase 1
- Week 2: WP2 - Dead Code Phase 2

**Weeks 3-5**: Mid-level work (Sonnet or advanced Haiku)
- Week 3: WP3 - Window Verbs Phase 1-2
- Week 4: WP4 - Window Verbs Phase 3-4
- Week 5: WP4 completion + WP6 documentation

---

## Detailed Work Package Descriptions

### WP1: Dead Code Removal Phase 1

**Goal**: Remove all xxx-prefixed blocks and explicit dead markers

**Guide**: `DEAD_CODE_REMOVAL_IMPLEMENTATION_GUIDE.md`

**Complexity**: ⭐ Entry-level (Haiku)

**Duration**: 1 week (3-4 days actual work + buffer)

**Tasks**:
1. Inventory all xxx-prefixed blocks
2. Remove each block individually (test after each)
3. Remove OBSOLETE block (whirlpool.c)
4. Remove NEVER block (langevaluate.c)
5. Verify NeverDefine_For_Reference is kept
6. Create PR

**Deliverables**:
- ~1,370 lines of code removed
- 16 dead code blocks eliminated
- PR merged to develop

**Success Criteria**:
- [ ] Build succeeds
- [ ] All tests pass
- [ ] No remaining xxx-prefixed blocks
- [ ] Migration test passes

---

### WP2: Dead Code Removal Phase 2

**Goal**: Remove obsolete platform code

**Guide**: `DEAD_CODE_REMOVAL_IMPLEMENTATION_GUIDE.md`

**Complexity**: ⭐ Entry-level (Haiku)

**Duration**: 1 week (2-3 days actual work + buffer)

**Tasks**:
1. Remove oldMACVERSION blocks (Mac Classic)
2. Remove commented WIN95VERSION blocks
3. Verify active WIN95VERSION block is kept
4. Create PR

**Deliverables**:
- ~540 lines of code removed
- 5 obsolete platform blocks eliminated
- PR merged to develop

**Success Criteria**:
- [ ] Build succeeds
- [ ] All tests pass
- [ ] Migration test passes
- [ ] No oldMACVERSION blocks remain
- [ ] Active WIN95VERSION block preserved

---

### WP3: Window Verbs Implementation Phase 1-2

**Goal**: Implement simple window verbs (target mgmt, properties, noops)

**Guide**: `HEADLESS_WINDOW_VERBS_IMPLEMENTATION_GUIDE.md`

**Complexity**: ⭐⭐ Mid-level (Sonnet)

**Duration**: 1 week

**Tasks**:
1. **Phase 1**: Target management (open, close)
2. **Phase 2**: Simple properties (getTitle, msg, error verbs)
3. **Phase 6**: Noop implementations (display verbs)
4. Test all implemented verbs
5. Create PR

**Deliverables**:
- window.open/close working
- window.getTitle working
- window.msg working
- Error verbs returning appropriate messages
- All noop verbs implemented
- ~200 lines of new code

**Success Criteria**:
- [ ] window.open sets target
- [ ] window.close clears target
- [ ] window.getTitle returns object name
- [ ] window.msg outputs to stdout
- [ ] All noop verbs don't crash
- [ ] Manual test suite passes

**Dependencies**: None (can start immediately)

---

### WP4: Window Verbs Implementation Phase 3-5

**Goal**: Implement complex window verbs (ODB properties, database file path, menu script detection)

**Guide**: `HEADLESS_WINDOW_VERBS_IMPLEMENTATION_GUIDE.md`

**Complexity**: ⭐⭐⭐⭐ Mid-level to Advanced (Sonnet)

**Duration**: 2 weeks

**Tasks**:
1. **Phase 3**: Research ODB property flags
2. **Phase 3**: Implement isReadOnly, isModified, setModified
3. **Phase 4**: Research database file path lookup
4. **Phase 4**: Implement getFile ⚠️ **CRITICAL**
5. **Phase 5**: Implement isMenuScript
6. Comprehensive testing
7. Create PR

**Deliverables**:
- ODB property verbs working (3 verbs)
- window.getFile working (critical)
- window.isMenuScript working
- Research documents for future reference
- ~200 lines of new code

**Success Criteria**:
- [ ] window.isReadOnly checks ODB flags
- [ ] window.isModified checks dirty state
- [ ] window.setModified updates dirty state
- [ ] window.getFile returns database path ⚠️
- [ ] window.isMenuScript detects menu scripts
- [ ] All tests pass

**Dependencies**: WP3 (builds on target management)

---

### WP5: Menu Verbs Implementation (OPTIONAL)

**Goal**: Enable menu data manipulation in headless mode

**Guide**: `HEADLESS_MENU_OPERATIONS_PLAN.md` (in planning/phase3/)

**Complexity**: ⭐⭐⭐ Mid-level (Sonnet)

**Duration**: 1-2 weeks

**Status**: **CONDITIONAL** - Awaiting user approval

**Decision Point**: Do UserTalk scripts need to manipulate menubarType objects in headless mode?

**Tasks** (if approved):
1. Provide headless menudata globals
2. Add menuverbs.c to headless build
3. Wrap display-only verbs with #ifdef guards
4. Implement menu data manipulation verbs
5. Test menu creation and manipulation
6. Create PR

**Deliverables**:
- menu.addMenuCommand working
- menu.getScript/setScript working
- menu.addSubMenu working
- ~2,400 lines now compiled in headless
- 8 menu verbs functional

**Success Criteria**:
- [ ] Can create menubarType objects
- [ ] Can add menu commands programmatically
- [ ] Can get/set menu item scripts
- [ ] Menu objects persist to database
- [ ] Display verbs properly stubbed

**Dependencies**: WP4 (requires understanding of external variables)

**Hold Point**: Don't start until user approves

---

### WP6: Documentation & Cleanup

**Goal**: Document all changes, update guides, create completion report

**Complexity**: ⭐ Entry-level (Haiku)

**Duration**: 0.5 weeks (2-3 days)

**Tasks**:
1. Update CONTRIBUTING.md with:
   - Dead code removal summary
   - Headless verb availability matrix
   - Build system architecture (Makefile exclusions)
2. Create verb behavior comparison table (GUI vs headless)
3. Update _CURRENT_STATUS.md
4. Archive planning docs to _STATUS_ARCHIVE.md
5. Create completion report

**Deliverables**:
- Updated documentation
- Verb availability matrix
- Completion report
- Lessons learned document

**Dependencies**: All previous work packages

---

## Daily Standup Template

**For coordination, daily update**:

```
Date: YYYY-MM-DD
Engineer: [Name] ([Haiku/Sonnet])
Work Package: WPX

Yesterday:
- Completed: [specific tasks]
- Commits: [links to commits]

Today:
- Plan: [specific tasks for today]
- Expected commits: [N]

Blockers:
- [None] OR [describe blocker, escalation plan]

Tests Status:
- Build: [✅ passing / ❌ failing]
- Headless tests: [✅ passing / ❌ failing]
- Manual tests: [N/N passed]
```

---

## Integration Points & Handoffs

### Haiku → Sonnet Handoff (after WP1)

**Haiku deliverables**:
- [ ] All xxx blocks removed
- [ ] OBSOLETE/NEVER blocks removed
- [ ] Branch merged to develop
- [ ] Progress doc updated

**Sonnet picks up**:
- Pulls latest develop
- Reviews completion summary
- Proceeds with WP3

---

### Sonnet → Haiku Handoff (after WP4)

**Sonnet deliverables**:
- [ ] All window verbs implemented
- [ ] Branch merged to develop
- [ ] Test suite passing
- [ ] Research docs created

**Haiku picks up**:
- Reviews implementation
- Starts documentation work (WP6)
- Creates verb availability matrix

---

## Testing Cadence

### Per-Commit Testing (Haiku)
After each dead code removal commit:
```bash
make -C frontier-cli clean && make -C frontier-cli
./frontier-cli/frontier-cli -e "1+1"  # Quick smoke test
```

### Daily Testing (Both)
At end of each day:
```bash
./tools/run_headless_tests.sh  # Full suite
```

### Weekly Testing (Both)
End of each week:
```bash
# Full regression
./tools/run_headless_tests.sh
make -C tests save_migration_tests && ./tests/save_migration_tests
cd tools/kernelverbs_parser && python3 cli.py analyze
```

---

## Risk Management

### Low-Risk Work (Haiku - WP1, WP2)

**If build fails**:
1. Restore file: `git checkout -- <file>`
2. Review what was removed
3. Try again more carefully
4. If stuck > 30 minutes, escalate

**If tests fail unexpectedly**:
1. Restore file
2. Document the failure
3. Escalate immediately (shouldn't happen with dead code)

---

### Medium-Risk Work (Sonnet - WP3, WP4)

**If implementation approach unclear**:
1. Spend max 4 hours researching
2. Document findings
3. Escalate with specific questions

**If tests fail**:
1. Debug locally
2. Use git bisect if needed
3. If stuck > 1 day, escalate

**If architecture issue discovered**:
1. Stop work immediately
2. Document the issue
3. Escalate to user

---

## Communication Protocols

### Haiku Engineer Questions

**Quick questions** (<5 min to answer):
- Ask in daily standup

**Research questions** (need investigation):
- Create question doc with:
  - What you're trying to do
  - What you've tried
  - Specific blocker
- Share with Sonnet engineer

**Blockers** (stuck >2 hours):
- Escalate immediately
- Don't spin wheels

---

### Sonnet Engineer Questions

**Architecture questions**:
- Document options
- Create decision doc
- Escalate to user

**Implementation questions**:
- Research first (max 4 hours)
- Document findings
- Propose approach
- Get approval before proceeding

---

## Success Metrics

### Code Cleanup Success
- [ ] ~1,900 lines of dead code removed
- [ ] 21 dead code blocks eliminated
- [ ] Build time improved
- [ ] No regressions
- [ ] Clean git history

### Headless Completeness Success
- [ ] 14 window verbs working headless
- [ ] window.getFile working (critical)
- [ ] Target management working
- [ ] ODB properties accessible
- [ ] All tests passing
- [ ] Documentation complete

### Optional: Menu Verbs Success
- [ ] 8 menu verbs working headless
- [ ] Menu objects persist correctly
- [ ] Display verbs properly stubbed

---

## Final Acceptance Criteria

Before marking entire project complete:

### Technical Criteria
- [ ] All work packages complete
- [ ] All tests passing
- [ ] No compiler warnings
- [ ] Migration test passes
- [ ] Verb binding check passes
- [ ] Build succeeds on both Intel and Apple Silicon

### Documentation Criteria
- [ ] CONTRIBUTING.md updated
- [ ] Verb availability matrix created
- [ ] _CURRENT_STATUS.md updated
- [ ] Completion report written
- [ ] Research docs archived

### Process Criteria
- [ ] All branches merged to develop
- [ ] Clean git history
- [ ] All PRs have descriptive messages
- [ ] Code follows existing patterns
- [ ] No technical debt introduced

---

## Estimated Timeline Summary

### Parallel Work (2 Engineers)
- **Week 1**: WP1 + WP3
- **Week 2**: WP2 + WP4 (start)
- **Week 3**: WP4 (complete) + testing
- **Week 4** (optional): WP5 (menu verbs if approved)
- **Total**: 3-4 weeks

### Sequential Work (1 Engineer)
- **Weeks 1-2**: WP1 + WP2 (dead code removal)
- **Weeks 3-5**: WP3 + WP4 (window verbs)
- **Week 6** (optional): WP5 (menu verbs if approved)
- **Total**: 5-6 weeks

---

## Getting Started Checklist

### Before Day 1

**Repository Setup**:
- [ ] Clone Frontier repository
- [ ] Checkout develop branch
- [ ] Run `make -C frontier-cli clean && make -C frontier-cli`
- [ ] Run `./tools/run_headless_tests.sh`
- [ ] Confirm all tests pass

**Documentation Review**:
- [ ] Read relevant implementation guide
- [ ] Review code patterns in existing files
- [ ] Understand git workflow
- [ ] Set up daily standup template

**Tools Setup**:
- [ ] ripgrep installed (`brew install ripgrep`)
- [ ] gh CLI installed (`brew install gh`)
- [ ] Editor configured
- [ ] Git configured with proper name/email

### Day 1 Morning

**Haiku Engineer**:
- [ ] Review `DEAD_CODE_REMOVAL_IMPLEMENTATION_GUIDE.md`
- [ ] Create inventory of xxx-prefixed blocks
- [ ] Create feature branch
- [ ] Start WP1 Task 1.2

**Sonnet Engineer**:
- [ ] Review `HEADLESS_WINDOW_VERBS_IMPLEMENTATION_GUIDE.md`
- [ ] Read prerequisite files
- [ ] Create feature branch
- [ ] Start WP3 Phase 1 (target management)

---

## Contact & Escalation

**Technical Questions**: User (via GitHub issues or direct contact)

**Architecture Decisions**: User approval required for:
- Menu verbs implementation (WP5)
- Any deviation from implementation guides
- Changes to API/behavior

**Daily Progress**: Update `planning/phase3/implementation/PROGRESS.md`

---

## Appendix: Quick Reference

### Key Files
- Master Roadmap: `planning/phase3/implementation/MASTER_IMPLEMENTATION_ROADMAP.md` (this file)
- Dead Code Guide: `planning/phase3/implementation/DEAD_CODE_REMOVAL_IMPLEMENTATION_GUIDE.md`
- Window Verbs Guide: `planning/phase3/implementation/HEADLESS_WINDOW_VERBS_IMPLEMENTATION_GUIDE.md`
- Progress Tracking: `planning/phase3/implementation/PROGRESS.md` (create on Day 1)

### Key Commands
```bash
# Build
make -C frontier-cli clean && make -C frontier-cli

# Test
./tools/run_headless_tests.sh

# Migration test
make -C tests save_migration_tests && ./tests/save_migration_tests

# Verb check
cd tools/kernelverbs_parser && python3 cli.py analyze

# Search code
rg '<pattern>' Common/source

# Create PR
gh pr create --title "..." --body "..."
```

### Complexity Levels
- ⭐ = Entry-level (Haiku)
- ⭐⭐ = Low-mid (Haiku with guidance)
- ⭐⭐⭐ = Mid-level (Sonnet)
- ⭐⭐⭐⭐ = Advanced (Sonnet)

---

*Roadmap created: 2025-12-21*
*Ready to begin implementation!*
