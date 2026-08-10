#!/bin/bash
#
# Unit 1.4 -- agent-driven debugging end-to-end over --protocol.
#
# Thin wrapper: stages Virgin.root into a tmpdir (same isolation pattern as
# debug_protocol_test.sh, issue #644) and runs the Python session driver
# tests/integration/agent_debug_session_test.py. No PTY, no ports, no REPL:
# the session speaks NDJSON over plain pipes.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
SOURCE_DB="$PROJECT_ROOT/databases/Virgin.root"

if [ ! -x "$CLI" ]; then
    echo "Error: frontier-cli not found at $CLI" >&2
    exit 1
fi
if [ ! -f "$SOURCE_DB" ]; then
    echo "Error: database not found at $SOURCE_DB" >&2
    exit 1
fi

STAGE_DIR="$(mktemp -d -t frontier-agent-debug-XXXXXX)"
DB="$STAGE_DIR/Virgin.root"
cp "$SOURCE_DB" "$DB"
trap 'rm -rf "$STAGE_DIR"' EXIT

export DEBUG_TEST_CLI="$CLI"
export DEBUG_TEST_DB="$DB"

python3 "$SCRIPT_DIR/integration/agent_debug_session_test.py"
