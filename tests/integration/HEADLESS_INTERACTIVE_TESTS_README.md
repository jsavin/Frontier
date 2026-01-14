# Headless Interactive Mode Tests

**Quick Reference for Running Phase 1 Tests**

---

## Test Files

### 1. File Dialog Verbs (Integration Tests)

**File:** `test_cases/file_verbs.yaml`

**Tests:**
- `file.getFileDialog` - Should error in headless mode
- `file.putFileDialog` - Should error in headless mode
- `file.getFolderDialog` - Should error in headless mode
- `file.getDiskDialog` - Should error in headless mode

**Run:**
```bash
cd tests && make test-integration
```

**Status:** ✅ Passing (4/4) - Verbs already return errors correctly

---

### 2. CLI Flag Parsing Tests (Shell)

**File:** `cli_flag_tests.sh`

**Tests:**
- `--batch` flag parsing
- `-b` short form
- `--non-interactive` alias
- Flag combination with other options
- Position independence

**Run:**
```bash
./tests/integration/cli_flag_tests.sh
```

**Status:** ❌ Failing (0/10) - Flags not yet implemented

---

### 3. TTY Detection Tests (Shell)

**File:** `tty_detection_tests.sh`

**Tests:**
- Batch flag override
- Piped input detection
- Redirected I/O detection
- CI environment detection

**Run:**
```bash
./tests/integration/tty_detection_tests.sh
```

**Status:** ⏸️ Skipped - Requires `--batch` flag implementation

---

## Quick Start

### Run All Tests

```bash
# Integration tests (file dialog verbs)
cd tests && make test-integration

# CLI flag tests
./tests/integration/cli_flag_tests.sh

# TTY detection tests
./tests/integration/tty_detection_tests.sh
```

### Expected Results (Before Implementation)

- Integration tests: ✅ 4/4 passing
- CLI flag tests: ❌ 0/10 passing (expected)
- TTY detection tests: ⏸️ Skipped

### Expected Results (After Phase 1 Implementation)

- Integration tests: ✅ 4/4 passing (no change)
- CLI flag tests: ✅ 10/10 passing
- TTY detection tests: ✅ Most passing (some platform-dependent)

---

## Documentation

- **Full Test Plan:** `../HEADLESS_INTERACTIVE_MODE_TESTS.md`
- **Test Summary:** `../TEST_SUMMARY_HEADLESS_INTERACTIVE.md`
- **Feature Planning:** `../../planning/phase3/HEADLESS_INTERACTIVE_MODE.md`

---

## Manual Testing

Some tests require manual validation from an actual terminal:

```bash
# Test 1: Interactive mode (from terminal)
./frontier-cli/frontier-cli -e "1+1"

# Test 2: Batch mode override
./frontier-cli/frontier-cli --batch -e "1+1"

# Test 3: Piped input (non-TTY)
echo "1+1" | ./frontier-cli/frontier-cli -e -

# Test 4: CI environment
CI=true ./frontier-cli/frontier-cli -e "1+1"
```

---

**Created:** 2026-01-13
**Status:** Tests Created, Implementation Pending
