#!/bin/bash
#
# Integration test for tools/verify_virgin_root_sync.sh (issue #675).
#
# Verifies both:
#   1. Positive case: a known-in-sync .ut file exits with status 0.
#   2. Negative case: a deliberately-drifted synthetic .ut sandbox exits
#      with status 1 and names the drifted path.
#
# Strategy: pick a small known-good .ut from the corpus (statusStream.ut),
# confirm it passes. For the negative case, copy that same file to a
# tmpdir, tamper with it, and verify the verifier flags it via the
# --corpus-root flag (which retargets the FS-prefix-strip so the kernel
# still looks up the original ODB path).

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
ROOT_DB="$PROJECT_ROOT/databases/Virgin.root"
VERIFIER="$PROJECT_ROOT/tools/verify_virgin_root_sync.sh"

if [ ! -x "$CLI" ]; then
    echo "Error: frontier-cli not found at $CLI" >&2
    echo "Build it first: make -C frontier-cli" >&2
    exit 1
fi
if [ ! -f "$ROOT_DB" ]; then
    echo "Error: Virgin.root not found at $ROOT_DB" >&2
    exit 1
fi
if [ ! -x "$VERIFIER" ]; then
    echo "Error: verifier not found or not executable: $VERIFIER" >&2
    exit 1
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

PASSED=0
FAILED=0

# Pick a small known-in-sync file. statusStream.ut is a one-line script
# touched recently in PR #670 work and confirmed in-sync during verifier
# development.
KNOWN_GOOD_UT="$PROJECT_ROOT/usertalk_scripts/Frontier.root/system/verbs/builtins/tcp/statusStream.ut"

if [ ! -f "$KNOWN_GOOD_UT" ]; then
    echo "Error: positive-test fixture missing: $KNOWN_GOOD_UT" >&2
    exit 1
fi

# ----------------------------------------------------------------
# Positive: known-good .ut passes
# ----------------------------------------------------------------

echo "Test 1: positive — known-good .ut file matches Virgin.root"
if "$VERIFIER" --paths "$KNOWN_GOOD_UT" --quiet --no-diff >/dev/null 2>&1; then
    echo -e "  ${GREEN}PASS${NC}"
    PASSED=$((PASSED + 1))
else
    echo -e "  ${RED}FAIL${NC}: verifier reported drift on known-good file"
    echo "  This means either statusStream.ut has been edited without re-installing"
    echo "  into Virgin.root, OR the verifier normalization regressed."
    echo "  Run for diagnostic: $VERIFIER --paths $KNOWN_GOOD_UT"
    FAILED=$((FAILED + 1))
fi

# ----------------------------------------------------------------
# Negative: synthetic drift in a sandbox is detected
# ----------------------------------------------------------------

echo "Test 2: negative — synthetic drift is detected"
TMPDIR_BASE=$(mktemp -d -t verify_odb_sync_test.XXXXXX)
# Build a mock corpus mirroring just the file we'll tamper with.
SANDBOX_ROOT="$TMPDIR_BASE/Frontier.root"
SANDBOX_FILE="$SANDBOX_ROOT/system/verbs/builtins/tcp/statusStream.ut"
mkdir -p "$(dirname "$SANDBOX_FILE")"
# Copy the known-good content, then tamper.
cp "$KNOWN_GOOD_UT" "$SANDBOX_FILE"
echo "INJECTED_DRIFT_MARKER" >> "$SANDBOX_FILE"

# Run verifier pointing at the sandbox. It will compute the ODB path from
# the sandbox file location relative to --corpus-root (yielding
# system.verbs.builtins.tcp.statusStream), then read that real path from
# the real Virgin.root and compare against the tampered sandbox content.
if "$VERIFIER" --corpus-root "$SANDBOX_ROOT" --paths "$SANDBOX_FILE" --quiet --no-diff >/dev/null 2>&1; then
    echo -e "  ${RED}FAIL${NC}: verifier did not detect the synthetic drift"
    echo "  The sandbox copy of statusStream.ut had INJECTED_DRIFT_MARKER appended;"
    echo "  the verifier should have flagged content drift."
    FAILED=$((FAILED + 1))
else
    # Confirm the exit code was specifically 1 (drift), not 2 (infra error).
    "$VERIFIER" --corpus-root "$SANDBOX_ROOT" --paths "$SANDBOX_FILE" --quiet --no-diff >/dev/null 2>&1
    rc=$?
    if [ "$rc" -eq 1 ]; then
        echo -e "  ${GREEN}PASS${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}FAIL${NC}: verifier exited $rc (expected 1 for drift)"
        FAILED=$((FAILED + 1))
    fi
fi

rm -rf "$TMPDIR_BASE"

# ----------------------------------------------------------------
# Summary
# ----------------------------------------------------------------

echo ""
echo "============================================================"
echo "verify_odb_sync_test.sh: $PASSED passed, $FAILED failed"
echo "============================================================"

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi
exit 0
