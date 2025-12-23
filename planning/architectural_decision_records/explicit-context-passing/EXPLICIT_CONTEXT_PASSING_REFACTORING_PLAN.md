# Explicit Database Context Passing: Refactoring Plan

**Created**: 2025-12-22
**Status**: DESIGN DOCUMENT
**Related Issue**: #147 - Convert table pack/unpack from global mode state to explicit context passing
**Related ADRs**:
- `mode_management_single_decision_point.md` - Single decision point principle
- `ADR-001-multi-database-context.md` - Multi-database context management

---

## Executive Summary

This document provides a comprehensive plan to refactor Frontier's table pack/unpack operations from using global mode state (`db_format_mode_current()`) to explicit context passing. The refactor addresses Issue #147 where recursive table packing operations inherit the wrong mode, causing tables to be written with v4 headers instead of v5 headers during v6→v7 migration.

**Scope**: 15 source files, ~5,000 lines of code
**Estimated Effort**: 3-4 weeks (phased implementation)
**Risk Level**: MEDIUM (touches critical serialization paths)
**Recommended Approach**: INCREMENTAL - Fix minimal set first, then expand

---

## Problem Statement

### Current Architecture Issue

The codebase uses a thread-local mode stack accessed via `db_format_mode_current()`:

```c
// In hashpacktable_internal()
if (ctx != NULL) {
    use_64bit = ctx->mode.use_64bit_format;
} else {
    use_64bit = db_format_mode_current().use_64bit_format;  // GLOBAL STATE
}
```

**Problem**: When legacy reader mode is pushed to load v6 tables, recursive table packing operations inherit the wrong mode:

1. Migration sets mode to v7 write (`use_64bit=1`)
2. External table variable needs to be loaded from v6 source database
3. Code pushes v6 read mode (`use_64bit=0`)
4. Recursive `hashpacktable()` call reads global mode → sees v6 mode
5. Table gets packed with v4 header instead of v5 header
6. Result: Corrupted v7 database with mixed header versions

### Root Cause

**Violation of Single Decision Point Principle**: Multiple code layers make format decisions by reading global mode state, rather than having ONE decision point that explicitly passes context down the call chain.

---

## System Context

### Four Address Spaces During Migration

1. **On-disk v6 addresses** (32-bit LE): Source database file format
2. **In-memory pointers** (64-bit): When `flinmemory=1`, variabledata is a memory pointer
3. **Expanded structures**: Objects in memory prepared for v7 format
4. **On-disk v7 addresses** (64-bit BE): Destination database file format

### Current Context Infrastructure

The `db_context` struct already exists in `db_format.h`:

```c
typedef struct db_format_mode {
    boolean use_64bit_format;   // v6 (32-bit LE) vs v7 (64-bit BE)
    boolean adapter_repack;     // Migration mode flag
    boolean drop_cancoon;       // Drop legacy UI data
} db_format_mode;

struct db_context {
    db_format_mode mode;
    hdldatabaserecord database;
    db_saveas_state saveas;
};
```

**Context Helpers Already Exist**:
- `db_context_init()` - Initialize from global state
- `db_context_init_with_mode()` - Initialize with explicit mode
- `db_context_init_legacy_read()` - Set up for v6 reading
- `db_context_init_v7_write()` - Set up for v7 writing
- `db_context_apply()` - Apply context to thread-local state

### Partial Migration Already Complete

**Good news**: The team has already implemented the Single Decision Point pattern in `langexternal.c`:

```c
// langexternalpack_internal() - THE ONLY function that manages mode
boolean langexternalpack_internal(const db_context *ctx, ...) {
    // ONE decision point for reading (v6 load)
    if (adapter_repack && !(**hv).flinmemory) {
        legacy_context.mode.use_64bit_format = false;
        if (!ensure_external_in_memory(&legacy_context, hv))
            return false;
    }

    // ONE decision point for writing (v7 pack)
    working_context.mode.use_64bit_format = true;
    switch ((**hv).id) {
        case idoutlineprocessor:
            opverbpack_internal(&working_context, ...);  // Passes context
            break;
        // ...
    }
}
```

