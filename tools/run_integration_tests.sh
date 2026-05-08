#!/bin/bash
#
# Frontier Integration Test Runner Wrapper
# Executes Python-based integration tests against frontier-cli
#

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RUNNER="$PROJECT_ROOT/tests/integration/runner.py"
CLI_PATH="$PROJECT_ROOT/frontier-cli/frontier-cli"
# Virgin.root is the source of truth for system DB content. Stage it (and its
# sibling guest DBs) into a disposable directory so test mutations never touch
# source files.
SOURCE_DB_DIR="$PROJECT_ROOT/databases"
SOURCE_ROOT="$SOURCE_DB_DIR/Virgin.root"
STAGE_DIR="$PROJECT_ROOT/tests/tmp/results/db"
SYSTEM_ROOT="$STAGE_DIR/Frontier.root"
TEST_CASES_DIR="$PROJECT_ROOT/tests/integration/test_cases"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=============================================="
echo "Frontier Integration Tests"
echo "=============================================="
echo

# Check if runner exists
if [ ! -f "$RUNNER" ]; then
    echo -e "${RED}Error: Test runner not found: $RUNNER${NC}"
    exit 1
fi

# Check if CLI binary exists
if [ ! -f "$CLI_PATH" ]; then
    echo -e "${RED}Error: frontier-cli binary not found: $CLI_PATH${NC}"
    echo "Please build the CLI first: cd frontier-cli && make"
    exit 1
fi

# Warn if the binary is older than any first-party source file that
# compiles into frontier-cli. Stale binaries silently produce "regression"
# failures that don't reflect the current code (e.g., recently merged fixes
# won't be present).
#
# Search Common/source/ and frontier-cli/ — the two trees that feed into
# frontier-cli. tests/ is intentionally excluded: tests/*.c are unit-test
# stubs that compile into the headless test binary, not frontier-cli, so
# touching them shouldn't trigger a "rebuild CLI" warning.
#
# Use absolute paths via $PROJECT_ROOT so the check works regardless of the
# caller's CWD — relative paths would silently match nothing when invoked
# from outside the repo root, defeating the guard.
STALE_SOURCES=$(find \
    "$PROJECT_ROOT/Common/source" \
    "$PROJECT_ROOT/frontier-cli" \
    \( -name '*.c' -o -name '*.h' -o -name '*.m' \) \
    -newer "$CLI_PATH" \
    2>/dev/null | head -5)
if [ -n "$STALE_SOURCES" ]; then
    echo -e "${YELLOW}Warning: source files newer than CLI binary ($CLI_PATH)${NC}"
    echo "  Sample (up to 5):"
    echo "$STALE_SOURCES" | sed 's/^/    /'
    echo "  Tests may fail against stale code. Rebuild with: make -C \"$PROJECT_ROOT/frontier-cli\""
    echo
fi

# Check for Python dependencies
if ! python3 -c "import yaml" 2>/dev/null; then
    echo -e "${YELLOW}Warning: PyYAML not installed${NC}"
    echo "Install with: pip3 install pyyaml"
    echo "Or: pip3 install -r tests/integration/requirements.txt"
    exit 1
fi

# Determine which tests to run
TEST_FILES=()
VERBOSE=""
BATCH_FLAG="--batch"
WORKERS_FLAG="-j 0"
# Track whether the user invoked us without args (run-everything mode) so
# the post-YAML shell test pass can opt out for targeted runs.
ORIG_ARG_COUNT=$#

