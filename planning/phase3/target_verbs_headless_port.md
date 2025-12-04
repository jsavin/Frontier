# Target Verbs Headless Port - Assessment and Plan

**Date:** 2025-12-03
**Status:** Analysis Complete - Ready for Implementation

---

## Executive Summary

The target.* verbs (`target.get()`, `target.set()`, `target.clear()`) are currently disabled in headless mode due to UI dependencies. This document analyzes those dependencies and provides an implementation plan for headless support.

**Conclusion:** The target system is **mostly portable** with minimal UI-specific logic that can be safely removed/stubbed for headless.

---

## Current Implementation Analysis

### Core Functions

#### 1. `langgettarget()` (line 764) ✅ PORTABLE
```c
static boolean langgettarget (hdlhashtable *htable, bigstring bsname) {
    pushouterlocaltable();
    fl = langgetsymbolval(nametargetval, &val, &hnode);
    pophashtable();
    return getaddressvalue(val, htable, bsname);
}
```
**Dependencies:** None - pure hash table lookup
**Action:** Already works in headless

#### 2. `langsettarget()` (line 712) ⚠️ HAS UI DEPS
```c
boolean langsettarget (hdlhashtable htable, bigstring bsname, tyvaluerecord *prevtarget) {
    // Creates address value for new target
    setaddressvalue(htable, bsname, &val);

    // If old target exists, close its hidden window
    if (flhadtarget) {
        if (!langclosehiddenwindow(oldval))  // ← UI DEPENDENCY
            setnilvalue(&oldval);
    }

    // Store new target
    hashassign(nametargetval, val);
    exemptfromtmpstack(&val);
    return true;
}
```
**Dependencies:** `langclosehiddenwindow` (window cleanup)
**Headless Strategy:** Skip window cleanup (no-op)

#### 3. `langcleartarget()` (line 677) ⚠️ HAS UI DEPS
```c
boolean langcleartarget (tyvaluerecord *prevtarget) {
    pushouterlocaltable();
    if (hashlookup(nametargetval, &val, &hnode)) {
        langclosehiddenwindow(val);  // ← UI DEPENDENCY
        hashdelete(nametargetval, true, true);
    }
    pophashtable();
    return fl;
}
```
**Dependencies:** `langclosehiddenwindow` (window cleanup)
**Headless Strategy:** Skip window cleanup (no-op)

#### 4. `langclosehiddenwindow()` (line 630) ❌ UI-ONLY
```c
static boolean langclosehiddenwindow (tyvaluerecord val) {
    getaddressvalue(val, &htable, bsname);
    langsymbolreference(htable, bsname, &val, &hnode);

    if (langexternalwindowopen(val, &hinfo)) {          // ← UI
        if (((**hinfo).flhidden) && (hinfo != shellwindowinfo))
            shellclosewindow((**hinfo).macwindow);      // ← UI
    }
    return true;
}
```
**Purpose:** Close hidden editor windows to prevent memory leaks in GUI
**Headless Strategy:** Return `true` (no windows to close)

---

### Verb Wrappers

#### 1. `target.get()` → `langgettargetfunc()` (line 1071) ⚠️ HAS UI FALLBACK
```c
static boolean langgettargetfunc (hdltreenode hparam1, tyvaluerecord *vreturned) {
    fl = langgettarget(&htable, bsname);  // ✅ Portable

    if (!fl) {  // No explicit target - try implicit
        if (langfindtargetwindow(-1, &target)) {      // ← UI
            shellpushglobals(target);                   // ← UI
            (*shellglobals.getvariableroutine)(&hvariable);
            langexternalfindvariable(hvariable, &htable, bsname);
            shellpopglobals();                          // ← UI
        }
    }

    return fl ? setaddressvalue(htable, bsname, vreturned) : setnilvalue(vreturned);
}
```
**UI Dependencies:**
- `langfindtargetwindow` - finds frontmost window
- `shellpushglobals` / `shellpopglobals` - window context
- `shellglobals.getvariableroutine` - window-specific callback

**Headless Strategy:** Skip implicit target fallback (return nil if no explicit target)

#### 2. `target.set()` → `langsettargetfunc()` (line 1113) ⚠️ HAS UI DEPS
```c
static boolean langsettargetfunc (hdltreenode hparam1, tyvaluerecord *vreturned) {
    getvarvalue(hparam1, 1, &htable, bsname, &val, &hnode);

    if (val.valuetype == novaluetype) {
        langcleartarget(vreturned);  // ⚠️ Has UI in langclosehiddenwindow
        return true;
    }

    langsettarget(htable, bsname, vreturned);  // ⚠️ Has UI in langclosehiddenwindow

    if (!langzoomvalwindow(htable, bsname, val, false)) {  // ← UI
        langcleartarget(nil);
        return false;
    }

    return true;
}
```
**UI Dependencies:**
- `langzoomvalwindow` - opens/zooms window for the targeted value
- `langcleartarget` → `langclosehiddenwindow` (indirect)

**Headless Strategy:** Skip window zooming (headless has no windows)

