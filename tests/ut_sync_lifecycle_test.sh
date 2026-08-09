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
# Unit 1.5 caveat closure (TDD RED until implemented):
#
#   Test 13: a .ut that fails to COMPILE must be rejected on import -- the
#            existing ODB script is kept and a loud diagnostic is logged.
#            (Previously: the broken text imported silently as a
#            non-compiling outline; the verb broke with no announcement.)
#   Test 14: when BOTH sides changed since the last sync (recorded content
#            hash), neither side may clobber the other -- the import is
#            refused, the shutdown export is refused, and the conflict is
#            logged loudly. (Previously: newer mtime silently won.)
#   Test 15: a broken NEW .ut (no existing ODB node) must NOT be created by
#            the import-discovery scan. (Previously: created silently as a
#            non-compiling script.)
# ---------------------------------------------------------------------------

# run_protocol_err ERRFILE -- like run_protocol but append stderr to ERRFILE
# so tests can assert on logged diagnostics.
run_protocol_err() {
    local errfile="$1"
    "$CLI" --protocol --skip-startup --ut-sync-dir "$SYNC_DIR" --system-root "$DB" 2>>"$errfile"
}

# eval_verb_named DOTTED_PATH -- like eval_verb but for an arbitrary verb path.
eval_verb_named() {
    "$CLI" --system-root "$DB" --lock-opened-roots -e "$1()" 2>/dev/null | tail -1 | tr -d '[:space:]'
}

# install_named_verb DOTTED_PATH HANDLER_NAME RETVAL -- generic variant of
# install_verb: installs "on HANDLER() { return (RETVAL) }" at DOTTED_PATH
# via script.newScriptObject, then shuts down (export fires on exit).
install_named_verb() {
    local vpath="$1" hname="$2" retval="$3" proto
    proto=$(cat <<'PROTOEOF'
{"id":1,"op":"script/eval","params":{"expression":"script.newScriptObject(\"on HNAME() {\\r\\treturn (RETVAL)}\", @VPATH)"}}
{"id":2,"op":"shutdown","params":{}}
PROTOEOF
)
    proto="${proto//VPATH/$vpath}"
    proto="${proto//HNAME/$hname}"
    proto="${proto//RETVAL/$retval}"
    printf '%s\n' "$proto" | run_protocol "" >/dev/null
}

# ---------------------------------------------------------------------------
# Test 13: broken .ut import is REJECTED (compile-check before install).
# ---------------------------------------------------------------------------
echo "==> Test 13: broken .ut import rejected, ODB version kept, loud log"
V13_UT="$ROOT_SYNC/scratchpad/utBroken13.ut"
T13_ERR="$STAGE_DIR/t13.stderr"
install_named_verb "scratchpad.utBroken13" "utBroken13" 41

if [ -f "$V13_UT" ]; then
    # Outline-parseable but non-compiling body (unbalanced parens), future
    # mtime so the import gate fires on next materialization.
    printf 'on utBroken13() {\n\tif ((( broken\n\treturn (41)}\n' > "$V13_UT"
    set_mtime_offset "$V13_UT" 3600
    T13_MD5_BEFORE="$(md5_of "$V13_UT")"

    T13_OUT=$(printf '%s\n' \
        '{"id":1,"op":"script/eval","params":{"expression":"scratchpad.utBroken13()"}}' \
        '{"id":2,"op":"shutdown","params":{}}' \
        | run_protocol_err "$T13_ERR")
    T13_VAL="$(probe_value "$T13_OUT" 1)"

    if [ "$T13_VAL" = "41" ]; then
        pass "broken .ut rejected: verb still returns 41 from the ODB"
    else
        fail "broken .ut imported: expected 41, got '${T13_VAL:-<none>}'"
    fi
    # The diagnostic must name the affected script (a generic boot-time
    # "reject" line from the scan pre-flight must not satisfy this).
    if grep -i "reject" "$T13_ERR" | grep -q "utBroken13"; then
        pass "rejection diagnostic logged (names utBroken13)"
    else
        fail "no rejection diagnostic naming utBroken13 in stderr log ($T13_ERR)"
    fi
    # The rejected .ut must be left in place (neither side clobbered).
    if [ "$(md5_of "$V13_UT")" = "$T13_MD5_BEFORE" ]; then
        pass "rejected .ut left untouched on disk"
    else
        fail "rejected .ut was rewritten on disk"
    fi
else
    fail "skipping test 13: exported .ut absent at $V13_UT"
fi

