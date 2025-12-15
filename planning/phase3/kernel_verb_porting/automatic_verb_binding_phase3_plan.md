# Phase 3: Kernel Verb Implementation - Detailed Plan

**Status:** Planning (Ready for user approval)
**Scope:** Implement stub verbs for headless execution based on Phase 2.C categorization strategy
**Priority:** Core headless infrastructure (24 error stubs + 3 noops)

---

## Executive Summary

**What:** Implement 24 error stubs and 3 noop stubs for verbs that can't run in headless mode

**Why:** Enable legacy UserTalk scripts to get clear error messages instead of crashes, and allow non-critical operations to succeed silently

**How:** Update 8 existing `tests/headless_*.c` files with proper error handling following CLAUDE.md conventions

**Verification:** All error messages must follow "Can't do X because Y" format, UserTalk-level tests verify propagation

---

## Overview

Phase 3 implements the 5-category stub strategy defined in Phase 2.C. Work focuses on headless-compatible verbs first, deferring optional databases (MySQL/SQLite) to future work.

**Key Metrics:**
- 24 verbs requiring "not implemented" error stubs (Category 1 core)
- 3 verbs requiring noop stubs (Category 2)
- 2 verbs requiring delegation OR error stubs - TBD (Category 3 - needs research)
- 2 verbs keeping hybrid implementations (Category 4)
- 38 verbs deferred (MySQL/SQLite - Category 1 optional databases)

---

## Implementation Architecture

### File Locations & Organization

**Primary Implementation Files:**
1. `Common/source/kernel_verbs_headless.c` (lines 1-2074)
   - Core registration system for headless kernel verbs
   - Contains 26 `init_efp_XXXX()` functions (one per processor group)
   - Uses custom verb callbacks instead of `langfunctionvalue`

2. `tests/headless_*.c` (50+ generated files)
   - Auto-generated stub files for each processor
   - Pattern: `headless_<processor>_verbs.c`
   - Structure: Enum (verb tokens) + switch/case dispatcher function + registration

3. `tools/kernelverbs_parser/verb_exceptions.py` (lines 11-192)
   - Exception tables for verb name → C enum token mapping
   - Pattern C: Irregular naming (e.g., `file.close` → `closefilefunc`)
   - Pattern D: Multi-processor consolidation (dialog, clock, date in `langverbs.c`)

4. `planning/phase3/kernel_verb_porting/automatic_verb_binding_phase2_plan.md`
   - Phase 2.C strategy documented
   - Phase 3 success criteria

**Test Infrastructure:**
- `tests/Makefile` (lines 160-161, 261-262) - Builds headless verb test suite
- `tools/run_headless_tests.sh` - Standard test execution flow
- `tests/headless_*.c` - Test implementations for each processor

---

## Implementation Strategy by Category

### Category 1: Error Stub Implementation (28 core + 38 deferred)

**Verbs Requiring "Not Implemented" Errors:**

**1a. OSA/AppleScript Verbs (8 verbs) - lang processor**
- `lang.DDEevent` - Mac-only Apple event
- `lang.callxcmd` - Mac Classic XCMD interface
- `lang.countapplelistitems` - Apple list ops
- `lang.getapplelistitem` - Apple list access
- `lang.putapplelistitem` - Apple list modification
- `lang.geteventattribute` - Apple event attrs
- `lang.seteventinteraction` - Apple event control
- `lang.transactionEvent` - Apple transaction

**Implementation Pattern (following CLAUDE.md error format):**
```c
case lang_ddeev_token:
    if (bserror)
        copystring(BIGSTRING("\pCan't call OSA verbs because AppleScript is not available in headless mode"), bserror);
    return false;
```

**1b. GUI-Only Dialog Verbs (5 verbs) - dialog processor**
- `dialog.hideitem`
- `dialog.ismodalcard`
- `dialog.runcard`
- `dialog.setmodalcardtimeout`
- `dialog.showitem`

**Implementation Pattern (following CLAUDE.md error format):**
```c
case dialog_hideitem_token:
    if (bserror)
        copystring(BIGSTRING("\pCan't use modal dialog verbs because GUI is not available in headless mode"), bserror);
    return false;
```

