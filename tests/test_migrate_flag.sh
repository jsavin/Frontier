#!/bin/bash
#
# Integration tests for --migrate CLI flag
#
# Usage: ./tests/test_migrate_flag.sh
#
# Exit codes:
#   0 = all tests passed
#   1 = one or more tests failed
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
CLI="$PROJECT_DIR/frontier-cli/frontier-cli"
TMP_DIR="$PROJECT_DIR/tests/tmp/migrate_tests"
V6_SOURCE="$PROJECT_DIR/tests/fixtures/v6/Frontier.root"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Setup
setup() {
    rm -rf "$TMP_DIR"
    mkdir -p "$TMP_DIR"
}

# Cleanup
cleanup() {
    rm -rf "$TMP_DIR"
}

# Test helper
run_test() {
    local name="$1"
    local expected_exit="$2"
    shift 2
    local cmd=("$@")

    TESTS_RUN=$((TESTS_RUN + 1))

    set +e
    output=$("${cmd[@]}" 2>&1)
    actual_exit=$?
    set -e

    if [ "$actual_exit" -eq "$expected_exit" ]; then
        echo -e "${GREEN}PASS${NC}: $name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}: $name"
        echo "  Expected exit code: $expected_exit, got: $actual_exit"
        echo "  Output: $output"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Test: file exists check
assert_file_exists() {
    local file="$1"
    local name="$2"

    TESTS_RUN=$((TESTS_RUN + 1))

    if [ -f "$file" ]; then
        echo -e "${GREEN}PASS${NC}: $name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}: $name"
        echo "  File does not exist: $file"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Test: file does not exist check
assert_file_not_exists() {
    local file="$1"
    local name="$2"

    TESTS_RUN=$((TESTS_RUN + 1))

    if [ ! -f "$file" ]; then
        echo -e "${GREEN}PASS${NC}: $name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}: $name"
        echo "  File exists but should not: $file"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Test: output contains string
assert_output_contains() {
    local output="$1"
    local expected="$2"
    local name="$3"

    TESTS_RUN=$((TESTS_RUN + 1))

    if echo "$output" | grep -qF -- "$expected"; then
        echo -e "${GREEN}PASS${NC}: $name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}: $name"
        echo "  Expected output to contain: $expected"
        echo "  Actual output: $output"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Test: v7 header check
assert_v7_header() {
    local file="$1"
    local name="$2"

    TESTS_RUN=$((TESTS_RUN + 1))

    # v7 databases have 0x0007 in the first two bytes
    local header=$(xxd -l 2 -p "$file" 2>/dev/null)

    if [ "$header" = "0007" ]; then
        echo -e "${GREEN}PASS${NC}: $name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}: $name"
        echo "  Expected v7 header (0007), got: $header"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

echo "========================================"
echo "  --migrate Flag Integration Tests"
echo "========================================"
echo ""

# Check prerequisites
if [ ! -f "$CLI" ]; then
    echo "Error: CLI not found at $CLI"
    echo "Run 'make -C frontier-cli' first"
    exit 1
fi

if [ ! -f "$V6_SOURCE" ]; then
    echo "Error: v6 source database not found at $V6_SOURCE"
    exit 1
fi

setup

echo "Test 1: Basic migration (default output path)"
rm -f "$TMP_DIR/test.root" "$TMP_DIR/test.v6.root"
cp "$V6_SOURCE" "$TMP_DIR/test.root"
run_test "Basic migration succeeds" 0 "$CLI" --migrate "$TMP_DIR/test.root"
assert_file_exists "$TMP_DIR/test.root" "v7 output at original path"
assert_v7_header "$TMP_DIR/test.root" "Output is v7 format"
assert_file_exists "$TMP_DIR/test.v6.root" "v6 backup created"
echo ""

echo "Test 2: Migration with custom output path"
rm -f "$TMP_DIR/custom_output.root"
cp "$V6_SOURCE" "$TMP_DIR/source.root"
run_test "Custom output path succeeds" 0 "$CLI" --migrate "$TMP_DIR/source.root" --output "$TMP_DIR/custom_output.root"
assert_file_exists "$TMP_DIR/custom_output.root" "Creates custom output file"
assert_v7_header "$TMP_DIR/custom_output.root" "Custom output is v7 format"
echo ""

echo "Test 3: Refuse to overwrite without --force"
output=$("$CLI" --migrate "$TMP_DIR/source.root" --output "$TMP_DIR/custom_output.root" 2>&1 || true)
assert_output_contains "$output" "already exists" "Error message mentions file exists"
assert_output_contains "$output" "--force" "Error message suggests --force"
echo ""

echo "Test 4: Overwrite with --force"
run_test "Force overwrite succeeds" 0 "$CLI" --migrate "$TMP_DIR/source.root" --output "$TMP_DIR/custom_output.root" --force
assert_file_exists "$TMP_DIR/custom_output.root" "File still exists after force overwrite"
echo ""

echo "Test 5: Short -f flag works"
run_test "Short -f flag succeeds" 0 "$CLI" --migrate "$TMP_DIR/source.root" --output "$TMP_DIR/custom_output.root" -f
echo ""

echo "Test 6: Already v7 detection"
output=$("$CLI" --migrate "$TMP_DIR/custom_output.root" 2>&1)
assert_output_contains "$output" "Already v7 format" "Detects already-v7 database"
echo ""

echo "Test 7: Non-existent input file"
run_test "Non-existent file fails" 1 "$CLI" --migrate "$TMP_DIR/nonexistent.root"
echo ""

echo "Test 8: Original v6 file preserved as backup"
# Copy a fresh v6 source and get its hash before migration
cp "$V6_SOURCE" "$TMP_DIR/source_test8.root"
rm -f "$TMP_DIR/source_test8.v6.root"
if command -v md5sum >/dev/null 2>&1; then
    src_hash_before=$(md5sum "$TMP_DIR/source_test8.root" | cut -d' ' -f1)
else
    src_hash_before=$(md5 -q "$TMP_DIR/source_test8.root")
fi
"$CLI" --migrate "$TMP_DIR/source_test8.root" >/dev/null 2>&1
# After migration, v6 backup is at .v6.root -- check its hash matches the original
if command -v md5sum >/dev/null 2>&1; then
    src_hash_after=$(md5sum "$TMP_DIR/source_test8.v6.root" | cut -d' ' -f1)
else
    src_hash_after=$(md5 -q "$TMP_DIR/source_test8.v6.root")
fi
TESTS_RUN=$((TESTS_RUN + 1))
if [ "$src_hash_before" = "$src_hash_after" ]; then
    echo -e "${GREEN}PASS${NC}: v6 backup matches original (data preserved)"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}FAIL${NC}: v6 backup hash does not match original!"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi
echo ""

echo "Test 9: --migrate cannot combine with --system-root"
run_test "Rejects --system-root combination" 1 "$CLI" --migrate "$TMP_DIR/source.root" --system-root "$TMP_DIR/source.root"
echo ""

echo "Test 10: --output requires --migrate"
run_test "Rejects --output without --migrate" 1 "$CLI" --output "$TMP_DIR/out.root" -e "1+1"
echo ""

cleanup

echo "========================================"
echo "  Results: $TESTS_PASSED/$TESTS_RUN passed"
echo "========================================"

if [ "$TESTS_FAILED" -gt 0 ]; then
    echo -e "${RED}$TESTS_FAILED test(s) failed${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed${NC}"
    exit 0
fi
