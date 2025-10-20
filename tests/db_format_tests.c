#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "db_format.h"

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

static void reset_use_64bit_format(void) {
    use_64bit_format = false;
}

static void test_detect_legacy_database(void) {
    reset_use_64bit_format();

    tydatabaserecord legacy = {0};
    legacy.versionnumber = 6;

    assert(detect_database_format(&legacy));
    assert(!use_64bit_format);
}

static void test_detect_modern_database(void) {
    reset_use_64bit_format();

    tydatabaserecord modern = {0};
    modern.versionnumber = 7;

    assert(detect_database_format(&modern));
    assert(use_64bit_format);
}

static void test_convert_header(void) {
    unsigned char old_header[LEGACY_DB_HEADER_BYTES];
    memset(old_header, 0, sizeof old_header);
    old_header[0] = 1;
    old_header[1] = 6;
    old_header[6] = 0;
    old_header[7] = 42;
    old_header[8] = 0x55;
    old_header[9] = 0xAA;
    write_legacy_dbaddress(old_header + legacy_view_base + 0 * legacy_view_stride, 0x01020304u);
    write_legacy_dbaddress(old_header + legacy_view_base + 1 * legacy_view_stride, 0x05060708u);
    write_legacy_dbaddress(old_header + legacy_view_base + 2 * legacy_view_stride, 0x090A0B0Cu);
    uint32_t header_len = (uint32_t) sizeof old_header;
    old_header[30] = (unsigned char)((header_len >> 24) & 0xFF);
    old_header[31] = (unsigned char)((header_len >> 16) & 0xFF);
    old_header[32] = (unsigned char)((header_len >> 8) & 0xFF);
    old_header[33] = (unsigned char)(header_len & 0xFF);
    old_header[34] = 0x00;
    old_header[35] = 0x06;
    old_header[36] = 0x00;
    old_header[37] = 0x01;
    old_header[38] = 0x0B;
    old_header[39] = 0xAD;
    old_header[40] = 0xF0;
    old_header[41] = 0x0D;

    tydatabaserecord_64 new_header;
    memset(&new_header, 0, sizeof new_header);

    assert(convert_32bit_header_to_64bit(old_header, &new_header));

    assert(new_header.systemid == old_header[0]);
    assert(new_header.versionnumber == 7);
    assert(new_header.availlist == read_legacy_dbaddress_test(old_header + 2));
    assert(new_header.oldfnumdatabase == 42);
    assert(new_header.flags == 0x55AA);
    assert(new_header.views[0] == read_legacy_dbaddress_test(old_header + legacy_view_base + 0 * legacy_view_stride));
    assert(new_header.views[1] == read_legacy_dbaddress_test(old_header + legacy_view_base + 1 * legacy_view_stride));
    assert(new_header.views[2] == read_legacy_dbaddress_test(old_header + legacy_view_base + 2 * legacy_view_stride));
    assert(new_header.releasestack == 0);
    assert(new_header.fnumdatabase == 0);
    assert(new_header.headerLength == (long) sizeof(tydatabaserecord_64));
    assert(new_header.longversionMajor == 6);
    assert(new_header.longversionMinor == 1);
    assert(new_header.u.extensions.availlistblock == 0x0BADF00D);
    assert(new_header.u.extensions.availlistshadow == nildbaddress);
    assert(!new_header.u.extensions.flreadonly);
    for (size_t i = 0; i < sizeof new_header.u.extensions.reserved; ++i)
        assert(new_header.u.extensions.reserved[i] == 0);
}

int main(void) {
    test_detect_legacy_database();
    test_detect_modern_database();
    test_convert_header();
    printf("db_format_tests: all checks passed\n");
    return 0;
}
