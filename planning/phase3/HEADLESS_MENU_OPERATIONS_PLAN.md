# Headless Menu Operations Implementation Plan

## Status
- State: Planning Phase
- Phase: 3
- Last Updated: 2025-12-21
- Notes: Design plan for headless menu operations


**Created**: 2025-12-21
**Status**: DRAFT - Awaiting approval
**Context**: UserTalk scripts need to manipulate menubarType objects in headless mode

---

## Executive Summary

**Requirement**: UserTalk verbs that manipulate menubarType objects must work in headless mode, even though menu display operations cannot.

**Current State**:
- menuverbs.c is NOT compiled in headless build
- headless_menu_stubs.c provides only pack/unpack for database persistence
- All menu verbs return false or are unavailable

**Solution**: Compile menuverbs.c in headless mode with selective stubbing

**Estimated Effort**: 3-5 days

---

## Part 1: Verb Classification

### Category A: Data Manipulation Verbs (MUST WORK HEADLESS)

These verbs manipulate menubarType object data structures without OS interaction:

| Verb | Function | Purpose | Implementation Status |
|------|----------|---------|----------------------|
| `menu.getScript()` | menugetscriptverb | Get script text from menu item | EXISTS in menuverbs.c |
| `menu.setScript()` | menusetscriptverb | Set script text for menu item | EXISTS in menuverbs.c |
| `menu.addMenuCommand()` | addmenucommandverb | Add item to menu | EXISTS in menuverbs.c |
| `menu.deleteMenuCommand()` | deletemenucommandverb | Remove item from menu | EXISTS in menuverbs.c |
| `menu.addSubMenu()` | addmenucommandverb (flsubmenu=true) | Add submenu | EXISTS in menuverbs.c |
| `menu.deleteSubMenu()` | deletemenucommandverb (flsubmenu=true) | Remove submenu | EXISTS in menuverbs.c |
| `menu.getCommandKey()` | menugetcommandkeyverb | Get keyboard shortcut | EXISTS in menuverbs.c |
| `menu.setCommandKey()` | menusetcommandkeyverb | Set keyboard shortcut | EXISTS in menuverbs.c |

**Assessment**: These functions already exist and operate on outline data structures (hdloutlinerecord). They should work headless with minimal changes.

---

### Category B: Display/Installation Verbs (STUB IN HEADLESS)

These verbs interact with the OS menu system and cannot work headless:

| Verb | Function | Purpose | Headless Action |
|------|----------|---------|-----------------|
| `menu.buildMenuBar()` | buildmenubarfunc | Install menus in OS menu bar | Return false |
| `menu.clearMenuBar()` | clearmenubarfunc | Remove menus from OS | Return false |
| `menu.install()` | installfunc | Install menu in OS | Return false |
| `menu.remove()` | removefunc | Remove menu from OS | Return false |
| `menu.isInstalled()` | isinstalledfunc | Check if menu is in OS | Return false |

**Assessment**: These must remain stubbed in headless mode.

---

## Part 2: Dependencies Analysis

### menuverbs.c Current Dependencies

From code inspection:

```c
#include "menueditor.h"    // ← Contains GUI globals: menudata, menuwindow, menuwindowinfo
#include "menuinternal.h"   // ← Function prototypes
#include "menubar.h"        // ← Menu bar data structures
#include "lang.h"           // ← Already compiled headless ✓
#include "langexternal.h"   // ← Already compiled headless ✓
#include "tablestructure.h" // ← Already compiled headless ✓
#include "opverbs.h"        // ← Already compiled headless ✓
#include "ops.h"            // ← Already compiled headless ✓
```

**Critical globals from menueditor.h**:
```c
extern hdlmenurecord menudata;      // ← Defined in menueditor.c (GUI file)
extern WindowPtr menuwindow;        // ← Defined in menueditor.c (GUI file)
extern hdlwindowinfo menuwindowinfo; // ← Defined in menueditor.c (GUI file)
```

**Problem**: These globals are defined in menueditor.c, which is NOT compiled in headless mode.

**Solution**: Provide headless versions of these globals.

---

### Key Functions Used by Data Manipulation Verbs

From code inspection of addmenucommandverb():

