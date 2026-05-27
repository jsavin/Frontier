#!/bin/bash
#
# Regression test for issue #127: --lock-opened-roots / FRONTIER_LOCK_OPENED_ROOTS
# keeps loaded-from-disk system root reads idempotent on disk.
#
# Legacy Frontier opens every system root and guest database read-write by
# default; in-process mutations persist back to disk on shutdown. Tests and
# other ephemeral consumers opt OUT of save-on-exit via:
#   --lock-opened-roots          (CLI flag)
#   FRONTIER_LOCK_OPENED_ROOTS=1 (environment variable, picked up by the runner)
#
# In-memory mutations still evaluate (so `workspace.x = 1` returns 1) -- only
# the on-disk save is suppressed. Newly created roots (file.save / file.saveAs
# / db.compactDatabase) are unaffected, since they're created, not loaded.
#
# The cases below cover the matrix of read-only / read-write defaults and
# how --lock-opened-roots and --protocol interact.
#
# Tests use staged copies under /tmp -- they never touch the canonical
# databases/Virgin.root.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
SOURCE_DB="$PROJECT_ROOT/databases/Virgin.root"

# Clear FRONTIER_LOCK_OPENED_ROOTS from our environment so the test exercises
# the CLI flag behavior directly. The integration runner sets the var; this
# stand-alone shell test must not inherit it or every "without --lock-opened-roots"
# case below would silently mask the actual default-RW contract.
unset FRONTIER_LOCK_OPENED_ROOTS

if [ ! -x "$CLI" ]; then
    echo "Error: frontier-cli not found at $CLI" >&2
    exit 1
fi
if [ ! -f "$SOURCE_DB" ]; then
    echo "Error: source database not found at $SOURCE_DB" >&2
    exit 1
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

PASSED=0
FAILED=0

# Portable md5: use md5 on macOS, md5sum on Linux.
md5_of() {
    if command -v md5 >/dev/null 2>&1; then
        md5 -q "$1"
    else
        md5sum "$1" | awk '{print $1}'
    fi
}

# Snapshot the canonical Virgin.root md5 BEFORE any frontier-cli runs.
# The final assertion compares against this rather than a hard-coded hash
# so the test doesn't break for unrelated reasons every time the canonical
# DB legitimately changes (ODB script edits, verb additions, etc.).
CANONICAL_MD5_BEFORE="$(md5_of "$SOURCE_DB")"

STAGE_DIR="$(mktemp -d -t frontier-cow-test-XXXXXX)"
trap 'rm -rf "$STAGE_DIR"' EXIT

stage_copy() {
    local name="$1"
    local dest="$STAGE_DIR/$name"
    cp "$SOURCE_DB" "$dest"
    echo "$dest"
}

assert_md5_unchanged() {
    local label="$1"
    local before="$2"
    local after="$3"

    if [ "$before" = "$after" ]; then
        echo -e "  ${GREEN}PASS${NC}: $label (md5 stable: $before)"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}FAIL${NC}: $label"
        echo "    before: $before"
        echo "    after:  $after"
        FAILED=$((FAILED + 1))
    fi
}

assert_md5_changed() {
    local label="$1"
    local before="$2"
    local after="$3"

    if [ "$before" != "$after" ]; then
        echo -e "  ${GREEN}PASS${NC}: $label (md5 changed as expected)"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}FAIL${NC}: $label -- md5 should have changed but did not"
        echo "    md5: $before"
        FAILED=$((FAILED + 1))
    fi
}

# Test 1: --lock-opened-roots -e 'sizeOf(system)' -- pure read, no mutate, no save.
echo "==> Test 1: --lock-opened-roots -e 'sizeOf(system)' must not modify Virgin.root"
DB1="$(stage_copy "virgin_eval.root")"
MD5_BEFORE_1="$(md5_of "$DB1")"
"$CLI" --lock-opened-roots --system-root "$DB1" -e 'sizeOf(system)' >/dev/null 2>&1 || true
MD5_AFTER_1="$(md5_of "$DB1")"
assert_md5_unchanged "--lock-opened-roots -e 'sizeOf(system)' is idempotent" "$MD5_BEFORE_1" "$MD5_AFTER_1"

