# Database Context Global State Fix - Architectural Analysis

**Document Status:** Analysis Complete - Awaiting TPM/CTO Decision
**Created:** 2026-01-10
**Related Issues:** #266 (P0), #264, #135
**Related ADRs:** ADR-006 (thread-local pattern)

## Executive Summary

The Frontier ODB system has a **critical architectural flaw** where database context is managed through global mutable state with stale cached copies in the cancoon record. This causes `db.save()` to fail catastrophically when system root is loaded, saving the wrong database. The problem is a manifestation of the broader global mutable state anti-pattern that blocks thread-safe operation and collaborative ODB features.

**Problem:** Lazy root table creation updates global `rootvariable` but not the cancoon record's `hrootvariable`, causing subsequent `db.save()` operations to reset globals to stale values and save the wrong database.

**Impact:** P0 launch blocker. All database verbs fail in production scenarios where system root is loaded.

**Recommendation:**
- **Phase 1 (v7 Launch):** Quick fix - Update cancoon record after lazy root table creation
- **Phase 2 (Production Deployment):** Full architectural fix - Explicit database context threading (eliminate global state entirely)

---

## Problem Statement

### The Anti-Pattern

The Frontier ODB system manages database context through a dual-state system:

1. **Global State** (updated during operations):
   ```c
   // tablestructure.c
   Handle rootvariable = nil;          // Current root table variable
   hdlhashtable roottable = nil;       // Current root hash table

   // db.c
   hdldatabaserecord databasedata = nil;  // Current database

   // langhash.c
   hdlhashtable currenthashtable = nil;   // Current hash table context
   hdltablestack hashtablestack = nil;    // Hash table stack
   ```

2. **Cancoon Record** (snapshot of state when database opened):
   ```c
   typedef struct tycancoonrecord {
       hdldatabaserecord hdatabase;      // Database handle
       hdlhashtable hroottable;          // Root table (CAN BE STALE!)
       Handle hrootvariable;             // Root variable (CAN BE STALE!)
       hdltablestack htablestack;        // Table stack
       boolean accesssing;               // Access flag
   } tycancoonrecord;
   ```

### The Bug Flow (Issue #266)

When `db.save()` is called on a guest database with system root loaded:

1. **Database Creation** (`db.new()` + `db.open()`):
   ```c
   odbOpenFile(fnum, &odb, false);
   // Creates cancoonrecord with:
   //   (**hc).hdatabase = databasedata (valid)
   //   (**hc).hrootvariable = nil (no root table yet - empty database)
   //   (**hc).hroottable = nil
   ```

2. **Lazy Root Table Creation** (`db.setvalue("key", "value")`):
   ```c
   odbSetValue(odb, "key", value);
   setcancoonglobals(hc);  // Restores globals from cancoon record
   langexpandtodotparams("key", &htable, bsname);
       → langgetdotparams(...)
           → langexternalgettable(...) CREATES root table
           → settablestructureglobals(hvariable) UPDATES GLOBAL rootvariable

   // PROBLEM: Global rootvariable is now valid
   //          BUT (**hc).hrootvariable is still nil (STALE!)
   ```

3. **Save Operation** (`db.save()`):
   ```c
   odbSaveFile(odb);
   setcancoonglobals(hc);  // Resets globals from STALE cancoon record
       → settablestructureglobals((**hc).hrootvariable)  // NIL!
           → rootvariable = nil  // GLOBAL RESET TO NIL

   // With system root loaded:
   // - rootvariable defaults to system root's rootvariable
   // - tablesavesystemtable(rootvariable) saves SYSTEM ROOT instead of guest DB!
   ```

### Evidence from Production

Integration test failure with system root loaded:
```
[table-ERROR] tablepack.c:395: tablepacktable failed for system table
[lang-ERROR] langcallbacks.c:208: Can't save the database because
              there was an error packing examples: .
[db-ERROR] odbengine.c:738: odbSaveFile: failed to save v7 root table
```

The error references "examples" table, which **only exists in system root**, proving that `db.save()` is attempting to save the system database instead of the guest database.

---

## Cascading Effects of Global State Pattern

### 1. Database Context Confusion

**Where it manifests:**
- Any operation that switches between databases (guest ↔ system root)
- Database verb operations (`db.setvalue`, `db.save`, `db.close`)
- External table variable loading (`tableverbinmemory`)

