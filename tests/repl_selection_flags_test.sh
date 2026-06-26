#!/bin/bash
#
# C.6 REPL selection flag tests.
#
# Verifies the post-C.6 (#691) flag semantics:
#   - `--debug-tui --plain`          -> rejected (mutually exclusive)
#   - `--debug-tui --protocol`       -> rejected (pre-existing)
#   - `--plain --protocol`           -> rejected (new)
#   - `--debug-tui` alone (with -e)  -> works + emits deprecation warning
#   - `--plain` alone (with -e)      -> works (routes to legacy execute path
#                                      for batch; no warning)
#   - default (with -e)              -> works (no warning)
#   - `--help` lists all three REPL flags
#
# These tests use -e mode so they exit cleanly without entering an
# interactive REPL.  The REPL dispatch itself is exercised end-to-end by
# the tui-tests harness (which now invokes frontier-cli with no flag --
# i.e. the new default boxen REPL).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"

# Every accepting invocation below passes --lock-opened-roots in addition
# to --skip-startup.  These tests open Frontier.root read-only as far as
# the flag-parsing logic is concerned, but the runtime would still save
# on exit by default, drifting the dist-staged DB.  --lock-opened-roots
# suppresses save-on-exit -- correct for the test intent.
DB="$PROJECT_ROOT/databases/Frontier.root"

if [ ! -x "$CLI" ]; then
    echo "Error: frontier-cli not found at $CLI" >&2
    exit 1
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

PASSED=0
FAILED=0

# Run CLI; on rejection, capture exit code + stderr.  Used by tests that
# expect the parser to reject the combination.
expect_rejection() {
    local name="$1"
    local expected_substring="$2"
    shift 2
    local stderr_out
    local exit_code=0
    stderr_out=$("$CLI" "$@" 2>&1 >/dev/null) || exit_code=$?

    if [ "$exit_code" -eq 0 ]; then
        echo -e "  ${RED}✗ FAIL${NC}: $name (expected non-zero exit, got 0)"
        FAILED=$((FAILED + 1))
        return
    fi
    if echo "$stderr_out" | grep -qF -- "$expected_substring"; then
        echo -e "  ${GREEN}✓ PASS${NC}: $name"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}✗ FAIL${NC}: $name"
        echo "    Expected substring: '$expected_substring'"
        echo "    Got stderr:"
        echo "$stderr_out" | sed 's/^/      /'
        FAILED=$((FAILED + 1))
    fi
}

# Run CLI; expect clean exit + stdout containing the expected substring.
expect_accept() {
    local name="$1"
    local expected_stdout_substring="$2"
    shift 2
    local stdout_out
    local exit_code=0
    stdout_out=$("$CLI" "$@" 2>/dev/null) || exit_code=$?

    if [ "$exit_code" -ne 0 ]; then
        echo -e "  ${RED}✗ FAIL${NC}: $name (exit code $exit_code)"
        FAILED=$((FAILED + 1))
        return
    fi
    if echo "$stdout_out" | grep -qF -- "$expected_stdout_substring"; then
        echo -e "  ${GREEN}✓ PASS${NC}: $name"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}✗ FAIL${NC}: $name"
        echo "    Expected stdout substring: '$expected_stdout_substring'"
        echo "    Got stdout:"
        echo "$stdout_out" | sed 's/^/      /'
        FAILED=$((FAILED + 1))
    fi
}

