# Headless Window Verbs Implementation Guide

## Status
- State: Implementation Guide Ready
- Phase: 3
- Last Updated: 2025-12-21
- Notes: Step-by-step implementation for headless window verb support; mid-level task

**Created**: 2025-12-21
**Purpose**: Step-by-step implementation guide for headless window verb support
**Target Audience**: Entry-level to mid-level C engineers with UserTalk knowledge

---

## Implementation Overview

**Total Effort**: 7-10 days
**Phases**: 5 sequential phases
**Engineers**: Mix of entry-level (Haiku) and mid-level (Sonnet) tasks
**Files Modified**: `tests/headless_window_verbs.c`, potentially new files

---

## Prerequisites

Before starting, ensure you have:
- [ ] Frontier codebase cloned and building successfully
- [ ] Run `./tools/run_headless_tests.sh` and all tests pass
- [ ] Familiarity with UserTalk verb binding patterns
- [ ] Understanding of external variables and ODB addressing
- [ ] Read `Common/source/langverbs.c` for target.get/set/clear implementation

**Required reading**:
1. `Common/headers/langexternal.h` - External variable types
2. `Common/source/langverbs.c:1870-1882` - Target management verbs
3. `tests/headless_window_verbs.c` - Current stub implementation
4. `Common/source/shellwindowverbs.c` - Original GUI implementation (for reference)

---

## Phase 1: Target Management Verbs (Entry-Level)

**Complexity**: ⭐ Low (Haiku)
**Effort**: 0.5 days
**Files**: `tests/headless_window_verbs.c`

### Task 1.1: Implement window.open()

**Purpose**: Make `window.open(adr)` set the current target to the given address

**Location**: `tests/headless_window_verbs.c`, case `winv_open`

**Current code**:
```c
case winv_open:
    /* Verb: window.open - not yet implemented */
    if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
    return false;
```

**New implementation**:
```c
case winv_open: {
    /* Verb: window.open - set current target to address */
    hdltreenode hparam1;
    tyvaluerecord vaddr;

    /* Get first parameter (the address to open) */
    if (!langgetparamnode(hfirst, 1, &hparam1))
        return false;

    flnextparamislast = true;

    /* Evaluate the parameter to get the address */
    if (!evaluatetree(hparam1, &vaddr))
        return false;

    /* Call langsettargetfunc - reuse existing target.set implementation */
    if (!langsettargetfunc(hfirst, vreturned))
        return false;

    return true;
}
```

**Testing**:
```bash
# Test 1: Basic open
./frontier-cli/frontier-cli -e "window.open(@system); defined(target.get())"
# Expected: true

# Test 2: Open sets target correctly
./frontier-cli/frontier-cli -e "window.open(@system.verbs); string(target.get())"
# Expected: "system.verbs"
```

**Acceptance criteria**:
- [ ] `window.open(adr)` successfully sets target
- [ ] Returns true on success
- [ ] Target can be retrieved with `target.get()`
- [ ] No compilation warnings

---

### Task 1.2: Implement window.close()

**Purpose**: Make `window.close()` clear the current target

**Location**: `tests/headless_window_verbs.c`, case `winv_close`

**Current code**:
```c
case winv_close:
    /* Verb: window.close - not yet implemented */
    if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
    return false;
```

**New implementation**:
```c
case winv_close: {
    /* Verb: window.close - clear current target */

    /* No parameters expected */
    if (!langcheckparamcount(hfirst, 0))
        return false;

    /* Call langcleartarget - reuse existing target.clear implementation */
    setbooleanvalue(langcleartarget(nil), vreturned);

    return true;
}
```

**Testing**:
```bash
# Test 1: Close clears target
./frontier-cli/frontier-cli -e "window.open(@system); window.close(); target.get()"
# Expected: undefined or empty

# Test 2: Close with no target
./frontier-cli/frontier-cli -e "window.close()"
# Expected: true (no error)
```

**Acceptance criteria**:
- [ ] `window.close()` clears target
- [ ] Returns true
- [ ] No crash if target already empty
- [ ] No compilation warnings

---

### Task 1.3: Testing Integration

**Create test file**: `tests/test_window_target_management.c`

