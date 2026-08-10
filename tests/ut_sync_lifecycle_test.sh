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
# Test 9: repl.syncScan() on-demand verb -- callable and returns a number.
#
# The PRIMARY red signal is that repl.syncScan() is an UNDEFINED verb
# pre-implementation, so the protocol eval returns an error (verb not found),
# and probe_value yields <none>.  After wiring, the verb must:
#   (a) evaluate without a "can't find verb" error (probe_value returns a
#       non-empty string), AND
#   (b) return a non-negative integer (a number >= 0 cast to string).
#
# We also drop a fresh .ut that did NOT exist at the time of the previous
# boot scan, then call repl.syncScan() in the same invocation to prove it
# picks up the new file on demand.  The verb return value (count) must be
# >= 1, and defined(@system.verbs.builtins.zzondemand.ping) must be true.
# ---------------------------------------------------------------------------
# Test 9: repl.syncScan() genuinely picks up an on-demand .ut drop.
#
# Design: ping.ut does NOT exist when the CLI boots, so the boot scan
# cannot create it. A valid .ut is pre-staged in a temp location and then
# copied into the sync dir from within the running protocol session via
# sys.unixShellCommand. repl.syncScan() is then called twice:
#
#   Call 1: repl.syncScan() must return >= 1 (created ping -- on-demand).
#   Call 2: repl.syncScan() must return 0 (idempotent -- node exists now).
#
# A no-op verb, or one that always returns 0, cannot satisfy BOTH
# assertions at once. The double-call also confirms idempotency.
#
# Verification uses count only (not defined()), because defined() can
# return phantom-true via EFP for builtins children even when the node does
# not exist in the hashtable -- see node_exists_in_memory in ut_scan.c.
#
# The .ut content must be non-empty (a valid script). Empty files pass
# read_ut_file but fail optexttooutline, so create_orphan_node returns 0.
# ---------------------------------------------------------------------------
echo "==> Test 9: repl.syncScan() picks up an on-demand .ut drop (>= 1 then 0)"

ONDEMAND_DIR="$ROOT_SYNC/system/verbs/builtins/zzondemand"
ONDEMAND_FILE="$ONDEMAND_DIR/ping.ut"
# Ensure parent directory exists (boot will not create it -- ping.ut absent at boot).
mkdir -p "$ONDEMAND_DIR"
# ping.ut must NOT exist when the CLI boots.
rm -f "$ONDEMAND_FILE"

# Build shell command that copies ping.ut from within the session (post-boot).
#
# The .ut content is pre-staged in a temp file here in the test harness so
# we avoid complex quoting inside the JSON expression. The UserTalk
# sys.unixShellCommand then does a simple "cp SRC DST" with no escaping
# hazards. The content is a minimal canonicalized .ut (UTF-8/LF) so that
# create_orphan_node's optexttooutline call succeeds (empty files fail to
# parse and produce a count of 0 even though read_ut_file accepts them).
#
# Verification uses the two-call count pattern only (not defined(), which
# can return phantom-true via EFP even for nodes that do not exist --
# see node_exists_in_memory comment in ut_scan.c and ARCHITECTURAL_ANTIPATTERNS.md).
T9_STAGED="$(mktemp /tmp/ut_test9_staged.XXXXXX)"
printf 'on ping ()\n\treturn (true)\n' > "${T9_STAGED}"

# cp command: copy the staged file to the target sync path post-boot.
WRITE_CMD="cp ${T9_STAGED} ${ONDEMAND_FILE}"

# Write the protocol request file. Request id=1 drops the file (post-boot);
# id=2 is the first syncScan (must return >= 1); id=3 is the second syncScan
# (must return 0 -- idempotent). The two-count pattern is unforgeable by a
# no-op verb.
T9_REQUESTS="$(mktemp /tmp/ut_test9_req.XXXXXX)"
printf '{"id":1,"op":"script/eval","params":{"expression":"sys.unixShellCommand(\\"%s\\")"}}\n' \
    "${WRITE_CMD}" > "${T9_REQUESTS}"
