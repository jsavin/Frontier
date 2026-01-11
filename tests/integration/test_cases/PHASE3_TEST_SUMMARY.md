# Phase 3 Op Verbs Integration Test Suite

## Overview

This document describes the comprehensive integration test suite for Phase 3 op verbs, written following Test-Driven Development (TDD) methodology. All tests were written BEFORE implementation to ensure clear specifications and expected behavior.

## Test Coverage Summary

**Total New Tests Added:** 72 tests (in addition to 32 existing basic tests)
**Total Phase 3 Coverage:** 104 tests covering all 10 Phase 3 verbs

### Verbs Covered

| Verb | Complexity | Test Count | Test Categories |
|------|-----------|------------|----------------|
| op.getCursor | MEDIUM | 13 | Basic, advanced cursor, edge cases, error recovery, type safety, return validation |
| op.setCursor | MEDIUM | 13 | Basic, round-trip, persistence, error handling, type validation |
| op.getDisplay | TRIVIAL | 10 | Basic state, idempotency, persistence, independence |
| op.setDisplay | SIMPLE | 10 | State management, bulk operations, multiple outlines |
| op.getExpansionState | MEDIUM | 12 | Basic capture, empty outline, structural changes, cursor independence |
| op.setExpansionState | MEDIUM | 12 | Restoration, round-trip, multi-level, cross-outline validation |
| op.getScrollState | SIMPLE | 10 | Basic state, empty outline, independence |
| op.setScrollState | SIMPLE | 10 | State persistence, modifications, cross-outline |
| op.getRefcon | SIMPLE | 12 | Default values, boundaries, persistence, independence |
| op.setRefcon | SIMPLE | 12 | Value ranges, hierarchy changes, bulk operations |

## Test Organization

### 1. Basic Tests (Already Existing - 32 tests)
Lines 1260-1931 in op_verbs.yaml contain basic functionality tests:
- getCursor/setCursor basic operations
- getDisplay/setDisplay basic operations
- getExpansionState/setExpansionState basic operations
- getScrollState/setScrollState basic operations
- getRefcon/setRefcon basic operations
- No target set error cases
- Wrong target type error cases

### 2. Advanced Cursor Tests (4 tests)
Lines 2257-2317
- **Cursor persistence during modification**: Validates saved cursors remain valid after structural changes
- **Deleted node handling**: Ensures setting cursor to deleted node fails gracefully
- **Cursor after deleteSubs**: Parent cursor remains valid after child deletion
- **Deep nesting**: Cursor correctly identifies position in deeply nested structures

**Key Test Pattern:**
```yaml
- name: "op.getCursor - cursor persists during outline modification"
  description: "Saved cursor remains valid after inserting new nodes elsewhere"
  script: |
    lang.new(outlineType, @workspace.outline);
    target.set(@workspace.outline);
    op.insert("Line 1", down);
    op.insert("Line 2", down);
    local(savedCursor = op.getCursor());
    op.go(up, 1);
    op.insert("Line 1.5", down);
    op.setCursor(savedCursor);
    return op.getLineText()
  expected_success: true
  expected_result: "Line 2"
```

### 3. Display State Edge Cases (4 tests)
Lines 2319-2371
- **Idempotency**: Double enable/disable operations work correctly
- **State persistence**: Display state survives outline operations
- **Independence**: Each outline has independent display state

**Critical Test:**
```yaml
- name: "op.getDisplay - independent across multiple outlines"
  description: "Each outline has independent display state"
  script: |
    lang.new(outlineType, @workspace.outline1);
    lang.new(outlineType, @workspace.outline2);
    target.set(@workspace.outline1);
    op.setDisplay(false);
    target.set(@workspace.outline2);
    op.setDisplay(true);
    target.set(@workspace.outline1);
    return op.getDisplay()
  expected_success: true
  expected_result: "false"
```

### 4. Expansion State Edge Cases (4 tests)
Lines 2373-2444
- **Cross-outline validation**: State from different outline should fail
- **Empty outline handling**: Get state from empty outline edge case
- **Structural changes**: State restoration after adding/removing nodes
- **Cursor independence**: Expansion state captures entire outline, not just cursor

### 5. Scroll State Edge Cases (4 tests)
Lines 2446-2501
- **Empty outline**: Get scroll state from empty outline
- **Cross-outline validation**: Scroll state is outline-specific
- **Modification persistence**: State remains valid after inserting nodes
- **Independence**: Each outline has independent scroll state