```c
#include "frontier.h"
#include "standard.h"
#include "lang.h"
#include "langinternal.h"
#include <assert.h>

void test_window_open_sets_target(void) {
    // Test that window.open sets the target
    // Implementation left as exercise for engineer
}

void test_window_close_clears_target(void) {
    // Test that window.close clears the target
    // Implementation left as exercise for engineer
}

int main(void) {
    // Initialize runtime
    langinitverbs();

    // Run tests
    test_window_open_sets_target();
    test_window_close_clears_target();

    printf("All target management tests passed!\n");
    return 0;
}
```

**Note**: Full test implementation requires runtime initialization. Defer to Sonnet-level engineer for test harness setup. For now, use UserTalk script testing.

---

## Phase 2: Simple Property Verbs (Entry-Level)

**Complexity**: ⭐⭐ Low-Medium (Haiku with guidance)
**Effort**: 1-2 days
**Files**: `tests/headless_window_verbs.c`

### Task 2.1: Implement window.getTitle()

**Purpose**: Return the name of the object at the given address

**Background**: In Frontier, every object in the ODB has a name. We need to extract this name given an address.

**Location**: `tests/headless_window_verbs.c`, case `winv_gettitle`

**Research needed**:
1. Look at `Common/source/langverbs.c` for how `nameOf()` works
2. Find the C equivalent of UserTalk's `nameOf(adr^)`
3. Understand how to dereference an address to get an object name

**Pattern to follow**:
```c
case winv_gettitle: {
    /* Verb: window.getTitle - return name of object at address */
    hdltreenode hparam1;
    tyvaluerecord vaddr;
    bigstring bsname;

    /* Get address parameter */
    if (!langgetparamnode(hfirst, 1, &hparam1))
        return false;

    flnextparamislast = true;

    if (!evaluatetree(hparam1, &vaddr))
        return false;

    /* TODO: Extract object name from address */
    /* HINT: Look at how langgetnamefunc works in langverbs.c */
    /* HINT: May need to use langexternalgetname or similar */

    /* For now, placeholder: */
    setemptystring(bsname);

    return setstringvalue(bsname, vreturned);
}
```

**Research hints**:
- Search for `nameOf` in `Common/source/langverbs.c`
- Look for functions that take an address and return a name
- Check `Common/source/langexternal.c` for name extraction functions

**Testing**:
```bash
# Test: Get title of system table
./frontier-cli/frontier-cli -e "window.getTitle(@system)"
# Expected: "system"

# Test: Get title of nested object
./frontier-cli/frontier-cli -e "window.getTitle(@system.verbs)"
# Expected: "verbs"
```

**Acceptance criteria**:
- [ ] Returns correct object name
- [ ] Works with tables, outlines, scripts
- [ ] Returns empty string if address invalid
- [ ] No memory leaks

**Note to engineer**: If you can't find the right function after 2 hours of searching, escalate to Sonnet-level engineer for guidance.

---

### Task 2.2: Implement window.msg()

**Purpose**: Output a message to stdout

**Location**: `tests/headless_window_verbs.c`, case `winv_msg`

**This is simpler**:
```c
case winv_msg: {
    /* Verb: window.msg - output message to stdout */
    hdltreenode hparam1;
    tyvaluerecord vtext;
    bigstring bs;

    /* Get message text parameter */
    if (!getstringvalue(hfirst, 1, bs))
        return false;

    flnextparamislast = true;

    /* Convert from Pascal string to C string for printing */
    nullterminate(bs);

    /* Output to stdout */
    printf("%s\n", stringbaseaddress(bs));

    /* Return true */
    setbooleanvalue(true, vreturned);
    return true;
}
```

**Testing**:
```bash
# Test: Simple message
./frontier-cli/frontier-cli -e "window.msg('Hello from headless')"
# Expected stdout: Hello from headless

# Test: Message with special characters
./frontier-cli/frontier-cli -e "window.msg('Testing: 1+1=' + (1+1))"
# Expected stdout: Testing: 1+1=2
```

**Acceptance criteria**:
- [ ] Message appears on stdout
- [ ] Returns true
- [ ] Handles empty strings
- [ ] Handles special characters correctly

---

### Task 2.3: Implement Error-Returning Verbs

