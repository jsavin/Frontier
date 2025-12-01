/* 2025-11-30 Codex: Add portable migration regression that exercises the v6 fixture and portable file helpers. */

/* 2025-11-30 Codex: Migration regression exercising v6 fixture under headless runtime. */
/* 2025-11-30 Codex: Allow KEEP_MIGRATION_ARTIFACTS to retain migrated files for debugging. */
/* 2025-12-01 Codex: Resolve fixture paths robustly and clamp debug logging to avoid garbage output. */

#include "../framework/test_framework.h"
#include "../../Common/headers/frontier.h"
#include "../../Common/headers/db.h"
#include "../../Common/headers/db_format.h"
#include "../../Common/headers/db_reader.h"
#include "../../Common/headers/dbinternal.h"
#include "../../Common/headers/strings.h"
#include "../../Common/headers/lang.h"
#include "../../Common/headers/tablestructure.h"
#include "../../Common/headers/tableverbs.h"
#include "test_migration_portable.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TEST_MIGRATION_HAS_RUNTIME
#define TEST_MIGRATION_HAS_RUNTIME 0
#endif

static const char *test_legacy_db = "test_migration_legacy.root";
static const size_t legacy_view_base = 10;
static const size_t legacy_view_stride = 4;

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

static void bs_to_cstring(const bigstring bs, char *out, size_t outlen) {
    short len = stringlength(bs);
    if (len < 0)
        len = 0;
    if ((size_t) len >= outlen)
        len = (short) (outlen - 1);
    memmove(out, stringbaseaddress(bs), (size_t) len);
    out[len] = '\0';
}

static bool resolve_fixture_path(char *out, size_t outlen) {
    static const char *candidates[] = {
        "tests/fixtures/v6/test.root",
        "fixtures/v6/test.root"
    };

    if (outlen == 0)
        return false;

    for (size_t i = 0; i < sizeof candidates / sizeof candidates[0]; ++i) {
        const char *candidate = candidates[i];
        FILE *fp = fopen(candidate, "rb");
        if (fp != NULL) {
            fclose(fp);
            strncpy(out, candidate, outlen - 1);
            out[outlen - 1] = '\0';
            return true;
        }
    }

    out[0] = '\0';
    return false;
}

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
    tm_remove_if_exists(path);
}

#if TEST_MIGRATION_HAS_RUNTIME
static bool assert_symbol_type(hdlhashtable htable, const char *name, short expected_type, const char *label) {
    bigstring bs;
    tyvaluerecord val = {0};
    hdlhashnode node = nil;
    boolean pushed = pushhashtable(htable);
    copyctopstring(name, bs);
    boolean ok = hashlookup(bs, &val, &node);
    if (pushed)
        pophashtable();
    TEST_ASSERT(ok, "Symbol '%s' should exist in %s", name, label);
    TEST_ASSERT(val.valuetype == expected_type, "Symbol '%s' type %d != expected %d", name, val.valuetype, expected_type);
    return true;
}
#endif

#if TEST_MIGRATION_HAS_RUNTIME
static bool copy_file(const char *src, const char *dst) {
    return tm_filecopy_path(src, dst);
}
#endif

/* Create a legacy database on disk (use the real v6 fixture when runtime is available). */
static bool create_legacy_database(void) {
    remove_if_exists(test_legacy_db);
#if TEST_MIGRATION_HAS_RUNTIME
    char fixture_path[PATH_MAX];
    if (!resolve_fixture_path(fixture_path, sizeof fixture_path))
        return false;
    return copy_file(fixture_path, test_legacy_db);
#else
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

    bool ok = fwrite(header, sizeof header, 1, f) == 1;
    fclose(f);
    return ok;
#endif
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
    TEST_ASSERT(new_header.headerLength == (long) sizeof(tydatabaserecord_64), "header length converted");
    TEST_ASSERT(new_header.longversionMajor == 6, "major version converted");
    return true;
}

static bool test_fixture_header_v6(void) {
    char fixture_path[PATH_MAX];
    TEST_ASSERT(resolve_fixture_path(fixture_path, sizeof fixture_path), "resolve fixture path");
    FILE *fp = fopen(fixture_path, "rb");
    TEST_ASSERT(fp != NULL, "fopen fixture v6");
    tydatabaserecord header;
    TEST_ASSERT(fread(&header, sizeof header, 1, fp) == 1, "read v6 header");
    fclose(fp);
    TEST_ASSERT(header.versionnumber == 6, "fixture is v6");
    return true;
}

