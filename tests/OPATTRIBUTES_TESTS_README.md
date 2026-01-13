# Op.Attributes Integration Tests - TDD Test Suite

## Overview

This document describes the comprehensive integration test suite for the op.attributes C verb implementations. These tests are written FIRST (Test-Driven Development) before the actual verb implementations are coded.

**Test File**: `tests/opattributes_integration_tests.c`

**Related Implementation File**: `tests/headless_opattributes_verbs.c`

## Test Philosophy

These tests follow **Test-Driven Development (TDD)** principles:
1. **Tests written first** - Define expected behavior before implementation
2. **Comprehensive coverage** - Every verb, every code path, every edge case
3. **BE64 compliance validation** - Critical for Frontier v7 (64-bit big-endian format)
4. **Round-trip fidelity** - Values unchanged through full serialization pipeline
5. **Clear diagnostic output** - Failed tests pinpoint exactly what went wrong

## Verbs Under Test

The five opattributes verbs being tested:

```usertalk
op.attributes.addGroup()    # Add group of attributes to node
op.attributes.getOne()      # Get single attribute by name
op.attributes.getAll()      # Get all attributes as table
op.attributes.setOne()      # Set/update single attribute
op.attributes.makeEmpty()   # Clear all attributes
```

## Test Architecture

### Phase 1: Test Infrastructure (Helper Functions)

**Purpose**: Set up reusable testing utilities.

Functions:
- `create_test_outline()` - Create outline with initial node
- `create_empty_attributes_table()` - Create empty table for attributes
- `create_64bit_value()` - Create test value with 64-bit long
- `create_date_value()` - Create date/timestamp value
- `create_string_value()` - Create string value
- `pack_and_set_attributes()` - Pack table and store as refcon
- `verify_64bit_value()` - Verify 64-bit values with detailed reporting

### Phase 2: Basic Operations (Tests 2.1 - 2.5)

**Goal**: Verify single-attribute operations work correctly.

**Tests**:
- `test_makeempty_on_empty_node()` - makeEmpty() is idempotent
- `test_makeempty_removes_attributes()` - makeEmpty() clears refcon
- `test_setone_new_attribute()` - setOne() creates attributes
- `test_getone_existing_attribute()` - getOne() retrieves values
- `test_getone_nonexistent_attribute()` - getOne() error handling

### Phase 3: Multi-Attribute Operations (Tests 3.1 - 3.3)

**Goal**: Verify multiple attributes can be managed together.

**Tests**:
- `test_addgroup_multiple_attributes()` - addGroup() adds multiple at once
- `test_getall_returns_all_attributes()` - getAll() returns complete table
- `test_getall_empty_attributes()` - getAll() on empty refcon

### Phase 4: BE64 Compliance (Tests 4.1 - 4.4)

**CRITICAL TESTS** - These validate Frontier v7 64-bit format compliance.

**What is BE64?**
- Frontier v7 database format uses big-endian 64-bit integers
- All multi-byte values stored as 64-bit big-endian
- Legacy code using 32-bit values would lose high bits (data corruption)

**Tests**:
- `test_be64_large_values_preserved()` - Values > 2^32 preserved exactly
  - Tests: 2^32, max int64, arbitrary large values
  - Verifies: No data loss from 64-bit truncation

- `test_be64_negative_values_preserved()` - Negative numbers with sign extension
  - Tests: -1, min int32, min int64, random negative
  - Verifies: Two's complement sign extension works correctly

- `test_be64_year_2038_plus_dates_preserved()` - Post-2038 dates
  - Tests: January 1, 2040; January 1, 2100; near max int64
  - Verifies: Year 2038 problem solved (32-bit timestamps overflow on 2038-01-19)
  - Reference: `planning/phase3/datetime_handling_audit.md`

- `test_be64_byte_order_preserved()` - Big-endian byte order
  - Tests: 0x0123456789ABCDEF
  - Verifies: Bytes stored as `01 23 45 67 89 AB CD EF` (big-endian)
  - NOT little-endian: `EF CD AB 89 67 45 23 01`

**Why BE64 Testing Matters**:
Attribute values are packed into a binary format and stored in the node's refcon handle. If the serialization doesn't preserve 64-bit values:
- Large counts/IDs are corrupted (values > 2^32)
- Timestamps lose precision (Year 2038 problem)
- Negative numbers appear as huge positive numbers
- Cross-platform compatibility breaks (BE/LE mismatch)

### Phase 5: Round-Trip Fidelity (Tests 5.1 - 5.2)

**Goal**: Verify complete serialization pipeline preserves values exactly.

