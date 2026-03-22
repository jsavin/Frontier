#!/bin/bash
# 2026-01-15 Codex: End-to-end migration integrity tests (PR #310)
#
# Test Purpose:
# - Verify v6 source database is never modified during migration
# - Verify v7 destination database is created correctly
# - Verify migration is idempotent (can be run multiple times safely)
# - Test complete migration workflow from CLI perspective
#
# Why This Matters:
# - PR #310 fixes critical bug where v6 source was being corrupted
# - These tests validate the fix from user's perspective (CLI workflow)
# - Ensures data integrity and safety for production migrations
#
# References:
# - PR #310: Prevent v6 source database modification during migration
# - docs/CLI_USAGE_GUIDE.md: frontier-cli migration workflow
# - planning/phase3/database_migration.md: Migration implementation details
#
# CRITICAL CONSTRAINT:
# - frontier-cli runs in macOS sandbox and CANNOT access /tmp
# - Use project-relative paths in .gitignore'd subdirectories
# - Use $(./tools/get_test_temp_path.sh) for temp directory

set -e  # Exit on error
set -u  # Exit on undefined variable

# Color output for readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get script directory (tests/system/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Test output directory (MUST be project-relative, not /tmp)
TEST_TMP_DIR="$("$PROJECT_ROOT/tools/get_test_temp_path.sh")"
MIGRATION_TEST_DIR="$PROJECT_ROOT/tests/tmp/migration"
mkdir -p "$MIGRATION_TEST_DIR"

# CLI binary path
CLI_BIN="$PROJECT_ROOT/frontier-cli/frontier-cli"

# Test counters
TEST_COUNT=0
TEST_PASSED=0

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $*"
    TEST_PASSED=$((TEST_PASSED + 1))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

test_start() {
    TEST_COUNT=$((TEST_COUNT + 1))
    log_info "Test $TEST_COUNT: $*"
}

# Calculate MD5 hash of a file
calculate_md5() {
    local file="$1"
    if [[ ! -f "$file" ]]; then
        echo "FILE_NOT_FOUND"
        return 1
    fi
    md5 -q "$file"
}

# Verify file exists and is readable
verify_file_exists() {
    local file="$1"
    if [[ ! -f "$file" ]]; then
        log_fail "File not found: $file"
        return 1
    fi
    log_success "File exists: $file"
    return 0
}

