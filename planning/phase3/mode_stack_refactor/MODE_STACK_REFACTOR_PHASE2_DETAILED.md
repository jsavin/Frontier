# Mode Stack Refactor - Phase 2 Detailed Implementation Plan

**Phase**: Helper Function Refactoring - Enable deferred file conversions
**Estimated Duration**: 3-5 days (12-20 working hours)
**Created**: 2025-12-23
**Status**: Ready to execute
**Prerequisites**: Phase 1 complete (core serialization functions use explicit context)

---

## Executive Summary

**The Problem**: Phase 1 successfully refactored core serialization functions (langhash.c, tablepack.c, langexternal.c) but identified 4 files that require underlying helper functions to be refactored first before they can be converted to the context pattern.

**The Solution**: Create context-aware variants of helper functions used by the deferred files, following the established `_context` pattern. This phase focuses on bottom-up dependency resolution - refactor helper functions first, then update their callers.

**Success Criteria**:
- All helper functions have `_context` variants
- All 4 deferred files converted to use explicit context
- Zero `dbpushdatabase/dbpopdatabase` calls in converted code
- All tests pass
- Migration deterministic

---

## Phase 2 Scope

### Files Deferred from Phase 1

**High Priority** (block other functionality):
1. **menuverbs.c** (`menuverbinmemory`) - Blocks menu external materialization
2. **langxml.c** (`decompilespecialtable`) - Used by XML compilation
3. **langhtml.c** (`additemtopagetable`) - Used by HTML rendering

**Lower Priority** (diagnostic/utility only):
4. **dbstats.c** (`dbstatsmessage`) - Database statistics window (rarely used)

### Helper Functions Required

**Primary Helpers** (required by multiple files):
1. `copyvaluerecord_context` - Used by langxml.c, langhtml.c (CRITICAL)

**Menu Helpers** (required by menuverbs.c):
2. `meloadmenurecord_context` - Menu record loading
3. `meloadoutline_context` - Outline loading for menus

**Database Utility Helpers** (required by dbstats.c):
4. `dbreadavailnode` - Already context-aware (uses `db_use64()`)
5. `dbgeteof` - Already context-aware (uses global database)
6. `dbreadheader` - Already context-aware (uses `db_use64()`)
7. `dbreadtrailer` - Already context-aware (uses `db_use64()`)

---

## Dependency Analysis

### Dependency Graph

```
copyvaluerecord
  ├─> Used by: langxml.c (decompilespecialtable)
  ├─> Used by: langhtml.c (additemtopagetable)
  └─> Dependencies: dbrefhandle (already has _context variant)

meloadmenurecord
  ├─> Used by: menuverbs.c (menuverbinmemory)
  ├─> Calls: dbreference (no _context variant needed - uses global db)
  └─> Calls: meloadoutline (needs _context variant)

meloadoutline
  ├─> Used by: meloadmenurecord
  ├─> Calls: dbrefhandle (already has _context variant)
  └─> Calls: opunpack (check if needs context)

dbstats.c helpers
  ├─> dbreadavailnode - uses db_use64() already
  ├─> dbgeteof - uses global databasedata
  ├─> dbreadheader - uses db_use64() already
  └─> dbreadtrailer - uses db_use64() already
```

### Critical Finding: Most helpers are already context-aware!

The db.c functions (`dbreadheader`, `dbreadtrailer`, `dbreadavailnode`, `dbgeteof`) already use `db_use64()` which checks global mode. They don't need context variants - `dbstats.c` can be converted by simply replacing push/pop with context application.

---

## Implementation Order (Bottom-Up)

### Phase 2.1: copyvaluerecord_context (CRITICAL PATH)
**Duration**: 2-3 hours
**Priority**: HIGH - Unblocks langxml.c and langhtml.c
**Complexity**: MEDIUM - Handles fldiskval reads

### Phase 2.2: langxml.c and langhtml.c Conversion
**Duration**: 2-3 hours
**Priority**: HIGH - Direct dependencies on 2.1
**Complexity**: SIMPLE - Only 2 push/pop sites total

### Phase 2.3: meloadoutline_context
**Duration**: 2-3 hours
**Priority**: MEDIUM - Enables menu external materialization
**Complexity**: MEDIUM - Calls dbrefhandle

### Phase 2.4: meloadmenurecord_context
**Duration**: 1-2 hours
**Priority**: MEDIUM - Depends on 2.3
**Complexity**: SIMPLE - Thin wrapper around meloadoutline

