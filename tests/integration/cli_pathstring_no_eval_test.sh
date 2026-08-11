#!/bin/bash
#
# Security regression test for issue #859: initializing Frontier.pathString at
# load must NOT invoke the UserTalk interpreter.
#
# BACKGROUND
# The first fix for #859 set pathString by evaluating the same one-liner
# startupScript.ut:60 uses:
#
#     Frontier.pathString = file.folderFromPath (frontier.getFilePath ())
#
# Keeping the path out of the source text closed string-literal injection, but
# not VERB RESOLUTION. The names file.folderFromPath and frontier.getFilePath
# resolve through the loaded root's own tables BEFORE the kernel efptable, and
# verb shadowing is a supported feature (docs/VERB_RESOLUTION_ARCHITECTURE.md).
# So a hostile root that stores a script at either path got arbitrary UserTalk
# executed at load -- including under --skip-startup --lock-opened-roots, an
# inspection posture that otherwise runs no root code at all.
#
# Measured against the eval-based build, this exact fixture yielded:
#     pathString=[/tmp/pwned/]  shadowRan=true
#
# The fix computes the folder in C (portable_folderfrompath) and assigns it via
# the hash-table APIs, so no interpreter participates in the load path.
#
# WHAT THIS TEST DOES
# Builds a throwaway root whose file.folderFromPath is shadowed by a script that
# records a side effect and returns an attacker-chosen path, then loads it in the
# inspection posture and asserts:
#   1. the shadowing script did NOT run   (no arbitrary code execution at load)
#   2. pathString is the REAL root folder, not the attacker's value
#
# All work happens in tests/tmp/integration; the canonical databases/ tree is
# never touched (the fixture is a copy).

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
SOURCE_ROOT="$PROJECT_ROOT/databases/Virgin.root"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo "================================================"
echo "  pathString load-time init runs no root code (issue #859)"
echo "================================================"
echo

if [ ! -x "$CLI" ]; then
    echo "SKIP: frontier-cli not built at $CLI"
    exit 0
fi
if [ ! -f "$SOURCE_ROOT" ]; then
    echo "SKIP: source root not found at $SOURCE_ROOT"
    exit 0
fi

WORK_DIR="$(mktemp -d "$PROJECT_ROOT/tests/tmp/integration/pathstring_no_eval.XXXXXX")" || {
    echo "SKIP: could not create work dir"
    exit 0
}
trap 'rm -rf "$WORK_DIR"' EXIT

HOSTILE_ROOT="$WORK_DIR/hostile.root"
cp "$SOURCE_ROOT" "$HOSTILE_ROOT"

FAILURES=0
pass() { printf "${GREEN}PASS${NC}: %s\n" "$1"; }
fail() { printf "${RED}FAIL${NC}: %s\n" "$1"; FAILURES=$((FAILURES + 1)); }

# Install the shadowing verb. Writing it is expected to succeed -- shadowing is a
# supported feature; the point of the test is that it must not be REACHED at load.
INSTALL_SCRIPT='local (src);
src = "on folderFromPath (p) {" + "\r" + "\tuser.SHADOW_EXECUTED = 1;" + "\r" + "\treturn (\"/tmp/pwned/\")}";
script.newScriptObject (src, @file.folderFromPath);
return ("compiles=" + defined (file.folderFromPath))'

INSTALL_OUT="$("$CLI" --skip-startup --system-root "$HOSTILE_ROOT" -e "$INSTALL_SCRIPT" 2>&1 | tail -1)"

if [[ "$INSTALL_OUT" != *"compiles=true"* ]]; then
    echo "SKIP: could not install shadowing fixture (got: $INSTALL_OUT)"
    echo "      Without a compiled shadow the assertions below would pass vacuously."
    exit 0
fi
echo "Fixture installed: file.folderFromPath is shadowed in $HOSTILE_ROOT"
echo

# Load the hostile root in the inspection posture and report both facts at once.
PROBE='return "shadowRan=" + defined (user.SHADOW_EXECUTED) + " path=" + Frontier.pathString'
PROBE_OUT="$("$CLI" --skip-startup --lock-opened-roots --system-root "$HOSTILE_ROOT" -e "$PROBE" 2>&1 | tail -1)"

echo "Probe output: $PROBE_OUT"
echo

# 1. No arbitrary code execution at load.
if [[ "$PROBE_OUT" == *"shadowRan=false"* ]]; then
    pass "shadowing script did not execute during load"
elif [[ "$PROBE_OUT" == *"shadowRan=true"* ]]; then
    fail "shadowing script EXECUTED at load -- arbitrary code execution from a hostile root"
else
    fail "could not determine whether the shadow ran (output: $PROBE_OUT)"
fi

# 2. pathString reflects the real root folder, not the attacker's return value.
if [[ "$PROBE_OUT" == *"/tmp/pwned/"* ]]; then
    fail "pathString was set from the shadowed verb's return value"
elif [[ "$PROBE_OUT" == *"path=$WORK_DIR/"* ]]; then
    pass "pathString derived from the real root location"
else
    fail "pathString is not the hostile root's folder (expected $WORK_DIR/, output: $PROBE_OUT)"
fi

echo
echo "================================================"
if [ $FAILURES -eq 0 ]; then
    echo "  Results: all checks passed, 0 failed"
    echo "================================================"
    exit 0
fi
echo "  Results: $FAILURES check(s) failed"
echo "================================================"
exit 1
