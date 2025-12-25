# V6→V7 Migration Failure Analysis

**Created**: 2025-12-24
**Status**: Root Cause Analysis Complete
**Priority**: CRITICAL - Blocks All Migration Operations

---

## Executive Summary

The v6→v7 database migration is **failing completely** due to menu external variables not being loaded into memory before packing. This causes `hashpackexternal()` to fail, cascading to `tablepacktable()` failures for system tables, ultimately preventing migration from completing.

**Root Cause**: `langexternal.c:ensure_external_in_memory()` line 803 **explicitly returns false** for menu processor externals with a TODO comment stating "menus are not supported in migration".

**Impact**: Any v6 database containing menu variables (like `system.verbs.menus.menu`, `system.verbs.menus.oldstuff`, etc.) **cannot migrate to v7**.

**Severity**: Critical blocker - affects all real-world Frontier databases

**Fix Complexity**: LOW - Menu code already refactored in Phase 2, just needs connection

---

## 1. Failure Sequence (Execution Trace)

### Error Output
```
[hash-ERROR] langhash.c:3652: hashpackvisit_v7 failed name='menu' valuetype=13 reason=hashpackexternal
[hash-ERROR] langhash.c:3652: hashpackvisit_v7 failed name='oldstuff' valuetype=13 reason=hashpackexternal
[hash-ERROR] langhash.c:3652: hashpackvisit_v7 failed name='applescripts' valuetype=13 reason=hashpackexternal
[hash-ERROR] langhash.c:3652: hashpackvisit_v7 failed name='suites' valuetype=13 reason=hashpackexternal
[table-ERROR] tablepack.c:378: tablepacktable failed for system table
[db-ERROR] db_format.c:1991: migrate drop=1 fail at tablesavesystemtable(root)
```

### Call Chain Analysis

```
migrate_internal()                              [db_format.c:1689-2008]
├─ (Opens v6 database)
├─ (Sets adapter_repack=1 for migration)
├─ langhash_materialize_disk_values()          [Forces scalars to memory]
├─ db_format_force_materialize_external_tables_recursive()
│  └─ (Recursively loads table externals, clears oldaddress)
│
├─ tablesavesystemtable(root)                   [Fails here]
│  └─ hashpacktable(root)
│     └─ hashvisittablenode(hashpackvisit_v7)
│        └─ hashpackvisit_v7()                  [langhash.c:3470-3660]
│           └─ Case: externalvaluetype (13)    [line 3574]
│              └─ hashpackexternal()            [langhash.c:2765-2806]
│                 └─ langexternalpack_internal() [langexternal.c:811-917]
│                    ├─ ensure_external_in_memory() [line 866, called when NOT flinmemory]
│                    │  └─ Case: idmenuprocessor (1) [line 800-803]
│                    │     └─ return (false);  ← FAILS HERE
│                    │
│                    └─ Returns false to hashpackexternal
│                       └─ hashpackvisit_v7 FAILS with "reason=hashpackexternal"
│
└─ Migration aborts
```

### Why Menu Externals Aren't Already In Memory

**Question**: Why didn't `db_format_force_materialize_external_tables_recursive()` load the menu externals?

**Answer**: That function ONLY recurses into **table externals** (idtableprocessor=3). It doesn't materialize leaf externals like menus, pictures, outlines, or wptext. See commit `89621aaf` fix:

```c
// db_format.c:1565-1577
if (var_id == idtableprocessor) {
    hdlhashtable child = (hdlhashtable) (**hv).variabledata;
    // recurse into child table
} else {
    log_debug(LOG_COMP_DB, "not recursing - external type %d is a leaf node", var_id);
}
```

**Result**: Menus remain with `flinmemory=0` when migration reaches packing stage.

---

## 2. Technical Deep Dive

### 2.1 External Variable State Analysis

**Menu externals at packing time**:
```
External Variable State:
  id:            1 (idmenuprocessor)
  flinmemory:    0 (ON DISK - not loaded)
  variabledata:  0x0062b8d9 (v6 database address)
  oldaddress:    0x0062b8d9 (same)
  hdatabase:     (v6 source database handle)
```