**Purpose**: Return appropriate errors for unsupported operations

**Location**: `tests/headless_window_verbs.c`

**These are simple - just return errors**:

```c
case winv_dbstats: {
    /* Verb: window.dbStats - not available in headless */
    if (bserror)
        copystring(BIGSTRING("\pCan't display database statistics because GUI is not available in headless mode"), bserror);
    return false;
}

case winv_quicktime: {
    /* Verb: window.quickTime - not supported */
    if (bserror)
        copystring(BIGSTRING("\pQuickTime is not supported on this platform"), bserror);
    return false;
}
```

**Testing**:
```bash
# Test: dbStats returns error
./frontier-cli/frontier-cli -e "window.dbStats()"
# Expected: Error message about GUI not available

# Test: quickTime returns error
./frontier-cli/frontier-cli -e "window.quickTime()"
# Expected: Error message about not supported
```

**Acceptance criteria**:
- [ ] Returns false
- [ ] Sets appropriate error message
- [ ] Error message follows pattern: "Can't do X because Y"

---

## Phase 3: ODB Property Verbs (Mid-Level)

**Complexity**: ⭐⭐⭐ Medium (Sonnet)
**Effort**: 2-3 days
**Files**: `tests/headless_window_verbs.c`

### Task 3.1: Research ODB Object Flags

**Purpose**: Understand how read-only and modified flags are stored in external variables

**Research tasks**:
1. **Find external variable structure**:
   - Look in `Common/headers/langexternal.h` for external variable definitions
   - Search for `hdlexternalvariable` and related structures
   - Identify flag fields

2. **Find flag access patterns**:
   - Search codebase for "flreadonly" or "fldirty" or "flmodified"
   - Look at how table/outline code checks these flags
   - Find existing setter/getter functions

3. **Document findings**:
   Create `ODB_FLAGS_RESEARCH.md` with:
   - Structure definitions found
   - Flag field names and meanings
   - Existing functions that access these flags
   - Code examples from existing usage

**Expected findings**:
```c
// Example (may vary - verify in actual code)
typedef struct tyexternalvariable {
    short id;  // Type: idtableprocessor, idoutlineprocessor, etc.
    boolean flinmemory: 1;
    boolean flreadonly: 1;  // ← Read-only flag?
    boolean fldirty: 1;     // ← Modified flag?
    // ... more fields
} tyexternalvariable;
```

**Deliverable**: Document with findings for implementation tasks

---

### Task 3.2: Implement window.isReadOnly()

**Purpose**: Check if an ODB object is read-only

**Prerequisites**: Complete Task 3.1 research

**Location**: `tests/headless_window_verbs.c`, case `winv_isreadonly`

**Implementation pattern**:
```c
case winv_isreadonly: {
    /* Verb: window.isReadOnly - check if object is read-only */
    hdltreenode hparam1;
    tyvaluerecord vaddr;
    hdlexternalvariable hv;
    boolean flreadonly = false;

    /* Get address parameter */
    if (!langgetparamnode(hfirst, 1, &hparam1))
        return false;

    flnextparamislast = true;

    if (!evaluatetree(hparam1, &vaddr))
        return false;

    /* Check if this is an external variable */
    if (vaddr.valuetype == externalvaluetype) {
        hv = (hdlexternalvariable) vaddr.data.externalvalue;

        /* TODO: Check the read-only flag */
        /* IMPLEMENTATION DEPENDS ON RESEARCH FROM TASK 3.1 */
        /* Example (verify actual structure): */
        /* flreadonly = (**hv).flreadonly; */
    }

    setbooleanvalue(flreadonly, vreturned);
    return true;
}
```

**Key considerations**:
- Different external types (table, outline, script) may store flags differently
- Need to check the external type ID first
- May need to call type-specific functions

**Testing**:
```bash
# Test: Check if system is read-only
./frontier-cli/frontier-cli -e "window.isReadOnly(@system)"
# Expected: false (system is writable)

# Test: Check read-only table (if you can create one)
./frontier-cli/frontier-cli -e "new(tableType, @test); window.isReadOnly(@test)"
# Expected: false (newly created tables are writable)
```

