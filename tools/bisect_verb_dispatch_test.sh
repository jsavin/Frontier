#!/bin/bash
# Git bisect test script for verb dispatch regression
#
# Usage:
#   git bisect start
#   git bisect bad HEAD
#   git bisect good 472ea6c9
#   git bisect run ./tools/bisect_verb_dispatch_test.sh
#
# This script returns:
#   0 (GOOD) if verb dispatch works
#   1 (BAD) if verb dispatch is broken
#   125 (SKIP) if build fails or test is inconclusive

set -e

echo "======================================"
echo "Bisect Test: Verb Dispatch Regression"
echo "======================================"
echo "Commit: $(git log -1 --oneline)"
echo ""

# Clean and rebuild
echo "1. Cleaning build..."
make clean >/dev/null 2>&1 || true
make -C frontier-cli clean >/dev/null 2>&1 || true

echo "2. Building frontier-cli..."
if ! env SANITIZE=1 make -C frontier-cli >/dev/null 2>&1; then
    echo "   ❌ Build failed - SKIP"
    exit 125
fi

echo "3. Testing verb dispatch..."

# Test 1: Basic verb (sizeOf)
if ./frontier-cli/frontier-cli -e "sizeOf('hello')" >/dev/null 2>&1; then
    echo "   ✅ sizeOf() works"
    TEST1=0
else
    echo "   ❌ sizeOf() fails"
    TEST1=1
fi

# Test 2: String verb
if ./frontier-cli/frontier-cli -e "string.upper('test')" >/dev/null 2>&1; then
    echo "   ✅ string.upper() works"
    TEST2=0
else
    echo "   ❌ string.upper() fails"
    TEST2=1
fi

# Test 3: Arithmetic (control - should always work)
if ./frontier-cli/frontier-cli -e "1+1" >/dev/null 2>&1; then
    echo "   ✅ Arithmetic works (control)"
    TEST3=0
else
    echo "   ❌ Arithmetic broken (unexpected)"
    TEST3=1
fi

echo ""

# If control test fails, skip this commit (build broken in unexpected way)
if [ $TEST3 -ne 0 ]; then
    echo "RESULT: SKIP - Arithmetic broken (unexpected build issue)"
    exit 125
fi

# If both verb tests pass, this commit is GOOD
if [ $TEST1 -eq 0 ] && [ $TEST2 -eq 0 ]; then
    echo "RESULT: GOOD - Verb dispatch working"
    exit 0
fi

# If either verb test fails, this commit is BAD
echo "RESULT: BAD - Verb dispatch broken"
exit 1
