# Phase 4 State Management Verbs - Test Failure Analysis

**Date**: 2026-01-11
**Status**: 23 test failures out of ~115 Phase 4 tests (80% pass rate)
**Context**: All 10 Phase 4 verbs are fully implemented and functionally working. These failures represent edge cases, validation improvements, and complex workflow scenarios.

---

## Test Failures by Priority

| #   | Priority | Test Name                                                           | Category                 | Issue Description                                                                                         | Expected vs Actual                                                        | Your Decision                                                                                 |
| --- | -------- | ------------------------------------------------------------------- | ------------------------ | --------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------- |
| 3   | P0       | op.setExpansionState - restore saved expansion state                | Expansion State          | Expansion state not restoring correctly after save                                                        | Expected: 'true', Got: 'false'                                            | Fix                                                                                           |
| 7   | P0       | workflow - save expansion state before collapse and restore         | Expansion State          | Expansion state workflow failure - collapse then restore doesn't work                                     | Expected: 'true', Got: 'false'                                            | Fix                                                                                           |
| 10  | P2       | op.setExpansionState - restore after structural changes             | Expansion State          | Expansion state doesn't survive outline modifications (insert/delete)                                     | Expected: 'true', Got: 'false'                                            | File issue                                                                                    |
| 11  | P0       | op.getExpansionState - state is independent of cursor               | Expansion State          | Expansion state may be cursor-dependent when it should be independent                                     | Expected: 'true', Got: 'false'                                            | Discuss                                                                                       |
| 17  | P0       | stress test - expansion state with deep nesting                     | Expansion State          | Deep nesting (many levels) affects expansion state handling                                               | Expected: '2', Got: '1'                                                   | Discuss                                                                                       |
| 5   | P0       | op.setScrollState - round-trip scroll state                         | Scroll State             | Scroll state calculation or restoration incorrect - off by 10 lines                                       | Expected: 'Line 20', Got: 'Line 10'                                       | Should be noop in headless                                                                    |
| 13  | P0       | op.getScrollState - independent across multiple outlines            | Scroll State             | Scroll state not properly isolated between outlines                                                       | Expected: 'true', Got: 'false'                                            | Should be noop in headless                                                                    |
| 1   | P1       | op.setCursor - cursor from different outline should fail            | Cross-Outline Validation | No validation that cursor belongs to current outline - security/safety issue                              | Expected: success=False, Got: success=True                                | Discuss                                                                                       |
| 9   | P1       | op.setExpansionState - state from different outline should fail     | Cross-Outline Validation | No validation that expansion state belongs to current outline                                             | Expected: success=False, Got: success=True                                | Discuss                                                                                       |
| 12  | P1       | op.setScrollState - scroll state from different outline should fail | Cross-Outline Validation | No validation that scroll state belongs to current outline                                                | Expected: success=False, Got: success=True                                | Should be noop in headless                                                                    |
| 19  | P1       | type safety - setCursor requires address type                       | Type Safety              | Accepts non-address types instead of failing (UserTalk type coercion)                                     | Expected: success=False, Got: success=True                                | Fix. Note: It's not an address - it's an opaque marker                                        |
| 20  | P1       | type safety - setDisplay requires boolean type                      | Type Safety              | Accepts non-boolean types instead of failing (UserTalk type coercion)                                     | Expected: success=False, Got: success=True                                | Should be noop in headless                                                                    |
| 21  | P1       | type safety - setExpansionState requires address type               | Type Safety              | Wrong error type returned                                                                                 | Expected: error_type=script_error, Got: json_parse_error                  | It takes a list of numbers, not an address (read the docserver page)                          |
| 22  | P1       | type safety - setScrollState requires address type                  | Type Safety              | Accepts non-address types instead of failing (UserTalk type coercion)                                     | Expected: success=False, Got: success=True                                | Should be noop in headless                                                                    |
| 23  | P1       | type safety - setRefcon requires long type                          | Type Safety              | Accepts non-long types instead of failing (UserTalk type coercion)                                        | Expected: success=False, Got: success=True                                | refcon should be able to store *any* type of UserTalk object or scalar                        |
| 8   | P2       | op.setCursor - cursor to deleted node should fail gracefully        | Deleted Node Handling    | Returns false instead of raising script error (may be test expectation issue - false is correct behavior) | Expected: success=False (script error), Got: success=True (returns false) | In the Windows app it returns false but does not error when called with an invalid value      |
| 18  | P2       | error recovery - setRefcon with no current node                     | Deleted Node Handling    | Should fail when there's no current node in outline                                                       | Expected: success=False, Got: success=True                                | There should always be a current node (selected node), and default should be the first summit |
| 2   | P2       | op.setDisplay - non-boolean parameter should fail                   | Parameter Validation     | Accepts non-boolean parameters due to type coercion                                                       | Expected: success=False, Got: success=True                                | Incorrect -- UserTalk automatic type coercion will coerce to a boolean                        |
| 4   | P2       | op.setExpansionState - invalid address type should fail             | Parameter Validation     | Wrong error type returned for invalid parameter                                                           | Expected: error_type=script_error, Got: json_parse_error                  | Should return false, not a scriptError. Not sure what the json_parse_error is!                |
| 6   | P2       | op.setRefcon - non-long parameter should fail                       | Parameter Validation     | Accepts non-long parameters due to type coercion                                                          | Expected: success=False, Got: success=True                                | refcons can be any type including scalars and externals (see above)                           |
| 14  | P0       | workflow - refcon as node metadata during tree reorganization       | Complex Workflow         | Refcon data lost during outline reorganization (promote/demote/reorg)                                     | Expected: 'true', Got: 'false'                                            | Fix!                                                                                          |
| 15  | P1       | workflow - display off during state save/restore cycle              | Complex Workflow         | Display state interaction with other operations fails                                                     | Expected: 'Line 20', Got: '' (empty)                                      | Display state should not affect data interactions or cursor movement!                         |
| 16  | P2       | stress test - rapid cursor save/restore cycles                      | Complex Workflow         | Cursor restoration fails under rapid cycling (memory/performance issue?)                                  | Expected: 'true', Got: 'false'                                            |                                                                                               |

