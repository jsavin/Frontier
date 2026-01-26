#!/bin/bash
cd "$(dirname "$0")"

# Baseline file to track "known good" failure count
BASELINE_FILE="/tmp/frontier_test_baseline.txt"

# Clean and rebuild
make -C frontier-cli clean > /dev/null 2>&1
make -C frontier-cli > /dev/null 2>&1

if [ ! -f frontier-cli/frontier-cli ]; then
    echo "Build failed"
    exit 125  # Skip this commit
fi

# Force fresh database migration
rm -f databases/Frontier.root7

# Run unit tests and count failures
UNIT_OUTPUT=$(./tools/run_headless_tests.sh 2>&1)
UNIT_FAILURES=$(echo "$UNIT_OUTPUT" | grep "FAIL:" | wc -l | tr -d ' ')

# Run integration tests and count failures
cd tests
INT_OUTPUT=$(make test-integration 2>&1)
INT_FAILURES=$(echo "$INT_OUTPUT" | grep "FAILED:" | wc -l | tr -d ' ')
cd ..

TOTAL_FAILURES=$((UNIT_FAILURES + INT_FAILURES))

echo "Commit $(git rev-parse --short HEAD): Unit=$UNIT_FAILURES, Integration=$INT_FAILURES, Total=$TOTAL_FAILURES"

# If baseline doesn't exist, we're establishing it
if [ ! -f "$BASELINE_FILE" ]; then
    echo "$TOTAL_FAILURES" > "$BASELINE_FILE"
    echo "BASELINE established: $TOTAL_FAILURES failures"
    exit 0  # Good (baseline)
fi

# Read baseline
BASELINE=$(cat "$BASELINE_FILE" | tr -d ' ')
DELTA=$((TOTAL_FAILURES - BASELINE))

echo "Baseline=$BASELINE, Current=$TOTAL_FAILURES, Delta=$DELTA"

# Regression = >5 NEW failures compared to baseline
if [ $DELTA -gt 5 ]; then
    echo "BAD: Regression detected (+$DELTA failures)"
    exit 1  # Bad commit
else
    echo "GOOD: Within acceptable range ($DELTA new failures)"
    exit 0  # Good commit
fi