**1c. Status Bar Verbs (5 verbs) - statusbar processor**
- `statusbar.msg`
- `statusbar.getmessage`
- `statusbar.getsectionone`
- `statusbar.getsections`
- `statusbar.setsections`

**Implementation Pattern (following CLAUDE.md error format):**
```c
case statusbar_msg_token:
    if (bserror)
        copystring(BIGSTRING("\pCan't use status bar verbs because GUI is not available in headless mode"), bserror);
    return false;
```

**1d. Window Verbs (3 verbs) - window processor**
- `window.dbstats` - Database stats visualization
- `window.getposition` - Window positioning
- `window.setposition` - Window positioning

**Implementation Pattern (following CLAUDE.md error format):**
```c
case window_dbstats_token:
    if (bserror)
        copystring(BIGSTRING("\pCan't use window verbs because GUI is not available in headless mode"), bserror);
    return false;
```

**1e. Admin-Required Operations (2 verbs)**
- `clock.set` → Requires elevated privileges to set system time
- `file.mountservervolume` → Requires admin to mount volumes programmatically

**Implementation Pattern (following CLAUDE.md error format):**
```c
case clock_set_token:
    if (bserror)
        copystring(BIGSTRING("\pCan't set system time because it requires administrator privileges"), bserror);
    return false;

case file_mountservervolume_token:
    if (bserror)
        copystring(BIGSTRING("\pCan't mount server volumes because it requires administrator privileges"), bserror);
    return false;
```

**1f. Platform-Specific Windows (1 verb) - sys processor**
- `sys.winshellcommand` → Windows-specific shell execution

**Implementation Pattern (following CLAUDE.md error format):**
```c
case sys_winshellcommand_token:
    if (bserror)
        copystring(BIGSTRING("\pCan't run Windows shell commands because they are not available on this platform"), bserror);
    return false;
```

**1g. Optional Databases (38 verbs - DEFERRED)**
- `mysql.*` (23 verbs) - MySQL driver not available
- `sqlite.*` (15 verbs) - SQLite driver not available

**SKIP FOR PHASE 3** - Mark in planning for Phase 4/5

---

### Category 2: Silent Success (Noop) Implementation (3 verbs)

**Non-Critical Display Operations:**

**2a. Table Display Settings (2 verbs) - table processor**
- `table.getdisplaysettings` → Return true (no display settings in headless)
- `table.setdisplaysettings` → Return true (display settings irrelevant)

**Implementation Pattern:**
```c
case table_getdisplaysettings_token:
    (void)hparam1;  /* Suppress unused parameter warning */
    return true;    /* Success: no display settings in headless mode */

case table_setdisplaysettings_token:
    (void)hparam1;
    return true;
```

**2b. Memory Management (1 verb) - lang processor**
- `lang.flushmemory` → Return true (modern GC makes it unnecessary)

**Implementation Pattern:**
```c
case lang_flushmemory_token:
    (void)hparam1;  /* Suppress unused parameter warning */
    return true;    /* Success: modern GC handles memory automatically */
```

---

### Category 3: Delegate to Object State (2 verbs - NEEDS RESEARCH)

**Window Modification Tracking - window processor**

**Status:** DEFER IMPLEMENTATION - Needs architectural research first

**Research Questions:**
1. How are addresses extracted from `hparam1` tree nodes? (getparamvalue? flnthparam?)
2. Which `fldirty` flag should be accessed? (outline, WPText, table format, heap?)
3. How to resolve address to actual object without window/shell context?
4. Do these verbs work in headless or should they be error stubs?

**Options:**
- **Option A:** Implement as error stubs (Category 1) if no headless use case
- **Option B:** Research and implement delegation if headless scripts depend on them
- **Option C:** Defer to Phase 4 after usage analysis

**Recommendation:** Add research task to Phase 3.A preparation, decide before implementation

---

### Category 4: Keep Hybrid Implementation (2 verbs - NO CHANGE)

**Multi-Platform Path Handling - file processor**
- `file.getspecialfolderpath` - Keep existing UserTalk + C hybrid
- `file.getsystemfolderpath` - Keep existing UserTalk + C hybrid

**Status:** Already implemented as hybrid. No changes needed in Phase 3.

---

### Category 5: Already Implemented (1 verb - NO CHANGE)

**Unix Shell Execution - sys processor**
- `sys.unixshellcommand` - Full C implementation (calls `unixshellcall()`)