static bool test_full_migration(void) {
#if !TEST_MIGRATION_HAS_RUNTIME
    TEST_PASS("Skipped: migration runtime not linked in this headless build");
#else
    TEST_ASSERT(create_legacy_database(), "Should create legacy database");
    boolean migrated = false;
    char output_path[1024] = {0};
    TEST_ASSERT(ensure_database_modern(test_legacy_db, &migrated, output_path, sizeof output_path), "Should migrate database");
    TEST_ASSERT(migrated, "Migration should report migrated");

    if (output_path[0] == '\0') {
        TEST_ASSERT(db_format_last_backup_path(output_path, sizeof output_path), "backup path recorded");
    }

    bigstring bs; tyfilespec fs; hdlfilenum fnum = 0;
    copyctopstring(output_path, bs);
    TEST_ASSERT(pathtofilespec(bs, &fs), "pathtofilespec migrated");
    TEST_ASSERT(openfile(&fs, &fnum, true), "openfile migrated");
    {
        tydatabaserecord_64 header;
        TEST_ASSERT(fileread(fnum, sizeof header, &header), "Should read migrated header");
        TEST_ASSERT(header.versionnumber == 7, "Migrated header is version 7");
    }
    closefile(fnum);
    remove_if_exists(output_path);
    db_format_clear_last_backup_path();
    return true;
#endif
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
#if !TEST_MIGRATION_HAS_RUNTIME
    TEST_PASS("Skipped: migration runtime not linked in this headless build");
#else
    TEST_ASSERT(!create_root_backup("does_not_exist.root"), "Backup should fail for missing file");
    TEST_ASSERT(!migrate_32bit_to_64bit("does_not_exist.root"), "Migration should fail for missing file");
    return true;
#endif
}

static bool test_ensure_modern(void) {
#if !TEST_MIGRATION_HAS_RUNTIME
    TEST_PASS("Skipped: migration runtime not linked in this headless build");
#else
    TEST_ASSERT(create_legacy_database(), "Should create legacy database");
    boolean migrated = false;
    char output_path[1024];
    bigstring bs; tyfilespec fs; hdlfilenum fnum = 0;
    TEST_ASSERT(ensure_database_modern(test_legacy_db, &migrated, output_path, sizeof output_path), "ensure_database_modern should succeed");
    TEST_ASSERT(migrated, "Legacy file should migrate");

    /* Verify v7 file was created */
    if (output_path[0] == '\0') {
        TEST_ASSERT(db_format_last_backup_path(output_path, sizeof output_path), "backup path recorded");
    }
    copyctopstring(output_path, bs);
    TEST_ASSERT(pathtofilespec(bs, &fs), "pathtofilespec v7");
    TEST_ASSERT(openfile(&fs, &fnum, true), "openfile v7");
    {
        tydatabaserecord_64 header;
        TEST_ASSERT(fileread(fnum, sizeof header, &header), "Should read v7 header");
        TEST_ASSERT(header.versionnumber == 7, "Header should be v7");
    }
    closefile(fnum);

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
#endif
}