**Functions that work headless** (already compiled):
- `oppushoutline()` - Push outline globals (ops.c)
- `oppopoutline()` - Pop outline globals (ops.c)
- `opaddheadline()` - Add headline to outline (ops.c)
- `opdeposit()` - Deposit headline in outline (ops.c)
- `opsiblingvisiter()` - Visit sibling headlines (ops.c)
- `oprecursivelyvisit()` - Recursively visit headlines (ops.c)
- `opgetheadstring()` - Get headline text (ops.c)

**Functions that might need work**:
- `mepushmenudata()` - Push menu globals (menuverbs.c) - EXISTS, should work
- `mepopmenudata()` - Pop menu globals (menuverbs.c) - EXISTS, should work
- `shellfinddatawindow()` - Find window for data (shell.c) - GUI-specific, but has else branch

**Pattern in addmenucommandverb()**:
```c
if (shellfinddatawindow ((Handle) hm, &hinfo)) {
    shellpushglobals ((**hinfo).macwindow);
    pushundoaction (0);
}
else
    mepushmenudata (hm);  // ← Headless path!
```

**Key insight**: The code already has a non-window path (`mepushmenudata`) for when there's no GUI window!

---

## Part 3: Implementation Strategy

### Step 1: Provide Headless Globals

Create `headless/menu_headless_globals.c`:

```c
#ifdef FRONTIER_HEADLESS

#include "frontier.h"
#include "standard.h"
#include "menueditor.h"

// Provide globals normally defined in menueditor.c
hdlmenurecord menudata = nil;
WindowPtr menuwindow = nil;
hdlwindowinfo menuwindowinfo = nil;

#endif /* FRONTIER_HEADLESS */
```

**Rationale**: These globals are accessed by menuverbs.c but don't need to be functional in headless mode (they'll be nil).

---

### Step 2: Wrap Display Dependencies in menuverbs.c

Add conditional compilation around display-specific code in menuverbs.c:

**Location 1**: Line 1941 (langfindtargetwindow check)
```c
// BEFORE:
if (!langfindtargetwindow (idmenuprocessor, &targetwindow)) {
    // error
}

// AFTER:
#ifndef FRONTIER_HEADLESS
if (!langfindtargetwindow (idmenuprocessor, &targetwindow)) {
    // error
}
#endif
```

**Location 2**: Display-only verbs (buildmenubar, install, etc.)
```c
#ifdef FRONTIER_HEADLESS
    case buildmenubarfunc:
    case clearmenubarfunc:
    case installfunc:
    case removefunc:
    case isinstalledfunc:
        setbooleanvalue (false, v);
        return (true);  // Stub: return false without error
#else
    case buildmenubarfunc:
        // ... original implementation
#endif
```

---

### Step 3: Add menuverbs.c to Headless Build

Modify `frontier-cli/Makefile`:

```makefile
# Add to RUNTIME_SOURCES:
../Common/source/menuverbs.c \
```

**Rationale**: menuverbs.c should compile with FRONTIER_HEADLESS defined.

---

### Step 4: Remove headless_menu_stubs.c

The stub file becomes unnecessary since we're compiling the real menuverbs.c.

Modify `frontier-cli/Makefile`:

```makefile
# REMOVE from HEADLESS_STUBS:
# $(TESTSDIR)/headless_menu_stubs.c \  # ← DELETE THIS LINE
```

**Rationale**: menuverbs.c provides full menu verb implementation; stub is redundant.

---

### Step 5: Handle menueditor.c Dependencies (If Needed)

If menuverbs.c calls functions defined in menueditor.c, we need to either:

**Option A**: Stub them in headless mode
**Option B**: Extract non-display parts of menueditor.c into menueditor_core.c

**Assessment needed**: Check which menueditor.c functions are called from menuverbs.c

---

## Part 4: Dependencies to Check

### Functions from menuinternal.h Used by menuverbs.c

Need to verify these are available in headless build:

| Function | Defined In | Headless Status |
|----------|-----------|-----------------|
| `menewmenurecord()` | menueditor.c | NEEDS CHECK |
| `medisposemenurecord()` | menueditor.c | NEEDS CHECK |
| `meloadmenurecord()` | menupack.c | NEEDS CHECK |
| `mesavemenurecord()` | menupack.c | NEEDS CHECK |
| `mepackmenustructure()` | menupack.c | NEEDS CHECK |
| `meunpackmenustructure()` | menupack.c | NEEDS CHECK |

**Action**: Grep menuverbs.c for calls to these functions, determine if they're needed for data manipulation verbs.

