#!/bin/bash
# cli_system_root_temp_path_boundary_test.sh - issue #715
#
# Behavioral coverage for the deep-stack copyctopstring defense at
# callsite #3 in Common/source/db_format.c (line 2509) added by PR #714
# (#712).
#
# Callsite coverage matrix: see the header of
# cli_system_root_long_path_test.sh for the full audit. Summary:
#
#   #1  db_format_compact_to_path          -- not live-reachable
#   #2  migrate_internal source path       -- covered by sibling test
#   #3  migrate_internal temp .v7.tmp path -- THIS TEST
#   #4  dbverbs.c                          -- pre-clamped upstream
#   #5  dbverbs.c                          -- pre-clamped upstream
#
# Callsite #3 is interesting because it is the ONLY case where the input
# can pass callsite #2 (db_path <= 255 bytes) and STILL trip the deep-
# stack guard:
#
#   migrate_internal:
#       copyctopstring(db_path,   bspath)   <-- callsite #2 (<= 255 ok)
#       ...
#       snprintf(temp_path, ..., "%s.v7.tmp", output_path)
#       copyctopstring(temp_path, bsdst)    <-- callsite #3 (+7 bytes)
#
# So temp_path = db_path + ".v7.tmp" (7 bytes longer). To exercise
# callsite #3 specifically we need len(db_path) in [249, 255], which
# yields len(temp_path) in [256, 262] -- passes #2, fails #3.
#
# Production message text from db_format.c:2511 (verified pre-write):
#   "migrate_internal: temp path exceeds 255 bytes and would be
#    truncated: <temp_path>"
#
# What this test asserts:
#   1. --system-root with a [249..255]-byte v6 path exits non-zero.
#   2. stderr contains the migrate_internal temp-path truncation
#      message (callsite #3), NOT the source-path one (callsite #2).
#      The distinction matters: a regression that lowers the limit at
#      callsite #2 to <248 would silently mask callsite #3.
#   3. No .v7.tmp file leaked.
#   4. Original v6 file hash unchanged.
#
# Skippable on long-prefix checkouts: if the test scratch root is itself
# so long that we cannot construct a db_path in [249, 255], we skip
# (exit 0 with a SKIP message) rather than fail. This prevents the test
# from going red on a developer machine with a deeply nested worktree
# path.
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
TMP_DIR="$(mktemp -d "$TMP_ROOT/cli_system_root_temp_path_boundary.XXXXXX")"

