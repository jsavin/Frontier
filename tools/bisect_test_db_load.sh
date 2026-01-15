#!/bin/bash
# Automated test for git bisect: Does database load without segfault?
#
# Usage: git bisect run ./tools/bisect_test_db_load.sh
#
# Exit codes:
#   0   = good commit (database loads successfully)
#   1   = bad commit (database load fails/segfaults)
#   125 = skip commit (build failed, can't test)

set -e  # Exit on error (except where we handle it)

COMMIT_HASH=$(git rev-parse --short HEAD)
echo "========================================================================"
echo "=== Bisect test: Database load at commit $COMMIT_HASH"
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

# Step 2: Remove old v7 database
echo ""
echo "Step 2: Cleaning old v7 database..."
rm -f databases/Frontier-v6-v7.root
echo "✓ Old v7 database removed"

# Step 3: Restore v6 database to pristine state from git
echo ""
echo "Step 3: Restoring v6 database from git..."
cd databases
git checkout Frontier.root > /dev/null 2>&1 || true
cd ..
echo "✓ v6 database restored"

# Step 4: Migrate v6 to v7
# This follows the approach in run_headless_tests.sh:
#   1. Copy v6 to v7
#   2. Load it - migration happens automatically on first load
echo ""
echo "Step 4: Migrating v6 to v7..."

# Copy v6 database to v7 location
if ! cp databases/Frontier.root databases/Frontier-v6-v7.root ; then
    echo "✗ Failed to copy v6 database - skipping (exit 125)"
    exit 125
fi

# Trigger automatic migration by loading the v6 database
# Migration happens in-place when a v6 database is loaded
if ! ./frontier-cli/frontier-cli \
    --system-root databases/Frontier-v6-v7.root \
    -e "1" > /tmp/bisect_migrate.log 2>&1 ; then
    echo ""
    echo "========================================================================"
    echo "✗✗✗ BAD COMMIT: Migration failed/crashed"
    echo "========================================================================"
    echo ""
    echo "Migration log:"
    cat /tmp/bisect_migrate.log 2>/dev/null | tail -30 || echo "(no log file)"
    exit 1  # Bad commit - migration failure is the bug we're tracking
fi

echo "✓ Migration completed successfully"

# Step 5: Test database loads with simple expression
echo ""
echo "Step 5: Testing if database loads..."

if ./frontier-cli/frontier-cli \
    --system-root databases/Frontier-v6-v7.root \
    -e "1+1" > /dev/null 2>&1 ; then
    echo ""
    echo "========================================================================"
    echo "✓✓✓ GOOD COMMIT: Database loads successfully"
    echo "========================================================================"
    exit 0  # Good commit
else
    EXIT_CODE=$?
    echo ""
    echo "========================================================================"
    echo "✗✗✗ BAD COMMIT: Database load failed (exit code: $EXIT_CODE)"
    echo "========================================================================"

    # Try to get more info about the failure
    echo ""
    echo "Attempting to get error details..."
    ./frontier-cli/frontier-cli \
        --system-root databases/Frontier-v6-v7.root \
        -e "1+1" 2>&1 | tail -20 || true

    exit 1  # Bad commit
fi