if [ $# -eq 0 ]; then
    # No arguments - run all tests. Files matching *_network.yaml were
    # historically gated on FRONTIER_RUN_NETWORK_TESTS=1 because they hit
    # external network resources. Those tests have since been migrated to
    # localhost listeners and are self-contained, so they now run by default.
    # (One DNS-resolution test in tcp_verbs_network.yaml depends on the
    # system resolver returning NXDOMAIN for an invalid hostname; environments
    # that hijack NXDOMAIN responses can opt out via
    # FRONTIER_SKIP_NETWORK_TESTS=1.)
    for f in "$TEST_CASES_DIR"/*.yaml; do
        if [ -f "$f" ]; then
            if [[ "$f" == *"_network.yaml" ]] && [ "${FRONTIER_SKIP_NETWORK_TESTS:-0}" = "1" ]; then
                echo -e "${YELLOW}Skipping network tests: $(basename "$f")${NC}"
                echo "  (FRONTIER_SKIP_NETWORK_TESTS=1)"
                continue
            fi
            TEST_FILES+=("$f")
        fi
    done
else
    # Parse arguments
    while [ $# -gt 0 ]; do
        case "$1" in
            -v|--verbose)
                VERBOSE="--verbose"
                shift
                ;;
            --no-batch)
                BATCH_FLAG="--no-batch"
                shift
                ;;
            --batch)
                BATCH_FLAG="--batch"
                shift
                ;;
            -j)
                shift
                WORKERS_FLAG="-j $1"
                shift
                ;;
            -h|--help)
                echo "Usage: $0 [OPTIONS] [TEST_FILES...]"
                echo
                echo "Options:"
                echo "  -v, --verbose      Verbose output"
                echo "  --batch            Use NDJSON protocol for batch execution (default)"
                echo "  --no-batch         Disable NDJSON protocol, use per-process execution"
                echo "  -j N               Number of parallel workers (0=auto, 1=sequential)"
                echo "  -h, --help         Show this help"
                echo
                echo "If no test files are specified, all tests in tests/integration/test_cases/"
                echo "will be run, including *_network.yaml files (which now use localhost"
                echo "listeners and are self-contained)."
                echo
                echo "Environment Variables:"
                echo "  FRONTIER_SKIP_NETWORK_TESTS=1    Opt out of *_network.yaml files (e.g."
                echo "                                   for environments that hijack NXDOMAIN"
                echo "                                   DNS responses)"
                echo
                echo "Examples:"
                echo "  $0                                    # Run all tests (batch + parallel)"
                echo "  $0 --no-batch -j 1                   # Old behavior (per-process, sequential)"
                echo "  FRONTIER_SKIP_NETWORK_TESTS=1 $0      # Skip *_network.yaml files"
                echo "  $0 tests/integration/test_cases/string_verbs.yaml"
                echo "  $0 --verbose tests/integration/test_cases/*.yaml"
                exit 0
                ;;
            *)
                if [ -f "$1" ]; then
                    TEST_FILES+=("$1")
                else
                    echo -e "${RED}Error: Test file not found: $1${NC}"
                    exit 1
                fi
                shift
                ;;
        esac
    done
fi

# Check if we have tests to run
if [ ${#TEST_FILES[@]} -eq 0 ]; then
    echo -e "${YELLOW}No test files found${NC}"
    exit 1
fi

echo "Running ${#TEST_FILES[@]} test file(s)..."
echo

# Stage a fresh copy of Virgin.root for the test run, with sibling guest DBs
# copied next to it. Both the system root and guest DBs are disposable copies
# so tests can freely mutate any database (e.g. via filemenu.save) without
# writing through to the canonical files in databases/.
#
# History: guest DBs were previously symlinked here for speed. That created a
# write-through risk: a test calling filemenu.save() on a guest DB would
# silently mutate the committed source file. cp -R closes that hole at the
# cost of ~30MB and <100ms per run.
if [ ! -f "$SOURCE_ROOT" ]; then
    echo -e "${RED}Error: Source database not found: $SOURCE_ROOT${NC}"
    exit 1
fi
# Defensive: ensure STAGE_DIR is non-empty before rm -rf (it's derived from
# PROJECT_ROOT so this should always hold, but guard anyway).
if [ -z "$STAGE_DIR" ]; then
    echo -e "${RED}Error: STAGE_DIR is empty — refusing to rm -rf${NC}"
    exit 1
fi
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
cp "$SOURCE_ROOT" "$SYSTEM_ROOT"
# Copy sibling files/directories from databases/ so guest-DB-dependent tests
# find them next to the staged Frontier.root. Exclusions:
#   - Virgin.root: source of truth, copied above as Frontier.root
#   - Frontier.root: stale local copy, not used as source
#   - *.v6.root: pre-migration v6 backups (e.g. Frontier.v6.root)
#   - *.v6.root.*: timestamped/numbered v6 backups (e.g. Frontier.v6.root.bak)
for entry in "$SOURCE_DB_DIR"/*; do
    name=$(basename "$entry")
    case "$name" in
        Virgin.root|Frontier.root|*.v6.root|*.v6.root.*)
            continue
            ;;
    esac
    cp -R "$entry" "$STAGE_DIR/$name"
done

# Helper: hash a path (file or directory). For directories, hashes the sorted
# concatenation of file hashes so we get a single deterministic checksum per
# entry. Uses md5 on macOS, md5sum on Linux.
#
# Note: the directory hash combines per-file *content* hashes only — file
# paths are intentionally excluded so the result depends solely on contents.
# That's sufficient for "did any file change?" drift detection. Two trees
# with identical contents but different filenames would hash the same;
# that's acceptable here because we're hashing the same staged tree before
# and after a test run, not comparing different trees. Paths are excluded
# rather than included so cross-platform `find` output ordering can't
# affect the result (LC_ALL=C sort handles ordering of the resulting
# hashes regardless of input path order).
_hash_path() {
    local path="$1"
    if [ -d "$path" ]; then
        # Hash files in sorted order; print only the combined hash
        find "$path" -type f -print0 | LC_ALL=C sort -z | xargs -0 -I{} sh -c '
            md5 -q "$1" 2>/dev/null || md5sum "$1" | cut -d" " -f1
        ' _ {} | (md5 -q 2>/dev/null || md5sum | cut -d" " -f1)
    elif [ -f "$path" ]; then
        md5 -q "$path" 2>/dev/null || md5sum "$path" | cut -d' ' -f1
    fi
}

# Record pre-test checksums for integrity verification: system root + every
# staged guest DB. Catches accidental writes to any staged database.
SYSTEM_ROOT_NAME=$(basename "$SYSTEM_ROOT")
declare -a STAGED_NAMES=()
declare -a CHECKSUMS_BEFORE=()
STAGED_NAMES+=("$SYSTEM_ROOT_NAME")
CHECKSUMS_BEFORE+=("$(_hash_path "$SYSTEM_ROOT")")
for entry in "$STAGE_DIR"/*; do
    name=$(basename "$entry")
    [ "$name" = "$SYSTEM_ROOT_NAME" ] && continue
    STAGED_NAMES+=("$name")
    CHECKSUMS_BEFORE+=("$(_hash_path "$entry")")
done

# Run the tests (using v7 source database directly)
"$RUNNER" $VERBOSE $BATCH_FLAG $WORKERS_FLAG --cli "$CLI_PATH" --system-root "$SYSTEM_ROOT" "${TEST_FILES[@]}"
EXIT_CODE=$?

# Shell-based protocol read-only tests (issue #588). Independent from the
# YAML runner because they exercise CLI argv parsing and on-disk md5
# checks — neither is well-expressed in the YAML/protocol-ops harness.
# Run only when the user did not pass explicit YAML test files (i.e.
# default "all tests" mode), so targeted invocations stay focused.
PROTOCOL_RO_TESTS="$PROJECT_ROOT/tests/integration/protocol_readonly_tests.sh"
if [ "$ORIG_ARG_COUNT" -eq 0 ] && [ -x "$PROTOCOL_RO_TESTS" ]; then
    echo
    "$PROTOCOL_RO_TESTS"
    PROTOCOL_RO_RC=$?
    if [ $PROTOCOL_RO_RC -ne 0 ]; then
        EXIT_CODE=$PROTOCOL_RO_RC
    fi
fi

# Verify integrity of every staged database after tests.
#
# Drift is reported as a warning only and does NOT fail EXIT_CODE. This
# matches prior behavior for Frontier.root, which has a known non-
# deterministic ODB save path (issue #545) — every test run that boots
# the CLI re-saves the system root and trips the warning even when the
# test made zero logical changes. Promoting drift to a hard failure
# would make the suite red on every run until #545 is resolved.
# Guest-DB drift will surface in this same warning channel; the operator
# is expected to investigate any guest DB that drifts (none should).
DRIFT_DETECTED=0
for i in "${!STAGED_NAMES[@]}"; do
    name="${STAGED_NAMES[$i]}"
    before="${CHECKSUMS_BEFORE[$i]}"
    # Both system root and guest DBs live under STAGE_DIR — same lookup.
    path="$STAGE_DIR/$name"
    after="$(_hash_path "$path")"
    if [ "$before" != "$after" ]; then
        if [ "$DRIFT_DETECTED" -eq 0 ]; then
            echo -e "${YELLOW}WARNING: Staged database(s) modified during tests${NC}"
            DRIFT_DETECTED=1
        fi
        echo "  $name"
        echo "    Before: $before"
        echo "    After:  $after"
    fi
done

echo
if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed${NC}"
else
    echo -e "${RED}✗ Some tests failed${NC}"
fi

exit $EXIT_CODE
