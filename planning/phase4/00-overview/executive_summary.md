# Global State Elimination - Executive Summary

**For**: Quick reference and strategic decision-making
**Full Plan**: See `GLOBAL_STATE_ELIMINATION_PLAN.md` (74 pages)
**Date**: 2026-01-13

---

## The Mission

**Eliminate ALL global mutable state from Frontier runtime**

Why? Thread-safety for launch (Automattic partnership) + foundation for collaborative ODB (Frontier 2.0).

---

## The Numbers

| Metric | Count |
|--------|-------|
| **Total Globals Found** | 200+ |
| **P0 (Launch-Blocking)** | 30 globals |
| **P1 (Multi-User)** | 25 globals |
| **P2 (Cleanup)** | 145 globals |
| **Files to Modify** | ~60 files |
| **Estimated Timeline** | 12-18 weeks |

---

## The Strategy

### 1. Thread-Local Storage (Primary Pattern)

**For per-thread execution state** (scope, parser, control flow, errors)

✅ **Proven**: ADR-005 (parameter state), ADR-006 (outline context)
✅ **Zero API changes**: Macros provide backward compatibility
✅ **Simple**: 3 files modified per migration

```c
// Add to tythreadglobals structure
typedef struct tythreadglobals {
    hdlhashtable currenthashtable;  // Variable scope
    hdltreenode yylval;              // Parser state
    boolean flbreak;                 // Control flow
} tythreadglobals;

// Backward-compatible macros
#define currenthashtable ((**hthreadglobals).currenthashtable)
#define yylval ((**hthreadglobals).yylval)
#define flbreak ((**hthreadglobals).flbreak)
```

### 2. Explicit Context Structures (Secondary Pattern)

**For per-operation/session state** (system tables, databases)

✅ **Proven**: `db_context` pattern
✅ **Gradual migration**: Global accessor → Thread-local → Explicit parameter
✅ **Flexible**: Supports reference counting for multi-user

```c
// System context (all system tables)
typedef struct system_context {
    hdlhashtable roottable;
    hdlhashtable systemtable;
    hdlhashtable efptable;  // Processor tables
    // ... 20+ system tables
    _Atomic uint32_t refcount;
} system_context;

// Phase 1: Global accessor (backward compatible)
#define roottable (get_system_context()->roottable)

// Phase 2: Explicit parameter (future, optional)
langfunctionvalue(system_context *ctx, ...);
```

### 3. Reference Counting (Phase P1)

**For shared objects across threads** (outlines, hash tables, contexts)

✅ **Standard pattern**: Cocoa NSObject, C++ shared_ptr
✅ **Thread-safe**: Atomic refcount operations
✅ **Foundation**: Enables collaborative ODB (Phase 6+)

```c
hdloutlinerecord op_outline_retain(hdloutlinerecord ho);
void op_outline_release(hdloutlinerecord ho);

// Usage (automatic in push/pop)
oppushoutline(ho);  // Internally retains
oppopoutline();     // Internally releases
```

---

## The Timeline

### Phase P0: Launch Readiness (Weeks 1-6)

**Goal**: Thread-safe runtime (no race conditions)

| Week | Deliverable | Globals Migrated |
|------|-------------|------------------|
| 1 | Hash table context | `currenthashtable`, `hmagictable`, `hashtablestack` |
| 2 | Parser state | `yylval`, `yyval`, `langparser_result` |
| 3 | Control flow & errors | `flbreak`, `flcontinue`, `lasterror` |
| 4 | System context structure | Design + implementation |
| 5 | System table migration | 20+ system tables → context |
| 6 | Shell config & dialogs | `config`, `dialogstack` |

**Milestone**: ✅ **READY FOR LAUNCH** (thread-safety achieved)

### Phase P1: Multi-User Foundation (Weeks 7-12)

**Goal**: Reference counting + concurrent operations

| Week | Deliverable | Focus |
|------|-------------|-------|
| 7 | Outline refcounting | `op_outline_retain/release` |
| 8 | Hash table refcounting | `hashtable_retain/release` |
| 9 | System context lifecycle | Full refcount for context |
| 10 | Stress testing | 10+ threads, concurrent ops |
| 11 | Lock state prep | CRDT design (Phase 6+) |
| 12 | Documentation | Multi-user architecture |

**Milestone**: ✅ **READY FOR MULTI-USER** (collaborative foundation)

### Phase P2: Comprehensive Cleanup (Weeks 13-18)

**Goal**: Zero global mutable state

| Week | Deliverable | Focus |
|------|-------------|-------|
| 13-14 | Static buffer audit | Thread-local or dynamic |
| 15-16 | Cache migration | Font cache, error cushions |
| 17 | Final global audit | Document remaining globals |
| 18 | Documentation | Architecture docs, lessons learned |