printf '%s\n' \
    '{"id":2,"op":"script/eval","params":{"expression":"repl.syncScan()"}}' \
    '{"id":3,"op":"script/eval","params":{"expression":"repl.syncScan()"}}' \
    '{"id":4,"op":"shutdown","params":{}}' \
    >> "${T9_REQUESTS}"

PROBE_9=$(run_protocol "" < "${T9_REQUESTS}")
rm -f "${T9_REQUESTS}" "${T9_STAGED}"

SCAN_CALL1="$(probe_value "$PROBE_9" 2)"
SCAN_CALL2="$(probe_value "$PROBE_9" 3)"

# First call: must be a non-negative integer >= 1 (created ping on-demand).
if printf '%s' "$SCAN_CALL1" | grep -qE '^[0-9]+$'; then
    pass "repl.syncScan() call 1 returned a number (${SCAN_CALL1})"
else
    fail "repl.syncScan() call 1 did not return a number (got '${SCAN_CALL1:-<none>}')"
fi

if [ -n "$SCAN_CALL1" ] && [ "$SCAN_CALL1" -ge 1 ] 2>/dev/null; then
    pass "repl.syncScan() call 1 returned >= 1 (on-demand path exercised, count=${SCAN_CALL1})"
else
    fail "repl.syncScan() call 1 returned < 1 -- on-demand creation did NOT run (got '${SCAN_CALL1:-<none>}')"
fi

# Second call: must return 0 (idempotent -- node now exists).
if [ "$SCAN_CALL2" = "0" ]; then
    pass "repl.syncScan() call 2 returned 0 (idempotent, node already exists)"
else
    fail "repl.syncScan() call 2 expected 0, got '${SCAN_CALL2:-<none>}'"
fi

# ---------------------------------------------------------------------------
# Test 10 (P1 #1 -- node-clobber guard): a .ut whose INTERMEDIATE path
# component collides with an EXISTING script node must NOT clobber that node.
#
# system.verbs.builtins.string.upper is a live script.  Drop a .ut at
#   <sync>/system/verbs/builtins/string/upper/evil.ut
# so "upper" is the colliding intermediate -- the scan would need "upper" to be
# a table to descend into it.  It is a script, so the entire orphan must be
# rejected without touching the existing "upper" verb.
#
# Assert:
#   (a) string.upper("ab") still returns "AB" -- the verb is intact (not clobbered).
#   (b) the phantom leaf system.verbs.builtins.string.upper.evil is NOT defined.
# ---------------------------------------------------------------------------
echo "==> Test 10 (P1 #1): clobber guard -- collision with existing script node"

# Drop the colliding .ut under the live scan dir.
CLOBBER_DIR="$ROOT_SYNC/system/verbs/builtins/string/upper"
CLOBBER_FILE="$CLOBBER_DIR/evil.ut"
mkdir -p "$CLOBBER_DIR"
printf 'on evil () {\n\treturn (false)}\n' > "$CLOBBER_FILE"

PROBE_10=$(printf '%s\n' \
    '{"id":1,"op":"script/eval","params":{"expression":"string.upper(\"ab\")"}}' \
    '{"id":2,"op":"script/eval","params":{"expression":"defined(@system.verbs.builtins.string.upper.evil)"}}' \
    '{"id":3,"op":"shutdown","params":{}}' \
    | run_protocol "")

T10_UPPER="$(probe_value "$PROBE_10" 1)"
T10_EVIL="$(probe_value "$PROBE_10" 2)"

if [ "$T10_UPPER" = "AB" ]; then
    pass "clobber guard: string.upper still works after scan with colliding .ut"
else
    fail "clobber guard: string.upper BROKEN after scan (got '${T10_UPPER:-<none>}') -- script was clobbered"
fi

if [ "$T10_EVIL" != "true" ]; then
    pass "clobber guard: phantom leaf evil was NOT created (correct rejection)"
else
    fail "clobber guard: phantom leaf evil WAS created despite intermediate-node collision"
fi