#### 3. `target.clear()` → Inline (line 1810) ⚠️ HAS UI DEPS
```c
case cleartargetfunc:
    if (!langcheckparamcount(hparam1, 0))
        return (false);

    setbooleanvalue(langcleartarget(nil), v);  // ⚠️ Has UI in langclosehiddenwindow
    return (true);
```
**UI Dependencies:** Via `langcleartarget` → `langclosehiddenwindow`
**Headless Strategy:** Use headless version of `langcleartarget`

---

## UI Dependency Summary

### Functions with UI Dependencies

| Function | UI Deps | Purpose | Headless Action |
|----------|---------|---------|-----------------|
| `langclosehiddenwindow` | `langexternalwindowopen`, `shellclosewindow` | Close hidden editor windows | **No-op** (always return true) |
| `langsettarget` | Calls `langclosehiddenwindow` | Set new target | **Remove window cleanup** |
| `langcleartarget` | Calls `langclosehiddenwindow` | Clear target | **Remove window cleanup** |
| `langgettargetfunc` | `langfindtargetwindow`, `shellpushglobals` | Get target (with fallback) | **Remove implicit target fallback** |
| `langsettargetfunc` | `langzoomvalwindow`, indirect via above | Set target and open window | **Remove window zoom** |

### Portable Functions (No Changes Needed)

- `langgettarget()` - Pure hash table lookup ✅
- `langunsettarget()` - Calls `langgettarget` and `langcleartarget` ✅ (after headless port)
- `copyexemptvalue()` - Pure value copy ✅

---

## Headless Implementation Plan

### Phase 1: Core Target Functions (30 minutes)

**File:** `Common/source/langverbs.c`

#### 1.1 Replace `#ifndef FRONTIER_HEADLESS` Guards

**Current:**
```c
#ifndef FRONTIER_HEADLESS
boolean langcleartarget (tyvaluerecord *prevtarget) { ... }
#endif

#ifndef FRONTIER_HEADLESS
boolean langsettarget (hdlhashtable htable, bigstring bsname, tyvaluerecord *prevtarget) { ... }
#endif
```

**Change to:**
```c
boolean langcleartarget (tyvaluerecord *prevtarget) { ... }
boolean langsettarget (hdlhashtable htable, bigstring bsname, tyvaluerecord *prevtarget) { ... }
```

#### 1.2 Add Headless `langclosehiddenwindow` Stub

**Insert before line 630:**
```c
#ifdef FRONTIER_HEADLESS
static boolean langclosehiddenwindow (tyvaluerecord val) {
    // In headless mode, there are no windows to close
    // Just verify the value is valid and return true
    (void)val;
    return true;
}
#else
static boolean langclosehiddenwindow (tyvaluerecord val) {
    // ... existing GUI implementation ...
}
#endif
```

**Rationale:** In headless, there are no editor windows, so closing them is a no-op.

### Phase 2: Verb Wrapper Functions (45 minutes)

#### 2.1 Make `langgettargetfunc` Headless-Aware

**Current (lines 1084-1104):**
```c
fl = langgettarget(&htable, bsname);

if (!fl) {  // Fallback to implicit target from frontmost window
    WindowPtr target;
    // ... 20 lines of window code ...
}
```

**Replace with:**
```c
fl = langgettarget(&htable, bsname);

#ifndef FRONTIER_HEADLESS
if (!fl) {  // Fallback to implicit target from frontmost window
    WindowPtr target;
    hdlexternalvariable hvariable;

    htable = nil;
    setemptystring(bsname);

    if (langfindtargetwindow(-1, &target)) {
        shellpushglobals(target);
        if ((*shellglobals.getvariableroutine)(&hvariable))
            fl = langexternalfindvariable(hvariable, &htable, bsname);
        shellpopglobals();
    }
}
#endif
```

**Headless Behavior:** If no explicit target is set, return `nil` (no implicit target)

#### 2.2 Make `langsettargetfunc` Headless-Aware

**Current (line 1156):**
```c
if (!langzoomvalwindow(htable, bsname, val, false)) {
    disposevaluerecord(*vreturned, false);
    disablelangerror();
    langcleartarget(nil);
    enablelangerror();
    return (false);
}
```

**Replace with:**
```c
#ifndef FRONTIER_HEADLESS
if (!langzoomvalwindow(htable, bsname, val, false)) {
    disposevaluerecord(*vreturned, false);
    disablelangerror();
    langcleartarget(nil);
    enablelangerror();
    return (false);
}
#endif
```

**Headless Behavior:** Setting target succeeds without opening a window

### Phase 3: Testing (30 minutes)

#### 3.1 Unit Tests

**File:** `tests/cli_runtime_tests.c`

