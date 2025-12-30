#!/bin/bash
#
# Error Message Coverage Verification Script
#
# Purpose:
#   Verify that all error constants defined in langinternal.h have
#   corresponding entries in langerrorlist.yaml.
#
# Usage:
#   ./tools/verify_error_coverage.sh
#
# Exit codes:
#   0 - All error constants are covered
#   1 - Missing error constants or count mismatch
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

HEADER_FILE="$PROJECT_ROOT/Common/headers/langinternal.h"
YAML_FILE="$PROJECT_ROOT/resources/strings/langerrorlist.yaml"

# Expected number of error messages (last error is sqlitecompileerror = 165)
EXPECTED_ERROR_COUNT=165

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=== Error Message Coverage Verification ==="
echo ""

# Check that required files exist
if [ ! -f "$HEADER_FILE" ]; then
    echo -e "${RED}ERROR: langinternal.h not found at $HEADER_FILE${NC}"
    exit 1
fi

if [ ! -f "$YAML_FILE" ]; then
    echo -e "${RED}ERROR: langerrorlist.yaml not found at $YAML_FILE${NC}"
    exit 1
fi

echo "Checking error constant coverage..."
echo ""

# Extract error constant names from langinternal.h
# Look for lines like: #define undefinederror 1
# Also include clipboard constant which uses different naming
ERROR_CONSTANTS=$(grep -E '#define.*error|#define clipboard' "$HEADER_FILE" | \
                  grep -v '/\*' | \
                  grep -v 'langerrorlist' | \
                  awk '{print $2}' | \
                  sort | \
                  uniq)

MISSING_COUNT=0
MISSING_ERRORS=""

# Check each constant has a YAML entry
for const in $ERROR_CONSTANTS; do
    if ! grep -q "id: $const" "$YAML_FILE"; then
        echo -e "${RED}MISSING: $const${NC}"
        MISSING_ERRORS="$MISSING_ERRORS\n  - $const"
        MISSING_COUNT=$((MISSING_COUNT + 1))
    fi
done

# Count YAML entries
YAML_COUNT=$(grep -c '^  - id:' "$YAML_FILE")

echo ""
echo "=== Coverage Summary ==="
echo "Expected error count: $EXPECTED_ERROR_COUNT"
echo "YAML entry count: $YAML_COUNT"
echo "Missing constants: $MISSING_COUNT"
echo ""

if [ $MISSING_COUNT -gt 0 ]; then
    echo -e "${RED}✗ FAILED: $MISSING_COUNT error constants missing from YAML${NC}"
    echo ""
    echo "Missing error constants:"
    echo -e "$MISSING_ERRORS"
    echo ""
    echo "Add these entries to $YAML_FILE"
    exit 1
fi

if [ "$YAML_COUNT" -lt "$EXPECTED_ERROR_COUNT" ]; then
    echo -e "${YELLOW}WARNING: YAML has $YAML_COUNT entries, expected $EXPECTED_ERROR_COUNT${NC}"
    echo "Some error indices may be missing."
    exit 1
fi

if [ "$YAML_COUNT" -gt "$EXPECTED_ERROR_COUNT" ]; then
    echo -e "${YELLOW}WARNING: YAML has $YAML_COUNT entries, expected $EXPECTED_ERROR_COUNT${NC}"
    echo "There may be duplicate or extra entries."
fi

echo -e "${GREEN}✓ All error constants covered in YAML${NC}"
echo -e "${GREEN}✓ Error message count matches expected ($EXPECTED_ERROR_COUNT)${NC}"
echo ""

exit 0
