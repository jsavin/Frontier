#!/bin/bash
# CLI Positional .root Argument Tests
# Tests positional .root/.root7 argument detection and handling
#
# Feature: CLI should accept .root/.root7 files as positional arguments
# and treat them as --system-root, while preserving existing .usertalk/.ut
# script file behavior.
#
# Reference: Feature request for CLI UX improvement

# Don't exit on first error - we want to run all tests
# set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
TEST_DB="$PROJECT_ROOT/databases/test.root"
FRONTIER_DB="$PROJECT_ROOT/databases/Frontier.root"

PASS=0
FAIL=0
SKIP=0
TOTAL=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
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

# Test helper that expects failure
test_case_fails() {
    local name="$1"
    shift
    TOTAL=$((TOTAL + 1))

    if "$@" > /dev/null 2>&1; then
        echo -e "${RED}FAIL${NC}: $name (expected failure but succeeded)"
        FAIL=$((FAIL + 1))
        return 1
    else
        echo -e "${GREEN}PASS${NC}: $name"
        PASS=$((PASS + 1))
        return 0
    fi
}

# Skip helper
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

echo "CLI Positional .root Argument Tests"
echo "===================================="
echo ""

# ============================================================================
# Test Group 1: Positional .root file detection
# ============================================================================
echo "Test Group 1: Positional .root file detection"
echo "----------------------------------------------"

if [ -f "$TEST_DB" ]; then
    # Test 1a: Positional .root file treated as system root
    test_case "Positional .root file loads as system root" \
        $CLI "$TEST_DB" -e "1+1"

    # Test 1b: Positional .root file at different argument positions
    test_case "Positional .root file before -e flag" \
        $CLI "$TEST_DB" -e "1+1"

    test_case "Positional .root file after -e flag" \
        $CLI -e "1+1" "$TEST_DB"

    # Test 1c: Positional .root file with other flags
    test_case "Positional .root with --batch flag" \
        $CLI "$TEST_DB" --batch -e "1+1"

    test_case "Positional .root with --verbose flag" \
        $CLI "$TEST_DB" --verbose -e "1+1"

    test_case "Positional .root with --output-json flag" \
        $CLI "$TEST_DB" --output-json -e "1+1"
else
    skip_test "Positional .root file tests" "test.root not found"
fi

echo ""

# ============================================================================
# Test Group 2: Positional .root7 file detection
# ============================================================================
echo "Test Group 2: Positional .root7 file detection"
echo "-----------------------------------------------"

if [ -f "$FRONTIER_DB" ]; then
    # Create a temporary .root7 symlink for testing
    TEMP_ROOT7="$PROJECT_ROOT/databases/test_temp.root7"
    ln -sf "$FRONTIER_DB" "$TEMP_ROOT7"

    # Test 2a: Positional .root7 file treated as system root
    test_case "Positional .root7 file loads as system root" \
        $CLI "$TEMP_ROOT7" -e "1+1"

    # Test 2b: Positional .root7 file at different positions
    test_case "Positional .root7 file before -e flag" \
        $CLI "$TEMP_ROOT7" -e "1+1"

    test_case "Positional .root7 file after -e flag" \
        $CLI -e "1+1" "$TEMP_ROOT7"

    # Cleanup
    rm -f "$TEMP_ROOT7"
else
    skip_test "Positional .root7 file tests" "Frontier.root not found"
fi

echo ""

# ============================================================================
# Test Group 3: Conflict detection (positional + --system-root)
# ============================================================================
echo "Test Group 3: Conflict detection"
echo "---------------------------------"

if [ -f "$TEST_DB" ] && [ -f "$FRONTIER_DB" ]; then
    # Test 3a: Error when both positional .root and --system-root provided
    test_case_fails "Error on conflicting positional .root and --system-root" \
        $CLI "$TEST_DB" --system-root "$FRONTIER_DB" -e "1+1"

    # Test 3b: Error with .root7 and --system-root
    test_case_fails "Error on conflicting positional .root7 and --system-root" \
        $CLI "$FRONTIER_DB" --system-root "$TEST_DB" -e "1+1"

    # Test 3c: Error when positional .root comes after --system-root
    test_case_fails "Error on positional .root after --system-root flag" \
        $CLI --system-root "$FRONTIER_DB" "$TEST_DB" -e "1+1"
