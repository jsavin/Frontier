# Headless Interactive Mode Test Plan

**Feature:** Phase 1 - Batch Flag and TTY Detection
**Reference:** `planning/phase3/HEADLESS_INTERACTIVE_MODE.md`
**Status:** Tests Created (TDD - Implementation Pending)

---

## Overview

This document describes the test strategy for Phase 1 of headless interactive mode:
1. CLI flags (`--batch`, `-b`, `--non-interactive`)
2. TTY detection via `isatty()`
3. File dialog verb error behavior

---

## Test Categories

### 1. CLI Flag Parsing Tests

**Goal:** Verify that CLI flags are parsed correctly and set the appropriate internal state.

#### Tests to Create

These tests should be added to the CLI argument parsing test infrastructure:

```c
// Test file: tests/cli_parser_tests.c (or similar)

// Test 1: --batch flag sets batch mode
void test_batch_flag_long_form(void) {
    // Setup: Parse arguments with --batch
    char* argv[] = {"frontier-cli", "--batch", "-e", "1+1"};
    int argc = 4;
    cli_options_t options;

    // Execute
    boolean result = cli_parse_arguments(argc, argv, &options);

    // Verify
    assert(result == true);
    assert(options.batch_mode == true);
    assert(options.inline_script != NULL);
    assert(strcmp(options.inline_script, "1+1") == 0);
}

// Test 2: -b short form works
void test_batch_flag_short_form(void) {
    char* argv[] = {"frontier-cli", "-b", "-e", "1+1"};
    int argc = 4;
    cli_options_t options;

    boolean result = cli_parse_arguments(argc, argv, &options);

    assert(result == true);
    assert(options.batch_mode == true);
}

// Test 3: --non-interactive alias works
void test_batch_flag_non_interactive_alias(void) {
    char* argv[] = {"frontier-cli", "--non-interactive", "-e", "1+1"};
    int argc = 4;
    cli_options_t options;

    boolean result = cli_parse_arguments(argc, argv, &options);

    assert(result == true);
    assert(options.batch_mode == true);
}

// Test 4: Batch flag combines with other options
void test_batch_flag_with_system_root(void) {
    char* argv[] = {"frontier-cli", "--batch", "--system-root",
                    "databases/Frontier.root", "-e", "1+1"};
    int argc = 6;
    cli_options_t options;

    boolean result = cli_parse_arguments(argc, argv, &options);

    assert(result == true);
    assert(options.batch_mode == true);
    assert(options.system_root != NULL);
    assert(strcmp(options.system_root, "databases/Frontier.root") == 0);
}

// Test 5: Batch flag can appear in different positions
void test_batch_flag_position_independence(void) {
    // Batch flag at end
    char* argv1[] = {"frontier-cli", "-e", "1+1", "--batch"};
    int argc1 = 4;
    cli_options_t options1;

    boolean result1 = cli_parse_arguments(argc1, argv1, &options1);
    assert(result1 == true);
    assert(options1.batch_mode == true);

    // Batch flag at beginning
    char* argv2[] = {"frontier-cli", "--batch", "-e", "1+1"};
    int argc2 = 4;
    cli_options_t options2;

    boolean result2 = cli_parse_arguments(argc2, argv2, &options2);
    assert(result2 == true);
    assert(options2.batch_mode == true);
}
```

**Implementation Location:** These tests will need a test harness. Options:
1. Add to existing `tests/cli_runtime_tests.c` (if suitable)
2. Create new `tests/cli_parser_tests.c` file
3. Create shell-based tests in `tests/integration/cli_flag_tests.sh`

**Recommendation:** Start with shell-based tests (easier to write, no C test infrastructure needed).

#### Shell-Based CLI Flag Tests