**How it breaks:**
- `setcancoonglobals()` is called at the start of every database verb
- This resets global `rootvariable` from the cancoon record's `hrootvariable`
- If `hrootvariable` is stale (due to lazy creation), wrong database becomes active
- Subsequent operations (save, pack, reference) operate on wrong database

### 2. Multi-Database Scenarios

**Current assumption:** Only one database is "active" at a time
**Reality:** System root is ALWAYS loaded in production, guest databases are opened alongside it

**Broken scenarios:**
1. **Guest database with system root loaded:** `db.save()` saves system root instead of guest DB
2. **Nested database operations:** External table variables from different databases can conflict
3. **Concurrent operations:** Thread-unsafe globals mean parallel operations corrupt each other

### 3. Thread Safety Blocking

**Global mutable state prevents:**
- Multi-threaded verb execution
- Concurrent database operations
- Collaborative ODB editing (multiple users, one database)

**Why it's critical:**
- enterprise partnerships requires stable multi-user concurrent access
- Dave Winer's collaborative ODB vision requires thread-safe operation
- Phase 2.0 CRDT foundation depends on eliminating global state

### 4. Context Guard Pattern Limitations

The `db_context_guard` pattern (used throughout db.c and db_format.c) is **correct for scoped operations** but **insufficient for cancoon record management**:

```c
// Context guards work for this pattern:
boolean dbrefhandle_context(const db_context *context, dbaddress adr, Handle *h) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);  // Save state
    boolean ok = dbrefhandle(adr, h);
    db_context_guard_exit(&guard);            // Restore state
    return ok;
}
```

**But fail for cancoon state management because:**
- Guards restore global state on exit (correct for temporary operations)
- Cancoon record expects to be the "source of truth" for database context
- When globals are updated by child operations (lazy table creation), cancoon record becomes stale
- No mechanism to "flush" updated globals back to the cancoon record

---

## Root Cause Analysis

### Where Lazy Root Table Creation Happens

Call chain for `db.setvalue("key", "value")`:
```
odbSetValue(odb, "key", value)
  → setcancoonglobals(hc)  // Set globals from cancoon record
  → odbexpandtodotparams("key", &htable, bsname)
      → langexpandtodotparams(...)
          → langgetdotparams(...)
              → langexternalgettable("root", &htable) OR first-level table lookup
                  → tablevaltotable(val, &htable, hnode)
                      → tableverbinmemory(ctx, hvariable, hnode)
                          → tableunpacktable(...) → creates root table in memory
                      → *htable = (hdlhashtable) (**hvariable).variabledata
              → settablestructureglobals(hvariable, false)
                  → rootvariable = (Handle) hvariable  ← GLOBAL UPDATED
                  → roottable = (hdlhashtable) (**hvariable).variabledata
```

**Critical insight:** `settablestructureglobals()` is called by `langgetdotparams()` when a root table is materialized, but the cancoon record's `hrootvariable` field is **never updated** to reflect this change.

### Why System Root Causes the Bug

Without system root loaded:
- `rootvariable = nil` after `setcancoonglobals(nil)`
- Guest database's root table becomes the active `rootvariable`
- `db.save()` saves the guest database (correct, but only by accident)

With system root loaded:
- `rootvariable = system_root_variable` after `setcancoonglobals(nil)`
- System root "shadows" the guest database's root table
- `db.save()` saves the system root database (WRONG!)

### The Fundamental Design Flaw

The cancoon record was designed for **single-database scenarios** where:
- One database is open at a time
- Root table is loaded at database open time (not lazily)
- Global state = cancoon record state (no divergence)

This breaks in **modern headless scenarios** where:
- System root is always loaded (for `system.verbs`, `builtins`, etc.)
- Guest databases are opened alongside system root
- Root tables are lazily created (empty databases start with nil root)
- Global state diverges from cancoon record state

---

## Options Analysis

### Option 1: Quick Fix - Update Cancoon Record After Lazy Creation

**Description:** Patch `langgetdotparams()` or `settablestructureglobals()` to update the cancoon record's `hrootvariable` field when a root table is lazily created.

