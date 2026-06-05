#!/bin/bash
# cli_system_root_long_path_test.sh - issue #715
#
# Behavioral coverage for the deep-stack copyctopstring defenses added by
# PR #714 (#712) in Common/source/db_format.c. PR #714 added five
# defense-in-depth checks where copyctopstring's return value was
# previously discarded. The pre-existing integration test
# cli_migrate_long_path_test.sh only exercises the CLI fast-fail at
# frontier-cli/main.c:660 (MIGRATE_MAX_PATH_BYTES). The deep-stack
# callsites needed live-caller coverage; this test provides it for
# callsite #2.
#
# Callsite coverage matrix (issue #715 audit):
#
#   #1  db_format.c:~2151  db_format_compact_to_path  -- UNTESTED
#       Reachable only via db.compactDatabase, which has its own length
#       guard at dbverbs.c:~1232 that fires BEFORE the deep-stack check.
#       The deep-stack defense is pure defense-in-depth; no live caller
#       can exercise it.
#
#   #2  db_format.c:~2388  migrate_internal (src path) -- THIS TEST
#       Reachable via `--system-root <long-v6-path>`. The CLI does NOT
#       fast-fail on --system-root the way it does on --migrate, so a
#       >255-byte path flows through hydrate_system_root_database ->
#       ensure_database_v7 -> migrate_internal and hits the source-path
#       copyctopstring check.
#
#   #3  db_format.c:~2509  migrate_internal (temp .v7.tmp path) -- sibling test
#       See cli_system_root_temp_path_boundary_test.sh. The temp_path
#       is db_path + ".v7.tmp", so it overflows when db_path is in
#       [248, 255]. That boundary needs its own test.
#
#   #4  dbverbs.c               -- UNTESTED
#   #5  dbverbs.c               -- UNTESTED
#       Both have their input pre-clamped upstream by filespectopath
#       (a Pascal-string converter that itself caps at 255). No live
#       caller path can deliver a >255-byte string to these sites; they
#       are pure defense-in-depth. Documented for completeness.
#
# What this test asserts (passes today on PR #714 / develop):
#   1. --system-root with a >255-byte path exits non-zero.
#   2. stderr contains the migrate_internal source-path truncation message
#      from db_format.c:2390:
#        "migrate_internal: source path exceeds 255 bytes and would be
#         truncated: %s"
#   3. No stray .v7.tmp file at the truncated-prefix location.
#   4. Original v6 source file hash unchanged.
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
TMP_DIR="$(mktemp -d "$TMP_ROOT/cli_system_root_long_path.XXXXXX")"

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
echo "  --system-root long-path truncation test (issue #715, callsite #2)"
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

if [ "$PATH_LEN" -le 255 ]; then
    echo -e "${RED}ERROR${NC}: Constructed path is only $PATH_LEN bytes; need > 255."
    exit 2
fi
if [ "$PATH_LEN" -ge 1024 ]; then
    echo -e "${RED}ERROR${NC}: Constructed path is $PATH_LEN bytes; would be rejected by CLI_MAX_PATH_LENGTH=1024."
    exit 2
fi

# What the truncated prefix looks like (the 255-byte cutoff of the full path).
TRUNCATED_PREFIX="${LONG_PATH:0:255}"
TRUNCATED_TMP="${TRUNCATED_PREFIX}.v7.tmp"

# Capture pre-state for the no-side-effects assertion.
PRE_V6_HASH=$(shasum -a 256 "$LONG_PATH" | cut -d' ' -f1)

# Run --system-root with the long v6 path. The CLI auto-migrates v6
# system roots via ensure_database_v7 -> migrate_internal, so the
# >255-byte db_path will reach callsite #2 in db_format.c (line 2388).
#
# Use --execute with a trivial script so the CLI has a reason to load
# the system root. Add --skip-startup to keep the failure cause focused
# on the migration step rather than startup-script noise.
STDOUT_FILE="$TMP_DIR/stdout.txt"
STDERR_FILE="$TMP_DIR/stderr.txt"

set +e
"$CLI" --system-root "$LONG_PATH" --skip-startup --execute 'echo("ok")' \
    >"$STDOUT_FILE" 2>"$STDERR_FILE"
ACTUAL_EXIT=$?
set -e

echo "--- stdout ---"
cat "$STDOUT_FILE"
echo "--- stderr ---"
cat "$STDERR_FILE"
echo "--- exit code: $ACTUAL_EXIT ---"
echo ""

# Assertion 1: non-zero exit.
if [ "$ACTUAL_EXIT" -ne 0 ]; then
    pass "exits non-zero on >255-byte --system-root path"
else
    fail "expected non-zero exit, got $ACTUAL_EXIT" \
         "System root load appears to have succeeded with a truncated path -- silent-corruption scenario."
fi

# Assertion 2: stderr surfaces the migrate_internal source-path
# truncation message. The exact message text from PR #714 at
# db_format.c:2390 is:
#   "migrate_internal: source path exceeds 255 bytes and would be truncated: <path>"
# We grep for the distinguishing prefix that is uniquely owned by
# callsite #2 (not callsite #1 or #3): "migrate_internal: source path".
if grep -qE "migrate_internal: source path exceeds 255 bytes" "$STDERR_FILE"; then
    pass "stderr contains migrate_internal source-path truncation message (callsite #2)"
else
    fail "stderr missing the callsite #2 truncation message" \
         "Expected: 'migrate_internal: source path exceeds 255 bytes'. Saw the above stderr instead -- check that log_error from db_format.c:2390 is reaching stderr (not just a log file)."
fi

# Assertion 3: no stray .v7.tmp file was created at the truncated-prefix
# location. The defense-in-depth fix should fail before opennewfile() is
# invoked with a truncated dst, so nothing should be written.
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
