#!/bin/bash
# Validate documentation structure after modular context refactor

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "=== Documentation Validation Tests ==="
echo

# Test 1: Check that all referenced docs exist
echo "Test 1: Verifying doc references in CLAUDE.md..."
MISSING_REFS=0

# Extract doc references from CLAUDE.md (patterns like docs/something/FILE.md)
REFS=$(grep -o 'docs/[a-zA-Z0-9_/.-]*\.md' CLAUDE.md 2>/dev/null || true)

for ref in $REFS; do
    if [ ! -f "$ref" ]; then
        echo "  ❌ MISSING: $ref"
        MISSING_REFS=$((MISSING_REFS + 1))
    else
        echo "  ✅ Found: $ref"
    fi
done

if [ $MISSING_REFS -gt 0 ]; then
    echo "  FAIL: $MISSING_REFS missing doc references"
    exit 1
fi
echo "  PASS: All doc references exist"
echo

# Test 2: Verify usertalk docs exist
echo "Test 2: Verifying UserTalk domain docs structure..."
EXPECTED_FILES=(
    "docs/usertalk/SYNTAX.md"
    "docs/usertalk/TYPEOF.md"
    "docs/usertalk/FILE_AND_DB.md"
)

for file in "${EXPECTED_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        echo "  ❌ MISSING: $file"
        exit 1
    else
        echo "  ✅ Found: $file"
    fi
done
echo "  PASS: UserTalk docs structure complete"
echo

# Test 3: Verify CLAUDE.md still has critical gotchas
echo "Test 3: Verifying CLAUDE.md has critical UserTalk gotchas..."
GOTCHAS=(
    "Double quotes for strings"
    "typeof() returns OSType codes"
    "Absolute paths required"
)

MISSING_GOTCHAS=0
for gotcha in "${GOTCHAS[@]}"; do
    if ! grep -q "$gotcha" CLAUDE.md; then
        echo "  ❌ MISSING: '$gotcha'"
        MISSING_GOTCHAS=$((MISSING_GOTCHAS + 1))
    else
        echo "  ✅ Found: '$gotcha'"
    fi
done

if [ $MISSING_GOTCHAS -gt 0 ]; then
    echo "  FAIL: $MISSING_GOTCHAS critical gotchas missing from CLAUDE.md"
    exit 1
fi
echo "  PASS: Critical gotchas present in CLAUDE.md"
echo

# Test 4: Check CLAUDE.md size reduction
echo "Test 4: Checking CLAUDE.md size..."
LINES=$(wc -l < CLAUDE.md)
echo "  Current size: $LINES lines"

if [ $LINES -gt 1000 ]; then
    echo "  ⚠️  WARNING: CLAUDE.md still > 1000 lines (target: ~950)"
else
    echo "  ✅ PASS: CLAUDE.md size under target"
fi
echo

echo "=== All validation tests passed ==="
exit 0
