#include "../framework/test_framework.h"
#include "../../Common/headers/db.h"
#include "../../Common/headers/dbinternal.h"
#include "../../Common/headers/db_format.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *test_legacy_db = "test_migration_legacy.root";

static void remove_if_exists(const char *path) {
    if (path && unlink(path) == 0) {
        return;
    }
}

/* Create a minimal v6 header on disk */
static bool create_legacy_database(void) {
    remove_if_exists(test_legacy_db);

    FILE *f = fopen(test_legacy_db, "wb");
    if (!f)
        return false;

    tydatabaserecord header;
    memset(&header, 0, sizeof header);
    header.systemid = dbsystemidMac;
    header.versionnumber = 6;
    header.availlist = nildbaddress;
    header.headerLength = sizeof header;
    header.longversionMajor = 6;
    header.longversionMinor = 0;

    bool ok = fwrite(&header, sizeof header, 1, f) == 1;
    fclose(f);
    return ok;
}

static bool test_backup_creation(void) {
    TEST_ASSERT(create_legacy_database(), "Should create legacy database");
    TEST_ASSERT(create_root_backup(test_legacy_db), "Should create timestamped backup");

    char backup_path[1024];
    TEST_ASSERT(db_format_last_backup_path(backup_path, sizeof backup_path), "Backup path should be recorded");
    FILE *backup = fopen(backup_path, "rb");
    TEST_ASSERT(backup != NULL, "Backup file should exist");
    if (backup)
        fclose(backup);

    remove_if_exists(backup_path);
    db_format_clear_last_backup_path();
    return true;
}

static bool test_header_conversion(void) {
    TEST_ASSERT(create_legacy_database(), "Should create legacy database");

    FILE *f = fopen(test_legacy_db, "rb");
    TEST_ASSERT(f != NULL, "Should open legacy database");

    tydatabaserecord old_header;
    TEST_ASSERT(fread(&old_header, sizeof old_header, 1, f) == 1, "Should read legacy header");
    fclose(f);

    tydatabaserecord_64 new_header;
    memset(&new_header, 0, sizeof new_header);
    TEST_ASSERT(convert_32bit_header_to_64bit(&old_header, &new_header), "Should convert header");

    TEST_ASSERT(new_header.systemid == old_header.systemid, "systemid preserved");
    TEST_ASSERT(new_header.versionnumber == 7, "version bumps to 7");
    TEST_ASSERT(new_header.availlist == old_header.availlist, "availlist converted");
    TEST_ASSERT(new_header.oldfnumdatabase == old_header.oldfnumdatabase, "oldfnum preserved");
    TEST_ASSERT(new_header.flags == old_header.flags, "flags preserved");
    for (int i = 0; i < ctviews; i++)
        TEST_ASSERT(new_header.views[i] == old_header.views[i], "views converted");
    TEST_ASSERT(new_header.u.extensions.availlistshadow == nildbaddress, "shadow reset");
    TEST_ASSERT(!new_header.u.extensions.flreadonly, "readonly cleared");
    for (size_t i = 0; i < sizeof new_header.u.extensions.reserved; ++i)
        TEST_ASSERT(new_header.u.extensions.reserved[i] == 0, "reserved zeroed");
    return true;
}

static bool test_full_migration(void) {
    TEST_ASSERT(create_legacy_database(), "Should create legacy database");
    TEST_ASSERT(migrate_32bit_to_64bit(test_legacy_db), "Should migrate database");

    FILE *migrated = fopen(test_legacy_db, "rb");
    TEST_ASSERT(migrated != NULL, "Migrated file should exist");
    if (migrated) {
        tydatabaserecord_64 header;
        TEST_ASSERT(fread(&header, sizeof header, 1, migrated) == 1, "Should read migrated header");
        fclose(migrated);
        TEST_ASSERT(header.versionnumber == 7, "Migrated header is version 7");
    }

    char backup_path[1024];
    if (db_format_last_backup_path(backup_path, sizeof backup_path)) {
        remove_if_exists(backup_path);
        db_format_clear_last_backup_path();
    }
    return true;
}

static bool test_version_detection(void) {
    tydatabaserecord legacy_header = {0};
    legacy_header.versionnumber = 6;
    TEST_ASSERT(detect_database_format(&legacy_header), "Should detect legacy format");
    TEST_ASSERT(!use_64bit_format, "Legacy keeps 32-bit flag");

    tydatabaserecord modern_header = {0};
    modern_header.versionnumber = 7;
    TEST_ASSERT(detect_database_format(&modern_header), "Should detect modern format");
    TEST_ASSERT(use_64bit_format, "Modern toggles 64-bit flag");
    return true;
}

static bool test_error_handling(void) {
    TEST_ASSERT(!create_root_backup("does_not_exist.root"), "Backup should fail for missing file");
    TEST_ASSERT(!migrate_32bit_to_64bit("does_not_exist.root"), "Migration should fail for missing file");
    return true;
}

static bool test_ensure_modern(void) {
    TEST_ASSERT(create_legacy_database(), "Should create legacy database");
    boolean migrated = false;
    TEST_ASSERT(ensure_database_modern(test_legacy_db, &migrated), "ensure_database_modern should succeed");
    TEST_ASSERT(migrated, "Legacy file should migrate");

    FILE *f = fopen(test_legacy_db, "rb");
    TEST_ASSERT(f != NULL, "Migrated database should open");
    tydatabaserecord_64 header;
    TEST_ASSERT(fread(&header, sizeof header, 1, f) == 1, "Should read ensured header");
    fclose(f);
    TEST_ASSERT(header.versionnumber == 7, "Header upgraded to v7");

    char backup_path[1024];
    if (db_format_last_backup_path(backup_path, sizeof backup_path)) {
        remove_if_exists(backup_path);
        db_format_clear_last_backup_path();
    }

    migrated = false;
    TEST_ASSERT(ensure_database_modern(test_legacy_db, &migrated), "ensure should succeed second time");
    TEST_ASSERT(!migrated, "Already-modern db should not migrate again");
    return true;
}

static bool test_cleanup(void) {
    remove_if_exists(test_legacy_db);
    char backup_path[1024];
    if (db_format_last_backup_path(backup_path, sizeof backup_path)) {
        remove_if_exists(backup_path);
        db_format_clear_last_backup_path();
    }
    return true;
}

test_case_t migration_tests[] = {
    {"Backup Creation", test_backup_creation},
    {"Header Conversion", test_header_conversion},
    {"Full Migration", test_full_migration},
    {"Version Detection", test_version_detection},
    {"Error Handling", test_error_handling},
    {"Ensure Modern", test_ensure_modern},
    {"Cleanup", test_cleanup},
};

int migration_test_count = 7;