### Phase 2.5: menuverbs.c Conversion
**Duration**: 1 hour
**Priority**: MEDIUM - Depends on 2.4
**Complexity**: SIMPLE - Single push/pop site

### Phase 2.6: dbstats.c Refactoring (OPTIONAL)
**Duration**: 1-2 hours
**Priority**: LOW - Diagnostic tool, rarely used
**Complexity**: SIMPLE - Direct context application

---

## Phase 2.1: copyvaluerecord_context (CRITICAL)

### Current Implementation Analysis

**File**: `Common/source/langvalue.c`
**Current Signature**: `boolean copyvaluerecord(tyvaluerecord v, tyvaluerecord *vreturned)`
**Call Sites**: 50+ across entire codebase
**Database Operations**: YES - calls `dbrefhandle()` when `v.fldiskval == true`

### Why Context is Needed

At line 918, `copyvaluerecord` reads disk-based scalars:
```c
if (v.fldiskval) {
    if (!dbrefhandle(v.data.diskvalue, &x))  // ← Database read!
        return (false);
}
```

During migration, if we're copying a value from a v6 source table, we need to read it with v6 format context.

### Implementation Plan

#### Step 2.1.1: Create copyvaluerecord_internal

**File**: `Common/source/langvalue.c`

**Location**: Insert new function before line 842 (before existing `copyvaluerecord`)

**New Function**:
```c
boolean copyvaluerecord_internal(const db_context *ctx, tyvaluerecord v,
                                  tyvaluerecord *vreturned) {
    /*
    Create a copy of value record with explicit database context.

    For disk-based scalars (fldiskval=1), reads from database using ctx.
    For in-memory values, behaves identically to copyvaluerecord().

    Preconditions:
      - ctx specifies database and format mode for disk reads
      - v is a valid value record

    Postconditions:
      - *vreturned contains deep copy of v
      - Disk values are loaded into memory in vreturned
      - Returns true on success, false on failure
    */

    Handle x;
    hdllistrecord hlist;

    if (!ctx) {
        db_context default_ctx;
        db_context_init(&default_ctx);
        return copyvaluerecord_internal(&default_ctx, v, vreturned);
    }

    switch (v.valuetype) {

        case addressvaluetype:
        case stringvaluetype:
        case passwordvaluetype:
        case rectvaluetype:
        case patternvaluetype:
        case rgbvaluetype:
        case objspecvaluetype:
        case filespecvaluetype:
        case aliasvaluetype:
        case doublevaluetype:
        case binaryvaluetype:
            initvalue(vreturned, novaluetype);

            if (v.fldiskval) {
                /*
                4.0.2b1 dmb: for disk-based scalars, the copy will be the actual
                data, while the original value (and the hashtable node) will still
                be on disk

                PHASE 2 CHANGE: Use context-aware dbrefhandle
                */

                if (!dbrefhandle_context(ctx, v.data.diskvalue, &x))
                    return (false);
            }
            else {
                if (!copyhandle(v.data.binaryvalue, &x))
                    return (false);
            }

            return setheapvalue(x, v.valuetype, vreturned);

        case listvaluetype:
        case recordvaluetype:
            initvalue(vreturned, v.valuetype);

            if (!opcopylist(v.data.listvalue, &hlist))
                return (false);

            return setheapvalue((Handle) hlist, v.valuetype, vreturned);

        case codevaluetype:
        case externalvaluetype:
            *vreturned = v;

            (*vreturned).fltmpdata = true; /*see hashassign, disposevaluerecord*/

            break;

        default:
            *vreturned = v;

            break;
    } /*switch*/

    return (true);
}
```

#### Step 2.1.2: Update Existing copyvaluerecord Wrapper

**File**: `Common/source/langvalue.c`

**Change at line 842**:
```c
boolean copyvaluerecord(tyvaluerecord v, tyvaluerecord *vreturned) {
    db_context ctx;
    db_context_init(&ctx);
    return copyvaluerecord_internal(&ctx, v, vreturned);
}
```

#### Step 2.1.3: Add Declaration to Header

**File**: `Common/headers/langvalue.h`

**Add**:
```c
/* Context-aware value copying */
extern boolean copyvaluerecord_internal(const db_context *ctx, tyvaluerecord v,
                                         tyvaluerecord *vreturned);
```

### Testing

