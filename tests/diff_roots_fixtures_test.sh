#!/bin/bash
#
# Behavioral fixture tests for --diff-roots (root-build Step 1).
#
# WHY THIS FILE EXISTS
#
# The gate found two P0s in the compare core, and BOTH survived the original
# acceptance evidence, because that evidence only ever proved the walker
# AGREES -- never that it DETECTS. Self-diff compares a file with itself, so no
# vacuous compare and no wrong skip window can surface. "43 menubars compared
# equal" measured agreement.
#
# So every compare path here is proven by a fixture that DIFFERS in exactly one
# known way, and the walker must find exactly that. Three of the fix commits are
# otherwise revert-invisible: revert storedaddressof, collectnodes, or the
# reportsubtree descent and a self-diff-only suite stays green.
#
# NO BINARY FIXTURES ARE COMMITTED. Every root is generated at run time from
# databases/StartupTasks.root (29 KB, v7, already in the repo) by copying it and
# flipping known bytes. Deterministic: same inputs, same offsets, same results.
#
# The byte offsets below are derived, not guessed. The live packed-outline
# header for StartupTasksData...Frontier.cliVersion starts at file offset 0x66;
# that was established by mutating its script text and reading back the walker's
# own reported first-diff-offset. Header field offsets come from
# typortablediskheader (oppack_v7.c): ctsaves 34-37, fltextmode 38-39,
# outlinesignature 40-43.
#
# License
# -------
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frontier contributors.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
SOURCE_DB="$PROJECT_ROOT/databases/StartupTasks.root"

if [ ! -x "$CLI" ]; then
    echo "Error: frontier-cli not found at $CLI" >&2
    exit 1
fi
if [ ! -f "$SOURCE_DB" ]; then
    echo "Error: database not found at $SOURCE_DB" >&2
    exit 1
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

PASSED=0
FAILED=0

WORKDIR=$(mktemp -d "${TMPDIR:-/tmp}/diff_roots_fixtures.XXXXXX")
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

BASE="$WORKDIR/base.root"
cp "$SOURCE_DB" "$BASE" || { echo "Error: could not stage the base root" >&2; exit 1; }

# Live packed-outline header for the cliVersion script inside the base root.
HDR=$((0x66))

pass() {
    echo -e "  ${GREEN}PASS${NC}: $1"
    PASSED=$((PASSED + 1))
}

fail() {
    echo -e "  ${RED}FAIL${NC}: $1"
    echo "    $2"
    FAILED=$((FAILED + 1))
}

# flipbyte <destfile> <offset> -- copy the base root and XOR one byte.
flipbyte() {
    local dest="$1" off="$2"
    cp "$BASE" "$dest"
    python3 - "$dest" "$off" <<'PYEOF'
import sys
path, off = sys.argv[1], int(sys.argv[2])
d = bytearray(open(path, 'rb').read())
d[off] ^= 0x01
open(path, 'wb').write(d)
PYEOF
}

# findings <a> <b> -- number of findings reported.
findings() {
    "$CLI" --diff-roots "$1" --against "$2" 2>/dev/null \
        | grep -cE '^(payload|only-in-a|only-in-b|type|unsupported-type|unreadable)'
}

# exitcode <a> <b>
exitcode() {
    "$CLI" --diff-roots "$1" --against "$2" >/dev/null 2>&1
    echo $?
}

echo "diff_roots fixture tests"
echo "========================"

# ---- identical -> empty ------------------------------------------------------

cp "$BASE" "$WORKDIR/same.root"
n=$(findings "$BASE" "$WORKDIR/same.root")
e=$(exitcode "$BASE" "$WORKDIR/same.root")
if [ "$n" -eq 0 ] && [ "$e" -eq 0 ]; then
    pass "identical roots -> 0 findings, exit 0"
else
    fail "identical roots -> 0 findings, exit 0" "got $n findings, exit $e"
fi

# ---- script payload difference -> exactly one -------------------------------
# Mutates one byte of script TEXT (inside the comment on the first line of
# cliVersion), which is content by any definition.

cp "$BASE" "$WORKDIR/script.root"
python3 - "$WORKDIR/script.root" <<'PYEOF'
import sys
path = sys.argv[1]
d = bytearray(open(path, 'rb').read())
i = d.find(b'return the version string')
assert i > 0, 'script-text anchor not found in StartupTasks.root'
d[i] = ord('R')
open(path, 'wb').write(d)
PYEOF
n=$(findings "$BASE" "$WORKDIR/script.root")
e=$(exitcode "$BASE" "$WORKDIR/script.root")
if [ "$n" -eq 1 ] && [ "$e" -eq 1 ]; then
    pass "one script-text byte changed -> exactly 1 finding, exit 1"
else
    fail "one script-text byte changed -> exactly 1 finding, exit 1" "got $n findings, exit $e"
fi

# ---- skip regions: volatile save metadata is IGNORED -------------------------
# ctsaves (header 34-37) is bumped by oppack itself. If it were compared, every
# value in a re-saved root would report as different and the acceptance gate
# would be useless.

