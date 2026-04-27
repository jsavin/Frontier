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

if [ $# -eq 0 ]; then
    # No arguments - run all tests. Files matching *_network.yaml were
    # historically gated on FRONTIER_RUN_NETWORK_TESTS=1 because they hit
    # external network resources. Those tests have since been migrated to
    # localhost listeners and are self-contained, so they now run by default.
    # (One DNS-resolution test in tcp_verbs_network.yaml depends on the
    # system resolver returning NXDOMAIN for an invalid hostname; environments
    # that hijack NXDOMAIN responses will surface that as a real signal rather
    # than flake.)
    for f in "$TEST_CASES_DIR"/*.yaml; do
        if [ -f "$f" ]; then
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
                echo "Examples:"
                echo "  $0                                    # Run all tests (batch + parallel)"
                echo "  $0 --no-batch -j 1                   # Old behavior (per-process, sequential)"
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
# symlinked next to it (read-only). Tests can mutate Frontier.root freely
# without touching source files.
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
# Link sibling files/directories from databases/ so guest-DB-dependent tests
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
    ln -s "$entry" "$STAGE_DIR/$name"
done

# Record pre-test database checksum for integrity verification
CHECKSUM_BEFORE=$(md5 -q "$SYSTEM_ROOT" 2>/dev/null || md5sum "$SYSTEM_ROOT" | cut -d' ' -f1)

# Run the tests (using v7 source database directly)
"$RUNNER" $VERBOSE $BATCH_FLAG $WORKERS_FLAG --cli "$CLI_PATH" --system-root "$SYSTEM_ROOT" "${TEST_FILES[@]}"
EXIT_CODE=$?

# Verify database integrity after tests
CHECKSUM_AFTER=$(md5 -q "$SYSTEM_ROOT" 2>/dev/null || md5sum "$SYSTEM_ROOT" | cut -d' ' -f1)
if [ "$CHECKSUM_BEFORE" != "$CHECKSUM_AFTER" ]; then
    echo -e "${YELLOW}WARNING: System root was modified during tests${NC}"
    echo "  Before: $CHECKSUM_BEFORE"
    echo "  After:  $CHECKSUM_AFTER"
fi

echo
if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed${NC}"
else
    echo -e "${RED}✗ Some tests failed${NC}"
fi

exit $EXIT_CODE
