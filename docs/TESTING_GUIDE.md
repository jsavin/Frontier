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
7. [Known Test Framework Limitations](#known-test-framework-limitations)

---

## Running frontier-cli

The frontier-cli executable must be run from the project root directory (NOT from within frontier-cli/ or tests/).

### Basic Syntax

```bash
# Execute inline UserTalk code (no database):
./frontier-cli/frontier-cli -e "1+1"

# Execute with system root database loaded:
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "sizeOf(system)"

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

# Run all tests (unit + integration + custom args):
cd tests && make test-all

# Run custom CLI args tests only (shell-based, uses -e mode):
cd tests && make test-custom-args

# Direct method - via shell wrapper:
./tools/run_integration_tests.sh
./tools/run_integration_tests.sh --verbose

# Run specific test file:
./tools/run_integration_tests.sh tests/integration/test_cases/string_verbs.yaml
```

### Custom CLI Arguments

Any unknown `--flag` is accepted and exposed to UserTalk scripts via `system.environment.args`:

```bash
# Custom flag with value (kebab-case → camelCase):
./frontier-cli/frontier-cli --system-root databases/Frontier.root --my-flag hello \
  -e 'system.environment.args.myFlag'
# → "hello"

# Boolean flag (no value):
./frontier-cli/frontier-cli --system-root databases/Frontier.root --dry-run \
  -e 'system.environment.args.dryRun'
# → true

# Inline --flag=value syntax also works:
./frontier-cli/frontier-cli --system-root databases/Frontier.root --browser=agent-browser \
  -e 'system.environment.args.browser'
# → "agent-browser"
```

**Built-in custom flag: `--browser`**

Controls which browser `sys.openUrl()` uses:

```bash
# Use agent-browser (AI-driven browser automation):
./frontier-cli/frontier-cli --system-root databases/Frontier.root --browser agent-browser

# Use system default browser (same as omitting --browser):
./frontier-cli/frontier-cli --system-root databases/Frontier.root --browser default
```

Tests for custom args are in `tests/custom_cli_args_test.sh` (run via `make test-custom-args`).

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
- `expected_pattern`: Regex pattern matched against the result using `re.fullmatch` (exact match). Use `.*` prefix/suffix for partial matching (e.g., `".*\\d+.*"` to match any string containing a number).
- `expected_contains`: List of substrings the result must contain
- `timeout`: Per-test timeout in seconds (default: 10)
- `description`: Human-readable test description

### File-Level Metadata

Test YAML files support optional top-level keys that control how the runner executes them. These go at the root of the YAML file, before the `tests:` array:

```yaml
sequential: true       # Run in main process, not parallel worker
needs_guest_dbs: true  # Copy sibling .root files + Guest Databases/ to worker

tests:
  - name: "my test"
    script: '...'
```

| Key | Default | Effect |
|-----|---------|--------|
| `sequential` | `false` | When `true`, file runs in the main process sequentially (not in a parallel worker). Use for tests with port conflicts, shared resources, or REPL stdin dependencies. |
| `needs_guest_dbs` | `false` | When `true`, the runner copies sibling `.root` files and `Guest Databases/` into the worker's temp directory. Use for tests that call `fileMenu.open()` or reference paths relative to `Frontier.getFilePath()`. |
| `protocol_mode` | `true` | When `false`, the runner skips the NDJSON protocol executor and spawns a new process per test. Use for tests that block or need special process behavior. |

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

### Interactive Tests (PTY-based)

For tests that require interactive user input (dialog verbs, prompts), use the `interactive_steps` field instead of `script`. This spawns the CLI in a pseudo-terminal via pexpect and drives interaction through expect/send pairs.

```yaml
tests:
  - name: "dialog.ask - accept default with Enter"
    description: "Pressing Enter should keep default value in @adr"
    interactive_steps:
      - expect: "\\[root\\]>"
        send: "local(r = \"default\"); dialog.ask(\"Enter name\", @r); return r"
      - expect: "Enter name"
        send: ""
      - expect: "\\[root\\]>"
        send: "/exit"
    expected_output_contains:
      - "default"
    expected_success: true
```

**How it works:**
1. The runner spawns `frontier-cli` in a PTY with `TERM=dumb` (disables raw mode) and `FRONTIER_PLAIN_REPL` (forces blocking REPL)
2. For each step, it waits for the `expect` pattern (regex) to appear in output, then sends the `send` text followed by a newline
3. After all steps, it waits for the process to exit and validates output using `expected_output_contains`

**Key patterns:**
- **REPL prompt**: Use `\\[root\\]>` (escaped brackets for regex)
- **Dialog prompt**: Use the prompt text as the expect pattern (e.g., `"Enter name"`)
- **Accept default**: Send empty string `""` (sends just Enter)
- **Button selection**: Send `"1"`, `"2"`, or `"3"` for numbered buttons
- **Always end with `/exit`**: Send `/exit` after the last `\\[root\\]>` expect

**Dependency:** Requires pexpect (`pip3 install pexpect`). Tests with `interactive_steps` auto-skip with a helpful message when pexpect is not installed. A vendored copy is available at `tests/vendor/` as fallback.

---

## Database State Isolation (system.temp Rule)

**CRITICAL: Integration tests must NEVER mutate anything outside `system.temp` in Frontier.root or any other .root file.**

- Use `system.temp.*` for all temporary test fixtures (outlines, tables, menus, scripts, etc.)
- `system.temp` is cleared on each process startup, preventing cross-test pollution
- NEVER use `workspace.*` for test fixtures -- workspace persists to disk via save_on_exit
- NEVER use `lang.new()` to create/overwrite top-level tables like `@workspace` -- this destroys pre-existing data
- Even if a test calls `delete()` to clean up, use `system.temp` anyway for safety

### Example Patterns

```usertalk
// GOOD -- uses system.temp
lang.new(outlineType, @system.temp.myOutline);
target.set(@system.temp.myOutline);

// BAD -- mutates workspace (persists to disk)
lang.new(outlineType, @workspace.myOutline);
target.set(@workspace.myOutline);

// VERY BAD -- overwrites the entire workspace table!
lang.new(tableType, @workspace);
```

### Applies To

- All YAML test files under `tests/integration/test_cases/`
- Any test that creates temporary database objects
- Both protocol-mode and per-process tests

---

## Temporary Files in Tests

### Temporary File Locations

Both `/tmp` and project-relative paths (`tests/tmp/`) are valid for test scratch files.

**C unit tests**: Use `/tmp` directly — it's the simplest option:
```c
const char *scratch_path = "/tmp/my_test_scratch.db";
```

**Integration tests**: Use `{FRONTIER_TEST_TMP_DIR}` template
```yaml
tests:
  - name: "file.write - create test file"
    script: 'file.write("{FRONTIER_TEST_TMP_DIR}/test.txt", "data")'
    expected_success: true
```

**Manual CLI tests**: Use `/tmp` or `get_test_temp_path.sh` helper
```bash
# Option 1: /tmp directly
./frontier-cli/frontier-cli -e 'file.write("/tmp/test.txt", "data")'

# Option 2: project-relative via helper
TESTDIR=$(./tools/get_test_temp_path.sh)
./frontier-cli/frontier-cli -e "file.write(\"$TESTDIR/test.txt\", \"data\")"
```

---

## Network Tests (TCP Verbs)

### Test Suite Organization

TCP networking tests are split across multiple files; all run by default:

**Local TCP Tests**:
- File: `tests/integration/test_cases/tcp_verbs.yaml`
- Local-only tests (error handling, address encoding, parameter validation)
- No external network connectivity required

**Self-Contained "Network" Tests**:
- File: `tests/integration/test_cases/tcp_verbs_network.yaml`
- 18 tests using `tcp.listenStream` + localhost connections (ports 9100-9115)
- The `_network.yaml` suffix is historical (these tests once hit external
  servers). They are now self-contained and run by default.
- One DNS-resolution test depends on the system resolver returning NXDOMAIN
  for `this.hostname.does.not.exist.invalid`. Environments that hijack
  NXDOMAIN responses (some corporate networks, captive portals) will
  surface that test failure as a real signal.

### Background: Why the `_network.yaml` Naming

Historically, `tcp_verbs_network.yaml` connected to external servers
(e.g. `example.com:80`) and was opt-in via `FRONTIER_RUN_NETWORK_TESTS=1`
to avoid CI/CD fragility from DNS, timeouts, and firewall restrictions.

After `tcp.listenStream()` landed (PR #330, 2026-01-24), the tests were
migrated to localhost listeners and are now deterministic. The opt-in gate
was removed; the filename is preserved for git history continuity.

**Example self-contained test**:
```yaml
# Phase 3 pattern - test server runs within test harness
tests:
  - name: "tcp.openStream - local test server"
    setup_script: |
      on serverHandler(stream, refcon) {
        tcp.writeStream(stream, "OK");
        tcp.closeStream(stream)
      };
      tcp.listenStream(8080, 5, @serverHandler)

    script: |
      local(stream = tcp.openAddrStream("127.0.0.1", 8080));
      local(response = tcp.readStream(stream, 1024));
      tcp.closeStream(stream);
      return response == "OK"
```

**Benefits of self-contained tests**:
- No external network dependencies
- Deterministic behavior (no DNS/network flakiness)
- Complete control over server responses
- Can simulate error conditions
- CI/CD friendly

**See also**: `docs/TCP_ARCHITECTURE.md` - Testing Strategy section

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

**Note:** `databases/Frontier.root` is already v7 format in the repository. To test migration, use the v6 fixture:

```bash
# Migrate the v6 test fixture (in-place — v6 backed up to .v6.root):
./frontier-cli/frontier-cli --migrate tests/fixtures/v6/Frontier.root

# Migrate with explicit output path (v6 source left untouched):
./frontier-cli/frontier-cli --migrate tests/fixtures/v6/Frontier.root --output /tmp/Frontier-v7.root
```

### What Happens During Migration

1. CLI opens the source file and detects v6 format
2. In-place mode: original v6 file is renamed to `.v6.root` (backup), v7 written to original path
3. With `--output PATH`: v6 source is left untouched, v7 is written to PATH
4. If the file is already v7, the command prints "Already v7 format" and exits

### Verification

```bash
# Check database version (first 2 bytes should be 0007 for v7)
xxd -l 2 databases/Frontier.root
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
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "sizeOf(system)"

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

### Comment Syntax (IMPORTANT - STRICT RULES)

**UserTalk has strict limitations on where comments can appear.**

**Comment types**:
1. **`//` comments** - Single-line comments to end of line
2. **`« ... »` comments** - Multi-line comments using special MacRoman characters (0xC7, 0xC8)
3. **`/* ... */` C-style comments** - NOT SUPPORTED (will cause syntax errors)

**CRITICAL: `//` comments CANNOT appear inside `try` or `else` blocks!**

```usertalk
// This is allowed - comment at top level
local(x = 1);

// WRONG - comment inside try block causes syntax error:
try {
  // THIS BREAKS THE PARSER
  local(x = 1);
  return x
}

// CORRECT - no comments inside try block:
try {
  local(x = 1);
  return x
}

// WRONG - comment inside else block also breaks:
try {
  local(x = 1/0)
} else {
  // THIS ALSO BREAKS
  return "error"
}

// CORRECT - comment before try/else is fine:
try {
  local(x = 1/0)
} else {
  return "error"
}
```

**Integration Test Guidelines**:
- DO NOT use `//` comments inside `try` or `else` blocks
- DO NOT use `/* */` style comments anywhere (not supported)
- Comments before or after try/else blocks are fine
- Comments at file top level are fine
- Use test descriptions in YAML instead of inline comments

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

### date.set() Parameter Order

**IMPORTANT:** `date.set()` uses DAY, MONTH, YEAR order (NOT month, day, year like JavaScript Date).

**Correct:**
```usertalk
date.set(15, 1, 2024, 10, 30, 0)  // January 15, 2024 at 10:30:00
// Arguments: day, month, year, hour, minute, second
```

**Wrong:**
```usertalk
date.set(2024, 1, 15, 10, 30, 0)  // WRONG - this is year=2024, month=1, day=15
```

**Mnemonic:** Think European date format (DD/MM/YYYY) rather than US format (MM/DD/YYYY).

### typeof() Comparisons and Type Constants

**typeof() returns OSType codes (4-byte constants), NOT string names.**

**Correct:**
```usertalk
// Using system.compiler.language.constants (requires system root loaded)
if typeof(x) == stringType { ... }    // stringType = 'TEXT'
if typeof(obj) == tableType { ... }   // tableType = 'tabl'

// Using literal OSType codes (works without system root)
if typeof(x) == 'TEXT' { ... }
if typeof(obj) == 'tabl' { ... }
```

**Wrong:**
```usertalk
if typeof(x) == "string" { ... }  // WRONG - typeof() returns 'TEXT', not "string"
```

**Test Framework Note:** In integration tests (YAML), use string literals like `"tabl"` because JSON serialization converts OSType codes to strings. This is a test framework limitation, not runtime behavior.

### contains Keyword vs. string.patternMatch()

**`contains` is a KEYWORD OPERATOR, not a verb.**

**Correct:**
```usertalk
if string1 contains string2 { ... }    // ✓ Keyword operator syntax
if string.patternMatch(string1, "*" + string2 + "*") { ... }  // ✓ Verb alternative
```

**Wrong:**
```usertalk
if string.contains(string1, string2) { ... }  // ✗ No such verb exists
```

**When to use each:**
- `contains` keyword: Simple substring checks, more readable
- `string.patternMatch()`: Pattern matching with wildcards (`*`, `?`), more powerful

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
xxd -l 2 -p databases/Frontier.root
# Expected for v6: 0006
# Expected for v7: 0007
```

**See also:** `planning/DATABASE_CORRUPTION_PREVENTION.md` for database protection details

---

## Known Test Framework Limitations

### OSType/JSON Serialization Issue

**Problem**: The integration test framework has a fundamental limitation with OSType constant comparisons due to JSON serialization.

**Background**:
- UserTalk's `typeof()` verb returns **OSType codes** (4-byte constants like `'tabl'`, `'TEXT'`, `'fss '`)
- Type constants (`tableType`, `stringType`, `filespecType`) are defined in `system.compiler.language.constants` and resolve to these OSType codes
- Production UserTalk code uses type constants: `if typeof(x) == tableType { ... }`

**The Limitation**:
- The test framework uses `--output-json` to capture CLI results
- JSON serialization converts OSType codes to strings during the conversion process
- When the test framework parses the JSON output, OSType values become string literals
- Result: Tests must use **string literals** instead of **type constants** for comparisons to work

**Example**:

Production code (correct):
```usertalk
if typeof(x) == tableType {
    return "is a table"
}
```

Integration test (workaround):
```yaml
tests:
  - name: "typeof - table check"
    script: |
      lang.new(tableType, @x);
      if typeof(x) == "tabl" {
          return "is a table"
      }
    expected_success: true
    expected_result: "is a table"
```

**Why This Matters**:
- Creates divergence from production code patterns
- Tests look "wrong" compared to real UserTalk code
- May confuse developers reviewing test cases
- **Not a bug in `typeof()` or the runtime** - purely a test framework serialization issue

**Common OSType String Literals in Tests**:
- `"tabl"` instead of `tableType`
- `"TEXT"` instead of `stringType`
- `"bool"` instead of `booleanType`
- `"long"` instead of `intType`
- `"fss "` instead of `filespecType` (note trailing space!)

**The Correct Approach**:
- ✅ Use type constants (`tableType`, `stringType`) in production code
- ✅ Use string literals (`"tabl"`, `"TEXT"`) in integration tests
- ✅ Document this divergence when it appears in test files
- ❌ Don't "fix" `typeof()` to return string names - breaks all production code

**Reference**: Issue #291 (PR review feedback identifying this pattern)

### Protocol Mode `with` Block Limitations

**Problem**: The NDJSON protocol executor wraps each test script in a `with system.temp.FrontierREPL.variables { ... }` block. This causes several classes of test failures.

**Affected patterns**:

1. **`@address` parameters don't work**: Verbs that take `@variable` output parameters (e.g., `sys.unixshellcommand("cmd", @stdout)`) fail inside the `with` block because the address resolution doesn't work correctly in the wrapped scope.

2. **`local` variables go out of scope before `return`**: If `local(ok = ...)` and `return ok` are on separate lines, the `with` block may cause `ok` to be out of scope by the time `return` executes. Putting them on the same line (semicolon-separated) keeps them in the same scope.

3. **Empty string returns become null**: When a script returns `""`, the protocol layer may serialize it as `null` instead of an empty string.

4. **`thread.evaluate` fails**: Spawned threads run outside the `with` block's scope and cannot interact with it properly.

5. **Target context interference**: The `with` block establishes a context that may interfere with `target.clear()` / `target.set()` operations.

**Workaround**: Set `repl_mode: true` on the test case. This runs the test via a per-process script file execution instead of the protocol, avoiding the `with` block wrapper. When using `repl_mode`, put all UserTalk code on a single line (semicolon-separated) to avoid scope issues.

**Example**:
```yaml
# Protocol mode — will fail due to @address parameter
- name: "sys.unixshellcommand - 2 params"
  script: |
    local(stdout);
    sys.unixshellcommand("echo hello", @stdout);
    return stdout
  expected_result: "hello"

# Fixed — repl_mode with single-line script
- name: "sys.unixshellcommand - 2 params"
  repl_mode: true
  script: 'local(stdout); sys.unixshellcommand("echo hello", @stdout); return stdout'
  expected_result: "hello"
```

**Note**: `repl_mode` tests are slower (each spawns a new process) so prefer protocol mode when possible. Only use `repl_mode` when protocol mode's `with` block causes failures.

---

## Related Documentation

- `docs/VERB_IMPLEMENTATION_GUIDE.md` - Implementing kernel verbs in C
- `docs/CLI_USAGE_GUIDE.md` - Complete CLI reference (600+ lines)
- `docs/LOGGING_STANDARDS.md` - Logging infrastructure
- `CLAUDE.md` - Quick reference and development guidelines
