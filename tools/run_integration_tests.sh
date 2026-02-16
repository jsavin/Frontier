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
SYSTEM_ROOT="$PROJECT_ROOT/databases/Frontier.root"
SYSTEM_ROOT7="$PROJECT_ROOT/databases/Frontier.root7"
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
    # No arguments - run all tests (except network tests unless opt-in)
    for f in "$TEST_CASES_DIR"/*.yaml; do
        if [ -f "$f" ]; then
            # Skip network tests unless FRONTIER_RUN_NETWORK_TESTS=1
            if [[ "$f" == *"_network.yaml" ]]; then
                if [ "${FRONTIER_RUN_NETWORK_TESTS:-0}" != "1" ]; then
                    echo -e "${YELLOW}Skipping network tests: $(basename "$f")${NC}"
                    echo "  (Set FRONTIER_RUN_NETWORK_TESTS=1 to enable)"
                    continue
                fi
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
                echo "If no test files are specified, all tests in tests/integration/test_cases/ will be run."
                echo "Network tests (*_network.yaml) are skipped by default unless FRONTIER_RUN_NETWORK_TESTS=1."
                echo
                echo "Environment Variables:"
                echo "  FRONTIER_RUN_NETWORK_TESTS=1    Enable network-dependent tests (default: 0)"
                echo
                echo "Examples:"
                echo "  $0                                    # Run all local tests (batch + parallel)"
                echo "  $0 --no-batch -j 1                   # Old behavior (per-process, sequential)"
                echo "  FRONTIER_RUN_NETWORK_TESTS=1 $0      # Run all tests including network"
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

# Remove stale .root7 and force fresh migration from v6 root on each run.
# This prevents tests from being bitten by stale migrated data.
if [ -f "$SYSTEM_ROOT7" ]; then
    echo "Removing stale $SYSTEM_ROOT7 to force fresh migration..."
    rm -f "$SYSTEM_ROOT7"
fi
echo "Migrating $SYSTEM_ROOT -> $SYSTEM_ROOT7 ..."
"$CLI_PATH" --system-root "$SYSTEM_ROOT" -e "1" > /dev/null 2>&1
if [ ! -f "$SYSTEM_ROOT7" ]; then
    echo -e "${RED}Error: Migration failed - $SYSTEM_ROOT7 not created${NC}"
    exit 1
fi

# Record pre-test database checksum for integrity verification
CHECKSUM_BEFORE=$(md5 -q "$SYSTEM_ROOT7" 2>/dev/null || md5sum "$SYSTEM_ROOT7" | cut -d' ' -f1)

# Run the tests (using the freshly migrated .root7)
"$RUNNER" $VERBOSE $BATCH_FLAG $WORKERS_FLAG --cli "$CLI_PATH" --system-root "$SYSTEM_ROOT7" "${TEST_FILES[@]}"
EXIT_CODE=$?

# Verify database integrity after tests
CHECKSUM_AFTER=$(md5 -q "$SYSTEM_ROOT7" 2>/dev/null || md5sum "$SYSTEM_ROOT7" | cut -d' ' -f1)
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
