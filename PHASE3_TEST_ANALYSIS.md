# Phase 3 Test Failure Analysis

## Test Results Summary
- **Integration Suite**: 514/564 passing (91.1%)
- **Phase 3 Op Verbs**: 183/224 passing (81.7%)
- **Remaining failures**: 50 tests (41 Phase 3-related)

---

## Critical Design Issue: Cursor Addressing

### Problem
**Current implementation uses line numbers (changeable)**:
```c
opgetnodeline(hcursor, &linenum);  // Get line number
linenum++;  // Convert 0-based to 1-based
return setlongvalue(linenum, vreturned);
```

**Legacy behavior uses opaque identifiers (stable)**:
- Memory addresses or node handles
- Remain valid even when outline reorganizes
- Don't change when you insert/move nodes

### Test Demonstrating the Bug
```yaml
op.getCursor - cursor persists during outline modification:
  1. Insert "Line 1", "Line 2"
  2. Save cursor at Line 2 (line number = 2)
  3. Insert "Line 1.5" between them
  4. Now Line 2 has become Line 3
  5. Restore cursor (line number 2 = "Line 1.5")
  Expected: "Line 2"
  Got: "Line 1.5"
```

### User's Guidance
> "the number (or list-members) were historically always opaque identifiers...
> if you reorganize the outline the identifiers need to remain constant so that
> even after moving a headline your script can reselect it"

> "that's a weakness I'd like to fix, but not now. (You should ask the issue
> writer agent to file a P1 issue to make op.get/setSelection non-opaque or
> design an extension to the addressing system that enables addressing specific
> outline nodes.)"

### Proposed Fix (for this PR)
Cast node handles to long values:
```c
// getCursor
hdlheadrecord hcursor = (**ho).hbarcursor;
return setlongvalue((long)hcursor, vreturned);  // Opaque identifier

// setCursor
hdlheadrecord hnode = (hdlheadrecord)linenum;  // Cast back
(**ho).hbarcursor = hnode;
```

**Risks**:
- Handle might be 64-bit, long might be 32-bit (need to verify)
- Crashes if node was deleted
- Not serializable across sessions
- But matches legacy behavior

### Issue to File
**Priority**: P1
**Title**: "Design improved outline cursor addressing system"
**Description**: Current op.getCursor/setCursor use opaque memory addresses which:
- Can't be serialized
- Become invalid if node deleted
- Not user-friendly
Need to design extension to addressing system that allows stable node identifiers

---

## Category Breakdown

### 1. Pre-existing Failures (Not Phase 3) - 9 tests
```
lang.callscript - simple script
lang.callscript - script with side effects
lang.point - record to point
lang.rect - record to rect
lang.rgb - record to rgb
lang.pattern - binary to pattern
lang.list - binary to list
lang.record - list to record
lang.enum - string to enum
```
**Status**: Not Phase 3 related, can ignore for this PR

---

### 2. Expand/Collapse Metadata - 13 tests

**User Confirmation**: "Expand/collapse needs to work in headless mode, and the outline should contain this metadata."

**Manual Test Results**:
```bash
# Test 1: Collapse works
op.collapse(); op.subsExpanded()
=> false ✓

# Test 2: Expand works
op.collapse(); op.expand(1); op.subsExpanded()
=> true ✓
```

**Failures**:
```
op.expand - expand one level (expected true, got false)
op.expand - expand all descendants (infinity) (FAILED)
op.collapse - collapse current node (FAILED)
op.collapse - returns boolean (expected true, got false)
op.deleteSubs - multi-level children (FAILED)
workflow - expand and collapse state (FAILED)
op.setExpansionState - restore saved expansion state (expected true, got false)
op.setExpansionState - round-trip expansion state (FAILED)
op.setExpansionState - complex outline with multiple levels (FAILED)
op.setExpansionState - restore after structural changes (FAILED)
op.getExpansionState - state is independent of cursor (expected true, got false)
stress test - expansion state with deep nesting (expected 2, got 1)
workflow - save expansion state before collapse and restore (FAILED)
```

**Analysis Needed**: Expand/collapse DO work, but specific test scenarios fail. Need to investigate:
- What specific edge cases are failing?
- Is getExpansionState/setExpansionState working correctly?
- Check delegation to `opgetexpansionstateverb()` / `opsetexpansionstateverb()`

