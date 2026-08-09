# Phase 4: Global State Elimination - Progress Tracker

**Last Updated**: 2026-01-13
**Current Phase**: Not Started
**Overall Status**: 0 / 200+ globals eliminated

---

## Status correction (2026-08-09)

The tracker below is stale. It says P0a is "Not Started" with "0 / 10 globals eliminated"; that is
wrong. Corrections of record, verified against git history:

- **`currenthashtable` is thread-local**, migrated in PR #536 (`f46fa7abd`, 2026-04-15). It now
  resolves through `hthreadglobals` (`Common/headers/processinternal.h:312`).
- **`hashtablestack` remains a global.** Its thread-local macro is deliberately commented out at
  `Common/headers/processinternal.h:295` because bootstrap runs before `hthreadglobals` is created
  and needs direct access to the stack.
- **This split is the live problem, not an oversight.** One half of the hash-table context is
  thread-local and the other half is not; that split-brain is what produced bug **#706**. Anyone
  resuming P0a should treat resolving it — not starting from zero — as the task.

Direction of record for current work is `product/VISION_1_0.md` plus
`product/plans/2026-08-09-phase-0-1-execution-plan.md`. The week-by-week timeline below was never
executed as written and should be read as an original plan, not as status.

Test-baseline note: the "Testing Status" table below claims a clean baseline. The current
integration baseline is approximately **2,186 passing / 21 known failures / 47 skipped** under
umbrella issue #620 (eval-trap unmasking, PRs #618/#619). The known-failures list currently lives
only in `/tmp`; persisting it into the repo is scheduled in Phase 2 of the product plan.

---

## Timeline Overview

| Phase | Timeline | Status | Milestone |
|-------|----------|--------|-----------|
| **P0a** | Weeks 1-3 | ⏸️ Not Started | Critical Thread-Safety |
| **P0b** | Weeks 4-6 | ⏸️ Not Started | **LAUNCH READY** 🚀 |
| **P1a** | Weeks 7-9 | ⏸️ Not Started | Multi-User Foundation |
| **P1b** | Weeks 10-12 | ⏸️ Not Started | **MULTI-USER READY** 🎉 |
| **P2** | Weeks 13-18 | ⏸️ Not Started | **COMPLETE** 🔥 |

---

## Phase P0a: Critical Thread-Safety (Weeks 1-3)

**Goal**: Eliminate globals causing immediate thread-safety violations

| Week | Focus | PR | Status | Globals Fixed |
|------|-------|----|----|---------------|
| **Week 1** | Hash Table Context | PR #XXX | ⏸️ Not Started | 0 / 3 |
| **Week 2** | Parser State | PR #XXX | ⏸️ Not Started | 0 / 3 |
| **Week 3** | Control Flow & Error State | PR #XXX | ⏸️ Not Started | 0 / 4 |

**P0a Totals**: 0 / 10 globals eliminated

---

## Phase P0b: Launch Requirements (Weeks 4-6)

**Goal**: Complete launch requirements, establish system context

| Week | Focus | PR | Status | Globals Fixed |
|------|-------|----|----|---------------|
| **Week 4** | System Context Structure | PR #XXX | ⏸️ Not Started | Foundation |
| **Week 5** | System Table Migration | PR #XXX | ⏸️ Not Started | 0 / 20+ |
| **Week 6** | Shell Config & Dialog State | PR #XXX | ⏸️ Not Started | 0 / 5 |

**P0b Totals**: 0 / 25+ globals eliminated

**Milestone**: 🚀 **LAUNCH READY** (P0 Complete)

---

## Phase P1a: Multi-User Foundation (Weeks 7-9)

**Goal**: Add reference counting for multi-user object sharing

| Week | Focus | PR | Status | Objects Updated |
|------|-------|----|----|-----------------|
| **Week 7** | Outline Reference Counting | PR #XXX | ⏸️ Not Started | Outlines |
| **Week 8** | Hash Table Reference Counting | PR #XXX | ⏸️ Not Started | Hash Tables |
| **Week 9** | System Context Lifecycle | PR #XXX | ⏸️ Not Started | System Context |

**P1a Totals**: 0 / 3 object types with refcounting

---

## Phase P1b: Multi-User Complete (Weeks 10-12)

**Goal**: Stress test concurrent operations, prepare for collaborative ODB

| Week | Focus | Deliverable | Status |
|------|-------|-------------|--------|
| **Week 10** | Concurrent Stress Tests | PR #XXX | ⏸️ Not Started |
| **Week 11** | Lock State Design | ADR-009 | ⏸️ Not Started |
| **Week 12** | Multi-User Documentation | Docs | ⏸️ Not Started |

**Milestone**: 🎉 **MULTI-USER READY** (P1 Complete)

---

## Phase P2: Comprehensive Cleanup (Weeks 13-18)

**Goal**: Eliminate remaining globals, complete state elimination

| Week | Focus | PR | Status | Globals Fixed |
|------|-------|----|----|---------------|
| **Week 13-14** | Static Buffer Audit | PR #XXX | ⏸️ Not Started | 0 / 40+ |
| **Week 15-16** | Cache Thread-Safety | PR #XXX | ⏸️ Not Started | 0 / 10+ |
| **Week 17** | Final Global Audit | PR #XXX | ⏸️ Not Started | 0 / 70+ |
| **Week 18** | Documentation & Milestone | Docs | ⏸️ Not Started | Complete |

**P2 Totals**: 0 / 120+ globals eliminated

**Milestone**: 🔥 **GLOBAL STATE ELIMINATION COMPLETE** (Phase 4 Complete)

---

## Current Week Status

**Week**: Not Started
**Focus**: N/A
**PR**: N/A
**Blockers**: None

**Next Steps**:
1. User review and approval of Phase 4 plan
2. Create feature branch: `feature/phase4-p0a-week1`
3. Begin P0a Week 1: Hash Table Context Migration

---

## Overall Metrics

**Globals Eliminated**: 0 / 200+ (0%)

**Progress by Category**:
- P0 (Launch-Blocking): 0 / 55 (0%)
- P1 (Multi-User): Reference counting not started
- P2 (Cleanup): 0 / 120+ (0%)

**PRs Merged**: 0 / 13+

**Files Modified**: 0 / 60+

---

## Testing Status

**Test Suite Pass Rate**: Baseline (100%)

| Test Type | Status | Notes |
|-----------|--------|-------|
| Unit Tests | ✅ Passing | Baseline |
| Integration Tests | ✅ Passing | 600+ tests |
| Thread Sanitizer | ⏸️ Not Run | Will run after P0a |
| Stress Tests | ⏸️ Not Created | Created in P1b |
| Leak Detection | ⏸️ Not Run | Will run in P1 |

---

## Blockers & Issues

**Current Blockers**: None

**Open Questions**:
- (Add questions as they arise)

**Risks**:
- See [00-overview/risks.md](00-overview/risks.md)

---

## Recent Updates

### 2026-01-13: Phase 4 Planning Complete
- Created Phase 4 structure and documentation
- Sub-phases P0a, P0b, P1a, P1b, P2 defined
- Ready to begin P0a Week 1
- Awaiting user review and approval

---

## Notes

**Update Frequency**: Weekly (after each PR merge)

**Tracking Issues**:
- Umbrella: #XXX (to be created)
- Sub-phases: #XXX (P0a), #XXX (P0b), etc.

**Labels**: `phase4`, `P0-launch-blocking`, `P1-multi-user`, `P2-cleanup`, `global-state`

---

**Navigation**: [← Back to INDEX](INDEX.md)
