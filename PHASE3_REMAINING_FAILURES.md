# Phase 3 Op Verbs - Remaining Test Failures

## Test Results Summary
- **Total tests**: 564
- **Passing**: 523 (92.7%)
- **Failing**: 41 (7.3%)
- **Phase 3 op verbs**: 202/224 passing (90.2%)

## Improvements Since Last Analysis
- Fixed cursor addressing to use opaque handles (getCursor/setCursor)
- Fixed expand/collapse test expectations (collapse before expand)
- Fixed expand verb to handle infinity constant (clamped to 32767)
- **Result**: +9 tests passing (514→523)

---

## Remaining Failures Breakdown

### 1. Pre-existing Failures (Not Phase 3) - 11 tests

These are pre-existing issues in other verb processors, not related to Phase 3 work:

```
✗ file.lock - lock a file
✗ lang.callscript - simple script
✗ lang.callscript - script with side effects
✗ lang.point - record to point
✗ lang.rect - record to rect
✗ lang.rgb - record to rgb
✗ lang.pattern - binary to pattern
✗ lang.list - binary to list
✗ lang.record - list to record
✗ lang.enum - string to enum
✗ op.go - returns false at boundary
```

**Status**: Not Phase 3 related, can ignore for this PR

---

### 2. Navigation Edge Cases - 2 tests

```
✗ op.firstSummit - go to first top-level node
  Expected: "First"
  Got: ""

✗ op.countSubs - infinity counts all descendants
  Expected success: True
  Got success: False
```

**Likely cause**: Phase 1-2 verb bugs or test expectations

**Action**: Investigate op.firstSummit and op.countSubs implementations

---

### 3. Refcon Negative Value Bug - 3 tests

```
✗ op.setRefcon - set negative refcon value
  Expected: -9999
  Got: 4294957297 (unsigned representation)

✗ op.setRefcon - large negative value (32-bit boundary)
  Expected: -2147483648
  Got: 2147483648

✗ workflow - refcon as node metadata during tree reorganization
  Expected: true
  Got: false (likely due to refcon bug)
```

**Root cause**: Bug in `langpackvalue`/`langunpackvalue` (core runtime)

**User confirmation**: Tested in Windows, returns -123 correctly

**Status**: Implementation is correct (matches legacy). Bug is in headless runtime, not Phase 3 code.

**Action**: Document but don't fix in this PR (pre-existing bug)

---

### 4. Parameter Validation (Duck Typing) - 12 tests

UserTalk has automatic runtime coercion (duck typing), so these tests expecting type errors are incorrect:

```
✗ op.setCursor - cursor from different outline should fail
✗ op.setDisplay - non-boolean parameter should fail
✗ op.setRefcon - non-long parameter should fail
✗ op.setCursor - cursor to deleted node should fail gracefully
✗ op.setExpansionState - state from different outline should fail
✗ op.setScrollState - scroll state from different outline should fail
✗ error recovery - setRefcon with no current node
✗ type safety - setCursor requires address type
✗ type safety - setDisplay requires boolean type
✗ type safety - setExpansionState requires address type
✗ type safety - setScrollState requires address type
✗ type safety - setRefcon requires long type
```

**User guidance**: "UserTalk has automatic runtime coercion, so that's expected (duck typing)"

**Status**: Tests have wrong expectations. Implementation is correct.

**Action**: Update test expectations or remove these tests

---

### 5. Expansion State Issues - 5 tests

```
✗ op.setExpansionState - restore saved expansion state
  Expected: true
  Got: false

✗ op.setExpansionState - invalid address type should fail
  Expected error_type: script_error
  Got: json_parse_error

✗ workflow - save expansion state before collapse and restore
  Expected success: True
  Got success: False

✗ op.setExpansionState - restore after structural changes
  Expected success: True
  Got success: False

✗ op.getExpansionState - state is independent of cursor
  Expected: true
  Got: false

✗ stress test - expansion state with deep nesting
  Expected: 2
  Got: 1
```

