#include "../framework/test_framework.h"
#include "../../Common/headers/db.h"
#include "../../Common/headers/db_format.h"
#include "../../Common/headers/db_reader.h"
#include "../../Common/headers/dbinternal.h"
#include "test_migration_shim.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *test_legacy_db = "test_migration_legacy.root";
static const size_t legacy_view_base = 10;
static const size_t legacy_view_stride = 4;
static const char *fixture_v6_path = "tests/fixtures/v6/test.root";

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
    test_remove_if_exists(path);
}

static bool copy_file(const char *src, const char *dst) {
    return test_copy_file(src, dst);
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

    bigstring bs; tyfilespec fs; hdlfilenum fnum = 0;
    copyctopstring(test_legacy_db, bs);
    TEST_ASSERT(pathtofilespec(bs, &fs), "pathtofilespec legacy");
    TEST_ASSERT(openfile(&fs, &fnum, true), "openfile legacy");

    unsigned char old_header[LEGACY_DB_HEADER_BYTES];
    TEST_ASSERT(fileread(fnum, sizeof old_header, old_header), "Should read legacy header");
    closefile(fnum);

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

    copyctopstring(test_legacy_db, bs);
    TEST_ASSERT(pathtofilespec(bs, &fs), "pathtofilespec migrated");
    TEST_ASSERT(openfile(&fs, &fnum, true), "openfile migrated");
    {
        tydatabaserecord_64 header;
        TEST_ASSERT(fileread(fnum, sizeof header, &header), "Should read migrated header");
        TEST_ASSERT(header.versionnumber == 7, "Migrated header is version 7");
        TEST_ASSERT(header.views[0] == 0x0057fca4, "View[0] migrated correctly");
        TEST_ASSERT(header.views[2] == 0x00004006, "View[2] migrated correctly");
    }
    closefile(fnum);

    char backup_path[1024];
    if (db_format_last_backup_path(backup_path, sizeof backup_path)) {
        remove_if_exists(backup_path);
        db_format_clear_last_backup_path();
    }
    return true;
}

static bool test_version_detection(void) {
    db_format_mode prev_mode = db_format_mode_current();
    db_format_mode mode = prev_mode;
    mode.use_64bit_format = false;
    db_format_mode_apply(&mode);

    tydatabaserecord legacy_header = {0};
    legacy_header.versionnumber = 6;
    TEST_ASSERT(detect_database_format(&legacy_header), "Should detect legacy format");
    TEST_ASSERT(!db_format_mode_current().use_64bit_format, "Legacy keeps 32-bit flag");

    tydatabaserecord modern_header = {0};
    modern_header.versionnumber = 7;
    TEST_ASSERT(detect_database_format(&modern_header), "Should detect modern format");
    TEST_ASSERT(!db_format_mode_current().use_64bit_format, "Detection is side-effect free");
    TEST_ASSERT(db_format_load_v7_reader(&modern_header, true), "Modern reader loads");
    TEST_ASSERT(db_format_mode_current().use_64bit_format, "Modern toggles 64-bit flag");

    db_format_mode_apply(&prev_mode);
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
    char output_path[1024];
    TEST_ASSERT(ensure_database_modern(test_legacy_db, &migrated, output_path, sizeof output_path), "ensure_database_modern should succeed");
    TEST_ASSERT(migrated, "Legacy file should migrate");

    /* Verify v7 file was created */
    bigstring bs; tyfilespec fs; hdlfilenum fnum = 0;
    copyctopstring(output_path, bs);
    TEST_ASSERT(pathtofilespec(bs, &fs), "pathtofilespec v7");
    TEST_ASSERT(openfile(&fs, &fnum, true), "openfile v7");
    {
        tydatabaserecord_64 header;
        TEST_ASSERT(fileread(fnum, sizeof header, &header), "Should read v7 header");
        TEST_ASSERT(header.versionnumber == 7, "Header should be v7");
    }
    closefile(fnum);
    TEST_ASSERT(header.versionnumber == 7, "Header should be v7");

    /* Verify original is still v6 */
    copyctopstring(test_legacy_db, bs);
    TEST_ASSERT(pathtofilespec(bs, &fs), "pathtofilespec original");
    TEST_ASSERT(openfile(&fs, &fnum, true), "openfile original");
    {
        tydatabaserecord orig_header;
        TEST_ASSERT(fileread(fnum, sizeof orig_header, &orig_header), "Should read original header");
        TEST_ASSERT(orig_header.versionnumber == 6, "Original should still be v6");
    }
    closefile(fnum);

    /* Clean up v7 file */
    remove_if_exists(output_path);
    db_format_clear_last_backup_path();

    migrated = false;
    TEST_ASSERT(ensure_database_modern(test_legacy_db, &migrated, output_path, sizeof output_path), "ensure should succeed second time");
    TEST_ASSERT(migrated, "Should migrate again since we deleted the v7 file");
    remove_if_exists(output_path);
    return true;
}

static bool test_fixture_migration(void) {
    /* Copy fixture so we don't mutate the source in-tree. */
    const char *tmp_v6 = "test_fixture_copy.root";
    remove_if_exists(tmp_v6);
    remove_if_exists("test_fixture_copy.root.v7");

    TEST_ASSERT(copy_file(fixture_v6_path, tmp_v6), "Copy v6 fixture");

    boolean migrated = false;
    char output_path[1024];
    memset(output_path, 0, sizeof output_path);
    TEST_ASSERT(ensure_database_modern(tmp_v6, &migrated, output_path, sizeof output_path), "ensure_database_modern on fixture");
    TEST_ASSERT(migrated, "Fixture should migrate to v7");

    const char *v7_path = (output_path[0] != '\0') ? output_path : tmp_v6;

    copyctopstring(v7_path, bs);
    TEST_ASSERT(pathtofilespec(bs, &fs), "pathtofilespec fixture v7");
    TEST_ASSERT(openfile(&fs, &fnum, true), "openfile fixture v7");
    {
        tydatabaserecord_64 header;
        TEST_ASSERT(fileread(fnum, sizeof header, &header), "Read migrated header");
        TEST_ASSERT(header.versionnumber == 7, "Migrated fixture version = 7");
        TEST_ASSERT(header.headerLength == (long) sizeof(tydatabaserecord_64), "Migrated header length = 88");
    }
    closefile(fnum);

    /* Open via dbopenfile to ensure header and views parse */
    copyctopstring(v7_path, bs);
    TEST_ASSERT(pathtofilespec(bs, &fs), "pathtofilespec(v7)");
    TEST_ASSERT(openfile(&fs, &fnum, true), "openfile(v7)");
    TEST_ASSERT(dbopenfile(fnum, true), "dbopenfile(v7)");
    TEST_ASSERT(dbclose(), "dbclose(v7)");
    TEST_ASSERT(closefile(fnum), "closefile(v7)");

    remove_if_exists(tmp_v6);
    if (v7_path != tmp_v6)
        remove_if_exists(v7_path);
    db_format_clear_last_backup_path();
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
    {"Fixture Migration", test_fixture_migration},
    {"Cleanup", test_cleanup},
};

int migration_test_count = 8;
