# Phase 4: Global State Elimination

**Status**: Active Planning
**Timeline**: 12-18 weeks (Weeks 1-18)
**Strategic Priority**: P0 (Launch-Blocking for thread-safety)

---

## Mission

**Eliminate ALL global mutable state from Frontier runtime to achieve thread-safety required for launch and enable collaborative ODB editing (Frontier 2.0).**

---

## Overview

**What We're Fixing**: 200+ global variables that create race conditions and block concurrent operations.

**Why It Matters**:
- **Launch Requirement**: Thread-safety for Automattic partnership
- **Strategic Foundation**: Enables collaborative ODB (Frontier 2.0)
- **Technical Debt**: Eliminates cascading failures from contaminated global state

**How We're Doing It**:
1. **Thread-Local Storage** - Per-thread execution state (proven in ADR-005, ADR-006)
2. **Explicit Context Structures** - Per-operation state (proven in db_context pattern)
3. **Reference Counting** - Shared object lifecycle for multi-user

---

## Quick Start

**New to Phase 4?** Start here:
1. Read **[00-overview/README.md](00-overview/README.md)** - Architecture and patterns
2. Pick a sub-phase based on your focus:
   - **Launch-blocking?** → P0a or P0b
   - **Multi-user prep?** → P1a or P1b
   - **Cleanup work?** → P2

**Implementing a migration?**
1. Read the sub-phase README for your work
2. Use **[00-overview/quick_reference.md](00-overview/quick_reference.md)** as checklist
3. Follow the proven patterns from ADR-005 and ADR-006

---

## Sub-Phases

### [00-overview/](00-overview/) - Architecture & Common Resources

**What's Here**: Architecture patterns, risk assessment, API compatibility analysis

**Read This First**: All sub-phases reference these common patterns.

**Key Documents**:
- [README.md](00-overview/README.md) - Architecture overview
- [quick_reference.md](00-overview/quick_reference.md) - Developer checklist
- [patterns.md](00-overview/patterns.md) - Thread-local and context patterns
- [risks.md](00-overview/risks.md) - Risk assessment and mitigation
- [api_compatibility.md](00-overview/api_compatibility.md) - UserTalk compatibility
- [alternatives.md](00-overview/alternatives.md) - Rejected approaches

---

### [p0a-critical-thread-safety/](p0a-critical-thread-safety/) - Weeks 1-3 (LAUNCH BLOCKING)

**Goal**: Eliminate globals causing immediate thread-safety violations

**Scope**: Hash table context, parser state, control flow flags (30 globals)

**Deliverables**:
- Week 1: Hash table context (currenthashtable, hmagictable)
- Week 2: Parser state (yylval, yyval, langparser_result)
- Week 3: Control flow (flbreak, flcontinue, error state)

**Success**: Multiple threads can execute UserTalk concurrently without race conditions

[→ Start P0a](p0a-critical-thread-safety/README.md)

---

### [p0b-launch-requirements/](p0b-launch-requirements/) - Weeks 4-6 (LAUNCH BLOCKING)

**Goal**: Complete launch requirements, establish system context pattern

**Scope**: System tables, shell configuration, dialog state (25 globals)

**Deliverables**:
- Week 4: System context structure (all system tables)
- Week 5: System table migration (remove ADR-008 workaround)
- Week 6: Shell config & dialog state (thread-safe UI configuration)

**Success**: No global mutable state blocks concurrent operations → **LAUNCH READY**

[→ Start P0b](p0b-launch-requirements/README.md)

---

### [p1a-multi-user-foundation/](p1a-multi-user-foundation/) - Weeks 7-9

**Goal**: Add reference counting for multi-user object sharing

**Scope**: Reference counting for outlines, hash tables, system context

**Deliverables**:
- Week 7: Outline reference counting (op_outline_retain/release)
- Week 8: Hash table reference counting
- Week 9: System context lifecycle (thread-local caching)

**Success**: Objects safely shared across threads, no dangling pointers