**Milestone**: ✅ **GLOBAL STATE ELIMINATION COMPLETE** 🔥

---

## Critical Globals (P0 Priority)

### Processor & Scope Tables
- `efptable` - Processor dispatch (ADR-008 workaround currently)
- `currenthashtable` - Variable scope (Issue #262)
- `roottable`, `systemtable`, `internaltable` - System tables

### Parser & Control Flow
- `yylval`, `yyval`, `langparser_result` - Parser state
- `flbreak`, `flcontinue` - Loop control
- `lasterror`, `lasterrormessage` - Error state

### Shell & Configuration
- `config` - Window configuration (push/pop pattern)
- `dialogstack` - Dialog nesting

---

## Zero Breaking Changes Guarantee

**All migrations maintain 100% backward compatibility**:

1. **Macros for thread-local globals**:
   ```c
   // Old code: currenthashtable = ht;
   // New code: #define currenthashtable ((**hthreadglobals).currenthashtable)
   // Same syntax, different implementation - zero changes
   ```

2. **Global accessors for contexts**:
   ```c
   // Old code: hashtablelookup(efptable, ...)
   // New code: #define efptable (get_system_context()->efptable)
   // Same syntax, works identically
   ```

3. **UserTalk scripts unchanged**:
   ```usertalk
   /* This script works identically before and after */
   op.insert("text", down)
   @x = 5
   db.save()
   ```

---

## Risk Mitigation

### Top 5 Risks & Mitigation

1. **Thread swap functions incomplete**
   - ✅ Use proven ADR-005/ADR-006 template
   - ✅ Checklist for each migration
   - ✅ Thread sanitizer testing

2. **Forgot to migrate a global**
   - ✅ Comprehensive audit (200+ globals catalogued)
   - ✅ Automated detection script
   - ✅ Phase-by-phase verification

3. **Performance degradation**
   - ✅ Thread-local is fast (TLS optimized)
   - ✅ Benchmark before/after each phase
   - ✅ Profile hot paths

4. **Reference counting leaks**
   - ✅ Simple refcount (no weak refs initially)
   - ✅ Comprehensive lifecycle testing
   - ✅ Memory leak detection

5. **Timeline delays**
   - ✅ 20% buffer per phase
   - ✅ Phases independent where possible
   - ✅ Parallelize testing

---

## Success Criteria

### P0 Success (Launch Ready)
- ✅ No race conditions on critical globals
- ✅ Multiple threads execute UserTalk concurrently
- ✅ All tests pass (unit + integration)
- ✅ Thread sanitizer clean

### P1 Success (Multi-User Ready)
- ✅ Reference counting working (retain/release)
- ✅ Stress tests pass (10+ threads)
- ✅ Foundation for collaborative ODB

### P2 Success (Complete)
- ✅ No global mutable state (except justified)
- ✅ Comprehensive documentation
- ✅ Architecture enables Phase 6+ features

---

## Immediate Next Steps

**Week 1 Action Items**:
1. ✅ User reviews and approves this plan
2. ⏳ Create GitHub tracking issues (P0a, P0b, P1, P2)
3. ⏳ Create feature branch: `feature/global-state-elimination-p0a`
4. ⏳ Start P0a Week 1: Hash table context migration

**User Decisions Needed**:
- [ ] Approve overall approach (thread-local + explicit context)
- [ ] Approve timeline (12-18 weeks, 3 phases)
- [ ] Decide on GUI-only globals (skip or migrate?)
- [ ] Approve zero-breaking-changes constraint

---

## Related Documentation

- **Full Plan**: `planning/GLOBAL_STATE_ELIMINATION_PLAN.md` (74 pages)
- **ADR-005**: Parameter State Thread-Safety (proven pattern)
- **ADR-006**: Outline Context Migration (proven pattern)
- **ADR-008**: Processor Table Workaround (to be removed in P0b)
- **CLAUDE.md**: "BURN THE GLOBALS WITH FIRE" strategic vision
- **Issue #135**: Outline context refactoring
- **Issue #262**: currenthashtable thread-safety
- **Issue #292**: efptable refactoring

---

## Key Quotes

> "BURN THE GLOBALS WITH FIRE. EVERYWHERE."
> — CLAUDE.md, Strategic Requirements

> "This code MUST be thread-safe before launch. Global mutable state makes thread safety impossible."
> — CLAUDE.md, Launch Requirements

> "The user (TPM/CTO) has decided to eliminate global state PROPERLY, not patch it with more workarounds."
> — Planning Context

---

**Ready to BURN THE GLOBALS WITH FIRE?** 🔥

**Next**: User review → Create tracking issues → Start P0a Week 1
