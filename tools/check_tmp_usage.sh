#!/bin/bash
# Detect unsafe /tmp usage in test code and portable implementations
# frontier-cli runs in macOS sandbox and CANNOT access /tmp
#
# Usage: ./tools/check_tmp_usage.sh
# Exit code: 0 if no issues, 1 if /tmp usage detected

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Track if we found any issues
FOUND_ISSUES=0

echo "Checking for unsafe /tmp usage in tests and portable code..."
echo ""

# Check test code for hardcoded /tmp paths
echo "Checking tests/ directory..."
if git grep -n '"/tmp' tests/ 2>/dev/null; then
    echo -e "${RED}ERROR: Found /tmp usage in tests/${NC}"
    echo "  Fix: Use {FRONTIER_TEST_TMP_DIR} template in integration tests"
    echo "       or \$(./tools/get_test_temp_path.sh) for manual testing"
    FOUND_ISSUES=1
fi

# Check portable/ for hardcoded /tmp paths
echo ""
echo "Checking portable/ directory..."
if git grep -n '"/tmp' portable/ 2>/dev/null; then
    echo -e "${RED}ERROR: Found /tmp usage in portable/${NC}"
    echo "  Fix: Use project-relative paths or user-provided paths"
    FOUND_ISSUES=1
fi

# Check YAML test files specifically (common source of issues)
echo ""
echo "Checking integration test YAML files..."
if git grep -n '/tmp' tests/integration/test_cases/*.yaml 2>/dev/null; then
    echo -e "${RED}ERROR: Found /tmp usage in integration test YAML${NC}"
    echo "  Fix: Replace with {FRONTIER_TEST_TMP_DIR} template"
    FOUND_ISSUES=1
fi

# Exit with appropriate code
echo ""
if [ $FOUND_ISSUES -eq 0 ]; then
    echo -e "${GREEN}✓ No /tmp usage detected${NC}"
    exit 0
else
    echo -e "${RED}✗ Found unsafe /tmp usage - see above for details${NC}"
    exit 1
fi