**Acceptance criteria**:
- [ ] Returns correct read-only status
- [ ] Works with tables, outlines, scripts
- [ ] Returns false for non-external values
- [ ] No crashes on invalid addresses

---

### Task 3.3: Implement window.isModified()

**Purpose**: Check if an ODB object has been modified

**Similar to Task 3.2**, but checking dirty/modified flag

**Location**: `tests/headless_window_verbs.c`, case `winv_ismodified`

**Implementation pattern**:
```c
case winv_ismodified: {
    /* Verb: window.isModified - check if object is modified */
    hdltreenode hparam1;
    tyvaluerecord vaddr;
    hdlexternalvariable hv;
    boolean flmodified = false;

    /* Get address parameter */
    if (!langgetparamnode(hfirst, 1, &hparam1))
        return false;

    flnextparamislast = true;

    if (!evaluatetree(hparam1, &vaddr))
        return false;

    /* Check if this is an external variable */
    if (vaddr.valuetype == externalvaluetype) {
        hv = (hdlexternalvariable) vaddr.data.externalvalue;

        /* TODO: Check the modified/dirty flag */
        /* May need to call type-specific function */
        /* Example approaches:
         * 1. Check (**hv).fldirty directly
         * 2. Call tableisdirty(htable) for tables
         * 3. Call opisdirty(houtline) for outlines
         */
    }

    setbooleanvalue(flmodified, vreturned);
    return true;
}
```

**Research needed**:
- How do table/outline/script types track modifications?
- Is there a unified dirty flag or type-specific functions?
- Look for: `tableisdirty`, `opisdirty`, similar functions

**Testing**:
```bash
# Test: Unmodified object
./frontier-cli/frontier-cli -e "window.isModified(@system)"
# Expected: depends on current state

# Test: After modification
./frontier-cli/frontier-cli -e "system.test = 1; window.isModified(@system)"
# Expected: true
```

---

### Task 3.4: Implement window.setModified()

**Purpose**: Set the modified flag on an ODB object

**Location**: `tests/headless_window_verbs.c`, case `winv_setmodified`

**Implementation pattern**:
```c
case winv_setmodified: {
    /* Verb: window.setModified - set modified flag */
    hdltreenode hparam1;
    tyvaluerecord vaddr, vflag;
    hdlexternalvariable hv;
    boolean flnewvalue;

    /* Get address parameter */
    if (!langgetparamnode(hfirst, 1, &hparam1))
        return false;

    if (!evaluatetree(hparam1, &vaddr))
        return false;

    /* Get boolean flag parameter */
    if (!getbooleanvalue(hfirst, 2, &flnewvalue))
        return false;

    flnextparamislast = true;

    /* Check if this is an external variable */
    if (vaddr.valuetype == externalvaluetype) {
        hv = (hdlexternalvariable) vaddr.data.externalvalue;

        /* TODO: Set the modified/dirty flag */
        /* May need to call type-specific function */
        /* Example approaches:
         * 1. Set (**hv).fldirty = flnewvalue
         * 2. Call tablesetdirty(htable, flnewvalue)
         * 3. Call opsetdirty(houtline, flnewvalue)
         */
    }

    setbooleanvalue(true, vreturned);
    return true;
}
```

**Testing**:
```bash
# Test: Clear modified flag
./frontier-cli/frontier-cli -e "system.test = 1; window.setModified(@system, false); window.isModified(@system)"
# Expected: false

# Test: Set modified flag
./frontier-cli/frontier-cli -e "window.setModified(@system, true); window.isModified(@system)"
# Expected: true
```

**Acceptance criteria**:
- [ ] Successfully sets modified flag
- [ ] Works with both true and false
- [ ] Works with tables, outlines, scripts
- [ ] Returns true on success

---

## Phase 4: Database File Operations (Mid-Level)

**Complexity**: ⭐⭐⭐⭐ Medium-High (Sonnet)
**Effort**: 1-2 days
**Files**: `tests/headless_window_verbs.c`

### Task 4.1: Research Database File Path Lookup

**Purpose**: Understand how to get the file path for a database given an address

**Research questions**:
1. How are databases tracked in the runtime?
2. Given a dbaddress, how do we find which database it belongs to?
3. How do we get the file path from a database handle?