```bash
# 1. Compile
make -C tests clean && make -C tests save_migration_tests
# Should compile without errors

# 2. Run migration (determinism check)
for i in {1..3}; do
  rm -f tests/test_save_migration-v7.root
  ./tests/save_migration_tests
  cp tests/test_save_migration-v7.root /tmp/step2.1-run$i.root
done

# 3. Verify identical
md5 /tmp/step2.1-run*.root
# All 3 should be IDENTICAL

# 4. Full test suite
./tools/run_headless_tests.sh
# All tests should pass

# 5. Verify no mode stack in new function
grep -A 100 "^boolean copyvaluerecord_internal" Common/source/langvalue.c | \
  grep "db_format_mode_push\|db_format_mode_pop"
# Should return NOTHING
```

### Success Criteria

- ✅ Compiles without errors
- ✅ Migration deterministic
- ✅ All tests pass
- ✅ No mode stack in `copyvaluerecord_internal`
- ✅ Existing `copyvaluerecord` calls still work (backward compatibility)

### Commit Message

```bash
git add Common/source/langvalue.c Common/headers/langvalue.h
git commit -m "refactor(langvalue): Add copyvaluerecord_internal with explicit context

Created context-aware variant of copyvaluerecord to support explicit format
control when copying disk-based scalar values during migration.

Changes:
- Added copyvaluerecord_internal(const db_context *ctx, ...) variant
- Updated copyvaluerecord() to delegate to _internal with default context
- Uses dbrefhandle_context() for disk value reads

This unblocks langxml.c and langhtml.c Phase 2 refactoring.

Testing: Migration deterministic, all tests pass.

Part of Phase 2: Helper Function Refactoring (Step 2.1)
Ref: planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PHASE2_DETAILED.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

---

## Phase 2.2: langxml.c and langhtml.c Conversion

### langxml.c Analysis

**File**: `Common/source/langxml.c`
**Function**: `decompilespecialtable`
**Push/Pop Location**: Lines 2479, 2484
**Usage**: Wrapping `copyvaluerecord` call

**Current Code**:
```c
if (hdb)
    dbpushdatabase(hdb);

fl = copyvaluerecord((**hn).val, &attvalue) && coercetostring(&attvalue);

if (hdb)
    dbpopdatabase();
```

### langhtml.c Analysis

**File**: `Common/source/langhtml.c`
**Function**: `additemtopagetable`
**Push/Pop Location**: Lines 2887, 2892
**Usage**: Wrapping `copyvaluerecord` call

**Current Code**:
```c
if (hdb)
    dbpushdatabase(hdb);

fl = copyvaluerecord(val, &val);

if (hdb)
    dbpopdatabase();
```

### Implementation Plan

#### Step 2.2.1: Refactor langxml.c

**File**: `Common/source/langxml.c`

**Replace lines 2478-2485**:
```c
// BEFORE:
if (hdb)
    dbpushdatabase(hdb);

fl = copyvaluerecord((**hn).val, &attvalue) && coercetostring(&attvalue);

if (hdb)
    dbpopdatabase();

// AFTER:
if (hdb) {
    db_context ctx;
    db_context_init(&ctx);
    ctx.database = hdb;
    fl = copyvaluerecord_internal(&ctx, (**hn).val, &attvalue) &&
         coercetostring(&attvalue);
}
else {
    fl = copyvaluerecord((**hn).val, &attvalue) && coercetostring(&attvalue);
}
```

#### Step 2.2.2: Refactor langhtml.c

**File**: `Common/source/langhtml.c`

**Replace lines 2886-2893**:
```c
// BEFORE:
if (hdb)
    dbpushdatabase(hdb);

fl = copyvaluerecord(val, &val);

if (hdb)
    dbpopdatabase();

// AFTER:
if (hdb) {
    db_context ctx;
    db_context_init(&ctx);
    ctx.database = hdb;
    fl = copyvaluerecord_internal(&ctx, val, &val);
}
else {
    fl = copyvaluerecord(val, &val);
}
```

### Testing

```bash
# 1. Compile
make -C tests clean && make -C tests save_migration_tests

# 2. Migration determinism
for i in {1..3}; do
  rm -f tests/test_save_migration-v7.root
  ./tests/save_migration_tests
  cp tests/test_save_migration-v7.root /tmp/step2.2-run$i.root
done
md5 /tmp/step2.2-run*.root

# 3. Full test suite
./tools/run_headless_tests.sh