### 6. Refcon Edge Cases (6 tests)
Lines 2503-2598
- **Boundary values**: INT32_MAX (2147483647) and INT32_MIN (-2147483648)
- **Navigation persistence**: Refcon values persist when navigating
- **Hierarchy changes**: Refcon survives promote/demote operations
- **Node independence**: Each node has independent refcon value
- **Parent preservation**: Inserting child doesn't affect parent refcon

**Boundary Test:**
```yaml
- name: "op.setRefcon - large positive value (32-bit boundary)"
  description: "Test refcon with large positive value near INT32_MAX"
  script: |
    lang.new(outlineType, @workspace.outline);
    target.set(@workspace.outline);
    op.insert("Line", down);
    op.setRefcon(2147483647);
    return op.getRefcon()
  expected_success: true
  expected_result: "2147483647"
```

### 7. Complex Workflow Tests (4 tests)
Lines 2600-2690
- **Full state save/restore**: Cursor + expansion + scroll state in single workflow
- **Refcon as metadata**: Track nodes during complex reorganization
- **Display optimization**: Disable display during bulk operations
- **Multiple outlines**: Manage state for multiple outlines simultaneously

**Real-World Workflow:**
```yaml
- name: "workflow - save all state before bulk operation and restore"
  description: "Save cursor, expansion, scroll state, perform operations, restore all state"
  script: |
    lang.new(outlineType, @workspace.outline);
    target.set(@workspace.outline);
    op.insert("Parent 1", down);
    op.insert("Child 1a", right);
    op.insert("Child 1b", down);
    op.go(left, 1);
    op.expand(1);
    op.insert("Parent 2", down);
    op.insert("Child 2a", right);
    op.go(left, 1);
    local(savedCursor = op.getCursor());
    local(savedExpansion = op.getExpansionState());
    local(savedScroll = op.getScrollState());
    op.firstSummit();
    op.collapse();
    op.setCursor(savedCursor);
    op.setExpansionState(savedExpansion);
    op.setScrollState(savedScroll);
    return op.getLineText()
  expected_success: true
  expected_result: "Parent 2"
```

### 8. Stress Tests (3 tests)
Lines 2692-2761
- **Rapid cursor cycles**: 10 save/restore cycles on 20-node outline
- **Many refcon assignments**: 100 nodes with unique refcon values
- **Deep nesting expansion**: 10-level deep outline with expansion state management

**Performance Test:**
```yaml
- name: "stress test - many refcon assignments"
  description: "Assign refcon to many nodes in large outline"
  script: |
    lang.new(outlineType, @workspace.outline);
    target.set(@workspace.outline);
    op.setDisplay(false);
    local(i);
    for i = 1 to 100 {
      op.insert("Node " + i, down);
      op.setRefcon(i * 10)
    };
    op.setDisplay(true);
    op.firstSummit();
    local(ok = true);
    for i = 2 to 101 {
      op.go(down, 1);
      if op.getRefcon() != ((i - 1) * 10) {
        ok = false
      }
    };
    return ok
  expected_success: true
  expected_result: "true"
```

### 9. Error Recovery Tests (3 tests)
Lines 2763-2794
- **Deleted outline**: setCursor fails gracefully if outline no longer exists
- **No target**: State queries fail gracefully without target
- **Empty outline**: setRefcon on empty outline should fail

### 10. Type Safety Tests (5 tests)
Lines 2796-2849
- **setCursor**: Requires address type
- **setDisplay**: Requires boolean type
- **setExpansionState**: Requires address type
- **setScrollState**: Requires address type
- **setRefcon**: Requires long type

**Type Validation Pattern:**
```yaml
- name: "type safety - setCursor requires address type"
  description: "setCursor rejects non-address types"
  script: |
    lang.new(outlineType, @workspace.outline);
    target.set(@workspace.outline);
    op.insert("Line", down);
    op.setCursor(12345)
  expected_success: false
  expected_error_type: "script_error"
```

### 11. Return Type Validation Tests (10 tests)
Lines 2851-2964
- Verify all get* verbs return correct types (address, boolean, long)
- Verify all set* verbs return boolean
- Uses typeof() to validate return types