**Files to investigate**:
- `Common/source/db.c` - Database management
- `Common/headers/db.h` - Database structures
- `Common/source/langexternal.c` - External variable database tracking

**Key structures to find**:
```c
// Example (verify in actual code)
typedef struct tydatabaserecord {
    // ... fields ...
    OSType path;  // or ptrfilespec, or similar
    // ... more fields ...
} tydatabaserecord, **hdldatabaserecord;
```

**Functions to find**:
- How to get database from address
- How to get file path from database
- Existing functions that do similar operations

**Deliverable**: `DATABASE_FILE_PATH_RESEARCH.md` with:
- Structure definitions
- Function signatures
- Code examples
- Implementation strategy

---

### Task 4.2: Implement window.getFile()

**Purpose**: Return the file path of the database containing the given address

**Prerequisites**: Complete Task 4.1 research

**Location**: `tests/headless_window_verbs.c`, case `winv_getfile`

**Implementation pattern**:
```c
case winv_getfile: {
    /* Verb: window.getFile - return database file path for address */
    hdltreenode hparam1;
    tyvaluerecord vaddr;
    hdlexternalvariable hv;
    hdldatabaserecord hdatabase = nil;
    bigstring bspath;

    /* Get address parameter */
    if (!langgetparamnode(hfirst, 1, &hparam1))
        return false;

    flnextparamislast = true;

    if (!evaluatetree(hparam1, &vaddr))
        return false;

    setemptystring(bspath);

    /* For external variables, get their associated database */
    if (vaddr.valuetype == externalvaluetype) {
        hv = (hdlexternalvariable) vaddr.data.externalvalue;

        /* TODO: Get database from external variable */
        /* IMPLEMENTATION DEPENDS ON RESEARCH FROM TASK 4.1 */

        /* Example approach (verify):
         * hdatabase = langexternalgetdatabase(hv);
         * if (hdatabase != nil) {
         *     // Get file path from database
         *     filegetpath((**hdatabase).path, bspath);
         * }
         */
    }

    return setstringvalue(bspath, vreturned);
}
```

**Key challenges**:
- External variables may or may not have an associated database
- Tables in system.compiler.files may be temporary (no file)
- Need to handle edge cases gracefully

**Testing**:
```bash
# Test: Get file for system table
./frontier-cli/frontier-cli --system-root databases/Frontier-v6-v7.root -e "window.getFile(@system)"
# Expected: "databases/Frontier-v6-v7.root" or absolute path

# Test: Get file for object in guest database
# (Create a guest database first, then test)
```

**Acceptance criteria**:
- [ ] Returns correct file path for database objects
- [ ] Returns empty string for non-database objects
- [ ] Returns full absolute path (or relative path consistently)
- [ ] Works with system root and guest databases
- [ ] No crashes on invalid addresses

**Critical importance**: User marked this as **"absolutely has to work!"**

---

## Phase 5: Menu Script Detection (Mid-Level)

**Complexity**: ⭐⭐⭐ Medium (Sonnet)
**Effort**: 1 day
**Files**: `tests/headless_window_verbs.c`

### Task 5.1: Implement window.isMenuScript()

**Purpose**: Detect if an address points to a menu script

**Background**: Menu scripts are stored as external variables with type `idmenuprocessor`

**Location**: `tests/headless_window_verbs.c`, case `winv_ismenuscript`

**Implementation**:
```c
case winv_ismenuscript: {
    /* Verb: window.isMenuScript - check if address is a menu script */
    hdltreenode hparam1;
    tyvaluerecord vaddr;
    hdlexternalvariable hv;
    boolean flismenu = false;

    /* Get address parameter */
    if (!langgetparamnode(hfirst, 1, &hparam1))
        return false;

    flnextparamislast = true;

    if (!evaluatetree(hparam1, &vaddr))
        return false;

    /* Check if this is an external variable of type menu */
    if (vaddr.valuetype == externalvaluetype) {
        hv = (hdlexternalvariable) vaddr.data.externalvalue;

        /* Check if the external type ID is idmenuprocessor */
        if ((**hv).id == idmenuprocessor) {
            flismenu = true;
        }
    }

    setbooleanvalue(flismenu, vreturned);
    return true;
}
```

