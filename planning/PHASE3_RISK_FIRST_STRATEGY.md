# Phase 3: Risk-First Implementation Strategy

**Date**: 2025-12-17
**Status**: Active Strategy
**Priority**: P0 (guides all Phase 3 work)
**Owner**: Team

---

## Executive Summary

Phase 3 implementation (kernel verb porting) is at a critical juncture: we've reached 68% verb coverage (479/707 verbs) with momentum from recent PRs. However, **implementing remaining verbs without addressing architectural risks first could waste weeks of work and create massive refactoring debt**.

**This document establishes a risk-first strategy**: Identify and resolve critical architectural decisions BEFORE implementing remaining verbs. This prevents building on faulty foundations.

---

## Why Risk-First Matters

### The Problem with Quick-Wins-First

**Tempting approach**: Implement 28 error stubs (3-4 hours) as a quick win to build momentum.

**Hidden risks**:
1. If #87 (file routing) is wrong, 86 file verbs need refactoring
2. If concurrency model is wrong, 19 thread verbs need rewriting
3. If TCP abstraction is poor, 23 networking verbs become painful to maintain
4. If migration still has bugs, all verification work was wasted

**Cost of discovery too late**:
- Quick win takes 4 hours
- But if discovered later that architecture is wrong: 40+ hours rework
- Net loss: 36+ hours

### The Payoff of Risk-First

**Upfront cost**: 5-6 hours of analysis, decisions, validation
- Verify migration works end-to-end (1 hour)
- Resolve #87 (file routing) (1-2 hours)
- Decide concurrency model (1 hour)
- Design TCP abstraction (1-2 hours)

**Payoff**:
- Prevents weeks of rework
- Remaining verb implementations proceed with confidence
- Technical decisions are documented for future developers
- Incremental work can be planned intelligently

---

## Critical Risks Assessment

### CRITICAL RISKS (Require immediate attention)

#### Risk #1: File Operations Routing (Issue #87 - P0 Blocker)

**Description**: During v6→v7 migration, how should file operations route to correct database?

**Why it matters**:
- 86 file verbs affected
- Silent data corruption possible if wrong
- Affects all file I/O operations

**Current state**:
- Issue #87 OPEN but not yet reviewed
- File operations partially working but routing unclear
- Risk: Implementation proceeds on faulty assumptions

**Mitigation**:
- [ ] Get system architect to review #87
- [ ] Document the routing decision
- [ ] Implement file routing correctly BEFORE verb implementation
- [ ] Test file operations with both v6 and v7 databases

**Timeline**: 1-2 hours decision + review

---

#### Risk #2: Concurrency Model (Blocks 19 verbs)

**Description**: Should thread/semaphore verbs use real threading or single-threaded stubs?

**Why it matters**:
- 17 thread.* verbs + 2 semaphore.* verbs depend on this
- Wrong choice = massive refactoring later
- Affects scripts using concurrent operations

**Current state**:
- Verbs are stubbed waiting for decision
- No concurrency model yet defined
- Risk: Implement as simple stubs, then need real threading later = rework

**Options**:
1. **Single-threaded stubs** (2-3 hours)
   - Verbs fail gracefully with "threading not supported"
   - Upgrade to real threading in Phase 4+
   - Pros: Low cost now, clear upgrade path
   - Cons: Scripts using threads won't work

2. **Real threading with Go-style goroutines** (2+ weeks)
   - Proper thread support, high complexity
   - Pros: Full compatibility
   - Cons: Major effort, architectural complexity

3. **Cooperative multitasking** (1-2 weeks)
   - Lightweight, portable, but limited concurrency
   - Pros: Works well for simple parallelism
   - Cons: Complex to implement correctly

**Mitigation**:
- [ ] Evaluate options with team
- [ ] Make explicit choice (likely option 1 for now)
- [ ] Document decision in ADR-002
- [ ] Implement stubs with clear upgrade path

**Timeline**: 1 hour decision

---

#### Risk #3: Database Migration Actually Works (Issue #116 - Fixed?)

**Description**: We closed #116, but have we actually validated end-to-end migration?

**Why it matters**:
- Migration is foundation for v7 database operations
- External objects could still be broken in subtle ways
- Silent failures possible

**Current state**:
- Issue #116 marked as fixed (hardening in commit 2bfc4cb0)
- But no end-to-end validation run
- Risk: Migration still broken in production scenarios