---

### 3. Refcon Negative Value Bug - 2 tests

**Bug Confirmed**:
```bash
op.setRefcon(-9999); op.getRefcon()
Expected: -9999
Got: 4294957297  # Unsigned representation of -9999
```

**Root Cause**: `langpackvalue` / `langunpackvalue` losing signedness

**Current Implementation**:
```c
// setRefcon
langpackvalue(val, &hbinary, HNoNode);  // Pack value
(**hcursor).hrefcon = hbinary;

// getRefcon
langunpackvalue(hrefcon, &linkedval);  // Unpack value
*vreturned = linkedval;
```

**User's Response**: "I don't understand the refcon negative values issue, but it sounds like a bug that needs investigating, or I could do some tests in the Windows app."

**Next Steps**:
- Investigate langpackvalue/langunpackvalue implementation
- Check if this is Phase 3 bug or core runtime bug
- User can test in Windows app to verify expected behavior

---

### 4. Parameter Validation (Not Implemented) - 10 tests

**User Question**: "What specifically would duck typing look like for parameter validation?"

**Current Behavior**: Phase 3 verbs accept any type, no validation
```c
// Example: setDisplay accepts any value
if (!getparamvalue(hparam1, 1, &val))  // Gets ANY type
    return false;
```

**Failing Tests**:
```
op.setCursor - cursor from different outline should fail
op.setDisplay - non-boolean parameter should fail
op.setExpansionState - invalid address type should fail
op.setExpansionState - state from different outline should fail
op.setScrollState - scroll state from different outline should fail
op.setCursor - cursor to deleted node should fail gracefully
error recovery - setRefcon with no current node
type safety - setCursor requires address type
type safety - setDisplay requires boolean type
type safety - setExpansionState requires address type
type safety - setScrollState requires address type
type safety - setRefcon requires long type
```

**Duck Typing Example** (what we have now):
```c
setDisplay(42)        // Accepts number, coerces to boolean
setDisplay("hello")   // Accepts string, coerces to boolean
setRefcon("string")   // Accepts string, coerces to long (probably 0)
```

**Strict Typing** (what tests expect):
```c
setDisplay(42)        // Error: requires boolean
setDisplay("hello")   // Error: requires boolean
setRefcon("string")   // Error: requires long
```

**User's Response**: "If you are worried about cross-outline pollution it would be good to check for that, but I don't know how to do it in UserTalk."

**Recommendation**:
- Add type checking with `langcheckparamcount` and specific type getters
- For cross-outline validation: probably not feasible in Phase 3

---

### 5. Navigation Edge Cases - 3 tests

```
op.go - returns false at boundary (expected false, got true)
op.firstSummit - go to first top-level node (expected "First", got empty)
op.countSubs - infinity counts all descendants (FAILED)
```

**Likely**: Phase 1-2 verb bugs, not Phase 3

---

### 6. Other Issues - 6 tests

```
op.reorg - move multiple levels right (expected true, got false)
workflow - build and navigate outline structure (expected true, got false)
stress test - rapid insert and delete (expected 5, got 6)
workflow - refcon as node metadata during tree reorganization (expected true, got false)
op.setScrollState - round-trip scroll state (expected "Line 20", got "Line 10")
workflow - display off during state save/restore cycle (expected "Line 20", got empty)
op.getScrollState - independent across multiple outlines (expected true, got false)
stress test - rapid cursor save/restore cycles (expected true, got false)
```

**Analysis**: Mix of cursor addressing bugs and potential scroll state issues

---

## Recommendations

### Immediate Actions (This PR):
1. ✅ **File issue** about cursor addressing (P1)
2. 🔧 **Investigate refcon negative value bug** (might be core runtime issue)
3. 🔍 **Debug expansion state failures** (user confirmed it should work)
4. ⚠️ **Fix one test expectation bug**: "op.getLineText - no parameters" still expects "str" not "TEXT"

### Discussion Needed:
1. **Cursor addressing**: Should I fix it in this PR or defer to future issue?
2. **Parameter validation**: Duck typing vs strict typing - user preference?
3. **Refcon bug**: Should user test in Windows app first?

### Can Defer:
- Navigation edge cases (likely pre-existing)
- Cross-outline validation (not clear how to implement)