**Status:** Already functional. No changes needed.

---

## Implementation Plan - Step by Step

### Phase 3.A: Preparation & Verification

**Step 1: Audit Current State**
1. Check which headless_*.c files already exist for target processors
2. Verify processor registration in kernel_verbs_headless.c (init_efp functions)
3. Confirm tests/Makefile includes all stub files
4. Check if verbs already have any implementation (even partial)

**Step 2: Research window.isModified/setModified**
1. Search for existing usage in headless startup scripts
2. Determine if headless needs these or if they should be error stubs
3. If needed: research proper address resolution pattern
4. Document decision in plan before proceeding

**Step 3: Verify Build System**
1. Confirm all 8 target files are in LANG_RUNTIME_SOURCES
2. Check linking order and dependencies
3. Note any Makefile changes needed

### Phase 3.B: Core Implementation (24 error stubs + 3 noops)

**Step 1: Update `tests/headless_<processor>_verbs.c` files** (8 files)

Generate or manually update:
1. `tests/headless_lang_verbs.c` - Add 8 OSA error stubs + 1 noop (flushmemory)
2. `tests/headless_dialog_verbs.c` - Add 5 dialog error stubs
3. `tests/headless_statusbar_verbs.c` - Add 5 statusbar error stubs
4. `tests/headless_window_verbs.c` - Add 3 window error stubs + 2 delegation stubs
5. `tests/headless_clock_verbs.c` - Update to add `clock.set` error stub
6. `tests/headless_file_verbs.c` - Update to add `file.mountservervolume` error stub
7. `tests/headless_sys_verbs.c` - Add `sys.winshellcommand` error stub
8. `tests/headless_table_verbs.c` - Update to add 2 display settings noop stubs

**Implementation Pattern for Each File:**

```c
enum {
    <processor>v_verb1 = 0,
    <processor>v_verb2 = 1,
    /* ... all verbs */
};

static boolean <processor>_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case <processor>v_verb1:
            if (bserror) copystring(BIGSTRING("\p<error message>"), bserror);
            return false;

        case <processor>v_verb2:
            (void)hparam1;
            return true;  /* noop success */
    }
    return false;
}

boolean init<processor>verbs(...) {
    /* Register all verbs */
}
```

**Step 2: Verify registration in `kernel_verbs_headless.c`**
- Confirm `newfunctionprocessor()` calls use custom callbacks
- Verify `langaddkeyword()` registrations match enum token values
- Check that callback function signatures match interface

**Step 3: Update Exception Tables (if needed)**
- Review `verb_exceptions.py` for any new mappings
- Add Pattern C/D exceptions if verb names don't map to standard patterns
- Ensure all verbs from stub files are listed

---

### Phase 3.C: Testing & Validation

**Step 1: Run Headless Test Suite**
```bash
./tools/run_headless_tests.sh
```

Expected: All existing tests pass, no regressions

**Step 2: Add UserTalk-Level Tests**

Create .ut test scripts in appropriate location:
- `test_osa_stubs.ut` - Test OSA verbs return proper errors
- `test_dialog_stubs.ut` - Test dialog verbs return proper errors
- `test_window_stubs.ut` - Test window verbs return proper errors
- `test_noop_stubs.ut` - Test noop operations succeed silently

Test pattern (UserTalk script):
```
on test_lang_ddevent_error() {
    local(result);
    try {
        lang.DDEevent(...);  // Should fail
        return false;  // Test failed - no error raised
    }
    else {
        result = string(tryError);
        // Verify error message contains expected text
        return string.contains(result, "Can't call OSA");
    }
}
```

**Note:** UserTalk-level tests verify error propagation through the entire stack, not just C-level behavior.

**Step 3: Verify Backward Compatibility**
- Scripts calling these verbs should get clear errors instead of crashes
- Noop verbs should allow scripts to continue
- Window delegation should maintain existing behavior

---

## Success Criteria