---

## Part 5: Testing Strategy

### Unit Tests

Create `tests/test_menu_headless.c`:

```c
#include "frontier.h"
#include "standard.h"
#include "lang.h"
#include "menuverbs.h"

void test_menu_create() {
    // Test creating a menubar object
    // menu.new("testMenu")
}

void test_menu_addcommand() {
    // Test adding a menu command
    // menu.addMenuCommand(@testMenu, "File", "New", "msg(\"New\")")
}

void test_menu_getscript() {
    // Test retrieving script
    // menu.getScript(@testMenu, "File", "New")
}

void test_menu_setscript() {
    // Test updating script
    // menu.setScript(@testMenu, "File", "New", "msg(\"Updated\")")
}

void test_menu_persistence() {
    // Test saving and loading menubar from database
}

void test_display_verbs_stubbed() {
    // Verify buildMenuBar, install, etc. return false
}
```

**Run**: `make -C tests test_menu_headless && ./tests/test_menu_headless`

---

### Integration Tests

Add to `tests/headless_tests.c`:

```c
// Test that menu verbs work in UserTalk scripts
run_test("menu.addMenuCommand works", test_menu_addcommand_usertalk);
run_test("menu.getScript works", test_menu_getscript_usertalk);
run_test("menu persistence works", test_menu_persistence_usertalk);
```

---

### Regression Tests

```bash
# Ensure existing tests still pass:
./tools/run_headless_tests.sh

# Verify migration still works (menus persist across v6→v7):
make -C tests save_migration_tests
./tests/save_migration_tests
```

---

## Part 6: Implementation Phases

### Phase 1: Infrastructure (1 day)

- [ ] Create `headless/menu_headless_globals.c` with menudata, menuwindow, menuwindowinfo
- [ ] Add to Makefile
- [ ] Test compilation with FRONTIER_HEADLESS

**Deliverable**: Code compiles without errors

---

### Phase 2: Selective Stubbing (1 day)

- [ ] Wrap display-only verbs in `#ifdef FRONTIER_HEADLESS` guards
- [ ] Stub buildmenubar, clearmenubar, install, remove, isinstalled
- [ ] Verify data manipulation verbs are NOT stubbed

**Deliverable**: menuverbs.c compiles in both GUI and headless modes

---

### Phase 3: Add to Build (0.5 days)

- [ ] Add menuverbs.c to frontier-cli/Makefile RUNTIME_SOURCES
- [ ] Remove headless_menu_stubs.c from HEADLESS_STUBS
- [ ] Test clean build: `make -C frontier-cli clean && make -C frontier-cli`

**Deliverable**: frontier-cli builds with menuverbs.c included

---

### Phase 4: Dependency Resolution (1-2 days)

- [ ] Identify menueditor.c functions called by menuverbs.c
- [ ] Provide headless implementations or stubs for these functions
- [ ] Handle any menubar.c or menupack.c dependencies

**Deliverable**: All dependencies resolved, linker errors fixed

---

### Phase 5: Testing (1 day)

- [ ] Create unit tests for menu verbs
- [ ] Test menu creation, addMenuCommand, getScript, setScript
- [ ] Test menu persistence (save/load from database)
- [ ] Verify display verbs are properly stubbed
- [ ] Run full headless test suite

**Deliverable**: All tests passing

---

### Phase 6: Documentation (0.5 days)

- [ ] Update CONTRIBUTING.md with menu verb behavior
- [ ] Document which verbs work headless vs which are stubbed
- [ ] Add examples of menu manipulation in headless scripts

**Deliverable**: Clear documentation for users

---

## Part 7: Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| menueditor.c dependencies break compilation | HIGH | Identify dependencies first (Phase 4), stub if needed |
| Outline operations don't work without GUI | MEDIUM | Verify op* functions are truly headless-compatible |
| Menu persistence breaks in v7 format | MEDIUM | Test migration thoroughly |
| Existing GUI mode breaks | LOW | Use #ifdef guards, test both modes |
| Performance impact from extra compiled code | LOW | menuverbs.c is small, minimal impact |

---

## Part 8: Open Questions

### 1. menueditor.c Function Dependencies

**Question**: Which menueditor.c functions are called by data manipulation verbs?

**Action**: Grep menuverbs.c for calls to:
- `menewmenurecord()` - Create new menu record
- `medisposemenurecord()` - Dispose menu record
- `meloadmenurecord()` - Load from database
- `mesavemenurecord()` - Save to database

