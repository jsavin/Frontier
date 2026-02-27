#!/bin/bash
# Returns a temporary directory path for frontier-cli testing
# Usage: TESTDIR=$(./tools/get_test_temp_path.sh)
#
# This script returns a project-relative path in a .gitignore'd directory.
# Note: /tmp is also accessible and can be used directly in tests.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_TMP_DIR="$PROJECT_ROOT/tests/tmp/unit"

# Create directory if it doesn't exist
mkdir -p "$TEST_TMP_DIR"

# Output the path (for capture via command substitution)
echo "$TEST_TMP_DIR"
