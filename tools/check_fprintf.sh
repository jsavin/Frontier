#!/bin/bash
# check_fprintf.sh - Enforce structured logging over fprintf(stderr)
#
# Purpose: Prevent new fprintf(stderr, ...) statements from being added to the codebase.
#         All logging must use structured logging macros: log_trace, log_debug, log_error, log_warn.
#
# Usage: ./tools/check_fprintf.sh [--fix]
#        --fix: Show suggested migrations (informational only, does not auto-fix)
#
# Integration:
#   - Pre-commit hook: optional (too strict during development)
#   - CI/CD pipeline: recommended
#   - Local check: run before committing

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Files exempt from this check (legacy compatibility)
# Add patterns here if needed for legitimate use cases
EXEMPT_PATTERNS=(
    "test_"           # Test files may have fprintf for diagnostics
    "legacy"          # Legacy support code
)

# Color output
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# Check for fprintf(stderr, ...) in source files
check_fprintf() {
    local violations=()

    # Search in *.c and *.h files
    while IFS= read -r line; do
        file=$(echo "$line" | cut -d: -f1)
        linenum=$(echo "$line" | cut -d: -f2)
        content=$(echo "$line" | cut -d: -f3-)

        # Check if file should be exempted
        should_exempt=0
        for pattern in "${EXEMPT_PATTERNS[@]}"; do
            if [[ "$file" == *"$pattern"* ]]; then
                should_exempt=1
                break
            fi
        done

        if [ $should_exempt -eq 0 ]; then
            violations+=("$file:$linenum:$content")
        fi
    done < <(git grep -n 'fprintf(stderr' -- '*.c' '*.h' 2>/dev/null || true)

    if [ ${#violations[@]} -gt 0 ]; then
        return 1
    fi
    return 0
}

# Show violations and suggestions
show_violations() {
    local violations=()

    while IFS= read -r line; do
        file=$(echo "$line" | cut -d: -f1)
        linenum=$(echo "$line" | cut -d: -f2)
        content=$(echo "$line" | cut -d: -f3-)

        # Check if file should be exempted
        should_exempt=0
        for pattern in "${EXEMPT_PATTERNS[@]}"; do
            if [[ "$file" == *"$pattern"* ]]; then
                should_exempt=1
                break
            fi
        done

        if [ $should_exempt -eq 0 ]; then
            violations+=("$file:$linenum:$content")
        fi
    done < <(git grep -n 'fprintf(stderr' -- '*.c' '*.h' 2>/dev/null || true)

    if [ ${#violations[@]} -gt 0 ]; then
        echo -e "${RED}ERROR: fprintf(stderr) detected${NC}"
        echo ""
        echo "The following files contain fprintf(stderr, ...) calls:"
        echo "All logging must use structured logging macros instead."
        echo ""

        for violation in "${violations[@]}"; do
            echo -e "${YELLOW}$violation${NC}"
        done

        echo ""
        echo "Suggested fixes:"
        echo "  1. Replace fprintf(stderr, ...) with log_trace/debug/error/warn"
        echo "  2. See docs/LOGGING_STANDARDS.md for patterns and examples"
        echo "  3. Use log_hex_dump() for binary data diagnostics"
        echo "  4. Use log_enabled() for conditional expensive operations"
        echo ""
        echo "Examples:"
        echo "  BEFORE: fprintf(stderr, \"[headless] error: %s\\n\", msg);"
        echo "  AFTER:  log_error(LOG_COMP_DB, \"error: %s\", msg);"
        echo ""
        echo "  BEFORE: for (int i = 0; i < len; i++) fprintf(stderr, \" %02x\", buf[i]);"
        echo "  AFTER:  log_hex_dump(LOG_COMP_HASH, LOG_LEVEL_TRACE, buf, len, \"label\");"
        echo ""

        return 1
    fi

    echo -e "${GREEN}✓ No fprintf(stderr) violations detected${NC}"
    return 0
}

# Main
cd "$REPO_ROOT"

if [ "$1" = "--fix" ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    show_violations
    exit $?
else
    if check_fprintf; then
        echo -e "${GREEN}✓ Logging check passed${NC}"
        exit 0
    else
        echo -e "${RED}✗ Logging check failed${NC}"
        echo "Run: ./tools/check_fprintf.sh --fix    (to see details)"
        exit 1
    fi
fi
