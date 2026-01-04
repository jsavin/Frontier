#!/bin/bash
# Returns a safe temporary directory path for frontier-cli testing
# Usage: TESTDIR=$(./tools/get_test_temp_path.sh)
#
# frontier-cli runs in the macOS sandbox and CANNOT access /tmp.
# This script returns a project-relative path in a .gitignore'd directory.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_TMP_DIR="$PROJECT_ROOT/test_tmp"

# Create directory if it doesn't exist
mkdir -p "$TEST_TMP_DIR"

# Output the path (for capture via command substitution)
echo "$TEST_TMP_DIR"
