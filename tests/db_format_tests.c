/* 2025-11-24 Codex: Add procedural BE goldens + PICT length check. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "frontier.h"
#include "db_format.h"
#include "dbinternal.h"

// 2025-11-20 Codex: Verify modern header serialization uses fixed big-endian encoding for portability.
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

static void test_write_modern_header_big_endian(void) {
    tydatabaserecord_64 header;
    unsigned char encoded[sizeof header];
    const unsigned char expected_availlist[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    const unsigned char expected_view0[] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11};
    const unsigned char expected_view1[] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18};
    const unsigned char expected_view2[] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x00, 0x99};
    const unsigned char expected_header_len[] = {0x10, 0x20, 0x30, 0x40};
    const unsigned char expected_major[] = {0x55, 0x66};
    const unsigned char expected_minor[] = {0x77, 0x88};
    const unsigned char expected_block[] = {0xCA, 0xFE, 0xBA, 0xBE, 0xDE, 0xAD, 0xB0, 0x0C};
    const unsigned char expected_shadow[] = {0x01, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    memset(&header, 0, sizeof header);
    header.systemid = 0x01;
    header.versionnumber = 7;
    header.availlist = 0x0102030405060708ULL;
    header.oldfnumdatabase = (short) 0x1122;
    header.flags = (short) 0x3344;
    header.views[0] = 0x0A0B0C0D0E0F1011ULL;
    header.views[1] = 0x1112131415161718ULL;
    header.views[2] = 0xFFEEDDCCBBAA0099ULL;
    header.releasestack = (Handle) 0xDEADBEEF;
    header.fnumdatabase = 0x13572468;
    header.headerLength = 0x10203040;
    header.longversionMajor = (short) 0x5566;
    header.longversionMinor = (short) 0x7788;
    header.u.extensions.availlistblock = 0xCAFEBABEDEADB00CULL;
    header.u.extensions.availlistshadow = 0x0100AABBCCDDEEFFULL;
    header.u.extensions.flreadonly = true;

    memset(encoded, 0, sizeof encoded);
    assert(db_format_write_header64(&header, encoded, sizeof encoded));

    assert(encoded[0] == header.systemid);
    assert(encoded[1] == header.versionnumber);
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, availlist), expected_availlist, sizeof expected_availlist) == 0);
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, oldfnumdatabase), "\x11\x22", 2) == 0);
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, flags), "\x33\x44", 2) == 0);
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, views[0]), expected_view0, sizeof expected_view0) == 0);
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, views[1]), expected_view1, sizeof expected_view1) == 0);
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, views[2]), expected_view2, sizeof expected_view2) == 0);
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, headerLength), expected_header_len, sizeof expected_header_len) == 0);
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, longversionMajor), expected_major, sizeof expected_major) == 0);
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, longversionMinor), expected_minor, sizeof expected_minor) == 0);
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, u.extensions.availlistblock), expected_block, sizeof expected_block) == 0);
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, u.extensions.availlistshadow), expected_shadow, sizeof expected_shadow) == 0);
    assert(encoded[offsetof(tydatabaserecord_64, u.extensions.flreadonly)] == 1);

    /* Runtime-only fields must be zeroed on disk */
    for (size_t i = 0; i < sizeof(Handle); ++i)
        assert(encoded[offsetof(tydatabaserecord_64, releasestack) + i] == 0);
    for (size_t i = 0; i < sizeof(long); ++i)
        assert(encoded[offsetof(tydatabaserecord_64, fnumdatabase) + i] == 0);
}

