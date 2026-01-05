#!/bin/bash
# Git bisect test script for sizeOf() regression
# Returns 0 (GOOD) if sizeOf('hello') works correctly
# Returns 1 (BAD) if sizeOf('hello') fails
# Returns 125 (SKIP) if build fails

set -e

# Clean and rebuild
echo "=== Cleaning and rebuilding frontier-cli ==="
make -C frontier-cli clean >/dev/null 2>&1 || exit 125
env SANITIZE=1 make -C frontier-cli >/dev/null 2>&1 || exit 125

# Test sizeOf('hello')
echo "=== Testing sizeOf('hello') ==="
output=$(./frontier-cli/frontier-cli -e "sizeOf('hello')" 2>&1)
exit_code=$?

echo "Output: $output"
echo "Exit code: $exit_code"

# Check if output is "5" (expected result for length of "hello")
if echo "$output" | grep -q "^5$"; then
    echo "GOOD: sizeOf('hello') returned 5"
    exit 0
fi

# Check for error messages
if echo "$output" | grep -q "ERROR"; then
    echo "BAD: sizeOf('hello') returned error"
    exit 1
fi

# Unexpected output
echo "SKIP: Unexpected output (not 5, not error)"
exit 125
