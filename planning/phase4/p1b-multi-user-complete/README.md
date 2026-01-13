# P1b: Multi-User Complete (Weeks 10-12)

**Status**: Multi-User Preparation
**Timeline**: 3 weeks
**Goal**: Stress test concurrent operations, prepare for collaborative ODB

---

## Scope

**What We're Validating**: Thread-safety under concurrent load

**Focus Areas**:
- Concurrent stress testing (10+ threads)
- Lock state design for CRDT (Phase 6+ prep)
- Multi-user architecture documentation

**Why P1b**: Validation that everything actually works under real concurrent load. Preparation for Phase 6+ collaborative features.

---

## Week-by-Week Plan

### Week 10: Concurrent Operation Testing

**Goal**: Stress test with 10+ threads executing UserTalk concurrently

**Test Scenarios**:
1. **Concurrent Script Execution**
   - 10 threads running different UserTalk scripts simultaneously
   - Verify no race conditions, no crashes

2. **Concurrent Outline Operations**
   - Multiple threads manipulating different outlines
   - Verify outline context isolation

3. **Concurrent Hash Table Access**
   - Multiple threads accessing system tables
   - Verify scope resolution correctness

4. **Concurrent Database Operations**
   - Multiple threads reading/writing to database
   - Verify no corruption

**Tools**:
```bash
# Thread sanitizer
make TSAN=1 && ./tests/stress_test_concurrent

# Stress test suite
./tests/run_stress_tests.sh --threads=10 --duration=60s
```

**Deliverable**: PR #10 - Concurrent Stress Tests

---

### Week 11: Lock State Preparation (CRDT Foundation)

**Goal**: Design lock state structure for collaborative ODB (Phase 6+)

**Not Implementing**: Full CRDT system (deferred to Phase 6+)

**Preparing**:
1. Add reserved fields to structures for future lock state
2. Design CRDT lock state structure (on paper)
3. Document lock state requirements
4. Create placeholder functions (stubs)

**Example Design**:
```c
// Future lock state (Phase 6+)
typedef struct crdt_lock_state {
    uint64_t lamport_clock;      // Logical timestamp
    char site_id[64];             // Site identifier
    void *vector_clock;           // Version vector
} crdt_lock_state;

// Reserved fields added now:
typedef struct tyoutlinerecord {
    // ... existing fields ...
    _Atomic uint32_t refcount;    // P1a
    void *lock_context;           // P1b (placeholder, NULL for now)
} tyoutlinerecord;
```

**Deliverable**: ADR-009 - Collaborative ODB Lock State Design

---

### Week 12: Multi-User Documentation

**Goal**: Comprehensive architecture documentation for multi-user operations

**Documents to Create**:
1. **docs/MULTI_USER_ARCHITECTURE.md**
   - Thread-safety guarantees
   - Reference counting patterns
   - Context lifecycle management

2. **docs/REFERENCE_COUNTING_GUIDE.md**
   - When to use retain/release
   - Common pitfalls
   - Memory leak prevention

3. **docs/THREAD_SAFETY_GUARANTEES.md**
   - What's thread-safe (after Phase 4)
   - What's not thread-safe (GUI globals)
   - Concurrency best practices

4. **Update CLAUDE.md**
   - Reference new ADRs
   - Document refcounting patterns
   - Update "BURN THE GLOBALS" section with completion status

**Deliverable**: Multi-User Architecture Documentation

---

## Success Criteria (P1b Complete)

- ✅ Multiple threads execute UserTalk concurrently (stress tested)
- ✅ No race conditions under load (thread sanitizer clean)
- ✅ Foundation complete for Phase 6+ collaborative ODB
- ✅ **READY FOR MULTI-USER FEATURES** 🎉

---

## Testing Strategy

**Stress Test Suite** (new in Week 10):
```c
// tests/stress_test_concurrent.c
void test_concurrent_scripts(int num_threads, int duration_sec) {
    // Spawn N threads, each running UserTalk scripts
    // Verify no crashes, no race conditions
}

void test_concurrent_outlines(int num_threads) {
    // Multiple threads manipulating outlines
    // Verify context isolation
}

void test_concurrent_hashtables(int num_threads) {
    // Multiple threads accessing system tables
    // Verify scope correctness
}
```

**Thread Sanitizer**:
```bash
# Must be clean:
make TSAN=1
./tests/stress_test_concurrent --threads=20 --duration=120s
# Expected: 0 warnings, 0 errors
```

**Memory Leak Detection**:
```bash
# Valgrind or LeakSanitizer
make LSAN=1
./tests/run_all_tests
# Expected: 0 leaks
```

---

## Risks & Mitigation

**Risk**: Stress tests uncover race conditions missed earlier
**Mitigation**: Fix immediately, may extend timeline

**Risk**: Performance degradation under concurrent load
**Mitigation**: Profile hot paths, optimize if > 10% slowdown

**Risk**: CRDT design too complex for P1b
**Mitigation**: Keep design simple, implementation deferred to Phase 6+

---

## Phase 6+ Preparation

**What We're NOT Doing in P1b** (deferred to Phase 6+):
- Full CRDT implementation
- Conflict resolution logic
- Operational transformation
- Multi-user session management

**What We ARE Preparing**:
- Reserved fields in structures
- Lock state design (on paper)
- Architecture documentation
- Testing infrastructure

**Phase 6+** will implement actual collaborative ODB on top of this foundation.

---

## Next Phase

After P1b completion → **[P2: Comprehensive Cleanup](../p2-comprehensive-cleanup/README.md)**

P2 eliminates remaining P2 globals (static buffers, caches).

---

**Last Updated**: 2026-01-13
**Status**: Starts after P1a completion (Week 10)
