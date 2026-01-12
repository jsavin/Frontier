# Table Operations Integration Tests

Issue #166: Comprehensive UserTalk integration tests for table operations

## Overview

This directory contains comprehensive integration tests for UserTalk table operation verbs. Tests are designed to verify correct behavior of table manipulation verbs at the UserTalk level.

## Test Files

### 1. table_operations_integration.c

**Primary test suite** - C test harness that executes UserTalk code via frontier-cli.

**Build & Run:**
```bash
# Build
make -C tests table_operations_integration

# Run
./tests/table_operations_integration
```

**Coverage (45 tests):**
- **table.assign()** (8 tests)
  - Basic string/number/boolean assignment
  - Empty string handling
  - Zero and negative values
  - Large number support
  - Overwrite existing values

- **table.copy()** (7 tests)
  - Basic copy operation
  - Source remains unchanged
  - Type preservation (string, numeric, boolean)
  - Copy to table with existing entries
  - Multiple copies from same source
  - Key name preservation

- **table.move()** (6 tests)
  - Basic move operation
  - Source entry removal after move
  - Sequential moves
  - Pre-populated destination tables
  - Type preservation (string, boolean)

- **table.rename()** (6 tests)
  - Basic rename operation
  - New key creation with value
  - Table size unchanged
  - Multiple entries handling
  - Numeric value preservation
  - Special character handling

- **table.emptytable()** (5 tests)
  - Single entry clearing
  - Return count verification
  - All entries removal
  - Table repopulation after emptying
  - Empty table behavior

- **table.moveandrename()** (6 tests)
  - Basic move and rename
  - New name in destination
  - Value preservation
  - Source size reduction
  - Destination size increase
  - Multiple sequential operations

- **Complex Scenarios** (7 tests)
  - Combined operations on table sets
  - Type preservation across operations
  - Large string values
  - Stress test (50+ entries)
  - Rename chains
  - Empty string handling
  - False/zero distinction

### 2. table_operations.ut

**UserTalk test source file** - Can be loaded into a UserTalk environment.

Contains 45 test cases written in pure UserTalk covering all table operation verbs with detailed comments.

Run with:
```bash
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root databases/Frontier-v6.root7 \
  -e "load(\"tests/table_operations/table_operations.ut\")"
```

### 3. table_verb_tests.c

**Existing C test harness** - Previous table verb tests, serves as reference.

## Test Methodology

### Test Framework
- Tests use frontier-cli to execute UserTalk code
- Output parsing filters out logging/debug output
- Each test returns either "PASS" or "FAIL" string
- Numeric assertions use `eval_expect_number()`
- String assertions use `eval_expect_string()`

### Edge Cases Covered
- Empty strings and zero values
- Negative and large numbers
- Boolean value handling
- Table size verification
- Entry existence checking (defined())
- Type preservation after operations
- Special characters in key names
- Concurrent operations on multiple tables
- Stress testing (50+ table entries)

## Integration with Build System

The test is integrated into the Makefile:
- Added to `RUN_BUILDABLE` list
- Compiles as a standalone executable
- Runs as part of `make -C tests test`

## Design Decisions

1. **C Test Harness vs Pure UserTalk**: Although table_operations.ut exists as pure UserTalk, the C harness (table_operations_integration.c) is the primary test suite because:
   - Integrates directly with build system
   - Runs automatically during test suite
   - Better error reporting and debugging
   - Consistent with other table verb tests

2. **Output Parsing**: Tests skip all `[lang-ERROR]`, `[headless]`, and `[WARN]` debug output to extract the actual return value from UserTalk scripts.

3. **Compound Conditions**: Tests avoid compound boolean conditions (e.g., `a and b and c`) in single if statements due to UserTalk parser limitations when running via `-e` flag. Instead, tests verify conditions separately for clarity.

4. **Scope**: Tests cover headless/CLI operations only (matching existing table_verb_tests.c approach), not GUI-dependent functionality.

## Known Limitations

1. Tests are headless-only - no GUI environment
2. `load()` verb for loading .ut files may have limitations
3. Some complex compound boolean expressions fail - tests use separate assertions instead

## Future Enhancements

1. Add tests for error conditions (nonexistent entries, type mismatches)
2. Add performance benchmarks for large table operations
3. Expand to test interaction with persistent tables
4. Add tests for table.validate() and table.packtable() verbs
5. Test concurrent table operations if multi-threading is enabled

## References

- Issue #166: UserTalk integration tests for table operations
- table_verb_tests.c: Original C test harness (similar approach)
- tests/lang_verbs/lang_new_verb_tests.ut: Example UserTalk test format
- tests/test_stateless_verbs.ut: Example stateless verb tests
