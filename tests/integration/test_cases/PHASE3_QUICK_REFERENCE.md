# Phase 3 Op Verbs - Quick Test Reference

## Test File Location
`tests/integration/test_cases/op_verbs.yaml`

## Phase 3 Test Range
**Lines 2245-2964** (720 lines, 72 tests)

## Quick Stats
- **Total Phase 3 Tests**: 72
- **Verbs Covered**: 10
- **Test Categories**: 11
- **Expected Pass Rate**: 95-100% after implementation

## Verbs Covered

| Verb | Complexity | Tests | Key Tests |
|------|-----------|-------|-----------|
| op.getCursor | MEDIUM | 12 | Persistence, deleted nodes, deep nesting |
| op.setCursor | MEDIUM | 13 | Round-trip, cross-outline, type safety |
| op.getDisplay | TRIVIAL | 7 | State query, independence |
| op.setDisplay | SIMPLE | 9 | State management, bulk ops |
| op.getExpansionState | MEDIUM | 9 | Empty outline, structural changes |
| op.setExpansionState | MEDIUM | 10 | Restoration, cross-outline validation |
| op.getScrollState | SIMPLE | 7 | State query, independence |
| op.setScrollState | SIMPLE | 8 | State persistence, cross-outline |
| op.getRefcon | SIMPLE | 9 | Boundaries, persistence, independence |
| op.setRefcon | SIMPLE | 11 | 32-bit values, hierarchy survival |

## Test Categories

| Category | Tests | Purpose |
|----------|-------|---------|
| Advanced Cursor | 4 | Persistence, deleted nodes, deep nesting |
| Display Edge Cases | 4 | Idempotency, independence |
| Expansion Edge Cases | 4 | Cross-outline, structural changes |
| Scroll Edge Cases | 4 | Independence, persistence |
| Refcon Edge Cases | 6 | Boundaries, persistence, hierarchy |
| Workflows | 4 | Real-world scenarios |
| Stress Tests | 3 | Performance, scale |
| Error Recovery | 3 | Graceful failure |
| Type Safety | 5 | Parameter validation |
| Return Types | 10 | Output validation |
| Basic Tests | 25 | Already existed (lines 1260-1931) |

## Run Commands

### All Phase 3 Tests
```bash
cd tests && make test-integration TEST_FILTER="Phase 3"
```

### By Verb
```bash
cd tests && make test-integration TEST_FILTER="op.getCursor"
cd tests && make test-integration TEST_FILTER="op.setCursor"
cd tests && make test-integration TEST_FILTER="op.getDisplay"
cd tests && make test-integration TEST_FILTER="op.setDisplay"
cd tests && make test-integration TEST_FILTER="op.getExpansionState"
cd tests && make test-integration TEST_FILTER="op.setExpansionState"
cd tests && make test-integration TEST_FILTER="op.getScrollState"
cd tests && make test-integration TEST_FILTER="op.setScrollState"
cd tests && make test-integration TEST_FILTER="op.getRefcon"
cd tests && make test-integration TEST_FILTER="op.setRefcon"
```

### By Category
```bash
cd tests && make test-integration TEST_FILTER="cursor"
cd tests && make test-integration TEST_FILTER="display"
cd tests && make test-integration TEST_FILTER="expansion"
cd tests && make test-integration TEST_FILTER="scroll"
cd tests && make test-integration TEST_FILTER="refcon"
cd tests && make test-integration TEST_FILTER="workflow"
cd tests && make test-integration TEST_FILTER="stress test"
cd tests && make test-integration TEST_FILTER="error recovery"
cd tests && make test-integration TEST_FILTER="type safety"
cd tests && make test-integration TEST_FILTER="return type"
```

## Implementation Order

### Batch 1: Trivial (2 verbs)
1. op.getDisplay
2. op.getRefcon

### Batch 2: Simple (4 verbs)
3. op.setDisplay
4. op.setRefcon
5. op.getScrollState
6. op.setScrollState

### Batch 3: Medium (4 verbs)
7. op.getCursor
8. op.setCursor
9. op.getExpansionState
10. op.setExpansionState

## Key Test Patterns

### Pattern 1: State Persistence
Test that saved state remains valid after modifications.
Example: `op.getCursor - cursor persists during outline modification`

### Pattern 2: Cross-Outline Independence
Test that state from one outline doesn't affect another.
Example: `op.getDisplay - independent across multiple outlines`

### Pattern 3: Boundary Values
Test 32-bit integer boundaries (INT32_MAX/MIN).
Example: `op.setRefcon - large positive value (32-bit boundary)`

### Pattern 4: Type Safety
Test that verbs reject wrong parameter types.
Example: `type safety - setCursor requires address type`

### Pattern 5: Return Type Validation
Test that verbs return correct types via typeof().
Example: `return type - getCursor returns address`

## Expected Test Results