# 4. Verify no push/pop in modified files
grep -c "dbpushdatabase\|dbpopdatabase" Common/source/langxml.c Common/source/langhtml.c
# Should be 0
```

### Success Criteria

- ✅ Compiles without errors
- ✅ Migration deterministic
- ✅ All tests pass
- ✅ Zero push/pop calls in langxml.c and langhtml.c

### Commit Message

```bash
git add Common/source/langxml.c Common/source/langhtml.c
git commit -m "refactor(langxml,langhtml): Remove dbpushdatabase pattern using copyvaluerecord_internal

Replaced dbpushdatabase/dbpopdatabase wrappers with explicit context-based
copyvaluerecord_internal calls in XML and HTML compilation functions.

Changes:
- langxml.c: decompilespecialtable uses copyvaluerecord_internal
- langhtml.c: additemtopagetable uses copyvaluerecord_internal
- Both functions now create explicit db_context when external database present

Result: Zero dbpushdatabase calls remaining in these files.

Testing: Migration deterministic, all tests pass.

Part of Phase 2: Helper Function Refactoring (Step 2.2)
Ref: planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PHASE2_DETAILED.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

---

## Phase 2.3: meloadoutline_context

### Current Implementation Analysis

**File**: `Common/source/menueditor.c`
**Function**: `meloadoutline`
**Current Signature**: `boolean meloadoutline(dbaddress adr, hdloutlinerecord *houtline)`
**Call Sites**: 2 (menupack.c, meloadmenurecord)
**Database Operations**: YES - calls `dbrefhandle()` at line 429

### Implementation Plan

#### Step 2.3.1: Create meloadoutline_internal

**File**: `Common/source/menueditor.c`

**Insert before line 402**:
```c
boolean meloadoutline_internal(const db_context *ctx, dbaddress adr,
                                 hdloutlinerecord *houtline) {
    /*
    Load outline with explicit database context.

    Preconditions:
      - ctx specifies database and format mode
      - adr is valid outline address or nildbaddress

    Postconditions:
      - *houtline contains loaded outline structure
      - Returns true on success, false on failure
    */

    register boolean fl;
    register hdloutlinerecord ho;
    Handle hpackedoutline;
    Rect r;
    long ixload = 0;

    if (!ctx) {
        db_context default_ctx;
        db_context_init(&default_ctx);
        return meloadoutline_internal(&default_ctx, adr, houtline);
    }

    *houtline = nil; /*default return*/

    oppushoutline(nil); /*preserve global*/

    if (adr == nildbaddress) { /*new structure is called for*/

        megetoutlinerect(&r);

        fl = opnewrecord(r, houtline);
    }
    else {
        fl = dbrefhandle_context(ctx, adr, &hpackedoutline);

        if (fl) {

            fl = opunpack(hpackedoutline, &ixload, houtline);

            disposehandle(hpackedoutline);
        }
    }

    ho = *houtline;

    oppopoutline(); /*restore global*/

    if (!fl)
        return (false);

    opvalidate(ho);

    (**ho).setscrollbarsroutine = &mesetscrollbarsroutine;

    (**ho).drawlinecallback = &medrawlineroutine;

    meclearhandles(ho);

    *houtline = ho;

    return (fl);
}
```

#### Step 2.3.2: Update Existing meloadoutline Wrapper

**File**: `Common/source/menueditor.c`

**Replace line 402**:
```c
boolean meloadoutline(dbaddress adr, hdloutlinerecord *houtline) {
    db_context ctx;
    db_context_init(&ctx);
    return meloadoutline_internal(&ctx, adr, houtline);
}
```

#### Step 2.3.3: Add Declaration to Header

**File**: `Common/headers/menueditor.h`

**Add**:
```c
extern boolean meloadoutline_internal(const db_context *ctx, dbaddress adr,
                                       hdloutlinerecord *houtline);
```

### Testing

```bash
# Standard testing procedure
make -C tests clean && make -C tests save_migration_tests

for i in {1..3}; do
  rm -f tests/test_save_migration-v7.root
  ./tests/save_migration_tests
  cp tests/test_save_migration-v7.root /tmp/step2.3-run$i.root
done

md5 /tmp/step2.3-run*.root
./tools/run_headless_tests.sh

# Verify no mode stack
grep -A 50 "^boolean meloadoutline_internal" Common/source/menueditor.c | \
  grep "db_format_mode_push\|db_format_mode_pop"
```

### Success Criteria

- ✅ Compiles without errors
- ✅ Migration deterministic
- ✅ All tests pass
- ✅ No mode stack in `meloadoutline_internal`

### Commit Message

