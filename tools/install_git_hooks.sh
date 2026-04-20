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
    # Existing pre-commit hook - check if it's ours.
    # Version detection: compare HOOK_VERSION in source vs installed copy.
    # Older hooks without HOOK_VERSION are detected via legacy markers.
    SRC_VERSION=$(grep -E '^HOOK_VERSION=' "$PRE_COMMIT_SRC" 2>/dev/null | head -1 | cut -d= -f2)
    DST_VERSION=$(grep -E '^HOOK_VERSION=' "$PRE_COMMIT_DST" 2>/dev/null | head -1 | cut -d= -f2)

    if [ -n "$SRC_VERSION" ] && [ "$SRC_VERSION" = "$DST_VERSION" ]; then
        echo -e "${YELLOW}Pre-commit hook already installed (HOOK_VERSION=$DST_VERSION, up to date)${NC}"
    elif [ -n "$DST_VERSION" ] || grep -q -E "pre-commit-integration-tests|Block commits to develop" "$PRE_COMMIT_DST" 2>/dev/null; then
        # Either a versioned hook at a different version, or an older unversioned hook — upgrade.
        cp "$PRE_COMMIT_SRC" "$PRE_COMMIT_DST"
        chmod +x "$PRE_COMMIT_DST"
        if [ -n "$DST_VERSION" ]; then
            echo -e "${GREEN}✓ Upgraded pre-commit hook (HOOK_VERSION $DST_VERSION → $SRC_VERSION)${NC}"
        else
            echo -e "${GREEN}✓ Upgraded pre-commit hook (unversioned → HOOK_VERSION=$SRC_VERSION)${NC}"
        fi
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
echo "  • Blocks commits to 'develop' in the main worktree"
echo "    - Use feature branches in worktrees instead"
echo "    - Bypass with: git commit --no-verify"
echo "  • Rejects staged *.c/*.h files with leading-space indentation"
echo "    - Frontier requires tabs for C code (outline editor compatibility)"
echo "    - Only checks staged files; pre-existing files are not affected"
echo "  • When you commit changes to tests/integration/test_cases/*.yaml"
echo "    - The hook automatically regenerates reports/integration_tests.opml"
echo "    - The updated OPML is added to your commit"
echo "  • When you commit changes to CLAUDE.md or docs/*.md"
echo "    - The hook validates all documentation references"
echo "    - Prevents broken links and missing doc files"