flipbyte "$WORKDIR/ctsaves.root" $((HDR + 37))
n=$(findings "$BASE" "$WORKDIR/ctsaves.root")
if [ "$n" -eq 0 ]; then
    pass "ctsaves byte changed (skip region) -> 0 findings"
else
    fail "ctsaves byte changed (skip region) -> 0 findings" "got $n findings"
fi

# ---- fltextmode is CONTENT and must NOT be skipped ---------------------------
# This is the exact byte the original off-by-six skip table swallowed. Offset 39
# is the low byte of the big-endian int16; the high byte at 38 is always zero and
# is discarded by the pack round-trip, so 39 is the one that carries the value.

flipbyte "$WORKDIR/fltextmode.root" $((HDR + 39))
n=$(findings "$BASE" "$WORKDIR/fltextmode.root")
if [ "$n" -eq 1 ]; then
    pass "fltextmode changed -> 1 finding (old +6 table swallowed this)"
else
    fail "fltextmode changed -> 1 finding" "got $n findings"
fi

# ---- outlinesignature is CONTENT and must NOT be skipped ---------------------
# It selects the OSA server that runs the script (scripts.c:943), drives tree
# building (:1302), and gates UserTalk text handling (:3049,
# outlinesignature == typeLAND). A difference here means a different LANGUAGE.

flipbyte "$WORKDIR/signature.root" $((HDR + 43))
n=$(findings "$BASE" "$WORKDIR/signature.root")
if [ "$n" -eq 1 ]; then
    pass "outlinesignature changed -> 1 finding (semantic, not metadata)"
else
    fail "outlinesignature changed -> 1 finding" "got $n findings"
fi

# ---- skip-window boundary ----------------------------------------------------
# Pins both edges at once: inside the window is silent, one byte past it is
# reported. An off-by-N in either direction breaks exactly one of these.

flipbyte "$WORKDIR/edge_in.root" $((HDR + 34))
flipbyte "$WORKDIR/edge_out.root" $((HDR + 39))
nin=$(findings "$BASE" "$WORKDIR/edge_in.root")
nout=$(findings "$BASE" "$WORKDIR/edge_out.root")
if [ "$nin" -eq 0 ] && [ "$nout" -eq 1 ]; then
    pass "skip-window boundary: 34 ignored, 39 reported"
else
    fail "skip-window boundary: 34 ignored, 39 reported" "got in=$nin out=$nout"
fi

# ---- operational failure is distinct from findings --------------------------
# Exit 2 (cannot run) must never look like exit 0 (clean) or exit 1 (differences),
# or a broken CI check reads as a passing one.

e=$(exitcode "$WORKDIR/does-not-exist.root" "$BASE")
if [ "$e" -eq 2 ]; then
    pass "missing input -> exit 2, distinct from 0 and 1"
else
    fail "missing input -> exit 2" "got exit $e"
fi

# ---- ut-sync refusal ---------------------------------------------------------
# ut-sync writes to the roots this mode opens read-only. Same command, same
# roots, env var the only variable.

FRONTIER_UT_SYNC_DIR="$WORKDIR/utsync" "$CLI" --diff-roots "$BASE" --against "$WORKDIR/same.root" >/dev/null 2>&1
e=$?
if [ "$e" -ne 0 ]; then
    pass "ut-sync active -> refused (exit $e)"
else
    fail "ut-sync active -> refused" "got exit 0; the guard did not fire"
fi

# ---- mbar detection (the P0-1 fixture) ---------------------------------------
# StartupTasks.root has no menubar, so this one needs Virgin.root. It is the
# fixture that proves the vacuous-mbar bug stays fixed: before the fix, EVERY
# menubar compared equal regardless of content, because oldaddress is nil for
# never-materialized externals and the stored address lives in variabledata.
#
# Uses suites.applescripts.oldstuff.menu, whose stored block starts at 0x14a97
# in the committed Virgin.root. If Virgin.root changes, this offset must be
# re-derived (run the walker against a byte-flipped copy and read the path it
# names); a stale offset shows up as "did not detect", never as a false pass.

VIRGIN="$PROJECT_ROOT/databases/Virgin.root"
MBAR_BLOCK=$((0x14a97))
MBAR_INTERIOR=$((MBAR_BLOCK + 64))

if [ -f "$VIRGIN" ]; then
    VA="$WORKDIR/virgin-a.root"
    VB="$WORKDIR/virgin-b.root"
    cp "$VIRGIN" "$VA"
    cp "$VIRGIN" "$VB"

    python3 - "$VB" "$MBAR_INTERIOR" <<'PYEOF'