# Test 2a: --protocol --system-root (no flags) -- default-RW per #127.
# A no-op script may still bump the on-disk image because the save runs on
# exit; mutate explicitly and verify the change persists.
echo "==> Test 2a: --protocol --system-root (no flags) is default-RW (#127)"
DB2A="$(stage_copy "virgin_protocol_default.root")"
MD5_BEFORE_2A="$(md5_of "$DB2A")"
PROTOCOL_INPUT_MUTATE=$(cat <<'PROTOEOF'
{"id":1,"op":"script/eval","params":{"expression":"workspace.protoflag = 11"}}
{"id":2,"op":"shutdown","params":{}}
PROTOEOF
)
printf '%s\n' "$PROTOCOL_INPUT_MUTATE" | "$CLI" --protocol --skip-startup --system-root "$DB2A" >/dev/null 2>&1 || true
MD5_AFTER_2A="$(md5_of "$DB2A")"
assert_md5_changed "--protocol default-RW persists mutation" "$MD5_BEFORE_2A" "$MD5_AFTER_2A"

# Test 2b: --protocol --lock-opened-roots -- lock suppresses the save.
echo "==> Test 2b: --protocol --lock-opened-roots must not modify Virgin.root"
DB2B="$(stage_copy "virgin_protocol_lock.root")"
MD5_BEFORE_2B="$(md5_of "$DB2B")"
printf '%s\n' "$PROTOCOL_INPUT_MUTATE" | "$CLI" --protocol --skip-startup --lock-opened-roots --system-root "$DB2B" >/dev/null 2>&1 || true
MD5_AFTER_2B="$(md5_of "$DB2B")"
assert_md5_unchanged "--protocol --lock-opened-roots: lock suppresses save" "$MD5_BEFORE_2B" "$MD5_AFTER_2B"

# Test 3: --lock-opened-roots REPL '/exit' only -- no mutate, no save.
echo "==> Test 3: --lock-opened-roots REPL /exit must not modify Virgin.root"
DB3="$(stage_copy "virgin_repl.root")"
MD5_BEFORE_3="$(md5_of "$DB3")"
printf '/exit\n' | "$CLI" --lock-opened-roots --system-root "$DB3" >/dev/null 2>&1 || true
MD5_AFTER_3="$(md5_of "$DB3")"
assert_md5_unchanged "--lock-opened-roots REPL /exit is idempotent" "$MD5_BEFORE_3" "$MD5_AFTER_3"

# Test 4: default-RW -e mutation -- must persist (legacy contract).
echo "==> Test 4 (positive): -e 'workspace.testflag = 42' must persist by default"
DB4="$(stage_copy "virgin_mutate.root")"
MD5_BEFORE_4="$(md5_of "$DB4")"
"$CLI" --system-root "$DB4" -e 'workspace.testflag = 42' >/dev/null 2>&1 || true
MD5_AFTER_4="$(md5_of "$DB4")"
assert_md5_changed "default-RW: mutation writes to disk" "$MD5_BEFORE_4" "$MD5_AFTER_4"

# Re-open and verify the value round-trips through disk.
READBACK="$("$CLI" --system-root "$DB4" -e 'workspace.testflag' 2>/dev/null | tail -1 | tr -d '[:space:]')"
if [ "$READBACK" = "42" ]; then
    echo -e "  ${GREEN}PASS${NC}: workspace.testflag persisted across processes (read back 42)"
    PASSED=$((PASSED + 1))
else
    echo -e "  ${RED}FAIL${NC}: workspace.testflag did not persist; readback='$READBACK'"
    FAILED=$((FAILED + 1))
fi