```bash
#!/bin/bash
# tests/integration/cli_flag_tests.sh
# CLI flag parsing tests for headless interactive mode

set -e

CLI="./frontier-cli/frontier-cli"
PASS=0
FAIL=0

# Test helper
test_case() {
    local name="$1"
    shift
    if "$@" > /dev/null 2>&1; then
        echo "PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $name"
        FAIL=$((FAIL + 1))
    fi
}

# Test 1: --batch flag doesn't cause parse error
test_case "CLI accepts --batch flag" \
    $CLI --batch -e "1+1"

# Test 2: -b short form works
test_case "CLI accepts -b short form" \
    $CLI -b -e "1+1"

# Test 3: --non-interactive alias works
test_case "CLI accepts --non-interactive" \
    $CLI --non-interactive -e "1+1"

# Test 4: Batch flag with system-root
test_case "Batch flag combines with --system-root" \
    $CLI --batch --system-root databases/Frontier.root -e "1"

# Test 5: Batch flag at different positions
test_case "Batch flag at end of args" \
    $CLI -e "1+1" --batch

test_case "Batch flag at start of args" \
    $CLI --batch -e "1+1"

# Summary
echo ""
echo "CLI Flag Tests: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
```

**Note:** These tests currently verify that the CLI doesn't reject the flags. Once implementation is complete, we'll add tests that verify the flags actually affect behavior (e.g., file dialog verbs error when --batch is set).

---

### 2. TTY Detection Tests

**Goal:** Verify that `isInteractiveMode()` correctly detects TTY vs non-TTY contexts.

#### Challenge: TTY Detection is Environment-Dependent

TTY detection depends on the actual execution environment:
- **TTY context:** Running directly in terminal
- **Non-TTY context:** Piped input/output, redirected I/O, background jobs

**Testing Approach:**

1. **Unit Tests (Limited):** Can test the logic, but cannot test actual TTY detection
2. **Shell-Based Tests:** Can simulate different contexts (pipe, redirect, etc.)

#### Shell-Based TTY Detection Tests

```bash
#!/bin/bash
# tests/integration/tty_detection_tests.sh
# TTY detection tests for headless interactive mode

set -e

CLI="./frontier-cli/frontier-cli"

# Test 1: Batch flag forces non-interactive mode (even from TTY)
echo "Test: --batch forces batch mode from terminal"
# This test runs from terminal (TTY), but --batch should override
# TODO: Once implemented, verify file.getFileDialog errors
$CLI --batch -e "1+1"
echo "PASS: --batch accepted from terminal"

# Test 2: Piped input should auto-detect batch mode
echo "Test: Piped input auto-detects batch mode"
echo "1+1" | $CLI -e -
echo "PASS: Piped input works"

# Test 3: Redirected output should auto-detect batch mode
echo "Test: Redirected output auto-detects batch mode"
$CLI -e "1+1" > /dev/null
echo "PASS: Redirected output works"

# Test 4: CI environment variable forces batch mode
echo "Test: CI=true forces batch mode"
CI=true $CLI -e "1+1"
echo "PASS: CI environment variable works"

# TODO: Once Phase 2 (stdio prompts) is implemented:
# Test 5: Interactive mode allows prompts (provide stdin)
# echo "yes" | $CLI -e "dialog.ask('Continue?')"
# Expected: Should work, not error

# Test 6: Batch mode errors on prompts
# $CLI --batch -e "dialog.ask('Continue?')" 2>&1 | grep "not implemented"
# Expected: Should error with "not implemented"
```

**Limitations:**
- Cannot fully test TTY detection without actually running in different contexts
- Test suite may always run in non-TTY context (CI/CD environment)
- Actual TTY detection validation requires manual testing

**Manual Testing Procedure:**