**Implementation:**
```c
// In langvalue.c, after settablestructureglobals() is called:
boolean langgetdotparams (hdltreenode htree, hdlhashtable *htable, bigstring bsname) {
    // ... existing code ...

    // After root table materialization:
    if (hsubtable == nil && rootvariable != nil) {
        // Update cancoon record if it exists and is stale
        if (cancoonglobals != nil && (**cancoonglobals).hrootvariable == nil) {
            (**cancoonglobals).hrootvariable = rootvariable;
            (**cancoonglobals).hroottable = roottable;
        }
    }

    // ... continue ...
}
```

**Pros:**
- ✅ Minimal code change (5-10 lines)
- ✅ Surgical fix - low regression risk
- ✅ Unblocks v7 launch immediately
- ✅ Does not require refactoring entire codebase

**Cons:**
- ❌ Does not fix underlying architectural problem
- ❌ Technical debt - another patch on top of global state pattern
- ❌ Does not enable thread safety or collaborative ODB
- ❌ Future lazy creations in other code paths may have same bug

**Risk Assessment:**
- **Low risk for v7 launch** - localized change, well-understood
- **Medium risk for Phase 2** - accumulates technical debt

**Effort:** 1-2 hours (implementation + testing)

---

### Option 2: Bidirectional Context Guards

**Description:** Enhance `db_context_guard` pattern to be bidirectional - save state on entry, restore state on exit, **AND** flush updated state back to cancoon record after child operations complete.

**Implementation:**
```c
// New pattern:
typedef struct db_context_guard {
    db_format_mode prev_mode;
    db_saveas_state prev_saveas;
    hdldatabaserecord prev_db;
    Handle prev_rootvariable;  // NEW: Track root variable changes
} db_context_guard;

void db_context_guard_enter(const db_context *context, db_context_guard *guard) {
    guard->prev_mode = db_format_mode_current();
    guard->prev_rootvariable = rootvariable;
    // ... existing code ...
}

void db_context_guard_exit(const db_context_guard *guard) {
    // Check if rootvariable changed during operation
    if (rootvariable != guard->prev_rootvariable && cancoonglobals != nil) {
        // Flush updated rootvariable back to cancoon record
        (**cancoonglobals).hrootvariable = rootvariable;
        (**cancoonglobals).hroottable = roottable;
    }

    // Restore previous state
    rootvariable = guard->prev_rootvariable;
    // ... existing restore code ...
}
```

**Pros:**
- ✅ Generalizes the fix to all database operations
- ✅ Makes context guards "correct by construction"
- ✅ Lower risk of future bugs from lazy creation in other code paths
- ✅ Still minimal code change (modify existing pattern)

**Cons:**
- ❌ Still relies on global mutable state
- ❌ Does not enable thread safety
- ❌ Bidirectional guards increase complexity
- ❌ Risk of "action at a distance" - updates may happen unexpectedly

**Risk Assessment:**
- **Medium risk for v7 launch** - more complex than Option 1, requires careful testing
- **Medium risk for Phase 2** - accumulates technical debt, but less than Option 1

**Effort:** 4-8 hours (implementation + testing + verification across all db operations)

---

### Option 3: Explicit Database Context Threading ⭐ **RECOMMENDED FOR PHASE 2**