**This is the pattern we need to extend** to hash/table packing operations.

---

## Proposed Solution

### Design Principle: Single Decision Point

**Rule**: If there are multiple code branches making read/write format decisions, the design is WRONG.

- **ONE decision point for reading format**: Decides "use v6 32-bit LE reader" vs "use v7 64-bit BE reader"
- **ONE decision point for writing format**: Decides "use v6 writer" vs "use v7 writer"

### Three-Tier Architecture

#### Tier 1: Top-Level Callers (Mode Managers)
**Responsibility**: Set context ONCE for entire operation, manage transitions when crossing address space boundaries

**Examples**:
- `langexternalpack_internal()` ✅ ALREADY DONE
- `hashpacktable_internal()` ⚠️ NEEDS COMPLETION
- `tablepacktable_internal()` ⚠️ NEEDS COMPLETION

**Pattern**:
```c
boolean hashpacktable_internal(const db_context *ctx, ...) {
    db_context working_context;

    // Initialize context ONCE
    if (ctx != NULL) {
        working_context = *ctx;
    } else {
        db_context_init(&working_context);
    }

    // Use context for format decision
    use_64bit = working_context.mode.use_64bit_format;

    // Pass context to children (don't let them change it)
    hashpackvisit(..., &working_context);
}
```

#### Tier 2: Mid-Level Operations (Context Consumers)
**Responsibility**: Accept context parameter, operate using it, NEVER change global mode

**Examples**:
- `hashpackvisit_v7()` - Visitor for packing individual hash nodes
- `hashpackscalar()` - Pack scalar values
- `hashpackexternal()` - Pack external variable references
- `hashpackstring()`, `hashpackdata()`, `hashpackbinary()` - Pack primitive types

**Pattern**:
```c
static boolean hashpackvisit_v7(bigstring bsname, hdlhashnode hnode,
                                 tyvaluerecord val, ptrvoid refcon) {
    typackinforecord *lpi = (typackinforecord *) refcon;

    // Read format from context passed via refcon
    boolean use_64bit = lpi->use_64bit;

    // NO db_context_apply() calls!
    // NO mode switching!
    // Just operate with context info

    if (val.valuetype == externalvaluetype) {
        // Pass context down when recursing
        if (!hashpackexternal(&lpi->s2, ..., lpi->context))
            return false;
    }
}
```

#### Tier 3: Database Operations (Context-Aware Primitives)
**Responsibility**: Use context for low-level read/write operations

**Examples**:
- `dbassign_context()` ✅ ALREADY EXISTS
- `dbreference_context()` ✅ ALREADY EXISTS
- `dbcopy_context()` ✅ ALREADY EXISTS
- `dballocate_context()` ✅ ALREADY EXISTS

**Already Implemented**: All database primitives have `_context()` variants!

---

## Call Graph Analysis

### Hash Pack/Unpack Call Chain

```
hashpacktable_internal(ctx)             [Tier 1: Mode Manager]
├─ hashpackvisit(refcon)                [Tier 2: Context Consumer]
│  ├─ hashpackvisit_v7(refcon)          [Tier 2: Context Consumer]
│  │  ├─ hashpackstring()               [Tier 3: Primitive]
│  │  ├─ hashpackdata()                 [Tier 3: Primitive]
│  │  ├─ hashpackbinary()               [Tier 3: Primitive]
│  │  ├─ hashpackscalar()               [Tier 3: Primitive]
│  │  └─ hashpackexternal()             [Tier 2: Context Consumer]
│  │     └─ langexternalpack_internal() [Tier 1: Mode Manager]
│  │        ├─ ensure_external_in_memory(ctx)  [Tier 2]
│  │        └─ tableverbpack_internal(ctx)     [Tier 1]
│  │           └─ tablepacktable_internal(ctx) [Tier 1]
│  │              └─ hashpacktable_internal(ctx) [Tier 1: RECURSION]
│  └─ hashpackvisit_legacy(refcon)      [Tier 2: Context Consumer]
└─ hashunpacktable_internal(ctx)        [Tier 1: Mode Manager]
   ├─ hashunpackscalar()                [Tier 3: Primitive]
   ├─ hashunpackexternal()              [Tier 3: Primitive]
   └─ hashunpackbinary()                [Tier 3: Primitive]
```

