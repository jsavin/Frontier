# Integration Test Runner: stdin_input Support

**Status:** ✅ Implemented (Phase 2C complete)

## Overview

The integration test runner (`tests/integration/runner.py`) now supports simulating keyboard input for interactive dialog verbs via the `stdin_input` field in test YAML files.

## How It Works

### 1. Test YAML Configuration

Tests can specify stdin input using the `stdin_input` field:

```yaml
tests:
  - name: "dialog.getString - accept default with Enter"
    script: |
      return dialog.getString("Enter name", "DefaultName")
    expected_success: true
    expected_result: "DefaultName"
    stdin_input: "\n"  # Press Enter to accept default
```

### 2. Special Characters

The runner supports escape sequences in stdin_input:

- `\n` → Newline (Enter key)
- `\t` → Tab character
- `\x1b[A` → Up arrow (ANSI escape code)
- `\x1b[B` → Down arrow
- `\x1b[C` → Right arrow
- `\x1b[D` → Left arrow
- `\x03` → Ctrl+C (SIGINT)

**Note:** Arrow keys may not work correctly when stdin is piped. This is a limitation of terminal input handling.

### 3. TTY Detection Override

When stdin is piped (via subprocess `input=` parameter), `isatty()` returns false, which would disable interactive mode. To work around this:

- **Environment Variable:** `FRONTIER_FORCE_INTERACTIVE=1` overrides TTY detection
- **Automatic:** Test runner automatically sets this when `stdin_input` is provided (unless `batch_mode: true`)

### 4. Batch Mode Testing

Tests can verify batch mode error handling:

```yaml
tests:
  - name: "dialog.ask - batch mode should error"
    script: |
      return dialog.ask("This should fail")
    expected_success: false
    expected_error: "not implemented"
    batch_mode: true  # Adds --batch flag, disables interactive mode
```

### 5. Environment Variables

Tests can set custom environment variables:

```yaml
tests:
  - name: "dialog.ask - CI environment forces batch mode"
    script: |
      return dialog.ask("This should fail in CI")
    expected_success: false
    expected_error: "not implemented"
    environment:
      CI: "true"  # Forces batch mode even without --batch flag
```

## Implementation Details

### Runner Changes (`tests/integration/runner.py`)

1. **TestCase class** - Added fields:
   - `stdin_input`: Optional string to pass to subprocess stdin
   - `batch_mode`: Boolean flag to test batch mode error behavior
   - `environment`: Dictionary of environment variables

2. **FrontierCLI.execute()** - Updated parameters:
   - `stdin_input`: Passed to subprocess `input=` parameter
   - `batch_mode`: Adds `--batch` flag to CLI command
   - `env`: Merged with current environment

3. **TestRunner.run_test()** - Automatic environment setup:
   - Sets `FRONTIER_FORCE_INTERACTIVE=1` when `stdin_input` is provided
   - Skips override if `batch_mode: true`

### CLI Changes (`frontier-cli/cli_utils.c`)

1. **cli_init_interactive_mode()** - Enhanced TTY detection:
   - Checks `FRONTIER_FORCE_INTERACTIVE` environment variable
   - Overrides `isatty()` check if set (testing only)

2. **isInteractiveMode()** - Updated documentation:
   - Returns true if `FRONTIER_FORCE_INTERACTIVE=1` set
   - Still respects `--batch` flag and `CI` environment

## Testing Status

### Phase 2C: Infrastructure ✅ COMPLETE

- ✅ Test runner accepts `stdin_input` field
- ✅ Subprocess receives input via `input=` parameter
- ✅ `FRONTIER_FORCE_INTERACTIVE=1` overrides TTY detection
- ✅ `batch_mode: true` tests verify error behavior
- ✅ Environment variable support implemented

### Phase 2D: Verb Implementation ⏳ PENDING

The following verbs need implementation before tests will pass:

- `msg()` - Output to stdout
- `dialog.ask()` - Yes/No prompt with arrow keys
- `dialog.getInt()` - Integer input with default
- `dialog.getString()` - String input with default
- `dialog.getPassword()` - Password input with masking

### Current Test Results

```bash
$ python3 tests/integration/runner.py tests/integration/test_cases/dialog_verbs.yaml

Running tests from: dialog_verbs.yaml
  Found 30 test(s)
    ✗ FAIL: msg - simple output to stdout  (verb not implemented)
    ✗ FAIL: dialog.ask - default Yes accepted with Enter  (verb not implemented)
    ✓ PASS: dialog.ask - batch mode should error  ✅ Works correctly
    ✗ FAIL: dialog.getInt - accept default with Enter  (verb not implemented)
    ✓ PASS: dialog.getInt - batch mode should error  ✅ Works correctly
    ...
```

**Expected behavior:** Batch mode tests pass, interactive tests fail until Phase 2D implements the verbs.

## Usage Examples

### Running Dialog Verb Tests

```bash
# Run all dialog verb tests
python3 tests/integration/runner.py tests/integration/test_cases/dialog_verbs.yaml

# Run with verbose output
python3 tests/integration/runner.py tests/integration/test_cases/dialog_verbs.yaml --verbose

# Run file dialog tests (same infrastructure)
python3 tests/integration/runner.py tests/integration/test_cases/file_dialog_verbs.yaml
```

### Manual Testing with stdin_input

```bash
# Simulate Enter key press
echo "" | FRONTIER_FORCE_INTERACTIVE=1 ./frontier-cli/frontier-cli -e 'dialog.getString("Name", "default")'

# Simulate typing + Enter
echo "UserInput" | FRONTIER_FORCE_INTERACTIVE=1 ./frontier-cli/frontier-cli -e 'dialog.getString("Name", "default")'

# Test batch mode (should error)
./frontier-cli/frontier-cli --batch -e 'dialog.ask("Proceed?")'
```

## Limitations

### 1. Arrow Keys with Piped Input

Arrow keys (ANSI escape sequences) may not work correctly when stdin is piped. This is a fundamental limitation of terminal input:

- **Real TTY:** Arrow keys work (terminal driver processes escape sequences)
- **Piped stdin:** Raw escape sequences passed through (may not be interpreted)

**Impact:** Tests for arrow key navigation may fail even with correct implementation.

**Workaround:** Use Tab key for selection (works with piped input).

### 2. Ctrl+C Handling

Tests expecting Ctrl+C (`\x03`) to abort execution may behave differently:

- **Real TTY:** Sends SIGINT, kills process immediately
- **Piped stdin:** May be read as input character, not signal

**Impact:** Ctrl+C tests may need special handling.

### 3. Visual Feedback

Dialog verbs with visual feedback (like `dialog.ask()` showing inverted text) won't be testable via automated tests. Visual verification requires manual testing with a real TTY.

## Next Steps (Phase 2D)

To complete Phase 2 interactive mode support:

1. **Implement msg() verb** (`tests/headless_dialog_verbs.c`)
   - Write string to stdout using `fputs()`
   - Return boolean true

2. **Implement dialog.ask()** with stdio prompts
   - Detect `isInteractiveMode()` → prompt via stdout/stdin
   - Detect batch mode → return `unimplementedverberror`

3. **Implement dialog.getInt()**, `dialog.getString()`, `dialog.getPassword()`
   - Same pattern: interactive vs batch mode

4. **Run integration tests** and verify all 58 tests pass

## Reference

- **Planning:** `planning/phase3/HEADLESS_INTERACTIVE_MODE.md`
- **Test Documentation:** `tests/PHASE2_INTERACTIVE_TESTS.md`
- **Dialog Verbs Tests:** `tests/integration/test_cases/dialog_verbs.yaml`
- **File Dialog Tests:** `tests/integration/test_cases/file_dialog_verbs.yaml`