```bash
git add Common/source/menueditor.c Common/headers/menueditor.h
git commit -m "refactor(menueditor): Add meloadoutline_internal with explicit context

Created context-aware variant of meloadoutline to support explicit format
control when loading menu outline structures from database.

Changes:
- Added meloadoutline_internal(const db_context *ctx, ...) variant
- Updated meloadoutline() to delegate to _internal with default context
- Uses dbrefhandle_context() for outline data reads

This enables meloadmenurecord_context and unblocks menuverbs.c refactoring.

Testing: Migration deterministic, all tests pass.

Part of Phase 2: Helper Function Refactoring (Step 2.3)
Ref: planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PHASE2_DETAILED.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

---

## Phase 2.4: meloadmenurecord_context

### Current Implementation Analysis

**File**: `Common/source/menupack.c`
**Function**: `meloadmenurecord`
**Current Signature**: `boolean meloadmenurecord(dbaddress adr, hdlmenurecord *hmenurecord)`
**Call Sites**: 1 (menuverbs.c)
**Database Operations**: YES - calls `dbreference()` and `meloadoutline()`

### Implementation Plan

#### Step 2.4.1: Create meloadmenurecord_internal

**File**: `Common/source/menupack.c`

**Insert before line 868**:
```c
boolean meloadmenurecord_internal(const db_context *ctx, dbaddress adr,
                                    hdlmenurecord *hmenurecord) {
    /*
    Load menu record with explicit database context.

    Preconditions:
      - ctx specifies database and format mode
      - adr is valid menu record address

    Postconditions:
      - *hmenurecord contains loaded menu structure
      - Returns true on success, false on failure
    */

    hdloutlinerecord houtline;
    tysavedmenuinfo info;

    if (!ctx) {
        db_context default_ctx;
        db_context_init(&default_ctx);
        return meloadmenurecord_internal(&default_ctx, adr, hmenurecord);
    }

    if (!dbreference(adr, sizeof(info), &info))
        return (false);

    if (!meloadoutline_internal(ctx, conditionallongswap(info.adroutline), &houtline))
        return (false);

    if (!mesetupmenurecord(&info, houtline, hmenurecord)) {

        opdisposeoutline(houtline, false);

        return (false);
    }

    return (true);
}
```

#### Step 2.4.2: Update Existing meloadmenurecord Wrapper

**File**: `Common/source/menupack.c`

**Replace line 868**:
```c
boolean meloadmenurecord(dbaddress adr, hdlmenurecord *hmenurecord) {
    db_context ctx;
    db_context_init(&ctx);
    return meloadmenurecord_internal(&ctx, adr, hmenurecord);
}
```

#### Step 2.4.3: Add Declaration to Header

**File**: `Common/headers/menupack.h`

**Add**:
```c
extern boolean meloadmenurecord_internal(const db_context *ctx, dbaddress adr,
                                          hdlmenurecord *hmenurecord);
```

### Testing

Same as previous steps.

### Commit Message

```bash
git add Common/source/menupack.c Common/headers/menupack.h
git commit -m "refactor(menupack): Add meloadmenurecord_internal with explicit context

Created context-aware variant of meloadmenurecord to support explicit format
control when loading menu records from database.

Changes:
- Added meloadmenurecord_internal(const db_context *ctx, ...) variant
- Updated meloadmenurecord() to delegate to _internal with default context
- Uses meloadoutline_internal() for outline loading

This directly enables menuverbs.c refactoring.

Testing: Migration deterministic, all tests pass.

Part of Phase 2: Helper Function Refactoring (Step 2.4)
Ref: planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PHASE2_DETAILED.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

---

## Phase 2.5: menuverbs.c Conversion

### Current Implementation Analysis

**File**: `Common/source/menuverbs.c`
**Function**: `menuverbinmemory`
**Push/Pop Location**: Lines 184, 190
**Usage**: Single push/pop wrapping `meloadmenurecord` call

### Implementation Plan

#### Step 2.5.1: Refactor menuverbinmemory

**File**: `Common/source/menuverbs.c`

**Replace lines 160-204**:
```c
static boolean menuverbinmemory(hdlmenuvariable hvariable) {

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
    ctx.database = (**hv).hdatabase;

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

### Testing

```bash
# Standard testing
make -C tests clean && make -C tests save_migration_tests

for i in {1..3}; do
  rm -f tests/test_save_migration-v7.root
  ./tests/save_migration_tests
  cp tests/test_save_migration-v7.root /tmp/step2.5-run$i.root
done

