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

if [ $# -eq 0 ]; then
    # No arguments - run all tests
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
            -h|--help)
                echo "Usage: $0 [OPTIONS] [TEST_FILES...]"
                echo
                echo "Options:"
                echo "  -v, --verbose      Verbose output"
                echo "  -h, --help         Show this help"
                echo
                echo "If no test files are specified, all tests in tests/integration/test_cases/ will be run."
                echo
                echo "Examples:"
                echo "  $0                                    # Run all tests"
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

# Run the tests
"$RUNNER" $VERBOSE --cli "$CLI_PATH" "${TEST_FILES[@]}"
EXIT_CODE=$?

echo
if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed${NC}"
else
    echo -e "${RED}✗ Some tests failed${NC}"
fi

exit $EXIT_CODE
