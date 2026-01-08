#!/bin/bash
# Install git hooks for Frontier development
#
# This script installs pre-commit hooks that automatically maintain
# generated files when source changes are committed.

set -e

# Get the root of the git repo
GIT_ROOT=$(git rev-parse --show-toplevel)
HOOKS_DIR="$GIT_ROOT/.git/hooks"
TOOLS_HOOKS_DIR="$GIT_ROOT/tools/hooks"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "Installing Frontier git hooks..."

# Install pre-commit hook for integration tests OPML export
PRE_COMMIT_SRC="$TOOLS_HOOKS_DIR/pre-commit-integration-tests"
PRE_COMMIT_DST="$HOOKS_DIR/pre-commit"

if [ -f "$PRE_COMMIT_DST" ]; then
    # Existing pre-commit hook - check if it's ours
    if grep -q "pre-commit-integration-tests" "$PRE_COMMIT_DST" 2>/dev/null; then
        echo -e "${YELLOW}Pre-commit hook already installed${NC}"
    else
        echo -e "${YELLOW}Warning: Existing pre-commit hook found${NC}"
        echo "You have an existing pre-commit hook. To use both hooks:"
        echo "  1. Backup your existing hook: cp $PRE_COMMIT_DST $PRE_COMMIT_DST.backup"
        echo "  2. Manually merge the hooks, or"
        echo "  3. Run: cat $PRE_COMMIT_SRC >> $PRE_COMMIT_DST"
        exit 1
    fi
else
    # No existing hook, install ours
    cp "$PRE_COMMIT_SRC" "$PRE_COMMIT_DST"
    chmod +x "$PRE_COMMIT_DST"
    echo -e "${GREEN}✓ Installed pre-commit hook for integration tests OPML export${NC}"
fi

echo ""
echo -e "${GREEN}Git hooks installed successfully!${NC}"
echo ""
echo "What this does:"
echo "  • When you commit changes to tests/integration/test_cases/*.yaml"
echo "  • The hook automatically regenerates reports/integration_tests.opml"
echo "  • The updated OPML is added to your commit"
echo ""
echo "This ensures Dave's OPML subscription always sees the latest tests!"
