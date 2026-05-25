#!/bin/bash
#
# Integration tests for tools/edit_virgin_root.sh (issue #644).
#
# The wrapper is hard to fully cover because its main flow includes an
# interactive prompt and spawning frontier-cli. We test the non-interactive
# paths:
#
#  1. --help exits 0 and prints usage.
#  2. --dry-run from project root stages a copy, does NOT spawn frontier-cli,
#     leaves databases/Virgin.root untouched, and cleans up the staged temp.
#  3. Running from the wrong directory (no databases/Virgin.root) errors out
#     clearly and exits non-zero.
#  4. If the frontier-cli binary is missing the wrapper errors out clearly
#     and exits non-zero.
#
# This file is invoked from tools/run_integration_tests.sh alongside the
# other bash-based integration test files. Its pass/fail tally is reported
# separately from the YAML runner.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
WRAPPER="$PROJECT_ROOT/tools/edit_virgin_root.sh"
SOURCE_ROOT="$PROJECT_ROOT/databases/Virgin.root"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

PASS=0
FAIL=0
TOTAL=0

_md5() {
	if command -v md5 >/dev/null 2>&1; then
		md5 -q "$1"
	else
		md5sum "$1" | cut -d' ' -f1
	fi
}

_record() {
	local name="$1"
	local result="$2"
	local detail="${3:-}"
	TOTAL=$((TOTAL + 1))
	if [ "$result" = "pass" ]; then
		echo -e "  ${GREEN}PASS${NC}: $name"
		PASS=$((PASS + 1))
	else
		echo -e "  ${RED}FAIL${NC}: $name"
		if [ -n "$detail" ]; then
			echo "    $detail"
		fi
		FAIL=$((FAIL + 1))
	fi
}

if [ ! -x "$WRAPPER" ]; then
	echo -e "${RED}Error: wrapper not found or not executable at $WRAPPER${NC}"
	exit 1
fi
if [ ! -f "$SOURCE_ROOT" ]; then
	echo -e "${RED}Error: source database not found at $SOURCE_ROOT${NC}"
	exit 1
fi

echo "=============================================="
echo "edit_virgin_root.sh wrapper tests (issue #644)"
echo "=============================================="
echo

# ---------------------------------------------------------------------------
# Test 1: --help exits 0 and mentions key flags
# ---------------------------------------------------------------------------
{
	out=$("$WRAPPER" --help 2>&1)
	rc=$?
	if [ "$rc" -eq 0 ] \
		&& echo "$out" | grep -q -- "--dry-run" \
		&& echo "$out" | grep -q -- "--read-only" \
		&& echo "$out" | grep -q -i "usage"; then
		_record "--help exits 0 with usage" pass
	else
		_record "--help exits 0 with usage" fail "rc=$rc out=$out"
	fi
}

# ---------------------------------------------------------------------------
# Test 2: --dry-run from project root stages, does not spawn CLI, leaves
# canonical Virgin.root untouched, and cleans up.
# ---------------------------------------------------------------------------
{
	before=$(_md5 "$SOURCE_ROOT")
	# Snapshot existing frontier-edit dirs so we can detect leftovers.
	pre_existing=$(ls -d /tmp/frontier-edit-* 2>/dev/null || true)
	out=$(cd "$PROJECT_ROOT" && "$WRAPPER" --dry-run 2>&1)
	rc=$?
	after=$(_md5 "$SOURCE_ROOT")
	post_existing=$(ls -d /tmp/frontier-edit-* 2>/dev/null || true)
	if [ "$rc" -eq 0 ] \
		&& [ "$before" = "$after" ] \
		&& echo "$out" | grep -q -i "dry.run" \
		&& [ "$pre_existing" = "$post_existing" ]; then
		_record "--dry-run leaves Virgin.root untouched and cleans up temp" pass
	else
		_record "--dry-run leaves Virgin.root untouched and cleans up temp" fail \
			"rc=$rc md5_before=$before md5_after=$after pre=$pre_existing post=$post_existing out=$out"
	fi
}

# ---------------------------------------------------------------------------
# Test 3: Running from wrong cwd (no databases/Virgin.root visible) errors.
# ---------------------------------------------------------------------------
{
	tmpdir=$(mktemp -d)
	out=$(cd "$tmpdir" && "$WRAPPER" --dry-run 2>&1)
	rc=$?
	rm -rf "$tmpdir"
	if [ "$rc" -ne 0 ] && echo "$out" | grep -qi "virgin.root"; then
		_record "rejects when run from wrong cwd" pass
	else
		_record "rejects when run from wrong cwd" fail "rc=$rc out=$out"
	fi
}

# ---------------------------------------------------------------------------
# Test 4: Missing frontier-cli binary errors out clearly.
# Simulate by temporarily renaming the binary. We restore it whatever happens.
# Skipped if binary isn't built — the missing-binary error path is the same
# logical check.
# ---------------------------------------------------------------------------
{
	cli="$PROJECT_ROOT/frontier-cli/frontier-cli"
	if [ -f "$cli" ]; then
		mv "$cli" "$cli.test-bak"
		# Restore on exit even if test errors
		trap 'mv "$cli.test-bak" "$cli" 2>/dev/null || true' EXIT
		out=$(cd "$PROJECT_ROOT" && "$WRAPPER" --dry-run 2>&1)
		rc=$?
		mv "$cli.test-bak" "$cli"
		trap - EXIT
		if [ "$rc" -ne 0 ] && echo "$out" | grep -qi "frontier-cli"; then
			_record "rejects when frontier-cli is missing" pass
		else
			_record "rejects when frontier-cli is missing" fail "rc=$rc out=$out"
		fi
	else
		# Binary not built — exercise the same code path by running from the
		# project root with no binary in place.
		out=$(cd "$PROJECT_ROOT" && "$WRAPPER" --dry-run 2>&1)
		rc=$?
		if [ "$rc" -ne 0 ] && echo "$out" | grep -qi "frontier-cli"; then
			_record "rejects when frontier-cli is missing" pass
		else
			_record "rejects when frontier-cli is missing" fail "rc=$rc out=$out"
		fi
	fi
}

# ---------------------------------------------------------------------------
# Test 5: bash -n syntax check on the wrapper (catches typos that pass
# happy-path runs).
# ---------------------------------------------------------------------------
{
	if bash -n "$WRAPPER" 2>/tmp/edit_virgin_root_syntax.err; then
		_record "wrapper passes bash -n syntax check" pass
	else
		_record "wrapper passes bash -n syntax check" fail "$(cat /tmp/edit_virgin_root_syntax.err)"
	fi
	rm -f /tmp/edit_virgin_root_syntax.err
}

echo
echo "=============================================="
echo "edit_virgin_root.sh wrapper tests: $PASS/$TOTAL passed"
if [ $FAIL -eq 0 ]; then
	echo -e "${GREEN}All tests passed${NC}"
	exit 0
else
	echo -e "${RED}$FAIL tests failed${NC}"
	exit 1
fi