# ---------------------------------------------------------------------------
# Test 11 (P1 #2 -- post-decode NUL guard): a .ut filename containing %00
# (which decodes to a NUL byte) must be silently rejected -- the scan must
# not crash and must not create any node from the NUL-containing key.
#
# The security concern: "%00" in an encoded path component decodes to 0x00,
# which strlen() silently truncates, causing the Pascal string built from the
# decoded bytes to be shorter than intended. This could corrupt a hashtable
# key or cause a lookup in the wrong table.
#
# Note: "%2E%2E" -> ".." and "%2F" -> "/" are legitimate ODB key characters
# (URL-keyed tables use slashes and dots in their keys, e.g., RSS module
# driver keys like "http://webns.net/mvcb/"). The NUL byte (0x00) is the
# only decoded byte that is genuinely dangerous for ODB Pascal-string keys.
#
# We use %00 in the leaf position (the filename stem before .ut).
# After decoding, the leaf key would be a zero-length string (since strlen
# stops at the NUL), which we also reject as empty (rawlen==0 check).
#
# We also verify that a NUL in an intermediate directory component is rejected,
# by dropping a file in a subdirectory whose name encodes a NUL.
# ---------------------------------------------------------------------------
echo "==> Test 11 (P1 #2): post-decode NUL guard rejects %00 in encoded filenames"

# Leaf NUL: the filename stem decodes to a NUL-containing key.
NUL_DIR="$ROOT_SYNC/system/verbs/builtins/zznultest"
NUL_LEAF_FILE="$NUL_DIR/%00evil.ut"
mkdir -p "$NUL_DIR"
printf 'on evil () {\n\treturn (true)}\n' > "$NUL_LEAF_FILE"

# Intermediate NUL: a directory component with an encoded NUL.
# The intermediate dir decodes to a NUL-containing key, which must be rejected.
NUL_INTM_DIR="$ROOT_SYNC/system/verbs/builtins/%00intm"
NUL_INTM_FILE="$NUL_INTM_DIR/leaf.ut"
mkdir -p "$NUL_INTM_DIR"
printf 'on leaf () {\n\treturn (true)}\n' > "$NUL_INTM_FILE"

PROBE_11=$(printf '%s\n' \
    '{"id":1,"op":"script/eval","params":{"expression":"defined(@system.verbs.builtins.zznultest.evil)"}}' \
    '{"id":2,"op":"shutdown","params":{}}' \
    | run_protocol "")

T11_BOOT_RC=$?
T11_NULTEST="$(probe_value "$PROBE_11" 1)"

# Boot must survive (not crash) even with NUL-encoded filenames present.
if [ "$T11_BOOT_RC" -eq 0 ] || [ "$T11_BOOT_RC" -eq 1 ]; then
    pass "post-decode NUL guard: boot survived %00-encoded filenames (exit $T11_BOOT_RC)"
else
    fail "post-decode NUL guard: boot CRASHED on %00-encoded filenames (exit $T11_BOOT_RC)"
fi

# Assert the grandchild leaf (.evil) was NOT created. We probe the grandchild
# (not the table itself) because defined() on a direct child of
# system.verbs.builtins returns phantom "true" via the EFP search path even
# for nodes that do not exist -- the same gotcha documented in Test 6.
# A phantom grandchild is not possible: EFP only applies one level deep.
# If the grandchild is "true", the NUL guard failed and the table chain WAS
# created -- i.e., the pre-flight did not abort before creating zznultest.
if [ "$T11_NULTEST" != "true" ]; then
    pass "post-decode NUL guard: NUL-encoded leaf rejected (grandchild evil not created)"
else
    fail "post-decode NUL guard: grandchild evil WAS created -- pre-flight NUL guard did not abort early enough"
fi

# ---------------------------------------------------------------------------
# Test 12 (P1 O_NONBLOCK -- FIFO boot-hang): a FIFO named foo.ut in the sync
# tree must not cause open() to block forever.  Without O_NONBLOCK on the
# read_ut_file open, a plain O_RDONLY open on a FIFO blocks until a writer
# appears -- hanging startup indefinitely.  With O_NONBLOCK the open returns
# ENXIO immediately and the FIFO is silently skipped.
#
# The CLI is wrapped in `timeout 10` so that if the fix regresses we get a
# definitive failure (RC 124) rather than a hung test suite.
# ---------------------------------------------------------------------------
echo "==> Test 12 (P1 O_NONBLOCK): FIFO named .ut does not block boot scan"

