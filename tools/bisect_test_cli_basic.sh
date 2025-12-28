#!/bin/bash
# Automated test for git bisect: Does CLI execute basic expression?
#
# Usage: git bisect run ./tools/bisect_test_cli_basic.sh
#
# Exit codes:
#   0   = good commit (CLI works)
#   1   = bad commit (CLI segfaults or fails)
#   125 = skip commit (build failed, can't test)

set -e  # Exit on error (except where we handle it)

COMMIT_HASH=$(git rev-parse --short HEAD)
echo "========================================================================"
echo "=== Bisect test: CLI basic execution at commit $COMMIT_HASH"
echo "========================================================================"

# Step 1: Clean rebuild of CLI
echo ""
echo "Step 1: Rebuilding frontier-cli..."
make -C frontier-cli clean > /dev/null 2>&1

if ! make -C frontier-cli > /tmp/bisect_build.log 2>&1; then
    echo "✗ Build failed - skipping this commit (exit 125)"
    cat /tmp/bisect_build.log | tail -20
    exit 125  # Tell bisect to skip unbuildable commits
fi
echo "✓ CLI built successfully"

# Step 2: Test basic CLI execution without database
echo ""
echo "Step 2: Testing basic CLI execution (no database)..."

# Run CLI test (no timeout available on macOS)
if env FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "1+1" > /tmp/bisect_cli.log 2>&1 ; then
    RESULT=$(cat /tmp/bisect_cli.log)
    if [ "$RESULT" = "2" ]; then
        echo ""
        echo "========================================================================"
        echo "✓✓✓ GOOD COMMIT: CLI executed successfully (result: $RESULT)"
        echo "========================================================================"
        exit 0  # Good commit
    else
        echo ""
        echo "========================================================================"
        echo "✗✗✗ BAD COMMIT: CLI returned wrong result: '$RESULT'"
        echo "========================================================================"
        exit 1  # Bad commit
    fi
else
    EXIT_CODE=$?
    echo ""
    echo "========================================================================"
    echo "✗✗✗ BAD COMMIT: CLI crashed or timed out (exit code: $EXIT_CODE)"
    echo "========================================================================"

    # Show any output
    echo ""
    echo "CLI output:"
    cat /tmp/bisect_cli.log 2>/dev/null | tail -20 || echo "(no output)"

    exit 1  # Bad commit
fi
