# Frontier Testing Guide

Complete guide for testing the headless Frontier runtime, including CLI usage, database migration, and testing patterns.

---

## Table of Contents

1. [Running frontier-cli](#running-frontier-cli)
2. [Integration Tests](#integration-tests)
3. [Database Migration (v6→v7)](#database-migration-v6v7)
4. [Testing Patterns](#testing-patterns)
5. [UserTalk Syntax Guide](#usertalk-syntax-guide)
6. [System Dependencies](#system-dependencies)

---

## Running frontier-cli

The frontier-cli executable must be run from the project root directory (NOT from within frontier-cli/ or tests/).

### Basic Syntax

```bash
# Execute inline UserTalk code (no database):
./frontier-cli/frontier-cli -e "1+1"

# Execute with system root database loaded:
./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e "sizeOf(system)"

# Run startup scripts (rarely needed - slows down CLI, default skips them):
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli -e "1+1"

# Default behavior (startup scripts skipped - faster for testing):
./frontier-cli/frontier-cli -e "1+1"
```

### Multi-line UserTalk Scripts

Multi-line scripts work using bash `$'...'` syntax for proper newline handling:

```bash
# Multi-line script with $'...\n...' syntax:
./frontier-cli/frontier-cli -e $'lang.new(tableType, @t);\nt.key1 = "hello";\nt.key2 = 42;\nreturn "size:" + sizeOf(t) + " key1:" + t.key1'

# Output: size:2 key1:hello

# Single-line works too (statements separated by semicolons):
./frontier-cli/frontier-cli -e "lang.new(tableType, @t); t.key1 = \"hello\"; return t.key1"
```

### Testing lang.new() Verb

The `lang.new()` verb creates new UserTalk objects (tables, outlines, scripts, etc.) in memory:

```bash
# Create a table and verify it exists:
./frontier-cli/frontier-cli -e "lang.new(tableType, @t); return defined(t)"
# Output: true

# Create a table, add data, and read it back:
./frontier-cli/frontier-cli -e "lang.new(tableType, @t); t.key1 = \"hello\"; t.key2 = 42; t.key3 = true; return \"size:\" + sizeOf(t) + \" key1:\" + t.key1 + \" key2:\" + t.key2"
# Output: size:3 key1:hello key2:42

# Test with different object types:
./frontier-cli/frontier-cli -e "lang.new(tableType, @myTable); return typeof(myTable)"
# Output: tableType
```

**Known issues:**
- Empty error message `[lang-ERROR] langcallbacks.c:208:` may appear after successful execution (harmless, can be ignored)
- Multi-line scripts passed as plain strings (without `$'...'`) will fail due to shell parsing

---

## Integration Tests

The integration test framework provides automated testing of UserTalk verbs using YAML test definitions and JSON output from frontier-cli.

### Prerequisites

- **Python 3** (pre-installed on macOS and most Linux distributions)
- **PyYAML** library: `pip3 install pyyaml` or `pip3 install -r tests/integration/requirements.txt`

### Running Integration Tests

```bash
# Quick method - via Makefile (recommended):
cd tests && make test-integration

# Verbose output:
cd tests && make test-integration-verbose

# Run all tests (unit + integration):
cd tests && make test-all

# Direct method - via shell wrapper:
./tools/run_integration_tests.sh
./tools/run_integration_tests.sh --verbose

# Run specific test file:
./tools/run_integration_tests.sh tests/integration/test_cases/string_verbs.yaml
```

### JSON Output Mode

The `--output-json` flag provides structured output for automation and testing:

```bash
# Regular output:
./frontier-cli/frontier-cli -e "1+1"
# Output: 2

# JSON output:
./frontier-cli/frontier-cli --output-json -e "1+1"
# Output (stdout):
# {
#   "success": true,
#   "result": "2",
#   "result_type": "string",
#   "error": null,
#   "error_type": null
# }

# Error case:
./frontier-cli/frontier-cli --output-json -e "undefined_var"
# JSON output (stdout):
# {
#   "success": false,
#   "result": null,
#   "result_type": null,
#   "error": "Script execution failed",
#   "error_type": "script_error"
# }
# Error logs (stderr):
# [lang-ERROR] langcallbacks.c:208: Cant evaluate...
# [general-ERROR] cli_utils.c:26: Execution error: Script execution failed
```

**Note**: JSON output goes to stdout, error logs go to stderr. This allows:
- Parsing JSON from stdout without interference from debug logs
- Capturing error context from stderr for debugging
- Standard Unix pipe semantics (json output | parser)

### Writing Test Cases

Test cases are defined in YAML files under `tests/integration/test_cases/`:

```yaml
tests:
  - name: "string.length - simple string"
    description: "Get length of a simple string"
    script: 'string.length("hello")'
    expected_success: true
    expected_result: "5"
    expected_result_type: "string"  # Optional: validate result type

  - name: "string.upper - basic case"
    description: "Convert string to uppercase"
    script: 'string.upper("hello")'
    expected_success: true
    expected_result: "HELLO"

  - name: "error case - undefined variable"
    description: "Access undefined variable"
    script: 'undefined_var'
    expected_success: false
    expected_error_type: "script_error"

  - name: "long running operation"
    description: "Custom timeout for potentially slow operations"
    script: 'complex.operation()'
    expected_success: true
    timeout: 30  # Optional: override default 10 second timeout
```

**Optional fields:**
- `expected_result_type`: Validates the `result_type` field from JSON output
- `timeout`: Per-test timeout in seconds (default: 10)
- `description`: Human-readable test description

### Test Path Placeholders

For portable file path handling in test scripts, use the `{FRONTIER_TEST_TMP_DIR}` placeholder instead of hardcoded paths:

```yaml
tests:
  - name: "file.new - create test file"
    description: "Create a file in the test tmp directory"
    script: |
      local(tmpDir = "{FRONTIER_TEST_TMP_DIR}");
      local(fs = file.fileFromPath(tmpDir + "/" + "test.txt"));
      local(ok = file.new(fs));
      return ok
    expected_success: true
    expected_result: "true"
```

**Available placeholders:**

- `{FRONTIER_TEST_TMP_DIR}` - Expands to `<project_root>/tests/tmp/integration` directory at test execution time

**How it works:**
1. Test runner loads YAML files from `tests/integration/test_cases/`
2. Before executing each script, runner substitutes `{FRONTIER_TEST_TMP_DIR}` with the absolute path to `<project_root>/tests/tmp/integration`
3. The `tests/tmp/integration/` directory is automatically created if it doesn't exist
4. Tests can safely create/read/delete files without hardcoding system-specific paths

**Why use placeholders:**
- ✅ Tests work on any system without modification
- ✅ No hardcoded usernames or system paths in git
- ✅ Supports CI/CD on different machines
- ✅ Makes tests portable across development environments

**Example expansion:**
```
Before substitution:
  local(tmpDir = "{FRONTIER_TEST_TMP_DIR}");

After substitution (on user's machine):
  local(tmpDir = "/Users/jake/dev/jsavin/Frontier/tests/tmp/integration");
```

**See also:** `tests/integration/runner.py` - `get_script_with_substitutions()` method for implementation details

---

## Temporary Files in Tests

### macOS Sandbox Restriction ⚠️

**CRITICAL**: frontier-cli runs in the macOS sandbox and **CANNOT access `/tmp`** or other system temp directories.

### Safe Patterns for Temporary Files

**Integration tests**: Use `{FRONTIER_TEST_TMP_DIR}` template
```yaml
tests:
  - name: "file.write - create test file"
    script: 'file.write("{FRONTIER_TEST_TMP_DIR}/test.txt", "data")'
    expected_success: true
```

**Manual CLI tests**: Use `get_test_temp_path.sh` helper
```bash
# Get safe temp directory
TESTDIR=$(./tools/get_test_temp_path.sh)

# Use in frontier-cli command
./frontier-cli/frontier-cli -e "file.write(\"$TESTDIR/test.txt\", \"data\")"
```

**Direct approach**: Use project-relative paths
```bash
# Create test directory (gitignore'd)
mkdir -p tests/tmp/unit

# Use directly in tests
./frontier-cli/frontier-cli -e 'file.write("tests/tmp/unit/test.txt", "data")'
```

### Unsafe Patterns (Will Fail)

❌ **NEVER use `/tmp`**:
```bash
# This will FAIL in sandbox
./frontier-cli/frontier-cli -e 'file.write("/tmp/test.txt", "data")'
```

❌ **NEVER use `/var/tmp`** or other system directories
❌ **NEVER use hardcoded absolute paths** outside the project

### Linting Check

Run the linting check to detect unsafe /tmp usage:
```bash
./tools/check_tmp_usage.sh
```

This script will scan tests/, portable/, and integration test YAML files for hardcoded /tmp paths.

---

### Test Output

```
Running tests from: string_verbs.yaml
  Found 21 test(s)
    ✓ PASS: string.length - simple string
    ✓ PASS: string.upper - basic case
    ✗ FAIL: string.nthCharacter - first char
      Error: Expected success=True, got success=False

======================================================================
TEST SUMMARY
======================================================================
Total:  21
Passed: 10
Failed: 11
======================================================================
```

### Current Test Coverage

- **String verbs**: 21 tests (basic operations, substring, pattern matching)
- **Pass rate**: ~47% (10/21 tests passing)
- Tests identify which verbs are implemented vs stubbed

---

## Database Migration (v6→v7)

**IMPORTANT: Always use clean migration before testing!**

Old migrated databases may be corrupted artifacts from earlier broken migrations. Always delete existing v7 databases and run a fresh migration before running tests.

### Quick Migration Command

```bash
# Clean migration workflow (ALWAYS do this before testing):
rm -f databases/Frontier-v7.root

# Run CLI with v6 database - creates v7 output file automatically
./frontier-cli/frontier-cli \
  --system-root databases/Frontier-v6.root -e "1"

# Output: databases/Frontier-v7.root (new file created by migration)
```

### What Happens During Migration

1. CLI opens `databases/Frontier-v6.root` and detects v6 format
2. Migration creates NEW output file: `databases/Frontier-v7.root`
3. Original `databases/Frontier-v6.root` is **never modified** (preserved)
4. Pattern: Version suffix is stripped, then `-v7` added: `Frontier-v6.root` → `Frontier-v7.root`

### Verification

```bash
# Check database version (first 2 bytes should be 0007 for v7)
xxd -l 2 databases/Frontier-v7.root
# Expected output: 00000000: 0007  ..
```

### Alternative: Using save_migration_tests

For testing the migration process itself:

```bash
# Clean rebuild and run migration test:
make -C tests clean && make -C tests save_migration_tests
./tests/save_migration_tests

# Output: test_save_migration-v7.root (v7 migrated database)
```

### Testing Migrated Database

```bash
# Test database loads and system table is accessible:
./frontier-cli/frontier-cli \
  --system-root test_save_migration-v7.root -e "defined(system)"

# Test external table variables (critical - tests Issue #123 fix):
./frontier-cli/frontier-cli \
  --system-root test_save_migration-v7.root -e "sizeOf(system.verbs.globals)"

# Test workspace access:
./frontier-cli/frontier-cli \
  --system-root test_save_migration-v7.root -e "defined(workspace)"
```

### Full Integration Test Suite

```bash
# Runs migration + all headless tests:
./tools/run_headless_tests.sh
```

See `planning/phase3/MIGRATION_VALIDATION_REPORT.md` for detailed test procedures and known issues.

---

## Testing Patterns

### Run Headless Test Suite

```bash
# Full test suite (rebuilds CLI, migrates DB, runs all tests):
./tools/run_headless_tests.sh
```

### Verb Coverage Analysis

```bash
# Show current verb detection (implemented vs stubbed):
cd tools/kernelverbs_parser && python3 cli.py analyze

# Generate detailed coverage reports:
python3 cli.py report

# Output to stdout:
python3 cli.py report -o -
```

### Common Test Commands

```bash
# Test basic arithmetic:
./frontier-cli/frontier-cli -e "1 + 1"

# Test string operations:
./frontier-cli/frontier-cli -e "string.upper(\"test\")"

# Test with database loaded:
./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e "sizeOf(system)"

# Test table operations:
./frontier-cli/frontier-cli -e "lang.new(tableType, @t); t.a = 1; return sizeOf(t)"
```

---

## UserTalk Syntax Guide

**IMPORTANT: UserTalk has unique syntax rules that differ from JavaScript, Python, and most modern languages.**

### String Literals (DIFFERENT FROM JS/Python/etc)

**Double quotes ("...") are for strings**:
- `"hello"` → string
- `sizeOf("hello")` → 5 ✓

**Single quotes ('...') are for character constants** (NOT strings!):
- `'A'` (1 char) → Character constant ✓
- `'TEXT'` (4 chars) → OSType (Mac file type) ✓
- `'hello'` (5 chars) → SYNTAX ERROR

**This is opposite of many modern languages where 'x' and "x" are equivalent!**

### Common Mistakes for AI Assistants

WRONG (JavaScript/Python style):
```javascript
sizeOf('hello')  // FAILS - single quotes not valid for strings in UserTalk
```

CORRECT (UserTalk style):
```usertalk
sizeOf("hello")  // Works - double quotes for strings
```

### Testing UserTalk Code

When testing built-in functions or verbs, ALWAYS use double quotes for string literals:
- ✓ `string.upper("test")`
- ✗ `string.upper('test')`  // Will fail!

### Error Messages

When single quotes are used incorrectly for strings, UserTalk produces:
```
"Character constant isnt correctly specified. Must be of the form 'c'."
```

This error indicates you tried to use single quotes for a multi-character string, which is invalid syntax.

### Quick Reference

| Syntax | UserTalk | JavaScript/Python |
|--------|----------|-------------------|
| String | `"hello"` | `"hello"` or `'hello'` |
| Character constant | `'A'` | N/A (just use `"A"`) |
| OSType (4-char) | `'TEXT'` | N/A (Mac-specific) |
| Multi-char with single quotes | ERROR | Works (string) |

**See also:** `docs/USERTALK_SYNTAX_REFERENCE.md` for comprehensive syntax guide.

---

## System Dependencies

### xxd (hex dump utility)

**Required for:** Database version verification and corruption detection

The `xxd` command is used by `tools/run_headless_tests.sh` to verify database file formats and detect corruption. The test runner automatically checks for `xxd` and will fail with a clear error message if it's not installed.

**Installation:**
- **macOS:** `brew install vim` (xxd is included with vim)
- **Linux (Debian/Ubuntu):** `apt-get install vim-common`
- **Linux (Red Hat/CentOS):** `yum install vim-common`

**Usage in Frontier:**
```bash
# Check database version (first 2 bytes)
xxd -l 2 -p databases/Frontier-v6.root
# Expected for v6: 0006
# Expected for v7: 0007
```

**See also:** `planning/DATABASE_CORRUPTION_PREVENTION.md` for database protection details

---

## Related Documentation

- `docs/VERB_IMPLEMENTATION_GUIDE.md` - Implementing kernel verbs in C
- `docs/CLI_USAGE_GUIDE.md` - Complete CLI reference (600+ lines)
- `docs/LOGGING_STANDARDS.md` - Logging infrastructure
- `CLAUDE.md` - Quick reference and development guidelines
