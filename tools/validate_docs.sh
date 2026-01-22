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

# Test 5: Check for broken internal links in NEW domain docs (usertalk/)
echo "Test 5: Checking for broken internal links in new domain docs..."
BROKEN_LINKS=0
BROKEN_LINKS_EXISTING=0

# Find markdown files in docs/usertalk (new docs from this PR)
NEW_DOCS=$(find docs/usertalk -name "*.md" -type f 2>/dev/null || true)

for doc in $NEW_DOCS; do
    # Extract markdown links: [text](path) or See: `path`
    LINKS=$(grep -o '\[.*\]([^)]*\.md)' "$doc" 2>/dev/null | sed 's/.*(\([^)]*\))/\1/' || true)
    BACKTICK_LINKS=$(grep -o '`docs/[^`]*\.md`' "$doc" 2>/dev/null | tr -d '`' || true)

    ALL_LINKS="$LINKS $BACKTICK_LINKS"

    for link in $ALL_LINKS; do
        # Handle relative paths (resolve relative to doc's directory)
        if [[ "$link" != /* ]]; then
            doc_dir=$(dirname "$doc")
            link="$doc_dir/$link"
        fi

        # Normalize path
        link=$(echo "$link" | sed 's|/\./|/|g')

        if [ ! -f "$link" ]; then
            echo "  ❌ BROKEN LINK in $doc: $link"
            BROKEN_LINKS=$((BROKEN_LINKS + 1))
        fi
    done
done

# Also scan existing docs but only warn (don't fail)
EXISTING_DOCS=$(find docs -name "*.md" -type f ! -path "docs/usertalk/*" ! -path "*/docserver/*" ! -path "*/frontier.userland.com/*" ! -path "*Frontier - The Definitive Guide*" 2>/dev/null || true)

for doc in $EXISTING_DOCS; do
    LINKS=$(grep -o '\[.*\]([^)]*\.md)' "$doc" 2>/dev/null | sed 's/.*(\([^)]*\))/\1/' || true)
    BACKTICK_LINKS=$(grep -o '`docs/[^`]*\.md`' "$doc" 2>/dev/null | tr -d '`' || true)

    ALL_LINKS="$LINKS $BACKTICK_LINKS"

    for link in $ALL_LINKS; do
        if [[ "$link" != /* ]]; then
            doc_dir=$(dirname "$doc")
            link="$doc_dir/$link"
        fi

        link=$(echo "$link" | sed 's|/\./|/|g')

        if [ ! -f "$link" ]; then
            BROKEN_LINKS_EXISTING=$((BROKEN_LINKS_EXISTING + 1))
        fi
    done
done

if [ $BROKEN_LINKS -gt 0 ]; then
    echo "  FAIL: $BROKEN_LINKS broken links in NEW docs (usertalk/)"
    exit 1
fi

if [ $BROKEN_LINKS_EXISTING -gt 0 ]; then
    echo "  ⚠️  INFO: $BROKEN_LINKS_EXISTING broken links in existing docs (pre-existing issue)"
fi

echo "  PASS: No broken links in new domain docs"
echo

# Test 6: Check for orphaned docs (not referenced anywhere)
echo "Test 6: Checking for orphaned documentation files..."
ORPHANED=0

# Get all markdown files in docs/
ALL_DOCS=$(find docs -name "*.md" -type f 2>/dev/null || true)

for doc in $ALL_DOCS; do
    # Skip certain files that are intentionally standalone
    if [[ "$doc" == *"/docserver/"* ]] || \
       [[ "$doc" == *"/frontier.userland.com/"* ]] || \
       [[ "$doc" == *"/Frontier - The Definitive Guide"* ]]; then
        continue
    fi

    # Check if referenced in CLAUDE.md or other docs
    doc_basename=$(basename "$doc")
    if ! grep -q "$doc_basename" CLAUDE.md 2>/dev/null && \
       ! grep -rl "$doc_basename" docs --exclude-dir=docserver --exclude-dir=frontier.userland.com --exclude-dir="Frontier - The Definitive Guide" 2>/dev/null | grep -v "^$doc$" > /dev/null; then
        echo "  ⚠️  ORPHANED (not referenced): $doc"
        ORPHANED=$((ORPHANED + 1))
    fi
done

if [ $ORPHANED -gt 0 ]; then
    echo "  INFO: $ORPHANED orphaned docs found (may be intentional)"
    echo "  (This is not a failure, just informational)"
fi
echo "  PASS: Orphan check complete"
echo

# Test 7: Validate UserTalk code examples syntax
echo "Test 7: Validating UserTalk code examples syntax..."
SYNTAX_ERRORS=0

# Check docs/usertalk files for common UserTalk syntax errors in examples
USERTALK_DOCS=$(find docs/usertalk -name "*.md" -type f 2>/dev/null || true)

for doc in $USERTALK_DOCS; do
    # Check for single quotes around strings (common error)
    if grep -n "sizeOf('.*')" "$doc" 2>/dev/null | grep -v "❌ WRONG" | grep -v "ERROR" > /dev/null; then
        echo "  ⚠️  WARNING: Found sizeOf() with single quotes in $doc (should use double quotes)"
        echo "    (May be intentional in error examples)"
    fi

    # Check for typeof() returning strings (critical error to avoid)
    if grep -n 'typeof.*=>.*"string"' "$doc" 2>/dev/null | grep -v "❌ WRONG" | grep -v "ERROR" > /dev/null; then
        echo "  ❌ CRITICAL: Found typeof() returning string in $doc"
        echo "    typeof() must always return OSType codes, never strings!"
        SYNTAX_ERRORS=$((SYNTAX_ERRORS + 1))
    fi
done

if [ $SYNTAX_ERRORS -gt 0 ]; then
    echo "  FAIL: $SYNTAX_ERRORS critical syntax errors in examples"
    exit 1
fi
echo "  PASS: UserTalk code examples look correct"
echo

echo "=== All validation tests passed ==="
exit 0
