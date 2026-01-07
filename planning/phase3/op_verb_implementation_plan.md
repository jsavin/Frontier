# OP Verb Implementation Strategy (TDD Approach)

## Executive Summary

This plan implements 45 op verbs in headless mode using Test-Driven Development (TDD). The implementation leverages:
- **77 integration tests** already written in `tests/integration/test_cases/op_verbs.yaml`
- **Full implementations** available in `Common/source/opverbs.c` (4,619 lines)
- **Stub framework** in `tests/headless_op_verbs.c` with proper dispatch infrastructure

## Critical Discovery: Target-Based Implementation

The key insight from the codebase analysis:

**GUI Frontier Pattern** (lines 3468-3481 in opverbs.c):
```c
// GUI: Find window with outline, set globals
if (!langfindtargetwindow(idoutlineprocessor, &targetwindow))
    goto error;
shellpushglobals(targetwindow);
(*shellglobals.gettargetdataroutine)(idoutlineprocessor);
ho = outlinedata;  // Global outline
```

**Headless Pattern** (must implement):
```c
// Headless: Get outline from target.get() instead of window
hdlexternalvariable htarget;
hdloutlinevariable hv;
hdloutlinerecord ho;

if (!langfindtargetwindow(idoutlineprocessor, nil))  // No window needed
    goto error;

// Get outline from target system (not from window)
if (!langexternalgettargetexternalvariable(idoutlineprocessor, &htarget))
    goto error;

hv = (hdloutlinevariable)htarget;
if (!opverbinmemory(NULL, hv))
    return false;

ho = (hdloutlinerecord)(**hv).variabledata;
oppushoutline(ho);  // Push to outline stack
```

This is the **critical pattern difference** between GUI and headless - we get the outline from the target system, not from a window.

---

## Phase 1: Foundational Verbs (Implement First)

### Priority Order (First 10 Verbs)

| # | Verb | Rationale | Tests | Implementation Complexity |
|---|------|-----------|-------|--------------------------|
| 1 | **op.insert** | Foundation - builds outline structure | 5 | **MEDIUM** - Needs `opinserthandle_ctx()` wrapper |
| 2 | **op.getLineText** | Read content - validates insert worked | 3 | **TRIVIAL** - Direct copy: `copyhandle((**hbarcursor).headstring, &htext)` |
| 3 | **op.setLineText** | Modify content - complete CRUD | 3 | **SIMPLE** - Calls `opsetlinetextverb()` |
| 4 | **op.level** | Query hierarchy - no mutation | 3 | **TRIVIAL** - Read field: `(**hbarcursor).headlevel + 1` |
| 5 | **op.go** | Navigate tree - enables cursor movement | 8 | **SIMPLE** - Calls `opmotionkey(dir, units, false)` |
| 6 | **op.firstSummit** | Go to top - common operation | 2 | **TRIVIAL** - Calls `opmotionkey(flatup, longinfinity, false)` |
| 7 | **op.countSubs** | Count children - validates structure | 3 | **TRIVIAL** - Calls `opcountsubheads(hbarcursor, level)` |
| 8 | **op.countSummits** | Count top-level nodes | 3 | **TRIVIAL** - Calls `opcountatlevel((**ho).hsummit)` |
| 9 | **op.expand** | Show children - state management | 2 | **SIMPLE** - Calls `opexpand(hbarcursor, level, true)` |
| 10 | **op.collapse** | Hide children - completes expand/collapse | 2 | **SIMPLE** - Calls `opcollapse(hbarcursor)` |

---

## Implementation Strategy Per Verb

### 1. op.insert (MEDIUM - Target Pattern Required)

**Signature:** `op.insert(text, direction) -> boolean`

**Implementation:**
```c
case opv_insert: {
    Handle htext;
    tydirection dir;
    hdlexternalvariable htarget;
    hdloutlinevariable hv;
    hdloutlinerecord ho;

    // Get outline from target
    if (!langexternalgettargetexternalvariable(idoutlineprocessor, &htarget)) {
        copystring(BIGSTRING("\pno outline target set"), bserror);
        return false;
    }

    hv = (hdloutlinevariable)htarget;
    if (!opverbinmemory(NULL, hv))
        return false;

    ho = (hdloutlinerecord)(**hv).variabledata;

    // Parse parameters
    if (!getexempttextvalue(hparam1, 1, &htext))
        break;

    flnextparamislast = true;
    if (!getdirectionvalue(hparam1, 2, &dir))
        break;

    // Push outline, insert, pop
    oppushoutline(ho);
    boolean result = opinserthandle_ctx(NULL, htext, dir);
    oppopoutline();

    disposehandle(htext);
    return setbooleanvalue(result, vreturned);
}
```