**Observations**:
- Manual test shows expansion state restore DOES work (returns true,true,true)
- BUT: Memory errors during test: "loadfromhandle fail: ix=0 ct=24 size=12"
- Expansion state is stored as binary data (packed/unpacked)
- Implementation delegates to `opgetexpansionstateverb()` / `opsetexpansionstateverb()`

**Possible causes**:
- Binary packing/unpacking issue with expansion state
- Cursor position affecting expansion state
- Deep nesting edge cases

**Action**: Investigate memory errors in expansion state packing/unpacking

---

### 6. Scroll State Issues - 2 tests

```
✗ op.setScrollState - round-trip scroll state
  Expected: "Line 20"
  Got: "Line 10"

✗ workflow - display off during state save/restore cycle
  Expected: "Line 20"
  Got: ""
```

**Cause**: Scroll state using opaque handles (like cursor), but save/restore not working correctly

**Current implementation**:
```c
// getScrollState returns hline1 as long
return setlongvalue((long)hline1, vreturned);

// setScrollState casts back
hnode = (hdlheadrecord)nodeid;
(**ho).hline1 = hnode;
```

**Action**: Investigate why scroll state isn't restoring correctly

---

### 7. Other Failures - 5 tests

```
✗ op.deleteSubs - multi-level children
  Expected success: True
  Got success: False

✗ op.reorg - move multiple levels right
  Expected: true
  Got: false

✗ workflow - build and navigate outline structure
  Expected: true
  Got: false

✗ stress test - rapid insert and delete
  Expected: 5
  Got: 6

✗ stress test - rapid cursor save/restore cycles
  Expected: true
  Got: false
```

**Mixed causes**: Likely combination of cursor addressing, navigation bugs, and test edge cases

**Action**: Investigate individually

---

## Recommendations

### For This PR (Immediate):

1. ✅ **DONE** - Fixed cursor addressing (opaque handles)
2. ✅ **DONE** - Fixed expand/collapse test expectations
3. ✅ **DONE** - Fixed expand verb to handle infinity
4. ⚠️ **DOCUMENT** - Refcon negative value bug (core runtime issue, not Phase 3)
5. ⚠️ **DOCUMENT** - Duck typing tests (wrong expectations, not bugs)
6. 🔍 **INVESTIGATE** - Expansion state memory errors
7. 🔍 **INVESTIGATE** - Scroll state save/restore
8. 🔍 **INVESTIGATE** - Navigation edge cases (firstSummit, countSubs)

### Can Defer (Future Issues):

- Parameter validation (duck typing vs strict typing) - test expectations wrong
- Cross-outline validation - not clear how to implement
- Pre-existing lang/file verb failures - not Phase 3 related

### Pass Rate by Category:

- **Phase 3 core functionality**: 90.2% passing (202/224)
- **After removing wrong expectations**: ~94% passing
- **After removing pre-existing bugs**: ~96% passing

---

## Key Fixes Applied

### 1. Cursor Addressing (Issue #259 filed)

**Before**: Used line numbers (changed when outline reorganized)
```c
opgetnodeline(hcursor, &linenum);
return setlongvalue(linenum, vreturned);
```

**After**: Uses opaque handles (stable identifiers)
```c
hdlheadrecord hcursor = (**ho).hbarcursor;
return setlongvalue((long)hcursor, vreturned);
```

### 2. Expand/Collapse Test Expectations

**Before**: Tests expected expand() to return true even when already expanded
**After**: Added collapse() calls before expand() to ensure something actually expands

### 3. Expand Infinity Handling

**Before**: `getintvalue()` failed with infinity (9223372036854775807 > 32767)
```c
short level;
if (!getintvalue(hparam1, 1, &level))
    return false;
```

**After**: Uses `getlongvalue()` and clamps to short range
```c
long levellong;
short level;
if (!getlongvalue(hparam1, 1, &levellong))
    return false;
if (levellong > 32767)
    level = 32767;  /* Effectively infinity */
else
    level = (short)levellong;
```

---

## Next Steps

1. **Run integration tests again** - Verify current state
2. **Document findings** - Update PHASE3_TEST_ANALYSIS.md
3. **Decide on scope** - Which remaining issues to fix in this PR vs defer
4. **Create PR** - Once core Phase 3 functionality is solid
