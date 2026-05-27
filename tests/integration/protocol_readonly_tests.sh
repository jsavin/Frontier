#!/bin/bash
#
# Integration tests for --protocol read/write behavior under issue #127.
#
# Issue #127 restores the legacy default: --protocol --system-root opens
# read-write. The previous #588 default (RO unless --allow-mutate) was
# removed because it inverted legacy Frontier semantics and required test
# infrastructure to thread an opt-in through every spawn site.
#
# Verifies:
#  1. Default --protocol with no-op script + --lock-opened-roots: file
#     unchanged.
#  2. Default --protocol with fileMenu.save(): file changes (default-RW
#     persists the save).
#  3. --lock-opened-roots + fileMenu.save(): save reports failure and the
#     file is not modified (lock blocks the save path).
#  4. --lock-opened-roots outside protocol mode (-e): still honored.
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

# Clear inherited lock env so the test exercises the CLI flag directly.
unset FRONTIER_LOCK_OPENED_ROOTS

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
echo "Protocol Read/Write Tests (issue #127)"
echo "=============================================="
echo

# -----------------------------------------------------------------------
# Test 1: --protocol --lock-opened-roots no-op — file unchanged.
# This is the "ephemeral inspection" path tests run on by default.
# -----------------------------------------------------------------------
{
	db=$(_stage_db "test1_lock_noop")
	before=$(_md5 "$db")
	out=$(echo '{"id":1,"op":"script/eval","params":{"expression":"1+1"}}' \
		| "$CLI" --protocol --skip-startup --lock-opened-roots --system-root "$db" 2>&1)
	after=$(_md5 "$db")
	if [ "$before" = "$after" ]; then
		_record_test "--protocol --lock-opened-roots leaves DB unchanged after no-op" pass
	else
		_record_test "--protocol --lock-opened-roots leaves DB unchanged after no-op" fail \
			"md5 changed: $before -> $after"
	fi
}

# -----------------------------------------------------------------------
# Test 2: default --protocol with fileMenu.save() — file changes.
# Confirms the install/build path (clean-root, ODB editing) works without
# any opt-in flag under the issue #127 default.
# -----------------------------------------------------------------------
{
	db=$(_stage_db "test2_default_save")
	before=$(_md5 "$db")
	# Touch a value, then save. The save should succeed and persist.
	out=$(printf '%s\n%s\n' \
		'{"id":1,"op":"script/eval","params":{"expression":"new(stringType,@workspace.testPersist127); workspace.testPersist127=\"changed\"; return true"}}' \
		'{"id":2,"op":"script/eval","params":{"expression":"fileMenu.save()"}}' \
		| "$CLI" --protocol --skip-startup --system-root "$db" 2>&1)
	after=$(_md5 "$db")
	if [ "$before" != "$after" ] && echo "$out" | grep -q '"id":2.*"success":true'; then
		_record_test "default --protocol permits fileMenu.save() to persist" pass
	else
		_record_test "default --protocol permits fileMenu.save() to persist" fail \
			"md5 unchanged or save failed. before=$before after=$after out=$out"
	fi
}

# -----------------------------------------------------------------------
# Test 3: --lock-opened-roots + fileMenu.save() — save reports failure and
# file is unchanged. Lock turns on the lower-layer flreadonly bit, so
# fileMenu.save() returns success:false with a read-only error.
# -----------------------------------------------------------------------
{
	db=$(_stage_db "test3_lock_blocks_save")
	before=$(_md5 "$db")
	out=$(echo '{"id":1,"op":"script/eval","params":{"expression":"fileMenu.save()"}}' \
		| "$CLI" --protocol --skip-startup --lock-opened-roots --system-root "$db" 2>&1)
	after=$(_md5 "$db")
	# Save must report failure AND file must not have changed.
	if [ "$before" = "$after" ] && echo "$out" | grep -q '"id":1' \
		&& echo "$out" | grep -qi "read-only"; then
		_record_test "--lock-opened-roots refuses fileMenu.save() with read-only error" pass
	else
		_record_test "--lock-opened-roots refuses fileMenu.save() with read-only error" fail \
			"md5 changed=$([ \"$before\" != \"$after\" ] && echo yes || echo no), out=$out"
	fi
}

# -----------------------------------------------------------------------
# Test 4: --lock-opened-roots honored outside --protocol (-e mode).
# Even with -e mode and an explicit fileMenu.save(), the file should not
# change when --lock-opened-roots is set.
# -----------------------------------------------------------------------
{
	db=$(_stage_db "test4_lock_opened_roots_outside_protocol")
	before=$(_md5 "$db")
	out=$("$CLI" --lock-opened-roots --skip-startup --system-root "$db" \
		-e "fileMenu.save()" 2>&1)
	after=$(_md5 "$db")
	if [ "$before" = "$after" ]; then
		_record_test "--lock-opened-roots honored in -e mode (file unchanged)" pass
	else
		_record_test "--lock-opened-roots honored in -e mode (file unchanged)" fail \
			"md5 changed: $before -> $after; out=$out"
	fi
}

echo
echo "=============================================="
echo "Protocol Read/Write Tests: $PASS/$TOTAL passed"
if [ $FAIL -eq 0 ]; then
	echo -e "${GREEN}All tests passed${NC}"
	rm -rf "$TMP_DIR"
	exit 0
else
	echo -e "${RED}$FAIL tests failed${NC}"
	echo "Test artifacts left in: $TMP_DIR"
	exit 1
fi