**Port Strategy:** Adapt from line 3612-3631 in opverbs.c, replace window lookup with target system.

**Tests to Pass:**
- `op.insert - insert first line (down)`
- `op.insert - insert sibling line (down)`
- `op.insert - insert child line (right)`
- `op.insert - insert above (up)`
- `op.insert - insert parent (left)`

---

### 2. op.getLineText (TRIVIAL - Direct Port)

**Signature:** `op.getLineText() -> string`

**Implementation:**
```c
case opv_getlinetext: {
    Handle htext;
    hdloutlinerecord ho;
    hdlheadrecord hbarcursor;

    if (!langcheckparamcount(hparam1, 0))
        break;

    // Get outline + cursor from target (same pattern as insert)
    // ... [target retrieval code] ...

    hbarcursor = (**ho).hbarcursor;

    opwriteeditbuffer();  // Flush edits if any

    if (!copyhandle((**hbarcursor).headstring, &htext))
        break;

    return setheapvalue(htext, stringvaluetype, vreturned);
}
```

**Port Strategy:** Direct port from line 3487-3501 in opverbs.c.

**Tests to Pass:**
- `op.getLineText - read inserted line`
- `op.getLineText - returns string type`
- `op.getLineText - empty line text`

---

### 3-10. Remaining Verbs (TRIVIAL/SIMPLE)

All follow the same pattern:
1. Get outline from target system
2. Push outline to stack (`oppushoutline(ho)`)
3. Call existing op function (e.g., `opmotionkey`, `opcountsubheads`)
4. Pop outline from stack (`oppopoutline()`)
5. Return result

**Implementation complexity is TRIVIAL** because:
- Core op functions already exist in `Common/source/op*.c`
- Only need target system wrapper + parameter parsing
- No new logic required

---

## Phase 2: Hierarchy & Structure (Next 8 Verbs)

| # | Verb | Complexity | Line Reference |
|---|------|-----------|---------------|
| 11 | **op.promote** | SIMPLE | 3704-3712 |
| 12 | **op.demote** | SIMPLE | 3714-3722 |
| 13 | **op.deleteLine** | SIMPLE | 3692-3702 |
| 14 | **op.deleteSubs** | SIMPLE | 3682-3690 |
| 15 | **op.reorg** | SIMPLE | 3663-3680 |
| 16 | **op.subsExpanded** | TRIVIAL | 3602-3610 |
| 17 | **op.sort** | SIMPLE | 3638-3646 |
| 18 | **op.find** | MEDIUM | 3633-3635 (calls opfindverb) |

---

## Phase 3: State Management (Next 10 Verbs)

| # | Verb | Complexity | Notes |
|---|------|-----------|-------|
| 19 | **op.getCursor** | MEDIUM | Needs address-to-path conversion |
| 20 | **op.setCursor** | MEDIUM | Needs path-to-address conversion |
| 21 | **op.getDisplay** | TRIVIAL | Read `(**ho).flinhibitdisplay` |
| 22 | **op.setDisplay** | SIMPLE | Call `opsetdisplayverb()` |
| 23 | **op.getExpansionState** | MEDIUM | Call `opgetexpansionstateverb()` |
| 24 | **op.setExpansionState** | MEDIUM | Call `opsetexpansionstateverb()` |
| 25 | **op.getScrollState** | SIMPLE | Call `opgetscrollstateverb()` |
| 26 | **op.setScrollState** | SIMPLE | Call `opsetscrollstateverb()` |
| 27 | **op.getRefcon** | SIMPLE | Call `opgetrefcon()` |
| 28 | **op.setRefcon** | SIMPLE | Call `opsetrefcon()` |

---

## Phase 4: Advanced Operations (Remaining 17 Verbs)

These require more complex implementation or GUI dependencies:

| Category | Verbs | Complexity |
|----------|-------|-----------|
| **Hoist** | hoist, deHoist | MEDIUM (hoisting not used in headless) |
| **Selection** | getSelection, getSelectedSubOutlines | HIGH (multi-select state) |
| **Outline Ops** | getSubOutline, insertOutline | MEDIUM (subtree extraction) |
| **Dynamic** | setDynamic, getDynamic | TRIVIAL (bit flags) |
| **HTML** | setHtmlFormatting, getHtmlFormatting | TRIVIAL (bit flags) |
| **Prefs** | tabKeyReorg, flatCursorKeys | TRIVIAL (global prefs) |
| **Other** | setModified, getheadNumber, visitAll | SIMPLE |
| **XML** | xmlToOutline, outlineToXml | HIGH (complex, may defer) |

---