```bash
# Manual Test 1: Interactive terminal (TTY detected)
./frontier-cli/frontier-cli -e "1+1"
# Expected: Works (no error about TTY)

# Manual Test 2: Piped input (no TTY)
echo "1+1" | ./frontier-cli/frontier-cli -e -
# Expected: Works (auto-detects batch mode)

# Manual Test 3: Batch flag override (TTY present, but forced batch)
./frontier-cli/frontier-cli --batch -e "1+1"
# Expected: Works (batch mode forced)

# Manual Test 4: CI environment (no TTY)
CI=true ./frontier-cli/frontier-cli -e "1+1"
# Expected: Works (auto-detects batch mode)

# TODO Phase 2: Manual Test 5: File dialog in interactive mode
./frontier-cli/frontier-cli -e "file.getFileDialog('Select', @f)"
# Expected Phase 1: Error (not implemented yet)
# Expected Phase 2: Prompt on stdio (enter path manually)

# TODO Phase 2: Manual Test 6: File dialog in batch mode
./frontier-cli/frontier-cli --batch -e "file.getFileDialog('Select', @f)"
# Expected Phase 1: Error (not implemented)
# Expected Phase 2: Still error (batch mode disallows prompts)
```

---

### 3. File Dialog Verb Tests (Integration)

**Goal:** Verify that file dialog verbs error correctly in headless/batch mode.

#### Tests Added to `tests/integration/test_cases/file_verbs.yaml`

```yaml
# File Dialog Verbs - Phase 1: Should Error in Headless Mode

- name: "file.getFileDialog - not implemented in headless mode"
  description: "File picker dialog should error in batch/headless mode"
  script: |
    local(selectedFile);
    local(result = file.getFileDialog("Select a file", @selectedFile));
    return result
  expected_success: false
  expected_error: "not implemented"

- name: "file.putFileDialog - not implemented in headless mode"
  description: "Save file dialog should error in batch/headless mode"
  script: |
    local(tmpDir = "{FRONTIER_TEST_TMP_DIR}");
    local(defaultPath = tmpDir + "/" + "save_target.txt");
    local(selectedFile);
    local(result = file.putFileDialog("Save file as", defaultPath, @selectedFile));
    return result
  expected_success: false
  expected_error: "not implemented"

- name: "file.getFolderDialog - not implemented in headless mode"
  description: "Folder picker dialog should error in batch/headless mode"
  script: |
    local(selectedFolder);
    local(result = file.getFolderDialog("Select a folder", @selectedFolder));
    return result
  expected_success: false
  expected_error: "not implemented"

- name: "file.getDiskDialog - not implemented in headless mode"
  description: "Volume/disk picker dialog should error in batch/headless mode"
  script: |
    local(selectedDisk);
    local(result = file.getDiskDialog("Select a volume", @selectedDisk));
    return result
  expected_success: false
  expected_error: "not implemented"
```

**Test Execution:**

```bash
# Run integration tests (includes file dialog tests)
cd tests && make test-integration

# Expected Output (Phase 1 - before implementation):
# FAIL: file.getFileDialog - not implemented in headless mode
#   Reason: Verb not yet implemented in headless mode
#
# After Phase 1 implementation:
# PASS: file.getFileDialog - not implemented in headless mode
#   Verb correctly returns unimplementedverberror
```

---

## Test Execution Workflow

### Phase 1: TDD Approach - Tests First

1. **Create Tests (DONE)**
   - Integration tests for file dialog verbs ✅
   - CLI flag parsing test plan documented ✅
   - TTY detection test plan documented ✅

2. **Verify Tests Fail (CURRENT STEP)**
   ```bash
   cd /Users/jake/dev/jsavin/Frontier-headless-interactive
   cd tests && make test-integration
   ```
   Expected: File dialog tests FAIL (verbs not implemented)

3. **Implement Phase 1 Features (NEXT)**
   - Add CLI flag parsing (`--batch`, `-b`, `--non-interactive`)
   - Implement `isInteractiveMode()` function
   - Add conditional compilation to file dialog verbs
   - Return `unimplementedverberror` in headless mode

4. **Verify Tests Pass**
   ```bash
   cd tests && make test-integration
   ```
   Expected: File dialog tests PASS (verbs return error)