md5 /tmp/step2.5-run*.root
./tools/run_headless_tests.sh

# Verify no push/pop in menuverbs.c
grep -c "dbpushdatabase\|dbpopdatabase" Common/source/menuverbs.c
# Should be 0
```

### Success Criteria

- ✅ Compiles without errors
- ✅ Migration deterministic
- ✅ All tests pass
- ✅ Zero push/pop calls in menuverbs.c

### Commit Message

```bash
git add Common/source/menuverbs.c
git commit -m "refactor(menuverbs): Remove dbpushdatabase pattern using meloadmenurecord_internal

Replaced dbpushdatabase/dbpopdatabase wrapper with explicit context-based
meloadmenurecord_internal call in menuverbinmemory.

Changes:
- menuverbinmemory now creates explicit db_context for menu loading
- Uses meloadmenurecord_internal instead of meloadmenurecord
- Removed TODO comment (Phase 2 complete for this file)

Result: Zero dbpushdatabase calls in menuverbs.c.

Testing: Migration deterministic, all tests pass.

Part of Phase 2: Helper Function Refactoring (Step 2.5)
Ref: planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PHASE2_DETAILED.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

---

## Phase 2.6: dbstats.c Refactoring (OPTIONAL)

### Analysis

**File**: `Common/source/dbstats.c`
**Function**: `dbstatsmessage`
**Push/Pop Location**: Lines 689, 694
**Complexity**: SIMPLE - The push/pop is just setting active database

**Key Finding**: The db.c helper functions (`dbreadheader`, `dbreadtrailer`, `dbgeteof`, `dbreadavailnode`) already use `db_use64()` and check global `databasedata`. They don't need context variants.

### Implementation Plan

#### Step 2.6.1: Direct Refactor (No Helper Functions Needed)

**File**: `Common/source/dbstats.c`

**Replace lines 673-699**:
```c
// BEFORE:
boolean dbstatsmessage(hdldatabaserecord hdb, boolean flincludeusedblocks) {

    if (!statsfindwindow()) {

        if (!statsnewwindow())
            return (false);
    }

    if (optionkeydown())
        windowbringtofront(statswindow);

    shellpushglobals(statswindow);

    smashrect((**statswindowinfo).contentrect);

    if (hdb != nil)
        dbpushdatabase(hdb);

    statscompute(flincludeusedblocks);

    if (hdb != nil)
        dbpopdatabase();

    shellpopglobals();

    return (true);
}

// AFTER:
boolean dbstatsmessage(hdldatabaserecord hdb, boolean flincludeusedblocks) {

    if (!statsfindwindow()) {

        if (!statsnewwindow())
            return (false);
    }

    if (optionkeydown())
        windowbringtofront(statswindow);

    shellpushglobals(statswindow);

    smashrect((**statswindowinfo).contentrect);

    if (hdb != nil) {
        db_context ctx;
        db_context_init(&ctx);
        ctx.database = hdb;
        db_context_apply(&ctx);
    }

    statscompute(flincludeusedblocks);

    if (hdb != nil) {
        db_context default_ctx;
        db_context_init(&default_ctx);
        db_context_apply(&default_ctx);
    }

    shellpopglobals();

    return (true);
}
```

### Testing

Same as previous steps.

### Success Criteria

- ✅ Compiles without errors
- ✅ Database stats window still works
- ✅ Zero push/pop calls in dbstats.c

### Commit Message

```bash
git add Common/source/dbstats.c
git commit -m "refactor(dbstats): Replace dbpushdatabase with db_context_apply

Replaced dbpushdatabase/dbpopdatabase pattern with explicit db_context_apply
in database statistics computation.

Changes:
- dbstatsmessage now uses db_context_apply to set active database
- Restores default context after stats computation
- No helper function changes needed (db.c functions already context-aware)

Result: Zero dbpushdatabase calls in dbstats.c.

Testing: Stats window functional, all tests pass.

Part of Phase 2: Helper Function Refactoring (Step 2.6)
Ref: planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PHASE2_DETAILED.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

---

## Phase 2 Final Validation

### Duration: 1 hour

### Validation Steps

#### 1. Migration Determinism (5 runs)

```bash
# Clean rebuild
make -C tests clean
rm -f tests/test_save_migration-v7.root

# Run migration 5 times
for i in {1..5}; do
  rm -f tests/test_save_migration-v7.root
  make -C tests save_migration_tests && ./tests/save_migration_tests
  if [ $? -ne 0 ]; then
    echo "ERROR: Migration failed on run $i"
    exit 1
  fi
  cp tests/test_save_migration-v7.root /tmp/phase2-migration-run-$i.root
