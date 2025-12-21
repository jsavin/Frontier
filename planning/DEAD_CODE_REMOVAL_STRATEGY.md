# Dead Code Removal Strategy

**Created**: 2025-12-20
**Status**: DRAFT - Awaiting user approval
**Context**: Systematic removal of dead code identified through static analysis (GUI-only, stubs, legacy formats, obsolete platforms)

---

## Executive Summary

**Problem**: Frontier codebase contains significant dead code that bloats the binary, complicates maintenance, and confuses developers:
- 28 GUI-only files (menus, windows, dialogs, display)
- 15+ "xxx"-prefixed disabled code blocks
- Legacy v6 writer code (no longer needed in v7-only builds)
- Obsolete platform code (Windows 95, old Mac OS)
- Stub functions that do nothing

**Solution**: Systematic phased removal with careful dependency analysis and testing

**Principles**:
- **Conservative**: Start with safest removals (explicit dead markers)
- **Incremental**: One category at a time, test between phases
- **Reversible**: Each phase in separate commits for easy rollback
- **Tested**: Run full test suite after each removal
- **Documented**: Track what was removed and why

**Impact**:
- **Binary size reduction**: Estimated 15-20% smaller executable
- **Build time**: Faster compilation (fewer files)
- **Maintenance**: Simpler codebase, less confusion
- **Cognitive load**: Developers don't have to understand dead code

---

## Current State Analysis

From `reports/static-analysis/dead-code/2025-12-20-dead-code-categories.md`:

| Category | Count | Risk | Priority |
|----------|-------|------|----------|
| GUI-only code | 28 files | Medium | High |
| Explicit dead markers | 18+ blocks | None | High |
| Legacy v6 writers | ~5 functions | Low | Medium |
| Obsolete platform code | 10+ blocks | Low | Medium |
| Stub functions | 10+ functions | Low | Low |

**Total estimated reduction**: 8,000-12,000 lines of code

---

## Guiding Principles

### 1. Safety First
- Remove only code that is provably dead
- Never remove code used by headless runtime
- Test thoroughly after each removal
- Keep git history for rollback

### 2. Systematic Approach
- Work from safest to riskiest
- One category per phase
- Document rationale for each removal
- Track removed functionality for future reference

### 3. Dependency Awareness
- Map dependencies before removal
- Stub functions called from kept code
- Remove entire dependency chains when safe
- Avoid orphaned function references

### 4. Build System Hygiene
- Update Makefile/Xcode project after each phase
- Ensure clean builds after removal
- No warnings about missing symbols
- Maintain backward compatibility for database files

### 5. Testing Discipline
- Run `./tools/run_headless_tests.sh` after each file removal
- Verify migration still works (v6→v7)
- Check verb bindings (`python3 tools/kernelverbs_parser/cli.py analyze`)
- Test on both Intel and Apple Silicon

---

## Phase 1: Explicit Dead Code Markers (SAFEST)

**Timeline**: Week 1
**Risk**: NONE - code explicitly marked as dead
**Impact**: ~1,500 lines removed

### 1A. Remove "xxx"-Prefixed Blocks

**Target**: All ifdef blocks with "xxx" prefix (convention for disabled code)

**Files and blocks**:
```
Common/source/strings.c:1339          #ifdef xxxWIN95VERSION
Common/source/shellwindow.c:59        #ifdef xxxWIN95VERSION
Common/source/langpack.c:868          #ifdef xxxWIN95VERSION
Common/source/shellwindowmenu.c:82    #ifdef xxxPIKE
Common/source/claybrowserexpand.c:504 #ifdef xxxfldebug
... (11 more xxxWIN95VERSION blocks)
```

**Removal process**:
1. For each file:
   - Search for `#ifdef xxx` blocks
   - Delete entire block (from `#ifdef` to `#endif`)
   - Verify no references to deleted symbols
2. Compile and test
3. Commit with message: `"cleanup: Remove xxxWIN95VERSION dead code from strings.c"`

**Testing**:
```bash
make clean && make
./tools/run_headless_tests.sh
```

