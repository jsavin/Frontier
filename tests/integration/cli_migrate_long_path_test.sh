#!/bin/bash
# cli_migrate_long_path_test.sh - issue #712
#
# Defense-in-depth test for copyctopstring callsites in the --migrate flow.
#
# Background: PR #708 fixed a stack buffer overflow in copyctopstring() by
# clamping the payload to 255 bytes and returning a boolean (false on
# truncation). The overflow class is gone, BUT three callsites in
# Common/source/db_format.c (lines 2151, 2378, 2493) discard the return,
# meaning a >255-byte path is silently truncated to its 255-byte prefix
# and then handed to pathtofilespec / openfile / opennewfile.
#
# The exit criterion in issue #712:
#   "Add an integration test that supplies a >255-byte path to --migrate
#    and confirms the truncation is surfaced as an error rather than
#    silently using the prefix."
#
# What this test asserts (will fail today, pass after the fix):
#   1. --migrate exits non-zero on a >255-byte path (true today, but for
#      the WRONG reason -- openfile fails on the truncated prefix).
#   2. stderr contains a clear truncation message (e.g. "too long",
#      "truncated", "exceeds 255"). Today the user sees "openfile(src)"
#      which is the same generic error they would see for permission
#      denied, missing file, etc. -- it does not tell them the actual
#      cause is a length-clamp.
#   3. No leftover .v7.tmp file at the truncated-prefix location -- a
#      silent truncation could have created garbage at the wrong path.
#
# Reference fix-shape pattern: frontier-cli/window_registry.c:116
#   if (!copyctopstring(window_path, bs_path)) {
#       log_warn(..., "exceeded 255 bytes and was truncated; skipping");
#       return;
#   }
#
# Test isolation: all temp state lives under tests/tmp/integration/
# (per testing_rules / docs/AI_SHARED_GUIDELINES.md). Cleanup via trap.

set -u  # NOT set -e -- we want to inspect failing exit codes manually

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
V6_FIXTURE="$PROJECT_ROOT/tests/fixtures/v6/test.root"

TMP_ROOT="$PROJECT_ROOT/tests/tmp/integration"
mkdir -p "$TMP_ROOT"
TMP_DIR="$(mktemp -d "$TMP_ROOT/cli_migrate_long_path.XXXXXX")"

cleanup() {
    if [ -n "${TMP_DIR:-}" ] && [ -d "$TMP_DIR" ]; then
        # Recursive remove of test-only scratch dir under tests/tmp/.
        # Pre-approved per CLAUDE.md (worktree paths / scratch space).
        chmod -R u+w "$TMP_DIR" 2>/dev/null || true
        rm -rf "$TMP_DIR"
    fi
}
trap cleanup EXIT INT TERM

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
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

echo "================================================"
echo "  --migrate long-path truncation test (issue #712)"
echo "================================================"
echo ""

# Preconditions
if [ ! -x "$CLI" ]; then
    echo -e "${RED}ERROR${NC}: CLI binary not found at $CLI"
    echo "Run 'make -C frontier-cli' first."
    exit 2
fi

if [ ! -f "$V6_FIXTURE" ]; then
    echo -e "${RED}ERROR${NC}: v6 fixture not found at $V6_FIXTURE"
    exit 2
fi

# Construct a path that exceeds 255 bytes but stays under
# CLI_MAX_PATH_LENGTH (1024). Each segment is ~60 chars (well under the
# 255-byte single-component filesystem limit on APFS / ext4). Five
# segments plus the tmp prefix + filename pushes the total to ~345 bytes.
SEG=$(printf 'a%.0s' $(seq 1 60))
LONG_PARENT="$TMP_DIR/$SEG/$SEG/$SEG/$SEG/$SEG"
mkdir -p "$LONG_PARENT"
LONG_PATH="$LONG_PARENT/test.root"
cp "$V6_FIXTURE" "$LONG_PATH"

