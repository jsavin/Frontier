#!/bin/bash
#
# Integration tests for --protocol read-only behavior (issue #588).
#
# Verifies:
#  1. Default: --protocol --system-root opens read-only; no-op script does
#     not modify the .root file.
#  2. --allow-mutate: opt-in to read-write mode; fileMenu.save() succeeds
#     and modifies the file on disk.
#  3. Default with mutation: fileMenu.save() returns success:false with
#     a "read-only" error message — the file is not modified.
#  4. --read-only and --allow-mutate together: rejected at argv parse.
#  5. --read-only outside protocol mode: still honored (defense in depth).
#
# Each test stages a fresh copy of Virgin.root in tests/tmp/ to avoid
# touching the canonical database under databases/.
#
# Note: this file is invoked from tools/run_integration_tests.sh. Tests are
# tallied separately from the YAML runner.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
SOURCE_ROOT="$PROJECT_ROOT/databases/Virgin.root"
TMP_DIR="$PROJECT_ROOT/tests/tmp/integration/protocol_readonly"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS=0
FAIL=0
TOTAL=0

if [ ! -x "$CLI" ]; then
	echo -e "${RED}Error: frontier-cli not found at $CLI${NC}"
	exit 1
fi
if [ ! -f "$SOURCE_ROOT" ]; then
	echo -e "${RED}Error: source database not found at $SOURCE_ROOT${NC}"
	exit 1
fi

mkdir -p "$TMP_DIR"

_md5() {
	# Cross-platform md5 of a file
	if command -v md5 >/dev/null 2>&1; then
		md5 -q "$1"
	else
		md5sum "$1" | cut -d' ' -f1
	fi
}

# Stage a fresh copy of the source DB and return its absolute path
_stage_db() {
	local label="$1"
	local dest="$TMP_DIR/$label.root"
	cp "$SOURCE_ROOT" "$dest"
	echo "$dest"
}

_record_test() {
	local name="$1"
	local result="$2"  # "pass" or "fail"
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

echo "=============================================="
echo "Protocol Read-Only Tests (issue #588)"
echo "=============================================="
echo

# -----------------------------------------------------------------------
# Test 1: --protocol --system-root (no flags) — file unchanged after no-op.
# This is the core regression: previously the CLI re-saved the file even
# when no mutation was requested.
# -----------------------------------------------------------------------
{
	db=$(_stage_db "test1_default_readonly")
	before=$(_md5 "$db")
	out=$(echo '{"id":1,"op":"script/eval","params":{"expression":"1+1"}}' \
		| "$CLI" --protocol --skip-startup --system-root "$db" 2>&1)
	after=$(_md5 "$db")
	if [ "$before" = "$after" ]; then
		_record_test "default --protocol leaves DB unchanged after no-op" pass
	else
		_record_test "default --protocol leaves DB unchanged after no-op" fail \
			"md5 changed: $before -> $after"
	fi
}

# -----------------------------------------------------------------------
# Test 2: --allow-mutate + protocol with fileMenu.save() — file changes.
# This confirms the install/build path (clean-root, ODB editing) still
# works when explicitly opted in.
# -----------------------------------------------------------------------
{
	db=$(_stage_db "test2_allow_mutate")
	before=$(_md5 "$db")
	# Touch a value, then save. The save should succeed and persist.
	out=$(printf '%s\n%s\n' \
		'{"id":1,"op":"script/eval","params":{"expression":"new(stringType,@workspace.testReadOnly588); workspace.testReadOnly588=\"changed\"; return true"}}' \
		'{"id":2,"op":"script/eval","params":{"expression":"fileMenu.save()"}}' \
		| "$CLI" --protocol --skip-startup --allow-mutate --system-root "$db" 2>&1)
	after=$(_md5 "$db")
	if [ "$before" != "$after" ] && echo "$out" | grep -q '"id":2.*"success":true'; then
		_record_test "--allow-mutate permits fileMenu.save() to persist" pass
	else
		_record_test "--allow-mutate permits fileMenu.save() to persist" fail \
			"md5 unchanged or save failed. before=$before after=$after out=$out"
	fi
}

# -----------------------------------------------------------------------
# Test 3: default --protocol with fileMenu.save() — error returned and
# file unchanged. Failure is loud (success:false), not silent.
# -----------------------------------------------------------------------
{
	db=$(_stage_db "test3_default_blocks_save")
	before=$(_md5 "$db")
	out=$(echo '{"id":1,"op":"script/eval","params":{"expression":"fileMenu.save()"}}' \
		| "$CLI" --protocol --skip-startup --system-root "$db" 2>&1)
	after=$(_md5 "$db")
	# Save must report failure AND file must not have changed.
	if [ "$before" = "$after" ] && echo "$out" | grep -q '"id":1' \
		&& echo "$out" | grep -qi "read-only"; then
		_record_test "default --protocol refuses fileMenu.save() with read-only error" pass
	else
		_record_test "default --protocol refuses fileMenu.save() with read-only error" fail \
			"md5 changed=$([ \"$before\" != \"$after\" ] && echo yes || echo no), out=$out"
	fi
}

# -----------------------------------------------------------------------
# Test 4: --read-only and --allow-mutate together are rejected.
# -----------------------------------------------------------------------
{
	db=$(_stage_db "test4_conflict")
	# Use --batch to force exit on argv parse error rather than dropping
	# into a REPL (some CLIs don't bail until an op arrives). The CLI
	# should print an error and exit non-zero.
	out=$("$CLI" --read-only --allow-mutate --skip-startup --system-root "$db" -e "1" 2>&1)
	rc=$?
	if [ "$rc" -ne 0 ] && echo "$out" | grep -qiE "read-only.*allow-mutate|allow-mutate.*read-only|cannot.*combined"; then
		_record_test "--read-only + --allow-mutate is rejected" pass
	else
		_record_test "--read-only + --allow-mutate is rejected" fail \
			"rc=$rc out=$out"
	fi
}

# -----------------------------------------------------------------------
# Test 5: --read-only honored outside --protocol (defense in depth).
# Even with -e mode and an explicit fileMenu.save(), the file should not
# change when --read-only is set.
# -----------------------------------------------------------------------
{
	db=$(_stage_db "test5_read_only_outside_protocol")
	before=$(_md5 "$db")
	out=$("$CLI" --read-only --skip-startup --system-root "$db" \
		-e "fileMenu.save()" 2>&1)
	after=$(_md5 "$db")
	if [ "$before" = "$after" ]; then
		_record_test "--read-only honored in -e mode (file unchanged)" pass
	else
		_record_test "--read-only honored in -e mode (file unchanged)" fail \
			"md5 changed: $before -> $after; out=$out"
	fi
}

echo
echo "=============================================="
echo "Protocol Read-Only Tests: $PASS/$TOTAL passed"
if [ $FAIL -eq 0 ]; then
	echo -e "${GREEN}All tests passed${NC}"
	rm -rf "$TMP_DIR"
	exit 0
else
	echo -e "${RED}$FAIL tests failed${NC}"
	echo "Test artifacts left in: $TMP_DIR"
	exit 1
fi
