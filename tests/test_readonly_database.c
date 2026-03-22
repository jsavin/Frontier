#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <CommonCrypto/CommonDigest.h>

/* 2026-01-15 Codex: Validate read-only database handling during migration (PR #310).
 *
 * Test Purpose:
 * - Ensure v6 source database is NOT modified during v6→v7 migration
 * - Verify MD5 hash unchanged after migration (regression test for PR #310 bug)
 * - Validate that setdirty() prevention works correctly
 *
 * Why This Matters:
 * - PR #310 fixes bug where v6 source database was being modified during migration
 * - Root cause: setdirty() was called even on read-only databases
 * - dbflushheader() wrote header changes to read-only files
 * - This test validates the defense-in-depth fix at multiple levels
 *
 * Test Strategy:
 * - Copy v6 database to test location
 * - Calculate MD5 hash of v6 source BEFORE migration
 * - Run migration via migrate_32bit_to_64bit()
 * - Calculate MD5 hash of v6 source AFTER migration
 * - Verify MD5 unchanged (proves v6 source not modified)
 * - Verify v7 destination created successfully
 *
 * References:
 * - PR #310: Prevent v6 source database modification during migration
 * - CLAUDE.md: Database debugging patterns
 * - planning/phase3/database_migration.md: Migration workflow documentation
 */

#include "frontier.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "tablestructure.h"

#include "file.h"
#include "odbinternal.h"
#include "db_format.h"
#include "logging.h"

static int test_count = 0;
static int test_passed = 0;

#define READONLY_TEST_PASS(desc) do { test_passed++; log_info(LOG_COMP_DB, "PASS: %s", desc); } while(0)
#define READONLY_TEST_FAIL(desc) do { log_error(LOG_COMP_DB, "FAIL: %s", desc); } while(0)

/* Get test output directory - creates tests/tmp/unit/ */
static bool get_test_unit_dir(char *out, size_t out_size) {
    char repo_root[1024];
    if (getcwd(repo_root, sizeof repo_root) == NULL)
        return false;

    // Strip /tests suffix if present (when run from tests/ directory)
    size_t len = strlen(repo_root);
    const char suffix[] = "/tests";
    size_t suffix_len = strlen(suffix);

    if (len >= suffix_len && strcmp(repo_root + len - suffix_len, suffix) == 0) {
        repo_root[len - suffix_len] = '\0';
    }

    // Construct unit test output directory
    snprintf(out, out_size, "%s/tests/tmp/unit", repo_root);

    // Create directory hierarchy
    char tmp_path[1280];
    snprintf(tmp_path, sizeof tmp_path, "%s/tests", repo_root);
    mkdir(tmp_path, 0755);  // Ignore errors if exists

    snprintf(tmp_path, sizeof tmp_path, "%s/tests/tmp", repo_root);
    mkdir(tmp_path, 0755);  // Ignore errors if exists

    if (mkdir(out, 0755) != 0 && errno != EEXIST) {
        return false;
    }

    return true;
}

/* Calculate MD5 hash of a file */
static bool calculate_md5(const char *path, unsigned char md5[CC_MD5_DIGEST_LENGTH]) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    CC_MD5_CTX ctx;
    CC_MD5_Init(&ctx);

    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        CC_MD5_Update(&ctx, buf, (CC_LONG)n);
    }

    CC_MD5_Final(md5, &ctx);
    fclose(f);
    return true;
}

/* Compare two MD5 hashes */
static bool md5_equal(const unsigned char md5a[CC_MD5_DIGEST_LENGTH],
                     const unsigned char md5b[CC_MD5_DIGEST_LENGTH]) {
    return memcmp(md5a, md5b, CC_MD5_DIGEST_LENGTH) == 0;
}

/* Format MD5 hash as hex string */
static void md5_to_string(const unsigned char md5[CC_MD5_DIGEST_LENGTH], char *out, size_t out_size) {
    char *p = out;
    for (int i = 0; i < CC_MD5_DIGEST_LENGTH && (size_t)(p - out) < out_size - 3; i++) {
        snprintf(p, 3, "%02x", md5[i]);
        p += 2;
    }
}