static void test_large_free_block_be64(void) {
    /* Simulate a free block >4GB to ensure size/link encode to BE64 without truncation. */
    use_64bit_format = true;

    const uint64_t data_bytes = (1ULL << 33) + 0x1234ULL; /* 8GB+ */
    const uint64_t freeflag = 0x8000000000000000ULL;
    const uint64_t raw_size = data_bytes | freeflag;
    const dbaddress next_link = (dbaddress) 0x0FEDCBA987654321ULL;

    unsigned char header[sizeheader_v7];
    unsigned char trailer[sizetrailer_v7];
    unsigned char link_bytes[sizeof(uint64_t)];
    memset(header, 0, sizeof header);
    memset(trailer, 0, sizeof trailer);
    memset(link_bytes, 0, sizeof link_bytes);

    db_format_write_be64(header + offsetof(tyheader64, sizefreeword) + offsetof(tysizefreeword64, size), raw_size);
    db_format_write_be64(trailer + offsetof(tytrailer64, sizefreeword) + offsetof(tysizefreeword64, size), raw_size);
    db_format_write_be64(link_bytes, (uint64_t) next_link);

    assert(db_format_read_be64(header + offsetof(tyheader64, sizefreeword) + offsetof(tysizefreeword64, size)) == raw_size);
    assert(db_format_read_be64(trailer + offsetof(tytrailer64, sizefreeword) + offsetof(tysizefreeword64, size)) == raw_size);
    assert(db_format_read_be64(link_bytes) == (uint64_t) next_link);

    uint64_t parsed_size = db_format_read_be64(header + offsetof(tyheader64, sizefreeword) + offsetof(tysizefreeword64, size)) & 0x7FFFFFFFFFFFFFFFULL;
    assert(parsed_size == data_bytes);

    use_64bit_format = false; /* leave global in legacy mode for other tests */
}

static void test_pict_length_be32(void) {
    /* PICT packer writes length with BE32 while leaving payload opaque. */
    const uint32_t pict_len = 0xA1B2C3D4u;
    unsigned char encoded[4];

    db_format_write_be32(encoded, pict_len);
    assert(encoded[0] == 0xA1);
    assert(encoded[1] == 0xB2);
    assert(encoded[2] == 0xC3);
    assert(encoded[3] == 0xD4);
    assert(db_format_read_be32(encoded) == pict_len);
}

static void test_header_version_and_loader_switch(void) {
    const size_t max_header = (sizeof(tydatabaserecord) > sizeof(tydatabaserecord_64) ? sizeof(tydatabaserecord) : sizeof(tydatabaserecord_64));
    unsigned char legacy_raw[max_header];
    unsigned char modern_raw[max_header];
    unsigned char truncated_legacy[sizeof(tydatabaserecord_64)]; /* smaller than legacy header */
    int version = 0;
    boolean header_is_modern = false;
    tydatabaserecord decoded;
    boolean prev_use64 = use_64bit_format;

    memset(&decoded, 0, sizeof decoded);
    memset(legacy_raw, 0, sizeof legacy_raw);
    legacy_raw[1] = 6; /* legacy v6 */
    assert(db_format_header_version(legacy_raw, sizeof legacy_raw, &version));
    assert(version == 6);
    assert(db_format_decode_header(legacy_raw, sizeof legacy_raw, &header_is_modern, &decoded));
    assert(!header_is_modern);
    assert(db_format_load_legacy_adapter(&decoded, true));
    assert(use_64bit_format == false);

    /* Decode should fail if buffer is smaller than legacy header size. */
    memset(truncated_legacy, 0, sizeof truncated_legacy);
    truncated_legacy[1] = 6;
    header_is_modern = false;
    memset(&decoded, 0, sizeof decoded);
    assert(!db_format_decode_header(truncated_legacy, sizeof truncated_legacy, &header_is_modern, &decoded));

    memset(&decoded, 0, sizeof decoded);
    memset(modern_raw, 0, sizeof modern_raw);
    modern_raw[1] = 7; /* modern v7 */
    header_is_modern = false;
    assert(db_format_header_version(modern_raw, sizeof modern_raw, &version));
    assert(version == 7);
    assert(db_format_decode_header(modern_raw, sizeof modern_raw, &header_is_modern, &decoded));
    assert(header_is_modern);
    assert(db_format_load_v7_reader(&decoded, true));
    assert(use_64bit_format == true);

    use_64bit_format = prev_use64;
}