### Table Pack/Unpack Call Chain

```
tablepacktable_internal(ctx)            [Tier 1: Mode Manager]
├─ hashpacktable_context(ctx)           [Wrapper]
│  └─ hashpacktable_internal(ctx)       [Tier 1]
└─ tablepackformats()                   [Legacy - no context needed]

tableunpacktable_internal(ctx)          [Tier 1: Mode Manager]
├─ hashunpacktable_context(ctx)         [Wrapper]
│  └─ hashunpacktable_internal(ctx)     [Tier 1]
└─ tableunpackformats()                 [Legacy - no context needed]
```

### External Pack/Unpack Call Chain

```
langexternalpack_internal(ctx)          [Tier 1: Mode Manager] ✅ DONE
├─ ensure_external_in_memory(ctx)       [Tier 2: Context Consumer] ✅ DONE
│  ├─ opverbinmemory(ctx)               [Tier 2: Context Consumer] ✅ DONE
│  ├─ wpverbinmemory(ctx)               [Tier 2: Context Consumer] ✅ DONE
│  └─ tableverbinmemory(ctx)            [Tier 2: Context Consumer] ✅ DONE
├─ opverbpack_internal(ctx)             [Tier 1: Mode Manager] ✅ DONE
├─ wpverbpack_internal(ctx)             [Tier 1: Mode Manager] ✅ DONE
└─ tableverbpack_internal(ctx)          [Tier 1: Mode Manager] ✅ DONE
   └─ tablepacktable_internal(ctx)      [Tier 1: Mode Manager]
      └─ hashpacktable_internal(ctx)    [Tier 1: Mode Manager]
```

---

## Scope Analysis

### Files Requiring Changes

| File | Lines | Tier | Changes Required | Risk |
|------|-------|------|------------------|------|
| `langhash.c` | 5,045 | 1, 2 | Thread context through visitors | HIGH |
| `tablepack.c` | 719 | 1 | Pass context to hashpack/unpack | LOW |
| `db.c` | 3,461 | 3 | ✅ Already has _context variants | NONE |
| `db_format.c` | 2,262 | 3 | ✅ Already has context helpers | NONE |
| `langexternal.c` | 3,089 | 1 | ✅ Already implements pattern | NONE |
| `tablestructure.c` | ~2,000 | 2 | May need context threading | MEDIUM |
| `tableverbs.c` | ~1,500 | 1 | ✅ Already has _internal variants | NONE |
| `opverbs.c` | ~1,200 | 1 | ✅ Already has _internal variants | NONE |
| `wpverbs.c` | ~1,000 | 1 | ✅ Already has _internal variants | NONE |

**Total Affected Code**: ~15 files, ~20,000 lines
**Critical Path Code**: ~6,000 lines (langhash.c + tablepack.c + tablestructure.c)

### Key Function Signature Changes

#### Before → After

```c
// BEFORE: Uses global mode state
boolean hashpacktable(hdlhashtable htable, boolean flmemory,
                      Handle *hpackedtable, boolean *flmustsave);

// AFTER: Context-aware internal variant already exists!
boolean hashpacktable_internal(const db_context *ctx, hdlhashtable htable,
                                boolean flmemory, Handle *hpackedtable,
                                boolean *flmustsave);
```

```c
// BEFORE: Global mode via refcon
static boolean hashpackvisit_v7(bigstring bsname, hdlhashnode hnode,
                                 tyvaluerecord val, ptrvoid refcon);

// AFTER: Context in refcon struct
typedef struct typackinforecord {
    handlestream s1;
    handlestream s2;
    boolean flmustsave;
    boolean use_64bit;
    const db_context *context;  // ADD THIS
} typackinforecord;
```