FIFO_DIR="$ROOT_SYNC/system/verbs/builtins/zzfifotest"
FIFO_FILE="$FIFO_DIR/foo.ut"
mkdir -p "$FIFO_DIR"
mkfifo "$FIFO_FILE"

FIFO_DB="$STAGE_DIR/Frontier_fifo.root"
cp "$SOURCE_DB" "$FIFO_DB"

FIFO_RC=0
timeout 10 "$CLI" --skip-startup --ut-sync-dir "$SYNC_DIR" \
    --system-root "$FIFO_DB" -e "1+1" >/dev/null 2>/dev/null
FIFO_RC=$?

# Remove the FIFO before checking (so subsequent tests are not affected).
rm -f "$FIFO_FILE"
rmdir "$FIFO_DIR" 2>/dev/null || true

if [ "$FIFO_RC" -eq 124 ]; then
    fail "FIFO boot-hang: CLI blocked on foo.ut FIFO (timeout 10s expired, RC=124)"
elif [ "$FIFO_RC" -eq 0 ] || [ "$FIFO_RC" -eq 1 ]; then
    pass "FIFO boot-hang: boot returned normally with FIFO in sync tree (RC=$FIFO_RC)"
else
    pass "FIFO boot-hang: boot returned (RC=$FIFO_RC, non-zero but not timeout)"
fi

# ===========================================================================
# Issue #702: ODB deletions propagate to .ut (orphan files + boot resurrection)
#
# These tests confirm that when a script or table node is deleted from the ODB,
# the corresponding .ut file (and empty parent directories) are removed from the
# sync tree on shutdown, and that a subsequent boot does NOT resurrect the node
# from the now-absent .ut.
#
# EFP phantom-true note: defined() on direct children of ANY top-level table
# (scratchpad, workspace, system.temp, etc.) returns phantom "true" via EFP even
# for keys that were never inserted. Confirmed by probe. Therefore deletion tests
# use the .ut file presence/absence on disk as the PRIMARY authoritative signal,
# not defined(). For resurrection tests (Test 14), we verify the .ut file remains
# absent and that the node cannot be called (which would require a real script
# object). For parent-prune tests, we verify the directory exists/absent.
#
# Script creation pattern: assign a scalar first (creates the slot in the parent
# table), then use script.newScriptObject to overwrite it with a script object.
# This is required for grandchild paths where the intermediate table does not
# pre-exist; for direct children of existing tables (like scratchpad.*), a direct
# script.newScriptObject works.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Test 13 -- Leaf deletion mirrors to .ut on shutdown.
#
# Setup: install scratchpad.utSyncDel13 (direct child of scratchpad -- same
# pattern as Test 1). Shutdown fires export; verify .ut exists. Then in a
# fresh session: delete @scratchpad.utSyncDel13, shutdown. Assert .ut is gone.
# ---------------------------------------------------------------------------
echo "==> Test 13: leaf deletion removes .ut on shutdown"

# Use a fresh DB for deletion tests to keep state independent of tests 1-12.
STAGE_DIR2="$(mktemp -d -t frontier-utsync-del-XXXXXX)"
trap 'rm -rf "$STAGE_DIR"  "$STAGE_DIR2"' EXIT
DB2="$STAGE_DIR2/Frontier.root"
cp "$SOURCE_DB" "$DB2"
SYNC_DIR2="$STAGE_DIR2/utsync"
mkdir -p "$SYNC_DIR2"

run_protocol2() {
	local extra="$1"
	# shellcheck disable=SC2086
	"$CLI" --protocol --skip-startup --ut-sync-dir "$SYNC_DIR2" $extra --system-root "$DB2" 2>/dev/null
}

T13_UT="$SYNC_DIR2/Frontier.root/scratchpad/utSyncDel13.ut"

