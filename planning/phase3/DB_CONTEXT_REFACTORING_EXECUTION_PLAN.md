# Database Context Refactoring - Detailed Execution Plan (Phase 2)

**Document Status:** Ready for TPM/CTO Decision
**Created:** 2026-01-10
**Author:** System Architect
**Related Issues:** #266 (P0), #264, #135
**Related ADRs:** ADR-005 (thread-local pattern), ADR-006 (outline push/pop elimination)
**Prerequisites:** Phase 1 (Quick Fix from PR #267) complete

## Executive Summary

This document provides a step-by-step execution plan for **Phase 2: Explicit Database Context Threading**, eliminating global mutable state in the ODB system. This is the architectural fix that enables thread-safe operation and collaborative ODB editing.

**Scope:** Comprehensive refactoring of database context management from global state to explicit context threading.

**Effort Estimate:** 40-80 hours (3-4 weeks of focused work)

**Complexity:** HIGH - Touches 100+ function signatures across 15+ files

**Strategic Value:** CRITICAL - Unblocks collaborative ODB (Phase 2.0 vision) and enterprise partnerships

**Risk Level:** MEDIUM-HIGH during implementation, LOW after validation (well-understood patterns)

---

## Table of Contents

1. [Current State Analysis](#current-state-analysis)
2. [Target Architecture](#target-architecture)
3. [Phase 2A: Infrastructure (Week 1-2)](#phase-2a-infrastructure-week-1-2)
4. [Phase 2B: Core Migration (Week 3-4)](#phase-2b-core-migration-week-3-4)
5. [Phase 2C: Deprecation (Week 5-6)](#phase-2c-deprecation-week-5-6)
6. [Phase 2D: Cleanup (Week 7-8)](#phase-2d-cleanup-week-7-8)
7. [Testing Strategy](#testing-strategy)
8. [Migration Checkpoints](#migration-checkpoints)
9. [Rollback Plan](#rollback-plan)
10. [Risk Assessment](#risk-assessment)

---

## Current State Analysis

### Global Variables to Eliminate

**Location: `Common/source/tablestructure.c`**
```c
Handle rootvariable = nil;         // Line 110 - Current root table variable
hdlhashtable roottable = nil;      // Line 112 - Current root hash table
```

**Location: `Common/source/db.c`**
```c
hdldatabaserecord databasedata;    // Line 192 - Current database handle
```

**Location: `Common/source/langhash.c`**
```c
hdlhashtable currenthashtable = nil;  // Line 835 - Current hash table context
hdltablestack hashtablestack = nil;   // (nearby) - Hash table stack
```

**Total: 5 global variables** managing database runtime state.

### Current Cancoon Record Structure

**Location: `Common/source/odbengine.c:56-72`**

```c
typedef struct tycancoonrecord {
    hdldatabaserecord hdatabase;      // Database handle
    hdlhashtable hroottable;          // Root table (CAN BE STALE!)
    Handle hrootvariable;             // Root variable (CAN BE STALE!)
    hdltablestack htablestack;        // Table stack
    boolean accesssing;               // Access flag
    WindowPtr shellwindow;            // GUI state (unused in headless)
} tycancoonrecord;
```

**Problem:** Cancoon record stores snapshot of state, but `setcancoonglobals()` restores globals from this snapshot. When globals are updated by child operations (lazy root table creation), the snapshot becomes stale.

### Call Sites for setcancoonglobals()

**Location: `Common/source/odbengine.c`**

- Line 223: `setcancoonglobals()` function definition
- Line 685: `odbNewFile()` - Sets globals for new database
- Line 846: `loadversion2cancoonfile()` - Loads v6 Cancoon record
- Line 871: `odbSetValue()` - **CRITICAL** - Called before every db.setvalue()
- Line 899: `odbGetValue()` - Called before db.getvalue()
- Line 917: `odbGetType()` - Called before db.gettype()
- Line 943: `odbDefined()` - Called before db.defined()
- Line 983: `odbDelete()` - Called before db.delete()
- Line 1024: `odbNewTable()` - Called before db.newtable()
- Line 1055: `odbCountItems()` - Called before db.countitems()
- Line 1083: `odbGetNthItem()` - Called before db.getnthitem()
- Line 1135: `odbGetModDate()` - Called before db.getmoddate()

**Total: 11 call sites** in database verb implementations (plus 2 in initialization code).

**Location: `Common/source/cancoon.c`**
- Line 536: `setcancoonglobals()` function definition (GUI version)
- Lines 748, 775, 992, 1004, 1355, 1397, 1454, 1744, 1766: Various GUI operations

**Note:** GUI-specific call sites in `cancoon.c` are OUT OF SCOPE for headless refactoring.

### Functions That Update Global State

**Root Table Materialization:**
- `langgetdotparams()` → `settablestructureglobals()` - Updates `rootvariable`, `roottable`
- `tablevaltotable()` → `tableverbinmemory()` - Loads external tables into memory

**Database Operations:**
- `dbopenfile()` - Sets `databasedata` global
- `dbclose()` - Clears `databasedata` global
- `hashtablelookup()` - May update `currenthashtable` implicitly

**Stack Operations:**
- `pushhashtable()` / `pophashtable()` - Modifies `hashtablestack`
- Nested table lookups build stack of scopes

---

## Target Architecture

### New Context Structure

**Location: `Common/headers/tablestructure.h` (to be created section)**

```c
/*
 * odb_runtime_context - Explicit database runtime context
 *
 * Replaces global variables:
 *   - rootvariable (tablestructure.c)
 *   - roottable (tablestructure.c)
 *   - databasedata (db.c)
 *   - currenthashtable (langhash.c)
 *   - hashtablestack (langhash.c)
 *
 * Thread-safety: Each thread maintains its own context.
 * Lifecycle: Created when database opened, destroyed when closed.
 * Ownership: Embedded in cancoon record (single owner).
 */
typedef struct odb_runtime_context {
    // Database context
    hdldatabaserecord database;        // Current database (was: databasedata)

    // Root table context
    hdlhashtable root_table;           // Root table (was: roottable)
    Handle root_variable;              // Root variable (was: rootvariable)

    // Hash table context
    hdlhashtable current_hashtable;    // Current scope (was: currenthashtable)
    hdltablestack table_stack;         // Table stack (was: hashtablestack)

    // Reserved for future expansion (collaborative ODB fields)
    void *reserved[4];

} odb_runtime_context;
```

**Size:** ~56 bytes (5 pointers + 4 reserved pointers = 9 * 8 bytes on 64-bit)

**Alignment:** Natural pointer alignment (no special packing needed)

### Refactored Cancoon Record

**Location: `Common/source/odbengine.c:56-72` (updated)**

```c
typedef struct tycancoonrecord {
    // Runtime context (replaces individual fields)
    odb_runtime_context runtime;      // Embedded runtime context

    // UI state (preserved for GUI builds, unused in headless)
    boolean accesssing;               // Access flag
    WindowPtr shellwindow;            // GUI window (headless: always nil)

} tycancoonrecord;
```

**Before:**
- 6 fields (56 bytes)
- Stale state problem (hrootvariable, hroottable)

**After:**
- Embedded runtime context (56 bytes)
- 2 legacy fields (9 bytes)
- Total: ~65 bytes (small increase, better organization)

### Context Threading Pattern

**Pattern 1: Database Verb Operations**

```c
// BEFORE (uses globals):
boolean odbSetValue(odbref odb, bigstring bspath, odbValueRecord *value) {
    hdlcancoonrecord hc = (hdlcancoonrecord) odb;
    setcancoonglobals(hc);  // Restores globals from snapshot (STALE!)

    // Operations use global rootvariable, databasedata, etc.
    langexpandtodotparams(bspath, &htable, bsname);
    // ...
}

// AFTER (uses explicit context):
boolean odbSetValue_context(odb_runtime_context *ctx, bigstring bspath, odbValueRecord *value) {
    // Operations pass ctx explicitly
    langexpandtodotparams_context(ctx, bspath, &htable, bsname);
    // ...
}

// Backward-compatible wrapper:
boolean odbSetValue(odbref odb, bigstring bspath, odbValueRecord *value) {
    hdlcancoonrecord hc = (hdlcancoonrecord) odb;
    return odbSetValue_context(&(**hc).runtime, bspath, value);
}
```

**Pattern 2: Table Structure Operations**

```c
// BEFORE (updates globals):
void settablestructureglobals(Handle hvariable, boolean flclearall) {
    rootvariable = hvariable;  // GLOBAL MUTATION
    if (hvariable != nil) {
        roottable = (hdlhashtable) (**hvariable).variabledata;  // GLOBAL MUTATION
    }
}

// AFTER (updates context):
void settablestructureglobals_context(odb_runtime_context *ctx, Handle hvariable) {
    ctx->root_variable = hvariable;  // Context mutation (safe)
    if (hvariable != nil) {
        ctx->root_table = (hdlhashtable) (*(hdltablevariable*)hvariable)->variabledata;
    }
}

// Backward-compatible wrapper (uses thread-local default context):
void settablestructureglobals(Handle hvariable, boolean flclearall) {
    odb_runtime_context *ctx = odb_get_thread_default_context();
    if (ctx != nil) {
        settablestructureglobals_context(ctx, hvariable);
    }
    // Also update globals for legacy callers (Phase 2C deprecation path)
    rootvariable = hvariable;
    if (hvariable != nil) {
        roottable = (hdlhashtable) (*(hdltablevariable*)hvariable)->variabledata;
    }
}
```

**Pattern 3: Thread-Local Default Context (Backward Compatibility)**

```c
// Thread-local storage for default context (temporary during migration)
static __thread odb_runtime_context *thread_default_odb_context = nil;

// Set default context for current thread
void odb_set_thread_default_context(odb_runtime_context *ctx) {
    thread_default_odb_context = ctx;
}

// Get default context for current thread
odb_runtime_context *odb_get_thread_default_context(void) {
    return thread_default_odb_context;
}

// Example usage in odbSetValue wrapper:
boolean odbSetValue(odbref odb, bigstring bspath, odbValueRecord *value) {
    hdlcancoonrecord hc = (hdlcancoonrecord) odb;
    odb_runtime_context *ctx = &(**hc).runtime;

    // Set thread-local default for legacy callers
    odb_runtime_context *prev_ctx = odb_get_thread_default_context();
    odb_set_thread_default_context(ctx);

    boolean result = odbSetValue_context(ctx, bspath, value);

    // Restore previous context
    odb_set_thread_default_context(prev_ctx);

    return result;
}
```

---

## Phase 2A: Infrastructure (Week 1-2)

**Goal:** Create context structure, add `_context` variants for core functions, establish thread-local default context pattern.

**Estimated Effort:** 16-20 hours

### Step 2A-1: Create odb_runtime_context Structure

**Files Modified:**
- `Common/headers/tablestructure.h`

**Changes:**

1. Add `odb_runtime_context` structure definition (see Target Architecture section above)

2. Add context initialization function:

```c
/*
 * odb_init_context - Initialize runtime context
 *
 * Initializes all fields to nil/default values.
 * Call this when creating a new database or opening an existing database.
 */
void odb_init_context(odb_runtime_context *ctx);
```

3. Add context copy function (for context guards):

```c
/*
 * odb_copy_context - Deep copy runtime context
 *
 * Used by context guards to save/restore state.
 * Does NOT copy heap data (handles are shared).
 */
void odb_copy_context(const odb_runtime_context *src, odb_runtime_context *dst);
```

**Implementation Location:** `Common/source/tablestructure.c`

```c
void odb_init_context(odb_runtime_context *ctx) {
    if (ctx == nil) {
        return;
    }

    ctx->database = nil;
    ctx->root_table = nil;
    ctx->root_variable = nil;
    ctx->current_hashtable = nil;
    ctx->table_stack = nil;

    // Clear reserved fields
    memset(ctx->reserved, 0, sizeof(ctx->reserved));
}

void odb_copy_context(const odb_runtime_context *src, odb_runtime_context *dst) {
    if (src == nil || dst == nil) {
        return;
    }

    // Shallow copy (handles are shared, not duplicated)
    dst->database = src->database;
    dst->root_table = src->root_table;
    dst->root_variable = src->root_variable;
    dst->current_hashtable = src->current_hashtable;
    dst->table_stack = src->table_stack;

    // Don't copy reserved fields (future use)
}
```

**Testing Checkpoint 2A-1:**

```bash
# Verify compilation
make clean && make

# Verify context initialization
./frontier-cli/frontier-cli -e '
local(ctx);
// Can't test directly (no UserTalk exposure), but compilation success means structure is valid
return true
'
```

Expected: Compilation succeeds, no errors.

---

### Step 2A-2: Add Thread-Local Default Context

**Files Modified:**
- `Common/source/tablestructure.c`
- `Common/headers/tablestructure.h`

**Changes:**

**Header (`tablestructure.h`):**

```c
/*
 * Thread-local default context (backward compatibility during migration)
 *
 * Used by legacy wrapper functions to access context without explicit parameter.
 * DEPRECATED: Will be removed in Phase 2D once all callers migrated to _context variants.
 */
void odb_set_thread_default_context(odb_runtime_context *ctx);
odb_runtime_context *odb_get_thread_default_context(void);
```

**Implementation (`tablestructure.c`):**

```c
// Thread-local storage for default context
// Note: Uses C11 _Thread_local for portability (fallback to __thread if not available)
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    _Thread_local odb_runtime_context *thread_default_odb_context = nil;
#elif defined(__GNUC__)
    __thread odb_runtime_context *thread_default_odb_context = nil;
#else
    #error "Thread-local storage not supported on this compiler"
#endif

void odb_set_thread_default_context(odb_runtime_context *ctx) {
    thread_default_odb_context = ctx;
}

odb_runtime_context *odb_get_thread_default_context(void) {
    return thread_default_odb_context;
}
```

**Testing Checkpoint 2A-2:**

```c
// Unit test in tests/headless_db_verbs.c

void test_thread_local_context(void) {
    odb_runtime_context ctx1, ctx2;
    odb_init_context(&ctx1);
    odb_init_context(&ctx2);

    // Test set/get
    odb_set_thread_default_context(&ctx1);
    assert(odb_get_thread_default_context() == &ctx1);

    // Test overwrite
    odb_set_thread_default_context(&ctx2);
    assert(odb_get_thread_default_context() == &ctx2);

    // Test clear
    odb_set_thread_default_context(nil);
    assert(odb_get_thread_default_context() == nil);

    printf("✓ Thread-local context works\n");
}
```

Expected: Test passes, thread-local storage works correctly.

---

### Step 2A-3: Create _context Variants for Core Functions

**Goal:** Add explicit context parameter variants for frequently-called functions.

**Functions to Create (Priority Order):**

#### Tier 1: Table Structure Functions

**File: `Common/source/tablestructure.c`**

1. **settablestructureglobals_context**

```c
// BEFORE (line ~156):
void settablestructureglobals(Handle hvariable, boolean flclearall) {
    rootvariable = hvariable;  // Global mutation
    if (hvariable != nil) {
        roottable = (hdlhashtable) (*(hdltablevariable*)hvariable)->variabledata;
    }
    // ... other globals ...
}

// AFTER (new function):
void settablestructureglobals_context(odb_runtime_context *ctx, Handle hvariable) {
    if (ctx == nil) {
        return;
    }

    ctx->root_variable = hvariable;

    if (hvariable != nil) {
        ctx->root_table = (hdlhashtable) (*(hdltablevariable*)hvariable)->variabledata;
    } else {
        ctx->root_table = nil;
    }

    // Update current_hashtable to match root_table
    ctx->current_hashtable = ctx->root_table;
}

// Backward-compatible wrapper (Phase 2B: update to use thread-local default):
void settablestructureglobals(Handle hvariable, boolean flclearall) {
    // Legacy behavior: Update globals
    rootvariable = hvariable;
    if (hvariable != nil) {
        roottable = (hdlhashtable) (*(hdltablevariable*)hvariable)->variabledata;
    } else {
        roottable = nil;
    }

    // Also update thread-local default context if present
    odb_runtime_context *ctx = odb_get_thread_default_context();
    if (ctx != nil) {
        settablestructureglobals_context(ctx, hvariable);
    }
}
```

2. **gettablestructureglobals_context**

```c
// New function (inverse of settablestructureglobals_context):
void gettablestructureglobals_context(const odb_runtime_context *ctx, Handle *hvariable, hdlhashtable *htable) {
    if (ctx == nil) {
        if (hvariable != nil) *hvariable = nil;
        if (htable != nil) *htable = nil;
        return;
    }

    if (hvariable != nil) {
        *hvariable = ctx->root_variable;
    }

    if (htable != nil) {
        *htable = ctx->root_table;
    }
}
```

#### Tier 2: Hash Table Functions

**File: `Common/source/langhash.c`**

1. **hashtablelookup_context**

```c
// BEFORE (reads global currenthashtable):
boolean hashtablelookup(hdlhashtable htable, bigstring bsname, tyvaluerecord *vreturned, hdlhashnode *hnode) {
    // ... uses currenthashtable implicitly ...
}

// AFTER (accepts explicit context):
boolean hashtablelookup_context(odb_runtime_context *ctx, hdlhashtable htable, bigstring bsname, tyvaluerecord *vreturned, hdlhashnode *hnode) {
    // Save current_hashtable, set new one
    hdlhashtable prev_hashtable = nil;
    if (ctx != nil) {
        prev_hashtable = ctx->current_hashtable;
        ctx->current_hashtable = htable;
    }

    // Call original implementation
    boolean result = hashtablelookup(htable, bsname, vreturned, hnode);

    // Restore previous hashtable
    if (ctx != nil) {
        ctx->current_hashtable = prev_hashtable;
    }

    return result;
}

// Backward-compatible wrapper:
boolean hashtablelookup(hdlhashtable htable, bigstring bsname, tyvaluerecord *vreturned, hdlhashnode *hnode) {
    odb_runtime_context *ctx = odb_get_thread_default_context();
    if (ctx != nil) {
        return hashtablelookup_context(ctx, htable, bsname, vreturned, hnode);
    }

    // Fallback: Use global currenthashtable (legacy path)
    hdlhashtable prev = currenthashtable;
    currenthashtable = htable;

    // ... original implementation ...

    currenthashtable = prev;
    return result;
}
```

2. **pushhashtable_context / pophashtable_context**

```c
// Stack operations with explicit context:
boolean pushhashtable_context(odb_runtime_context *ctx, hdlhashtable htable) {
    if (ctx == nil) {
        return false;
    }

    // Allocate new stack node
    hdltablestack hstack = nil;
    if (!newfilledhandle(&hstack, sizeof(tytablestack))) {
        return false;
    }

    (**hstack).htable = htable;
    (**hstack).hnext = ctx->table_stack;
    ctx->table_stack = hstack;

    return true;
}

boolean pophashtable_context(odb_runtime_context *ctx, hdlhashtable *htable) {
    if (ctx == nil || ctx->table_stack == nil) {
        return false;
    }

    hdltablestack hstack = ctx->table_stack;
    if (htable != nil) {
        *htable = (**hstack).htable;
    }

    ctx->table_stack = (**hstack).hnext;
    disposehandle((Handle) hstack);

    return true;
}
```

#### Tier 3: Language Value Functions

**File: `Common/source/langvalue.c`**

1. **langgetdotparams_context**

**This is the CRITICAL function** that updates global `rootvariable` during lazy root table creation (Issue #266 root cause).

```c
// BEFORE (line ~3850):
boolean langgetdotparams(hdltreenode htree, hdlhashtable *htable, bigstring bsname) {
    // ... code ...

    // Line ~3905: This is where rootvariable gets updated
    if (langgettableval(hlist, &val, &hnode)) {
        tablevaltotable(val, htable, hnode);
        settablestructureglobals(hvariable, false);  // GLOBAL MUTATION!
    }

    // ... more code ...
}

// AFTER (new function with explicit context):
boolean langgetdotparams_context(odb_runtime_context *ctx, hdltreenode htree, hdlhashtable *htable, bigstring bsname) {
    // ... same logic, but use ctx instead of globals ...

    if (langgettableval_context(ctx, hlist, &val, &hnode)) {
        tablevaltotable_context(ctx, val, htable, hnode);
        settablestructureglobals_context(ctx, hvariable);  // Context mutation (safe!)
    }

    // ... more code ...
}

// Backward-compatible wrapper:
boolean langgetdotparams(hdltreenode htree, hdlhashtable *htable, bigstring bsname) {
    odb_runtime_context *ctx = odb_get_thread_default_context();
    if (ctx != nil) {
        return langgetdotparams_context(ctx, htree, htable, bsname);
    }

    // Fallback: Use globals (legacy behavior)
    // ... original implementation unchanged ...
}
```

2. **langexpandtodotparams_context**

```c
// BEFORE:
boolean langexpandtodotparams(bigstring bspath, hdlhashtable *htable, bigstring bsname);

// AFTER (new function):
boolean langexpandtodotparams_context(odb_runtime_context *ctx, bigstring bspath, hdlhashtable *htable, bigstring bsname) {
    // ... use langgetdotparams_context internally ...
    return langgetdotparams_context(ctx, htree, htable, bsname);
}

// Backward-compatible wrapper:
boolean langexpandtodotparams(bigstring bspath, hdlhashtable *htable, bigstring bsname) {
    odb_runtime_context *ctx = odb_get_thread_default_context();
    if (ctx != nil) {
        return langexpandtodotparams_context(ctx, bspath, htable, bsname);
    }
    // ... legacy implementation ...
}
```

#### Tier 4: ODB Engine Functions

**File: `Common/source/odbengine.c`**

1. **odbSetValue_context**

```c
// BEFORE (line ~973):
boolean odbSetValue(odbref odb, bigstring bspath, odbValueRecord *value) {
    hdlcancoonrecord hc = (hdlcancoonrecord) odb;

    setcancoonglobals(hc);  // Restores globals from STALE snapshot

    langexpandtodotparams(bspath, &htable, bsname);  // Uses globals
    // ...
}

// AFTER (new function):
boolean odbSetValue_context(odb_runtime_context *ctx, bigstring bspath, odbValueRecord *value) {
    hdlhashtable htable;
    bigstring bsname;

    // Use context explicitly (no global state)
    if (!langexpandtodotparams_context(ctx, bspath, &htable, bsname)) {
        return false;
    }

    tyvaluerecord val;
    if (!convertfromodbvalue(value, &val)) {
        return false;
    }

    return hashtableassign(htable, bsname, val);
}

// Backward-compatible wrapper:
boolean odbSetValue(odbref odb, bigstring bspath, odbValueRecord *value) {
    hdlcancoonrecord hc = (hdlcancoonrecord) odb;

    // Set thread-local default context
    odb_runtime_context *prev_ctx = odb_get_thread_default_context();
    odb_set_thread_default_context(&(**hc).runtime);

    // Call context variant
    boolean result = odbSetValue_context(&(**hc).runtime, bspath, value);

    // Restore previous context
    odb_set_thread_default_context(prev_ctx);

    return result;
}
```

2. **odbSaveFile_context**

```c
// BEFORE (line ~669):
boolean odbSaveFile(odbref odb) {
    hdlcancoonrecord hc = (hdlcancoonrecord) odb;

    setcancoonglobals(hc);  // Restores globals from STALE snapshot

    // Uses global rootvariable (which may be wrong!)
    if (rootvariable == nil) {
        // ...
    }
}

// AFTER (new function):
boolean odbSaveFile_context(odb_runtime_context *ctx) {
    // Use context explicitly
    if (ctx->root_variable == nil) {
        // Empty database - no root table to save
        dbsetview(cancoonview, nildbaddress);
        return true;
    }

    // Save root table using context
    dbaddress root_adr;
    if (!tablesavesystemtable_context(ctx, ctx->root_variable, &root_adr)) {
        return false;
    }

    dbsetview(cancoonview, root_adr);
    return true;
}

// Backward-compatible wrapper:
boolean odbSaveFile(odbref odb) {
    hdlcancoonrecord hc = (hdlcancoonrecord) odb;

    // Set thread-local default context
    odb_runtime_context *prev_ctx = odb_get_thread_default_context();
    odb_set_thread_default_context(&(**hc).runtime);

    // Call context variant
    boolean result = odbSaveFile_context(&(**hc).runtime);

    // Restore previous context
    odb_set_thread_default_context(prev_ctx);

    return result;
}
```

**Complete List of _context Variants to Create (Tier 1-4):**

| Function | File | Priority | Estimated Lines |
|----------|------|----------|----------------|
| settablestructureglobals_context | tablestructure.c | P0 | 15 |
| gettablestructureglobals_context | tablestructure.c | P0 | 10 |
| langgetdotparams_context | langvalue.c | P0 | 50 |
| langexpandtodotparams_context | langvalue.c | P0 | 20 |
| hashtablelookup_context | langhash.c | P1 | 25 |
| pushhashtable_context | langhash.c | P1 | 20 |
| pophashtable_context | langhash.c | P1 | 15 |
| odbSetValue_context | odbengine.c | P0 | 30 |
| odbSaveFile_context | odbengine.c | P0 | 40 |
| odbOpenFile_context | odbengine.c | P1 | 60 |
| odbGetValue_context | odbengine.c | P2 | 25 |
| odbDefined_context | odbengine.c | P2 | 20 |
| odbDelete_context | odbengine.c | P2 | 25 |

**Total: ~355 lines of new code** (rough estimate)

---

### Step 2A-4: Update Cancoon Record Structure

**Files Modified:**
- `Common/source/odbengine.c`

**Changes:**

1. **Refactor tycancoonrecord structure** (line ~56):

```c
// BEFORE:
typedef struct tycancoonrecord {
    hdldatabaserecord hdatabase;
    hdlhashtable hroottable;          // STALE!
    Handle hrootvariable;             // STALE!
    hdltablestack htablestack;
    boolean accesssing;
    WindowPtr shellwindow;
} tycancoonrecord;

// AFTER:
typedef struct tycancoonrecord {
    // Runtime context (replaces hdatabase, hroottable, hrootvariable, htablestack)
    odb_runtime_context runtime;

    // Legacy fields (preserved for GUI compatibility)
    boolean accesssing;
    WindowPtr shellwindow;

} tycancoonrecord;
```

2. **Update newcancoonrecord()** to initialize runtime context:

```c
// BEFORE:
static boolean newcancoonrecord(hdlcancoonrecord *hcancoon) {
    hdlcancoonrecord hc;

    if (!newclearhandle(sizeof(tycancoonrecord), (Handle *) &hc))
        return (false);

    (**hc).hdatabase = nil;
    (**hc).hroottable = nil;
    (**hc).hrootvariable = nil;
    (**hc).htablestack = nil;
    // ...
}

// AFTER:
static boolean newcancoonrecord(hdlcancoonrecord *hcancoon) {
    hdlcancoonrecord hc;

    if (!newclearhandle(sizeof(tycancoonrecord), (Handle *) &hc))
        return (false);

    // Initialize runtime context
    odb_init_context(&(**hc).runtime);

    // Initialize legacy fields
    (**hc).accesssing = false;
    (**hc).shellwindow = nil;

    *hcancoon = hc;
    return true;
}
```

3. **Update setcancoonglobals()** to use runtime context:

```c
// BEFORE (line ~223):
static void setcancoonglobals(hdlcancoonrecord hcancoon) {
    hdlcancoonrecord hc = hcancoon;

    databasedata = (**hc).hdatabase;
    hashtablestack = (**hc).htablestack;
    settablestructureglobals((**hc).hrootvariable, false);  // Uses STALE hrootvariable!
    currenthashtable = roottable;
    cancoonglobals = hc;
}

// AFTER:
static void setcancoonglobals(hdlcancoonrecord hcancoon) {
    hdlcancoonrecord hc = hcancoon;
    odb_runtime_context *ctx = &(**hc).runtime;

    // Update globals from context (backward compatibility)
    databasedata = ctx->database;
    hashtablestack = ctx->table_stack;
    rootvariable = ctx->root_variable;
    roottable = ctx->root_table;
    currenthashtable = ctx->current_hashtable;
    cancoonglobals = hc;

    // Set thread-local default context
    odb_set_thread_default_context(ctx);
}
```

**Note:** This updated `setcancoonglobals()` is a transitional implementation. In Phase 2C, we'll deprecate this function entirely.

---

### Testing Checkpoint 2A (End of Week 1-2)

**Unit Tests:**

```c
// tests/headless_db_verbs.c

void test_odb_context_initialization(void) {
    odb_runtime_context ctx;
    odb_init_context(&ctx);

    assert(ctx.database == nil);
    assert(ctx.root_table == nil);
    assert(ctx.root_variable == nil);
    assert(ctx.current_hashtable == nil);
    assert(ctx.table_stack == nil);

    printf("✓ Context initialization works\n");
}

void test_odb_context_copy(void) {
    odb_runtime_context src, dst;
    odb_init_context(&src);
    odb_init_context(&dst);

    // Set some values in src
    src.database = (hdldatabaserecord) 0x1234;
    src.root_table = (hdlhashtable) 0x5678;

    odb_copy_context(&src, &dst);

    assert(dst.database == src.database);
    assert(dst.root_table == src.root_table);

    printf("✓ Context copy works\n");
}
```

**Integration Tests:**

```yaml
# tests/integration/db_verbs/context_isolation.yaml

name: "Database Context Isolation Tests"
description: "Verify runtime context is properly isolated per database"

tests:
  - name: "db.setvalue with context - no global corruption"
    script: |
      local(dbPath = "{FRONTIER_TEST_TMP_DIR}/test_context.root7");
      db.new(dbPath);
      db.open(dbPath, false);
      db.setvalue(dbPath, "key1", "value1");
      db.save(dbPath);
      db.close(dbPath);
      return true
    expected_success: true

  - name: "Multiple databases - independent contexts"
    script: |
      local(db1 = "{FRONTIER_TEST_TMP_DIR}/test_db1.root7");
      local(db2 = "{FRONTIER_TEST_TMP_DIR}/test_db2.root7");

      db.new(db1);
      db.new(db2);
      db.open(db1, false);
      db.open(db2, false);

      db.setvalue(db1, "key", "value1");
      db.setvalue(db2, "key", "value2");

      db.save(db1);
      db.save(db2);

      // Verify correct values saved to correct databases
      local(val1);
      db.getvalue(db1, "key", @val1);
      assert(val1 == "value1", "db1 has wrong value");

      local(val2);
      db.getvalue(db2, "key", @val2);
      assert(val2 == "value2", "db2 has wrong value");

      db.close(db1);
      db.close(db2);
      return true
    expected_success: true
```

**Success Criteria for Phase 2A:**

- ✅ All unit tests pass
- ✅ Integration tests pass (context_isolation.yaml)
- ✅ No regression in existing db_verbs tests
- ✅ Compilation succeeds with no warnings
- ✅ Thread-local default context works correctly
- ✅ Cancoon record structure updated successfully
- ✅ `_context` variants compile and link correctly

**Deliverables:**

- [ ] `odb_runtime_context` structure created
- [ ] 13 `_context` variant functions implemented
- [ ] Thread-local default context infrastructure working
- [ ] Cancoon record refactored to embed runtime context
- [ ] Unit tests passing
- [ ] Integration tests passing
- [ ] Documentation updated (inline comments)

**Risk Mitigation:**

- If any test fails: Revert to Phase 1 quick fix, re-analyze issue
- If compilation fails: Check macro definitions, structure alignment
- If thread-local storage fails: Fallback to global context (log warning)

---

## Phase 2B: Core Migration (Week 3-4)

**Goal:** Migrate all database verb implementations to use `_context` variants, update lazy root table creation to update cancoon record.

**Estimated Effort:** 20-24 hours

### Step 2B-1: Migrate Database Verb Implementations

**Files Modified:**
- `Common/source/odbengine.c`

**Verbs to Migrate (Priority Order):**

1. ✅ **odbSetValue** (already migrated in Phase 2A)
2. ✅ **odbSaveFile** (already migrated in Phase 2A)
3. **odbOpenFile**
4. **odbGetValue**
5. **odbDefined**
6. **odbDelete**
7. **odbGetType**
8. **odbNewTable**
9. **odbCountItems**
10. **odbGetNthItem**
11. **odbGetModDate**

**Example Migration: odbGetValue**

```c
// BEFORE (line ~889):
boolean odbGetValue(odbref odb, bigstring bspath, odbValueRecord *value) {
    hdlcancoonrecord hc = (hdlcancoonrecord) odb;
    hdlhashtable htable;
    bigstring bsname;

    setcancoonglobals(hc);  // STALE GLOBALS!

    if (!langexpandtodotparams(bspath, &htable, bsname))
        return (false);

    tyvaluerecord val;
    if (!hashtablelookup(htable, bsname, &val, nil))
        return (false);

    return converttoodbvalue(&val, value);
}

// AFTER (new _context variant):
boolean odbGetValue_context(odb_runtime_context *ctx, bigstring bspath, odbValueRecord *value) {
    hdlhashtable htable;
    bigstring bsname;

    if (!langexpandtodotparams_context(ctx, bspath, &htable, bsname))
        return (false);

    tyvaluerecord val;
    if (!hashtablelookup_context(ctx, htable, bsname, &val, nil))
        return (false);

    return converttoodbvalue(&val, value);
}

// Backward-compatible wrapper:
boolean odbGetValue(odbref odb, bigstring bspath, odbValueRecord *value) {
    hdlcancoonrecord hc = (hdlcancoonrecord) odb;

    // Set thread-local default context
    odb_runtime_context *prev_ctx = odb_get_thread_default_context();
    odb_set_thread_default_context(&(**hc).runtime);

    boolean result = odbGetValue_context(&(**hc).runtime, bspath, value);

    odb_set_thread_default_context(prev_ctx);
    return result;
}
```

**Complete Migration Checklist:**

| Verb | Lines Before | Lines After | Estimated Effort |
|------|--------------|-------------|------------------|
| odbOpenFile | 60 | 80 | 2 hours |
| odbGetValue | 25 | 35 | 30 mins |
| odbDefined | 20 | 30 | 30 mins |
| odbDelete | 30 | 40 | 1 hour |
| odbGetType | 25 | 35 | 30 mins |
| odbNewTable | 30 | 40 | 1 hour |
| odbCountItems | 25 | 35 | 30 mins |
| odbGetNthItem | 30 | 40 | 1 hour |
| odbGetModDate | 20 | 30 | 30 mins |

**Total Effort:** ~8 hours

---

### Step 2B-2: Fix Lazy Root Table Creation (Issue #266 Core Fix)

**Goal:** Ensure lazy root table creation updates the cancoon record's runtime context, eliminating stale state bug.

**Files Modified:**
- `Common/source/langvalue.c` (langgetdotparams_context function)

**Changes:**

```c
// In langgetdotparams_context() - after root table materialization:

boolean langgetdotparams_context(odb_runtime_context *ctx, hdltreenode htree, hdlhashtable *htable, bigstring bsname) {
    // ... existing code ...

    // After line ~3905 where root table is materialized:
    if (hsubtable == nil && *htable != nil) {
        // Root table was just materialized
        hdltablevariable hv = nil;

        if (tablevaltotable((**(*htable)).hashtablerefcon, &hv, nil) && hv != nil) {
            // Update context with new root table (FIX for Issue #266!)
            settablestructureglobals_context(ctx, (Handle) hv);

            log_debug(LOG_COMP_LANG, "langgetdotparams_context: Updated runtime context with root table htable=%p", (void *)*htable);
        }
    }

    // ... continue ...
}
```

**Before:** Root table creation updated global `rootvariable`, but cancoon record's `hrootvariable` remained nil (stale).

**After:** Root table creation updates runtime context `root_variable` directly, no staleness possible.

---

### Step 2B-3: Add Context Isolation Tests

**Goal:** Verify that multiple database operations don't corrupt each other's context.

**Integration Tests:**

```yaml
# tests/integration/db_verbs/context_isolation_advanced.yaml

name: "Advanced Database Context Isolation Tests"
description: "Stress test context isolation with nested operations and multiple databases"

tests:
  - name: "Nested db operations - context isolation"
    script: |
      local(db1 = "{FRONTIER_TEST_TMP_DIR}/nested_db1.root7");
      local(db2 = "{FRONTIER_TEST_TMP_DIR}/nested_db2.root7");

      db.new(db1);
      db.new(db2);
      db.open(db1, false);
      db.open(db2, false);

      // Nested operation: Set value in db1, then db2, then db1 again
      db.setvalue(db1, "outer", "value1");
      db.setvalue(db2, "inner", "value2");
      db.setvalue(db1, "final", "value3");

      // Save both
      db.save(db1);
      db.save(db2);

      // Verify values in db1
      local(v1, v2);
      db.getvalue(db1, "outer", @v1);
      db.getvalue(db1, "final", @v2);
      assert(v1 == "value1", "db1 outer wrong");
      assert(v2 == "value3", "db1 final wrong");

      // Verify value in db2
      local(v3);
      db.getvalue(db2, "inner", @v3);
      assert(v3 == "value2", "db2 inner wrong");

      db.close(db1);
      db.close(db2);
      return true
    expected_success: true

  - name: "Lazy root table creation - cancoon record updated"
    description: "This is the CRITICAL test for Issue #266 fix"
    script: |
      local(dbPath = "{FRONTIER_TEST_TMP_DIR}/lazy_root.root7");

      db.new(dbPath);
      db.open(dbPath, false);

      // At this point, database is empty, root table is nil

      // This triggers lazy root table creation
      db.setvalue(dbPath, "testkey", "testvalue");

      // This should save the GUEST database, NOT system root
      db.save(dbPath);

      db.close(dbPath);

      // Reopen and verify
      db.open(dbPath, true);
      local(val);
      db.getvalue(dbPath, "testkey", @val);
      assert(val == "testvalue", "Value not saved correctly");

      db.close(dbPath);
      return true
    expected_success: true

  - name: "System root loaded - guest database saves correctly"
    description: "Regression test for Issue #266 (system root shadowing)"
    setup_requires_system_root: true
    script: |
      local(guestPath = "{FRONTIER_TEST_TMP_DIR}/guest_with_system.root7");

      db.new(guestPath);
      db.open(guestPath, false);

      // Set value in guest database
      db.setvalue(guestPath, "guest_key", "guest_value");

      // Save guest database (this FAILED before fix - saved system root instead!)
      db.save(guestPath);

      db.close(guestPath);

      // Reopen guest database and verify
      db.open(guestPath, true);
      local(val);
      db.getvalue(guestPath, "guest_key", @val);
      assert(val == "guest_value", "Guest database not saved correctly");

      // Verify guest database does NOT contain system tables
      local(hasExamplesTable);
      hasExamplesTable = db.defined(guestPath, "examples");
      assert(!hasExamplesTable, "Guest database incorrectly contains system.examples table!");

      db.close(guestPath);
      return true
    expected_success: true
```

**Critical Test:** The "System root loaded - guest database saves correctly" test is the **regression test for Issue #266**. This MUST pass after Phase 2B.

---

### Testing Checkpoint 2B (End of Week 3-4)

**Success Criteria:**

- ✅ All database verb `_context` variants implemented
- ✅ Lazy root table creation updates runtime context (Issue #266 fixed!)
- ✅ context_isolation_advanced.yaml tests PASS
- ✅ "System root loaded - guest database saves correctly" test PASSES (Issue #266 regression test)
- ✅ No regression in existing db_verbs tests
- ✅ All 11 database verbs use explicit context internally

**Regression Testing:**

```bash
# Run full test suite with system root loaded
./tools/run_headless_tests.sh

# Run integration tests with system root
cd tests && make test-integration FRONTIER_CLI_ARGS="--system-root ../databases/Frontier-v6.root7"

# Run db_verbs tests specifically
cd tests && python3 run_integration_tests.py integration/db_verbs/*.yaml
```

Expected: ALL tests PASS (no failures, no errors).

**Deliverables:**

- [ ] 11 database verbs migrated to `_context` variants
- [ ] Lazy root table creation fix verified
- [ ] Advanced context isolation tests passing
- [ ] Issue #266 regression test passing
- [ ] Documentation updated (function comments)

---

## Phase 2C: Deprecation (Week 5-6)

**Goal:** Mark global variables as deprecated, audit all uses, migrate remaining callers to `_context` variants.

**Estimated Effort:** 16-20 hours

### Step 2C-1: Mark Global Variables as Deprecated

**Files Modified:**
- `Common/source/tablestructure.c`
- `Common/source/db.c`
- `Common/source/langhash.c`

**Changes:**

1. **Add deprecation warnings:**

```c
// Common/source/tablestructure.c (line ~110):

// DEPRECATED: Use odb_runtime_context instead (see ADR-XXX)
// This global will be removed in Phase 2D.
// DO NOT add new code that references this global.
#ifdef __GNUC__
__attribute__((deprecated("Use odb_runtime_context->root_variable instead")))
#endif
Handle rootvariable = nil;

#ifdef __GNUC__
__attribute__((deprecated("Use odb_runtime_context->root_table instead")))
#endif
hdlhashtable roottable = nil;
```

2. **Add compile-time warnings:**

```c
// Common/headers/tablestructure.h

#ifdef WARN_DEPRECATED_GLOBALS
    #warning "Direct access to rootvariable/roottable is deprecated - use odb_runtime_context"
#endif

// Usage: make CFLAGS="-DWARN_DEPRECATED_GLOBALS" to enable warnings during migration
```

---

### Step 2C-2: Audit Global State Usage

**Goal:** Find all remaining uses of deprecated globals and categorize them.

**Automated Search:**

```bash
# Find all uses of rootvariable
grep -rn "rootvariable" Common/source/*.c | grep -v "odb_runtime_context" | grep -v "DEPRECATED"

# Find all uses of roottable
grep -rn "roottable" Common/source/*.c | grep -v "odb_runtime_context" | grep -v "DEPRECATED"

# Find all uses of databasedata
grep -rn "databasedata" Common/source/*.c | grep -v "odb_runtime_context" | grep -v "DEPRECATED"

# Find all uses of currenthashtable
grep -rn "currenthashtable" Common/source/*.c | grep -v "odb_runtime_context" | grep -v "DEPRECATED"

# Find all uses of hashtablestack
grep -rn "hashtablestack" Common/source/*.c | grep -v "odb_runtime_context" | grep -v "DEPRECATED"
```

**Expected Results:**

After Phase 2B, most uses should be in:
1. Backward-compatible wrapper functions (acceptable during migration)
2. `setcancoonglobals()` / `clearcancoonglobals()` (to be removed in Phase 2D)
3. Legacy GUI code (`cancoon.c`, `shellcallbacks.c`) - OUT OF SCOPE

**Categorization:**

| Category | Action | Priority |
|----------|--------|----------|
| ODB Engine wrappers | Keep during Phase 2C, remove in Phase 2D | P0 |
| setcancoonglobals() | Deprecate, remove in Phase 2D | P0 |
| Table structure functions | Already migrated to `_context` | ✅ Done |
| Hash table functions | Already migrated to `_context` | ✅ Done |
| GUI code (cancoon.c) | OUT OF SCOPE (headless focus) | N/A |
| Unknown callers | Investigate and migrate | P1 |

---

### Step 2C-3: Migrate Remaining Callers

**Goal:** Convert any remaining non-wrapper uses of globals to use `_context` variants.

**Process:**

For each remaining caller:

1. **Determine context source:**
   - Is there a `hdlcancoonrecord` in scope? Use `(**hc).runtime`
   - Is there a thread-local default? Use `odb_get_thread_default_context()`
   - Neither? Create explicit context parameter and thread it through call chain

2. **Refactor call site:**

```c
// BEFORE:
if (rootvariable == nil) {
    // ...
}

// AFTER:
odb_runtime_context *ctx = odb_get_thread_default_context();
if (ctx != nil && ctx->root_variable == nil) {
    // ...
}
```

3. **Verify no regression:**

Run tests after each migration to ensure behavior unchanged.

**Example Migration: tableverbinmemory**

```c
// BEFORE (uses global rootvariable):
boolean tableverbinmemory(hdltablevariable hvariable, hdlhashnode hnode) {
    if (rootvariable == (Handle) hvariable) {
        // Already in memory
        return true;
    }
    // ...
}

// AFTER (uses context):
boolean tableverbinmemory_context(odb_runtime_context *ctx, hdltablevariable hvariable, hdlhashnode hnode) {
    if (ctx != nil && ctx->root_variable == (Handle) hvariable) {
        // Already in memory
        return true;
    }
    // ...
}

// Backward-compatible wrapper:
boolean tableverbinmemory(hdltablevariable hvariable, hdlhashnode hnode) {
    odb_runtime_context *ctx = odb_get_thread_default_context();
    if (ctx != nil) {
        return tableverbinmemory_context(ctx, hvariable, hnode);
    }

    // Fallback: Use global (legacy path)
    if (rootvariable == (Handle) hvariable) {
        return true;
    }
    // ...
}
```

---

### Testing Checkpoint 2C (End of Week 5-6)

**Audit Results:**

```bash
# Generate deprecation report
./tools/audit_deprecated_globals.sh > reports/deprecated_globals_audit.txt

# Expected: Only backward-compatible wrappers and setcancoonglobals() remain
```

**Success Criteria:**

- ✅ All globals marked as deprecated with `__attribute__((deprecated))`
- ✅ Compile-time warnings enabled for deprecated global access
- ✅ Audit complete: All non-wrapper uses identified and categorized
- ✅ Remaining callers migrated to `_context` variants
- ✅ No new code using deprecated globals
- ✅ All tests still passing

**Deliverables:**

- [ ] Deprecation warnings added to all 5 global variables
- [ ] Audit report generated (reports/deprecated_globals_audit.txt)
- [ ] Remaining callers migrated
- [ ] Documentation updated with deprecation notices

---

## Phase 2D: Cleanup (Week 7-8)

**Goal:** Remove global database state variables, remove backward-compatible wrappers, verify thread safety.

**Estimated Effort:** 12-16 hours

**WARNING:** This is the HIGHEST RISK phase. Proceed carefully with extensive testing.

### Step 2D-1: Remove setcancoonglobals()

**Files Modified:**
- `Common/source/odbengine.c`

**Changes:**

1. **Delete setcancoonglobals() function** (line ~223):

```c
// BEFORE:
static void setcancoonglobals(hdlcancoonrecord hcancoon) {
    // ... 17 lines ...
}

// AFTER:
// DELETED - No longer needed, runtime context embedded in cancoon record
```

2. **Remove all call sites:**

All 11 call sites in `odbengine.c` should already be inside backward-compatible wrappers that set thread-local default context. If `setcancoonglobals()` was still being called elsewhere, Phase 2C migration missed something - STOP and investigate.

3. **Verify no regressions:**

```bash
./tools/run_headless_tests.sh
cd tests && make test-integration
```

Expected: ALL tests PASS (no change in behavior).

---

### Step 2D-2: Remove Global Variable Declarations

**Files Modified:**
- `Common/source/tablestructure.c`
- `Common/source/db.c`
- `Common/source/langhash.c`

**Changes:**

1. **Comment out (don't delete yet!) global declarations:**

```c
// Common/source/tablestructure.c (line ~110):

// PHASE 2D: Global variables removed (migrated to odb_runtime_context)
// Commented out for safety - will delete after verification period
// If compilation fails, Phase 2C migration was incomplete

// Handle rootvariable = nil;
// hdlhashtable roottable = nil;
```

2. **Compile and verify:**

```bash
make clean && make
```

Expected: Compilation SUCCEEDS with ZERO errors referencing `rootvariable`, `roottable`, etc.

If compilation FAILS:
- Grep for error locations: `grep -rn "error:.*rootvariable"`
- Investigate why code is still using globals
- Likely cause: Phase 2C migration incomplete
- Action: ROLLBACK to Phase 2C, fix missing migration

3. **Run full test suite:**

```bash
./tools/run_headless_tests.sh
cd tests && make test-integration
```

Expected: ALL tests PASS (no regressions).

4. **After 1 week verification period: Delete commented-out globals**

If no issues discovered after 1 week:

```c
// Common/source/tablestructure.c (line ~110):
// DELETED - migrated to odb_runtime_context (Phase 2D complete)
```

---

### Step 2D-3: Thread Safety Verification

**Goal:** Verify that database operations are thread-safe with explicit context.

**Concurrency Tests:**

```c
// tests/headless_db_verbs.c

#include <pthread.h>

typedef struct thread_test_data {
    char db_path[256];
    int thread_id;
    int iteration_count;
    boolean success;
} thread_test_data;

void *thread_db_operations(void *arg) {
    thread_test_data *data = (thread_test_data *) arg;

    // Each thread operates on its own database
    char db_path[300];
    sprintf(db_path, "%s_thread%d.root7", data->db_path, data->thread_id);

    // Create database
    if (!odbNewFile(db_path)) {
        data->success = false;
        return nil;
    }

    // Open database
    odbref odb;
    if (!odbOpenFile(db_path, &odb, false)) {
        data->success = false;
        return nil;
    }

    // Perform iterations
    for (int i = 0; i < data->iteration_count; i++) {
        bigstring key, value;
        sprintf(key, "key_%d", i);
        sprintf(value, "value_%d_%d", data->thread_id, i);

        // Set value
        odbValueRecord val;
        val.valuetype = 'TEXT';
        val.data.stringvalue = value;

        if (!odbSetValue(odb, key, &val)) {
            data->success = false;
            return nil;
        }
    }

    // Save and close
    if (!odbSaveFile(odb)) {
        data->success = false;
        return nil;
    }

    if (!odbCloseFile(odb)) {
        data->success = false;
        return nil;
    }

    data->success = true;
    return nil;
}

void test_concurrent_database_operations(void) {
    const int NUM_THREADS = 10;
    const int ITERATIONS_PER_THREAD = 50;

    pthread_t threads[NUM_THREADS];
    thread_test_data thread_data[NUM_THREADS];

    char base_path[256];
    sprintf(base_path, "%s/concurrent_test", get_test_temp_path());

    // Launch threads
    for (int i = 0; i < NUM_THREADS; i++) {
        strcpy(thread_data[i].db_path, base_path);
        thread_data[i].thread_id = i;
        thread_data[i].iteration_count = ITERATIONS_PER_THREAD;
        thread_data[i].success = false;

        pthread_create(&threads[i], nil, thread_db_operations, &thread_data[i]);
    }

    // Wait for completion
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nil);
    }

    // Verify all threads succeeded
    for (int i = 0; i < NUM_THREADS; i++) {
        assert(thread_data[i].success == true);
    }

    printf("✓ Concurrent database operations successful (%d threads, %d ops each)\n",
           NUM_THREADS, ITERATIONS_PER_THREAD);
}
```

**Stress Test:**

```bash
# Run concurrent test 100 times to catch race conditions
for i in {1..100}; do
    echo "Iteration $i"
    ./tests/test_concurrent_db_ops
    if [ $? -ne 0 ]; then
        echo "FAILED on iteration $i"
        exit 1
    fi
done

echo "✓ All 100 iterations passed - thread safety verified"
```

---

### Step 2D-4: Performance Benchmarking

**Goal:** Verify that context passing doesn't introduce measurable performance overhead.

**Benchmark Test:**

```c
// tests/benchmark_context_overhead.c

#include <time.h>

void benchmark_db_operations(int num_operations) {
    char db_path[256];
    sprintf(db_path, "%s/benchmark.root7", get_test_temp_path());

    // Create and open database
    odbNewFile(db_path);
    odbref odb;
    odbOpenFile(db_path, &odb, false);

    // Benchmark setvalue operations
    clock_t start = clock();

    for (int i = 0; i < num_operations; i++) {
        bigstring key, value;
        sprintf(key, "key_%d", i);
        sprintf(value, "value_%d", i);

        odbValueRecord val;
        val.valuetype = 'TEXT';
        val.data.stringvalue = value;

        odbSetValue(odb, key, &val);
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    // Save and close
    odbSaveFile(odb);
    odbCloseFile(odb);

    printf("Benchmark: %d operations in %.3f seconds (%.0f ops/sec)\n",
           num_operations, elapsed, num_operations / elapsed);
}

int main(void) {
    printf("Running performance benchmark...\n");
    benchmark_db_operations(10000);
    return 0;
}
```

**Acceptance Criteria:**

- Context passing overhead should be < 5% compared to global state baseline
- Operations per second should be within 95% of baseline performance
- No memory leaks detected (run with valgrind/leaks)

---

### Testing Checkpoint 2D (End of Week 7-8)

**Success Criteria:**

- ✅ `setcancoonglobals()` function removed
- ✅ All 5 global variables removed (or commented out pending verification)
- ✅ Compilation succeeds with ZERO errors
- ✅ All unit tests pass
- ✅ All integration tests pass
- ✅ Concurrent database operations test passes
- ✅ Stress test (100 iterations) passes
- ✅ Performance benchmark shows < 5% overhead
- ✅ No memory leaks detected

**Final Validation:**

```bash
# Full test suite
./tools/run_headless_tests.sh

# Integration tests with system root
cd tests && make test-integration FRONTIER_CLI_ARGS="--system-root ../databases/Frontier-v6.root7"

# Concurrent operations stress test
./tests/stress_test_concurrent_db_ops.sh

# Performance benchmark
./tests/benchmark_context_overhead

# Memory leak check (if valgrind available)
valgrind --leak-check=full ./tests/test_db_context_cleanup
```

**Deliverables:**

- [ ] Global variables removed
- [ ] `setcancoonglobals()` deleted
- [ ] Thread safety verified
- [ ] Performance benchmarks acceptable
- [ ] No memory leaks
- [ ] Documentation updated (mark Phase 2D complete)

---

## Testing Strategy

### Unit Tests

**Location:** `tests/headless_db_verbs.c`

**Test Categories:**

1. **Context Initialization Tests**
   - `test_odb_context_init()`
   - `test_odb_context_copy()`
   - `test_thread_local_context()`

2. **Context Isolation Tests**
   - `test_multiple_databases_isolated_contexts()`
   - `test_nested_db_operations()`
   - `test_context_not_corrupted_by_child_operations()`

3. **Lazy Root Table Creation Tests**
   - `test_lazy_root_table_updates_context()`
   - `test_cancoon_record_not_stale_after_lazy_creation()`

4. **Thread Safety Tests**
   - `test_concurrent_database_operations()`
   - `test_context_per_thread_isolated()`
   - `test_stress_concurrent_setvalue()`

5. **Performance Tests**
   - `test_context_passing_overhead()`
   - `test_no_memory_leaks()`

### Integration Tests

**Location:** `tests/integration/db_verbs/`

**Test Files:**

1. **context_isolation.yaml** (Phase 2A)
   - Basic context isolation
   - Multiple databases don't interfere

2. **context_isolation_advanced.yaml** (Phase 2B)
   - Nested operations
   - Lazy root table creation fix
   - System root loaded scenario (Issue #266 regression test)

3. **thread_safety.yaml** (Phase 2D)
   - Concurrent operations
   - Stress testing
   - Race condition detection

### Regression Tests

**All existing db_verbs tests MUST pass:**

```bash
cd tests
python3 run_integration_tests.py integration/db_verbs/*.yaml
```

**With system root loaded:**

```bash
cd tests
python3 run_integration_tests.py integration/db_verbs/*.yaml --system-root ../databases/Frontier-v6.root7
```

### Migration Tests

**Verify v6 → v7 migration still works:**

```bash
# Migrate system root
rm -f databases/Frontier-v6.root7
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root -e "1"

# Run migration tests
cd tests && make test-migration
```

Expected: Migration succeeds, no errors.

---

## Migration Checkpoints

### Checkpoint 2A (End of Week 1-2)

**Verification Steps:**

1. Compile project: `make clean && make`
2. Run unit tests: `./tools/run_headless_tests.sh`
3. Run integration tests: `cd tests && make test-integration`
4. Check context isolation: `python3 run_integration_tests.py integration/db_verbs/context_isolation.yaml`

**Exit Criteria:**

- ✅ All tests pass
- ✅ No compilation warnings
- ✅ `odb_runtime_context` structure works correctly
- ✅ Thread-local default context functional

**Go/No-Go Decision:**

- **GO:** Proceed to Phase 2B
- **NO-GO:** Revert to Phase 1 quick fix, investigate failures

---

### Checkpoint 2B (End of Week 3-4)

**Verification Steps:**

1. Run full test suite: `./tools/run_headless_tests.sh`
2. Run advanced isolation tests: `python3 run_integration_tests.py integration/db_verbs/context_isolation_advanced.yaml`
3. **CRITICAL:** Run Issue #266 regression test: `cd tests && python3 run_integration_tests.py integration/db_verbs/issue_266_regression.yaml`
4. Verify with system root: `cd tests && make test-integration FRONTIER_CLI_ARGS="--system-root ../databases/Frontier-v6.root7"`

**Exit Criteria:**

- ✅ All database verbs migrated to `_context` variants
- ✅ Lazy root table creation fix verified
- ✅ Issue #266 regression test PASSES
- ✅ No context corruption observed

**Go/No-Go Decision:**

- **GO:** Proceed to Phase 2C
- **NO-GO:** Fix failing tests, investigate root cause

---

### Checkpoint 2C (End of Week 5-6)

**Verification Steps:**

1. Generate audit report: `./tools/audit_deprecated_globals.sh > reports/deprecated_globals_audit.txt`
2. Review audit report: Verify only wrappers and `setcancoonglobals()` remain
3. Run full test suite: `./tools/run_headless_tests.sh`
4. Compile with warnings: `make clean && make CFLAGS="-DWARN_DEPRECATED_GLOBALS"`

**Exit Criteria:**

- ✅ All globals marked deprecated
- ✅ Audit report shows only expected usage
- ✅ All tests pass
- ✅ Compile warnings show deprecated global access only in wrappers

**Go/No-Go Decision:**

- **GO:** Proceed to Phase 2D (with CAUTION - highest risk phase)
- **NO-GO:** Complete remaining migrations in Phase 2C

---

### Checkpoint 2D (End of Week 7-8)

**Verification Steps:**

1. Compile after global removal: `make clean && make`
2. Run full test suite: `./tools/run_headless_tests.sh`
3. Run concurrent tests: `./tests/stress_test_concurrent_db_ops.sh`
4. Run performance benchmark: `./tests/benchmark_context_overhead`
5. Check for memory leaks: `valgrind --leak-check=full ./tests/test_db_context_cleanup`
6. Run integration tests: `cd tests && make test-integration`

**Exit Criteria:**

- ✅ Compilation succeeds (ZERO errors)
- ✅ All tests pass
- ✅ Concurrent operations succeed
- ✅ Performance acceptable (< 5% overhead)
- ✅ No memory leaks

**Go/No-Go Decision:**

- **GO:** Phase 2 COMPLETE - Mark production-ready
- **NO-GO:** Rollback to Phase 2C, investigate issues

---

## Rollback Plan

### Rollback from Phase 2A

**If tests fail in Phase 2A:**

1. **Revert code changes:**
   ```bash
   git checkout HEAD -- Common/headers/tablestructure.h
   git checkout HEAD -- Common/source/tablestructure.c
   git checkout HEAD -- Common/source/odbengine.c
   ```

2. **Verify Phase 1 still works:**
   ```bash
   make clean && make
   ./tools/run_headless_tests.sh
   ```

3. **Investigate failure:**
   - Check unit test logs
   - Verify context initialization logic
   - Review thread-local storage implementation

4. **Retry Phase 2A with fixes**

---

### Rollback from Phase 2B

**If tests fail in Phase 2B:**

1. **Identify failing component:**
   - Which database verb fails?
   - Which test case fails?
   - Is it a context isolation issue or lazy root table creation issue?

2. **Selective rollback:**
   ```bash
   # Revert specific function if only one fails
   git checkout HEAD -- Common/source/odbengine.c

   # OR revert all Phase 2B changes
   git checkout HEAD -- Common/source/langvalue.c
   git checkout HEAD -- Common/source/odbengine.c
   ```

3. **Verify Phase 2A still works:**
   ```bash
   make clean && make
   ./tools/run_headless_tests.sh
   ```

4. **Fix and retry Phase 2B**

---

### Rollback from Phase 2C

**If deprecation warnings break build or tests fail:**

1. **Revert deprecation markers:**
   ```bash
   git checkout HEAD -- Common/source/tablestructure.c
   git checkout HEAD -- Common/source/db.c
   git checkout HEAD -- Common/source/langhash.c
   ```

2. **Keep Phase 2A/2B work intact:**
   - `_context` variants remain functional
   - Backward-compatible wrappers still work
   - Thread-local default context still works

3. **Continue using Phase 2B state** until deprecation issues resolved

---

### Rollback from Phase 2D

**If global removal causes failures:**

1. **CRITICAL:** Immediately restore globals:
   ```bash
   git checkout HEAD -- Common/source/tablestructure.c
   git checkout HEAD -- Common/source/db.c
   git checkout HEAD -- Common/source/langhash.c
   git checkout HEAD -- Common/source/odbengine.c
   ```

2. **Rebuild and verify:**
   ```bash
   make clean && make
   ./tools/run_headless_tests.sh
   ```

3. **Investigate what was still using globals:**
   - Grep for error messages
   - Check call chains
   - Likely missed caller in Phase 2C audit

4. **Return to Phase 2C, complete migration, retry Phase 2D**

---

### Full Rollback to Phase 1

**If Phase 2 proves too risky or time-consuming:**

1. **Revert all Phase 2 changes:**
   ```bash
   git checkout <phase-1-commit-hash>
   ```

2. **Verify Phase 1 quick fix still works:**
   ```bash
   make clean && make
   ./tools/run_headless_tests.sh
   cd tests && make test-integration FRONTIER_CLI_ARGS="--system-root ../databases/Frontier-v6.root7"
   ```

3. **File issue for Phase 2 as future work:**
   - Title: "Database context refactoring (Phase 2 - deferred)"
   - Label: "enhancement", "tech-debt"
   - Milestone: "Phase 2.0 (Collaborative ODB)"

4. **Proceed with v7 launch using Phase 1 quick fix**

---

## Risk Assessment

### Risk 1: Incomplete Migration (HIGH)

**Scenario:** Phase 2C audit misses a caller still using globals, Phase 2D global removal causes compilation failure.

**Likelihood:** MEDIUM (100+ function signatures, easy to miss one)

**Impact:** HIGH (blocks Phase 2D completion)

**Mitigation:**
- Automated grep scripts to find all global references
- Compile with `-Werror` to turn warnings into errors
- Incremental testing after each function migration
- Keep commented-out globals for 1 week before deletion

**Contingency:**
- Rollback to Phase 2C
- Complete missing migration
- Retry Phase 2D

---

### Risk 2: Thread-Local Storage Compatibility (MEDIUM)

**Scenario:** `_Thread_local` keyword not supported on target compiler, fallback to `__thread` fails.

**Likelihood:** LOW (modern compilers support thread-local)

**Impact:** MEDIUM (blocks backward compatibility pattern)

**Mitigation:**
- Test on all target platforms early (macOS, Linux)
- Fallback to `pthread_getspecific()` if needed
- Document compiler requirements

**Contingency:**
- Use `pthread_key_create()` / `pthread_getspecific()` for thread-local storage
- Add wrapper macros to abstract platform differences

---

### Risk 3: Performance Degradation (LOW)

**Scenario:** Context passing adds measurable overhead, slows down database operations.

**Likelihood:** VERY LOW (pointer parameter is trivial overhead)

**Impact:** LOW (acceptable if < 5%)

**Mitigation:**
- Benchmark early (Phase 2A)
- Profile hot paths with context passing
- Inline context accessor functions if needed
- Compiler optimization should eliminate overhead

**Contingency:**
- If > 5% overhead: Investigate bottleneck, optimize
- Worst case: Acceptable trade-off for thread safety

---

### Risk 4: Subtle Behavioral Changes (MEDIUM)

**Scenario:** Context isolation changes behavior in ways tests don't catch, causes production bugs.

**Likelihood:** MEDIUM (complex global state interactions)

**Impact:** HIGH (production failures)

**Mitigation:**
- Extensive integration testing
- Stress testing with concurrent operations
- 1-week soak period after Phase 2D before marking production-ready
- Canary deployment to subset of users

**Contingency:**
- Rollback to Phase 1 if production issues discovered
- Fix behavioral difference
- Add regression test
- Retry deployment

---

### Risk 5: Memory Leaks (LOW)

**Scenario:** Context lifecycle not properly managed, leads to memory leaks.

**Likelihood:** LOW (contexts embedded in cancoon records, already managed)

**Impact:** MEDIUM (memory growth over time)

**Mitigation:**
- Valgrind testing in Phase 2D
- Long-running stress tests
- Monitor memory usage during soak period

**Contingency:**
- Identify leak source with valgrind
- Fix lifecycle management
- Retest

---

### Risk 6: GUI Build Compatibility (LOW - OUT OF SCOPE)

**Scenario:** GUI builds (`cancoon.c`) break due to context refactoring.

**Likelihood:** LOW (GUI code out of scope for headless)

**Impact:** LOW (headless is focus, GUI is future work)

**Mitigation:**
- Keep `setcancoonglobals()` in GUI builds if needed
- Headless and GUI can diverge during Phase 2
- Reconcile in future GUI modernization effort

**Contingency:**
- GUI builds use legacy global state path
- Headless builds use explicit context path
- Merge strategies revisited when GUI work begins

---

## Summary and Recommendations

### Effort Summary

| Phase | Estimated Hours | Risk Level | Dependencies |
|-------|----------------|------------|--------------|
| 2A: Infrastructure | 16-20 | LOW | None (starts from Phase 1) |
| 2B: Core Migration | 20-24 | MEDIUM | Phase 2A complete |
| 2C: Deprecation | 16-20 | MEDIUM | Phase 2B complete |
| 2D: Cleanup | 12-16 | HIGH | Phase 2C complete |
| **TOTAL** | **64-80 hours** | **MEDIUM-HIGH** | **4-8 weeks elapsed** |

### Key Decision Points

**Decision 1 (End of Phase 2A):** Context infrastructure working?
- **YES:** Proceed to Phase 2B
- **NO:** Investigate failures, rollback if needed

**Decision 2 (End of Phase 2B):** Issue #266 regression test passes?
- **YES:** Proceed to Phase 2C
- **NO:** Fix lazy root table creation, retry

**Decision 3 (End of Phase 2C):** Audit shows only wrappers remain?
- **YES:** Proceed to Phase 2D (CAUTION: High risk)
- **NO:** Complete missing migrations, retry audit

**Decision 4 (End of Phase 2D):** Thread safety verified, no regressions?
- **YES:** Mark Phase 2 complete, production-ready
- **NO:** Rollback to Phase 2C, investigate issues

### TPM/CTO Decision Framework

**Recommend Phase 2 NOW if:**
- ✅ Phase 1 quick fix is stable
- ✅ 4-8 weeks available for focused refactoring work
- ✅ Thread safety is required for near-term roadmap (collaborative ODB)
- ✅ Technical debt reduction is prioritized

**Defer Phase 2 if:**
- ❌ v7 launch timeline is tight (< 4 weeks)
- ❌ Other higher-priority work blocks allocation
- ❌ Thread safety not needed until Phase 6+ (12+ months out)
- ❌ Risk tolerance is low (prefer incremental improvements)

### Strategic Value

**Immediate Benefits:**
- Fixes Issue #266 architecturally (not just symptomatically)
- Eliminates entire class of global state bugs
- Enables confident multi-database operations

**Long-Term Benefits:**
- **CRITICAL:** Unblocks collaborative ODB (Phase 2.0 vision)
- Foundation for multi-user concurrent editing
- Thread-safe verb execution
- Cleaner architecture for future contributors

**Technical Debt Reduction:**
- Removes 5 global variables (major architectural improvement)
- Establishes pattern for future global elimination
- Aligns with "BURN THE GLOBALS WITH FIRE" directive

---

## References

### Internal Documentation

- **DB_CONTEXT_GLOBAL_STATE_FIX.md** - High-level analysis and options
- **ADR-005** - Thread-local parameter state pattern
- **ADR-006** - Outline push/pop elimination (similar pattern)
- **CLAUDE.md** - Global state elimination directive
- **Issue #266** - P0 bug (db.save() saves wrong database)
- **Issue #135** - Outline context refactoring (reference pattern)

### Code References

- `Common/source/odbengine.c:223` - `setcancoonglobals()` function
- `Common/source/tablestructure.c:110` - Global `rootvariable` declaration
- `Common/source/langvalue.c:~3905` - Lazy root table creation site
- `Common/source/langhash.c:835` - Global `currenthashtable` declaration
- `Common/source/db.c:192` - Global `databasedata` declaration

### Testing Infrastructure

- `./tools/run_headless_tests.sh` - Full unit test suite
- `tests/integration/db_verbs/*.yaml` - Integration test cases
- `tests/headless_db_verbs.c` - Unit test implementations

---

**Document Version:** 1.0
**Last Updated:** 2026-01-10
**Next Review:** After Phase 2A completion (or user feedback)