```c
// BEFORE: No context
static boolean hashpackexternal(handlestream *s, hdlexternalvariable h,
                                 int32_t *ix, boolean *flnewdbaddress);

// AFTER: Accept context
static boolean hashpackexternal(handlestream *s, hdlexternalvariable h,
                                 int32_t *ix, boolean *flnewdbaddress,
                                 const db_context *ctx);
```

---

## Phased Implementation Plan

### Phase 0: Discovery & Validation (1 day)

**Goal**: Confirm current state and establish testing baseline

**Tasks**:
- [x] Map current context usage across codebase
- [x] Identify which functions already have `_internal` variants
- [x] Document existing context infrastructure
- [ ] Run baseline migration tests
- [ ] Capture baseline logs showing mode flip-flopping

**Success Criteria**:
- Migration test passes (proves system works now)
- Log shows mode changes during recursive packing (proves problem exists)
- Documentation shows exactly which functions need changes

**Testing**:
```bash
./tools/run_headless_tests.sh 2>&1 | grep "db_format_mode_apply" > baseline_mode_changes.log
```

---

### Phase 1: Minimal Fix for Issue #147 (3-5 days)

**Goal**: Fix the immediate bug - table packing during external variable migration

**Strategy**: Thread context through `hashpackexternal()` → `langexternalpack_internal()` → `tableverbpack_internal()` → `tablepacktable_internal()` → `hashpacktable_internal()` recursion.

#### Step 1.1: Add context to typackinforecord (langhash.c)

```c
// In langhash.c around line 2590
typedef struct typackinforecord {
    handlestream s1;
    handlestream s2;
    boolean flmustsave;
    boolean use_64bit;
    const db_context *context;  // NEW: Context for child operations
} typackinforecord;
```

#### Step 1.2: Update hashpackexternal() to accept and use context

```c
// BEFORE (line ~2780)
static boolean hashpackexternal(handlestream *s, hdlexternalvariable h,
                                 int32_t *ix, boolean *flnewdbaddress);

// AFTER
static boolean hashpackexternal(handlestream *s, hdlexternalvariable h,
                                 int32_t *ix, boolean *flnewdbaddress,
                                 const db_context *ctx) {
    // ...
    if (flexternalmemorypack)
        fl = langexternalmemorypack(h, &hpacked, HNoNode);
    else
        fl = langexternalpack_internal(ctx, h, hpacked, flnewdbaddress);  // PASS CONTEXT
    // ...
}
```

#### Step 1.3: Update hashpackvisit_v7() to pass context from refcon

```c
// In hashpackvisit_v7() around line 3342
static boolean hashpackvisit_v7(bigstring bsname, hdlhashnode hnode,
                                 tyvaluerecord val, ptrvoid refcon) {
    typackinforecord *lpi = (typackinforecord *) refcon;
    const db_context *ctx = lpi->context;  // Extract from refcon

    // ...
    case externalvaluetype:
        if (!hashpackexternal(&lpi->s2,
                              (hdlexternalvariable) val.data.externalvalue,
                              &data_index, &flnewdbaddress,
                              ctx))  // PASS CONTEXT
            HASH_PACK_FAIL("hashpackexternal");
        break;
    // ...
}
```

#### Step 1.4: Set context in hashpacktable_internal()

```c
// In hashpacktable_internal() around line 3692
boolean hashpacktable_internal(const db_context *ctx, ...) {
    // ... existing setup ...

    clearbytes(&packrec, sizeof(packrec));
    packrec.flmustsave = *flmustsave;
    packrec.use_64bit = use_64bit;
    packrec.context = (ctx != NULL) ? ctx : &working_context;  // NEW

    // ... rest of function ...
}
```

**Files Modified**: 1 (`langhash.c`)
**Lines Changed**: ~30 lines
**Risk**: MEDIUM (changes critical packing path, but minimal scope)

