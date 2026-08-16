#!/bin/bash
#
# Integration tests for the headless sys.openUrl GUI-browser guard (#891).
#
# Legacy Frontier launched the owner's browser at the setupFrontier page to
# collect config from the server owner. That onboarding intent is preserved;
# only the host-GUI launch from a headless/server/test context is wrong.
#
# Headless default: no process exec. The URL is logged and recorded at
# system.temp.Frontier.pendingBrowserUrl so flows and tests can assert the
# onboarding intent was reached.
# Opt-in (--browser allow-gui): the real launcher runs again.
#
# HARD RULE: no test here may launch a host GUI browser. The opt-in exec is
# proven with a PATH-shadowing stub named "open" that records its argv; the
# real /usr/bin/open is never reached because execlp resolves through PATH.
#
# These tests use -e mode (not --protocol) because the protocol runner cannot
# pass arbitrary unknown flags to the frontier-cli subprocess.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
SOURCE_DB="$PROJECT_ROOT/databases/Frontier.root"

if [ ! -x "$CLI" ]; then
    echo "Error: frontier-cli not found at $CLI" >&2
    exit 1
fi
if [ ! -f "$SOURCE_DB" ]; then
    echo "Error: database not found at $SOURCE_DB" >&2
    exit 1
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

PASSED=0
FAILED=0

TESTURL="http://127.0.0.1:5336/setupFrontier"

# Scratch dir holding the PATH stubs, their recording file, and a private copy
# of the database. The CLI opens the system root read-write, so these runs must
# never point at the shared working DB in databases/.
STUBDIR=$(mktemp -d "${TMPDIR:-/tmp}/openurl_guard.XXXXXX")
RECORD="$STUBDIR/exec_record.txt"
DB="$STUBDIR/Frontier.root"

cleanup() {
    rm -rf "$STUBDIR"
}
trap cleanup EXIT

cp "$SOURCE_DB" "$DB" || {
    echo "Error: could not copy $SOURCE_DB to the scratch dir" >&2
    exit 1
}

# Stub launchers. Named exactly as the ones execlp() resolves through PATH, so
# a stub firing proves an exec happened and the real browser is never invoked.
for stub in open xdg-open agent-browser; do
    cat > "$STUBDIR/$stub" <<STUBEOF
#!/bin/bash
echo "$stub \$*" >> "$RECORD"
exit 0
STUBEOF
    chmod +x "$STUBDIR/$stub"
done

pass() {
    echo -e "  ${GREEN}PASS${NC}: $1"
    PASSED=$((PASSED + 1))
}

fail() {
    echo -e "  ${RED}FAIL${NC}: $1"
    echo "    $2"
    FAILED=$((FAILED + 1))
}

# Run the CLI with the stub dir FIRST on PATH so any exec hits a stub.
# The recording file is truncated per run so each assertion is independent.
run_cli() {
    : > "$RECORD"
    PATH="$STUBDIR:$PATH" "$CLI" "$@" 2>/dev/null
}

# The double-fork means the grandchild runs asynchronously; give a real exec a
# moment to land before concluding the record is empty.
settle() {
    local i
    for i in 1 2 3 4 5 6 7 8 9 10; do
        if [ -s "$RECORD" ]; then
            return
        fi
        sleep 0.1
    done
}

echo "=============================================="
echo "Headless sys.openUrl GUI-browser guard (#891)"
echo "=============================================="
echo

echo "--- Default (no --browser): log and mark, never exec ---"

actual=$(run_cli --skip-startup --system-root "$DB" \
    -e "sys.openUrl(\"$TESTURL\")")
if [ "$actual" = "true" ]; then
    pass "default returns true (onboarding intent honored)"
else
    fail "default returns true (onboarding intent honored)" "Got: '$actual'"
fi

settle
if [ ! -s "$RECORD" ]; then
    pass "default performs NO exec"
else
    fail "default performs NO exec" "Exec recorded: $(cat "$RECORD")"
fi

actual=$(run_cli --skip-startup --system-root "$DB" \
    -e "sys.openUrl(\"$TESTURL\");system.temp.Frontier.pendingBrowserUrl")
if [ "$actual" = "$TESTURL" ]; then
    pass "default sets pendingBrowserUrl to the URL"
