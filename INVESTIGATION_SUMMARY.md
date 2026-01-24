# Investigation Summary: defined(webserver.init) Regression

**Date**: 2026-01-23
**Issue**: PR #337 broke `defined(webserver.init)` - returns false when it should return true
**Working**: `defined(builtins.webserver.init)` returns true (direct path works)

---

## Root Causes Identified

### Issue #1: Uninitialized Variable in evaluatereadonlyparam()
**File**: `Common/source/langvalue.c:4508`

**Problem**: Local variable `bigstring bs;` declared without initialization
- Contains garbage from previous stack usage
- When `langgetdotparams()` writes "init" (5 bytes), remaining 251 bytes still have garbage
- Causes string corruption: "init" + "erverSm" remnant = "initerverSm"

**Evidence**:
```
[lang-TRACE] langvalue.c:4027: langgetdotparams result table=0x... name=init
[lang-TRACE] langvalue.c:4539: evaluatereadonlyparam dot table=0x... name=init
[lang-TRACE] langvalue.c:118: langsymbolreference: htable=0x... looking for 'initerverSm'
                                                                                  ^^^^^^^^^^
```

**Fix Applied**: Added `setemptystring(bs);` at line 4512 ✅

---

### Issue #2: EFP Table Hydration Timing
**File**: `Common/source/tablestructure.c:543-607`

**Problem**: During `headless_init_system_paths()`:
- `findnamedtable(builtins, "webserver", ...)` was called
- But `builtins.webserver` is an external table variable with `flinmemory=0` (not loaded yet)
- At boot time, table hasn't been hydrated from disk
- `findnamedtable()` failed, so system.paths pointed to EFP stub (7 items) instead of full table (31 items)

**Fix Applied**: Force hydration before `findnamedtable()` ✅
```c
if (hashtablelookup(hbuiltins, bs_processor_name, &val_check, &hnode_check)) {
    if (val_check.valuetype == externalvaluetype) {
        hdlexternalvariable hv = (hdlexternalvariable)val_check.data.externalvalue;
        if (tableverbinmemory(NULL, hv, hnode_check)) {
            log_trace(LOG_COMP_LANG, "Hydrated external table %s before findnamedtable", cname);
        }
    }
}
```

---

### Issue #3: Corrupted system.paths Entries in Migrated Database
**Problem**: v6→v7 migration created entries named "path01", "path02", etc. with invalid address values
- These persist in the v7 database
- When loading v7 database, corrupted entries prevent proper path resolution

**Fix Applied**: Corruption detection and clearing ✅
```c
// If first entry is named "path01", "path02", etc., these are corrupted entries
if (strncmp(cfirstname, "path", 4) == 0 && isdigit(cfirstname[4])) {
    log_warn(LOG_COMP_LANG, "system.paths has corrupted entries, clearing and repopulating");
    // Clear all entries and fall through to repopulation
}
```

---

### Issue #4: Handle Management - Different Table Instances ❌ UNRESOLVED

**Problem**: Even after all fixes, `system.paths.webserver` and `builtins.webserver` resolve to different table instances:
- `@builtins.webserver`: 31 items, includes "init" ✅
- `@system.paths.webserver`: 22 items, does NOT include "init" ❌

**Expected**: Both should point to the SAME table handle

**Investigation Findings**:

1. **Value Copying Attempted**:
   - Changed from creating address values to copying external variable values directly
   - Log shows: `Copied external value from builtins.webserver (type=13)`
   - But still results in different table instances

2. **Augmentation May Modify Tables**:
   - Sequence: `headless_init_system_paths()` → `augment_database_tables_with_efp()`
   - Augmentation merges EFP kernel verbs into database tables
   - May create new table instances instead of modifying in-place
   - Augmentation skips external values (checks for `addressvaluetype`), but something still causes divergence

3. **Database Corruption**:
   - Accessing `system.paths.webserver` from saved v7 database triggers:
     - Multiple `dbread seek failed` errors (v6 addresses in v7 database)
     - Segmentation fault when trying to coerce to string
   - Suggests deeper migration/persistence issues

---

## Attempted Fixes

### ✅ Fix #1: Initialize bs Variable
**Status**: Applied and working
**Location**: `Common/source/langvalue.c:4512`
```c
bigstring bs;
setemptystring(bs);  /* Initialize to prevent reading garbage from stack */
```

### ✅ Fix #2: Force External Table Hydration
**Status**: Applied, partially working
**Location**: `Common/source/tablestructure.c:580-593`
- Successfully hydrates `builtins.webserver` before `findnamedtable()`
- Allows `findnamedtable()` to succeed
- But final table instance still wrong

### ✅ Fix #3: Detect and Clear Corrupted system.paths
**Status**: Applied and working
**Location**: `Common/source/tablestructure.c:500-527`
- Detects "path01/path02" pattern
- Clears corrupted entries
- Repopulates on each load

### ❌ Fix #4: Copy External Variable Values
**Status**: Applied, but ineffective
**Location**: `Common/source/tablestructure.c:615-642`
- Changed from creating address values to copying external variable values
- Intended to preserve same table handle
- Log confirms copy happens: `Copied external value from builtins.webserver (type=13)`
- BUT: Still results in different table instances (22 vs 31 items)

**Why This Failed**:
- External variable values may contain internal pointers that get re-resolved
- Copying the value struct doesn't guarantee same table handle after dereference
- `augment_database_tables_with_efp()` may still modify or replace tables
- Database persistence/loading may corrupt or split table instances

---

## Test Results