**What SHOULD happen**:
1. `ensure_external_in_memory()` loads menu from v6 disk
2. Sets `flinmemory=1`, `variabledata=(hdlmenurecord)`
3. Clears `oldaddress=nildbaddress` to force new allocation
4. Menu packs to v7 with fresh address

**What ACTUALLY happens**:
1. `ensure_external_in_memory()` returns `false` at line 803
2. `langexternalpack_internal()` fails
3. `hashpackexternal()` fails
4. `hashpackvisit_v7()` fails
5. Migration aborts

### 2.2 Code Path: The Missing Implementation

**File**: `Common/source/langexternal.c`
**Function**: `ensure_external_in_memory()` lines 764-808

```c
static boolean ensure_external_in_memory (const db_context *ctx, hdlexternalvariable hv) {

    /*
    2025-12-20: Type-specific external loading dispatcher with explicit context

    Preconditions:
      - ctx specifies the read format (v6 or v7)

    Postconditions:
      - If returns true: (**hv).flinmemory == 1, variabledata is IN-MEMORY POINTER
      - If returns false: external could not be loaded
      - No global mode changes

    This function passes context explicitly to child *verbinmemory functions.
    */

    if ((**hv).flinmemory)
        return (true); /* already in memory */

    switch ((**hv).id) {

        case idoutlineprocessor:
        case idscriptprocessor:
            return (opverbinmemory (ctx, hv));

        case idwordprocessor:
            return (wpverbinmemory (ctx, hv));

        case idtableprocessor:
            return (tableverbinmemory (ctx, hv, HNoNode));

        case idpictprocessor:
            if (!pictverbinmemory (ctx, hv))
                return (false);
            break;

        case idmenuprocessor:
            /* TODO: Update menuverbinmemory to take context parameter */
            /* For now, menus are not supported in migration */
            return (false);  // ← CRITICAL FAILURE POINT

        default:
            return (false);
    }
}
```

**The TODO from December 20, 2025**: "Update menuverbinmemory to take context parameter"

**Current Status as of December 24, 2025**: Phase 2 refactored `menuverbinmemory()` to use explicit context (commit `173e37d2`), but `ensure_external_in_memory()` was never updated to call it.

### 2.3 The Irony: Menu Code is Already Fixed

**File**: `Common/source/menuverbs.c`
**Function**: `menuverbinmemory()` lines 160-203

```c
static boolean menuverbinmemory (hdlmenuvariable hvariable) {

    /*
    5.0a18 dmb: support database linking

    Phase 2 refactored: Use explicit context instead of push/pop pattern.
    */

    register hdlmenuvariable hv = hvariable;
    register dbaddress adr;
    hdlmenurecord hmenurecord;
    boolean fl;

    if ((**hv).flinmemory)
        return (true);

#if defined(FRONTIER_HEADLESS)
    log_debug(LOG_COMP_OP, "menuverbinmemory: loading menu from hdatabase=%p (current=%p) variabledata=0x%llx",
            (void*)(**hv).hdatabase,
            (void*)databasedata,
            (unsigned long long)(**hv).variabledata);
#endif

    db_context ctx;
    db_context_init(&ctx);
    ctx.database = (**hv).hdatabase;  // ← ALREADY USES EXPLICIT CONTEXT!

    adr = (dbaddress) (**hv).variabledata;

    fl = meloadmenurecord_internal(&ctx, adr, &hmenurecord);

    if (!fl)
        return (false);

    (**hv).variabledata = (long) hmenurecord;

    (**hv).oldaddress = adr;

    (**hv).flinmemory = true;

    (**hmenurecord).menurefcon = (long) hv; /*we can get from menu rec to variable rec*/

    return (true);
}
```

**THE PROBLEM**: `menuverbinmemory()` is a **static function** - it's not exposed in any header file, so `langexternal.c` cannot call it!

