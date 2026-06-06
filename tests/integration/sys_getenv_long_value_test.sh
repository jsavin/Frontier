#!/bin/bash
# sys_getenv_long_value_test.sh - issue #716 item 3
#
# Behavioral regression test locking in the headless behavior of
# sys.getenvironmentvariable for OS-level env var values > 255 bytes.
#
# Background:
#   The audit in #716 item 3 pointed at Common/source/shellsysverbs.c:594,
#   which uses copyctopstring into a bigstring and rejects values > 255
#   bytes with a langerrormessage. That file is the legacy (full Mac app)
#   implementation; it is NOT linked into the headless frontier-cli build.
#
#   The actual headless path (tests/headless_sys_verbs.c::sysv_getenvironmentvariable)
#   uses newfilledhandle + setheapvalue, returning a heap-allocated string
#   handle that supports arbitrary length. There is no bigstring on this
#   path, and therefore no 255-byte truncation surface.
#
# What this test asserts:
#   1. A 200-byte env var (control) reads back at length 200.
#   2. A 300-byte env var reads back at length 300 -- not 255, not an
#      error. This is the design behavior of the heap-handle path; the
#      test guards against a future regression that mistakenly applied
#      a bigstring guard to the headless path.
#   3. The value content is preserved verbatim (no truncation at any
#      byte boundary).
#
# Test isolation: env var is scoped to the test's subshell only. No
# temp files needed.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
SYSTEM_ROOT="$PROJECT_ROOT/dist/Frontier.root"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

PASS=0
FAIL=0
TOTAL=0

pass() {
    echo -e "${GREEN}PASS${NC}: $1"
    PASS=$((PASS + 1))
    TOTAL=$((TOTAL + 1))
}

fail() {
    echo -e "${RED}FAIL${NC}: $1"
    if [ -n "${2:-}" ]; then
        echo "  $2"
    fi
    FAIL=$((FAIL + 1))
    TOTAL=$((TOTAL + 1))
}

echo "============================================================"
echo "  sys.getenvironmentvariable >255-byte regression (#716 #3)"
echo "============================================================"
echo ""

if [ ! -x "$CLI" ]; then
    echo -e "${RED}ERROR${NC}: CLI binary not found at $CLI"
    echo "Run 'make build' first."
    exit 2
fi

if [ ! -f "$SYSTEM_ROOT" ]; then
    echo -e "${RED}ERROR${NC}: Frontier.root not found at $SYSTEM_ROOT"
    echo "Run 'make build' to produce dist/Frontier.root."
    exit 2
fi

# Filter helper: strip warning lines so the assertion sees only the
# script's stdout result.
strip_warnings() {
    grep -vE '^\[(table|lang|startup|general|headless)-(WARN|ERROR|INFO)\]' || true
}

# --- 200-byte control ---
VALUE_200=$(printf 'x%.0s' $(seq 1 200))
LEN_200=$(FRONTIER_TEST_LONG_VAR="$VALUE_200" "$CLI" --system-root "$SYSTEM_ROOT" --batch -e \
    'string.length(sys.getenvironmentvariable("FRONTIER_TEST_LONG_VAR"))' 2>&1 | strip_warnings | tail -1)

if [ "$LEN_200" = "200" ]; then
    pass "200-byte env var reads back at length 200 (control)"
else
    fail "200-byte env var expected length 200, got '$LEN_200'"
fi

# --- 300-byte regression guard: must return full length, not truncated ---
# Mixed-content payload: 150 'A's then 150 'B's. Length-only assertions
# would pass even if internal bytes were silently rewritten; mid-buffer
# markers around byte 150/151 catch that class of regression too.
VALUE_300="$(printf 'A%.0s' $(seq 1 150))$(printf 'B%.0s' $(seq 1 150))"
LEN_300=$(FRONTIER_TEST_LONG_VAR="$VALUE_300" "$CLI" --system-root "$SYSTEM_ROOT" --batch -e \
    'string.length(sys.getenvironmentvariable("FRONTIER_TEST_LONG_VAR"))' 2>&1 | strip_warnings | tail -1)

if [ "$LEN_300" = "300" ]; then
    pass "300-byte env var reads back at length 300 (heap-handle path, no 255-byte truncation)"
elif [ "$LEN_300" = "255" ]; then
    fail "300-byte env var was truncated to 255 -- regression: someone applied a bigstring guard" \
         "got length=255, expected 300"
else
    fail "300-byte env var unexpected length" \
         "got '$LEN_300', expected 300"
fi

# --- content fidelity: A/B boundary preserved at byte 150/151, and last
# byte (300) is the final 'B'. Together these catch length-preserving
# corruption that a single end-of-string check would miss. ---
PROBE=$(FRONTIER_TEST_LONG_VAR="$VALUE_300" "$CLI" --system-root "$SYSTEM_ROOT" --batch -e \
    'string.mid(sys.getenvironmentvariable("FRONTIER_TEST_LONG_VAR"), 150, 1) + string.mid(sys.getenvironmentvariable("FRONTIER_TEST_LONG_VAR"), 151, 1) + string.mid(sys.getenvironmentvariable("FRONTIER_TEST_LONG_VAR"), 300, 1)' 2>&1 | strip_warnings | tail -1)

if [ "$PROBE" = "ABB" ]; then
    pass "300-byte env var content preserved verbatim across mid-buffer + tail markers"
else
    fail "300-byte env var content corrupted somewhere in the buffer" \
         "got '$PROBE', expected 'ABB' (byte 150='A', byte 151='B', byte 300='B')"
fi

echo ""
echo "============================================================"
echo "  Results: $PASS passed, $FAIL failed, $TOTAL total"
echo "============================================================"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