static bool test_fixture_migration(void) {
#if !TEST_MIGRATION_HAS_RUNTIME
    TEST_PASS("Skipped: migration runtime not linked in this headless build");
#else
    /* Copy fixture so we don't mutate the source in-tree. */
    const char *tmp_v6 = "test_fixture_copy.root";
    remove_if_exists(tmp_v6);
    remove_if_exists("test_fixture_copy.root.v7");

    char fixture_path[PATH_MAX];
    TEST_ASSERT(resolve_fixture_path(fixture_path, sizeof fixture_path), "resolve fixture path");
    TEST_ASSERT(copy_file(fixture_path, tmp_v6), "Copy v6 fixture");

    boolean migrated = false;
    char output_path[1024];
    memset(output_path, 0, sizeof output_path);
    TEST_ASSERT(ensure_database_modern(tmp_v6, &migrated, output_path, sizeof output_path), "ensure_database_modern on fixture");
    TEST_ASSERT(migrated, "Fixture should migrate to v7");

    const char *v7_path = (output_path[0] != '\0') ? output_path : tmp_v6;

    bigstring bs; tyfilespec fs; hdlfilenum fnum = 0;
    copyctopstring(v7_path, bs);
    TEST_ASSERT(pathtofilespec(bs, &fs), "pathtofilespec fixture v7");
    TEST_ASSERT(openfile(&fs, &fnum, true), "openfile fixture v7");
    {
        tydatabaserecord_64 header;
        TEST_ASSERT(fileread(fnum, sizeof header, &header), "Read migrated header");
        TEST_ASSERT(header.versionnumber == 7, "Migrated fixture version = 7");
        TEST_ASSERT(header.headerLength > 0, "Migrated header length present");
    }
    closefile(fnum);

    /* Open via dbopenfile and validate key payload types */
    copyctopstring(v7_path, bs);
    TEST_ASSERT(pathtofilespec(bs, &fs), "pathtofilespec(v7)");
    TEST_ASSERT(openfile(&fs, &fnum, false), "openfile(v7)");
    TEST_ASSERT(dbopenfile(fnum, false), "dbopenfile(v7)");
    dbaddress view = nildbaddress;
    dbgetview(cancoonview, &view);
    TEST_ASSERT(view != nildbaddress, "dbgetview(v7)");
    Handle hvar = nil; hdlhashtable hroot = nil;
    TEST_ASSERT(tableloadsystemtable(view, &hvar, &hroot, false), "tableloadsystemtable(v7)");
    /* Enumerate keys for debugging before lookup. */
    fprintf(stderr, "[headless-test] root keys:\n");
    for (short b = 0; b < ctbuckets; ++b) {
        hdlhashnode hnode = (**hroot).hashbucket[b];
        while (hnode != nil) {
            bigstring key;
            gethashkey(hnode, key);
            char keybuf[256];
            bs_to_cstring(key, keybuf, sizeof keybuf);
            fprintf(stderr, "  key=%s type=%d\n", keybuf, (int) (**hnode).val.valuetype);
            hnode = (**hnode).hashlink;
        }
    }
    hdlhashtable htestdata = nil;
    copyctopstring("testData", bs);
    fprintf(stderr, "[headless-test] currenthashtable=%p hroot=%p probe name=%s len=%d\n",
            (void *) currenthashtable,
            (void *) hroot,
            "testData",
            (int) (unsigned char) bs[0]);
    boolean has_testdata = findnamedtable(hroot, bs, &htestdata);
    fprintf(stderr, "[headless-test] findnamedtable returned=%d htestdata=%p\n", (int) has_testdata, (void *) htestdata);
    TEST_ASSERT(has_testdata, "testData table should exist");
    TEST_ASSERT(assert_symbol_type(htestdata, "addressValueGuest", addressvaluetype, "fixture"), "addressValueGuest type");
    TEST_ASSERT(assert_symbol_type(htestdata, "booleanTrue", booleanvaluetype, "fixture"), "booleanTrue type");
    TEST_ASSERT(assert_symbol_type(htestdata, "dateEpoch", datevaluetype, "fixture"), "dateEpoch type");
    TEST_ASSERT(dbclose(), "dbclose(v7)");
    TEST_ASSERT(closefile(fnum), "closefile(v7)");

    remove_if_exists(tmp_v6);
    if (v7_path != tmp_v6)
        remove_if_exists(v7_path);
    db_format_clear_last_backup_path();
    return true;
#endif
}

static bool test_cleanup(void) {
    remove_if_exists(test_legacy_db);
    char backup_path[1024];
    const char *keep_artifacts = getenv("KEEP_MIGRATION_ARTIFACTS");
    if (db_format_last_backup_path(backup_path, sizeof backup_path) && !(keep_artifacts && keep_artifacts[0] != '\0')) {
        remove_if_exists(backup_path);
        db_format_clear_last_backup_path();
    }
    return true;
}

test_case_t migration_tests[] = {
    {"Backup Creation", test_backup_creation},
    {"Header Conversion", test_header_conversion},
    {"Fixture V6 Header", test_fixture_header_v6},
    {"Full Migration", test_full_migration},
    {"Version Detection", test_version_detection},
    {"Error Handling", test_error_handling},
    {"Ensure Modern", test_ensure_modern},
    {"Fixture Migration", test_fixture_migration},
    {"Cleanup", test_cleanup},
};

int migration_test_count = 9;

int main(void) {
    test_init();
    test_setup();
    bool ok = run_test_suite(migration_tests, migration_test_count);
    test_teardown();
    test_summary();
    return ok ? 0 : 1;
}