## TDD Workflow (Per Verb)

### Step-by-Step Process

**1. Select Next Verb (e.g., `op.insert`)**

**2. Review Tests**
```bash
cd /Users/jake/dev/jsavin/Frontier-op-verb-implementation
grep -A 10 "op.insert" tests/integration/test_cases/op_verbs.yaml
```

**3. Run Test (Expect Failure)**
```bash
cd tests && make test-integration TEST_FILTER="op.insert - insert first line"
# Expected: FAIL with "not implemented" error
```

**4. Implement Verb**
```bash
# Edit tests/headless_op_verbs.c
# Replace stub with implementation (see patterns above)
```

**5. Build & Test**
```bash
cd /Users/jake/dev/jsavin/Frontier-op-verb-implementation
make clean && make
cd tests && make test-integration TEST_FILTER="op.insert"
# Expected: PASS for all op.insert tests
```

**6. Commit**
```bash
git add tests/headless_op_verbs.c
git commit -m "feat: Implement op.insert verb

- Add target-based outline retrieval
- Support down/up/left/right directions
- Pass 5 integration tests

Closes #[issue] (partial - op.insert)"
```

**7. Repeat for Next Verb**

---

## Implementation Batching Strategy

### Batch 1: Foundation (Verbs 1-5) - Single PR
- op.insert
- op.getLineText
- op.setLineText
- op.level
- op.go

**Rationale:** These form a complete CRUD cycle + navigation. Natural unit of work.

**Estimated Time:** 4-6 hours (including testing, debugging target pattern)

---

### Batch 2: Queries & Basic Navigation (Verbs 6-10) - Single PR
- op.firstSummit
- op.countSubs
- op.countSummits
- op.expand
- op.collapse

**Rationale:** All read-only or simple state changes. No complex logic.

**Estimated Time:** 2-3 hours

---

### Batch 3-N: Continue in similar batches of 5-8 verbs

Each batch should be:
- Logically related (hierarchy ops, state management, etc.)
- Completable in 1 session (2-4 hours)
- Independent PR (easier review)

---

## Testing Strategy

### Unit Test Coverage
- Each verb: 2-4 tests (normal case, edge case, error case)
- Total: 77 integration tests already written

### Test Execution
```bash
# Run all op verb tests
cd tests && make test-integration TEST_FILTER="op."

# Run specific verb tests
cd tests && make test-integration TEST_FILTER="op.insert"

# Run single test
cd tests && make test-integration TEST_FILTER="op.insert - insert first line"
```

### Expected Test Results Per Phase

| Phase | Verbs | Tests | Expected Pass Rate |
|-------|-------|-------|-------------------|
| Phase 1 | 10 | 34 | 100% (foundational) |
| Phase 2 | 8 | 15 | 95% (minor edge cases) |
| Phase 3 | 10 | 18 | 90% (state management complexity) |
| Phase 4 | 17 | 10 | 70% (advanced features, may defer some) |

---

## Key Technical Challenges

### Challenge 1: Target System Integration ⚠️

**Problem:** GUI Frontier gets outline from window. Headless must get from target system.

**Solution:**
```c
// Replace this GUI pattern:
if (!langfindtargetwindow(idoutlineprocessor, &targetwindow))
    goto error;

// With this headless pattern:
hdlexternalvariable htarget;
if (!langexternalgettargetexternalvariable(idoutlineprocessor, &htarget))
    goto error;
```

**Implementation Location:** Create helper function in headless_op_verbs.c:
```c
static boolean getoutlinefromtarget(hdloutlinerecord *ho, bigstring bserror) {
    // Common pattern used by all verbs
}
```

---

### Challenge 2: Outline Stack Management

**Problem:** Multiple outline contexts, must push/pop correctly.

**Solution:** Every verb follows this pattern:
```c
oppushoutline(ho);
boolean result = /* operation */;
oppopoutline();
return result;
```

**Critical:** NEVER return early without popping!

---

### Challenge 3: Direction Constants

**Problem:** Tests use `down`, `up`, `left`, `right` as direction values.

**Solution:** These are already defined in `Common/headers/ops.h`:
```c
typedef enum tydirection {
    nodirection,
    up, down, left, right,
    flatup, flatdown,
    pageleft, pageright,
    pageup, pagedown,
    ...
} tydirection;
```

Use `getdirectionvalue(hparam1, paramnum, &dir)` to parse.

---

## Dependencies & Prerequisites

### Already Complete ✅
- Outline data structure (`tyoutlinerecord`, `tyheadrecord`)
- Outline operations (`op.c`, `opops.c`, `opstructure.c`)
- Target system (`target.c`, `langexternal.c`)
- Integration test infrastructure (`tests/integration/`)

