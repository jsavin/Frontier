#!/bin/bash
#
# Test for tools/strings_compiler multi-input support (issue #681).
#
# Before the fix: strings_compiler accepted only ONE positional input
# file. A second positional argument printed "unexpected argument" and
# exited 1. Both Makefiles worked around this by piping concatenated
# YAML to stdin via `cat <inputs> | compiler ... -`, which loses source
# filenames on parse errors.
#
# After the fix: argv-loop multi-input. The compiler accepts multiple
# positional inputs, processes them in order, and reports parse errors
# with the real filename.
#
# Strategy: build the compiler, invoke it with both real corpus inputs
# (idsystemtablescripts.yaml + langerrorlist.yaml) as separate args,
# assert exit 0 and that the produced .c and .h outputs contain symbols
# from BOTH inputs. Then re-run with a deliberately-broken second file
# and confirm the error message names the right file.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
COMPILER="$PROJECT_ROOT/tools/strings_compiler/strings_compiler"
INPUTS_DIR="$PROJECT_ROOT/resources/strings"

if [ ! -x "$COMPILER" ]; then
    echo "Error: strings_compiler not found or not executable: $COMPILER" >&2
    echo "Build it first: make -C tools/strings_compiler" >&2
    exit 1
fi

# The two real inputs in the corpus today; the issue exists precisely
# because there are >=2 inputs. If the corpus drops back to a single
# YAML in the future, this test becomes trivial and should still pass.
INPUT_A="$INPUTS_DIR/idsystemtablescripts.yaml"
INPUT_B="$INPUTS_DIR/langerrorlist.yaml"
for f in "$INPUT_A" "$INPUT_B"; do
    if [ ! -f "$f" ]; then
        echo "Error: fixture missing: $f" >&2
        exit 1
    fi
done

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

PASSED=0
FAILED=0

TMPDIR_BASE=$(mktemp -d -t strings_compiler_multi_input_test.XXXXXX)
trap 'rm -rf "$TMPDIR_BASE"' EXIT

# ----------------------------------------------------------------
# Test 1: two positional inputs are accepted; both contribute symbols
# ----------------------------------------------------------------

echo "Test 1: positive - two positional inputs both contribute symbols"

C_OUT="$TMPDIR_BASE/test1.c"
H_OUT="$TMPDIR_BASE/test1.h"
MANIFEST="$TMPDIR_BASE/test1.manifest"

if "$COMPILER" --c-output "$C_OUT" --h-output "$H_OUT" --manifest "$MANIFEST" "$INPUT_A" "$INPUT_B" 2>"$TMPDIR_BASE/test1.err"; then
    # Confirm both sources contributed to the manifest. Each input
    # declares a top-level table name; the manifest enumerates them.
    # idsystemtablescripts.yaml contributes idsystemtablescripts (plus
    # several embedded sub-tables); langerrorlist.yaml contributes
    # langerrorlist. Both names must appear.
    if grep -q "idsystemtablescripts" "$MANIFEST" && grep -q "langerrorlist" "$MANIFEST"; then
        echo -e "  ${GREEN}PASS${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}FAIL${NC}: output produced but manifest missing expected symbols"
        echo "  Manifest contents:"
        sed 's/^/    /' "$MANIFEST"
        FAILED=$((FAILED + 1))
    fi
else
    echo -e "  ${RED}FAIL${NC}: compiler rejected two positional inputs"
    echo "  stderr:"
    sed 's/^/    /' "$TMPDIR_BASE/test1.err"
    FAILED=$((FAILED + 1))
fi

# ----------------------------------------------------------------
# Test 2: parse error names the source file (not "stdin")
# ----------------------------------------------------------------
#
# Create a sandboxed bad-syntax YAML and invoke with the good input first
# then the bad one. The error message must name the bad file, not "stdin"
# (which was the pre-fix behavior via the cat-pipe workaround).

echo "Test 2: parse error names the source file"

BAD_YAML="$TMPDIR_BASE/bad.yaml"
printf 'table: this_is_not_valid_strings_yaml\n: : :\n' > "$BAD_YAML"

C_OUT2="$TMPDIR_BASE/test2.c"
H_OUT2="$TMPDIR_BASE/test2.h"
MANIFEST2="$TMPDIR_BASE/test2.manifest"

if "$COMPILER" --c-output "$C_OUT2" --h-output "$H_OUT2" --manifest "$MANIFEST2" "$INPUT_A" "$BAD_YAML" >/dev/null 2>"$TMPDIR_BASE/test2.err"; then
    echo -e "  ${RED}FAIL${NC}: compiler should have rejected the bad YAML"
    FAILED=$((FAILED + 1))
else
    # Expect the error to mention the bad file's basename, not "stdin".
    if grep -q "bad.yaml" "$TMPDIR_BASE/test2.err"; then
        echo -e "  ${GREEN}PASS${NC}"
        PASSED=$((PASSED + 1))
    elif grep -q "stdin" "$TMPDIR_BASE/test2.err"; then
        echo -e "  ${RED}FAIL${NC}: error reported 'stdin' instead of source filename"
        echo "  stderr:"
        sed 's/^/    /' "$TMPDIR_BASE/test2.err"
        FAILED=$((FAILED + 1))
    else
        echo -e "  ${RED}FAIL${NC}: error message did not name a source file"
        echo "  stderr:"
        sed 's/^/    /' "$TMPDIR_BASE/test2.err"
        FAILED=$((FAILED + 1))
    fi
fi

# ----------------------------------------------------------------
# Summary
# ----------------------------------------------------------------

echo ""
echo "============================================================"
echo "strings_compiler_multi_input_test.sh: $PASSED passed, $FAILED failed"
echo "============================================================"

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi
exit 0