# Run CLI; check that the deprecation warning IS or IS NOT present in
# stderr AND that the CLI exited as the third arg expects (0 for clean
# exit, "any" for "we don't care about the exit code, just the warning").
#
# The "any" mode is needed for --debug-tui invocations: that flag always
# routes through boxen_repl_main which fails to init on a non-TTY (test
# environment).  The deprecation warning fires BEFORE the init attempt,
# so the warning-presence check is still meaningful even with a non-zero
# exit.
expect_warning_presence() {
    local name="$1"
    local should_warn="$2"      # "yes" or "no"
    local expect_exit="$3"      # "0" or "any"
    shift 3
    local stderr_out
    local exit_code=0
    stderr_out=$("$CLI" "$@" 2>&1 >/dev/null) || exit_code=$?

    if [ "$expect_exit" = "0" ] && [ "$exit_code" -ne 0 ]; then
        echo -e "  ${RED}✗ FAIL${NC}: $name (CLI exited $exit_code; warning-check skipped)"
        echo "    Stderr:"
        echo "$stderr_out" | sed 's/^/      /'
        FAILED=$((FAILED + 1))
        return
    fi

    local has_warning=no
    if echo "$stderr_out" | grep -qF "debug-tui is now the default"; then
        has_warning=yes
    fi

    if [ "$has_warning" = "$should_warn" ]; then
        echo -e "  ${GREEN}✓ PASS${NC}: $name (warning=$has_warning)"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}✗ FAIL${NC}: $name (expected warning=$should_warn, got $has_warning)"
        echo "    Stderr:"
        echo "$stderr_out" | sed 's/^/      /'
        FAILED=$((FAILED + 1))
    fi
}

echo "=============================================="
echo "C.6 REPL Selection Flag Tests"
echo "=============================================="
echo

echo "--- Mutual exclusion ---"
expect_rejection "--debug-tui + --plain rejected" \
    "--plain and --debug-tui are mutually exclusive" \
    --debug-tui --plain --lock-opened-roots --system-root "$DB" -e "1"

expect_rejection "--debug-tui + --protocol rejected" \
    "--debug-tui and --protocol are mutually exclusive" \
    --debug-tui --protocol --system-root "$DB"

expect_rejection "--plain + --protocol rejected" \
    "--plain and --protocol are mutually exclusive" \
    --plain --protocol --system-root "$DB"

echo
echo "--- Flag acceptance via -e (batch) ---"
# In batch mode (-e), the REPL flag selects which interactive path
# WOULD be taken if there were no script.  With -e, execute_script_mode
# runs regardless; the flag still parses successfully.
expect_accept "default + -e works" "42" \
    --skip-startup --lock-opened-roots --system-root "$DB" -e "42"

expect_accept "--plain + -e works (--plain doesn't force REPL launch)" "42" \
    --skip-startup --plain --system-root "$DB" -e "42"

# --debug-tui DOES force a REPL launch (pre-C.6 behavior preserved as a
# no-op alias), so combining with -e tries to boot boxen which fails
# without a TTY -- expected, not a regression.  No test row for that
# combination; the deprecation-warning test below covers the flag's
# behavior in a TTY-friendly way.

echo
echo "--- Deprecation warning ---"
expect_warning_presence "default (no flag) emits NO deprecation warning" "no" "0" \
    --skip-startup --lock-opened-roots --system-root "$DB" -e "1"

expect_warning_presence "--plain emits NO deprecation warning" "no" "0" \
    --skip-startup --lock-opened-roots --plain --system-root "$DB" -e "1"

# --debug-tui always routes to boxen_repl_main, which fails to init on
# a non-TTY (test environment).  The deprecation warning fires BEFORE
# the init attempt, so the warning-presence check is still meaningful
# even with a non-zero exit.  Pass "any" for expect_exit.
expect_warning_presence "--debug-tui emits deprecation warning" "yes" "any" \
    --skip-startup --lock-opened-roots --debug-tui --system-root "$DB" -e "1"

echo
echo "--- --help text ---"
expect_accept "--help lists --plain" "--plain" --help
expect_accept "--help lists --debug-tui as DEPRECATED" "DEPRECATED" --help
expect_accept "--help lists --protocol" "--protocol" --help
expect_accept "--help has REPL Selection section" "REPL Selection:" --help

echo
echo "=============================================="
echo "Results: $PASSED passed, $FAILED failed"
echo "=============================================="

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi
exit 0