### Before Implementation (TDD Red Phase)
- **Pass Rate**: 0%
- **Expected**: All 72 tests FAIL with "not implemented"
- **This is CORRECT**: Tests define spec before code

### After Implementation (TDD Green Phase)
- **Pass Rate**: 95-100%
- **Expected**: 68+ of 72 tests PASS
- **Acceptable**: 0-4 failures requiring discussion

## Common Implementation Pattern

```c
case opv_VERBNAME: {
    hdloutlinerecord ho;
    hdlheadrecord hbarcursor;

    // 1. Validate parameters
    if (!langcheckparamcount(hparam1, COUNT))
        break;

    // 2. Get outline from target
    if (!getoutlinefromtarget(&ho, bserror))
        break;

    // 3. Get cursor (if needed)
    hbarcursor = (**ho).hbarcursor;

    // 4. Perform operation
    // ... implementation ...

    // 5. Return result
    return setXXXvalue(result, vreturned);
}
```

## Critical Implementation Notes

1. **Target System**: Use `langexternalgettargetexternalvariable()` to get outline
2. **Outline Stack**: Push/pop with `oppushoutline()`/`oppopoutline()`
3. **Parameter Count**: Always validate with `langcheckparamcount()` first
4. **Type Safety**: Use type-specific getters (`getbooleanvalue`, `getaddressvalue`, etc.)
5. **Error Messages**: Follow "Can't do X because Y" pattern

## Success Criteria

- ✅ All 10 verbs implemented
- ✅ 68+ tests pass (95%+ rate)
- ✅ All type safety tests pass
- ✅ All return type tests pass
- ✅ 3+ workflow tests pass
- ✅ 2+ stress tests pass

## Documentation Files

- **PHASE3_TEST_SUMMARY.md** - Comprehensive test documentation
- **PHASE3_TESTS_DELIVERED.md** - Delivery summary and statistics
- **PHASE3_QUICK_REFERENCE.md** - This file (quick lookup)

## Implementation Files

- **tests/headless_op_verbs.c** - Where implementations go (lines 74-256 are stubs)
- **Common/source/opverbs.c** - Reference implementation (GUI version, lines 3278-4200)
- **Common/headers/op.h** - Function prototypes

## Planning References

- **planning/phase3/op_verb_implementation_plan.md** - Implementation strategy
- **planning/phase3/processor_audits/op.md** - Verb specifications

## Test Categories at a Glance

| Lines | Category | Tests | Focus |
|-------|----------|-------|-------|
| 2257-2317 | Advanced Cursor | 4 | Persistence, edge cases |
| 2319-2371 | Display Edge | 4 | Idempotency, independence |
| 2373-2444 | Expansion Edge | 4 | Cross-outline, structural |
| 2446-2501 | Scroll Edge | 4 | Independence, persistence |
| 2503-2598 | Refcon Edge | 6 | Boundaries, hierarchy |
| 2600-2690 | Workflows | 4 | Real-world scenarios |
| 2692-2761 | Stress | 3 | Performance, scale |
| 2763-2794 | Error Recovery | 3 | Graceful failure |
| 2796-2849 | Type Safety | 5 | Parameter validation |
| 2851-2964 | Return Types | 10 | Output validation |

## Noteworthy Tests

### Most Complex Test
`workflow - save all state before bulk operation and restore`
- Tests cursor + expansion + scroll state in one scenario
- Real-world use case for state management

### Best Boundary Test
`op.setRefcon - large positive value (32-bit boundary)`
- Tests INT32_MAX (2147483647)
- Validates 32-bit refcon assumption

### Best Stress Test
`stress test - many refcon assignments`
- 100 nodes with unique refcon values
- Validates performance and correctness at scale

### Best Error Test
`op.setCursor - cursor to deleted node should fail gracefully`
- Tests error handling when cursor points to deleted node
- Validates graceful failure pattern

## Quick Verification

### Verify Tests Are Written
```bash
grep -c "Phase 3:" tests/integration/test_cases/op_verbs.yaml
# Should show multiple matches
```

### Verify YAML Validity
```bash
python3 -c "import yaml; yaml.safe_load(open('tests/integration/test_cases/op_verbs.yaml'))"
# Should succeed with no errors
```

### Verify Test Count
```bash
awk '/# Phase 3: Additional State Management Tests/,0' tests/integration/test_cases/op_verbs.yaml | grep -c "^  - name:"
# Should show 47 (additional tests beyond basic ones)
```

## TDD Workflow

1. **Red**: Run tests, verify all fail (not implemented)
2. **Green**: Implement verb, tests pass
3. **Refactor**: Clean up code, tests still pass
4. **Repeat**: Next verb

## Ready to Implement!

Everything is in place to begin TDD implementation:
- ✅ Tests written (72 new tests)
- ✅ Test file valid (YAML validated)
- ✅ Documentation complete (3 docs)
- ✅ Implementation guidance provided
- ✅ Success criteria defined

Start with Batch 1 (trivial verbs) and work through each batch, running tests after each implementation.
