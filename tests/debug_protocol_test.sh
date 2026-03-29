#!/bin/bash
#
# Integration tests for debug/* protocol operations
# Tests the debugger MVP: debug/run, debug/continue, debug/kill, debug/pause
#
# Uses multi-line protocol sessions with timed delays between commands.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
DB="$PROJECT_ROOT/databases/Virgin.root"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

PASSED=0
FAILED=0

if [ ! -x "$CLI" ]; then
    echo "Error: frontier-cli not found at $CLI" >&2
    exit 1
fi
if [ ! -f "$DB" ]; then
    echo "Error: database not found at $DB" >&2
    exit 1
fi

# Run a debug protocol session and capture output
# Args: test_name expected_patterns... -- commands...
run_debug_test() {
    local name="$1"
    shift

    # Collect expected patterns until --
    local expected=()
    while [ "$1" != "--" ]; do
        expected+=("$1")
        shift
    done
    shift  # skip --

    # Remaining args are the commands (with sleep delays)
    local output
    local exit_code=0
    output=$(eval "$@" 2>/dev/null) || exit_code=$?

    # Check each expected pattern
    local all_found=true
    local missing=""
    for pattern in "${expected[@]}"; do
        if ! echo "$output" | grep -q "$pattern"; then
            all_found=false
            missing="$missing  Missing: $pattern\n"
        fi
    done

    if $all_found; then
        echo -e "  ${GREEN}✓ PASS${NC}: $name"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}✗ FAIL${NC}: $name"
        echo -e "$missing"
        echo "  Output was:"
        echo "$output" | sed 's/^/    /'
        if [ "$exit_code" -ne 0 ]; then
            echo "  CLI exit code: $exit_code"
        fi
        FAILED=$((FAILED + 1))
    fi
}

echo "=============================================="
echo "Debug Protocol Tests"
echo "=============================================="
echo

echo "--- debug/run + debug/continue ---"
run_debug_test "run and continue simple expression" \
    '"threadId":3' \
    '"reason":"entry"' \
    '"success":true' \
    '"op":"debug/completed"' \
    -- \
    '(echo '"'"'{"op":"debug/run","id":1,"params":{"expression":"return 1+1"}}'"'"'; sleep 2; echo '"'"'{"op":"debug/continue","id":2,"params":{"threadId":3}}'"'"'; sleep 2; echo '"'"'{"op":"shutdown","id":99}'"'"'; sleep 1) | '"$CLI"' --protocol --skip-startup --system-root '"$DB"''

echo
echo "--- debug/run + debug/kill ---"
run_debug_test "run and kill" \
    '"threadId":3' \
    '"reason":"entry"' \
    '"status":"killed"' \
    '"op":"debug/completed"' \
    '"success":false' \
    -- \
    '(echo '"'"'{"op":"debug/run","id":1,"params":{"expression":"return 1+1"}}'"'"'; sleep 2; echo '"'"'{"op":"debug/kill","id":2,"params":{"threadId":3}}'"'"'; sleep 2; echo '"'"'{"op":"shutdown","id":99}'"'"'; sleep 1) | '"$CLI"' --protocol --skip-startup --system-root '"$DB"''

echo
echo "--- debug/pause on running thread ---"
run_debug_test "pause interrupts running loop" \
    '"reason":"entry"' \
    '"reason":"interrupted"' \
    '"status":"interrupting"' \
    -- \
    '(echo '"'"'{"op":"debug/run","id":1,"params":{"expression":"local (i); for i = 1 to 1000000 {i = i}; return true"}}'"'"'; sleep 1; echo '"'"'{"op":"debug/continue","id":2,"params":{"threadId":3}}'"'"'; sleep 1; echo '"'"'{"op":"debug/pause","id":3,"params":{"threadId":3}}'"'"'; sleep 2; echo '"'"'{"op":"debug/kill","id":4,"params":{"threadId":3}}'"'"'; sleep 1; echo '"'"'{"op":"shutdown","id":99}'"'"'; sleep 1) | '"$CLI"' --protocol --skip-startup --system-root '"$DB"''

echo
echo "--- error cases ---"
run_debug_test "continue with invalid threadId" \
    '"No debug thread with that ID"' \
    -- \
    '(echo '"'"'{"op":"debug/continue","id":1,"params":{"threadId":999}}'"'"'; sleep 1; echo '"'"'{"op":"shutdown","id":99}'"'"'; sleep 1) | '"$CLI"' --protocol --skip-startup --system-root '"$DB"''

run_debug_test "kill with invalid threadId" \
    '"No debug thread with that ID"' \
    -- \
    '(echo '"'"'{"op":"debug/kill","id":1,"params":{"threadId":999}}'"'"'; sleep 1; echo '"'"'{"op":"shutdown","id":99}'"'"'; sleep 1) | '"$CLI"' --protocol --skip-startup --system-root '"$DB"''

run_debug_test "run with missing expression" \
    'expression' \
    'success.*false' \
    -- \
    '(echo '"'"'{"op":"debug/run","id":1,"params":{}}'"'"'; sleep 1; echo '"'"'{"op":"shutdown","id":99}'"'"'; sleep 1) | '"$CLI"' --protocol --skip-startup --system-root '"$DB"''

echo
echo "=============================================="
echo "RESULTS: $PASSED passed, $FAILED failed"
echo "=============================================="

exit $FAILED
