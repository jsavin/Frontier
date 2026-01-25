# CLI Positional .root Argument Tests

## Overview

This test suite validates the CLI positional .root argument feature, which allows users to pass `.root` or `.root7` database files as positional arguments instead of using the `--system-root` flag.

## Feature Specification

**Current behavior (before feature)**:
```bash
./frontier-cli --system-root databases/test.root -e "1+1"
```

**New behavior (after feature)**:
```bash
./frontier-cli databases/test.root -e "1+1"
```

**Key Requirements**:
1. Files with `.root` or `.root7` extensions should be treated as `--system-root` arguments
2. Files with `.usertalk` or `.ut` extensions should still be treated as script files (backward compatibility)
3. Providing both a positional .root file AND `--system-root` flag should produce an error
4. The positional .root argument should work at any position in the argument list

## Running the Tests

### Quick Start

```bash
# Run all tests
./tests/integration/cli_positional_root_tests.sh
```

### Prerequisites

1. Build the CLI binary:
   ```bash
   cd frontier-cli && make
   ```

2. Ensure test databases exist:
   - `databases/test.root` - Small test database
   - `databases/Frontier.root` - Full Frontier database

### Test Groups

The test suite is organized into 6 groups:

#### Group 1: Positional .root file detection (6 tests)
- Tests that `.root` files are recognized as system root arguments
- Validates different argument positions (before/after `-e` flag)
- Checks compatibility with other flags (`--batch`, `--verbose`, `--output-json`)

**Expected status**: FAIL (feature not implemented)

#### Group 2: Positional .root7 file detection (3 tests)
- Tests that `.root7` files are recognized as system root arguments
- Validates different argument positions

**Expected status**: FAIL (feature not implemented)

#### Group 3: Conflict detection (3 tests)
- Ensures providing both positional .root AND `--system-root` produces an error
- Tests various ordering of conflicting arguments

**Expected status**: PASS (error handling already works)

#### Group 4: Backward compatibility with script files (3 tests)
- Ensures `.usertalk` and `.ut` files are still treated as scripts
- Validates script execution with `--system-root` flag

**Expected status**: MIXED (2/3 passing, 1 failing due to feature dependency)

#### Group 5: Edge cases (4 tests)
- Non-existent .root files
- Invalid .root files (corrupted/wrong format)
- Multiple positional .root files
- Empty .root path

**Expected status**: PASS (error handling already works)

#### Group 6: Verify correct database loaded (1 test)
- Confirms that the positional .root actually loads the database
- Tests by checking if `system` table is accessible

**Expected status**: FAIL (feature not implemented)

## Current Test Results (Before Implementation)

```
CLI Positional .root Argument Tests: 9/20 passed
11 tests failed
```

**Passing tests (9)**: Error handling and backward compatibility
**Failing tests (11)**: New feature functionality

## Expected Test Results (After Implementation)

```
CLI Positional .root Argument Tests: 20/20 passed
All tests passed!
```

## Test Implementation Details

### Test Framework

The tests use a shell script framework similar to `cli_flag_tests.sh`, with three test helper functions:

- `test_case()` - Expects success (exit code 0)
- `test_case_fails()` - Expects failure (non-zero exit code)
- `skip_test()` - Skips test with reason

### Test Database Setup

Tests use existing project databases:
- `databases/test.root` - Small test database for basic functionality
- `databases/Frontier.root` - Full database for comprehensive testing

A temporary `.root7` symlink is created for testing `.root7` extension handling.

### Test Script Files

Temporary script files are created in `tests/tmp/`:
- `test_script.usertalk` - For backward compatibility testing
- `test_script.ut` - For backward compatibility testing

These are automatically cleaned up after tests complete.

## Integration with CI/CD

This test suite should be run as part of the CI/CD pipeline:

```bash
# In CI pipeline
make -C frontier-cli
./tests/integration/cli_positional_root_tests.sh
```

Exit code:
- `0` - All tests passed
- `1` - One or more tests failed

## Related Files

- **Test script**: `tests/integration/cli_positional_root_tests.sh`
- **CLI implementation**: `frontier-cli/main.c` (argument parsing)
- **CLI flag tests**: `tests/integration/cli_flag_tests.sh` (similar pattern)
- **CLI documentation**: `docs/CLI_USAGE_GUIDE.md` (to be updated)

## Future Enhancements

Potential test additions:
1. Test with relative paths (e.g., `../databases/test.root`)
2. Test with symbolic links to .root files
3. Test with .root files in different directories
4. Performance testing (ensure no regression in startup time)
5. Test with very long file paths
6. Test with special characters in file paths