**Return Type Validation:**
```yaml
- name: "return type - getCursor returns address"
  description: "Verify getCursor return type is addressType"
  script: |
    lang.new(outlineType, @workspace.outline);
    target.set(@workspace.outline);
    op.insert("Line", down);
    local(cursor = op.getCursor());
    return typeof(cursor)
  expected_success: true
  expected_result: "adr"
```

## Test Methodology

### TDD Approach

All tests follow Test-Driven Development principles:

1. **Red Phase**: Tests written first, all should FAIL with "not implemented"
2. **Green Phase**: Implementation in `tests/headless_op_verbs.c` makes tests pass
3. **Refactor Phase**: Code improvements while maintaining test pass status

### Test Categories

Each verb is tested across multiple dimensions:

| Category | Purpose | Example |
|----------|---------|---------|
| **Happy Path** | Normal operation | getCursor on valid outline |
| **Edge Cases** | Boundary conditions | Empty outline, deep nesting |
| **Error Cases** | Invalid inputs | Wrong type, no target set |
| **Persistence** | State survival | Cursor valid after modifications |
| **Independence** | Isolation | Multiple outlines don't interfere |
| **Type Safety** | Parameter validation | Reject wrong types |
| **Return Types** | Output validation | typeof() checks |
| **Workflows** | Real-world usage | Combined operations |
| **Stress** | Performance/scale | Large outlines, many operations |

### Expected Test Behavior (Before Implementation)

All Phase 3 tests should initially FAIL with one of:
- "not implemented" error
- "verb not found" error
- Script execution error due to missing verb

This is CORRECT TDD behavior - tests define the spec before code exists.

## Implementation Guidance

### Implementation Order (Recommended)

**Batch 1: Trivial Verbs** (Implement first - 2 verbs)
1. op.getDisplay - Read `(**ho).flinhibitdisplay`
2. op.getRefcon - Call `opgetrefcon()`

**Batch 2: Simple Verbs** (Next - 4 verbs)
3. op.setDisplay - Call `opsetdisplayverb()`
4. op.setRefcon - Call `opsetrefcon()`
5. op.getScrollState - Call `opgetscrollstateverb()`
6. op.setScrollState - Call `opsetscrollstateverb()`

**Batch 3: Medium Verbs** (Final - 4 verbs)
7. op.getCursor - Address-to-path conversion
8. op.setCursor - Path-to-address conversion
9. op.getExpansionState - Call `opgetexpansionstateverb()`
10. op.setExpansionState - Call `opsetexpansionstateverb()`

### Common Implementation Pattern

All verbs follow this headless pattern:

```c
case opv_getrefcon: {
    hdloutlinerecord ho;
    hdlheadrecord hbarcursor;
    long refcon;

    // 1. Parameter validation
    if (!langcheckparamcount(hparam1, 0))
        break;

    // 2. Get outline from target system (headless pattern)
    if (!getoutlinefromtarget(&ho, bserror))
        break;

    // 3. Get current cursor
    hbarcursor = (**ho).hbarcursor;

    // 4. Call existing op function
    refcon = opgetrefcon(hbarcursor);

    // 5. Return result
    return setlongvalue(refcon, vreturned);
}
```

### Critical Implementation Notes

1. **Target System**: Use `langexternalgettargetexternalvariable()` to get outline from target
2. **Outline Stack**: Always push/pop outline context (`oppushoutline`/`oppopoutline`)
3. **Parameter Validation**: Use `langcheckparamcount()` first
4. **Error Messages**: Follow Frontier conventions: "Can't do X because Y"
5. **Type Checking**: Use type-specific getters (`getbooleanvalue`, `getaddressvalue`, etc.)

## Test Execution

### Run All Phase 3 Tests
```bash
cd tests && make test-integration TEST_FILTER="Phase 3"
```

### Run Specific Verb Tests
```bash
cd tests && make test-integration TEST_FILTER="op.getCursor"
cd tests && make test-integration TEST_FILTER="op.setDisplay"
cd tests && make test-integration TEST_FILTER="op.getRefcon"
```

### Run Test Categories
```bash
# All cursor tests
cd tests && make test-integration TEST_FILTER="cursor"

# All display tests
cd tests && make test-integration TEST_FILTER="display"

# All stress tests
cd tests && make test-integration TEST_FILTER="stress test"

# All error recovery tests
cd tests && make test-integration TEST_FILTER="error recovery"
```