**Testing**:
```bash
# Before fix: Should see mode flip-flopping
./tools/run_headless_tests.sh 2>&1 | grep "hashpacktable_internal use_64bit"

# After fix: Should see consistent mode
./tools/run_headless_tests.sh 2>&1 | grep "hashpacktable_internal use_64bit"
# Expected: All lines show use_64bit=1 during migration
```

**Success Criteria**:
- Migration test passes
- Migrated v7 database has ALL tables with version=5 headers (not version=4)
- Logs show NO mode changes during `hashpacktable_internal()` recursion
- External table variables (e.g., `system.verbs.globals`) are accessible post-migration

---

### Phase 2: Complete Hash Pack/Unpack Context Threading (1 week)

**Goal**: Eliminate ALL global mode reads in hash packing/unpacking operations

#### Step 2.1: Thread context through all hashpack helpers

Update remaining helper functions to accept context parameter:

```c
// Before
static boolean hashpackscalar(handlestream *s, hdlhashnode hnode,
                               int32_t *ix, boolean use_64bit);

// After
static boolean hashpackscalar(handlestream *s, hdlhashnode hnode,
                               int32_t *ix, const db_context *ctx) {
    boolean use_64bit = ctx->mode.use_64bit_format;
    // ... rest unchanged ...
}
```

**Functions to Update**:
- `hashpackscalar()` - Accept context instead of boolean
- `hashunpackscalar()` - Accept context instead of boolean
- `hashpackvisit_legacy()` - Use context from refcon
- `hashunpackexternal()` - May need context for future use

#### Step 2.2: Remove use_64bit boolean from typackinforecord

```c
typedef struct typackinforecord {
    handlestream s1;
    handlestream s2;
    boolean flmustsave;
    // boolean use_64bit;  // REMOVE: Now derived from context
    const db_context *context;
} typackinforecord;
```

#### Step 2.3: Update all callers to derive use_64bit from context

```c
// Pattern: Replace this
boolean use_64bit = lpi->use_64bit;

// With this
boolean use_64bit = lpi->context->mode.use_64bit_format;
```

**Files Modified**: 1 (`langhash.c`)
**Lines Changed**: ~100 lines
**Risk**: MEDIUM (broader changes, but still single file)

**Testing**:
```bash
# Verify no direct mode reads remain
grep -n "db_format_mode_current()" common/source/langhash.c
# Expected: Should only appear in hashpacktable_internal() fallback

# Run full test suite
./tools/run_headless_tests.sh
```

**Success Criteria**:
- All tests pass
- `db_format_mode_current()` only called in `hashpacktable_internal()` when `ctx == NULL`
- No mode stack push/pop in hash operations
- Logs show deterministic mode usage

---

### Phase 3: Eliminate Global Mode Fallbacks (3 days)

**Goal**: Remove `db_format_mode_current()` fallback paths, make context mandatory

#### Step 3.1: Audit all hashpacktable_internal() callers

```bash
grep -rn "hashpacktable_internal" --include="*.c" | grep -v "^langhash.c"
```

**Expected Callers**:
- `tablepacktable_internal()` in `tablepack.c` ✅ Already passes context
- `langhtml.c` - May need context initialization
- `tableverbs.c` - Via `hashpacktable_context()` wrapper ✅
- `tablestructure.c` - Via `hashpacktable_context()` wrapper ✅

#### Step 3.2: Update remaining callers to always pass context

```c
// Pattern for updating callers
db_context ctx;
db_context_init(&ctx);  // Initialize from current global state
if (!hashpacktable_internal(&ctx, htable, flmemory, hpacked, flmustsave))
    return false;
```

#### Step 3.3: Make context parameter non-nullable

```c
// BEFORE
boolean hashpacktable_internal(const db_context *ctx, ...);

// AFTER
boolean hashpacktable_internal(const db_context *ctx, ...) {
    assert(ctx != NULL);  // Context is now mandatory
    boolean use_64bit = ctx->mode.use_64bit_format;
    // Remove fallback to db_format_mode_current()
}
```