**Evidence we have**:
- ✅ Hash pack/unpack hardened
- ✅ String bounds checking added
- ✅ External handle fix applied (PR #117)
- ❓ But: No functional test of migrated database with real verbs

**Mitigation**:
- [ ] Run full migration: v6 → v7
- [ ] Test external object access:
  - `sizeOf(system.verbs.globals)`
  - `workspace.test="hello"; workspace.test`
  - Table operations
  - Script access
- [ ] Verify source database unchanged (md5sum before/after)
- [ ] Document results

**Timeline**: 30 minutes - 1 hour

---

### MEDIUM RISKS (Important, but not blocking)

#### Risk #4: TCP Socket Abstraction Design

**Description**: How should networking verbs abstract socket operations?

**Why it matters**:
- 23 tcp.* verbs depend on this
- Future networking features will build on it
- Poor abstraction = painful to maintain

**Current state**:
- TCP verbs are stubbed
- No abstraction layer designed
- Risk: Implement verbs, then realize design is wrong

**Mitigation**:
- [ ] Sketch `network_portable.h` interface
- [ ] Review for extensibility
- [ ] Document design rationale
- [ ] Implement socket layer before TCP verbs

**Timeline**: 1-2 hours

---

#### Risk #5: Carbon Migration Incremental Strategy

**Description**: How to incrementally clean up Carbon as verbs are ported?

**Why it matters**:
- Technical debt could compound quickly
- Future work gets harder without cleanup
- Risk: Carbon removal becomes massive refactoring later

**Current state**:
- Carbon migration deferred to P2 (Phases 2-5)
- No plan for incremental cleanup
- Risk: Never happens, technical debt grows

**Mitigation**:
- [ ] Map which verb implementations trigger Carbon cleanups
- [ ] Create backlog of incremental cleanup tasks
- [ ] Schedule cleanup alongside verb porting
- [ ] Example: When string.* verbs done, do TEC removal

**Timeline**: 1-2 hours planning

---

## Risk-First Implementation Roadmap

### Phase 1: Risk Mitigation (This Week)

**Goal**: Resolve all critical architectural risks before scaling verb implementation

**Tasks** (in priority order):

1. **Validate Migration Works** (30 min - 1 hour)
   - Run: `make -C frontier-cli && ./frontier-cli --migrate databases/Frontier-v6.root databases/test-v7.root`
   - Test external access: `FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli --system-root databases/test-v7.root -e "sizeOf(system.verbs.globals)"`
   - Verify source unchanged: `md5sum databases/Frontier-v6.root` (before and after)
   - Document results
   - **Issue**: None specific (validation task)

2. **Resolve Issue #87 - File Routing Decision** (1-2 hours)
   - Get system architect review
   - Document routing architecture
   - Identify implementation requirements
   - **Blocker**: Can't implement file verbs without this
   - **Issue**: #87

3. **Decide Concurrency Model** (1 hour)
   - Team discussion: Real threading vs. stubs?
   - Document choice
   - Create ADR-002 if major decision
   - **Blocker**: Guides 19 verb implementations
   - **Related Issue**: #94 (P1)

4. **Design TCP Socket Abstraction** (1-2 hours)
   - Sketch `network_portable.h`
   - Review extensibility
   - Document design rationale
   - **Blocker**: Guides 23 networking verb implementations
   - **Related Issue**: #122

5. **Plan Incremental Carbon Cleanup** (1-2 hours)
   - Map verbs → Carbon cleanups
   - Create tasks in GitHub
   - Schedule alongside verb work
   - **Issue**: #120

**Total Phase 1**: ~5-7 hours (one solid day of focused work)

---

### Phase 2: Implementation (Following Weeks)

**Now that risks are mitigated**, implement with confidence:

1. **Implement 28 Error Stubs** (3-4 hours)
   - clock.set, dialog.*, statusbar.*, window.*, OSA stubs
   - **Issue**: #121

2. **Implement TCP Networking** (6-8 hours)
   - Use socket abstraction designed in Phase 1
   - 23 tcp.* verbs
   - **Issue**: #122

3. **Implement Remaining Verbs** (ongoing)
   - Script.* (4-6 hours)
   - Table.* edge cases (2-3 hours)
   - Launch.* (3-4 hours)

4. **Incremental Carbon Cleanup**
   - As planned in Phase 1
   - Parallel with verb work

---

## Decision Matrix

| Risk | Decision Needed | Owner | Deadline | Issue |
|------|-----------------|-------|----------|-------|
| File routing (#87) | Architectural approach | System architect | This week | #87 |
| Concurrency model | Real vs. stubs | Team | This week | #94 |
| TCP abstraction | Interface design | Lead architect | This week | #122 |
| Migration validation | Works end-to-end? | Engineer | This week | None |
| Carbon cleanup plan | Incremental strategy | Team | This week | #120 |

---

## Success Criteria

**Phase 1 Complete When**:
- ✅ Migration end-to-end validated (external access works)
- ✅ Issue #87 reviewed and decision documented
- ✅ Concurrency model chosen and rationale recorded
- ✅ TCP abstraction designed and reviewed
- ✅ Carbon cleanup plan created
- ✅ All risks have mitigation plans

**Then**: Proceed to Phase 2 implementation with confidence

---

## Risk Escalation

If any risk cannot be resolved this week:

1. **Document the blocker** in GitHub issue
2. **Mark issue as `status/blocked` and `status/decision-needed`**
3. **Create follow-up issue** for when decision is available
4. **Don't proceed** with related verb implementation until resolved

Better to slow down now than ship on faulty foundations.

---

## Related Documents

- `planning/architectural_decision_records/ADR-001-multi-database-context.md` - Multi-database architecture
- `planning/phase3/database_architecture/MULTI_DATABASE_PREVENTION_STRATEGY.md` - Database safety strategy
- `planning/phase3/kernel_verb_porting/automatic_verb_binding_phase3_plan.md` - Verb implementation details
- `planning/phase3/carbon_migration/README.md` - Carbon removal strategy (deferred, P2)

---

## Appendix: Quick Reference

### Critical Issues to Resolve

- [ ] **#87** - Headless EFP routing (file operations)
- [ ] **#94** - Concurrency model decision
- [ ] **#122** - TCP socket abstraction design
- [ ] **#120** - Carbon cleanup plan

### Validation Tasks

- [ ] Migration produces working v7 database
- [ ] External objects accessible post-migration
- [ ] Source database unchanged
- [ ] File operations routing correct

### Architecture Decisions to Document

- [ ] File routing strategy (ADR or decision log)
- [ ] Concurrency model choice (ADR-002?)
- [ ] TCP abstraction design (architecture doc)
- [ ] Carbon cleanup roadmap (linked to #120)

---

**This strategy prioritizes foundation over features. Solid architecture now enables confident, rapid implementation later.**
