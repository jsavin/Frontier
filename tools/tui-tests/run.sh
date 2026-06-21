#!/bin/sh
# Convenience wrapper around the TUI test runner.
# Used by the /auto Test Manifest as a `skippable` TUI layer.
#
# Exit codes:
#   0 = all tests passed
#   1 = at least one test failed
#   2 = no tests discovered or harness error
#   3 = required infra missing (tmux)
#
# The runner itself notes when `aha` is missing (HTML snapshots are
# the optional feature; text + ANSI captures still work without it).

set -e

DIR="$(cd "$(dirname "$0")" && pwd)"

if ! command -v tmux > /dev/null 2>&1; then
    echo "FATAL: tmux not in PATH" >&2
    echo "Install with: brew install tmux" >&2
    exit 3
fi

# Optional: rebuild dist/ if requested
if [ "$1" = "--rebuild" ]; then
    make -C "$(git -C "$DIR" rev-parse --show-toplevel)/frontier-cli"
fi

exec python3 "$DIR/tui_harness.py"