cleanup() {
    if [ -n "${TMP_DIR:-}" ] && [ -d "$TMP_DIR" ]; then
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

skip() {
    echo -e "${YELLOW}SKIP${NC}: $1"
    echo ""
    echo "================================================"
    echo "  Results: SKIPPED (environmental, not a failure)"
    echo "================================================"
    exit 0
}

echo "================================================"
echo "  --system-root temp-path boundary test (issue #715, callsite #3)"
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

# Target: a db_path whose length lands in [249, 255] so that
# db_path + ".v7.tmp" overflows 255.
#
# We construct: TMP_DIR / <pad>/test.root
# where <pad> is a single directory whose name length we pick to land
# the total at TARGET_LEN.
#
# Total = len(TMP_DIR) + 1 + len(<pad>) + 1 + len("test.root")
#       = len(TMP_DIR) + len(<pad>) + 11
#
# We pick TARGET_LEN = 252 (middle of [249, 255] window, leaves a tiny
# buffer on either side for off-by-one issues). Then:
#   len(<pad>) = TARGET_LEN - len(TMP_DIR) - 11

TARGET_LEN=252
TMP_DIR_LEN=${#TMP_DIR}
PAD_LEN=$((TARGET_LEN - TMP_DIR_LEN - 11))

echo "TMP_DIR length: $TMP_DIR_LEN bytes"
echo "Target db_path length: $TARGET_LEN bytes"
echo "Required pad length: $PAD_LEN bytes"
echo ""

# Sanity bounds for the pad. The single-component limit on APFS/ext4 is
# 255 bytes, so PAD_LEN must be in (0, 255]. If PAD_LEN is too small or
# negative, the worktree path is already too long -- skip rather than
# fail. (Each filesystem path component must also be non-empty.)
#
# Minimum useful PAD_LEN: 1. Lower limit on db_path: we want >= 249 to
# make sure callsite #2 (which fires at >255) does NOT trip first.
#
# Upper limit: we want <= 255 for the same reason. At db_path == 256
# callsite #2 fires before #3 ever runs.
MIN_DB_PATH=249
MAX_DB_PATH=255
MIN_PAD=$((MIN_DB_PATH - TMP_DIR_LEN - 11))
MAX_PAD=$((MAX_DB_PATH - TMP_DIR_LEN - 11))

if [ "$MAX_PAD" -lt 1 ] || [ "$MIN_PAD" -gt 254 ]; then
    skip "tmp prefix too long for boundary construction (need db_path in [$MIN_DB_PATH..$MAX_DB_PATH]; TMP_DIR is $TMP_DIR_LEN bytes, leaving pad window [$MIN_PAD..$MAX_PAD] which is unsatisfiable)"
fi

if [ "$PAD_LEN" -lt 1 ]; then
    # Re-target near MAX_PAD if our nominal target produces a sub-1 pad.
    PAD_LEN=$MAX_PAD
    TARGET_LEN=$((TMP_DIR_LEN + PAD_LEN + 11))
    echo "Adjusted TARGET_LEN down to $TARGET_LEN (PAD_LEN=$PAD_LEN) to fit window"
fi

# Build the pad directory name.
PAD=$(printf 'p%.0s' $(seq 1 "$PAD_LEN"))

LONG_PARENT="$TMP_DIR/$PAD"
mkdir -p "$LONG_PARENT"
LONG_PATH="$LONG_PARENT/test.root"
cp "$V6_FIXTURE" "$LONG_PATH"

PATH_LEN=${#LONG_PATH}
TEMP_PATH_LEN=$((PATH_LEN + 7))  # ".v7.tmp" is 7 bytes
echo "Constructed db_path length: $PATH_LEN bytes"
echo "Implied temp_path length:   $TEMP_PATH_LEN bytes"
echo ""

# Hard preconditions: db_path must NOT exceed 255 (else callsite #2 wins)
# AND temp_path must exceed 255 (else neither callsite fires).
if [ "$PATH_LEN" -gt 255 ]; then
    echo -e "${RED}ERROR${NC}: constructed db_path is $PATH_LEN bytes (> 255). Callsite #2 would fire before #3; reduce PAD_LEN."
    exit 2
fi
if [ "$TEMP_PATH_LEN" -le 255 ]; then
    echo -e "${RED}ERROR${NC}: implied temp_path is $TEMP_PATH_LEN bytes (<= 255). Neither callsite would fire; increase PAD_LEN."
    exit 2
fi

# What the truncated prefix looks like (for the no-side-effects check).
TRUNCATED_PREFIX="${LONG_PATH:0:255}"
TRUNCATED_TMP_AT_PREFIX="${TRUNCATED_PREFIX}.v7.tmp"
EXPECTED_TMP="${LONG_PATH}.v7.tmp"

# Capture pre-state.
PRE_V6_HASH=$(shasum -a 256 "$LONG_PATH" | cut -d' ' -f1)

# Run --system-root. Auto-migration kicks in for v6 input.
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
    pass "exits non-zero on boundary-length --system-root path"
else
    fail "expected non-zero exit, got $ACTUAL_EXIT" \
         "System root load appears to have succeeded with a truncated temp path -- silent-corruption scenario."
fi

# Assertion 2: stderr contains the temp-path truncation message, NOT
# the source-path one. This is what makes the test specific to
# callsite #3 vs the sibling test that covers callsite #2.
if grep -qE "migrate_internal: temp path exceeds 255 bytes" "$STDERR_FILE"; then
    pass "stderr contains migrate_internal temp-path truncation message (callsite #3)"
else
    fail "stderr missing the callsite #3 truncation message" \
         "Expected: 'migrate_internal: temp path exceeds 255 bytes'. Saw the above stderr instead."
fi

# Cross-check: callsite #2 should NOT have fired (db_path is <= 255).
# If we see the source-path message, our boundary construction was off.
if grep -qE "migrate_internal: source path exceeds 255 bytes" "$STDERR_FILE"; then
    fail "unexpectedly tripped callsite #2 (source path)" \
         "Boundary construction is wrong: db_path is supposed to be <= 255 but callsite #2 fired. Investigate length calculation."
else
    pass "callsite #2 (source path) correctly did NOT fire"
fi

# Assertion 3: no .v7.tmp file leaked. Two places to check:
#  - the truncated 255-byte prefix
#  - the canonical db_path + ".v7.tmp" location
LEAK_FOUND=0
if [ -e "$TRUNCATED_TMP_AT_PREFIX" ]; then
    fail "stray .v7.tmp file at truncated prefix" \
         "Found: $TRUNCATED_TMP_AT_PREFIX"
    LEAK_FOUND=1
fi
if [ -e "$EXPECTED_TMP" ]; then
    fail "stray .v7.tmp file at canonical location" \
         "Found: $EXPECTED_TMP -- the defense should fail BEFORE opennewfile()."
    LEAK_FOUND=1
fi
if [ "$LEAK_FOUND" -eq 0 ]; then
    pass "no .v7.tmp file leaked"
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