import sys
path, off = sys.argv[1], int(sys.argv[2])
d = bytearray(open(path, 'rb').read())
d[off] ^= 0x01
open(path, 'wb').write(d)
PYEOF

    n=$("$CLI" --diff-roots "$VA" --against "$VB" 2>/dev/null | grep -c 'type=mbar')
    if [ "$n" -eq 1 ]; then
        pass "menubar content changed -> 1 mbar finding (vacuous-compare regression)"
    else
        fail "menubar content changed -> 1 mbar finding" \
             "got $n mbar findings; if Virgin.root moved, re-derive MBAR_BLOCK"
    fi

    # Same roots unmutated must still be clean, so the above cannot pass by
    # reporting menubars indiscriminately.
    cp "$VIRGIN" "$VB"
    n=$("$CLI" --diff-roots "$VA" --against "$VB" 2>/dev/null | grep -c 'type=mbar')
    if [ "$n" -eq 0 ]; then
        pass "unmutated menubars -> 0 mbar findings"
    else
        fail "unmutated menubars -> 0 mbar findings" "got $n"
    fi
else
    echo "  SKIP: Virgin.root not present; mbar detection fixture not run"
fi

# ---- divergent insertion order and subtree enumeration (P1-3 / P2) ----------
# These need a root that can be opened READ-WRITE as a system root so values can
# be scripted in; StartupTasks.root cannot. Virgin.root can, so they ride along
# with the Virgin block above.
#
# Order fixture: the SAME three values inserted in two different orders. Before
# the name-keyed fix, this produced four findings for three values -- each
# reported as both only-in-A and only-in-B -- and their payloads were never
# compared at all. The masked comparison was the serious half.
#
# Subtree fixture: a nested table present on one side only. Before the fix,
# reportsubtree stopped at any non-resident subtable, so the table was named but
# its contents were not.

if [ -f "$VIRGIN" ]; then
    OA="$WORKDIR/order-a.root"
    OB="$WORKDIR/order-b.root"
    cp "$VIRGIN" "$OA"
    cp "$VIRGIN" "$OB"

    cat > "$WORKDIR/order-a.ut" <<'UTEOF'
new(tabletype, @workspace.orderProbe);
workspace.orderProbe.zebra = "z";
workspace.orderProbe.alpha = "a";
workspace.orderProbe.mango = "m";
"ok"
UTEOF
    cat > "$WORKDIR/order-b.ut" <<'UTEOF'
new(tabletype, @workspace.orderProbe);
workspace.orderProbe.alpha = "a";
workspace.orderProbe.mango = "m";
workspace.orderProbe.zebra = "z";
"ok"
UTEOF

    "$CLI" --system-root "$OA" --skip-startup -b "$WORKDIR/order-a.ut" >/dev/null 2>&1
    "$CLI" --system-root "$OB" --skip-startup -b "$WORKDIR/order-b.ut" >/dev/null 2>&1

    n=$("$CLI" --diff-roots "$OA" --against "$OB" 2>/dev/null | grep -ci 'orderProbe')
    if [ "$n" -eq 0 ]; then
        pass "same values, different insertion order -> 0 findings"
    else
        fail "same values, different insertion order -> 0 findings" \
             "got $n (insertion-order matching regressed; payloads go uncompared)"
    fi

    SA="$WORKDIR/subtree-a.root"
    SB="$WORKDIR/subtree-b.root"
    cp "$VIRGIN" "$SA"
    cp "$VIRGIN" "$SB"

    cat > "$WORKDIR/subtree.ut" <<'UTEOF'
new(tabletype, @workspace.subtreeProbe);
new(tabletype, @workspace.subtreeProbe.inner);
workspace.subtreeProbe.inner.leaf1 = "one";
workspace.subtreeProbe.inner.leaf2 = "two";
workspace.subtreeProbe.topLevel = "top";
"ok"
UTEOF
    "$CLI" --system-root "$SB" --skip-startup -b "$WORKDIR/subtree.ut" >/dev/null 2>&1

    n=$("$CLI" --diff-roots "$SA" --against "$SB" 2>/dev/null | grep -ci 'subtreeProbe')
    if [ "$n" -eq 5 ]; then
        pass "one-sided subtree -> all 5 nested values enumerated"
    else
        fail "one-sided subtree -> all 5 nested values enumerated" \
             "got $n (expected table, inner table, 2 leaves, 1 scalar)"
    fi
fi

# ---- read-only guarantee -----------------------------------------------------
# The whole tool is worthless if it can perturb its inputs.

before=$(md5 -q "$BASE" 2>/dev/null || md5sum "$BASE" | cut -d' ' -f1)
"$CLI" --diff-roots "$BASE" "$WORKDIR/script.root" >/dev/null 2>&1
"$CLI" --diff-roots "$BASE" --against "$WORKDIR/script.root" >/dev/null 2>&1
after=$(md5 -q "$BASE" 2>/dev/null || md5sum "$BASE" | cut -d' ' -f1)
if [ "$before" = "$after" ]; then
    pass "inputs byte-identical after comparison (read-only)"
else
    fail "inputs byte-identical after comparison" "md5 changed: $before -> $after"
fi

echo
echo "  Passed: $PASSED"
echo "  Failed: $FAILED"

[ "$FAILED" -eq 0 ] || exit 1
exit 0