### Must Have (Phase 3 Completion)
- [ ] **Preparation:** Current state audited, build system verified
- [ ] **Research:** window.isModified/setModified decision documented
- [ ] **24 Category 1 error stubs** implemented (OSA, GUI, admin, platform)
- [ ] **3 Category 2 noop stubs** implemented (table display, flushmemory)
- [ ] All error messages follow CLAUDE.md format: "Can't X because Y"
- [ ] All new stubs in appropriate `headless_*.c` files
- [ ] Verb registration verified in `kernel_verbs_headless.c`
- [ ] Exception tables updated (if needed)
- [ ] Headless test suite passes (no regressions)
- [ ] UserTalk-level tests verify error propagation

### Should Have (Quality Assurance)
- [ ] Code follows existing Frontier patterns
- [ ] Noop stubs allow legacy scripts to continue
- [ ] Error messages are user-friendly and actionable

### Could Have (Future Work)
- [ ] Automated stub generation tool improvements
- [ ] Enhanced error context for developers
- [ ] Performance profiling of new verbs

---

## Deferred Work (Phase 4/5)

**Optional Databases (38 verbs):**
- MySQL verbs (23) - Defer until database driver available
- SQLite verbs (15) - Defer until database driver available

**Plan:** Mark these in exception tables with "DEFERRED_DATABASE" flag for future phases.

---

## Critical Files Summary

| File | Lines | Purpose |
|------|-------|---------|
| `kernel_verbs_headless.c` | 1-2074 | Registration system |
| `tests/headless_lang_verbs.c` | - | Lang processor stubs |
| `tests/headless_dialog_verbs.c` | - | Dialog stubs |
| `tests/headless_statusbar_verbs.c` | - | Statusbar stubs |
| `tests/headless_window_verbs.c` | - | Window stubs + delegation |
| `tests/headless_clock_verbs.c` | - | Clock stubs |
| `tests/headless_file_verbs.c` | - | File stubs |
| `tests/headless_sys_verbs.c` | - | Sys stubs |
| `tests/headless_table_verbs.c` | - | Table stubs |
| `verb_exceptions.py` | 11-192 | Verb name mappings |
| `Makefile` | 160-161, 261-262 | Test build integration |
| `run_headless_tests.sh` | - | Test execution |

---

## IMPORTANT: Testing During Implementation

**Test Early and Often:**
- Run `./tools/run_headless_tests.sh` after EVERY set of changes (not just at the end)
- Implement unit tests ALONGSIDE code changes, not after
- If a change breaks existing tests, fix it immediately before proceeding
- Never accumulate untested changes - test incrementally to catch regressions early

**Testing Workflow:**
1. Make changes to 1-2 verbs in a processor file
2. Run full test suite to verify no regressions
3. Add UserTalk test for the new verb behavior
4. Run test suite again to verify new test passes
5. Commit working changes before moving to next verb
6. Repeat

**Why:** This prevents accumulating a huge number of untested changes and makes it easy to identify exactly which change caused a regression.

---

## Implementation Checklist

### Phase 3.A: Preparation & Verification
- [ ] Audit current state of headless_*.c files for target processors
- [ ] Verify processor registration in kernel_verbs_headless.c
- [ ] Confirm tests/Makefile includes all stub files
- [ ] Check if verbs already have any implementation (even partial)
- [ ] Research window.isModified/setModified usage in headless scripts
- [ ] Decide on window.isModified/setModified approach (error stub vs delegation)
- [ ] Verify build system integration for all 8 target files

### Phase 3.B: Core Implementation
- [ ] Implement 24 error stubs across 8 processor files
- [ ] Implement 3 noop stubs (table display settings, lang.flushmemory)
- [ ] Update verb_exceptions.py if needed

### Phase 3.C: Testing & Validation
- [ ] Run headless test suite and verify no regressions
- [ ] Create UserTalk-level tests for error propagation
- [ ] Verify backward compatibility with legacy scripts

### Phase 3.D: Documentation & PR
- [ ] Update MISSING_VERBS_REVIEW_WITH_AUDITS.md with completion status
- [ ] Commit all changes to feature/automatic-verb-binding
- [ ] Create PR to develop with summary of changes
- [ ] Merge after approval

---

## Next Steps After Approval

1. **Implementation:** Execute Phase 3.A steps in order
2. **Testing:** Run Phase 3.B validation
3. **PR:** Create PR to feature/automatic-verb-binding with all changes
4. **Review:** Merge after approval
5. **Planning:** Begin Phase 4 (Developer Experience) or Phase 5 (CI/CD)