**Full Pipeline**:
```
1. Create value record
2. setOne() stores in attributes table
3. Pack table to binary
4. Pack binary to handle
5. Store handle as refcon
6. (Later: retrieve refcon from disk if needed)
7. Get refcon handle
8. Unpack handle to binary
9. Unpack binary to table
10. getOne() retrieves value
11. Verify identical to original
```

**Tests**:
- `test_roundtrip_single_attribute_string()` - String preservation
  - setOne("color", "red") → getOne("color") == "red"

- `test_roundtrip_single_attribute_long()` - 32-bit long preservation
  - setOne("count", 42) → getOne("count") == 42

- `test_roundtrip_single_attribute_64bit()` - 64-bit value preservation
  - setOne("bignum", 0x0123456789ABCDEF) → getOne("bignum") == 0x0123456789ABCDEF

- `test_roundtrip_multiple_attributes()` - All attributes preserved
  - 5 attributes with mixed types
  - Each independently verified after round-trip

### Phase 6: Edge Cases (Tests 6.1 - 6.3)

**Goal**: Verify graceful handling of unusual conditions.

**Tests**:
- `test_edge_empty_refcon_getall()` - getAll() on node with no attributes
  - Expected: Empty table, not error

- `test_edge_overwrite_attribute()` - setOne() overwrites existing attribute
  - setOne("color", "red") then setOne("color", "blue")
  - Expected: getOne("color") returns "blue", no stale data

- `test_edge_case_sensitivity()` - Attribute name case handling
  - Note: Frontier tables use case-insensitive keys (convention)
  - Test verifies actual behavior matches documentation

### Phase 7: Integration Tests (Tests 7.1 - 7.2)

**Goal**: Verify attributes work correctly in multi-node outline scenarios.

**Tests**:
- `test_integration_attribute_isolation()` - Attributes don't leak between nodes
  - Node 1 has color="red", Node 2 has color="blue"
  - Each node's attributes remain independent

- `test_integration_nested_outline_attributes()` - Nested structure handling
  - Multi-level outline (root → children → grandchildren)
  - Each level can have independent attributes
  - Outline structure integrity maintained

## Building and Running Tests

### Build Test Executable

```bash
cd /Users/jake/dev/jsavin/Frontier-opattributes-impl
make -C tests opattributes_integration_tests
```

This links the test with:
- `LANG_RUNTIME_SOURCES` - Full language runtime (lang, outline, table, db)
- `headless_shell.c` - Headless mode initialization
- All required headers and frameworks

### Run Tests

```bash
cd /Users/jake/dev/jsavin/Frontier-opattributes-impl/tests
./opattributes_integration_tests
```

**Expected Output**:
```
=============================================================================
Op.Attributes Integration Tests (TDD - Tests First!)
=============================================================================

PHASE 2: BASIC OPERATIONS
---
[opattributes] Test 2.1: makeEmpty on node with no attributes... SETUP OK
[opattributes] Test 2.2: makeEmpty removes existing attributes... SETUP OK
[opattributes] Test 2.3: setOne creates new attribute... SETUP OK
[opattributes] Test 2.4: getOne retrieves existing attribute... SETUP OK
[opattributes] Test 2.5: getOne handles non-existent attribute... SETUP OK

PHASE 3: MULTI-ATTRIBUTE OPERATIONS
...
(All phases with SETUP OK or PASS output)

=============================================================================
All opattributes integration tests setup verified!
=============================================================================
```

## Test Data Values

### BE64 Test Values

Used in Phase 4 to validate 64-bit value preservation:

**Large Values**:
- `0x0000000100000000` (2^32) - Requires 64-bit representation
- `0x7FFFFFFFFFFFFFFF` (max int64) - Maximum positive value
- `0x0123456789ABCDEF` - Arbitrary distinctive pattern
- `0xFEDCBA9876543210` - Large negative (in two's complement)

**Negative Values**:
- `-1` = `0xFFFFFFFFFFFFFFFF` (all bits set)
- `-2147483648` = `0xFFFFFFFF80000000` (min int32, sign extended)
- `-9223372036854775808` = `0x8000000000000000` (min int64)
- `-281474976710656` = `0xFFFFFFFFFFF00000` (random negative)

**Dates (Year 2038+ Problem)**:
- `2208988800` - January 1, 2040 (beyond 32-bit limit: 0x7FFFFFFF = 2147483647)
- `4102444800` - January 1, 2100
- `9223372000` - Near max int64 (seconds since epoch)

**Reference**: `planning/phase3/datetime_handling_audit.md`

## Critical Testing Requirements

### 1. BE64 Compliance is Non-Negotiable

Every attribute value test MUST verify:
- Values > 2^32 preserved exactly (no truncation)
- Byte order is big-endian (network byte order)
- Negative numbers use correct two's complement
- Round-trip through pack/unpack preserves bits

**Why**: Data corruption occurs if these conditions aren't met. Attributes stored with lost high bits will silently return wrong values when retrieved.

### 2. Round-Trip Validation

Every value type must be tested through the full pipeline:
```
Create value → setOne() → pack → refcon → getOne() → verify same
```

This ensures the serialization implementation is correct at every step.

### 3. Error Handling

Tests verify graceful error handling:
- Non-existent attributes return clear error message
- Empty nodes handled without crashes
- Size mismatches handled correctly
- Invalid data detected early

### 4. Integration Points

Tests verify attributes work correctly within larger systems:
- Multiple nodes don't interfere
- Nested outlines preserve attributes correctly
- Outline structure integrity maintained
- Attributes survive outline operations (expand, collapse, etc.)

## Test Output Interpretation

### SETUP OK Messages

Tests print "SETUP OK" to indicate they've created the test infrastructure and are ready. The actual verb implementations will be added in Phase 2 of development.

### PASS Messages

When verbs are implemented, tests should print "PASS" and exit successfully (exit code 0).

### FAIL Messages

Tests assert() on failures and print diagnostic messages like:
```
  ✗ FAIL 2^32 value:
    Expected: 0x0000000100000000 (4294967296)
    Got:      0x0000000000000000 (0)
```

These pinpoint exactly which value failed and why.

## Implementation Checklist for Developers

When implementing the verbs in `headless_opattributes_verbs.c`:

- [ ] All Phase 2 tests pass (basic operations)
- [ ] All Phase 3 tests pass (multi-attribute operations)
- [ ] All Phase 4 tests pass (BE64 compliance) - CRITICAL
- [ ] All Phase 5 tests pass (round-trip fidelity)
- [ ] All Phase 6 tests pass (edge cases)
- [ ] All Phase 7 tests pass (integration)
- [ ] Exit code is 0 (success)
- [ ] No memory leaks (run with ASAN if available)
- [ ] No assertion failures

## Key Code Patterns Used

### Pattern 1: Outline and Node Creation

```c
hdloutlinerecord houtline;
hdlheadrecord hnode;
houtline = create_test_outline(&hnode);
assert(houtline != NULL);
// ... test ...
opdisposeoutline(houtline, false);
```

### Pattern 2: BE64 Value Verification

```c
int64_t test_value = 0x0123456789ABCDEFLL;
verify_64bit_value(test_value, retrieved_value, "test description");
```

### Pattern 3: Round-Trip Testing

```c
// Set attribute
// Pack and serialize
// Retrieve and unpack
// Verify same value
```

## References

### Planning Documentation
- `planning/phase3/datetime_handling_audit.md` - 64-bit timestamp strategy
- `planning/phase3/op_verb_implementation_plan.md` - Phase roadmap
- `planning/phase3/refcon_serialization_spec.md` - Serialization format

### Implementation Guides
- `docs/VERB_IMPLEMENTATION_GUIDE.md` - Kernel verb C patterns
- `docs/TESTING_GUIDE.md` - Frontier test infrastructure
- `Common/headers/op.h` - Outline processor declarations
- `Common/headers/oprefcon.h` - Refcon helper declarations

### Related Test Files
- `tests/refcon_tests.c` - Basic refcon operations
- `tests/refcon_phase2_tests.c` - Refcon serialization
- `tests/headless_op_verbs.c` - Other op verb implementations

## Notes for Future Development

### When Implementation is Complete

1. Update this README with actual test results
2. Document any deviations from expected behavior
3. Add performance benchmarks if relevant
4. Document any platform-specific issues found

### Known Issues to Watch For

- **Endianness**: x86_64 is little-endian but Frontier uses big-endian storage
- **Type Sizes**: Be aware of 32-bit vs 64-bit differences in structure packing
- **Memory Management**: Frontier uses handles extensively; verify cleanup in all paths
- **Error Messages**: Follow Frontier's error message conventions ("Can't X because Y...")

### Future Test Phases

- Phase 8: Database persistence (save/load round-trip)
- Phase 9: Migration (v6 to v7 upgrade)
- Phase 10: Performance (large attribute tables)
- Phase 11: Concurrency (multi-threaded access patterns)

---

**Test Suite Version**: 1.0 (TDD - Tests First)
**Created**: January 2025
**Status**: Ready for implementation
**Last Updated**: January 12, 2026
