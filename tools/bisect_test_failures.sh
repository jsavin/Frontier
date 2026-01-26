#!/bin/bash
cd "$(dirname "$0")"

# Clean and rebuild
make -C frontier-cli clean > /dev/null 2>&1
make -C frontier-cli > /dev/null 2>&1

if [ ! -f frontier-cli/frontier-cli ]; then
    echo "Build failed"
    exit 125  # Skip this commit
fi

# Force fresh database migration
rm -f databases/Frontier.root7

# Run unit tests
UNIT_OUTPUT=$(./tools/run_headless_tests.sh 2>&1)
UNIT_EXIT=$?

# Run integration tests
cd tests
INT_OUTPUT=$(make test-integration 2>&1)
INT_EXIT=$?
cd ..

# Count failures
UNIT_FAILURES=$(echo "$UNIT_OUTPUT" | grep -c "FAIL:" || echo "0")
INT_FAILURES=$(echo "$INT_OUTPUT" | grep -c "FAILED:" || echo "0")

TOTAL_FAILURES=$((UNIT_FAILURES + INT_FAILURES))

echo "Commit $(git rev-parse --short HEAD): Unit failures=$UNIT_FAILURES, Integration failures=$INT_FAILURES, Total=$TOTAL_FAILURES"

# Consider "severe breakage" as > 5 total failures
if [ $TOTAL_FAILURES -gt 5 ]; then
    echo "BAD: Severe test breakage ($TOTAL_FAILURES failures)"
    exit 1  # Bad commit
else
    echo "GOOD: Acceptable test results ($TOTAL_FAILURES failures)"
    exit 0  # Good commit
fi
