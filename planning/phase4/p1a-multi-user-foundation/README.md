# P1a: Multi-User Foundation (Weeks 7-9)

**Status**: Multi-User Preparation
**Timeline**: 3 weeks
**Goal**: Add reference counting for multi-user object sharing

---

## Scope

**What We're Adding**: Reference counting lifecycle for shared objects

**Objects**:
- Outline objects (tyoutlinerecord)
- Hash tables (tyhashtable)
- System context (system_context)

**Why P1a**: Foundation for collaborative ODB. Objects must be safely shared across threads with clear lifecycle.

---

## Model Selection Guide

Each week uses the appropriate Claude model based on task complexity:

| Week | Scope | Complexity | Model | Rationale |
|------|-------|------------|-------|-----------|
| **Week 7** | Outline Reference Counting | Medium | 🟡 Sonnet | Architectural pattern, first refcount implementation, lifecycle API design |
| **Week 8** | Hash Table Reference Counting | Low | 🟢 Haiku | Pattern-following from Week 7, same refcount pattern |
| **Week 9** | System Context Lifecycle | Medium | 🟡 Sonnet | Thread-local caching design, context lifecycle complexity |

**Using This Guide**:
- **🟢 Haiku**: Pattern-following once refcount pattern established (Week 8)
- **🟡 Sonnet**: First implementation of pattern (Week 7), or complex lifecycle (Week 9)

**When to Escalate to Sonnet**:
- Reference counting leaks or cycles discovered
- Performance issues from atomic operations
- Thread-local caching proves more complex than expected

---

## Week-by-Week Plan

### Week 7: Outline Reference Counting

**Model**: 🟡 **Sonnet** - First refcount pattern implementation, lifecycle API design

**Goal**: Add refcount lifecycle to outline objects

**Implementation** (🟡 Sonnet for all tasks):
```c
// Add to tyoutlinerecord
typedef struct tyoutlinerecord {
    // ... existing fields ...
    _Atomic uint32_t refcount;
    hdlthread owner_thread;  // Current owner
} tyoutlinerecord;

// Lifecycle API
hdloutlinerecord op_outline_create(void);
hdloutlinerecord op_outline_retain(hdloutlinerecord ho);
void op_outline_release(hdloutlinerecord ho);
```

**Integration**:
- `oppushoutline()` internally calls `op_outline_retain()`
- `oppopoutline()` internally calls `op_outline_release()`
- Existing code works unchanged

**Testing**:
- Outline lifecycle tests
- Verify no dangling pointers
- Stress test with multiple push/pop cycles

**Deliverable**: PR #7 - Outline Reference Counting

---

### Week 8: Hash Table Reference Counting

**Model**: 🟢 **Haiku** - Pattern-following from Week 7, same refcount pattern

**Goal**: Add refcount lifecycle to hash tables

**Implementation** (🟢 Haiku for all tasks):
```c
// Add to tyhashtable
typedef struct tyhashtable {
    // ... existing fields ...
    _Atomic uint32_t refcount;
} tyhashtable;

// Lifecycle API
hdlhashtable hashtable_create(...);
hdlhashtable hashtable_retain(hdlhashtable ht);
void hashtable_release(hdlhashtable ht);
```

**Integration**:
- Update `hashtabledispose()` to check refcount
- Update scope push/pop to use retain/release
- Existing APIs work unchanged

**Testing**:
- Hash table lifecycle tests
- Verify no memory leaks
- Stress test with nested scopes

**Deliverable**: PR #8 - Hash Table Reference Counting

---

### Week 9: System Context Reference Counting

**Model**: 🟡 **Sonnet** - Thread-local caching design, context lifecycle complexity

**Goal**: Complete system_context lifecycle with thread-local caching

**Implementation** (🟡 Sonnet for all tasks):
```c
// Already has refcount from P0b Week 4
// Add thread-local caching:
_Thread_local system_context* tls_system_context = NULL;

system_context* get_system_context(void) {
    if (tls_system_context == NULL) {
        tls_system_context = system_context_retain(g_system_context);
    }
    return tls_system_context;
}
```

**Benefits**:
- Each thread retains own reference to system context
- System context can't be disposed while threads using it
- Foundation for per-user system contexts (Phase 6+)

**Testing**:
- Multi-thread system context access
- Verify refcount correctness
- No premature disposal

**Deliverable**: PR #9 - System Context Lifecycle

---

## Success Criteria (P1a Complete)

- ✅ Objects safely shared across threads
- ✅ No dangling pointers (refcount prevents premature disposal)
- ✅ Foundation for concurrent operations on shared objects
- ✅ All tests pass
- ✅ Memory leak tests clean

---

## Files Modified (Estimated)

**Outline Context**:
- `Common/headers/op.h` - tyoutlinerecord refcount field
- `Common/source/opinternal.c` - Lifecycle API
- `Common/source/op.c` - Update push/pop

**Hash Table Context**:
- `Common/headers/tablestruct.h` - tyhashtable refcount field
- `Common/source/langhash.c` - Lifecycle API

**System Context**:
- `Common/source/tablestructure.c` - Thread-local caching

**Total**: ~6-8 files modified across 3 PRs

---

## Developer Resources

**Pattern**: See [../00-overview/README.md](../00-overview/README.md) - Pattern 3 (Reference Counting)

**Reference Counting Best Practices**:
- Use C11 atomics (`_Atomic uint32_t`)
- Always retain on assignment, release on disposal
- Check for NULL before retain/release
- Last reference (refcount == 1) triggers disposal

---

## Risks & Mitigation

**Risk**: Reference counting leaks (forgot to release)
**Mitigation**: Comprehensive lifecycle tests, leak detection tools

**Risk**: Reference cycles (outline→table→outline)
**Mitigation**: Document ownership patterns, defer weak refs to Phase 6+

**Risk**: Performance overhead from atomic operations
**Mitigation**: Profile hot paths, atomics are generally fast

---

## Next Phase

After P1a completion → **[P1b: Multi-User Complete](../p1b-multi-user-complete/README.md)**

P1b stress tests concurrent operations and prepares for collaborative ODB.

---

**Last Updated**: 2026-01-19
**Status**: Starts after P0b completion (Week 10, with model selection guidance)
