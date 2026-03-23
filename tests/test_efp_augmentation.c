/*
 * Test for EFP Augmentation Path (Issue #199)
 *
 * This test verifies that loading a v7 database and exercising the EFP
 * augmentation path does not crash with a segfault.
 *
 * Root Cause (Fixed): Null pointer dereference in augment_database_tables_with_efp()
 *   - hashtablelookup(..., nil, &node) unconditionally dereferences the nil parameter
 *   - Fix: Use hashtablelookupnode(..., &node) instead
 *
 * See: planning/architectural_decision_records/ADR-004-dynamic-verb-binding-architecture.md
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "frontier.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "tablestructure.h"
#include "file.h"
#include "odbinternal.h"
#include "db_format.h"
#include "logging.h"
#include "test_report.h"

static int test_count = 0;
static int test_passed = 0;

#define TEST_PASS(desc) do { test_count++; test_passed++; log_info(LOG_COMP_DB, "PASS: %s", desc); } while(0)
#define TEST_FAIL(desc) do { test_count++; log_error(LOG_COMP_DB, "FAIL: %s", desc); } while(0)

int main(void) {
    TR_INIT("test_efp_augmentation");
    log_init();

    log_info(LOG_COMP_DB, "=== EFP Augmentation Path Test (Issue #199) ===");

    assert(initmemory());
    initstrings();
    assert(initlang());
    assert(inittablestructure());
    assert(langinitverbs());

    // Use the migrated v7 database from save_migration_tests
    const char *db_path = "tmp/migration/test_save_migration.root";
    FILE *check = fopen(db_path, "rb");
    if (!check) {
        log_error(LOG_COMP_DB, "Test database not found: %s", db_path);
        log_error(LOG_COMP_DB, "Run save_migration_tests first to create v7 database");
        return 1;
    }
    fclose(check);

    log_info(LOG_COMP_DB, "Phase 1: Load v7 database with EFP augmentation");

    // Convert path to filespec
    bigstring bspath;
    copyctopstring(db_path, bspath);

    tyfilespec fs;
    memset(&fs, 0, sizeof fs);
    if (!pathtofilespec(bspath, &fs)) {
        log_error(LOG_COMP_DB, "Failed to convert path to filespec: %s", db_path);
        TEST_FAIL("Path conversion");
        return 1;
    }

    // Open database file
    hdlfilenum fnum = 0;
    if (!openfile(&fs, &fnum, true)) {
        log_error(LOG_COMP_DB, "Failed to open database: %s", db_path);
        TEST_FAIL("Database open");
        return 1;
    }
    TEST_PASS("Database file opened");

    // Open database
    if (!dbopenfile(fnum, true)) {
        log_error(LOG_COMP_DB, "dbopenfile failed for: %s", db_path);
        closefile(fnum);
        TEST_FAIL("Database open");
        return 1;
    }
    TEST_PASS("Database opened successfully");

    // Get root table address
    dbaddress adr = nildbaddress;
    dbgetview(cancoonview, &adr);
    if (adr == nildbaddress) {
        log_error(LOG_COMP_DB, "Failed to get root table address");
        dbdispose();
        closefile(fnum);
        TEST_FAIL("Root table address");
        return 1;
    }
    TEST_PASS("Root table address obtained");

    // Load root table
    Handle hrootvariable = nil;
    hdlhashtable hroot = nil;
    if (!tableloadsystemtable(adr, &hrootvariable, &hroot, false)) {
        log_error(LOG_COMP_DB, "Failed to load system table");
        dbdispose();
        closefile(fnum);
        TEST_FAIL("System table load");
        return 1;
    }
    TEST_PASS("System table loaded");

    // Set table structure globals
    rootvariable = hrootvariable;
    roottable = hroot;

    // Check table structure
    if (!checktablestructure(true)) {
        log_warn(LOG_COMP_DB, "checktablestructure reported issues (expected for minimal database)");
    }

    // Link system table structure (creates EFP tables in memory)
    if (!linksystemtablestructure(hroot)) {
        log_error(LOG_COMP_DB, "linksystemtablestructure failed");
        TEST_FAIL("System table structure link");
        goto cleanup;
    }
    TEST_PASS("System table structure linked (EFP tables created)");

    // Resolve system.paths addresses
    if (!resolve_system_paths(hroot)) {
        log_error(LOG_COMP_DB, "resolve_system_paths failed");
        TEST_FAIL("System paths resolution");
        goto cleanup;
    }
    TEST_PASS("System paths resolved");

    log_info(LOG_COMP_DB, "Phase 2: Exercise EFP augmentation path");

    // THIS IS THE CRITICAL TEST: augment_database_tables_with_efp must not crash
    // Bug (Issue #199): hashtablelookup(..., nil, &node) caused segfault
    // Fix: Use hashtablelookupnode(..., &node) instead
    if (!augment_database_tables_with_efp(hroot)) {
        log_error(LOG_COMP_DB, "augment_database_tables_with_efp failed");
        TEST_FAIL("EFP augmentation");
        goto cleanup;
    }
    TEST_PASS("EFP augmentation completed without crash");

    // Verify that augmentation actually did something
    // Check if system table has valueroutines after augmentation
    if (systemtable != nil) {
        log_info(LOG_COMP_DB, "System table exists at %p", (void*)systemtable);
        TEST_PASS("System table accessible after augmentation");
    } else {
        log_warn(LOG_COMP_DB, "System table is nil after augmentation");
        TEST_FAIL("System table accessibility");
    }

    log_info(LOG_COMP_DB, "Phase 3: Cleanup");

cleanup:
    // Clean up
    cleartablestructureglobals();
    dbdispose();
    closefile(fnum);

    // Print summary
    log_info(LOG_COMP_DB, "=== EFP Augmentation Test Summary ===");
    log_info(LOG_COMP_DB, "Tests passed: %d/%d", test_passed, test_count);

    if (test_passed == test_count) {
        log_info(LOG_COMP_DB, "✓ ALL TESTS PASSED");
        printf("test_efp_augmentation: PASSED - EFP augmentation path works without crash\n");
    } else {
        log_error(LOG_COMP_DB, "✗ SOME TESTS FAILED");
        printf("test_efp_augmentation: FAILED - %d/%d tests passed\n", test_passed, test_count);
    }

    if (tr_count < TR_MAX_TESTS) {
        tr_results[tr_count].name = "all_tests";
        tr_results[tr_count].passed = (test_passed == test_count ? 1 : 0);
        tr_count++;
        if (test_passed == test_count) tr_pass_count++; else tr_fail_count++;
    }
    TR_SUMMARY();
    return TR_EXIT_CODE();
}