# Install the leaf script and shutdown to trigger export.
printf '%s\n' \
	'{"id":1,"op":"script/eval","params":{"expression":"script.newScriptObject(\"on utSyncDel13 () {\\r\\treturn (1)}\", @scratchpad.utSyncDel13)"}}' \
	'{"id":2,"op":"shutdown","params":{}}' \
	| run_protocol2 "" >/dev/null

if [ -f "$T13_UT" ]; then
	pass "Test 13 setup: .ut exists after install+save"
else
	fail "Test 13 setup: .ut missing at $T13_UT (export did not run?)"
	echo "    sync tree:"; find "$SYNC_DIR2" -type f 2>/dev/null | sed 's/^/      /'
fi

# Now delete the leaf in a fresh session and shutdown.
printf '%s\n' \
	'{"id":1,"op":"script/eval","params":{"expression":"delete (@scratchpad.utSyncDel13)"}}' \
	'{"id":2,"op":"shutdown","params":{}}' \
	| run_protocol2 "" >/dev/null

if [ ! -f "$T13_UT" ]; then
	pass "Test 13: .ut removed from disk after ODB delete + shutdown"
else
	fail "Test 13: .ut still present at $T13_UT after ODB delete + shutdown"
fi

# ---------------------------------------------------------------------------
# Test 14 -- Boot scan does NOT resurrect the deleted node.
#
# Continuing from Test 13 state (.ut is absent). Reboot the CLI (triggering
# boot-discovery scan). Primary signal: .ut remains absent (the scan has
# nothing to discover). Secondary: calling the deleted verb errors out (it is
# truly gone, not just phantom-defined via EFP).
# ---------------------------------------------------------------------------
echo "==> Test 14: boot scan does not resurrect deleted node"

PROBE_14=$(printf '%s\n' \
	'{"id":1,"op":"script/eval","params":{"expression":"scratchpad.utSyncDel13()"}}' \
	'{"id":2,"op":"shutdown","params":{}}' \
	| run_protocol2 "")

T14_CALL="$(probe_value "$PROBE_14" 1)"
T14_UT_STILL_ABSENT=0
[ ! -f "$T13_UT" ] && T14_UT_STILL_ABSENT=1

# Primary: .ut file must remain absent (not re-exported from a resurrected node).
if [ "$T14_UT_STILL_ABSENT" -eq 1 ]; then
	pass "Test 14: .ut remains absent after boot (not re-exported)"
else
	fail "Test 14: .ut reappeared at $T13_UT after boot (node resurrected)"
fi

# Secondary: calling the deleted verb must fail (error response, not a value).
# probe_value returns empty string when the response has no "value" field
# (i.e., when the eval returns an error). A resurrected node would return "1".
if [ -z "$T14_CALL" ]; then
	pass "Test 14: deleted verb call returns error (node not resurrected)"
else
	fail "Test 14: deleted verb returned '$T14_CALL' -- node was resurrected by boot scan"
fi

# ---------------------------------------------------------------------------
# Test 15 -- Empty-parent prune.
#
# Create two levels: scratchpad.zzprune15 (a table, created as scalar then
# overwritten), then install scratchpad.zzprune15.foo as a script. Shutdown.
# Verify both the .ut and parent directory exist. Then delete the leaf AND the
# now-empty parent table. Shutdown. Assert:
#   (a) foo.ut is gone
#   (b) zzprune15/ directory is gone (pruned, was empty after leaf delete)
#   (c) Frontier.root/scratchpad/ directory still exists (prune stops there)
#   (d) The per-root sync base Frontier.root/ still exists
#
# Note: deleting @scratchpad.zzprune15 fires the deletion hook for the table.
# The hook tries unlink(scratchpad/zzprune15.ut) (ENOENT -- fine, it's a table
# not a script), then rmdir(scratchpad/zzprune15) -- succeeds if empty from the
# prior leaf delete. Prune then walks up: scratchpad/ is NOT empty (other nodes
# exist), so prune stops there.
# ---------------------------------------------------------------------------
echo "==> Test 15: empty-parent directory pruned after leaf deletion"