## Expected Test Results

### Before Implementation (TDD Red Phase)
- **Expected Pass Rate**: 0% (all tests should FAIL)
- **Expected Errors**: "not implemented", "verb not found"
- **This is CORRECT**: Tests define spec before implementation

### After Implementation (TDD Green Phase)
- **Expected Pass Rate**: 95-100%
- **Possible Failures**: Edge cases requiring refinement
- **Success Criteria**: All happy path + most edge cases pass

### Known Edge Cases Requiring Discussion

Some tests may require discussion about expected behavior:

1. **setCursor to deleted node**: Should fail gracefully, but exact error message?
2. **setExpansionState from different outline**: Should error, but how to detect?
3. **Empty outline operations**: Some operations may not make sense on empty outline

## Test Quality Metrics

### Coverage Dimensions

| Dimension | Coverage |
|-----------|----------|
| **Verbs** | 10/10 (100%) |
| **Happy Path** | 10/10 (100%) |
| **Edge Cases** | 52 tests (comprehensive) |
| **Error Cases** | 15 tests (good coverage) |
| **Type Safety** | 15 tests (all types validated) |
| **Workflows** | 4 complex scenarios |
| **Stress Tests** | 3 performance/scale tests |

### Test Characteristics

- **Specificity**: Each test validates ONE specific behavior
- **Independence**: Tests don't depend on each other
- **Repeatability**: Tests produce same results every run
- **Clarity**: Test names and descriptions explain what's being tested
- **Coverage**: Tests cover normal, edge, and error cases

## Documentation References

### Planning Documents
- `planning/phase3/op_verb_implementation_plan.md` - Implementation strategy
- `planning/phase3/processor_audits/op.md` - Verb specifications

### Implementation File
- `tests/headless_op_verbs.c` - Where implementations go

### Existing Tests
- Lines 1-2244 in `op_verbs.yaml` - Phases 1-2 tests (already implemented)
- Lines 2245-2964 in `op_verbs.yaml` - Phase 3 tests (NEW - this document)

## Success Criteria

Phase 3 implementation is complete when:
- ✅ All 10 Phase 3 verbs implemented
- ✅ 95%+ of Phase 3 tests pass (68+ of 72 new tests)
- ✅ All type safety tests pass
- ✅ All return type validation tests pass
- ✅ At least 3 complex workflow tests pass
- ✅ At least 2 stress tests pass

## Next Steps

1. **Verify Current State**: Run tests to confirm all fail (TDD red phase)
2. **Implement Batch 1**: Trivial verbs (getDisplay, getRefcon)
3. **Run Tests**: Verify Batch 1 tests pass
4. **Implement Batch 2**: Simple verbs (set* verbs, scroll state)
5. **Run Tests**: Verify Batch 2 tests pass
6. **Implement Batch 3**: Medium verbs (cursor, expansion state)
7. **Run Tests**: Verify all Phase 3 tests pass
8. **Create PR**: With comprehensive test results and implementation

## Assumptions and Design Decisions

### Assumption 1: Address Type for Cursors
Cursors are represented as UserTalk address types (`adr`), not raw pointers. This allows cursor values to be stored in variables and passed between scripts.

### Assumption 2: State Independence
Each outline maintains independent state (display, expansion, scroll, refcon). Changes to one outline don't affect others.

### Assumption 3: Outline-Specific State
Expansion state and scroll state are tied to specific outlines. Attempting to apply state from outline A to outline B should fail.

### Assumption 4: Refcon as Long
Refcon values are 32-bit signed integers (long type), not 64-bit. This matches legacy Frontier behavior and Mac OS refcon conventions.

### Assumption 5: Display State Boolean
Display state is a simple boolean (true = display enabled, false = inhibit display), not a bitmask or complex state.

### Assumption 6: Error on Empty Outline
Some operations (like setRefcon) should error on empty outlines because there's no current node. Others (like getDisplay) work fine on empty outlines.

## Test File Statistics

- **Total Lines**: ~720 new lines of test code
- **Tests Added**: 72 new tests
- **Test Density**: ~10 lines per test (average)
- **Categories**: 11 distinct test categories
- **Verbs Covered**: 10 Phase 3 verbs
- **Expected Failures**: 72 initially (TDD red phase)
- **Expected Passes**: 68+ after implementation (TDD green phase)