**Files Modified**: 3-5 files (`langhash.c`, `tablepack.c`, `langhtml.c`, etc.)
**Lines Changed**: ~50 lines
**Risk**: LOW (cleanup phase, tests should catch any issues)

**Testing**:
```bash
# Verify no global mode reads in hash operations
grep -n "db_format_mode_current" common/source/langhash.c
# Expected: ZERO matches

# Run full test suite
./tools/run_headless_tests.sh

# Check for assertion failures
./tools/run_headless_tests.sh 2>&1 | grep "assert"
```

**Success Criteria**:
- All tests pass
- No `db_format_mode_current()` calls in `langhash.c`
- No `db_format_mode_push/pop()` calls in `langhash.c`
- Context is always explicit, never implicit

---

### Phase 4: Extend to Table Operations (Optional, 1 week)

**Goal**: Apply same pattern to other table-related operations

**Scope**: Functions in `tablestructure.c` that may read global mode:
- `tableassign()` → needs context variant
- `tablereference()` → needs context variant
- Table iteration/search operations

**Decision Point**: Is this necessary for correctness, or just architectural cleanup?

**Recommendation**: DEFER until Phase 1-3 complete and validated. Monitor for bugs that would be prevented by this work.

---

## Risk Assessment & Mitigation

### Risk 1: Breaking Recursive Operations
**Probability**: MEDIUM
**Impact**: HIGH (data corruption)

**Mitigation**:
- Start with minimal fix (Phase 1)
- Add context assertions to catch NULL context
- Test with complex nested table structures
- Create regression test with 5+ levels of table nesting

**Test Case**:
```usertalk
// Create deeply nested table structure
workspace.test_table.level1.level2.level3.level4.level5 = "deep value"
table.pack(@workspace.test_table)
// Verify all levels pack correctly
```

### Risk 2: Caller Misuse (NULL Context)
**Probability**: LOW
**Impact**: MEDIUM (runtime failures)

**Mitigation**:
- Phase 3 makes context mandatory with assertions
- Audit all callers before Phase 3
- Add debug logging when context is NULL (Phase 1-2)

### Risk 3: Performance Regression
**Probability**: LOW
**Impact**: LOW (minimal overhead)

**Mitigation**:
- Context passing is pointer copy (negligible cost)
- Context lookup replaces mode stack lookup (same cost)
- Benchmark migration before/after

**Benchmark**:
```bash
time ./tools/run_headless_tests.sh
# Compare before/after Phase 1-3
```

### Risk 4: Incomplete Refactor
**Probability**: MEDIUM
**Impact**: MEDIUM (leaves technical debt)

**Mitigation**:
- Document remaining global mode usage
- Create GitHub issues for deferred work
- Add TODO comments with issue numbers

---

## Testing Strategy

### Unit Tests (Per-Phase)

**Phase 1 Tests**:
```c
// tests/langhash_context_tests.c
void test_hashpack_external_passes_context(void) {
    // Create external table variable
    // Pack it during migration
    // Verify table header version = 5 (not 4)
}

void test_hashpack_recursive_uses_parent_context(void) {
    // Create nested table: table1.table2.table3
    // Pack table1 with v7 context
    // Verify all child tables use v7 format
}
```

**Phase 2 Tests**:
```c
void test_hashpack_no_global_mode_reads(void) {
    // Set global mode to v6
    // Pack table with explicit v7 context
    // Verify table uses v7 format (not global v6)
}
```

### Integration Tests

**Migration Validation**:
```bash
# Comprehensive migration test
./tools/run_headless_tests.sh

# Specific external variable test
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "sizeOf(system.verbs.globals)"
# Expected: Returns table size, not error
```

**Table Header Validation**:
```bash
# Add to save_migration_tests.c
void test_all_tables_have_v5_headers(void) {
    // Walk all tables in migrated database
    // Verify header.version == 5 for each
}
```

### Regression Tests

