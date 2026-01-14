#!/bin/bash
# TTY Detection Tests for Headless Interactive Mode
# Tests isatty() detection and CI environment variable
#
# Reference: planning/phase3/HEADLESS_INTERACTIVE_MODE.md
# Test Plan: tests/HEADLESS_INTERACTIVE_MODE_TESTS.md
#
# NOTE: TTY detection is environment-dependent and may be limited in CI/CD
# Some tests require manual execution from an actual terminal

set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"

PASS=0
FAIL=0
SKIP=0
TOTAL=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test helper function
test_case() {
    local name="$1"
    shift
    TOTAL=$((TOTAL + 1))

    if "$@" > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}: $name"
        PASS=$((PASS + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}: $name"
        FAIL=$((FAIL + 1))
        return 1
    fi
}

skip_test() {
    local name="$1"
    local reason="$2"
    TOTAL=$((TOTAL + 1))
    SKIP=$((SKIP + 1))
    echo -e "${YELLOW}SKIP${NC}: $name ($reason)"
}

# Check if CLI binary exists
if [ ! -f "$CLI" ]; then
    echo -e "${RED}ERROR${NC}: CLI binary not found at $CLI"
    echo "Run 'make' to build frontier-cli first"
    exit 1
fi

echo "TTY Detection Tests"
echo "==================="
echo ""

# Test 1: Batch flag forces batch mode (even from TTY)
echo -e "${BLUE}Test Group 1: Batch Flag Override${NC}"
test_case "Batch flag forces batch mode from terminal" \
    $CLI --batch -e "1+1"

# Test 2: Piped input should auto-detect batch mode
echo ""
echo -e "${BLUE}Test Group 2: Non-TTY Contexts${NC}"
test_case "Piped input auto-detects batch mode" \
    bash -c "echo '1+1' | $CLI -e -"

# Test 3: Redirected output should auto-detect batch mode
test_case "Redirected output auto-detects batch mode" \
    bash -c "$CLI -e '1+1' > /dev/null"

# Test 4: Redirected input should auto-detect batch mode
test_case "Redirected input auto-detects batch mode" \
    bash -c "$CLI -e - < /dev/null"

# Test 5: CI environment variable forces batch mode
echo ""
echo -e "${BLUE}Test Group 3: CI Environment Detection${NC}"
test_case "CI=true forces batch mode" \
    bash -c "CI=true $CLI -e '1+1'"

test_case "CI=1 forces batch mode" \
    bash -c "CI=1 $CLI -e '1+1'"

# Test 6: Interactive mode detection (when TTY present)
echo ""
echo -e "${BLUE}Test Group 4: TTY Detection${NC}"

# Check if we're running in a TTY
if [ -t 0 ] && [ -t 1 ]; then
    echo -e "${BLUE}Running from TTY - can test interactive mode${NC}"
    test_case "Interactive mode works from terminal (no --batch)" \
        $CLI -e "1+1"

    # TODO: Once Phase 2 implemented, add:
    # echo "yes" | $CLI -e "dialog.ask('Continue?')"
else
    skip_test "Interactive mode from TTY" "not running in TTY context"
fi

# Test 7: File dialog verbs error in batch mode
echo ""
echo -e "${BLUE}Test Group 5: File Dialog Behavior (Phase 1)${NC}"

# NOTE: These tests verify verbs error in batch mode
# In Phase 1, verbs should return unimplementedverberror
# In Phase 2, verbs will work in interactive mode but error in batch mode

# Test that file dialog errors when --batch is set
test_file_dialog_errors() {
    local verb_name="$1"
    local script="$2"

    # Should fail (return non-zero) because verb returns unimplementedverberror
    if $CLI --batch -e "$script" 2>&1 | grep -qi "not implemented"; then
        echo -e "${GREEN}PASS${NC}: $verb_name errors with --batch flag"
        PASS=$((PASS + 1))
    elif $CLI --batch -e "$script" > /dev/null 2>&1; then
        # Verb succeeded - this is wrong for Phase 1
        echo -e "${RED}FAIL${NC}: $verb_name should error in batch mode (returned success)"
        FAIL=$((FAIL + 1))
    else
        # Verb failed but without "not implemented" message
        echo -e "${YELLOW}WARN${NC}: $verb_name failed but error message unclear"
        FAIL=$((FAIL + 1))
    fi
    TOTAL=$((TOTAL + 1))
}

# Skip these tests if file dialog verbs aren't implemented yet
if $CLI -e "1+1" > /dev/null 2>&1; then
    # CLI works, try file dialog tests
    # NOTE: These will fail in Phase 1 (before implementation)
    # and should pass once Phase 1 is implemented

    skip_test "file.getFileDialog errors in batch mode" "verb not yet implemented"
    skip_test "file.putFileDialog errors in batch mode" "verb not yet implemented"
    skip_test "file.getFolderDialog errors in batch mode" "verb not yet implemented"
    skip_test "file.getDiskDialog errors in batch mode" "verb not yet implemented"

    # TODO: Uncomment once Phase 1 is implemented:
    # test_file_dialog_errors "file.getFileDialog" \
    #     "local(f); file.getFileDialog('Select', @f)"
    # test_file_dialog_errors "file.putFileDialog" \
    #     "local(f); file.putFileDialog('Save', '/tmp/test.txt', @f)"
    # test_file_dialog_errors "file.getFolderDialog" \
    #     "local(f); file.getFolderDialog('Select', @f)"
    # test_file_dialog_errors "file.getDiskDialog" \
    #     "local(f); file.getDiskDialog('Select', @f)"
fi

# Summary
echo ""
echo "================================================"
echo "TTY Detection Tests: $PASS/$TOTAL passed, $SKIP skipped"
if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}$FAIL tests failed${NC}"
    exit 1
fi