PATH_LEN=${#LONG_PATH}
echo "Long path constructed: ${PATH_LEN} bytes (must be > 255 and < 1024)"
echo ""

# Sanity: path must actually be > 255 bytes for this test to be valid.
if [ "$PATH_LEN" -le 255 ]; then
    echo -e "${RED}ERROR${NC}: Constructed path is only $PATH_LEN bytes; need > 255."
    exit 2
fi
if [ "$PATH_LEN" -ge 1024 ]; then
    echo -e "${RED}ERROR${NC}: Constructed path is $PATH_LEN bytes; would be rejected by CLI_MAX_PATH_LENGTH=1024."
    exit 2
fi

# What the truncated prefix looks like (the 255-byte cutoff of the full path).
# A future fix can choose to use this for stronger assertions, but for the
# fail-loud expectation we mainly care that no .v7.tmp is created anywhere.
TRUNCATED_PREFIX="${LONG_PATH:0:255}"
TRUNCATED_TMP="${TRUNCATED_PREFIX}.v7.tmp"

# Capture pre-state for the no-side-effects assertion.
PRE_V6_HASH=$(shasum -a 256 "$LONG_PATH" | cut -d' ' -f1)

# Run --migrate. Capture stderr separately so we can grep for a clear
# truncation message vs the generic "openfile" error we get today.
STDOUT_FILE="$TMP_DIR/stdout.txt"
STDERR_FILE="$TMP_DIR/stderr.txt"

set +e
"$CLI" --migrate "$LONG_PATH" >"$STDOUT_FILE" 2>"$STDERR_FILE"
ACTUAL_EXIT=$?
set -e

echo "--- stdout ---"
cat "$STDOUT_FILE"
echo "--- stderr ---"
cat "$STDERR_FILE"
echo "--- exit code: $ACTUAL_EXIT ---"
echo ""

# Assertion 1: non-zero exit. (Already true today; will stay true post-fix.)
if [ "$ACTUAL_EXIT" -ne 0 ]; then
    pass "exits non-zero on >255-byte --migrate path"
else
    fail "expected non-zero exit, got $ACTUAL_EXIT" \
         "Migration appears to have succeeded with a truncated path -- this is the worst-case silent-corruption scenario."
fi

# Assertion 2: stderr surfaces a CLEAR truncation cause.
# This is the headline assertion of issue #712. Pre-fix the user sees
# "openfile(src)" which is indistinguishable from permission denied,
# missing file, or any other downstream failure. Post-fix we expect the
# error to name the actual cause (path too long / truncated / 255).
if grep -qiE "path.*too.*long|too.*long.*path|exceeds.*255|truncat|> 255 bytes|exceeded 255" "$STDERR_FILE" "$STDOUT_FILE"; then
    pass "error message identifies path-length / truncation as the cause"
else
    fail "stderr does not mention truncation or path length" \
         "Expected one of: 'path too long' / 'truncated' / 'exceeds 255' / 'exceeded 255'. Got the generic downstream error instead -- which is the issue #712 bug."
fi

# Assertion 3: no stray .v7.tmp file was created at the truncated-prefix
# location (or anywhere else that isn't under TMP_DIR). A defense-in-depth
# fix should fail before opennewfile() is invoked with a truncated dst.
if [ -e "$TRUNCATED_TMP" ]; then
    fail "stray .v7.tmp file created at truncated prefix" \
         "Found: $TRUNCATED_TMP -- a silent truncation wrote a file at the wrong path."
else
    pass "no .v7.tmp file leaked at the truncated-prefix location"
fi

# Assertion 4: original v6 fixture is unchanged.
POST_V6_HASH=$(shasum -a 256 "$LONG_PATH" | cut -d' ' -f1)
if [ "$PRE_V6_HASH" = "$POST_V6_HASH" ]; then
    pass "original v6 file untouched by the failed migration"
else
    fail "original v6 file was modified despite migration failure" \
         "pre: $PRE_V6_HASH  post: $POST_V6_HASH"
fi

echo ""
echo "================================================"
echo "  Results: $PASS/$TOTAL passed, $FAIL failed"
echo "================================================"

if [ "$FAIL" -gt 0 ]; then
    exit 1
else
    exit 0
fi