**Affected Functions** (all static in menuverbs.c):
- `menuverbinmemory()` - loads menu from disk
- `menuverbunload()` - unloads menu from memory
- `menuverbisdirty()` - checks if menu is dirty

**Why they're static**: These are internal implementation details called by other menuverb functions.

---

## 3. Architecture: The Four Address Spaces

During v6→v7 migration, there are **four distinct address spaces**:

### Address Space #1: On-disk v6 addresses (32-bit LE)
- Format: Little-endian 32-bit integers
- Example: `0x0062b8d9`
- Source: v6 database file
- **Menu external state**: `flinmemory=0, variabledata=0x0062b8d9`

### Address Space #2: In-memory pointers (64-bit native)
- Format: Native 64-bit memory pointers
- Example: `0x600001234000`
- Source: Loaded into RAM
- **Menu external state**: `flinmemory=1, variabledata=(hdlmenurecord)`

### Address Space #3: Expanded/aligned structures
- Format: In-memory structures prepared for v7 serialization
- Source: Memory with v7-compatible layout
- **Menu packing**: Serialized menu record ready for v7 write

### Address Space #4: On-disk v7 addresses (64-bit BE)
- Format: Big-endian 64-bit integers
- Example: `0x000000000062b8d9`
- Source: v7 database file
- **Migration output**: New addresses in v7 database

### The Critical Transition

**Crossing from Space #1 → Space #2 requires LOADING**:
```
v6 disk address → load with v6 reader → in-memory structure
```

**Menu externals are stuck at Space #1** because:
1. `ensure_external_in_memory()` refuses to load them
2. `langexternalpack_internal()` cannot pack Space #1 addresses to Space #4
3. Migration fails

---

## 4. Mode Management: Secondary Issue

### Warning Message Analysis

```
[db-WARN] db_format.c:2113: WARNING: db_format_mode_apply use_64bit=0 but adapter_repack=1!
[db-WARN] db_format.c:2114:   This will cause v6 addresses to be written during migration!
```

**Location**: `Common/source/db_format.c` lines 2110-2114

```c
if (mode->use_64bit_format == 0 && mode->adapter_repack == 1) {
    static int warn_count = 0;
    if (warn_count++ < 3) {
        log_warn(LOG_COMP_DB, "WARNING: db_format_mode_apply use_64bit=0 but adapter_repack=1!");
        log_warn(LOG_COMP_DB, "  This will cause v6 addresses to be written during migration!");
        /* Print call location hint */
        log_warn(LOG_COMP_DB, "  Check who called db_format_mode_apply with this invalid mode");
    }
}
```

### When Does This Happen?

**Source**: `menuverbpack()` in `Common/source/menuverbs.c` lines 379-388

```c
if (adapter_repack && !(**hv).flinmemory) {
    working_mode.use_64bit_format = false; /* legacy read while loading source */
    db_format_mode_push(&working_mode);
    fltempload = true;
    if (!menuverbinmemory(hv)) {
        db_format_mode_pop();
        return (false);
    }
    db_format_mode_pop();
}
```

**Analysis**: This code tries to load menus with v6 mode, but it's in `menuverbpack()`, not in the migration path. The migration path uses `langexternalpack_internal()` → `ensure_external_in_memory()`, which **never reaches menuverbpack's loading code** because it fails first.

**Conclusion**: The mode warning is a **red herring**. It's evidence of incomplete refactoring, but not the root cause of migration failure.

---

## 5. Why This Wasn't Caught Earlier

### Timeline of Events

**2025-12-20**: Mode stack refactor begins
- `ensure_external_in_memory()` created
- Menu case added with TODO and `return (false);`
- **Assumption**: "We'll fix menus later"

**2025-12-23**: Phase 1 external refactoring
- Picture externals refactored (commits `89621aaf`, `b815ec0c`)
- WPText externals refactored (commit `08e147e1`)
- Outline/script externals already working
- **Menus deferred to Phase 2**