**Description:** Eliminate global database state entirely by threading explicit database context through all ODB operations. Similar to outline context refactoring (Issue #135).

**Implementation:**
```c
// New explicit context structure:
typedef struct odb_runtime_context {
    hdldatabaserecord database;        // Current database
    hdlhashtable root_table;           // Root table
    Handle root_variable;              // Root variable
    hdltablestack table_stack;         // Table stack
    hdlhashtable current_hashtable;    // Current context
} odb_runtime_context;

// All ODB operations take explicit context:
boolean odbSetValue_context(odb_runtime_context *ctx, bigstring bspath, tyvaluerecord val);
boolean odbSaveFile_context(odb_runtime_context *ctx);

// Cancoon record stores runtime context (no global state):
typedef struct tycancoonrecord {
    odb_runtime_context runtime;  // Embedded runtime context
    boolean accesssing;
} tycancoonrecord;

// Operations update context directly:
boolean langgetdotparams_context(odb_runtime_context *ctx, ...) {
    // Update ctx->root_variable directly (no globals)
    if (root_table_materialized) {
        ctx->root_variable = hvariable;
        ctx->root_table = htable;
    }
}
```

**Migration Strategy:**
1. Create `odb_runtime_context` structure
2. Add `_context` variants of all database functions
3. Maintain backward-compatible wrappers using thread-local context
4. Gradually migrate internal code to use `_context` variants
5. Deprecate global state variables (mark with `DEPRECATED_GLOBAL_STATE`)
6. Final phase: Remove globals entirely

**Pros:**
- ✅ **Eliminates global mutable state entirely**
- ✅ **Enables thread-safe operation** (multiple contexts, one per thread)
- ✅ **Foundation for collaborative ODB** (multiple users, isolated contexts)
- ✅ **Proper architectural solution** (aligns with Phase 2.0 vision)
- ✅ **No stale state bugs** (context is always up-to-date)
- ✅ **Consistent with outline context pattern** (Issue #135)

**Cons:**
- ❌ **Major refactoring effort** (100+ function signatures)
- ❌ **High regression risk** (touches entire database layer)
- ❌ **Cannot be done in time for v7 launch**
- ❌ **Requires extensive testing** (all database operations)

**Risk Assessment:**
- **Too risky for v7 launch** - scope too large
- **Essential for Phase 2** - enables multi-user collaborative ODB

**Effort:** 40-80 hours (3-4 weeks of focused work)

---

### Option 4: Reference Counting for Database Handles

**Description:** Add reference counting to database handles to prevent premature disposal and enable shared ownership across threads.

**Implementation:**
```c
typedef struct tydatabaserecord {
    // ... existing fields ...
    int32_t refcount;  // Reference count
} tydatabaserecord;

// Reference counting operations:
void db_retain(hdldatabaserecord hdb) {
    if (hdb != nil) {
        (**hdb).refcount++;
    }
}

void db_release(hdldatabaserecord hdb) {
    if (hdb != nil && --(**hdb).refcount == 0) {
        // Dispose database when refcount reaches 0
        dbdispose(hdb);
    }
}
```

**Pros:**
- ✅ Enables shared database ownership
- ✅ Prevents use-after-free bugs
- ✅ Foundation for multi-threaded access
- ✅ Similar to outline context reference counting (Issue #135)

**Cons:**
- ❌ **Does not solve the stale cancoon record bug** (Issue #266)
- ❌ Still requires explicit context threading for full solution
- ❌ Reference counting adds overhead to every database operation
- ❌ Risk of reference cycles if not implemented carefully

**Risk Assessment:**
- **Not applicable for v7 launch** - does not fix Issue #266
- **Useful for Phase 2** - complements explicit context threading

**Effort:** 16-24 hours (reference counting infrastructure + testing)

---

## Recommendation

### Phase 1 (v7 Launch) - Immediate Fix

**Use Option 1: Quick Fix - Update Cancoon Record After Lazy Creation**

**Rationale:**
- ✅ **Minimal risk** - surgical change, well-isolated
- ✅ **Fast implementation** - 1-2 hours
- ✅ **Unblocks launch** - fixes P0 bug immediately
- ✅ **Low regression risk** - does not touch existing code paths

**Exit Criteria:**
- `db.save()` correctly saves guest database when system root is loaded
- All db_verbs integration tests pass with `--system-root databases/Frontier.root7`
- Verified with `xxd` that correct database file is written
- No regression in tests without system root

**Implementation Location:**
```c
// In langvalue.c, langgetdotparams() function:
// After line ~3905 where langgettableval() or langexternalgettable() succeeds

if (hsubtable == nil && *htable != nil) {
    // Root table was just materialized - update cancoon record if stale
    if (cancoonglobals != nil) {
        hdltablevariable hv = nil;
        if (tablevaltotable((**(*htable)).hashtablerefcon, &hv, nil) && hv != nil) {
            if ((**cancoonglobals).hrootvariable == nil ||
                (**cancoonglobals).hroottable != *htable) {
                (**cancoonglobals).hrootvariable = (Handle) hv;
                (**cancoonglobals).hroottable = *htable;
                log_debug(LOG_COMP_LANG, "Updated cancoon root: htable=%p", (void *)*htable);
            }
        }
    }
}
```

**File Issue for Phase 2:**
- Title: "Refactor database context to use explicit threading (eliminate global state)"
- Reference: This document, Option 3
- Milestone: Phase 2 (Production Deployment)

---

### Phase 2 (Production Deployment) - Architectural Fix

**Use Option 3: Explicit Database Context Threading**

**Rationale:**
- ✅ **Enables thread-safe operation** (launch requirement for production deployment)
- ✅ **Foundation for collaborative ODB** (multi-user, concurrent access)
- ✅ **Eliminates entire class of bugs** (stale state, context confusion)
- ✅ **Aligns with outline context pattern** (Issue #135)
- ✅ **Proper long-term architecture** (no technical debt accumulation)

**Prerequisites:**
1. Outline context refactoring (Issue #135) complete
2. Learned lessons from outline context migration applied to database context
3. Thread-local storage pattern (ADR-006) fully validated in production

**Execution Plan:**

**Phase 2A: Infrastructure (Week 1-2)**
1. Create `odb_runtime_context` structure
2. Add `_context` variants for core database functions:
   - `langgetdotparams_context`
   - `settablestructureglobals_context`
   - `odbSetValue_context`
   - `odbSaveFile_context`
3. Create thread-local default context for backward compatibility
4. Implement context accessor macros

**Phase 2B: Core Migration (Week 3-4)**
1. Migrate database verb implementations to use `_context` variants
2. Update cancoon record to embed `odb_runtime_context`
3. Refactor `setcancoonglobals()` to work with embedded context
4. Add comprehensive tests for context isolation

**Phase 2C: Deprecation (Week 5-6)**
1. Mark global variables as `DEPRECATED_GLOBAL_STATE`
2. Audit all uses of global database state
3. Migrate remaining callers to `_context` variants
4. Add compile-time warnings for global state access

**Phase 2D: Cleanup (Week 7-8)**
1. Remove global database state variables
2. Remove backward-compatible wrappers
3. Verify thread safety with concurrent test suite
4. Performance testing and optimization

**Testing Strategy:**
- Unit tests: Verify context isolation (multiple contexts, no interference)
- Integration tests: All database verbs with explicit context
- Concurrency tests: Multiple threads accessing different databases
- Stress tests: Rapid context switching, nested operations
- Migration tests: Ensure v6 → v7 migration still works correctly

---

## Testing Strategy for Quick Fix (Option 1)

### Unit Tests

**Test 1: Cancoon record update after lazy root creation**
```bash
# Create empty guest database, populate, verify cancoon record
TESTDIR=$(./tools/get_test_temp_path.sh)
./frontier-cli/frontier-cli --system-root databases/Frontier.root7 -e '
local(dbPath = "'$TESTDIR'/test_lazy_root.root7");
db.new(dbPath);
db.open(dbPath, false);
db.setvalue(dbPath, "testkey", "testvalue");
// Verify cancoon record hrootvariable is non-nil
db.save(dbPath);
db.close(dbPath);
return true
'
```

Expected: SUCCESS (no errors)

**Test 2: Verify correct database is saved**
```bash
# After Test 1, inspect database file to ensure it contains guest data, not system data
xxd $TESTDIR/test_lazy_root.root7 | head -50
```

Expected: Should NOT contain "examples" table or system.* references

**Test 3: Multiple guest databases**
```bash
# Open multiple guest databases, verify each saves correctly
TESTDIR=$(./tools/get_test_temp_path.sh)
./frontier-cli/frontier-cli --system-root databases/Frontier.root7 -e '
local(db1 = "'$TESTDIR'/test_db1.root7");
local(db2 = "'$TESTDIR'/test_db2.root7");
db.new(db1); db.open(db1, false);
db.new(db2); db.open(db2, false);
db.setvalue(db1, "key1", "value1");
db.setvalue(db2, "key2", "value2");
db.save(db1);
db.save(db2);
db.close(db1);
db.close(db2);
return true
'
```

Expected: SUCCESS (both databases save correctly)

### Integration Tests

**Run full db_verbs test suite with system root:**
```bash
cd tests
make test-integration FRONTIER_CLI_ARGS="--system-root ../databases/Frontier.root7"
```

Expected: All db_verbs tests PASS

### Regression Tests

**Verify tests without system root still pass:**
```bash
cd tests
make test-integration
```

Expected: No regression (all tests still PASS)

---

## Migration Considerations (Phase 2)

### Backward Compatibility

**Maintain existing API surface:**
- Keep global state accessors during migration
- Provide `_context` variants alongside existing functions
- Use thread-local default context for backward compatibility
- Gradual migration path (can be done incrementally)

### Database Format Impact

**None** - This is a runtime architecture change only:
- v7 database format is unchanged
- v6 → v7 migration is unaffected
- On-disk representation is identical

### Headless Focus

**Headless-only migration:**
- Implementation focuses exclusively on headless builds
- No GUI dependencies or GUI-specific code paths
- Thread safety validation in headless environment only

### Performance Impact

**Minimal expected overhead:**
- Context passing adds 1 pointer parameter (8 bytes on 64-bit)
- No heap allocations (context embedded in cancoon record)
- Context access is direct (no hash lookups or indirection)
- Potential optimization: inline context accessors

---

## Related Work and Precedents

### ADR-006: Thread-Local Storage Pattern

**Lessons learned:**
- Thread-local storage works for per-thread state (`flnextparamislast`)
- Zero API changes possible with macro accessors
- Transparent to existing code (backward compatible)

**Applicability to database context:**
- ✅ Can use thread-local default context for backward compatibility
- ✅ Macro accessors can hide context passing initially
- ❌ Thread-local alone insufficient - need explicit context for multi-database scenarios

### Issue #135: Outline Context Refactoring

**Parallel problem:**
- Outline context has same global state issues
- Needs reference counting for multi-user collaborative editing
- Explicit context threading required

**Shared solution:**
- Apply same pattern to database context
- Reuse reference counting infrastructure
- Consistent API design across subsystems

### Issue #264: Context Guard Pattern Issues

**Related bug:**
- Context guard pattern has disposal/restoration issues
- Guards work for temporary operations but fail for persistent state
- Bidirectional guards (Option 2) may have similar issues

**Implications:**
- Option 1 (quick fix) avoids context guard complexity
- Option 3 (explicit context) eliminates need for guards entirely

---

## Open Questions

### For TPM/CTO Decision

1. **Phase 1 timing:** Can we afford 1-2 hours for Option 1 implementation before v7 launch?
2. **Phase 2 priority:** Should database context refactoring (Option 3) be prioritized over other Phase 2 work?
3. **Thread safety requirements:** What level of concurrent access is required for enterprise partnerships?
4. **Resource allocation:** Can we dedicate 3-4 weeks for Phase 2 database context refactoring?

### For Further Investigation

1. **Other lazy creation sites:** Are there other code paths where tables are lazily created that may have the same bug?
2. **External table variables:** Do external table variables (non-root tables) have similar stale state issues?
3. **Hash table stack:** Does `hashtablestack` have similar staleness problems?
4. **Multi-database nesting:** What happens when database operations nest (db.open inside db verb)?

---

## Exit Criteria

### Phase 1 (v7 Launch)

- [x] **Problem understood:** Full analysis of global state anti-pattern complete
- [ ] **Quick fix implemented:** Option 1 code changes in place
- [ ] **Tests passing:** All db_verbs tests pass with `--system-root`
- [ ] **Verified correct behavior:** `db.save()` saves correct database (inspected with `xxd`)
- [ ] **No regressions:** Tests without system root still pass
- [ ] **Issue filed for Phase 2:** Architectural fix tracked for future work

### Phase 2 (Production Deployment)

- [ ] **Infrastructure complete:** `odb_runtime_context` structure and `_context` variants implemented
- [ ] **Core migration complete:** Database verbs use explicit context
- [ ] **Cancoon record refactored:** Embedded context, no global state
- [ ] **Thread safety validated:** Concurrent test suite passes
- [ ] **Performance acceptable:** No measurable overhead from context passing
- [ ] **Documentation updated:** API docs reflect new context-based design
- [ ] **Global state removed:** All database globals deprecated and removed

---

## Conclusion

The database context global state anti-pattern is a **foundational architectural flaw** that:
1. Causes immediate P0 bug (Issue #266) blocking v7 launch
2. Prevents thread-safe operation required for enterprise partnerships
3. Blocks collaborative ODB features (Phase 2.0 vision)

**For v7 Launch:** Use Option 1 (quick fix) to unblock immediately with minimal risk.

**For Phase 2:** Invest in Option 3 (explicit context threading) to eliminate global state entirely and enable collaborative ODB vision.

This is not just a bug fix - it's a **critical architectural decision** that determines whether Frontier can support multi-user collaborative editing at scale.