### Verified Working ✅
- **Target system with outlines:** `lang.new(outlineType, @workspace.outline); target.set(@workspace.outline)` works
- **Outline type:** `typeof(workspace.outline)` returns `"outl"`
- **Stub dispatch:** `op.insert("test", down)` correctly returns "not implemented"

### Need to Verify
1. **langexternalgettargetexternalvariable()** exists and works
2. **oppushoutline()/oppopoutline()** safe in headless mode
3. **opverbinmemory()** loads outline from database if needed

---

## Critical Files for Implementation

### 1. **/Users/jake/dev/jsavin/Frontier-op-verb-implementation/tests/headless_op_verbs.c**
- **Why:** Stub implementations to replace (line 74-256)
- **What:** Add target retrieval, parameter parsing, call existing op functions
- **Estimated Changes:** ~500 lines (10-15 lines per verb × 45 verbs)

### 2. **/Users/jake/dev/jsavin/Frontier-op-verb-implementation/Common/source/opverbs.c**
- **Why:** Reference implementation for GUI version (line 3278-4200)
- **What:** Copy logic, adapt for target system instead of window
- **Pattern:** Each verb case (lines 3487-3799) is template

### 3. **/Users/jake/dev/jsavin/Frontier-op-verb-implementation/Common/headers/op.h**
- **Why:** Function prototypes for op operations
- **What:** Verify all functions exist (`opinserthandle`, `opcountsubheads`, etc.)

### 4. **/Users/jake/dev/jsavin/Frontier-op-verb-implementation/tests/integration/test_cases/op_verbs.yaml**
- **Why:** 77 integration tests define expected behavior
- **What:** Reference for parameter types, return values, edge cases

### 5. **/Users/jake/dev/jsavin/Frontier-op-verb-implementation/Common/source/langexternal.c**
- **Why:** Target system implementation
- **What:** May need to add `langexternalgettargetexternalvariable()` if missing
- **Fallback:** Use `langfindtargetwindow()` + custom outline retrieval

---

## Success Criteria

### Phase 1 Complete When:
- ✅ 10 foundational verbs implemented
- ✅ 34 integration tests pass
- ✅ Can build outline, navigate, modify content
- ✅ Target pattern proven to work

### Full Implementation Complete When:
- ✅ 45 kernel verbs implemented
- ✅ 77 integration tests pass (100%)
- ✅ All verb categories covered
- ✅ No "not implemented" errors

---

## Risk Assessment & Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| Target system doesn't work for outlines | LOW | HIGH | ✅ VERIFIED WORKING |
| Outline stack corruption | LOW | HIGH | Add assertions, test push/pop balance |
| Direction parsing fails | LOW | MEDIUM | Test with simple `op.go(down, 1)` first |
| Tests don't match implementation | LOW | MEDIUM | Review test expectations vs. opverbs.c behavior |

---

## Timeline Estimate

**Estimated Timeline:**
- Phase 1 (10 verbs): 1-2 days
- Phase 2 (8 verbs): 1 day
- Phase 3 (10 verbs): 1-2 days
- Phase 4 (17 verbs): 2-3 days

**Total: 5-8 days for full 45 verb implementation**

The TDD approach ensures correctness at each step, and the batching strategy allows incremental progress with reviewable PRs.

---

## Next Immediate Actions

1. ✅ **Verify target system works for outlines** - DONE
   - `lang.new(outlineType, @workspace.outline)` creates outline
   - `target.set(@workspace.outline)` sets target
   - `typeof(workspace.outline)` returns `"outl"`
   - `op.insert("test", down)` returns "not implemented" as expected

2. **Implement helper function `getoutlinefromtarget()`**
   - Encapsulates target→outline retrieval pattern
   - Used by all 45 verbs
   - Reduces code duplication

3. **Implement op.insert (first verb)**
   - Most complex due to target pattern
   - Once working, others are trivial

4. **Implement op.getLineText (validate insert)**
   - Confirms insert worked
   - Tests end-to-end flow

5. **Batch implement remaining Phase 1 verbs (3-10)**
   - All use same pattern as insert/getLineText
   - Should be rapid once pattern proven

---

## Conclusion

This implementation is **HIGH IMPACT, MEDIUM EFFORT**:

- **High Impact:** Enables 45 outline manipulation verbs, unlocking outline-based workflows
- **Medium Effort:** Core functions exist, just need target system wrapper + parameter parsing
- **Low Risk:** 77 tests already written, clear reference implementation exists, target system verified working

The TDD approach with comprehensive tests ensures correctness, and the phased batching strategy allows for incremental progress with focused, reviewable PRs.