**Complex Nesting**:
```usertalk
// Create structure with multiple nesting patterns
workspace.test = {}
workspace.test.table1 = table.new()
workspace.test.table1^.subtable = table.new()
workspace.test.table1^.subtable^.script = "1+1"  // External (script)
workspace.test.table1^.subtable^.outline = outline.new()  // External (outline)

// Migrate and verify all externals + tables intact
```

**External Variable Coverage**:
Test all external types during migration:
- ✅ Outlines/scripts (tested via `langexternalpack_internal`)
- ✅ WPText (tested via `wpverbpack_internal`)
- ✅ Tables (tested via `tableverbpack_internal`)
- ⚠️ Menus (may need context support)
- ⚠️ Pictures (may need context support)

---

## Validation Criteria

### Phase 1 Success Metrics

- [ ] Migration test passes
- [ ] All tables in migrated v7 database have `version=5` headers
- [ ] External table variables accessible: `sizeOf(system.verbs.globals)` works
- [ ] Logs show NO mode flip-flopping during table packing
- [ ] Context passed explicitly through `hashpackexternal()` chain

**Log Signature (Success)**:
```
[headless] hashpacktable_internal use_64bit=1 (ctx=0x7ffee... ctx_mode=1 ...)
[headless] langexternalpack: using v7 write context (no global mode set)
[headless] hashpacktable_internal use_64bit=1 (ctx=0x7ffee... ctx_mode=1 ...)
```

**NOT**:
```
[headless] hashpacktable_internal use_64bit=1 ...
[headless] db_format_mode_apply use_64bit=0 ...  # WRONG!
[headless] hashpacktable_internal use_64bit=0 ...  # WRONG!
```

### Phase 2 Success Metrics

- [ ] All Phase 1 metrics pass
- [ ] `use_64bit` boolean removed from `typackinforecord`
- [ ] All hash helpers derive format from context, not parameters
- [ ] `grep "db_format_mode_current" langhash.c` returns only fallback in `hashpacktable_internal`

### Phase 3 Success Metrics

- [ ] All Phase 1-2 metrics pass
- [ ] `grep "db_format_mode_current" langhash.c` returns ZERO matches
- [ ] Context assertions prevent NULL context bugs
- [ ] All callers explicitly initialize context

---

## Rollback Plan

### Phase 1 Rollback
**Trigger**: Migration test fails, tables corrupted

**Steps**:
1. Revert `langhash.c` changes
2. Run baseline migration test to confirm rollback works
3. Document failure mode for future attempt

**Effort**: 1 hour

### Phase 2 Rollback
**Trigger**: Unexpected test failures, performance regression

**Steps**:
1. Keep Phase 1 changes (proven stable)
2. Revert `use_64bit` removal from `typackinforecord`
3. Restore boolean parameters to helpers

**Effort**: 2 hours

### Phase 3 Rollback
**Trigger**: Callers can't be updated to provide context

**Steps**:
1. Keep Phase 1-2 changes (proven stable)
2. Restore `db_format_mode_current()` fallback
3. Remove assertions on NULL context

**Effort**: 1 hour

---

## Decision: Minimal Fix vs. Comprehensive Refactoring

### Recommendation: INCREMENTAL (Minimal → Comprehensive)

**Rationale**:
1. **Phase 1 is low-risk, high-value**: Fixes the immediate bug with ~30 lines changed
2. **Validation checkpoint**: Can ship Phase 1, monitor in production
3. **Learning opportunity**: Phase 1 reveals edge cases for Phase 2-3
4. **Incremental delivery**: Each phase ships independently if needed

**Shipping Strategy**:
- **Week 1**: Ship Phase 1, monitor logs
- **Week 2**: If stable, proceed to Phase 2
- **Week 3**: If stable, proceed to Phase 3
- **Week 4**: Phase 4 evaluation (defer vs. implement)

**Alternative (NOT Recommended)**: Big-bang refactor all phases at once
- **Risk**: Higher chance of breaking multiple systems
- **Debugging**: Harder to isolate failures
- **Timeline**: Delays any fix for 3-4 weeks