**Estimated removal**: ~150 lines

---

### 1B. Remove Explicit Dead Code Markers

**Target**: Blocks marked OBSOLETE, NEVER, NeverDefine_For_Reference

#### OBSOLETE - Whirlpool crypto tables (whirlpool.c:618)

**What**: 1000+ line static lookup table for Whirlpool hash algorithm

**Why dead**: Replaced with optimized implementation

**Removal**:
```c
// DELETE ENTIRE BLOCK:
#ifdef OBSOLETE
    static const u64 C0[256] = { ... };
    static const u64 C1[256] = { ... };
    // ... tables C2-C7 ...
#endif
```

**Impact**: ~1,200 lines removed
**Risk**: NONE - marked OBSOLETE

---

#### NEVER - Error reporting code (langevaluate.c:897)

**What**: Unused error message formatting code

**Why dead**: Marked NEVER to prevent compilation

**Removal**:
```c
// DELETE ENTIRE BLOCK:
#ifdef NEVER
    bigstring bsline, bschar;
    long len, offset;
    // ... error formatting code ...
#endif
```

**Impact**: ~20 lines removed
**Risk**: NONE - marked NEVER

---

#### NeverDefine_For_Reference - WinSock documentation (WinSockNetEvents.c:50)

**What**: Documentation block listing Windows socket error codes

**Why "dead"**: Not actually code, just reference documentation

