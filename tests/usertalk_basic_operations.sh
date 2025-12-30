#!/bin/bash
#
# UserTalk Basic Operations Regression Test Suite
#
# Purpose:
#   Verify that fundamental UserTalk operations work correctly.
#   This prevents regressions in core functionality.
#
# Usage:
#   ./tests/usertalk_basic_operations.sh
#
# Exit codes:
#   0 - All tests passed
#   1 - One or more tests failed
#
# Note:
#   This script must be run from the project root directory.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CLI_BIN="$PROJECT_ROOT/frontier-cli/frontier-cli"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Helper function to run a test
run_test() {
    local test_name="$1"
    local usertalk_code="$2"
    local expected_output="$3"

    TESTS_RUN=$((TESTS_RUN + 1))
    echo -n "Testing: $test_name ... "

    # Run the CLI with the UserTalk code
    local actual_output
    actual_output=$(FRONTIER_HEADLESS_SKIP_STARTUP=1 "$CLI_BIN" -e "$usertalk_code" 2>&1 | tail -1)

    if [ "$actual_output" = "$expected_output" ]; then
        echo -e "${GREEN}PASSED${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}FAILED${NC}"
        echo "  Expected: $expected_output"
        echo "  Got:      $actual_output"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Helper function to test for expected error message
run_error_test() {
    local test_name="$1"
    local usertalk_code="$2"
    local expected_error_substring="$3"

    TESTS_RUN=$((TESTS_RUN + 1))
    echo -n "Testing: $test_name ... "

    # Run the CLI and capture both stdout and stderr
    local output
    output=$(FRONTIER_HEADLESS_SKIP_STARTUP=1 "$CLI_BIN" -e "$usertalk_code" 2>&1 || true)

    if echo "$output" | grep -qi "$expected_error_substring"; then
        echo -e "${GREEN}PASSED${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}FAILED${NC}"
        echo "  Expected error containing: $expected_error_substring"
        echo "  Got: $output"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

echo "=== UserTalk Basic Operations Test Suite ==="
echo ""

# Check CLI exists and is executable
if [ ! -x "$CLI_BIN" ]; then
    echo -e "${RED}ERROR: frontier-cli not found or not executable at $CLI_BIN${NC}"
    echo "Run 'make -C frontier-cli' to build it."
    exit 1
fi

echo "Using CLI: $CLI_BIN"
echo ""

# ============================================================================
# Arithmetic Operations
# ============================================================================
echo "--- Arithmetic Operations ---"
run_test "addition" "1+1" "2"
run_test "subtraction" "10-3" "7"
run_test "multiplication" "6*7" "42"
run_test "division" "100/4" "25"
run_test "complex expression" "(5+3)*2" "16"

# ============================================================================
# Assignment and Variables
# ============================================================================
echo ""
echo "--- Assignment and Variables ---"
run_test "simple assignment" "x=5; return x" "5"
run_test "string assignment" 'x="hello"; return x' "hello"
run_test "multiple assignments" "x=10; y=20; return x+y" "30"

# ============================================================================
# String Operations (CRITICAL: Must use DOUBLE quotes!)
# ============================================================================
echo ""
echo "--- String Operations ---"
run_test "sizeOf with double quotes" 'sizeOf("hello")' "5"
run_test "string concatenation" '"hello" + " " + "world"' "hello world"
run_test "string.upper" 'string.upper("test")' "TEST"
run_test "string.lower" 'string.lower("TEST")' "test"

# ============================================================================
# Table Operations
# ============================================================================
echo ""
echo "--- Table Operations ---"
run_test "lang.new table" 'lang.new(tableType, @t); return defined(t)' "true"
run_test "table assignment and access" $'lang.new(tableType, @t); t.key1 = "hello"; return t.key1' "hello"
run_test "table sizeOf" $'lang.new(tableType, @t); t.a = 1; t.b = 2; t.c = 3; return sizeOf(t)' "3"

# ============================================================================
# Error Handling - Character Constant Syntax
# ============================================================================
echo ""
echo "--- Error Handling - Single Quote Strings (Should Fail) ---"
run_error_test "single quotes for string (should error)" "sizeOf('hello')" "Character constant"

# ============================================================================
# Error Handling - Division by Zero
# ============================================================================
echo ""
echo "--- Error Handling - Division by Zero ---"
run_error_test "division by zero" "5/0" "divide by zero"

# ============================================================================
# Print Summary
# ============================================================================
echo ""
echo "=== Test Summary ==="
echo "Total tests: $TESTS_RUN"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All basic operations working${NC}"
    echo -e "${GREEN}✓ UserTalk runtime is healthy${NC}"
    exit 0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    echo "Basic UserTalk operations may be broken."
    exit 1
fi