# ---------------------------------------------------------------------------
# Test 14: both-sides-changed conflict -- refuse to clobber either side.
#
# Sequence: install (61) + shutdown = the recorded sync point. In ONE
# session: edit the ODB (62), then out-of-band rewrite the .ut (63) with a
# future mtime, sleep past the hot-path throttle, and call the verb. The
# hot-path import check sees the .ut as newer; with both sides changed since
# the sync point it must REFUSE (verb stays 62). At shutdown the export must
# also refuse to overwrite the conflicted .ut (file stays 63).
# ---------------------------------------------------------------------------
echo "==> Test 14: both-sides-changed conflict is detected, nothing clobbered"
V14_UT="$ROOT_SYNC/scratchpad/utConflict14.ut"
T14_ERR="$STAGE_DIR/t14.stderr"
install_named_verb "scratchpad.utConflict14" "utConflict14" 61

if [ -f "$V14_UT" ]; then
    T14_STAGED="$STAGE_DIR/t14_staged.ut"
    printf 'on utConflict14() {\n\treturn (63)}\n' > "$T14_STAGED"

    # Future timestamp (touch -t format) so the conflicted .ut wins the
    # mtime gate; BSD and GNU date forms.
    if date -v +1H +%Y%m%d%H%M.%S >/dev/null 2>&1; then
        T14_STAMP="$(date -v +1H +%Y%m%d%H%M.%S)"
    else
        T14_STAMP="$(date -d '+1 hour' +%Y%m%d%H%M.%S)"
    fi

    T14_REQ="$STAGE_DIR/t14_req.jsonl"
    cat > "$T14_REQ" <<'T14EOF'
{"id":1,"op":"script/eval","params":{"expression":"script.newScriptObject(\"on utConflict14() {\\r\\treturn (62)}\", @scratchpad.utConflict14)"}}
{"id":2,"op":"script/eval","params":{"expression":"sys.unixShellCommand(\"CPCMD\")"}}
{"id":3,"op":"script/eval","params":{"expression":"scratchpad.utConflict14()"}}
{"id":4,"op":"shutdown","params":{}}
T14EOF
    # sleep 2 outlasts the hot-path import-check throttle (1s), so the
    # id=3 call re-checks the filesystem and sees the conflicted .ut.
    T14_CMD="cp $T14_STAGED $V14_UT && touch -t $T14_STAMP $V14_UT && sleep 2"
    T14_PROTO="$(cat "$T14_REQ")"
    T14_PROTO="${T14_PROTO//CPCMD/$T14_CMD}"
    printf '%s\n' "$T14_PROTO" > "$T14_REQ"

    T14_OUT=$(run_protocol_err "$T14_ERR" < "$T14_REQ")
    T14_VAL="$(probe_value "$T14_OUT" 3)"

    if [ "$T14_VAL" = "62" ]; then
        pass "conflicted .ut did not clobber in-session ODB edit (verb = 62)"
    else
        fail "conflict not detected: expected 62, got '${T14_VAL:-<none>}' (silent clobber)"
    fi

    RB14="$(eval_verb_named scratchpad.utConflict14)"
    if [ "$RB14" = "62" ]; then
        pass "ODB kept its edit across shutdown (62)"
    else
        fail "ODB edit lost after shutdown: expected 62, got '$RB14'"
    fi

    if grep -q "63" "$V14_UT"; then
        pass "conflicted .ut left untouched on disk (still 63)"
    else
        fail "conflicted .ut was overwritten by export"
    fi

    if grep -i "conflict" "$T14_ERR" | grep -q "utConflict14"; then
        pass "conflict diagnostic logged (names utConflict14)"
    else
        fail "no conflict diagnostic naming utConflict14 in stderr log ($T14_ERR)"
    fi
else
    fail "skipping test 14: exported .ut absent at $V14_UT"
fi

# ---------------------------------------------------------------------------
# Test 15: broken NEW .ut (no ODB node) must NOT be created by the scan.
# Probe the GRANDCHILD of builtins (honest signal -- see Test 6 note).
# ---------------------------------------------------------------------------
echo "==> Test 15: discovery scan rejects a broken new .ut"
T15_DIR="$ROOT_SYNC/system/verbs/builtins/zzbroken15"
T15_FILE="$T15_DIR/bad.ut"
T15_ERR="$STAGE_DIR/t15.stderr"
mkdir -p "$T15_DIR"
printf 'on bad() {\n\tif ((( nope\n' > "$T15_FILE"

T15_OUT=$(printf '%s\n' \
    '{"id":1,"op":"script/eval","params":{"expression":"defined(@system.verbs.builtins.zzbroken15.bad)"}}' \
    '{"id":2,"op":"shutdown","params":{}}' \
    | run_protocol_err "$T15_ERR")
T15_DEFINED="$(probe_value "$T15_OUT" 1)"

if [ "$T15_DEFINED" != "true" ]; then
    pass "broken new .ut not created (defined -> '${T15_DEFINED:-false}')"
else
    fail "broken new .ut WAS created as a non-compiling script"
fi
if grep -iE "reject|compile" "$T15_ERR" | grep -q "zzbroken15"; then
    pass "scan rejection diagnostic logged (names zzbroken15)"
else
    fail "no scan rejection diagnostic naming zzbroken15 in stderr log ($T15_ERR)"
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