**Decision needed**: Do we need to extract these from menueditor.c, or can we stub them?

---

### 2. menubar.c Dependencies

**Question**: Does menuverbs.c depend on menubar.c (OS menu bar management)?

**Action**: Check if hmenustack (hdlmenubarstack) is accessed by data manipulation verbs

**Decision needed**: If yes, we may need menubar_headless.c with data-only implementation

---

### 3. Window Context Handling

**Question**: Can `shellfinddatawindow()` work in headless mode?

**Current code**:
```c
if (shellfinddatawindow ((Handle) hm, &hinfo)) {
    shellpushglobals ((**hinfo).macwindow);
}
else
    mepushmenudata (hm);  // ← Headless falls through to here
```

**Assessment**: Appears to already handle headless case (else branch)

**Decision needed**: Verify shellfinddatawindow returns false in headless, triggering else branch

---

## Part 9: Success Criteria

Implementation is complete when:

1. ✅ menuverbs.c compiles in headless mode
2. ✅ Data manipulation verbs work (addMenuCommand, getScript, setScript, etc.)
3. ✅ Display verbs are properly stubbed (buildMenuBar, install, etc. return false)
4. ✅ Menu objects persist correctly in database (v6→v7 migration works)
5. ✅ All existing headless tests still pass
6. ✅ New menu verb tests pass
7. ✅ UserTalk scripts can create and manipulate menubarType objects
8. ✅ Documentation updated

---

## Part 10: Alternative Approaches Considered

### Alternative 1: Create menuverbs_headless.c (NOT RECOMMENDED)

**Approach**: Duplicate menuverbs.c as menuverbs_headless.c with stubbed display verbs

**Pros**:
- No #ifdef clutter
- Clear separation

**Cons**:
- Code duplication (2,381 lines)
- Maintenance burden (changes need to be duplicated)
- Violates DRY principle

**Recommendation**: REJECT - Use #ifdef guards instead

---

### Alternative 2: Extract to menudata_core.c (OVERKILL)

**Approach**: Extract all data manipulation logic to separate file

**Pros**:
- Clean architecture
- No GUI dependencies at all

**Cons**:
- Significant refactoring (3-5 days)
- High risk of breaking GUI mode
- Premature abstraction

**Recommendation**: DEFER - Only do this if simple #ifdef approach fails

---

## Part 11: Next Steps

**Immediate**:
1. Get user approval on this plan
2. Verify user's list of required verbs matches Category A above
3. Confirm estimated effort (3-5 days) is acceptable

**Implementation**:
1. Start with Phase 1 (infrastructure)
2. Proceed through phases sequentially
3. Test after each phase
4. Adjust plan based on discoveries

**Fallback**:
- If dependencies are too complex, extract menuverbs_data.c with only data manipulation functions
- If op* functions don't work headless, investigate outline layer dependencies

---

## Appendix A: Code Locations

### Files to Modify

| File | Changes | Lines Affected |
|------|---------|----------------|
| `frontier-cli/Makefile` | Add menuverbs.c, remove stub | 2 lines |
| `Common/source/menuverbs.c` | Add #ifdef FRONTIER_HEADLESS guards | ~20 locations |
| `headless/menu_headless_globals.c` | Create new file | ~15 lines |

### Files to Create

| File | Purpose | Estimated Size |
|------|---------|----------------|
| `headless/menu_headless_globals.c` | Provide menudata globals | 15 lines |
| `tests/test_menu_headless.c` | Unit tests | 200-300 lines |

---

## Appendix B: Verification Checklist

After implementation, verify:

- [ ] `frontier-cli` builds cleanly with menuverbs.c
- [ ] No linker errors for menudata, menuwindow, menuwindowinfo
- [ ] menu.addMenuCommand() works in UserTalk
- [ ] menu.getScript() works in UserTalk
- [ ] menu.setScript() works in UserTalk
- [ ] menu.buildMenuBar() returns false in headless
- [ ] menubarType objects persist to database
- [ ] v6→v7 migration preserves menus
- [ ] `./tools/run_headless_tests.sh` passes all tests
- [ ] No new warnings in compilation

---

*Plan created: 2025-12-21*
*Status: DRAFT - Awaiting user review*
*Estimated effort: 3-5 days*
