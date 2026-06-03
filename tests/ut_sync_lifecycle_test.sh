#!/bin/bash
#
# Lifecycle test for ODB<->.ut bidirectional sync (issue #675 follow-on).
#
# The sync feature is gated by --ut-sync-dir DIR (or FRONTIER_UT_SYNC_DIR).
# Effective sync base = <ut_sync_dir>/<rootBasename>, where rootBasename is
# the loaded .root's filename (e.g. "Frontier.root"). ODB is authoritative;
# .ut files are a read-only mirror; resolution is last-write-wins by mtime.
#
# These cases require multiple CLI invocations with on-disk .ut edits in
# between, so they live in a shell harness rather than the single-invocation
# YAML integration suite.
#
#   1. Export round-trip: create a verb, save with --ut-sync-dir active, and
#      the .ut file appears on disk with the verb body.
#   2. Last-write-wins (import): edit the .ut to a newer body + future mtime,
#      reload, and the in-ODB verb reflects the .ut edit.
#   3. Convergence: reload again with no further .ut edit, and the value is
#      stable (no oscillation back to the old body).
#   4. Older .ut does NOT clobber: with the .ut mtime older than the ODB
#      value, reload keeps the ODB body (ODB authoritative when newer).
#   5. Broken .ut survives boot: a malformed .ut must not crash startup.
#
# Tests stage a copy of Virgin.root under a private temp dir and never touch
# the canonical databases/Virgin.root.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
SOURCE_DB="$PROJECT_ROOT/databases/Virgin.root"

# This test drives the --ut-sync-dir CLI flag directly. Clear any inherited
# FRONTIER_UT_SYNC_DIR so the runner's environment can't redirect our sync base.
unset FRONTIER_UT_SYNC_DIR

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

pass() { echo -e "  ${GREEN}PASS${NC}: $1"; PASSED=$((PASSED + 1)); }
fail() { echo -e "  ${RED}FAIL${NC}: $1"; FAILED=$((FAILED + 1)); }

md5_of() {
    if command -v md5 >/dev/null 2>&1; then md5 -q "$1"; else md5sum "$1" | awk '{print $1}'; fi
}

# Set a file's mtime relative to now (in seconds, signed). touch -t is portable
# enough for our needs; compute the stamp with `date` for both BSD and GNU.
set_mtime_offset() {
    local file="$1" offset="$2" signed
    # BSD `date -v` requires an explicit +/- sign on the offset.
    case "$offset" in
        -*|+*) signed="$offset" ;;
        *)     signed="+$offset" ;;
    esac
    if date -v "${signed}S" +%Y%m%d%H%M.%S >/dev/null 2>&1; then
        # BSD date (macOS)
        touch -t "$(date -v "${signed}S" +%Y%m%d%H%M.%S)" "$file"
    else
        # GNU date (Linux)
        touch -d "$offset seconds" "$file" 2>/dev/null || \
            touch -d "now $offset seconds" "$file"
    fi
}

CANONICAL_MD5_BEFORE="$(md5_of "$SOURCE_DB")"

STAGE_DIR="$(mktemp -d -t frontier-utsync-test-XXXXXX)"
trap 'rm -rf "$STAGE_DIR"' EXIT

DB="$STAGE_DIR/Frontier.root"
cp "$SOURCE_DB" "$DB"
SYNC_DIR="$STAGE_DIR/utsync"
mkdir -p "$SYNC_DIR"

# The verb under test. scratchpad is a transient table that exists in Virgin.
VERB_PATH="scratchpad.utSyncLifecycle"
# .ut path = <sync_dir>/<rootBasename>/<dotted-path-as-dirs>.ut
UT_FILE="$SYNC_DIR/Frontier.root/scratchpad/utSyncLifecycle.ut"

run_protocol() {
    # $1 = extra CLI args (string), stdin = protocol lines
    local extra="$1"
    # shellcheck disable=SC2086
    "$CLI" --protocol --skip-startup --ut-sync-dir "$SYNC_DIR" $extra --system-root "$DB" 2>/dev/null
}

eval_verb() {
    # Returns the verb's evaluated result on its own line.
    "$CLI" --system-root "$DB" --lock-opened-roots -e "$VERB_PATH()" 2>/dev/null | tail -1 | tr -d '[:space:]'
}