**Key points**:
- `idmenuprocessor` is defined in external variable headers
- Need to include proper headers to access this constant
- Simple comparison, but must handle non-external values

**Testing**:
```bash
# Test: Non-menu object
./frontier-cli/frontier-cli -e "window.isMenuScript(@system)"
# Expected: false

# Test: Menu object (if menu support is implemented)
./frontier-cli/frontier-cli -e "new(menubarType, @testmenu); window.isMenuScript(@testmenu)"
# Expected: true
```

**Acceptance criteria**:
- [ ] Returns true for menu objects
- [ ] Returns false for non-menu objects (tables, outlines, scripts)
- [ ] Returns false for non-external values
- [ ] No crashes

---

## Phase 6: Noop Implementations (Entry-Level)

**Complexity**: ⭐ Very Low (Haiku)
**Effort**: 0.5 days
**Files**: `tests/headless_window_verbs.c`

### Task 6.1: Implement Noop Verbs

**Purpose**: Make display-only verbs return silently without errors

**These are simple - they just return success or reasonable defaults**:

```c
/* Navigation/Focus - all return true (pretend they worked) */
case winv_bringtofront:
case winv_sendtoback:
case winv_hide:
case winv_show:
case winv_update:
case winv_scroll:
case winv_zoom: {
    /* Display operations - noop in headless */
    setbooleanvalue(true, vreturned);
    return true;
}

/* Navigation/Focus - return nil/empty (no window available) */
case winv_frontmost:
case winv_next: {
    /* No windows in headless mode */
    setstringvalue(emptystring, vreturned);
    return true;
}

/* Boolean checks - return false (no windows to check) */
case winv_isfront:
case winv_isvisible:
case winv_isopen: {
    /* No windows in headless mode */
    setbooleanvalue(false, vreturned);
    return true;
}

/* Property getters - return empty/default */
case winv_getposition:
case winv_getsize: {
    /* Return empty point/rect */
    // TODO: Create empty point or rect value
    // For now, return empty string
    setstringvalue(emptystring, vreturned);
    return true;
}

/* Property setters - noop */
case winv_setposition:
case winv_setsize:
case winv_settitle: {
    /* Noop - pretend it worked */
    setbooleanvalue(true, vreturned);
    return true;
}

/* Special cases */
case winv_about:
case winv_quickscript:
case winv_runselection: {
    /* GUI-only operations - return false */
    if (bserror)
        copystring(BIGSTRING("\pNot available in headless mode"), bserror);
    return false;
}
```

**Testing**: Run each verb and ensure it doesn't crash:
```bash
./frontier-cli/frontier-cli -e "window.show(@system)"  # Should not crash
./frontier-cli/frontier-cli -e "window.hide(@system)"  # Should not crash
./frontier-cli/frontier-cli -e "window.frontmost()"    # Should return empty
```

**Acceptance criteria**:
- [ ] All noops return appropriate values
- [ ] No crashes
- [ ] No compilation warnings

---

## Testing & Verification

### Manual Testing Checklist

Create file: `tests/manual_window_verb_tests.ut`

```usertalk
// Manual test suite for window verbs

on testTargetManagement() {
    local (oldtarget = target.get());
    try {
        window.open(@system);
        assert(defined(target.get()), "window.open should set target");

        window.close();
        assert(not defined(target.get()), "window.close should clear target");

        return (true)}
    else {
        target.set(oldtarget);
        return (false)}}

on testGetTitle() {
    local (title = window.getTitle(@system));
    assert(title == "system", "getTitle should return 'system'");
    return (true)}

on testMsg() {
    window.msg("Test message");
    // Check stdout manually
    return (true)}

on testIsReadOnly() {
    // Test with known writable object
    local (fl = window.isReadOnly(@system));
    assert(fl == false, "system should not be read-only");
    return (true)}

on testIsModified() {
    // Test modification detection
    system.test = 1;
    local (fl = window.isModified(@system));
    // May be true depending on state
    return (true)}

on testGetFile() {
    local (path = window.getFile(@system));
    assert(path contains "root", "getFile should return database path");
    return (true)}

on testNoops() {
    // These should not crash
    window.show(@system);
    window.hide(@system);
    window.frontmost();
    return (true)}

// Run all tests
testTargetManagement() and testGetTitle() and testMsg() and
testIsReadOnly() and testIsModified() and testGetFile() and testNoops()
```