```c
void test_target_set_get_clear(void) {
    char output[4096];

    // Test 1: target.get() with no target returns nil
    run_cli_command("-e 'typeOf(target.get())'", output, sizeof(output));
    assert(string_contains(output, "novaluetype"));

    // Test 2: target.set() on a table value
    run_cli_command("-e 'new(tableType, @workspace.test); target.set(@workspace.test); defined(workspace.test)'",
                    output, sizeof(output));
    assert(string_contains(output, "true"));

    // Test 3: target.get() returns the set target
    run_cli_command("-e 'new(tableType, @workspace.test); target.set(@workspace.test); string(target.get())'",
                    output, sizeof(output));
    assert(string_contains(output, "workspace.test"));

    // Test 4: target.clear() clears the target
    run_cli_command("-e 'new(tableType, @workspace.test); target.set(@workspace.test); target.clear(); typeOf(target.get())'",
                    output, sizeof(output));
    assert(string_contains(output, "novaluetype"));
}
```

#### 3.2 Integration Test

```bash
# Test script execution with target manipulation
./frontier-cli -e '
    new(tableType, @workspace.testTarget);
    target.set(@workspace.testTarget);
    msg("Target set to: " + string(target.get()));
    target.clear();
    msg("Target after clear: " + string(target.get()))
'
```

**Expected output:**
```
Target set to: workspace.testTarget
Target after clear: nil
```

---

## Implementation Checklist

### Core Functions
- [ ] Remove `#ifndef FRONTIER_HEADLESS` guards from `langcleartarget`
- [ ] Remove `#ifndef FRONTIER_HEADLESS` guards from `langsettarget`
- [ ] Add headless stub for `langclosehiddenwindow` (always return true)

### Verb Wrappers
- [ ] Guard implicit target fallback in `langgettargetfunc` with `#ifndef FRONTIER_HEADLESS`
- [ ] Guard window zoom in `langsettargetfunc` with `#ifndef FRONTIER_HEADLESS`
- [ ] Verify `langunsettarget` works (should work automatically)

### Testing
- [ ] Add `test_target_set_get_clear()` to `cli_runtime_tests.c`
- [ ] Run integration test with target manipulation script
- [ ] Verify target persists across script execution
- [ ] Verify `frontier.getFilePath()` can use target (next phase)

### Documentation
- [ ] Document headless behavior differences (no implicit target from windows)
- [ ] Update planning docs with target support completion

---

## Headless Behavior Differences

### What Works Identically
- `target.set(address)` - sets explicit target ✅
- `target.get()` - retrieves current target ✅
- `target.clear()` - clears current target ✅
- Target storage in `nametargetval` ✅
- Target value is an `addressvaluetype` ✅

### What's Different
- **No implicit target fallback**: In GUI, if no explicit target, falls back to frontmost window's variable
  - Headless: Returns `nil` if no explicit target
- **No window operations**: Setting/clearing target doesn't open/close editor windows
  - Headless: Target is purely logical - no visual side effects
- **No window zoom**: `target.set()` doesn't bring window to front
  - Headless: Setting target always succeeds (no window to fail opening)

---

## Benefits for `frontier.getFilePath()`

Once target verbs are ported, `frontier.getFilePath()` can be implemented properly:

```c
case getfilepathfunc: {
    hdlhashtable htarget;
    bigstring bstarget;
    const char *path;

    // Check if target is set
    if (langgettarget(&htarget, bstarget)) {
        // TODO: Map htarget → database → filepath
        // For now, always return system root
    }

    // Default: return system root path
    path = cli_get_system_root_path();

    tyfilespec fs;
    pathtofilespec_headless(path, &fs);
    return setfilespecvalue(&fs, v);
}
```

**Future enhancement:** Implement database→filepath mapping to return correct path for guest databases

---

## Estimated Effort

| Phase | Task | Time |
|-------|------|------|
| 1 | Remove FRONTIER_HEADLESS guards | 10 min |
| 1 | Add langclosehiddenwindow stub | 10 min |
| 1 | Verify compilation | 10 min |
| 2 | Guard implicit target fallback | 15 min |
| 2 | Guard window zoom | 15 min |
| 2 | Verify compilation | 15 min |
| 3 | Write unit tests | 20 min |
| 3 | Run integration tests | 10 min |
| **Total** | | **~2 hours** |

---

## Risk Assessment

**Risk Level:** Low ✅

**Rationale:**
- Core logic is purely hash table operations (already portable)
- UI dependencies are isolated to window management (easily stubbed)
- No changes to data structures or storage format
- Existing GUI code remains unchanged (guarded with `#ifndef FRONTIER_HEADLESS`)

**Mitigation:**
- Keep UI code paths intact with preprocessor guards
- Test both GUI and headless builds after changes
- Comprehensive unit tests ensure correct behavior

---

## Next Steps

1. **Implement Phase 1** - Enable core target functions
2. **Implement Phase 2** - Update verb wrappers
3. **Test** - Verify target.* verbs work in headless
4. **Document** - Update planning docs
5. **Implement `frontier.getFilePath()`** - Use target system

---

## Conclusion

The target verb system is **ready for headless porting** with minimal changes:
- 90% of code is already portable (hash table operations)
- 10% is window management (can be safely stubbed/removed)
- No architectural changes needed
- Low risk, high value (unblocks `frontier.getFilePath()` and editing verbs)

**Recommendation:** Proceed with implementation ✅