[→ Start P1a](p1a-multi-user-foundation/README.md)

---

### [p1b-multi-user-complete/](p1b-multi-user-complete/) - Weeks 10-12

**Goal**: Stress test concurrent operations, prepare for collaborative ODB

**Scope**: Concurrent stress testing, lock state design

**Deliverables**:
- Week 10: Concurrent stress tests (10+ threads)
- Week 11: Lock state design (CRDT foundation)
- Week 12: Multi-user architecture documentation

**Success**: Thread-safe under concurrent load → **MULTI-USER READY**

[→ Start P1b](p1b-multi-user-complete/README.md)

---

### [p2-comprehensive-cleanup/](p2-comprehensive-cleanup/) - Weeks 13-18

**Goal**: Eliminate remaining globals, complete state elimination

**Scope**: Static buffers, caches, final audit (145 globals)

**Deliverables**:
- Week 13-14: Static buffer audit and migration
- Week 15-16: Cache thread-safety
- Week 17: Final global audit
- Week 18: Documentation and milestone celebration

**Success**: Zero global mutable state (except justified shared state) → **COMPLETE** 🎉

[→ Start P2](p2-comprehensive-cleanup/README.md)

---

## Timeline & Dependencies

```
P0a (Weeks 1-3)  ─────────┐
                          ├──→ LAUNCH READY
P0b (Weeks 4-6)  ─────────┘

P1a (Weeks 7-9)  ─────────┐
                          ├──→ MULTI-USER READY
P1b (Weeks 10-12) ────────┘

P2 (Weeks 13-18) ─────────────→ COMPLETE
```

**Parallelization Opportunities**:
- P0a and P0b can overlap (different code areas)
- P1a can start before P0b complete (independent work)
- P2 work can proceed incrementally alongside P1

---

## Progress Tracking

**Live Status**: See [PROGRESS.md](PROGRESS.md) (updated weekly)

**GitHub Issues**:
- Umbrella: #XXX - Phase 4: Global State Elimination
- Sub-phases: #XXX (P0a), #XXX (P0b), #XXX (P1a), #XXX (P1b), #XXX (P2)

**Labels**: `phase4`, `P0-launch-blocking`, `P1-multi-user`, `P2-cleanup`, `global-state`

---

## Success Criteria

**P0 Complete (Week 6)**:
- ✅ No race conditions on critical globals
- ✅ Multiple threads execute UserTalk concurrently
- ✅ All tests pass (unit + integration + thread sanitizer)
- ✅ **LAUNCH READY**

**P1 Complete (Week 12)**:
- ✅ Reference counting working for all shared objects
- ✅ Concurrent stress tests pass (10+ threads)
- ✅ Foundation ready for collaborative ODB
- ✅ **MULTI-USER READY**

**P2 Complete (Week 18)**:
- ✅ No global mutable state (except justified cases)
- ✅ All static buffers thread-safe
- ✅ Comprehensive documentation
- ✅ **GLOBAL STATE ELIMINATION COMPLETE** 🔥

---

## Related Documentation

**ADRs**:
- [ADR-005](../../architectural_decision_records/ADR-005-parameter-state-thread-safety.md) - Thread-local pattern template
- [ADR-006](../../architectural_decision_records/ADR-006-outline-context-stack-refactoring.md) - Outline context example
- [ADR-008](../../architectural_decision_records/ADR-008-efp-workaround-processor-table-global.md) - Workaround to remove in P0b

**Project Docs**:
- [CLAUDE.md](../../CLAUDE.md) - "BURN THE GLOBALS WITH FIRE" context
- [Issue #135](https://github.com/jsavin/Frontier/issues/135) - Outline context refactoring
- [Issue #262](https://github.com/jsavin/Frontier/issues/262) - currenthashtable thread-safety

---

**Last Updated**: 2026-01-13
**Next Review**: After P0a Week 1 completion
**Status**: Ready to begin P0a Week 1