**Run with**:
```bash
./frontier-cli/frontier-cli --system-root databases/Frontier-v6-v7.root \
  -e "load('tests/manual_window_verb_tests.ut')"
```

---

### Automated Testing

**Create**: `tests/test_window_verbs.c` (Sonnet-level task)

```c
#include "frontier.h"
#include "standard.h"
#include "lang.h"
#include <assert.h>
#include <stdio.h>

// TODO: Full C test suite
// Requires runtime initialization
// Defer to Sonnet-level engineer familiar with test harness

int main(void) {
    printf("Window verb automated tests\n");
    printf("TODO: Implement C test suite\n");
    return 0;
}
```

---

### Regression Testing

**After each phase, run**:
```bash
# 1. Clean build
make -C frontier-cli clean && make -C frontier-cli

# 2. Full test suite
./tools/run_headless_tests.sh

# 3. Verb binding check
cd tools/kernelverbs_parser && python3 cli.py analyze

# 4. Migration test
make -C tests save_migration_tests && ./tests/save_migration_tests
```

All must pass before proceeding to next phase.

---

## Implementation Sequencing

**Week 1**:
- Day 1: Phase 1 (Target management) - Haiku
- Day 2: Phase 2 (Simple properties) - Haiku
- Day 3: Phase 2 continued + testing

**Week 2**:
- Day 1-2: Phase 3 (ODB properties) - Sonnet
- Day 3: Phase 4 (Database files) - Sonnet
- Day 4: Phase 5 (Menu script detection) - Sonnet
- Day 5: Phase 6 (Noops) + final testing - Haiku

**Daily standups**: Review progress, escalate blockers

---

## Common Pitfalls

### 1. Pascal vs C Strings

**Problem**: Frontier uses Pascal strings (length-prefixed), C uses null-terminated

**Solution**: Always use `nullterminate(bs)` before passing to C string functions

```c
// WRONG
printf("%s", bs);

// RIGHT
nullterminate(bs);
printf("%s", stringbaseaddress(bs));
```

---

### 2. Memory Management

**Problem**: Forgetting to dispose of handles

**Solution**: Use existing disposal functions, follow patterns in codebase

```c
// If you allocate, you must dispose
if (!newfilledhandle(&data, size, &h))
    return false;

// ... use h ...

disposehandle(h);
```

---

### 3. Error Handling

**Problem**: Not checking return values

**Solution**: Always check boolean returns, propagate errors

```c
// WRONG
evaluatetree(hparam1, &v);
// continue...

// RIGHT
if (!evaluatetree(hparam1, &v))
    return false;
```

---

### 4. Parameter Counting

**Problem**: Incorrect parameter count checks

**Solution**: Always mark last parameter with `flnextparamislast`

```c
// For function with 2 parameters
if (!getparam1(...))
    return false;

flnextparamislast = true;  // Mark second param as last

if (!getparam2(...))
    return false;
```

---

## Completion Checklist

Before marking implementation complete:

- [ ] All Phase 1 verbs implemented and tested
- [ ] All Phase 2 verbs implemented and tested
- [ ] All Phase 3 verbs implemented and tested
- [ ] All Phase 4 verbs implemented and tested
- [ ] All Phase 5 verbs implemented and tested
- [ ] All Phase 6 verbs implemented and tested
- [ ] Manual test suite passes
- [ ] Regression tests pass
- [ ] No compilation warnings
- [ ] Code follows existing patterns
- [ ] Comments explain non-obvious decisions
- [ ] Documentation updated

---

## Escalation Path

**Haiku-level engineer stuck?**
1. Spend max 2 hours trying to solve
2. Document what you've tried
3. Escalate to Sonnet with specific question

**Sonnet-level engineer stuck?**
1. Spend max 4 hours researching
2. Document findings
3. Escalate to user with options

**Critical blockers**:
- Contact user immediately if fundamental architecture issue discovered
- Don't spend > 1 day on any single task without escalation

---

*Implementation guide created: 2025-12-21*
*Total pages: This is a comprehensive guide - reference as needed*
*Good luck!*