int main(void) {
    log_init();

    // Get unit test output directory
    char unit_dir[1024];
    if (!get_test_unit_dir(unit_dir, sizeof unit_dir)) {
        log_error(LOG_COMP_DB, "Failed to get unit test output directory");
        return 1;
    }

    log_info(LOG_COMP_DB, "=== Read-Only Database Migration Test (PR #310) ===");
    log_info(LOG_COMP_DB, "Test output directory: %s", unit_dir);

    assert(initmemory());
    initstrings();
    assert(initlang());
    assert(inittablestructure());
    assert(langinitverbs());

    // Pick a legacy v6 database to test with (from test fixtures)
    const char *src = "tests/fixtures/v6/Frontier.root";
    FILE *in = fopen(src, "rb");
    if (!in) {
        src = "../tests/fixtures/v6/Frontier.root"; // when running from tests/
        in = fopen(src, "rb");
    }
    assert(in != NULL);

    // Construct test database paths:
    // test_v6_db: the input path (v6 goes here, then gets renamed to v6_backup after migration)
    // test_v7_db: after migration, v7 output goes to the original path
    // test_v6_backup: after migration, v6 original is backed up here
    char test_v6_db[1280];
    snprintf(test_v6_db, sizeof test_v6_db, "%s/test_readonly_v6.root", unit_dir);

    char test_v7_db[1280];
    snprintf(test_v7_db, sizeof test_v7_db, "%s/test_readonly_v6.root", unit_dir);

    char test_v6_backup[1280];
    snprintf(test_v6_backup, sizeof test_v6_backup, "%s/test_readonly_v6.v6.root", unit_dir);

    // Remove any existing test databases
    unlink(test_v6_db);
    unlink(test_v7_db);
    unlink(test_v6_backup);

    // Copy source to test location
    FILE *out = fopen(test_v6_db, "wb");
    assert(out != NULL);

    char buf[64 * 1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
        assert(fwrite(buf, 1, n, out) == n);
    }
    fclose(out);
    fclose(in);

    log_info(LOG_COMP_DB, "Created test v6 database: %s", test_v6_db);

    // =============================================================================
    // Test 1: Calculate MD5 hash of v6 source BEFORE migration
    // =============================================================================
    test_count++;
    log_info(LOG_COMP_DB, "Test 1: Calculate MD5 hash of v6 source BEFORE migration");

    unsigned char md5_before[CC_MD5_DIGEST_LENGTH];
    if (!calculate_md5(test_v6_db, md5_before)) {
        log_error(LOG_COMP_DB, "Failed to calculate MD5 of v6 source");
        return 1;
    }

    char md5_str[33];
    md5_to_string(md5_before, md5_str, sizeof md5_str);
    log_info(LOG_COMP_DB, "v6 MD5 before migration: %s", md5_str);
    READONLY_TEST_PASS("MD5 calculated before migration");

    // =============================================================================
    // Test 2: Run migration via migrate_32bit_to_64bit()
    // =============================================================================
    test_count++;
    log_info(LOG_COMP_DB, "Test 2: Run migration via migrate_32bit_to_64bit()");

    if (!migrate_32bit_to_64bit(test_v6_db)) {
        log_error(LOG_COMP_DB, "FATAL: Migration failed");
        return 1;
    }

    READONLY_TEST_PASS("Migration completed successfully");

    // =============================================================================
    // Test 3: Calculate MD5 hash of v6 backup AFTER migration
    // Migration renames v6 original to .v6.root and writes v7 to original path.
    // =============================================================================
    test_count++;
    log_info(LOG_COMP_DB, "Test 3: Calculate MD5 hash of v6 backup after migration");

    unsigned char md5_after[CC_MD5_DIGEST_LENGTH];
    if (!calculate_md5(test_v6_backup, md5_after)) {
        log_error(LOG_COMP_DB, "Failed to calculate MD5 of v6 backup: %s", test_v6_backup);
        return 1;
    }

    md5_to_string(md5_after, md5_str, sizeof md5_str);
    log_info(LOG_COMP_DB, "v6 backup MD5 after migration:  %s", md5_str);

    // =============================================================================
    // Test 4: Verify v6 backup MD5 matches original (data preserved)
    // =============================================================================
    test_count++;
    log_info(LOG_COMP_DB, "Test 4: Verify v6 backup MD5 matches original");

    if (md5_equal(md5_before, md5_after)) {
        READONLY_TEST_PASS("v6 backup MD5 matches original (data preserved)");
    } else {
        READONLY_TEST_FAIL("v6 backup MD5 does NOT match original (data corruption)");
        log_error(LOG_COMP_DB, "  Before: %s", md5_str);
        char md5_before_str[33];
        md5_to_string(md5_before, md5_before_str, sizeof md5_before_str);
        log_error(LOG_COMP_DB, "  After:  %s", md5_before_str);
    }

    // =============================================================================
    // Test 5: Verify v7 destination file exists
    // =============================================================================
    test_count++;
    log_info(LOG_COMP_DB, "Test 5: Verify v7 destination file exists");

    struct stat st;
    if (stat(test_v7_db, &st) == 0) {
        READONLY_TEST_PASS("v7 destination file created");
    } else {
        READONLY_TEST_FAIL("v7 destination file NOT created");
    }

    // =============================================================================
    // Test 6: Verify v7 file is larger than 0 bytes (basic sanity check)
    // =============================================================================
    test_count++;
    log_info(LOG_COMP_DB, "Test 6: Verify v7 file is valid (size > 0)");

    if (stat(test_v7_db, &st) == 0 && st.st_size > 0) {
        READONLY_TEST_PASS("v7 destination file has valid size");
        log_info(LOG_COMP_DB, "  v7 file size: %lld bytes", (long long)st.st_size);
    } else {
        READONLY_TEST_FAIL("v7 destination file has invalid size");
    }

    // Print summary
    log_info(LOG_COMP_DB, "=== Test Summary ===");
    log_info(LOG_COMP_DB, "Tests passed: %d/%d", test_passed, test_count);

    if (test_passed == test_count) {
        log_info(LOG_COMP_DB, "✓ ALL TESTS PASSED");
        log_info(LOG_COMP_DB, "");
        log_info(LOG_COMP_DB, "PR #310 fix verified:");
        log_info(LOG_COMP_DB, "  ✓ v6 source database NOT modified during migration");
        log_info(LOG_COMP_DB, "  ✓ v7 destination database created successfully");
        log_info(LOG_COMP_DB, "  ✓ setdirty() prevention working correctly");
        printf("test_readonly_database: All tests passed [%d/%d]\n", test_passed, test_count);
        return 0;
    } else {
        log_error(LOG_COMP_DB, "✗ SOME TESTS FAILED");
        printf("test_readonly_database: Some tests failed [%d/%d]\n", test_passed, test_count);
        return 1;
    }
}