# Verify database version (first 2 bytes: 0006 for v6, 0007 for v7)
verify_database_version() {
    local file="$1"
    local expected_version="$2"

    if [[ ! -f "$file" ]]; then
        log_fail "Database file not found: $file"
        return 1
    fi

    # Read first 2 bytes (version number)
    local version_bytes=$(xxd -l 2 -p "$file")

    # Extract version (big-endian: first byte is high, second is low)
    local version=$((16#$version_bytes))

    if [[ $version -eq $expected_version ]]; then
        log_success "Database version is v$version (expected v$expected_version)"
        return 0
    else
        log_fail "Database version is v$version (expected v$expected_version)"
        return 1
    fi
}

# Main test suite
main() {
    echo ""
    echo "========================================="
    echo "Migration Integrity Test Suite (PR #310)"
    echo "========================================="
    echo ""
    log_info "Project root: $PROJECT_ROOT"
    log_info "Test output directory: $MIGRATION_TEST_DIR"
    log_info "CLI binary: $CLI_BIN"
    echo ""

    # Verify CLI binary exists
    if [[ ! -x "$CLI_BIN" ]]; then
        log_fail "CLI binary not found or not executable: $CLI_BIN"
        echo ""
        echo "Please build frontier-cli first:"
        echo "  cd $PROJECT_ROOT/frontier-cli && make"
        exit 1
    fi

    log_success "CLI binary found and executable"
    echo ""

    # Find source database
    V6_SOURCE="$PROJECT_ROOT/databases/Frontier.root"
    if [[ ! -f "$V6_SOURCE" ]]; then
        log_fail "Source database not found: $V6_SOURCE"
        exit 1
    fi

    log_success "Source database found: $V6_SOURCE"

    # Verify source is v6
    test_start "Verify source database is v6"
    if verify_database_version "$V6_SOURCE" 6; then
        log_success "Source database is v6 (correct)"
    else
        log_fail "Source database is not v6 (cannot test migration)"
        exit 1
    fi
    echo ""

    # =============================================================================
    # Test 1: Copy v6 database to test location
    # =============================================================================
    test_start "Copy v6 database to test location"

    V6_TEST_DB="$MIGRATION_TEST_DIR/test_migration_source.root"
    V6_BACKUP_DB="$MIGRATION_TEST_DIR/test_migration_source.v6.root"
    V7_OUTPUT_DB="$MIGRATION_TEST_DIR/test_migration_source.root"

    # Remove any existing test databases
    rm -f "$V6_TEST_DB" "$V6_BACKUP_DB"

    # Copy source to test location
    cp "$V6_SOURCE" "$V6_TEST_DB"

    if verify_file_exists "$V6_TEST_DB"; then
        log_success "v6 test database created"
    else
        log_fail "Failed to create v6 test database"
        exit 1
    fi
    echo ""

    # =============================================================================
    # Test 2: Calculate MD5 hash of v6 source BEFORE migration
    # =============================================================================
    test_start "Calculate MD5 hash of v6 source BEFORE migration"

    V6_MD5_BEFORE=$(calculate_md5 "$V6_TEST_DB")
    if [[ $? -ne 0 || "$V6_MD5_BEFORE" == "FILE_NOT_FOUND" ]]; then
        log_fail "Failed to calculate MD5 of v6 source"
        exit 1
    fi

    log_success "v6 MD5 before migration: $V6_MD5_BEFORE"
    echo ""

    # =============================================================================
    # Test 3: Run migration via frontier-cli
    # =============================================================================
    test_start "Run migration via frontier-cli"

    log_info "Executing: $CLI_BIN --system-root \"$V6_TEST_DB\" -e \"1+1\""

    # Run CLI to trigger migration (evaluation forces database open)
    OUTPUT=$("$CLI_BIN" --system-root "$V6_TEST_DB" -e "1+1" 2>&1)
    CLI_EXIT_CODE=$?

    if [[ $CLI_EXIT_CODE -eq 0 ]]; then
        log_success "CLI execution completed successfully"
        log_info "CLI output: $OUTPUT"
    else
        log_fail "CLI execution failed with exit code $CLI_EXIT_CODE"
        log_info "CLI output: $OUTPUT"
        exit 1
    fi
    echo ""

    # =============================================================================
    # Test 4: Verify v6 backup preserves original data
    # Migration renames v6 to .v6.root and writes v7 to original path.
    # =============================================================================
    test_start "Verify v6 backup MD5 matches original"

    V6_MD5_AFTER=$(calculate_md5 "$V6_BACKUP_DB")
    if [[ $? -ne 0 || "$V6_MD5_AFTER" == "FILE_NOT_FOUND" ]]; then
        log_fail "Failed to calculate MD5 of v6 backup: $V6_BACKUP_DB"
        exit 1
    fi

    log_info "v6 MD5 before: $V6_MD5_BEFORE"
    log_info "v6 backup MD5: $V6_MD5_AFTER"

    if [[ "$V6_MD5_BEFORE" == "$V6_MD5_AFTER" ]]; then
        log_success "v6 backup MD5 matches original (data preserved)"
    else
        log_fail "v6 backup MD5 does NOT match original (data corruption)"
        log_fail "  Before: $V6_MD5_BEFORE"
        log_fail "  After:  $V6_MD5_AFTER"
        exit 1
    fi
    echo ""

    # =============================================================================
    # Test 5: Verify v7 destination file exists
    # =============================================================================
    test_start "Verify v7 output file exists at original path"

    if verify_file_exists "$V7_OUTPUT_DB"; then
        log_success "v7 output file exists at original path"
    else
        log_fail "v7 output file NOT found at original path"
        exit 1
    fi
    echo ""

    # =============================================================================
    # Test 6: Verify v7 destination is valid v7 database
    # =============================================================================
    test_start "Verify v7 destination is valid v7 database"

    if verify_database_version "$V7_OUTPUT_DB" 7; then
        log_success "v7 destination is valid v7 database"
    else
        log_fail "v7 destination is NOT valid v7 database"
        exit 1
    fi
    echo ""

    # =============================================================================
    # Test 7: Verify v7 database can be loaded by CLI
    # =============================================================================
    test_start "Verify v7 database can be loaded by CLI"

    log_info "Executing: $CLI_BIN --system-root \"$V7_OUTPUT_DB\" -e \"sizeOf(system)\""

    OUTPUT=$("$CLI_BIN" --system-root "$V7_OUTPUT_DB" -e "sizeOf(system)" 2>&1)
    CLI_EXIT_CODE=$?

    if [[ $CLI_EXIT_CODE -eq 0 ]]; then
        log_success "v7 database loaded successfully"
        log_info "CLI output: $OUTPUT"
    else
        log_fail "Failed to load v7 database"
        log_info "CLI output: $OUTPUT"
        exit 1
    fi
    echo ""

    # =============================================================================
    # Test 8: Run migration AGAIN (idempotency test)
    # =============================================================================
    test_start "Run migration AGAIN (idempotency test)"

    # Calculate v6 MD5 before second migration
    V6_MD5_BEFORE_2ND=$(calculate_md5 "$V6_TEST_DB")

    # Remove v7 output to force re-migration
    rm -f "$V7_OUTPUT_DB"

    log_info "Executing second migration: $CLI_BIN --system-root \"$V6_TEST_DB\" -e \"1+1\""

    OUTPUT=$("$CLI_BIN" --system-root "$V6_TEST_DB" -e "1+1" 2>&1)
    CLI_EXIT_CODE=$?

    if [[ $CLI_EXIT_CODE -eq 0 ]]; then
        log_success "Second migration completed successfully"
    else
        log_fail "Second migration failed"
        exit 1
    fi

    # Verify v6 still unchanged after second migration
    V6_MD5_AFTER_2ND=$(calculate_md5 "$V6_TEST_DB")

    if [[ "$V6_MD5_BEFORE_2ND" == "$V6_MD5_AFTER_2ND" ]]; then
        log_success "v6 source unchanged after second migration (idempotent)"
    else
        log_fail "v6 source CHANGED after second migration (NOT idempotent)"
        exit 1
    fi

    # Verify v7 re-created
    if verify_file_exists "$V7_OUTPUT_DB"; then
        log_success "v7 database re-created after second migration"
    else
        log_fail "v7 database NOT re-created"
        exit 1
    fi
    echo ""

    # =============================================================================
    # Test Summary
    # =============================================================================
    echo ""
    echo "========================================="
    echo "Test Summary"
    echo "========================================="
    echo "Tests passed: $TEST_PASSED / $TEST_COUNT"
    echo ""

    if [[ $TEST_PASSED -eq $TEST_COUNT ]]; then
        log_success "ALL TESTS PASSED"
        echo ""
        echo "PR #310 fix verified:"
        echo "  ✓ v6 source database never modified"
        echo "  ✓ v7 destination database created correctly"
        echo "  ✓ Migration is idempotent"
        echo "  ✓ v7 database loads successfully"
        echo ""
        exit 0
    else
        log_fail "SOME TESTS FAILED"
        echo ""
        echo "Failed: $((TEST_COUNT - TEST_PASSED)) / $TEST_COUNT"
        echo ""
        exit 1
    fi
}

# Run main test suite
main "$@"
