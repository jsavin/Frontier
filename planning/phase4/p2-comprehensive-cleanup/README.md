# P2: Comprehensive Cleanup (Weeks 13-18)

**Status**: Cleanup & Completion
**Timeline**: 6 weeks
**Goal**: Eliminate remaining globals, complete state elimination

---

## Scope

**What We're Cleaning Up**: 120+ remaining P2 globals

**Categories**:
- Static buffers in verb processors
- Caches (font cache, error cushions, memory cushions)
- Remaining stacks (graphics state, quickdraw)
- Final audit of any missed globals

**Why P2**: Complete the mission - zero global mutable state (except justified shared state).

---

## Model Selection Guide

Each phase uses the appropriate Claude model based on task complexity:

| Weeks | Scope | Complexity | Model | Rationale |
|-------|-------|------------|-------|-----------|
| **13-14** | Static Buffer Audit | Medium | 🟡 Sonnet | Audit and decision framework, migration strategy |
| **15-16** | Cache Thread-Safety | Medium | 🟡 Sonnet | Performance-critical, requires benchmarking and tradeoffs |
| **17** | Final Global Audit | Low | 🟢 Haiku | Pattern-following, comprehensive checklist execution |
| **18** | Documentation & Milestone | Low | 🟢 Haiku | Documentation writing, celebration content |

**Using This Guide**:
- **🟢 Haiku**: Audit execution (Week 17), documentation (Week 18)
- **🟡 Sonnet**: Migration strategy (Weeks 13-14), performance analysis (Weeks 15-16)

**When to Escalate to Sonnet**:
- Cache migration causes performance regression > 10%
- Unexpected complexity in static buffer patterns
- Final audit discovers critical globals missed

---

## Week-by-Week Plan

### Week 13-14: Static Buffer Audit & Migration

**Model**: 🟡 **Sonnet** - Audit and decision framework, migration strategy

**Goal**: Audit all static buffers, migrate to thread-local or dynamic allocation

**Approach** (🟡 Sonnet for audit and strategy):
```bash
# Audit script to find static buffers
grep -n "static.*\[" Common/source/*.c | grep -v "const"
```

**Decision Framework**:
- **Hot path?** → Thread-local (performance)
- **Cold path?** → Dynamic allocation (simplicity)
- **Read-only?** → Document and keep

**Example Migration**:
```c
// Before (global static buffer)
static bigstring tempbuffer;

void somefunction(void) {
    copystring("value", tempbuffer);
    // ... use tempbuffer
}

// After (thread-local)
_Thread_local bigstring tempbuffer;
// Same code, thread-safe storage
```

**Testing**: Verify no cross-thread contamination

**Deliverable**: PR #11 - Static Buffer Cleanup

---

### Week 15-16: Cache Thread-Safety

**Model**: 🟡 **Sonnet** - Performance-critical, requires benchmarking and tradeoffs

**Goal**: Make caches thread-safe or thread-local

**Caches to Audit** (🟡 Sonnet for performance analysis):
- Font cache (`cachedfontname`, `cachedfontnum`)
- Error cushions (`herrorcushion`)
- Memory cushions (`hsafetycushion`)
- Any other caching mechanisms

**Approaches**:
1. **Thread-local cache** - Each thread has own cache
2. **Lock-protected cache** - Shared cache with mutex (if read-heavy)
3. **No cache** - Eliminate if not performance-critical

**Testing**:
- Cache correctness under concurrent load
- Performance benchmarks (verify no regression)

**Deliverable**: PR #12 - Cache Thread-Safety

---

### Week 17: Final Global Audit

**Model**: 🟢 **Haiku** - Pattern-following, comprehensive checklist execution

**Goal**: Comprehensive audit to catch any remaining globals

**Audit Process** (🟢 Haiku for systematic execution):
```bash
# Find all global variables
./tools/audit_global_variables.sh

# Review each one:
# - Already migrated? (check off)
# - Read-only? (document as acceptable)
# - Needs migration? (add to tracking)
# - GUI-only? (document as out-of-scope)
```

**Document Remaining Globals**:
Create `planning/phase4/REMAINING_GLOBALS.md`:
- List any globals NOT migrated
- Justify each one (why acceptable)
- Document if GUI-only, platform-specific, or read-only

**Testing**: Full test suite (unit + integration + stress + thread sanitizer)

**Deliverable**: PR #13 - Final Global State Audit

---

### Week 18: Documentation & Milestone

**Model**: 🟢 **Haiku** - Documentation writing, celebration content

**Goal**: Comprehensive documentation, celebrate completion

