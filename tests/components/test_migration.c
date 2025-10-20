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
static const size_t legacy_view_base = 10;
static const size_t legacy_view_stride = 4;

static void write_legacy_dbaddress(unsigned char *dest, uint32_t value) {
    dest[0] = (unsigned char)((value >> 24) & 0xFF);
    dest[1] = (unsigned char)((value >> 16) & 0xFF);
    dest[2] = (unsigned char)((value >> 8) & 0xFF);
    dest[3] = (unsigned char)(value & 0xFF);
}

static dbaddress read_legacy_dbaddress_test(const unsigned char *field) {
    uint32_t value = ((uint32_t) field[0] << 24) |
                     ((uint32_t) field[1] << 16) |
                     ((uint32_t) field[2] << 8)  |
                      (uint32_t) field[3];
    return (dbaddress) value;
}

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

    unsigned char header[LEGACY_DB_HEADER_BYTES];
    memset(header, 0, sizeof header);
    header[0] = dbsystemidMac;
    header[1] = 6;
    write_legacy_dbaddress(header + legacy_view_base + 0 * legacy_view_stride, 0x0057fca4u);
    write_legacy_dbaddress(header + legacy_view_base + 1 * legacy_view_stride, 0x00003000u);
    write_legacy_dbaddress(header + legacy_view_base + 2 * legacy_view_stride, 0x00004006u);
    uint32_t header_len = (uint32_t) sizeof header;
    header[30] = (unsigned char)((header_len >> 24) & 0xFF);
    header[31] = (unsigned char)((header_len >> 16) & 0xFF);
    header[32] = (unsigned char)((header_len >> 8) & 0xFF);
    header[33] = (unsigned char)(header_len & 0xFF);
    header[34] = 0x00;
    header[35] = 0x06;
    header[38] = 0x00;
    header[39] = 0x00;
    header[40] = 0xDE;
    header[41] = 0xAD;

    bool ok = fwrite(header, sizeof header, 1, f) == sizeof header;
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

    unsigned char old_header[LEGACY_DB_HEADER_BYTES];
    TEST_ASSERT(fread(old_header, 1, sizeof old_header, f) == sizeof old_header, "Should read legacy header");
    fclose(f);

    tydatabaserecord_64 new_header;
    memset(&new_header, 0, sizeof new_header);
    TEST_ASSERT(convert_32bit_header_to_64bit(old_header, &new_header), "Should convert header");

    TEST_ASSERT(new_header.systemid == old_header[0], "systemid preserved");
    TEST_ASSERT(new_header.versionnumber == 7, "version bumps to 7");
    TEST_ASSERT(new_header.availlist == read_legacy_dbaddress_test(old_header + 2), "availlist converted");
    TEST_ASSERT(new_header.oldfnumdatabase == 0, "oldfnum preserved");
    TEST_ASSERT(new_header.flags == 0, "flags preserved");
    TEST_ASSERT(new_header.views[0] == read_legacy_dbaddress_test(old_header + legacy_view_base + 0 * legacy_view_stride), "view0 converted");
    TEST_ASSERT(new_header.views[2] == read_legacy_dbaddress_test(old_header + legacy_view_base + 2 * legacy_view_stride), "view2 converted");
    TEST_ASSERT(new_header.headerLength == (long) sizeof(tydatabaserecord_64), "header length converted");
    TEST_ASSERT(new_header.longversionMajor == 6, "major version converted");
    TEST_ASSERT(new_header.longversionMinor == 0, "minor version converted");
    TEST_ASSERT(new_header.u.extensions.availlistblock == 0x0000DEAD, "avail block converted");
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
        TEST_ASSERT(header.views[0] == 0x0057fca4, "View[0] migrated correctly");
        TEST_ASSERT(header.views[2] == 0x00004006, "View[2] migrated correctly");
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