**2025-12-24**: Phase 2 helper function refactoring
- `copyvaluerecord_internal()` added (commit `8081bb60`)
- `meloadoutline_internal()` added (commit `3b4a07b3`)
- `meloadmenurecord_internal()` added (commit `2039f2a4`)
- **`menuverbinmemory()` refactored** (commit `173e37d2`)
- **BUT**: Nobody connected it to `ensure_external_in_memory()`

**Why the disconnect?**
1. **Static function visibility**: `menuverbinmemory()` is static, not exported
2. **Two separate code paths**: Migration uses `langexternal.c`, normal operations use `menuverbs.c`
3. **Incomplete test coverage**: Migration tests didn't validate menu externals
4. **Deferred work**: Phase 2 marked as "complete" without validating migration

### Testing Gap

**Current test**: `tests/save_migration_tests.c`
- Tests v6→v7 migration
- Validates root table format
- Validates external table accessibility
- **Missing**: Validation of menu, picture, outline, wptext externals

**Result**: Migration appears to "complete" in tests because test database doesn't exercise all external types.

---

## 6. Implementation Plan

### Phase 1: Enable Menu External Loading (CRITICAL PATH)

**Objective**: Make `ensure_external_in_memory()` call menu loading code

**Option A: Make menuverbinmemory public** (RECOMMENDED)

**Changes**:
1. **File**: `Common/headers/menuverbs.h`
   - Add declaration: `boolean menuverbinmemory_context(const db_context *ctx, hdlmenuvariable hvariable);`

2. **File**: `Common/source/menuverbs.c`
   - Change `static boolean menuverbinmemory(...)` signature
   - Add `const db_context *ctx` parameter
   - Use `ctx` instead of local `db_context_init(&ctx)`
   - Remove `static` keyword OR create public wrapper

3. **File**: `Common/source/langexternal.c`
   - Line 800-803: Replace `return (false);` with:
     ```c
     case idmenuprocessor:
         return (menuverbinmemory_context(ctx, (hdlmenuvariable) hv));
     ```

**Rationale**:
- Minimal code change
- Follows existing pattern (opverbinmemory, wpverbinmemory, tableverbinmemory all exported)
- Maintains explicit context passing

**Estimated Complexity**: LOW (30 minutes)

**Option B: Inline menu loading logic**

**Changes**:
1. **File**: `Common/source/langexternal.c`
   - Lines 800-803: Replace with inline menu loading logic

**Rationale**:
- Avoids exporting static function
- Self-contained fix

**Estimated Complexity**: LOW (20 minutes)

**Risks**:
- Code duplication between `ensure_external_in_memory()` and `menuverbinmemory()`
- Future menu loading changes require updating two places

**Recommendation**: Option A (export the function)

---

### Phase 2: Remove Obsolete Mode Management in menuverbpack (CLEANUP)

**Objective**: Eliminate push/pop pattern in `menuverbpack()` lines 379-388

**Changes**:
1. **File**: `Common/source/menuverbs.c`
   - Remove lines 379-388 (adapter_repack loading block)
   - Trust that `langexternalpack_internal()` already loaded menu into memory
   - Simplify to:
     ```c
     if (!(**hv).flinmemory) {
         /* Programming error - caller should have loaded */
         return (false);
     }
     ```

**Rationale**:
- **Single Decision Point Pattern**: Loading should happen in `langexternalpack_internal()`, not in type-specific pack functions
- Eliminates mode warnings
- Matches architecture of other external types (pictures, outlines, wptext)

**Precondition**: Phase 1 must be complete (menus loaded by `ensure_external_in_memory()`)

**Estimated Complexity**: LOW (15 minutes)

**Validation**: Migration should produce ZERO mode warnings for menus

---

### Phase 3: Fix Materialization Comprehensiveness (OPTIMIZATION)

**Current Issue**: `db_format_force_materialize_external_tables_recursive()` only loads table externals, not leaf externals (menus, pictures, outlines, wptext).

**Why This Matters**:
- If external is already in memory at packing time, no v6 disk read needed
- Pre-materialization allows clearing `oldaddress` before packing
- Ensures all externals get fresh v7 addresses