---

## Code Ownership & Review

### Critical Path Files (Requires Architecture Review)
- `langhash.c` - Core hash packing (THIS IS THE KEY FILE)
- `tablepack.c` - Table packing wrapper
- `db_format.c` - Format adapter (already has context support)

### Standard Review Files
- `langexternal.c` - Already follows pattern, minimal changes
- `tableverbs.c` - Wrapper updates only
- Test files - Standard test review

### Review Checklist

**Before Merging Phase 1**:
- [ ] Migration test passes on developer machine
- [ ] Migration test passes in CI
- [ ] Code reviewer confirms context threading is correct
- [ ] Logs show deterministic mode usage (no flip-flopping)
- [ ] External table variables tested manually

**Before Merging Phase 2**:
- [ ] All Phase 1 checklist items
- [ ] Grep confirms no residual `use_64bit` parameters
- [ ] Performance benchmark shows no regression

**Before Merging Phase 3**:
- [ ] All Phase 2 checklist items
- [ ] Grep confirms no `db_format_mode_current()` in `langhash.c`
- [ ] Assertion failures tested (NULL context → assert fires)

---

## Estimated Scope Summary

| Phase | Files | Lines Changed | Risk | Duration | Shippable? |
|-------|-------|---------------|------|----------|------------|
| 0 | 0 | 0 | None | 1 day | No |
| 1 | 1 | ~30 | Medium | 3-5 days | **YES** |
| 2 | 1 | ~100 | Medium | 5-7 days | **YES** |
| 3 | 3-5 | ~50 | Low | 2-3 days | **YES** |
| 4 | 5-10 | ~200 | Medium | 5-7 days | Deferred |
| **Total** | **~10** | **~380** | **Medium** | **3-4 weeks** | **Incremental** |

---

## Related Work & Future Improvements

### Immediate Next Steps (After Phase 3)
- [ ] Issue #147 closed - Table packing uses explicit context
- [ ] Document pattern in `CLAUDE.md` for future developers
- [ ] Add static analysis rule: Detect `db_format_mode_current()` in pack/unpack functions

### Future Architectural Improvements
- [ ] Extend pattern to menu/pict operations (Phase 4)
- [ ] Eliminate ALL global mode state (replace with context everywhere)
- [ ] Thread-safety audit for multi-database scenarios
- [ ] Performance profiling of context passing overhead

### Known Limitations After Phase 3
- Menu operations may still use global mode (low priority - not used in migration)
- Picture operations may still use global mode (low priority - not used in migration)
- Some legacy code paths preserve global mode for backward compatibility

---

## References

**Planning Documents**:
- `planning/architectural_decision_records/mode_management_single_decision_point.md` - Single decision point principle
- `planning/architectural_decision_records/ADR-001-multi-database-context.md` - Multi-database context management
- `planning/phase3/MIGRATION_VALIDATION_REPORT.md` - Migration testing procedures

**Code**:
- `common/headers/db_format.h` - Context struct definitions
- `common/source/db_format.c` - Context helpers implementation
- `common/source/langhash.c` - Hash pack/unpack (CRITICAL PATH)
- `common/source/langexternal.c` - External packing (REFERENCE IMPLEMENTATION)
- `common/source/tablepack.c` - Table packing wrapper

**Issues**:
- #147 - Table pack/unpack mode state bug
- #123 - External table variable migration bug (related)

---

## Questions for User

Before proceeding with implementation:

1. **Approval for Phase 1?** Should we proceed with minimal fix (~30 lines, 3-5 days)?
2. **Shipping strategy?** Ship Phase 1 independently, or wait for Phase 2-3?
3. **Phase 4 priority?** Extend to menu/pict operations now, or defer?
4. **Test coverage?** Are current migration tests sufficient, or add more?
5. **Performance requirements?** Any migration performance SLAs we need to maintain?

---

**Status**: Ready for review and approval
**Next Action**: Await user decision on phasing strategy