else
    fail "default sets pendingBrowserUrl to the URL" "Expected '$TESTURL', got '$actual'"
fi

echo
echo "--- Explicit --browser default: same as absent ---"

actual=$(run_cli --skip-startup --system-root "$DB" --browser default \
    -e "sys.openUrl(\"$TESTURL\")")
if [ "$actual" = "true" ]; then
    pass "--browser default returns true"
else
    fail "--browser default returns true" "Got: '$actual'"
fi

settle
if [ ! -s "$RECORD" ]; then
    pass "--browser default performs NO exec"
else
    fail "--browser default performs NO exec" "Exec recorded: $(cat "$RECORD")"
fi

echo
echo "--- Marker overwrites on each call (last URL wins) ---"

actual=$(run_cli --skip-startup --system-root "$DB" \
    -e "sys.openUrl(\"http://example.com/first\");sys.openUrl(\"$TESTURL\");system.temp.Frontier.pendingBrowserUrl")
if [ "$actual" = "$TESTURL" ]; then
    pass "marker holds the most recent URL"
else
    fail "marker holds the most recent URL" "Expected '$TESTURL', got '$actual'"
fi

echo
echo "--- Opt-in --browser allow-gui: real launcher runs (stubbed) ---"

actual=$(run_cli --skip-startup --system-root "$DB" --browser allow-gui \
    -e "sys.openUrl(\"$TESTURL\")")
if [ "$actual" = "true" ]; then
    pass "allow-gui returns true"
else
    fail "allow-gui returns true" "Got: '$actual'"
fi

settle
if grep -q "$TESTURL" "$RECORD" 2>/dev/null; then
    pass "allow-gui EXECS the launcher with the URL"
else
    fail "allow-gui EXECS the launcher with the URL" "Record: '$(cat "$RECORD" 2>/dev/null)'"
fi

echo
echo "--- Opt-in does NOT set the pending marker ---"

actual=$(run_cli --skip-startup --system-root "$DB" --browser allow-gui \
    -e "sys.openUrl(\"$TESTURL\");defined(system.temp.Frontier.pendingBrowserUrl)")
if [ "$actual" = "false" ]; then
    pass "allow-gui leaves pendingBrowserUrl undefined"
else
    fail "allow-gui leaves pendingBrowserUrl undefined" "Got: '$actual'"
fi

echo
echo "--- agent-browser still execs (automation path unchanged) ---"

actual=$(run_cli --skip-startup --system-root "$DB" --browser agent-browser \
    -e "sys.openUrl(\"$TESTURL\")")
if [ "$actual" = "true" ]; then
    pass "agent-browser returns true"
else
    fail "agent-browser returns true" "Got: '$actual'"
fi

settle
if grep -q "^agent-browser .*$TESTURL" "$RECORD" 2>/dev/null; then
    pass "agent-browser EXECS agent-browser with the URL"
else
    fail "agent-browser EXECS agent-browser with the URL" "Record: '$(cat "$RECORD" 2>/dev/null)'"
fi

echo
echo "--- Unknown --browser value is rejected ---"

actual=$(run_cli --skip-startup --system-root "$DB" --browser firefox \
    -e "sys.openUrl(\"$TESTURL\")")
if [ "$actual" != "true" ]; then
    pass "unknown browser value does not report success"
else
    fail "unknown browser value does not report success" "Got: '$actual'"
fi

settle
if [ ! -s "$RECORD" ]; then
    pass "unknown browser value performs NO exec"
else
    fail "unknown browser value performs NO exec" "Exec recorded: $(cat "$RECORD")"
fi

echo
echo "--- Empty URL still errors before any fork ---"

actual=$(run_cli --skip-startup --system-root "$DB" -e 'sys.openUrl("")')
if [ "$actual" != "true" ]; then
    pass "empty URL does not report success"
else
    fail "empty URL does not report success" "Got: '$actual'"
fi

settle
if [ ! -s "$RECORD" ]; then
    pass "empty URL performs NO exec"
else
    fail "empty URL performs NO exec" "Exec recorded: $(cat "$RECORD")"
fi

echo
echo "=============================================="
echo "Passed: $PASSED  Failed: $FAILED"
echo "=============================================="

if [ "$FAILED" -ne 0 ]; then
    exit 1
fi
exit 0
