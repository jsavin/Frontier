#!/bin/bash
# Git bisect script to find when string(123) broke
# Tests if string(123) works correctly (should return "123")

cd "$(dirname "$0")"

# Clean and rebuild
make -C frontier-cli clean > /dev/null 2>&1
make -C frontier-cli > /dev/null 2>&1

if [ ! -f frontier-cli/frontier-cli ]; then
    echo "Build failed"
    exit 125  # Skip this commit
fi

# Clean up any legacy migration artifacts
rm -f databases/Frontier.root7 databases/Frontier.v6.root

# Test string(123) - should work
OUTPUT=$(./frontier-cli/frontier-cli -e "string(123)" 2>&1)
EXIT_CODE=$?

COMMIT=$(git rev-parse --short HEAD)

# Check for the specific error that indicates string() is broken
if echo "$OUTPUT" | grep -q "missing valueroutine"; then
    echo "COMMIT $COMMIT: BAD - string(123) broken (missing valueroutine error)"
    exit 1  # Bad commit
elif echo "$OUTPUT" | grep -q "does not exist"; then
    echo "COMMIT $COMMIT: BAD - string(123) broken (verb does not exist)"
    exit 1  # Bad commit
elif [ $EXIT_CODE -ne 0 ]; then
    echo "COMMIT $COMMIT: BAD - string(123) failed with exit code $EXIT_CODE"
    echo "Output: $OUTPUT"
    exit 1  # Bad commit
else
    # Check if it returned "123" (the expected result)
    if echo "$OUTPUT" | grep -q "123"; then
        echo "COMMIT $COMMIT: GOOD - string(123) works correctly"
        exit 0  # Good commit
    else
        echo "COMMIT $COMMIT: UNCERTAIN - no error but unexpected output"
        echo "Output: $OUTPUT"
        exit 1  # Treat as bad to be safe
    fi
fi