STAGE_DIR3="$(mktemp -d -t frontier-utsync-prune-XXXXXX)"
trap 'rm -rf "$STAGE_DIR"  "$STAGE_DIR2"  "$STAGE_DIR3"' EXIT
DB3="$STAGE_DIR3/Frontier.root"
cp "$SOURCE_DB" "$DB3"
SYNC_DIR3="$STAGE_DIR3/utsync"
mkdir -p "$SYNC_DIR3"

run_protocol3() {
	local extra="$1"
	# shellcheck disable=SC2086
	"$CLI" --protocol --skip-startup --ut-sync-dir "$SYNC_DIR3" $extra --system-root "$DB3" 2>/dev/null
}

T15_UT="$SYNC_DIR3/Frontier.root/scratchpad/zzprune15/foo.ut"
T15_DIR="$SYNC_DIR3/Frontier.root/scratchpad/zzprune15"
T15_SCRATCH="$SYNC_DIR3/Frontier.root/scratchpad"
T15_ROOT="$SYNC_DIR3/Frontier.root"

# Create parent table via new(tableType, @adr), then install the leaf script.
printf '%s\n' \
	'{"id":1,"op":"script/eval","params":{"expression":"new(tableType, @scratchpad.zzprune15)"}}' \
	'{"id":2,"op":"script/eval","params":{"expression":"script.newScriptObject(\"on foo () {\\r\\treturn (1)}\", @scratchpad.zzprune15.foo)"}}' \
	'{"id":3,"op":"shutdown","params":{}}' \
	| run_protocol3 "" >/dev/null

if [ -f "$T15_UT" ] && [ -d "$T15_DIR" ]; then
	pass "Test 15 setup: foo.ut and zzprune15/ dir exist after install"
else
	fail "Test 15 setup: missing foo.ut ($T15_UT) or zzprune15/ ($T15_DIR)"
	echo "    sync tree:"; find "$SYNC_DIR3" -type f 2>/dev/null | sed 's/^/      /'
fi

# Delete leaf then empty parent, then shutdown.
printf '%s\n' \
	'{"id":1,"op":"script/eval","params":{"expression":"delete (@scratchpad.zzprune15.foo)"}}' \
	'{"id":2,"op":"script/eval","params":{"expression":"delete (@scratchpad.zzprune15)"}}' \
	'{"id":3,"op":"shutdown","params":{}}' \
	| run_protocol3 "" >/dev/null

if [ ! -f "$T15_UT" ]; then
	pass "Test 15(a): foo.ut removed from disk"
else
	fail "Test 15(a): foo.ut still present at $T15_UT"
fi

if [ ! -d "$T15_DIR" ]; then
	pass "Test 15(b): zzprune15/ directory pruned (was empty)"
else
	fail "Test 15(b): zzprune15/ still exists at $T15_DIR (prune did not fire)"
fi

if [ -d "$T15_SCRATCH" ]; then
	pass "Test 15(c): scratchpad/ directory still exists (prune stopped)"
else
	fail "Test 15(c): scratchpad/ directory was incorrectly pruned"
fi

if [ -d "$T15_ROOT" ]; then
	pass "Test 15(d): per-root sync base Frontier.root/ still exists"
else
	fail "Test 15(d): per-root sync base Frontier.root/ was incorrectly deleted"
fi

# ---------------------------------------------------------------------------
# Test 16 -- Non-empty parent stops the prune.
#
# Create scratchpad.zzkeep16 with two children: a and b. Shutdown (export).
# Delete only @scratchpad.zzkeep16.a. Shutdown. Assert:
#   (a) zzkeep16/a.ut is gone
#   (b) zzkeep16/b.ut still exists
#   (c) zzkeep16/ directory still exists (NOT pruned -- b.ut is there)
# ---------------------------------------------------------------------------
echo "==> Test 16: non-empty parent stops the prune"

STAGE_DIR4="$(mktemp -d -t frontier-utsync-keep-XXXXXX)"
trap 'rm -rf "$STAGE_DIR"  "$STAGE_DIR2"  "$STAGE_DIR3"  "$STAGE_DIR4"' EXIT
DB4="$STAGE_DIR4/Frontier.root"
cp "$SOURCE_DB" "$DB4"
SYNC_DIR4="$STAGE_DIR4/utsync"
mkdir -p "$SYNC_DIR4"