**Conversion** (don't delete, convert to comment):
```c
// BEFORE:
#ifdef NeverDefine_For_Reference
    For reference I am listing the error codes from the windows winsock.h file here
    #define WSABASEERR 10000
    ...
#endif

// AFTER:
/*
 * Windows Sockets Error Codes Reference
 * (From winsock.h)
 *
 * WSABASEERR 10000
 * All Windows Sockets error constants are biased by WSABASEERR
 * ...
 */
```

**Impact**: ~50 lines (preserved as comment)
**Risk**: NONE - not compiled code

---

**Phase 1 Total**: ~1,370 lines removed, 18 blocks eliminated

---

## Phase 2: Obsolete Platform Code (LOW RISK)

**Timeline**: Week 2
**Risk**: LOW - platforms not supported in headless mode
**Impact**: ~500 lines removed

### 2A. Old Mac OS Code

**Target**: oldMACVERSION blocks (Mac OS Classic alias handling)

**Files**: langhash.c (3 blocks)

**What**: Mac OS Classic filespec-to-alias conversion for database serialization

**Why dead**: v7 format doesn't use Mac aliases, uses portable paths

**Example** (langhash.c:3052):
```c
// DELETE:
#ifdef oldMACVERSION
    case filespecvaluetype: {
        register hdlfilespec x = val.data.filespecvalue;
        tyfilespec fs = **x;
        AliasHandle halias = nil;
        // ... Mac Classic alias handling ...
    }
#endif
```

**Testing**:
- Verify v7 format migration still works
- Check filespec handling in headless mode
- No regression in path handling

**Risk**: LOW - v7 migration already removed this dependency

---

### 2B. Windows 95 Code (Commented Out)

**Target**: Commented WIN95VERSION blocks

**Files**:
```
Common/source/langtrace.c:59         (trace file logging)
Common/source/frontierwindows.c:128  (window display)
```

**What**: Windows 95-specific code that's already commented out

**Why dead**: Already disabled, likely for years

**Removal**: Delete entire commented blocks

**Example**:
```c
// DELETE:
#ifdef WIN95VERSION
    static FILE * tracefile = NULL;
    #define messageset(bs) ...
#endif
```

**Impact**: ~40 lines removed

---

### 2C. Keep Active Platform Code

**DO NOT REMOVE**: Active WIN95VERSION block (shellsysverbs.c:610)

**Why keep**: Active code path for Windows environment variables

```c
// KEEP THIS:
#ifdef WIN95VERSION
    if (_putenv_s ((char *)varname, (char *)varvalue) != 0) {
#else
    if (setenv ((char *)varname, (char *)varvalue, 1) != 0) {
#endif
```

**Rationale**: Needed for Windows build compatibility (if ever supported)

---

**Phase 2 Total**: ~540 lines removed, 5 blocks eliminated

---

## Phase 3: GUI-Only Code (MEDIUM RISK)

**Timeline**: Weeks 3-6 (incremental)
**Risk**: MEDIUM - need careful dependency analysis
**Impact**: ~8,000 lines removed, 28 files potentially eliminated

### Strategy Overview

**Problem**: GUI files have dependencies from non-GUI code

**Approach**:
1. **Audit**: Map all callers of GUI functions
2. **Stub**: Replace called GUI functions with stubs
3. **Isolate**: Wrap GUI code in `#ifdef FRONTIER_GUI` (compile-time off)
4. **Verify**: Ensure headless builds work with GUI code disabled
5. **Remove**: Delete GUI files in later phase (optional)

**Why not immediate deletion**: Too risky - better to disable first, remove later

---

### 3A. Dependency Analysis

**High-dependency GUI files** (most references from other code):

| File | Refs | Strategy |
|------|------|----------|
| menu.c | 55 | Stub critical functions |
| frontierwindows.c | 44 | Stub window operations |
| dialogs.c | 38 | Stub dialog functions |
| shellmenu.c | 18 | Stub menu integration |
| menuverbs.c | 9 | Return false for verb calls |
| menueditor.c | 8 | Stub editor functions |

**Low-dependency GUI files** (few external references):

Files with 0-4 references can likely be wrapped entirely without stubbing.

---

### 3B. Stubbing Strategy

**Create**: `Common/source/gui_stubs.c` (headless-only)

**Purpose**: Provide no-op implementations of GUI functions called from non-GUI code

**Example**:
```c
#include "gui_stubs.h"

#ifdef FRONTIER_HEADLESS

// Stub: menu operations
boolean menubar_init(void) {
    return true;  // Success, but do nothing
}

void menubar_update(void) {
    // No-op
}

boolean dialog_run(DialogPtr *dlg, short *item) {
    *item = 0;
    return false;  // No dialog shown
}

// Stub: window operations
boolean window_new(WindowPtr *w, Rect *r, bigstring title) {
    *w = NULL;
    return false;  // No window created
}

void window_update(WindowPtr w) {
    // No-op
}

#endif /* FRONTIER_HEADLESS */
```

**Build integration**:
```makefile
# In Makefile
ifdef FRONTIER_HEADLESS
    SOURCES += Common/source/gui_stubs.c
    CFLAGS += -DFRONTIER_HEADLESS
else
    SOURCES += Common/source/menu.c Common/source/frontierwindows.c ...
endif
```

---

### 3C. Verb Table Stubbing

**Problem**: Some GUI files export UserTalk verbs (menuverbs.c, shellwindowverbs.c)

**Solution**: Keep verb bindings, but return false

**Example** (menuverbs.c):
```c
#ifdef FRONTIER_HEADLESS
// Stub implementations for headless mode
static boolean menuverbstub(hdltreenode hparam1, tyvaluerecord *v) {
    // All menu verbs return false in headless mode
    setbooleanvalue(false, v);
    return true;  // Call succeeded, but operation failed
}

// Map all menu verbs to stub
case menufunc:
    switch (token) {
        case installmenufunc:
        case deletemenufunc:
        case getmenubartextfunc:
            return menuverbstub(hparam1, v);
        // ... all menu verbs ...
    }
#else
    // Original GUI implementation
    case installmenufunc:
        // ... actual menu installation code ...
#endif
```

**Impact**: Scripts calling menu verbs get `false` return, can check and handle

---

### 3D. Testing Strategy

**Test 1: Build with GUI disabled**
```bash
make clean
FRONTIER_HEADLESS=1 make
# Should compile successfully with no GUI code
```

**Test 2: Run headless tests**
```bash
./tools/run_headless_tests.sh
# Should pass all tests
```

**Test 3: Verify verb stubs**
```bash
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "menu.install()"
# Should return false, not crash
```

**Test 4: Migration still works**
```bash
make -C tests clean && make -C tests save_migration_tests
./tests/save_migration_tests
# Should succeed
```

---

### 3E. Incremental Rollout

**Week 3**: Audit dependencies, create stub plan
**Week 4**: Implement gui_stubs.c, wrap highest-dependency files (menu.c, frontierwindows.c, dialogs.c)
**Week 5**: Wrap medium-dependency files (shellmenu.c, menuverbs.c, menueditor.c)
**Week 6**: Wrap low-dependency files (remaining 22 files)

**Per-file checklist**:
1. Identify all exported functions
2. Create stubs in gui_stubs.c (if called from non-GUI)
3. Wrap file with `#ifdef FRONTIER_HEADLESS ... #else ... #endif`
4. Update Makefile to conditionally compile
5. Build and test
6. Commit

---

### 3F. Future: Complete Removal (Optional)

**When**: After GUI stubbing has been stable for 6+ months

**Approach**: Delete GUI files entirely, keep only gui_stubs.c

**Why wait**:
- Gives time to discover missed dependencies
- Allows reverting if needed
- Less risky to disable first, remove later

**Decision point**: User approval needed before deletion

---

**Phase 3 Total**: ~8,000 lines disabled (not deleted yet), 28 files wrapped

---

## Phase 4: Legacy Format Writers (LOW RISK)

**Timeline**: Week 7
**Risk**: LOW - v6 format writing no longer needed
**Impact**: ~800 lines removed

### 4A. v6 Writer Identification

**Context**: Frontier v7 only writes v7 format, but reads v6 for migration

**Keep**: v6 readers (needed for migration)
**Remove**: v6 writers (never used)

**Files to analyze**:
```
Common/source/tablepack.c    - v4/v6 packing functions
Common/source/oppack_v7.c    - Mixed v6/v7 code
Common/source/langhash.c     - Hash serialization
Common/source/db_format.c    - Format adapter (keep mode switching for reads)
```

---

### 4B. Safe Removals

**Pattern**: Functions that write v4/v6 formats

**Example candidates**:
```c
// In tablepack.c - v4 header writing
static boolean packv4tableheader(...) {
    // Never called in v7-only builds
}

// In oppack_v7.c - v6 outline packing
static boolean packoutlinev6(...) {
    // Never called in v7-only builds
}
```

**Verification approach**:
1. Use `rg` to search for all callers
2. If only called from dead code or ifdef'd out, safe to remove
3. If unsure, wrap in `#ifdef SUPPORT_V6_WRITING` (off by default)

---

### 4C. Keep v6 Readers

**DO NOT REMOVE**: Any function with "unpack" or "read" in v6 context

**Examples to KEEP**:
```c
// KEEP - needed for migration
boolean hashunpacktable_v6(...)
boolean unpackoutline_v6(...)
boolean dbopen_v6format(...)
```

**Why**: Database migration requires reading v6 format

---

### 4D. Conservative Approach

**If unsure**: Wrap in ifdef rather than delete

```c
#ifdef SUPPORT_V6_WRITING  // Off by default
static boolean packv4tableheader(...) {
    // ... v4 writing code ...
}
#endif
```

**Rationale**:
- Can remove ifdef later if truly unused
- Safer than immediate deletion
- Build will fail if code is actually needed (missing symbol)

---

**Phase 4 Total**: ~800 lines removed or ifdef'd out

---

## Phase 5: Stub Functions (LOW RISK)

**Timeline**: Week 8
**Risk**: LOW - functions that do nothing
**Impact**: ~200 lines removed

### 5A. Identification

**Pattern**: Functions that immediately return false/NULL/0

**Search command**:
```bash
rg -A 2 "^\w+\s+\w+\s*\([^)]*\)\s*\{[^}]{0,50}return\s+(false|0|NULL|nil);?\s*\}" Common/source/*.c
```

**Example findings**:
```c
// Obvious stub - does nothing
boolean platformstub(void) {
    return false;
}

// Another stub - no-op with immediate return
void updatestub(void) {
    return;
}
```

---

### 5B. Verification Before Removal

**Check**:
1. Is function referenced anywhere? (`rg "functionname" Common/source/*.c`)
2. Is it exported in a header? (check corresponding .h file)
3. Is it part of a verb table or callback?

**If yes to any**: Keep or replace with stub marker comment

**Example safe removal**:
```c
// BEFORE:
static boolean unusedhelper(void) {
    return false;
}

// AFTER:
// (deleted - was unused stub)
```

**Example: keep with documentation**:
```c
// BEFORE:
boolean platformstub(void) {
    return false;
}

// AFTER:
// Platform-specific stub for headless mode
// Returns false to indicate operation not supported
boolean platformstub(void) {
    return false;
}
```

---

**Phase 5 Total**: ~200 lines removed or documented

---

## Integration with ifdef Cleanup Strategy

**Coordination**: Dead code removal and ifdef cleanup are complementary

### Shared Removals

Both strategies target:
- "xxx"-prefixed blocks → Dead code Phase 1
- OBSOLETE/NEVER markers → Dead code Phase 1
- oldMACVERSION → Dead code Phase 2

**Recommendation**: Execute Phase 1-2 of dead code removal **before** ifdef cleanup

**Rationale**: Reduces ifdef count early, making ifdef cleanup easier

---

### Sequencing

**Optimal order**:
1. **Week 1-2**: Dead code Phases 1-2 (explicit markers, obsolete platforms)
2. **Week 3-10**: Logging infrastructure (from ifdef cleanup strategy)
3. **Week 11-14**: Dead code Phase 3 (GUI stubbing)
4. **Week 15-16**: Dead code Phases 4-5 (legacy formats, stubs)
5. **Month 5+**: Continue ifdef cleanup (feature flags, etc.)

**Why this order**:
- Quick wins first (dead code markers)
- Logging infrastructure eliminates 76 debug ifdefs
- GUI stubbing is complex, needs dedicated focus
- Legacy formats and stubs are straightforward cleanup

---

## Build System Integration

### Makefile Changes

**Add conditional compilation for GUI code**:

```makefile
# Detect headless mode (default: yes)
FRONTIER_HEADLESS ?= 1

ifeq ($(FRONTIER_HEADLESS),1)
    CFLAGS += -DFRONTIER_HEADLESS

    # Exclude GUI files
    EXCLUDED_SOURCES := \
        Common/source/menu.c \
        Common/source/menubar.c \
        Common/source/menueditor.c \
        # ... (list all 28 GUI files)

    # Include stub implementations
    SOURCES += Common/source/gui_stubs.c
else
    # Include GUI files for full build
    SOURCES += \
        Common/source/menu.c \
        Common/source/menubar.c \
        # ... (list all GUI files)
endif

# Filter out excluded sources
SOURCES := $(filter-out $(EXCLUDED_SOURCES),$(SOURCES))
```

---

### Xcode Project Changes

**Create build configurations**:
1. **Frontier Headless** (default) - FRONTIER_HEADLESS=1
2. **Frontier Full** (optional) - GUI code included

**Conditional file inclusion**:
- Add GUI files with condition: "Include if FRONTIER_HEADLESS not defined"
- Add gui_stubs.c with condition: "Include if FRONTIER_HEADLESS defined"

---

## Testing Strategy

### Per-Phase Testing

**After each phase**:

```bash
# 1. Clean build
make clean && make

# 2. Run headless tests
./tools/run_headless_tests.sh

# 3. Verify migration
make -C tests clean && make -C tests save_migration_tests
./tests/save_migration_tests

# 4. Check verb bindings
cd tools/kernelverbs_parser && python3 cli.py analyze

# 5. Verify no broken references
# (build will fail if we removed something still needed)
```

---

### Regression Testing

**Before starting Phase 3 (GUI)**:

```bash
# Capture baseline behavior
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root databases/Frontier-v6-v7.root -e "defined(system)" > baseline.txt
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root databases/Frontier-v6-v7.root -e "sizeOf(system.verbs)" >> baseline.txt
```

**After Phase 3 (GUI stubbing)**:

```bash
# Compare behavior
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root databases/Frontier-v6-v7.root -e "defined(system)" > after_gui.txt
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root databases/Frontier-v6-v7.root -e "sizeOf(system.verbs)" >> after_gui.txt

# Should be identical
diff baseline.txt after_gui.txt
```

---

### Stub Testing

**Verify GUI stubs work correctly**:

```bash
# Test menu verb stubs (should return false, not crash)
./frontier-cli/frontier-cli -e "menu.install('Test', 1)"
# Expected: returns false

# Test window verb stubs
./frontier-cli/frontier-cli -e "window.new('Test Window')"
# Expected: returns false

# Test dialog verb stubs
./frontier-cli/frontier-cli -e "dialog.alert('Test')"
# Expected: returns false or no-op
```

---

## Risk Management

### Risk Assessment by Phase

| Phase | Risk | Mitigation |
|-------|------|------------|
| 1: Explicit dead markers | NONE | Code marked as dead explicitly |
| 2: Obsolete platforms | LOW | Not used in headless, v7 migration tested |
| 3: GUI stubbing | MEDIUM | Incremental, thorough dependency analysis, keep stubs |
| 4: Legacy writers | LOW | v6 readers kept, writers never called in v7 |
| 5: Stub functions | LOW | Static analysis + verification before removal |

---

### Rollback Strategy

**Each phase in separate commits**:
```bash
git commit -m "cleanup: Remove xxxWIN95VERSION dead code (Phase 1A)"
git commit -m "cleanup: Remove OBSOLETE whirlpool tables (Phase 1B)"
git commit -m "cleanup: Remove oldMACVERSION alias code (Phase 2A)"
# etc.
```

**If issues found**:
```bash
# Revert specific commit
git revert <commit-hash>

# Or reset to before phase
git reset --hard <commit-before-phase>
```

---

### Early Warning System

**Build failures = early detection**:
- Missing symbol errors → removed something still needed
- Linker errors → dependency chain broken
- Compile errors → ifdef mismatch

**Test failures = functional regression**:
- Headless tests fail → broke runtime
- Migration tests fail → broke v6→v7 compatibility
- Verb binding analysis changes → broke verb exports

**Action**: Investigate immediately, revert if needed, add stubs if appropriate

---

## Documentation Requirements

### Code Comments

**For stubbed functions**:
```c
/**
 * Stub: Menu installation (headless mode)
 *
 * In headless mode, menu operations are not supported.
 * Returns false to indicate operation not available.
 *
 * @return false (always fails in headless)
 */
boolean menu_install(void) {
    return false;
}
```

---

### Commit Messages

**Format**:
```
cleanup: <action> <what> (<phase>)

<Why this code is dead>
<What was removed>
<Testing performed>

Phase: <phase number>
Risk: <NONE|LOW|MEDIUM|HIGH>
```

**Example**:
```
cleanup: Remove xxxWIN95VERSION dead code from strings.c (Phase 1A)

The "xxx" prefix marks code as intentionally disabled. This block
contains Windows 95 text measurement code that's never compiled.

Removed: 15 lines from strings.c:1339-1354
Testing: make clean && make && ./tools/run_headless_tests.sh

Phase: 1A
Risk: NONE
```

---

### Tracking Document

**Create**: `planning/DEAD_CODE_REMOVAL_PROGRESS.md`

**Track**:
- Phase completion dates
- Lines removed per phase
- Files affected
- Issues encountered
- Rollbacks performed

**Example entry**:
```markdown
## Phase 1A: Remove "xxx"-Prefixed Blocks

**Status**: ✅ Complete
**Date**: 2025-12-21
**Lines removed**: 150
**Files affected**: 8
**Commits**:
  - e8a3f21: cleanup: Remove xxxWIN95VERSION from strings.c
  - a4b2c19: cleanup: Remove xxxWIN95VERSION from shellwindow.c
  - ...
**Issues**: None
**Testing**: All tests passed
```

---

## Success Metrics

### Quantitative

**Before dead code removal**:
- Total lines: ~150,000
- GUI files: 28 (active)
- Dead code blocks: 33+
- Binary size: ~2.5 MB (example)
- Build time: ~30 seconds

**After Phase 1-5** (target):
- Total lines: ~140,000 (7% reduction)
- GUI files: 28 (disabled via ifdef)
- Dead code blocks: 0
- Binary size: ~2.1 MB (16% reduction)
- Build time: ~25 seconds (17% faster)

---

### Qualitative

**Developer experience**:
- ✅ Less confusing code (no dead branches)
- ✅ Faster navigation (fewer files)
- ✅ Clearer intent (no "why is this here?" questions)
- ✅ Easier refactoring (fewer files to update)

**Maintenance burden**:
- ✅ Fewer files to track in git
- ✅ Simpler build configuration
- ✅ No accidental use of dead code
- ✅ Clear separation of GUI vs headless

---

## Timeline Summary

| Week | Phase | Activity | Impact |
|------|-------|----------|--------|
| 1 | 1A-1B | Remove explicit dead markers | -1,370 lines |
| 2 | 2A-2C | Remove obsolete platform code | -540 lines |
| 3 | 3A-3B | Audit GUI dependencies, create stubs | -0 lines (prep) |
| 4 | 3C-3D | Wrap high-dependency GUI files | -2,500 lines disabled |
| 5 | 3E | Wrap medium-dependency GUI files | -2,500 lines disabled |
| 6 | 3E | Wrap low-dependency GUI files | -3,000 lines disabled |
| 7 | 4A-4D | Remove legacy format writers | -800 lines |
| 8 | 5A-5B | Remove stub functions | -200 lines |

**Total**: 8 weeks, ~10,400 lines removed or disabled, 28 files wrapped

---

## Open Questions for User Approval

1. **GUI removal vs. stubbing**: Should we eventually delete GUI files entirely, or keep them ifdef'd indefinitely?
   - **Recommendation**: Keep ifdef'd for now, revisit in 6 months

2. **v6 writer support**: Should we completely remove v6 writing capability, or keep ifdef'd for future compatibility?
   - **Recommendation**: Remove completely (v7-only future)

3. **Stub retention**: Should we keep stub functions with documentation, or remove entirely?
   - **Recommendation**: Keep with clear comments (helps future developers)

4. **Build configuration**: Should headless mode be the only build target, or support GUI builds?
   - **Recommendation**: Headless-only by default, GUI ifdef'd out

5. **Phasing with ifdef cleanup**: Should we do dead code removal first, or interleave with logging infrastructure?
   - **Recommendation**: Dead code Phases 1-2 first (quick wins), then logging, then GUI

---

## Approval Checklist

Before starting implementation:

- [ ] **Phase 1-2** (explicit dead markers, obsolete platforms) approved for immediate execution
- [ ] **Phase 3** (GUI stubbing) strategy approved
- [ ] **Phase 4-5** (legacy formats, stubs) approach confirmed
- [ ] **Testing strategy** is adequate
- [ ] **Build system changes** (conditional compilation) acceptable
- [ ] **Timeline** (8 weeks) is reasonable
- [ ] **Integration with ifdef cleanup** sequencing approved

---

## Next Steps

1. **User approval** on strategy
2. **Create branch**: `dead-code-removal-phase1`
3. **Week 1**: Execute Phase 1 (explicit dead markers)
4. **Document progress**: Update `DEAD_CODE_REMOVAL_PROGRESS.md` after each phase
5. **Coordinate with logging infrastructure**: Sequence work appropriately

---

## References

- Analysis reports:
  - `reports/static-analysis/dead-code/2025-12-20-dead-code-categories.md`
  - `reports/static-analysis/dead-code/2025-12-20-gui-only-candidates.md`
  - `reports/static-analysis/2025-12-20-ifdef-inventory.md`
- Related planning docs:
  - `planning/IFDEF_CLEANUP_STRATEGY.md`
  - `planning/LOGGING_INFRASTRUCTURE_PLAN.md`