# ---------------------------------------------------------------------------
# Test 1: Export round-trip -- create verb, save, .ut appears on disk.
# ---------------------------------------------------------------------------
echo "==> Test 1: export writes .ut on save"
# install_verb BODY_RETURN_VALUE -- install scratchpad.utSyncLifecycle returning
# the given value via script.newScriptObject, then save (export fires on exit).
# Quoted heredoc preserves JSON backslash escapes (\r \t \"); the @VERB_PATH
# placeholder is substituted afterward so $VERB_PATH expansion can't mangle them.
install_verb() {
    local retval="$1"
    local proto
    proto=$(cat <<'PROTOEOF'
{"id":1,"op":"script/eval","params":{"expression":"script.newScriptObject(\"on utSyncLifecycle() {\\r\\treturn (RETVAL)}\", @VERB_PATH)"}}
{"id":2,"op":"shutdown","params":{}}
PROTOEOF
)
    proto="${proto//VERB_PATH/$VERB_PATH}"
    proto="${proto//RETVAL/$retval}"
    printf '%s\n' "$proto" | run_protocol "" >/dev/null
}

install_verb 42
if [ -f "$UT_FILE" ]; then
    pass "exported .ut exists at expected path"
else
    fail "exported .ut missing at $UT_FILE"
    echo "    sync tree:"; find "$SYNC_DIR" -type f 2>/dev/null | sed 's/^/      /'
fi

# Sanity: the verb evaluates to 42 from the ODB.
RB1="$(eval_verb)"
if [ "$RB1" = "42" ]; then
    pass "verb returns 42 from ODB after create"
else
    fail "verb expected 42, got '$RB1'"
fi

# ---------------------------------------------------------------------------
# Test 2: Last-write-wins -- newer .ut body imports on reload.
# ---------------------------------------------------------------------------
echo "==> Test 2: newer .ut imports (last-write-wins)"
if [ -f "$UT_FILE" ]; then
    # Rewrite the .ut body to return 99 and stamp a future mtime so it wins.
    printf 'on utSyncLifecycle() {\n\treturn (99)}\n' > "$UT_FILE"
    set_mtime_offset "$UT_FILE" 3600
    # Reload (save-on-exit re-stamps ODB to .ut mtime for convergence).
    printf '{"id":1,"op":"shutdown","params":{}}\n' | run_protocol "" >/dev/null
    RB2="$(eval_verb)"
    if [ "$RB2" = "99" ]; then
        pass "newer .ut body (99) imported into ODB"
    else
        fail "expected 99 after newer-.ut import, got '$RB2'"
    fi
else
    fail "skipping test 2: .ut from test 1 absent"
fi

# ---------------------------------------------------------------------------
# Test 3: Convergence -- reload again with no .ut edit stays stable.
# ---------------------------------------------------------------------------
echo "==> Test 3: convergence (no oscillation)"
printf '{"id":1,"op":"shutdown","params":{}}\n' | run_protocol "" >/dev/null
RB3="$(eval_verb)"
if [ "$RB3" = "99" ]; then
    pass "value stable at 99 across reload (converged)"
else
    fail "expected stable 99, got '$RB3' (oscillation)"
fi

# ---------------------------------------------------------------------------
# Test 4: Older .ut does NOT clobber a newer ODB value.
# ---------------------------------------------------------------------------
echo "==> Test 4: older .ut does not clobber ODB"
if [ -f "$UT_FILE" ]; then
    # Tests 2/3 left the .ut with a FUTURE mtime; age it into the past first so
    # the in-session edit below is unambiguously the newest write. (Otherwise
    # the boot import would re-apply the stale future-dated .ut, masking the
    # ODB-newer case this test exists to check.)
    set_mtime_offset "$UT_FILE" -7200
    # Mutate the ODB to 7 with a fresh save -- ODB is now the newest value.
    install_verb 7
    # Point the .ut at an OLD body (123) with a stale mtime. Reload must keep 7.
    printf 'on utSyncLifecycle() {\n\treturn (123)}\n' > "$UT_FILE"
    set_mtime_offset "$UT_FILE" -7200
    printf '{"id":1,"op":"shutdown","params":{}}\n' | run_protocol "" >/dev/null
    RB4="$(eval_verb)"
    if [ "$RB4" = "7" ]; then
        pass "older .ut (123) did not clobber newer ODB (7)"
    else
        fail "expected 7 (ODB authoritative), got '$RB4'"
    fi
else
    fail "skipping test 4: .ut absent"
fi

# ---------------------------------------------------------------------------
# Test 5: Broken .ut survives boot (must not crash startup).
# ---------------------------------------------------------------------------
echo "==> Test 5: malformed .ut does not crash boot"
printf 'on utSyncLifecycle() {\n\treturn (((( unbalanced\n' > "$UT_FILE"
set_mtime_offset "$UT_FILE" 3600
printf '{"id":1,"op":"shutdown","params":{}}\n' | run_protocol "" >/dev/null
BOOT_RC=$?
if [ "$BOOT_RC" -eq 0 ] || [ "$BOOT_RC" -eq 1 ]; then
    pass "boot survived malformed .ut (exit $BOOT_RC, no crash)"
else
    fail "boot crashed on malformed .ut (exit $BOOT_RC)"
fi