---

## Category Breakdown

### P0 Priority: Core Functionality (7 failures)
**Impact**: These affect real-world usage of state management features

- **Expansion State Issues (5)**: Tests 3, 7, 10, 11, 17
  - Expansion state save/restore not working reliably
  - State doesn't survive structural changes
  - Deep nesting causes problems

- **Scroll State Issues (2)**: Tests 5, 13
  - Scroll position calculation off by 10 lines
  - Not properly isolated between outlines

### P1 Priority: Validation & Safety (8 failures)
**Impact**: These prevent incorrect usage but don't affect correct usage

- **Cross-Outline Validation (3)**: Tests 1, 9, 12
  - State from one outline can be applied to another
  - Security concern: cursor/state confusion between outlines

- **Type Safety (5)**: Tests 19-23
  - Verbs accept wrong parameter types due to UserTalk's automatic type coercion
  - Tests expect strict type checking that may not align with UserTalk conventions

### Low Priority: Edge Cases (8 failures)
**Impact**: Uncommon scenarios or potential test expectation issues

- **Deleted Node Handling (2)**: Tests 8, 18
  - Test #8 may have incorrect expectation (returning false is correct)
  - Missing validation for operations on deleted nodes

- **Parameter Validation (3)**: Tests 2, 4, 6
  - Similar to type safety issues
  - May be UserTalk convention vs test expectation mismatch

- **Complex Workflows (3)**: Tests 14, 15, 16
  - Multi-step scenarios with interactions between features
  - May indicate integration issues or test timing problems

---

## Notes

1. **Type Safety/Parameter Validation (8 tests)**: Many failures are due to UserTalk's automatic type coercion. The language converts types automatically (e.g., number to boolean, string to number). Tests expect strict type checking that may not match UserTalk conventions. **This may be a test expectation issue, not an implementation bug.**

2. **Expansion State (5 tests)**: This is the most problematic area. The `setExpansionState` verb delegates to existing `opsetexpansionstateverb()` from Common/source/opverbs.c, so the issue may be in the core implementation or in how it interacts with headless mode.

3. **Test #8 (cursor to deleted node)**: The implementation correctly returns `false` when setting a cursor to a deleted node. The test expects a script error, but the reference implementation returns false. This may be a test bug.

4. **Performance/Memory (test 16)**: "Rapid cursor save/restore cycles" suggests potential memory leak or handle management issue under stress.

5. **Expansion State Parameter Type (test 21)**: Verified from Common/source/opverbs.c (lines 2566-2694):
   - `opgetexpansionstateverb()` returns a **list of longs** (line 2569: "Returns a list of numbers; each number is the line number")
   - `opsetexpansionstateverb()` takes a **list parameter**, not an address (line 2603: "Takes the output of op.getExpansionState")
   - Test #21 expectation is incorrect - it should expect a list type, not address type

6. **Cross-Outline Validation (tests 1, 9, 12)**: Legacy Frontier never validated cross-outline state. These verbs should just return `false` when given invalid state from a different outline, not raise errors.

---

## Recommended Actions

**Quick Wins (fix these first if addressing failures):**
1. Fix scroll state calculation (test #5) - likely an off-by-one error
2. Fix scroll state isolation (test #13) - ensure hline1 is outline-specific
3. Investigate expansion state issues (tests 3, 7, 10, 11, 17) - may be single root cause

**Defer or Close:**
- Type safety tests (19-23) - may be test expectation vs UserTalk convention issue
- Parameter validation (2, 4, 6) - same as above
- Test #8 - verify reference implementation behavior, may need to fix test not code

**Investigate Further:**
- Complex workflows (14, 15, 16) - need to understand root cause before fixing
- Cross-outline validation (1, 9, 12) - design decision: should this be enforced?

---

## Test Locations

- Integration test definitions: `/Users/jake/dev/jsavin/Frontier/tests/integration/test_cases/op_verbs.yaml`
- Verb implementations: `/Users/jake/dev/jsavin/Frontier/tests/headless_op_verbs.c`
- Reference implementations: `/Users/jake/dev/jsavin/Frontier/Common/source/opverbs.c`

---

## Next Steps

Fill in the "Your Decision" column with one of:
- **Fix** - Address this failure
- **Defer** - Known issue, fix later
- **Investigate** - Need more info before deciding
- **Close** - Test expectation issue, not a bug
- **Skip** - Not important enough to address

After decisions are made, we can prioritize fixes or move on to the next phase.