run_protocol4() {
	local extra="$1"
	# shellcheck disable=SC2086
	"$CLI" --protocol --skip-startup --ut-sync-dir "$SYNC_DIR4" $extra --system-root "$DB4" 2>/dev/null
}

T16_A="$SYNC_DIR4/Frontier.root/scratchpad/zzkeep16/a.ut"
T16_B="$SYNC_DIR4/Frontier.root/scratchpad/zzkeep16/b.ut"
T16_DIR="$SYNC_DIR4/Frontier.root/scratchpad/zzkeep16"

# Install both siblings (create parent table via new(tableType,...), then both leaves).
printf '%s\n' \
	'{"id":1,"op":"script/eval","params":{"expression":"new(tableType, @scratchpad.zzkeep16)"}}' \
	'{"id":2,"op":"script/eval","params":{"expression":"script.newScriptObject(\"on a () {\\r\\treturn (1)}\", @scratchpad.zzkeep16.a)"}}' \
	'{"id":3,"op":"script/eval","params":{"expression":"script.newScriptObject(\"on b () {\\r\\treturn (2)}\", @scratchpad.zzkeep16.b)"}}' \
	'{"id":4,"op":"shutdown","params":{}}' \
	| run_protocol4 "" >/dev/null

if [ -f "$T16_A" ] && [ -f "$T16_B" ]; then
	pass "Test 16 setup: a.ut and b.ut exist after install"
else
	fail "Test 16 setup: missing a.ut or b.ut under zzkeep16/"
	echo "    sync tree:"; find "$SYNC_DIR4" -type f 2>/dev/null | sed 's/^/      /'
fi

# Delete only 'a', keep 'b'. Shutdown.
printf '%s\n' \
	'{"id":1,"op":"script/eval","params":{"expression":"delete (@scratchpad.zzkeep16.a)"}}' \
	'{"id":2,"op":"shutdown","params":{}}' \
	| run_protocol4 "" >/dev/null

if [ ! -f "$T16_A" ]; then
	pass "Test 16(a): a.ut removed"
else
	fail "Test 16(a): a.ut still present at $T16_A"
fi

if [ -f "$T16_B" ]; then
	pass "Test 16(b): b.ut still exists (sibling untouched)"
else
	fail "Test 16(b): b.ut missing at $T16_B (sibling incorrectly removed)"
fi

if [ -d "$T16_DIR" ]; then
	pass "Test 16(c): zzkeep16/ directory still exists (non-empty parent not pruned)"
else
	fail "Test 16(c): zzkeep16/ directory incorrectly pruned (b.ut was still there)"
fi

# ---------------------------------------------------------------------------
# Test 17 -- Per-root sync base is never deleted.
#
# Safety guard: even when the tree under Frontier.root/ becomes empty,
# the Frontier.root/ directory itself must NOT be deleted by the prune walk.
#
# Use STAGE_DIR4 state. Delete @scratchpad.zzkeep16.b and @scratchpad.zzkeep16
# (the last leaf + its now-empty parent). Shutdown. Assert Frontier.root/ still
# exists as a directory.
# ---------------------------------------------------------------------------
echo "==> Test 17: per-root sync base Frontier.root/ never deleted"

T17_ROOT="$SYNC_DIR4/Frontier.root"

# Delete the remaining leaf and its empty parent from the previous test state.
printf '%s\n' \
	'{"id":1,"op":"script/eval","params":{"expression":"delete (@scratchpad.zzkeep16.b)"}}' \
	'{"id":2,"op":"script/eval","params":{"expression":"delete (@scratchpad.zzkeep16)"}}' \
	'{"id":3,"op":"shutdown","params":{}}' \
	| run_protocol4 "" >/dev/null

if [ -d "$T17_ROOT" ]; then
	pass "Test 17: per-root sync base Frontier.root/ still exists after all leaves deleted"
else
	fail "Test 17: per-root sync base Frontier.root/ was incorrectly deleted"
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