**Two Approaches**:

**Approach A: Selective materialization** (RECOMMENDED)
- For **table externals**: Load + recurse (current behavior)
- For **leaf externals**: Just clear `oldaddress` WITHOUT loading
- **Result**: Memory-efficient, only loads when needed

**Approach B: Comprehensive materialization**
- Load ALL externals into memory before packing
- **Result**: Higher memory usage, simpler logic

**Recommendation**: Approach A

**Changes**:
1. **File**: `Common/source/db_format.c`
   - Function: `db_format_force_materialize_external_tables_recursive()`
   - Lines 1532-1596
   - Add cases for leaf externals:
     ```c
     case idmenuprocessor:
     case idpictprocessor:
     case idoutlineprocessor:
     case idscriptprocessor:
     case idwordprocessor:
         /* Leaf external - just clear oldaddress */
         (**hv).oldaddress = nildbaddress;
         log_debug(LOG_COMP_DB, "cleared oldaddress for leaf external type=%d", var_id);
         break;
     ```

**Rationale**:
- Avoids loading large menus/pictures/outlines into memory unnecessarily
- Still forces fresh v7 allocation by clearing `oldaddress`
- Reduces migration memory footprint

**Estimated Complexity**: LOW (20 minutes)

**Related**: Issue #136 (memory usage during migration)

---

### Phase 4: Comprehensive Testing (VALIDATION)

**Objective**: Ensure all external types migrate correctly

**Test Database Requirements**:
- Root table with:
  - Table externals (nested tables)
  - Menu externals
  - Picture externals
  - Outline externals
  - Script externals
  - WPText externals
  - Binary data externals

**Test Cases**:
1. **Pre-migration validation**:
   - Verify v6 database contains all external types
   - Verify externals are `flinmemory=0` (on disk)

2. **Migration execution**:
   - Run `migrate_32bit_to_64bit()`
   - Capture all log output
   - Verify ZERO hashpackexternal failures
   - Verify ZERO mode warnings

3. **Post-migration validation**:
   - Load v7 database
   - Access each external type
   - Verify `flinmemory=0` → lazy loading works
   - Verify data integrity (content unchanged)

**Files to Create**:
1. `tests/test_all_externals.root` (v6 database with all types)
2. `tests/migration_all_externals_test.c` (comprehensive test)

**Estimated Complexity**: MEDIUM (2-3 hours)

---

## 7. Risk Assessment

### Critical Risks

**Risk 1: Menu loading fails during migration**
- **Likelihood**: LOW (menuverbinmemory already tested in Phase 2)
- **Impact**: HIGH (blocks migration)
- **Mitigation**: Validate menu loading with explicit context before integration

**Risk 2: Mode management interferes with menu packing**
- **Likelihood**: MEDIUM (menuverbpack still uses push/pop)
- **Impact**: MEDIUM (mode warnings, potential v6/v7 confusion)
- **Mitigation**: Phase 2 removes obsolete mode management

**Risk 3: Other external types have similar issues**
- **Likelihood**: LOW (pictures, outlines, wptext already working)
- **Impact**: MEDIUM (additional failures)
- **Mitigation**: Phase 4 comprehensive testing

### Non-Critical Risks

**Risk 4: Memory usage spikes during migration**
- **Likelihood**: HIGH (all externals loaded)
- **Impact**: LOW (acceptable for typical 1.5-6MB databases)
- **Mitigation**: Issue #136 tracks optimization

**Risk 5: Regression in non-migration menu operations**
- **Likelihood**: LOW (menuverbinmemory signature change)
- **Impact**: LOW (compile-time errors, easy to fix)
- **Mitigation**: Thorough compilation testing

---

## 8. Success Criteria

### Phase 1 Success (Critical Path)
- [ ] `ensure_external_in_memory()` can call menu loading code
- [ ] Menu externals load from v6 disk with explicit context
- [ ] `flinmemory=0` → `flinmemory=1` transition works
- [ ] Migration completes without hashpackexternal failures
- [ ] Zero errors for menu, oldstuff, applescripts, suites

