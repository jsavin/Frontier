#!/bin/bash
#
# Integration tests for arbitrary CLI arguments in system.environment.args
# Tests the two-pass parser and custom arg handling.
#
# These tests use -e mode (not --protocol) because the protocol runner
# cannot pass arbitrary unknown flags to the frontier-cli subprocess.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
DB="$PROJECT_ROOT/databases/Frontier.root"

if [ ! -x "$CLI" ]; then
    echo "Error: frontier-cli not found at $CLI" >&2
    exit 1
fi
if [ ! -f "$DB" ]; then
    echo "Error: database not found at $DB" >&2
    exit 1
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

PASSED=0
FAILED=0

run_test() {
    local name="$1"
    local expected="$2"
    shift 2
    local actual
    actual=$("$CLI" "$@" 2>/dev/null) || true

    if [ "$actual" = "$expected" ]; then
        echo -e "  ${GREEN}✓ PASS${NC}: $name"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}✗ FAIL${NC}: $name"
        echo "    Expected: '$expected'"
        echo "    Got:      '$actual'"
        FAILED=$((FAILED + 1))
    fi
}

echo "=============================================="
echo "Custom CLI Arguments Tests"
echo "=============================================="
echo

echo "--- Unknown flag with value ---"
run_test "browser flag with value" "agent-browser" \
    --skip-startup --system-root "$DB" --browser agent-browser \
    -e 'system.environment.args.browser'

run_test "custom flag with value" "hello-world" \
    --skip-startup --system-root "$DB" --my-flag hello-world \
    -e 'system.environment.args.myFlag'

echo
echo "--- Unknown boolean flag ---"
run_test "boolean flag (no value)" "true" \
    --skip-startup --system-root "$DB" --my-flag \
    -e 'system.environment.args.myFlag'

echo
echo "--- kebab-case to camelCase conversion ---"
run_test "single-hyphen flag" "true" \
    --skip-startup --system-root "$DB" --my-custom-flag \
    -e 'system.environment.args.myCustomFlag'

run_test "multi-hyphen flag with value" "test-value" \
    --skip-startup --system-root "$DB" --my-long-flag-name test-value \
    -e 'system.environment.args.myLongFlagName'

echo
echo "--- Multiple unknown flags ---"
run_test "first of multiple flags" "bar" \
    --skip-startup --system-root "$DB" --foo bar --baz \
    -e 'system.environment.args.foo'

run_test "second of multiple flags (boolean)" "true" \
    --skip-startup --system-root "$DB" --foo bar --baz \
    -e 'system.environment.args.baz'

echo
echo "--- Known flags still work alongside unknowns ---"
run_test "skipStartup with unknown flag" "true" \
    --skip-startup --system-root "$DB" --my-extra extra \
    -e 'system.environment.args.skipStartup'

run_test "systemRoot with unknown flag" "true" \
    --skip-startup --system-root "$DB" --my-extra extra \
    -e 'system.environment.args.systemRoot contains "Frontier.root"'

echo
echo "--- Inline --flag=value syntax ---"
run_test "inline equals syntax with value" "hello" \
    --skip-startup --system-root "$DB" --my-flag=hello \
    -e 'system.environment.args.myFlag'

run_test "inline equals syntax with kebab-case" "world" \
    --skip-startup --system-root "$DB" --my-long-flag=world \
    -e 'system.environment.args.myLongFlag'

run_test "inline browser=agent-browser" "agent-browser" \
    --skip-startup --system-root "$DB" --browser=agent-browser \
    -e 'system.environment.args.browser'

echo
echo "--- Duplicate unknown flags (last wins) ---"
run_test "duplicate flag last value wins" "second" \
    --skip-startup --system-root "$DB" --foo first --foo second \
    -e 'system.environment.args.foo'

echo
echo "--- Absent unknown flags ---"
run_test "undefined unknown flag" "false" \
    --skip-startup --system-root "$DB" \
    -e 'defined(system.environment.args.myFlag)'

echo
echo "--- Browser flag validation (via openUrl error) ---"
run_test "invalid browser value rejected" "true" \
    --skip-startup --system-root "$DB" --browser invalid-browser \
    -e 'try {sys.openUrl("http://example.com"); return false} else {return true}'

run_test "default browser accepted (no error)" "true" \
    --skip-startup --system-root "$DB" --browser default \
    -e 'typeof(system.environment.args.browser) == stringType'

echo
echo "=============================================="
echo "RESULTS: $PASSED passed, $FAILED failed"
echo "=============================================="

exit $FAILED