# ===========================================================================
# Issue #699 Part 2: BOOT-TRIGGER import-discovery scan (TDD RED).
#
# The scan (to be implemented as ut_sync_scan_and_create, invoked at boot AFTER
# hydration when --ut-sync-dir is active) walks the sync tree, finds .ut leaves
# whose ODB node does NOT exist, and AUTO-CREATES the intermediate table chain
# plus the leaf script, marking them dirty so they persist on save.
#
# Cases A and B are RED until the scan exists: the dropped nodes are never
# created, so defined() returns "false". Case C is a no-damage regression guard,
# expected GREEN throughout (no scan = no damage; with scan, must not duplicate
# or shadow an already-existing node).
#
# A leaf .ut on disk is simply the script body text (no header lines):
#   on hello () {<TAB>return (true)}
# A dotted ODB address a.b.c maps to <sync>/<root>/a/b/c.ut; a node key with
# collision chars is ONE percent-encoded path component (see ut_sync.c codec):
#   key "http://webns.net/mvcb/" -> dir "http%3A%2F%2Fwebns%2Enet%2Fmvcb%2F".
#
# Assertion mechanism mirrors run_protocol usage above: pipe a heredoc of
# script/eval probes to run_protocol and grep the JSON result for the id-tagged
# value we expect. probe_value extracts the "value" for a given response id.
# ---------------------------------------------------------------------------
ROOT_SYNC="$SYNC_DIR/Frontier.root"

# probe_value OUTPUT ID -- echo the result "value" string for response "id":ID.
# Matches the protocol JSON shape: {"id":N,"result":{"value":"X",...},...}.
probe_value() {
    local output="$1" id="$2"
    printf '%s\n' "$output" \
        | grep -o "\"id\":$id,\"result\":{\"value\":\"[^\"]*\"" \
        | sed 's/.*"value":"//; s/"$//'
}

# ---------------------------------------------------------------------------
# Test 6 (Case A): boot scan auto-creates a dropped .ut under a NEW subdir
# with a simple identifier path. New table "zzdroptest", new leaf "hello".
# RED until ut_sync_scan_and_create exists.
#
# IMPORTANT discriminator note: defined()/typeof()/sizeof() on a DIRECT child of
# system.verbs.builtins (e.g. @...builtins.zzdroptest) report a phantom
# "true"/"addr"/32 even pre-scan -- a verb-search/EFP resolution artifact, NOT a
# real ODB node. Verified: defined(@...builtins.neverEverDropped12345) -> true,
# but its grandchild .child -> false. So the ONLY honest signal is the GRANDCHILD
# leaf: @...builtins.zzdroptest.hello resolves to true iff the scan really built
# BOTH the zzdroptest table AND the hello leaf. We assert on the leaf, and also
# on calling the created verb (hello() -> true), which cannot succeed unless a
# real script object exists.
# ---------------------------------------------------------------------------
echo "==> Test 6 (Case A): boot scan creates dropped .ut (simple path)"
DROP_A_DIR="$ROOT_SYNC/system/verbs/builtins/zzdroptest"
DROP_A_FILE="$DROP_A_DIR/hello.ut"
mkdir -p "$DROP_A_DIR"
# Minimal well-formed leaf script body (no header lines required on disk).
printf 'on hello () {\n\treturn (true)}\n' > "$DROP_A_FILE"

PROBE_A=$(printf '%s\n' \
    '{"id":1,"op":"script/eval","params":{"expression":"defined(@system.verbs.builtins.zzdroptest.hello)"}}' \
    '{"id":2,"op":"script/eval","params":{"expression":"system.verbs.builtins.zzdroptest.hello()"}}' \
    '{"id":3,"op":"shutdown","params":{}}' \
    | run_protocol "")

A_LEAF="$(probe_value "$PROBE_A" 1)"
A_CALL="$(probe_value "$PROBE_A" 2)"
if [ "$A_LEAF" = "true" ]; then
    pass "leaf zzdroptest.hello created by boot scan (table chain built)"
else
    fail "leaf zzdroptest.hello NOT created (defined -> '${A_LEAF:-<none>}')"
fi
if [ "$A_CALL" = "true" ]; then
    pass "created verb zzdroptest.hello() is callable (returns true)"
else
    fail "created verb zzdroptest.hello() not callable (got '${A_CALL:-<none>}')"
fi

# Persistence: the previous run saved on shutdown. Re-boot and re-probe; a
# correctly-dirtied orphan must survive the save/reload.
PROBE_A2=$(printf '%s\n' \
    '{"id":1,"op":"script/eval","params":{"expression":"defined(@system.verbs.builtins.zzdroptest.hello)"}}' \
    '{"id":2,"op":"shutdown","params":{}}' \
    | run_protocol "")
A_LEAF2="$(probe_value "$PROBE_A2" 1)"
if [ "$A_LEAF2" = "true" ]; then
    pass "created node persists across save/reload"