done

# Verify all identical
echo "=== Phase 2 Determinism Validation ==="
md5 /tmp/phase2-migration-run-*.root
# All 5 should be IDENTICAL
```

#### 2. Push/Pop Elimination Verification

```bash
echo "=== Phase 2 Push/Pop Elimination ==="

# Check deferred files
for file in menuverbs.c langxml.c langhtml.c dbstats.c; do
  count=$(grep -c "dbpushdatabase\|dbpopdatabase" Common/source/$file)
  if [ $count -gt 0 ]; then
    echo "❌ FAIL: $file still has $count push/pop calls"
    exit 1
  else
    echo "✅ PASS: $file - zero push/pop calls"
  fi
done

# Count remaining push/pop calls across entire codebase
total=$(grep -r "dbpushdatabase\|dbpopdatabase" Common/source/*.c | wc -l)
echo ""
echo "Total remaining dbpushdatabase calls: $total"
echo "(Should be significantly less than Phase 1 baseline of 22)"
```

#### 3. Helper Function Context Verification

```bash
echo "=== Phase 2 Helper Function Verification ==="

# Verify new _internal functions don't use mode stack
for func in copyvaluerecord_internal meloadoutline_internal meloadmenurecord_internal; do
  file=$(grep -l "^boolean $func" Common/source/*.c)

  if grep -A 50 "^boolean $func" $file | grep -q "db_format_mode_push\|db_format_mode_pop"; then
    echo "❌ FAIL: $func still uses mode stack!"
    exit 1
  else
    echo "✅ PASS: $func - clean (no mode stack)"
  fi
done
```

#### 4. Database Access Validation

```bash
echo "=== Database Access Validation ==="

# Test migrated database
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "defined(system)"
echo "System table: OK"

FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "sizeOf(system.verbs.colors) > 0"
echo "External table access: OK"
```

#### 5. Full Test Suite

```bash
echo "=== Full Test Suite ==="
./tools/run_headless_tests.sh

if [ $? -eq 0 ]; then
  echo "✅ All tests pass"
else
  echo "❌ Some tests failed"
  exit 1
fi
```

### Phase 2 Completion Checklist

- [ ] Migration is deterministic (5 runs = identical md5)
- [ ] Zero dbpushdatabase calls in menuverbs.c
- [ ] Zero dbpushdatabase calls in langxml.c
- [ ] Zero dbpushdatabase calls in langhtml.c
- [ ] Zero dbpushdatabase calls in dbstats.c
- [ ] All helper functions have `_internal` variants
- [ ] No mode stack in `_internal` functions
- [ ] All tests pass
- [ ] External tables accessible
- [ ] Total push/pop count significantly reduced

### If All Checks Pass

```bash
# Create milestone commit
git commit --allow-empty -m "milestone: Complete Phase 2 - Helper Function Refactoring

Phase 2 complete: All deferred files from Phase 1 now use explicit context.

Accomplishments:
- Created copyvaluerecord_internal (Step 2.1)
- Refactored langxml.c and langhtml.c (Step 2.2)
- Created meloadoutline_internal (Step 2.3)
- Created meloadmenurecord_internal (Step 2.4)
- Refactored menuverbs.c (Step 2.5)
- Refactored dbstats.c (Step 2.6 - optional)

Results:
- 4 deferred files now use explicit context
- Migration deterministic (5 runs identical)
- All tests pass
- Total dbpushdatabase calls reduced from 22 to ~18

Next: Continue eliminating remaining push/pop calls across codebase

Ref: planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PHASE2_DETAILED.md

🤖 Generated with Claude Code

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"

# Create safety backup
git branch phase2-complete
git tag phase2-complete-$(date +%Y%m%d)

echo "✅ Phase 2 COMPLETE - Ready for Phase 3"
```

---

## Emergency Rollback Procedures

### If Any Step Fails

```bash
# Option 1: Revert last commit
git revert HEAD
make -C tests clean && ./tools/run_headless_tests.sh

# Option 2: Revert to last successful step
git reset --hard <previous-step-commit-hash>

# Option 3: Start Phase 2 over
git reset --hard phase1-complete
# Re-run from Step 2.1
```

### If Phase 2 Completes But Breaks Something

```bash
# Created backup at end of Phase 2
git reset --hard phase2-complete

# Can restart Phase 3 planning fresh
```

---

## Risk Assessment

### Low Risk Items

1. **copyvaluerecord_internal** - Only changes disk value reading path, in-memory path unchanged
2. **langxml.c, langhtml.c** - Simple push/pop replacements, single call sites
3. **meloadmenurecord** - Thin wrapper, minimal logic

### Medium Risk Items

1. **meloadoutline_internal** - Called during menu loading, affects menu externals
2. **menuverbs.c** - Menu external materialization (but similar to picture/outline patterns)

### Low Priority (Can Defer)

1. **dbstats.c** - Diagnostic tool, rarely used, not critical path

### Mitigation Strategies

1. **Test After Every Step** - Don't proceed if tests fail
2. **Maintain Determinism** - Migration must produce identical output on repeated runs
3. **Preserve Backward Compatibility** - All existing call sites continue to work via wrappers
4. **Commit Atomically** - Each step is independently revertable

---

## Notes for Autonomous Execution

### Pattern Consistency

All refactorings follow this pattern:
1. Create `_internal(const db_context *ctx, ...)` variant
2. Add NULL context check that creates default and recurses
3. Use `dbrefhandle_context(ctx, ...)` instead of `dbrefhandle()`
4. Update wrapper to delegate to `_internal` with default context
5. Add declaration to header file

### Testing Requirements

After EVERY step:
- Compile must succeed
- Migration must be deterministic (same md5 on repeated runs)
- Full test suite must pass
- Verify no mode stack in new `_internal` functions

### Decision Points

**If copyvaluerecord has too many call sites to audit**:
- Don't worry - wrapper maintains backward compatibility
- Only update call sites that explicitly need context (langxml.c, langhtml.c)
- Other call sites can continue using wrapper

**If dbstats.c refactoring seems risky**:
- DEFER IT - mark as Phase 3
- It's a diagnostic tool, not critical path
- Focus on menu externals first

**If tests fail at any step**:
- STOP IMMEDIATELY
- Don't proceed to next step
- Revert and investigate
- Check migration determinism first

---

## Success Metrics

### Completion Criteria

Phase 2 is complete when:
- [ ] All 4 deferred files converted to explicit context
- [ ] All helper functions have `_internal` variants
- [ ] Zero mode stack in `_internal` functions
- [ ] Migration deterministic
- [ ] All tests pass
- [ ] External tables accessible
- [ ] Total push/pop count reduced by ~4 call sites

### Performance

- Migration time should remain within ±5% of Phase 1 baseline
- No memory leaks (check with valgrind if available)
- Database loads correctly after migration

---

## Related Documentation

### Architectural Decision Records
- **ADR-002-context-based-format-versioning.md** - Context pattern rationale and implementation
- **mode_management_single_decision_point.md** - Single Decision Point Principle

### Planning Documents
- **MODE_STACK_REFACTOR_PROGRESS.md** - Current progress tracking
- **MODE_STACK_REFACTOR_PHASE1_DETAILED_v2.md** - Phase 1 reference

### Code References
- Phase 1 pattern: See pictverbs.c, wptext_runtime.c for similar refactorings
- Context initialization: See db_format.c for `db_context_init*` functions

---

## Appendix: Helper Function Summary

### copyvaluerecord_internal
- **Complexity**: MEDIUM
- **Call Sites**: 50+ (but wrapper maintains compatibility)
- **Database Ops**: YES (dbrefhandle for fldiskval)
- **Dependencies**: dbrefhandle_context (already exists)
- **Estimated Effort**: 2-3 hours

### meloadoutline_internal
- **Complexity**: MEDIUM
- **Call Sites**: 2
- **Database Ops**: YES (dbrefhandle)
- **Dependencies**: dbrefhandle_context (already exists)
- **Estimated Effort**: 2-3 hours

### meloadmenurecord_internal
- **Complexity**: SIMPLE
- **Call Sites**: 1
- **Database Ops**: YES (dbreference + meloadoutline)
- **Dependencies**: meloadoutline_internal
- **Estimated Effort**: 1-2 hours

### dbstats.c helpers
- **Complexity**: SIMPLE (no refactoring needed)
- **Call Sites**: N/A
- **Database Ops**: Read-only (already context-aware via db_use64())
- **Dependencies**: None
- **Estimated Effort**: 1 hour for dbstats.c refactor only

---

**End of Phase 2 Detailed Plan**
**Status**: Production-ready for autonomous execution by Claude Sonnet
**Created**: 2025-12-23
**Last Updated**: 2025-12-23