static void test_procedural_v7_golden_header_and_avail(void) {
    /*
     * Procedural golden for header + avail: fixed byte expectations to ensure BE encoding is stable
     * regardless of host endianness. Future cross-arch runs can rely on these expectations.
     */
    tydatabaserecord_64 header;
    unsigned char encoded[sizeof header];

    memset(&header, 0, sizeof header);
    header.systemid = 0x01;
    header.versionnumber = 7;
    header.availlist = 0x0000000012345678ULL;
    header.oldfnumdatabase = (short) 0x0102;
    header.flags = (short) 0x0304;
    header.views[0] = 0x0A0B0C0D0E0F1011ULL;
    header.views[1] = 0x1112131415161718ULL;
    header.views[2] = 0xFFEEDDCCBBAA0099ULL;
    header.releasestack = (Handle) 0xDEADBEEF;
    header.fnumdatabase = 0xCAFEBABE;
    header.headerLength = (long) sizeof(tydatabaserecord_64);
    header.longversionMajor = (short) 0x1122;
    header.longversionMinor = (short) 0x3344;
    header.u.extensions.availlistblock = 0x0000000001020304ULL;
    header.u.extensions.availlistshadow = nildbaddress;
    header.u.extensions.flreadonly = false;

    memset(encoded, 0, sizeof encoded);
    assert(db_format_write_header64(&header, encoded, sizeof encoded));

    /* Expected big-endian bytes for a subset of fields */
    const unsigned char expected[] = {
        0x01, 0x07,                         /* systemid, versionnumber */
        /* availlist */ 0x00, 0x00, 0x00, 0x00, 0x12, 0x34, 0x56, 0x78,
        /* oldfnumdatabase, flags */ 0x01, 0x02, 0x03, 0x04,
        /* views[0] */ 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11,
        /* views[1] */ 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        /* views[2] */ 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x00, 0x99,
    };
    assert(memcmp(encoded, expected, sizeof expected) == 0);

    /* headerLength, version fields, and availlistblock */
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, headerLength),
                  "\x00\x00\x00\x60", 4) == 0); /* sizeof(tydatabaserecord_64) is 96 on this build */
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, longversionMajor),
                  "\x11\x22", 2) == 0);
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, longversionMinor),
                  "\x33\x44", 2) == 0);
    assert(memcmp(encoded + offsetof(tydatabaserecord_64, u.extensions.availlistblock),
                  "\x00\x00\x00\x00\x01\x02\x03\x04", 8) == 0);

    /* Runtime-only fields must be zeroed */
    for (size_t i = 0; i < sizeof(Handle); ++i)
        assert(encoded[offsetof(tydatabaserecord_64, releasestack) + i] == 0);
    for (size_t i = 0; i < sizeof(long); ++i)
        assert(encoded[offsetof(tydatabaserecord_64, fnumdatabase) + i] == 0);

    /* Avail list free block (simulate a single free node) */
    use_64bit_format = true;
    const uint64_t freeflag = 0x8000000000000000ULL;
    const uint64_t avail_size = 0x0000000011111111ULL | freeflag;
    unsigned char avail_header[sizeheader_v7];
    memset(avail_header, 0, sizeof avail_header);
    db_format_write_be64(avail_header + offsetof(tyheader64, sizefreeword) + offsetof(tysizefreeword64, size), avail_size);
    assert(db_format_read_be64(avail_header + offsetof(tyheader64, sizefreeword) + offsetof(tysizefreeword64, size)) == avail_size);
    use_64bit_format = false;
}
int main(void) {
    test_detect_legacy_database();
    test_detect_modern_database();
    test_convert_header();
    test_write_modern_header_big_endian();
    test_large_free_block_be64();
    test_pict_length_be32();
    test_header_version_and_loader_switch();
    test_procedural_v7_golden_header_and_avail();
    printf("db_format_tests: all checks passed\n");
    return 0;
}