# Test 5: --lock-opened-roots -- in-memory mutation only, no disk save.
# Reopen and verify workspace.locktestval is undefined (because save was suppressed).
echo "==> Test 5: --lock-opened-roots evaluates but does not persist"
DB5="$(stage_copy "virgin_lock_mutate.root")"
MD5_BEFORE_5="$(md5_of "$DB5")"
"$CLI" --lock-opened-roots --system-root "$DB5" -e 'workspace.locktestval = 1' >/dev/null 2>&1 || true
MD5_AFTER_5="$(md5_of "$DB5")"
assert_md5_unchanged "--lock-opened-roots in-memory mutation does not persist (md5)" "$MD5_BEFORE_5" "$MD5_AFTER_5"

READBACK5="$("$CLI" --system-root "$DB5" -e 'defined(workspace.locktestval)' 2>/dev/null | tail -1 | tr -d '[:space:]')"
if [ "$READBACK5" = "false" ]; then
    echo -e "  ${GREEN}PASS${NC}: workspace.locktestval was not persisted (defined=false)"
    PASSED=$((PASSED + 1))
else
    echo -e "  ${RED}FAIL${NC}: workspace.locktestval unexpectedly persisted; defined='$READBACK5'"
    FAILED=$((FAILED + 1))
fi

# Test 6: no flags + -e mutation -- legacy default-RW contract, must persist.
echo "==> Test 6 (positive): -e 'workspace.legacyflag = 7' must persist by default"
DB6="$(stage_copy "virgin_default_mutate.root")"
MD5_BEFORE_6="$(md5_of "$DB6")"
"$CLI" --system-root "$DB6" -e 'workspace.legacyflag = 7' >/dev/null 2>&1 || true
MD5_AFTER_6="$(md5_of "$DB6")"
assert_md5_changed "default-RW: -e mutation persists" "$MD5_BEFORE_6" "$MD5_AFTER_6"

READBACK6="$("$CLI" --system-root "$DB6" -e 'workspace.legacyflag' 2>/dev/null | tail -1 | tr -d '[:space:]')"
if [ "$READBACK6" = "7" ]; then
    echo -e "  ${GREEN}PASS${NC}: workspace.legacyflag persisted (read back 7)"
    PASSED=$((PASSED + 1))
else
    echo -e "  ${RED}FAIL${NC}: workspace.legacyflag did not persist; readback='$READBACK6'"
    FAILED=$((FAILED + 1))
fi

# Belt-and-braces: re-opening DB4 read-only with --lock-opened-roots after the
# mutation must also be idempotent.
MD5_AFTER_MUTATE="$(md5_of "$DB4")"
"$CLI" --lock-opened-roots --system-root "$DB4" -e 'workspace.testflag' >/dev/null 2>&1 || true
MD5_AFTER_RECHECK="$(md5_of "$DB4")"
assert_md5_unchanged "--lock-opened-roots re-open of mutated DB is idempotent" "$MD5_AFTER_MUTATE" "$MD5_AFTER_RECHECK"

# Final canonical-protection assertion. None of the above should ever have
# opened the canonical Virgin.root, but assert anyway. The expected value is
# the md5 snapshotted at script start -- compared this way the test catches
# real drift without breaking whenever the canonical DB legitimately changes.
ACTUAL_CANONICAL="$(md5_of "$SOURCE_DB")"
if [ "$CANONICAL_MD5_BEFORE" = "$ACTUAL_CANONICAL" ]; then
    echo -e "  ${GREEN}PASS${NC}: canonical Virgin.root md5 unchanged ($ACTUAL_CANONICAL)"
    PASSED=$((PASSED + 1))
else
    echo -e "  ${RED}FAIL${NC}: canonical Virgin.root md5 drifted!"
    echo "    before: $CANONICAL_MD5_BEFORE"
    echo "    after:  $ACTUAL_CANONICAL"
    FAILED=$((FAILED + 1))
fi

echo ""
echo "==============================================="
echo "Results: $PASSED passed, $FAILED failed"
echo "==============================================="

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi
exit 0
