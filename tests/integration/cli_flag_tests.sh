#!/bin/bash
# CLI Flag Parsing Tests for Headless Interactive Mode
# Tests --batch, -b, and --non-interactive flags
#
# Reference: planning/phase3/HEADLESS_INTERACTIVE_MODE.md
# Test Plan: tests/HEADLESS_INTERACTIVE_MODE_TESTS.md

set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"

PASS=0
FAIL=0
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

# Check if CLI binary exists
if [ ! -f "$CLI" ]; then
    echo -e "${RED}ERROR${NC}: CLI binary not found at $CLI"
    echo "Run 'make' to build frontier-cli first"
    exit 1
fi

echo "CLI Flag Parsing Tests"
echo "======================"
echo ""

# Test 1: --batch flag doesn't cause parse error
test_case "CLI accepts --batch flag" \
    $CLI --batch -e "1+1"

# Test 2: -b short form works
test_case "CLI accepts -b short form" \
    $CLI -b -e "1+1"

# Test 3: --non-interactive alias works
test_case "CLI accepts --non-interactive" \
    $CLI --non-interactive -e "1+1"

# Test 4: Batch flag with --system-root
if [ -f "$PROJECT_ROOT/databases/Frontier-v6.root" ]; then
    test_case "Batch flag combines with --system-root" \
        $CLI --batch --system-root "$PROJECT_ROOT/databases/Frontier-v6.root" -e "1"
else
    echo -e "${YELLOW}SKIP${NC}: Batch flag combines with --system-root (database not found)"
fi

# Test 5: Batch flag at different positions
test_case "Batch flag at end of args" \
    $CLI -e "1+1" --batch

test_case "Batch flag at start of args" \
    $CLI --batch -e "1+1"

# Test 6: Batch flag in middle of args
test_case "Batch flag in middle of args" \
    $CLI -e --batch "1+1"

# Test 7: Multiple flags together
test_case "Batch flag with verbose" \
    $CLI --batch --verbose -e "1+1"

test_case "Batch flag with JSON output" \
    $CLI --batch --output-json -e "1+1"

# Test 8: Short flags combined
test_case "Short flags -b and -v combined" \
    $CLI -bv -e "1+1"

# Summary
echo ""
echo "================================================"
echo "CLI Flag Parsing Tests: $PASS/$TOTAL passed"
if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}$FAIL tests failed${NC}"
    exit 1
fi