### Working Cases ✅
```bash
./frontier-cli/frontier-cli -e 'defined(builtins.webserver.init)'  # true
./frontier-cli/frontier-cli -e 'typeof(builtins.webserver.init)'   # scpt
./frontier-cli/frontier-cli -e 'sizeOf(builtins.webserver)'        # 31
```

### Failing Cases ❌
```bash
./frontier-cli/frontier-cli -e 'defined(webserver.init)'           # false (should be true)
./frontier-cli/frontier-cli -e 'typeof(webserver.init)'            # ERROR: init hasn't been defined
./frontier-cli/frontier-cli -e 'local(t=@webserver); sizeOf(t)'    # 22 (should be 31)
```

### Diagnostic Results
```bash
# Table size comparison
./frontier-cli/frontier-cli -e 'local(t1=@system.paths.webserver, t2=@builtins.webserver); return sizeOf(t1) + " vs " + sizeOf(t2)'
# Output: "22 vs 31"

# Path entry exists
./frontier-cli/frontier-cli -e 'defined(system.paths.webserver)'   # true ✅

# But wrong table
# @system.paths.webserver has 22 items, missing "init"
# @builtins.webserver has 31 items, includes "init"
```

---

## Boot Sequence Analysis

### Migration (v6 → v7)
1. `dbopenfile()` - Opens v6 database
2. `db_format_prepare_runtime()`:
   - `linksystemtablestructure()` - Links EFP tables
   - `headless_init_system_paths()` - Populates system.paths
     - ✅ **Now works**: Forces hydration, copies builtins values
     - Log: `Copied external value from builtins.webserver (type=13)`
     - Log: `Added system.paths.webserver -> builtins.webserver`
   - `augment_database_tables_with_efp()` - Merges EFP verbs into database tables
     - **May create new table instances here**
3. Database saved to v7 format

### Runtime (loading v7 database)
1. `dbopenfile()` - Opens v7 database
2. Detects corrupted system.paths entries ("path01", "path02")
3. Clears and repopulates system.paths
   - Same hydration + copy logic runs
   - **Still results in wrong table instance**

---

## Key Observations

1. **Two "webserver" entries created during migration**:
   ```
   [DEBUG] Added system.paths.webserver -> system.compiler.kernel.webserver
   [DEBUG] Copied external value from builtins.webserver (type=13)
   [DEBUG] Added system.paths.webserver -> builtins.webserver
   ```
   The second should overwrite the first, but maybe augmentation happens in between?

2. **External variable type (13) skipped by augmentation**:
   ```c
   // augment_database_tables_with_efp() line 712-713
   if (val->valuetype != addressvaluetype)
       continue; // Only process address values
   ```
   External values (type=13) are skipped, so `builtins.webserver` shouldn't be augmented.

3. **Database address corruption**:
   - Many `dbread seek failed` errors when accessing system.paths entries
   - Suggests v6 addresses persisted in v7 database
   - May explain why dereferencing gives wrong table

---

## Files Modified

### Common/source/langvalue.c
- **Line 4512**: Added `setemptystring(bs);` to fix uninitialized variable
- **Lines 4570-4590**: Added debug logging to trace table resolution

### Common/source/tablestructure.c
- **Line 31**: Added `#include <ctype.h>` for `isdigit()`
- **Lines 500-527**: Added corruption detection and clearing logic
- **Lines 580-593**: Added forced hydration before `findnamedtable()`
- **Lines 615-642**: Changed to copy external variable values instead of creating address values

---

## Related Issues

- **Issue #339**: P1 - Pascal string logging displays garbage characters
  - Same `stringbaseaddress(bs)` with `%s` pattern
  - Reads past Pascal string length byte
  - Separate issue but discovered during this investigation

---

## Recommended Next Steps

### Option A: Document and Review
**Status**: ✅ COMPLETE (this document)

### Option B: Simpler Workaround - Skip system.paths for Builtins
**Strategy**: Modify `langsearchpathvisit()` to check `builtins.PROCESSOR_NAME` directly before checking system.paths

**Rationale**:
- Avoids complex handle management issues
- `builtins.PROCESSOR_NAME` always has correct table
- system.paths still works for non-builtins processors
- Simpler, lower-risk change

**Implementation**: See next section for code changes

### Option C: Deep Investigation into Handle Management
- Understand why copying external variable values doesn't preserve table handles
- Debug `augment_database_tables_with_efp()` to see if it creates new table instances
- Fix database address corruption in system.paths
- More complex, higher risk, may uncover deeper architectural issues

---

## Integration Tests Created

**File**: `tests/integration/test_cases/path_resolution.yaml` (Section 10)

13 new test cases covering:
- Core regression: `defined(webserver.init)`
- Nested lookups: `webserver.init` (path-resolved)
- Direct lookups: `builtins.webserver.init` (baseline)
- External script object types
- Error cases (non-existent paths/children)
- Edge cases (case sensitivity)

**Current Status**: All 13 tests failing ❌ (waiting for fix)

---

## Conclusion

We've successfully fixed 3 out of 4 root causes:
1. ✅ Uninitialized variable
2. ✅ EFP table hydration timing
3. ✅ Corrupted system.paths detection

The remaining issue (#4 - handle management) is more complex than anticipated and involves:
- External variable handle semantics
- Table augmentation creating new instances
- Database address corruption in persisted v7 files
- Unclear ownership/lifecycle of table handles

**Recommendation**: Implement simpler workaround (Option B) to unblock users while investigating deeper architectural issues.