---

## Implementation Checklist (From Planning Doc)

### Phase 1: Error-Only Behavior

- [ ] **CLI Flag Infrastructure**
  - [ ] Add `--batch` / `-b` flag to `cli_parser.c`
  - [ ] Add `--non-interactive` as alias
  - [ ] Set global `fl_batch_mode` boolean
  - [ ] Add tests (shell-based or C unit tests)

- [ ] **Detection Logic**
  - [ ] Add `isInteractiveMode()` function to `cli_utils.c`
  - [ ] Check `isatty(STDIN_FILENO)` and `isatty(STDOUT_FILENO)`
  - [ ] Check `fl_batch_mode` global
  - [ ] Check `CI` environment variable
  - [ ] Export to verb processors (extern declaration in header)

- [ ] **File Dialog Verb Wrapping**
  - [ ] Add conditional compilation to `Common/source/fileverbs.c`
  - [ ] Wrap `sfgetfilefunc` (file.getFileDialog)
  - [ ] Wrap `sfputfilefunc` (file.putFileDialog)
  - [ ] Wrap `sfgetfolderfunc` (file.getFolderDialog)
  - [ ] Wrap `sfgetdiskfunc` (file.getDiskDialog)
  - [ ] Return `unimplementedverberror` in headless mode
  - [ ] Add TODO comments for Phase 2 stdio implementation

- [ ] **Testing**
  - [x] Integration tests for file dialog verbs (created)
  - [ ] Shell tests for CLI flags (documented, needs creation)
  - [ ] TTY detection manual tests (documented)
  - [ ] Verify tests fail before implementation (TDD)
  - [ ] Verify tests pass after implementation

- [ ] **Documentation**
  - [x] Test plan created (`tests/HEADLESS_INTERACTIVE_MODE_TESTS.md`)
  - [ ] Update `docs/CLI_USAGE_GUIDE.md` with `--batch` flag
  - [ ] Update `docs/HEADLESS_ADAPTATIONS.md` with link

---

## Expected Test Results

### Before Implementation (Current State)

```
cd tests && make test-integration

Expected Output:
FAIL: file.getFileDialog - not implemented in headless mode
  Reason: Can't evaluate the script
  (Verb not yet bound or returns wrong error)

FAIL: file.putFileDialog - not implemented in headless mode
FAIL: file.getFolderDialog - not implemented in headless mode
FAIL: file.getDiskDialog - not implemented in headless mode
```

### After Phase 1 Implementation

```
cd tests && make test-integration

Expected Output:
PASS: file.getFileDialog - not implemented in headless mode
PASS: file.putFileDialog - not implemented in headless mode
PASS: file.getFolderDialog - not implemented in headless mode
PASS: file.getDiskDialog - not implemented in headless mode

All file dialog verb tests pass (correctly return unimplementedverberror)
```

---

## Future Work: Phase 2 Testing

Once Phase 2 (stdio prompt implementation) is complete, update tests:

1. **Add Interactive Mode Tests**
   - Provide stdin for dialog prompts
   - Verify prompts work when TTY detected
   - Test default values when Enter pressed

2. **Add Batch Mode Override Tests**
   - Verify `--batch` flag forces error even from TTY
   - Verify `CI=true` forces batch mode
   - Verify piped input forces batch mode

3. **Update Existing Tests**
   - Change file dialog tests to test interactive mode
   - Add separate tests for batch mode errors

---

## References

- **Planning:** `planning/phase3/HEADLESS_INTERACTIVE_MODE.md`
- **Implementation Guide:** `docs/VERB_IMPLEMENTATION_GUIDE.md`
- **CLI Guide:** `docs/CLI_USAGE_GUIDE.md`
- **Integration Tests:** `tests/integration/test_cases/file_verbs.yaml`

---

**Last Updated:** 2026-01-13
**Status:** Tests Created - Ready for Implementation