**Tasks** (🟢 Haiku for all documentation):
1. Update all ADRs with "Implemented" status
2. Create architecture documentation:
   - `docs/GLOBAL_STATE_ELIMINATION_COMPLETE.md`
   - Summary of what was done
   - Lessons learned
   - Future recommendations

3. Update CLAUDE.md:
   - Mark "BURN THE GLOBALS WITH FIRE" as COMPLETE
   - Document patterns for future work
   - Reference all ADRs

4. Create visualization:
   - Before/after comparison
   - Metrics (# globals eliminated, files modified, LOC changed)

5. Celebrate milestone: **GLOBAL STATE ELIMINATION COMPLETE** 🔥🎉

**Deliverable**: Architecture Documentation & Milestone

---

## Success Criteria (P2 Complete)

- ✅ No global mutable state in Frontier runtime (except justified shared state)
- ✅ All static buffers thread-safe or eliminated
- ✅ Comprehensive architecture documentation
- ✅ All tests pass (unit + integration + stress + thread sanitizer)
- ✅ **READY FOR COLLABORATIVE ODB (Phase 6+)** 🚀

---

## Justified Shared State

**Acceptable Globals After P2**:

1. **Read-Only Tables** (initialized once, never modified):
   - Built-in function tables
   - Keyword tables
   - Constant tables

2. **Locks and Mutexes** (inherently shared):
   - Database file locks
   - Memory allocator locks

3. **GUI-Only Globals** (not in headless builds):
   - Window state
   - Graphics state
   - Platform-specific UI globals

**Document in**: `planning/phase4/REMAINING_GLOBALS.md`

---

## Files Modified (Estimated)

**Static Buffers**: ~20 files (verb processors, utilities)
**Caches**: ~6 files (font, error, memory)
**Final Audit**: Any missed globals
**Documentation**: Multiple docs updated

**Total**: 30-40 files modified across 4 deliverables

---

## Testing Strategy

**Comprehensive Test Suite** (Week 17):
```bash
# Unit tests
./tools/run_headless_tests.sh

# Integration tests
cd tests && make test-integration

# Stress tests
./tests/stress_test_concurrent --threads=20 --duration=300s

# Thread sanitizer (MUST be clean)
make TSAN=1 && ./tests/run_all_tests

# Memory leak detection
make LSAN=1 && ./tests/run_all_tests

# Performance benchmarks
./tests/benchmark_suite
# Expected: < 5% regression from baseline
```

---

## Risks & Mitigation

**Risk**: More globals discovered during audit
**Mitigation**: Triage as launch-blocking or acceptable

**Risk**: Cache elimination causes performance regression
**Mitigation**: Benchmark, revert if > 10% slowdown

**Risk**: Scope creep (trying to optimize while cleaning)
**Mitigation**: Focus on correctness, not optimization

---

## Lessons Learned (To Document)

After P2 completion, document in `docs/GLOBAL_STATE_ELIMINATION_COMPLETE.md`:

1. **What Worked Well**:
   - Thread-local pattern (ADR-005, ADR-006)
   - Phased approach (P0 → P1 → P2)
   - Comprehensive testing

2. **What Didn't Work**:
   - (To be filled in during execution)

3. **Future Recommendations**:
   - Never introduce new global mutable state
   - Use patterns from this phase
   - Reference ADRs in code comments

4. **Metrics**:
   - # globals eliminated: 200+
   - # files modified: 60+
   - # PRs merged: 13+
   - Timeline: 18 weeks

---

## Celebration 🎉

**After Week 18 completion**:

1. Create milestone commit:
   ```bash
   git commit -m "feat: GLOBAL STATE ELIMINATION COMPLETE 🔥

   All 200+ global mutable variables eliminated or justified.
   Frontier runtime is now thread-safe and ready for collaborative ODB.

   Phases completed:
   - P0a: Critical thread-safety (Weeks 1-3)
   - P0b: Launch requirements (Weeks 4-6) → LAUNCH READY
   - P1a: Multi-user foundation (Weeks 7-9)
   - P1b: Multi-user complete (Weeks 10-12) → MULTI-USER READY
   - P2: Comprehensive cleanup (Weeks 13-18) → COMPLETE

   Co-Authored-By: [Your Name] <email>"
   ```

2. Update project status documents
3. Plan Phase 6+ (collaborative ODB implementation)

---

## Next Phase

After P2 completion → **Phase 6+: Collaborative ODB**

Implementation of actual collaborative editing features on top of thread-safe foundation.

---

**Last Updated**: 2026-01-19
**Status**: Starts after P1b completion (Week 19, with model selection guidance)