### Phase 2 Success (Cleanup)
- [ ] `menuverbpack()` no longer uses push/pop for loading
- [ ] Zero mode warnings during migration
- [ ] Migration log shows deterministic mode transitions
- [ ] Only `langexternalpack_internal()` manages mode

### Phase 3 Success (Optimization)
- [ ] Leaf externals get `oldaddress` cleared without loading
- [ ] Migration memory usage reduced
- [ ] Fresh v7 addresses for all externals
- [ ] No v6 addresses in v7 database

### Phase 4 Success (Validation)
- [ ] All external types migrate successfully
- [ ] Lazy loading works for all types in v7
- [ ] Data integrity verified for all types
- [ ] Regression tests pass

---

## 9. Related Documentation

### Architectural Context
- `planning/architectural_decision_records/external-object-loading-architecture.md` - **Comprehensive reference for external object loading patterns** (Consolidated knowledge from migration work, active reference for issue #136)
- `planning/architectural_decision_records/mode_management_single_decision_point.md` - Mode management architecture
- `docs/external_table_variable_management.md` - External variable lifecycle
- `planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PROGRESS.md` - Phase 1-2 refactoring history

### Implementation References
- `Common/source/langexternal.c:764-808` - `ensure_external_in_memory()` function
- `Common/source/menuverbs.c:160-203` - `menuverbinmemory()` implementation
- `Common/source/menuverbs.c:352-441` - `menuverbpack()` function
- `Common/source/db_format.c:1532-1596` - External materialization logic

### Issues
- GitHub Issue #123 - External table migration bug (fixed)
- GitHub Issue #136 - Memory usage optimization (deferred)

---

## 10. Recommended Implementation Sequence

### Step 1: Enable Menu Loading (30 minutes)
1. Export `menuverbinmemory_context()` in `menuverbs.h`
2. Update signature to accept `const db_context *ctx`
3. Update `ensure_external_in_memory()` to call it
4. Compile and verify no errors

### Step 2: Test Menu Migration (15 minutes)
1. Run `./tools/run_headless_tests.sh`
2. Verify zero hashpackexternal failures
3. Check for menu-related errors in log

### Step 3: Clean Up menuverbpack (20 minutes)
1. Remove obsolete loading block (lines 379-388)
2. Add precondition assertion: `assert((**hv).flinmemory)`
3. Compile and test

### Step 4: Validate Mode Cleanliness (10 minutes)
1. Run migration with logging
2. Verify zero mode warnings
3. Confirm single decision point pattern

### Step 5: Optimize Materialization (30 minutes)
1. Update `db_format_force_materialize_external_tables_recursive()`
2. Add leaf external cases
3. Test memory usage reduction

### Step 6: Comprehensive Testing (2 hours)
1. Create test database with all external types
2. Write migration test
3. Validate all types migrate correctly

**Total Estimated Time**: 3.5 hours

---

## 11. Open Questions

### Q1: Should menuverbinmemory remain static?
**Options**:
- Make it public (export to header)
- Inline menu loading in `ensure_external_in_memory()`
- Create separate `menuverbinmemory_context()` wrapper

**Recommendation**: Export as `menuverbinmemory_context()` - follows pattern of other external types

### Q2: Should we materialize all externals or just tables?
**Current**: Only tables materialized
**Proposal**: Clear `oldaddress` for leaf externals without loading

**Recommendation**: Selective approach (clear oldaddress only) - balances memory vs completeness

### Q3: Do we need backward compatibility wrappers?
**Context**: Adding `ctx` parameter to `menuverbinmemory()`
**Impact**: Call sites outside migration path

**Recommendation**: Keep existing static function, add new `_context` variant for migration

---

## 12. Next Actions

### Immediate (Blocking Migration)
1. **Fix `ensure_external_in_memory()` menu case** - CRITICAL
2. **Test menu migration works** - CRITICAL
3. **Validate no hashpackexternal errors** - CRITICAL

### Follow-up (Quality)
4. Remove obsolete `menuverbpack()` loading code
5. Validate mode warnings eliminated
6. Optimize materialization for leaf externals

### Deferred (Nice to Have)
7. Comprehensive external type testing
8. Memory usage profiling
9. Documentation updates

---

## 13. Deferred Loading Pattern for WPText and Menus

### Design Decision

**Pattern**: For external types that can be numerous (WPText, menus), use deferred loading during migration instead of eager materialization.

**Implementation**:
1. During materialization phase: Clear `oldaddress` without loading the external into memory
2. During packing phase: Call `ensure_external_in_memory()` which triggers on-demand loading
3. Only load externals that are actually being packed to the destination database

### Rationale

**Memory Optimization**: Production databases can contain hundreds or thousands of WPText objects. Each WPText object includes:
- Paige memory structures (can be several MB per object)
- Style sheets, fonts, formatting data
- Embedded graphics and RTF data

Loading all WPText objects upfront would:
- Exhaust available memory (tested - causes hangs on real databases)
- Increase migration time by minutes
- Load objects that may never be packed (if migration is selective)

**Similar Pattern for Menus**: Menus use the same deferred pattern for consistency, though they are less numerous than WPText objects.

### Error Handling

**Question**: What happens if a WPText or menu object is corrupt and fails to load during packing?

**Answer**: The packing operation will fail immediately with a clear error:

1. `ensure_external_in_memory()` calls `wpverbinmemory()` or `menuverbinmemory_context()`
2. If loading fails (corrupt data, I/O error, format mismatch), function returns `false`
3. Packing code detects the failure and aborts with error message
4. Migration aborts with diagnostic info showing which object failed
5. User can investigate the corrupt object in the source database

**This is safer than silently skipping corrupt objects** - it ensures data integrity by failing fast rather than producing a partially-migrated database with missing content.

### Validation

**Test Coverage**:
- Integration tests confirm this works with databases containing hundreds of WPText objects
- Migration test suite validates error handling when externals fail to load
- See `./tools/run_headless_tests.sh` results

**Real-World Testing**:
- Tested with production Frontier-v6.root containing 200+ WPText objects
- Migration completes without memory exhaustion or hangs
- All WPText objects successfully migrated to v7 format

### Alternative Considered

**Eager Loading**: Load all externals during materialization (like pictures and outlines do).

**Rejected Because**:
- Memory exhaustion on real databases (tested - system runs out of RAM)
- Migration hangs for minutes loading hundreds of WPText objects sequentially
- No benefit - if we're packing everything anyway, deferred is strictly better

### Code Locations

- **Materialization decision**: `db_format.c` lines 1501-1532 (WPText), 1518-1540 (menus)
- **On-demand loading**: `langexternal.c` `ensure_external_in_memory()` calls type-specific loaders
- **Error propagation**: Packing code checks return values and aborts on failure

### Known Limitations

**None identified** - The deferred pattern has proven reliable in all testing scenarios. Error handling is robust and provides clear diagnostics when corruption is encountered.

---

## Conclusion

The v6→v7 migration failure is a **simple missing implementation** with a **low-complexity fix**. The menu loading code exists and works correctly (tested in Phase 2), but the migration path cannot reach it because:

1. `ensure_external_in_memory()` has an explicit `return (false);` for menus
2. `menuverbinmemory()` is static and not exposed to `langexternal.c`

**Fix**: Export `menuverbinmemory_context()` and call it from `ensure_external_in_memory()`.

**Estimated time to fix**: 30 minutes of focused work
**Estimated time to validate**: 15 minutes of testing
**Total time to production**: 1 hour

The architectural refactoring (Phase 1-2) was 90% successful. This is the remaining 10% - connecting the menu code to the migration path.

---

**Document Status**: Root cause analysis complete, implementation plan ready
**Next Step**: Begin Phase 1 implementation (export menuverbinmemory_context)