else
    skip_test "Conflict detection tests" "databases not found"
fi

echo ""

# ============================================================================
# Test Group 4: .usertalk files still treated as scripts (backward compat)
# ============================================================================
echo "Test Group 4: Backward compatibility with script files"
echo "-------------------------------------------------------"

# Create temporary test script files
TEMP_DIR="$PROJECT_ROOT/tests/tmp"
mkdir -p "$TEMP_DIR"
TEMP_USERTALK="$TEMP_DIR/test_script.usertalk"
TEMP_UT="$TEMP_DIR/test_script.ut"

# Write simple test scripts
echo "1+1" > "$TEMP_USERTALK"
echo "2+2" > "$TEMP_UT"

# Test 4a: .usertalk files still treated as scripts
test_case ".usertalk file executes as script (not database)" \
    $CLI "$TEMP_USERTALK"

# Test 4b: .ut files still treated as scripts
test_case ".ut file executes as script (not database)" \
    $CLI "$TEMP_UT"

# Test 4c: .usertalk with --system-root works
if [ -f "$TEST_DB" ]; then
    test_case ".usertalk script with --system-root flag" \
        $CLI --system-root "$TEST_DB" "$TEMP_USERTALK"
else
    skip_test ".usertalk script with --system-root" "test.root not found"
fi

# Cleanup
rm -f "$TEMP_USERTALK" "$TEMP_UT"

echo ""

# ============================================================================
# Test Group 5: Edge cases
# ============================================================================
echo "Test Group 5: Edge cases"
echo "------------------------"

# Test 5a: Non-existent .root file should fail gracefully
test_case_fails "Non-existent .root file fails" \
    $CLI "/nonexistent/path/test.root" -e "1+1"

# Test 5b: File with .root extension but not a database
FAKE_ROOT="$TEMP_DIR/fake.root"
echo "not a database" > "$FAKE_ROOT"
test_case_fails "Invalid .root file fails gracefully" \
    $CLI "$FAKE_ROOT" -e "1+1"
rm -f "$FAKE_ROOT"

# Test 5c: Multiple positional .root files should fail
if [ -f "$TEST_DB" ] && [ -f "$FRONTIER_DB" ]; then
    test_case_fails "Multiple positional .root files rejected" \
        $CLI "$TEST_DB" "$FRONTIER_DB" -e "1+1"
else
    skip_test "Multiple positional .root files test" "databases not found"
fi

# Test 5d: Empty .root path
test_case_fails "Empty .root path rejected" \
    $CLI "" -e "1+1"

echo ""

# ============================================================================
# Test Group 6: Verify system root is actually loaded
# ============================================================================
echo "Test Group 6: Verify correct database loaded"
echo "---------------------------------------------"

if [ -f "$FRONTIER_DB" ]; then
    # Test 6a: Can access system table from Frontier.root
    # (This assumes Frontier.root has a 'system' table)
    OUTPUT=$($CLI "$FRONTIER_DB" --output-json -e "defined(system)" 2>/dev/null || true)
    if echo "$OUTPUT" | grep -q '"success".*true'; then
        echo -e "${GREEN}PASS${NC}: Positional .root can access database tables"
        PASS=$((PASS + 1))
        TOTAL=$((TOTAL + 1))
    else
        echo -e "${RED}FAIL${NC}: Positional .root cannot access database tables"
        FAIL=$((FAIL + 1))
        TOTAL=$((TOTAL + 1))
    fi
else
    skip_test "Database content verification" "Frontier.root not found"
fi

echo ""

# ============================================================================
# Summary
# ============================================================================
echo "================================================"
echo "CLI Positional .root Argument Tests: $PASS/$TOTAL passed"
if [ $SKIP -gt 0 ]; then
    echo -e "${YELLOW}$SKIP tests skipped${NC}"
fi

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}$FAIL tests failed${NC}"
    exit 1
fi