else
    fail "created node did NOT persist across reload (defined -> '${A_LEAF2:-<none>}')"
fi

# ---------------------------------------------------------------------------
# Test 7 (Case B): boot scan auto-creates a URL-keyed (percent-encoded) path.
# New table "zztest" so we never collide with the real xml.rss.moduleDrivers.
# On disk the URL key is ONE percent-encoded component; the UserTalk address
# uses the RAW key in brackets -- this exercises reverse-map + decode end to end.
# RED until ut_sync_scan_and_create exists.
# ---------------------------------------------------------------------------
echo "==> Test 7 (Case B): boot scan creates URL-keyed (percent-encoded) path"
# "http://webns.net/mvcb/" encodes to http%3A%2F%2Fwebns%2Enet%2Fmvcb%2F
URLKEY_ENC='http%3A%2F%2Fwebns%2Enet%2Fmvcb%2F'
DROP_B_DIR="$ROOT_SYNC/system/verbs/builtins/zztest/moduleDrivers/$URLKEY_ENC"
DROP_B_FILE="$DROP_B_DIR/init.ut"
mkdir -p "$DROP_B_DIR"
printf 'on init () {\n\treturn (true)}\n' > "$DROP_B_FILE"

PROBE_B=$(printf '%s\n' \
    '{"id":1,"op":"script/eval","params":{"expression":"defined(@system.verbs.builtins.zztest.moduleDrivers[\"http://webns.net/mvcb/\"].init)"}}' \
    '{"id":2,"op":"script/eval","params":{"expression":"typeof(@system.verbs.builtins.zztest.moduleDrivers[\"http://webns.net/mvcb/\"])"}}' \
    '{"id":3,"op":"shutdown","params":{}}' \
    | run_protocol "")

B_LEAF="$(probe_value "$PROBE_B" 1)"
B_TABLE_TYPE="$(probe_value "$PROBE_B" 2)"
if [ "$B_LEAF" = "true" ]; then
    pass "URL-keyed leaf init created by boot scan (decode path works)"
else
    fail "URL-keyed leaf NOT created (defined -> '${B_LEAF:-<none>}')"
fi
if [ "$B_TABLE_TYPE" = "addr" ]; then
    pass "URL-keyed node is a table (typeof -> addr)"
else
    # Pre-scan the chain is absent, so typeof() errors out ("Script evaluation
    # failed") and probe_value yields <none>; post-scan it must resolve to addr.
    fail "URL-keyed node not a table (typeof -> '${B_TABLE_TYPE:-<none> (chain absent)}')"
fi

# ---------------------------------------------------------------------------
# Test 8 (Case C): scan must NOT duplicate / NOT create for an EXISTING node.
# No new drop. The scan runs over the full existing corpus; a well-known node
# must remain intact, singular, and callable. NO-DAMAGE GUARD: expected GREEN
# throughout (pre-implementation no scan = no damage; post-implementation the
# scan must treat existing/lazy nodes as present and not shadow them).
# ---------------------------------------------------------------------------
echo "==> Test 8 (Case C): scan does not damage an existing node (guard)"
PROBE_C=$(printf '%s\n' \
    '{"id":1,"op":"script/eval","params":{"expression":"defined(@system.verbs.builtins.string.upper)"}}' \
    '{"id":2,"op":"script/eval","params":{"expression":"string.upper(\"ab\")"}}' \
    '{"id":3,"op":"shutdown","params":{}}' \
    | run_protocol "")
C_DEFINED="$(probe_value "$PROBE_C" 1)"
C_RESULT="$(probe_value "$PROBE_C" 2)"
if [ "$C_DEFINED" = "true" ]; then
    pass "existing node string.upper still defined after scan"
else
    fail "existing node string.upper missing after scan (defined -> '${C_DEFINED:-<none>}')"
fi
if [ "$C_RESULT" = "AB" ]; then
    pass "existing node string.upper still works (\"ab\" -> AB)"
else
    fail "string.upper broken after scan (got '${C_RESULT:-<none>}')"
fi

# ---------------------------------------------------------------------------
# Canonical protection: the source Virgin.root must be untouched.
# ---------------------------------------------------------------------------
ACTUAL_CANONICAL="$(md5_of "$SOURCE_DB")"
if [ "$CANONICAL_MD5_BEFORE" = "$ACTUAL_CANONICAL" ]; then
    pass "canonical Virgin.root md5 unchanged"
else
    fail "canonical Virgin.root md5 drifted!"
    echo "    before: $CANONICAL_MD5_BEFORE"
    echo "    after:  $ACTUAL_CANONICAL"
fi

echo ""
echo "==============================================="
echo "Results: $PASSED passed, $FAILED failed"
echo "==============================================="

[ "$FAILED" -gt 0 ] && exit 1
exit 0
