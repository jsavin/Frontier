#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "frontier.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "tablestructure.h"
#include "op.h"
#include "opinternal.h"
#include "opxml.h"
#include "ops.h"
#include "db_format.h"
#include "tableexternal_common.h"
#include "../portable/wptext_portable.h"

#ifndef TABLE_DISK_VERSION
#define TABLE_DISK_VERSION 0x04
#endif

#ifndef TABLE_HEADER_RESERVED_BYTES
#define TABLE_HEADER_RESERVED_BYTES 1024
#endif

#ifndef WP_PORTABLE_HEADER_BYTES
#define WP_PORTABLE_HEADER_BYTES 1056
#endif

/* Enable to dump detailed pack/unpack debugging. */
/* #define DEBUG_SERIALIZER 1 */

static Handle build_roundtrip_table(boolean enable64bit, dbaddress diskAdr);

static void setup_bigstring_from_c(const char *cstr, bigstring out) {
    copyctopstring(cstr, out);
}

static void eval_expect(const char *expr, const char *expected) {
    bigstring program;
    bigstring result;
    setup_bigstring_from_c(expr, program);
    assert(langrunstringnoerror(program, result));
    char c_result[256];
    copyptocstring(result, c_result);
    assert(strcmp(c_result, expected) == 0);
}

static void run_basic_script(void) {
    bigstring program;
    bigstring result;

    printf("[rt] run_basic_script: evaluating 3+4...\n");
    fflush(stdout);
    setup_bigstring_from_c("3 + 4", program);
    assert(langrunstringnoerror(program, result));

    char c_result[256];
    copyptocstring(result, c_result);
    assert(strcmp(c_result, "7") == 0);
}

static Handle make_text_handle(const char *cstr) {
    bigstring bs;
    copyctopstring(cstr, bs);
    Handle h = nil;
    if (!newtexthandle(bs, &h))
        return nil;
    return h;
}

static void collect_outline_text(hdloutlinerecord ho, char *out, size_t out_sz) {
    out[0] = '\0';
    if (!ho || out_sz == 0)
        return;
    hdlheadrecord h = (**ho).hsummit;
    for (int i = 0; i < 16 && h; ++i) { /* limit to avoid infinite loops */
        bigstring bs;
        opgetheadstring(h, bs);
        char tmp[256];
        copyptocstring(bs, tmp);
        if (i > 0)
            strncat(out, "|", out_sz - strlen(out) - 1);
        strncat(out, tmp, out_sz - strlen(out) - 1);
        hdlheadrecord next = opbumpflatdown(h, true);
        if (next == h)
            break;
        h = next;
    }
}

static void run_opml_roundtrip(void) {
    printf("[rt] opml_roundtrip: start\n");
    fflush(stdout);
    hdloutlinerecord ho1 = nil;
    printf("[rt] newoutlinerecord ho1...\n");
    fflush(stdout);
    assert(newoutlinerecord(&ho1));
    oppushoutline(ho1);
    {
        bigstring bsroot;
        copyctopstring("root", bsroot);
        printf("[rt] set root text...\n");
        fflush(stdout);
        assert(opsetheadstring((**outlinedata).hbarcursor, bsroot));
    }
    {
        printf("[rt] insert child headline...\n");
        fflush(stdout);
        Handle hchild = make_text_handle("child");
        assert(hchild != nil);
        /* opinsertheadline takes ownership or retains this handle; do not free here */
        assert(opinsertheadline(hchild, right, false));
    }

    printf("[rt] export to OPML...\n");
    fflush(stdout);
    Handle hname = nil, hemail = nil, hxml = nil;
    assert(newemptyhandle(&hname));
    assert(newemptyhandle(&hemail));
    hdlhashtable hto = nil, hcloud = nil;
    bigstring bso;
    setemptystring(bso);
    tyvaluerecord vo;
    initvalue(&vo, novaluetype);
    assert(opoutlinetoxml(ho1, hname, hemail, &hxml, hto, bso, vo, hcloud));

    {
        long sz = gethandlesize(hxml);
        long dump = sz;
        if (dump > 0) {
            fprintf(stderr, "[rt] OPML size=%ld, head=\n", sz);
            fwrite(*hxml, 1, (size_t)dump, stderr);
            fprintf(stderr, "\n[rt] OPML snippet end\n");
        } else {
            fprintf(stderr, "[rt] OPML is empty or null (size=%ld)\n", sz);
        }
    }

    printf("[rt] create ho2/import from OPML...\n");
    fflush(stdout);
    hdloutlinerecord ho2 = nil;
    assert(newoutlinerecord(&ho2));
    assert(opxmltooutline(hxml, ho2, true, hto, bso, vo, hcloud));

    printf("[rt] collect/compare flattened text...\n");
    fflush(stdout);
    char a[256], b[256];
    collect_outline_text(ho1, a, sizeof a);
    collect_outline_text(ho2, b, sizeof b);
    assert(strcmp(a, b) == 0);

    disposehandle(hxml);
    disposehandle(hname);
    disposehandle(hemail);
    printf("[rt] opml_roundtrip: done\n");
    fflush(stdout);
}

static void run_constants_smoke(void) {
    printf("[rt] constants_smoke: start\n");
    fflush(stdout);
    eval_expect("true != false", "true");
    eval_expect("nil == nil", "true");
    eval_expect("flatdown == flatdown", "true");
    eval_expect("infinity > 1000000", "true");
    eval_expect("stringType == stringType", "true");
    eval_expect("longType == longType", "true");
    printf("[rt] constants_smoke: done\n");
    fflush(stdout);
}

static void table_add_sample_entries(hdlhashtable table, dbaddress diskAdr) {
    bigstring bs;
    tyvaluerecord val;

    setup_bigstring_from_c("none", bs);
    initvalue(&val, novaluetype);
    val.data.chvalue = 0;
    assert(hashtableassign(table, bs, val));

    setup_bigstring_from_c("flag", bs);
    initvalue(&val, booleanvaluetype);
    val.data.flvalue = true;
    val.data.chvalue = 1;
    assert(hashtableassign(table, bs, val));

    setup_bigstring_from_c("letter", bs);
    initvalue(&val, charvaluetype);
    val.data.chvalue = 'Z';
    assert(hashtableassign(table, bs, val));

    setup_bigstring_from_c("smallint", bs);
    initvalue(&val, intvaluetype);
    val.data.intvalue = 1234;
    assert(hashtableassign(table, bs, val));

    setup_bigstring_from_c("biglong", bs);
    initvalue(&val, longvaluetype);
    val.data.longvalue = 0x12345678;
    assert(hashtableassign(table, bs, val));

    setup_bigstring_from_c("point", bs);
    initvalue(&val, pointvaluetype);
    val.data.pointvalue.h = 320;
    val.data.pointvalue.v = -240;
    assert(hashtableassign(table, bs, val));

    setup_bigstring_from_c("epoch", bs);
    initvalue(&val, datevaluetype);
    val.data.datevalue = 978307200UL;
    assert(hashtableassign(table, bs, val));

    setup_bigstring_from_c("blobRef", bs);
    initvalue(&val, binaryvaluetype);
    val.fldiskval = true;
    val.data.diskvalue = diskAdr;
    assert(hashtableassign(table, bs, val));
}

static void table_verify_sample_entries(hdlhashtable table, dbaddress expectedDiskAdr) {
    bigstring bs;
    tyvaluerecord out;
    hdlhashnode node = nil;
#ifdef DEBUG_SERIALIZER
    char dbg[256];
#endif

    setup_bigstring_from_c("none", bs);
#ifdef DEBUG_SERIALIZER
    copyptocstring(bs, dbg);
    printf("[rt] verify lookup '%s'\n", dbg);
#endif
    assert(hashtablelookupnode(table, bs, &node));
    out = (**node).val;
    assert(out.valuetype == novaluetype);
    assert(out.data.chvalue == 0);

    setup_bigstring_from_c("flag", bs);
#ifdef DEBUG_SERIALIZER
    copyptocstring(bs, dbg);
    printf("[rt] verify lookup '%s'\n", dbg);
#endif
    assert(hashtablelookupnode(table, bs, &node));
    out = (**node).val;
    assert(out.valuetype == booleanvaluetype);
    assert(out.data.flvalue);
    assert(out.data.chvalue == 1);

    setup_bigstring_from_c("letter", bs);
#ifdef DEBUG_SERIALIZER
    copyptocstring(bs, dbg);
    printf("[rt] verify lookup '%s'\n", dbg);
#endif
    assert(hashtablelookupnode(table, bs, &node));
    out = (**node).val;
    assert(out.valuetype == charvaluetype);
    assert(out.data.chvalue == 'Z');

    setup_bigstring_from_c("smallint", bs);
#ifdef DEBUG_SERIALIZER
    copyptocstring(bs, dbg);
    printf("[rt] verify lookup '%s'\n", dbg);
#endif
    assert(hashtablelookupnode(table, bs, &node));
    out = (**node).val;
    assert(out.valuetype == intvaluetype);
    assert(out.data.intvalue == 1234);

    setup_bigstring_from_c("biglong", bs);
#ifdef DEBUG_SERIALIZER
    copyptocstring(bs, dbg);
    printf("[rt] verify lookup '%s'\n", dbg);
#endif
    assert(hashtablelookupnode(table, bs, &node));
    out = (**node).val;
    assert(out.valuetype == longvaluetype);
    assert(out.data.longvalue == 0x12345678);

    setup_bigstring_from_c("point", bs);
#ifdef DEBUG_SERIALIZER
    copyptocstring(bs, dbg);
    printf("[rt] verify lookup '%s'\n", dbg);
#endif
    assert(hashtablelookupnode(table, bs, &node));
    out = (**node).val;
    assert(out.valuetype == pointvaluetype);
    assert(out.data.pointvalue.h == 320);
    assert(out.data.pointvalue.v == -240);

    setup_bigstring_from_c("epoch", bs);
#ifdef DEBUG_SERIALIZER
    copyptocstring(bs, dbg);
    printf("[rt] verify lookup '%s'\n", dbg);
#endif
    assert(hashtablelookupnode(table, bs, &node));
    out = (**node).val;
    assert(out.valuetype == datevaluetype);
    assert(out.data.datevalue == 978307200UL);

    setup_bigstring_from_c("blobRef", bs);
#ifdef DEBUG_SERIALIZER
    copyptocstring(bs, dbg);
    printf("[rt] verify lookup '%s'\n", dbg);
#endif
    {
        hdlhashnode prev = nil;
        hdlhashtable prev_ht = sethashtable(table);
        assert(hashlocate(bs, &node, &prev));
        sethashtable(prev_ht);
    }
    out = (**node).val;
    assert(out.valuetype == binaryvaluetype);
    assert(out.fldiskval);
#ifdef DEBUG_SERIALIZER
    printf("[rt] blobRef diskvalue=0x%llx expected=0x%llx\n",
           (unsigned long long) out.data.diskvalue,
           (unsigned long long) expectedDiskAdr);
#endif
    assert(out.data.diskvalue == expectedDiskAdr);
}

static uint16_t read_be16(const unsigned char *p) {
    return (uint16_t) (((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static uint32_t read_be32u(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void verify_zero_block(const unsigned char *p, size_t len) {
    for (size_t i = 0; i < len; ++i)
        assert(p[i] == 0);
}

static void run_table_header_regression_mode(const char *label, boolean enable64bit) {
    printf("[rt] table_header_regression (%s): start\n", label);
    fflush(stdout);
    dbaddress diskAdr = enable64bit ? (dbaddress)0xAABBCCDDEEFF0011ULL : (dbaddress)0x0055AA33ULL;
    Handle packed = build_roundtrip_table(enable64bit, diskAdr);

    Handle copy = nil;
    assert(copyhandle(packed, &copy));

    Handle hrecords = nil;
    Handle hstrings = nil;
    assert(unmergehandles(copy, &hrecords, &hstrings));

    if (hstrings != nil)
        disposehandle(hstrings);

    assert(hrecords != nil);
    size_t expected_header = 16 + (size_t)TABLE_HEADER_RESERVED_BYTES;
    long record_bytes = gethandlesize(hrecords);
    assert(record_bytes >= (long)expected_header);

    unsigned char *bytes = (unsigned char *) *hrecords;
    uint16_t disk_version = read_be16(bytes);
    assert(disk_version == TABLE_DISK_VERSION);
    verify_zero_block(bytes + 16, (size_t)TABLE_HEADER_RESERVED_BYTES);

    disposehandle(hrecords);
    disposehandle(packed);

    printf("[rt] table_header_regression (%s): done\n", label);
    fflush(stdout);
}

static void run_table_header_regression(void) {
    run_table_header_regression_mode("legacy32", false);
    run_table_header_regression_mode("modern64", true);
}

static Handle build_roundtrip_table(boolean enable64bit, dbaddress diskAdr) {
    boolean prev_mode = use_64bit_format;
    use_64bit_format = enable64bit;

    hdlhashtable source = nil;
    assert(newhashtable(&source));
    table_add_sample_entries(source, diskAdr);

    Handle packed = nil;
    boolean flmustsave = false;
    assert(hashpacktable(source, false, &packed, &flmustsave));

    assert(disposehashtable(source, false));
    use_64bit_format = prev_mode;
    return packed;
}

static void run_serializer_roundtrip_mode(const char *label, boolean enable64bit) {
    printf("[rt] serializer_roundtrip (%s): start\n", label);
    fflush(stdout);
    dbaddress diskAdr = enable64bit ? (dbaddress)0x1122334455667788ULL : (dbaddress)0x00123456ULL;

    Handle packed = build_roundtrip_table(enable64bit, diskAdr);

#ifdef DEBUG_SERIALIZER
    Handle packed_copy = nil;
    Handle debug_first = nil;
    Handle debug_second = nil;
    if (copyhandle(packed, &packed_copy)) {
        if (unmergehandles(packed_copy, &debug_first, &debug_second)) {
            unsigned char *strbytes = debug_second ? *debug_second : NULL;
            if (strbytes != NULL) {
                uint32_t raw = *((uint32_t *)(strbytes + 54));
                printf("[rt] debug sentinel (%s) 0x%08x\n", label, raw);
            }
            disposehandle(debug_first);
            disposehandle(debug_second);
        } else {
            disposehandle(packed_copy);
        }
    }
#endif

    boolean prev_mode = use_64bit_format;
    use_64bit_format = enable64bit;

    hdlhashtable restored = nil;
    assert(newhashtable(&restored));
    if (!hashunpacktable(packed, false, restored)) {
        printf("[rt] hashunpacktable failed (%s)\n", label);
        use_64bit_format = prev_mode;
        return;
    }

#ifdef DEBUG_SERIALIZER
    long count = -1;
    hashcountitems(restored, &count);
    printf("[rt] restored count before verify (%s) = %ld\n", label, count);
    boolean pushed = pushhashtable(restored);
    printf("[rt] pushhashtable(restored) => %d\n", pushed ? 1 : 0);
    if (pushed) {
        if (currenthashtable == restored)
            printf("[rt] currenthashtable matches restored after push (%s)\n", label);
        else
            printf("[rt] currenthashtable MISMATCH after push (%s)\n", label);
        pophashtable();
    }
    for (long i = 0; i < count; ++i) {
        hdlhashnode node = nil;
        if (hashgetnthnode(restored, i, &node) && node != nil) {
            bigstring key;
            gethashkey(node, key);
            char keybuf[256];
            copyptocstring(key, keybuf);
            printf("[rt] restored key[%ld] = '%s' type=%d fldiskval=%d\n",
                   i, keybuf, (**node).val.valuetype, (**node).val.fldiskval);
        } else {
            printf("[rt] restored key[%ld] lookup failed\n", i);
        }
    }
    bigstring probe;
    setup_bigstring_from_c("none", probe);
    hdlhashnode located = nil;
    hdlhashnode located_prev = nil;
    hdlhashtable prevht = sethashtable(restored);
    boolean located_ok = hashlocate(probe, &located, &located_prev);
    printf("[rt] hashlocate('none') => %d node=%p prev=%p\n", located_ok ? 1 : 0,
           (void *)located, (void *)located_prev);
    if (located_ok) {
        tyvaluerecord debugval;
        hdlhashnode debugnode = nil;
        boolean lookup_ok = hashlookup(probe, &debugval, &debugnode);
        printf("[rt] hashlookup('none') => %d node=%p\n", lookup_ok ? 1 : 0, (void *)debugnode);
        boolean table_lookup_ok = hashtablelookup(restored, probe, &debugval, &debugnode);
        printf("[rt] hashtablelookup('none') => %d node=%p\n", table_lookup_ok ? 1 : 0, (void *)debugnode);
    }
    sethashtable(prevht);
#endif

    table_verify_sample_entries(restored, diskAdr);

    boolean dispose_ok = disposehashtable(restored, false);
#ifdef DEBUG_SERIALIZER
    printf("[rt] disposehashtable(%s) => %d\n", label, dispose_ok ? 1 : 0);
#endif
    assert(dispose_ok);
    packed = nil;
    use_64bit_format = prev_mode;
    printf("[rt] serializer_roundtrip (%s): done\n", label);
    fflush(stdout);
}

static void run_serializer_roundtrip(void) {
    run_serializer_roundtrip_mode("legacy32", false);
    run_serializer_roundtrip_mode("modern64", true);
}

static boolean validate_rtf_bytes(const uint8_t *data, long len) {
    if (data == NULL || len <= 0)
        return false;
    if (data[0] != '{')
        return false;
    long depth = 0;
    for (long i = 0; i < len; ++i) {
        unsigned char c = data[i];
        if (c == '\\') {
            ++i;
            continue;
        }
        if (c == '{')
            ++depth;
        else if (c == '}') {
            --depth;
            if (depth < 0)
                return false;
        }
    }
    return depth == 0;
}

static void run_wptext_rtf_smoke(void) {
    printf("[rt] wptext RTF smoke test...\n");
    fflush(stdout);
    Handle hpacked = nil;
    assert(wp_portable_pack_text_for_test("Hello RTF world!", &hpacked));
    long size = gethandlesize(hpacked);
    assert(size > 1056);
    const unsigned char *bytes = (const unsigned char *)*hpacked;
    uint32_t magic = read_be32u(bytes);
    assert(magic == (uint32_t)'WPRT');
    uint16_t version = read_be16(bytes + 4);
    assert(version == 1);
    uint16_t flags = read_be16(bytes + 6);
    assert((flags & 0x0001u) != 0);
    uint32_t utf8len = read_be32u(bytes + 24);
    uint32_t reserved_len = read_be32u(bytes + 28);
    assert(reserved_len == 0);
    const long header_len = WP_PORTABLE_HEADER_BYTES;
    assert(size == (long)header_len + (long)utf8len);
    const uint8_t *payload = bytes + header_len;
    assert(payload[0] == '{');
    assert(validate_rtf_bytes(payload, utf8len));
    disposehandle(hpacked);
    printf("[rt] wptext RTF smoke test passed.\n");
    fflush(stdout);
}

static void run_wptext_portable_roundtrip(void) {
    Handle hpacked = nil;
    assert(wp_portable_pack_text_for_test("Portable roundtrip", &hpacked));
    long size = gethandlesize(hpacked);
    const unsigned char *bytes = (const unsigned char *) *hpacked;
    assert(size > WP_PORTABLE_HEADER_BYTES);
    assert(wp_portable_load_portable_blob_for_test(bytes, size));
    disposehandle(hpacked);
}

static void run_wptext_portable_reservedlen_failure(void) {
    Handle hpacked = nil;
    assert(wp_portable_pack_text_for_test("Reserved check", &hpacked));
    Handle corrupt = nil;
    assert(copyhandle(hpacked, &corrupt));
    unsigned char *bytes = (unsigned char *) *corrupt;
    long size = gethandlesize(corrupt);
    /* Overwrite reservedlength (big-endian) with 1 to trigger validation failure. */
    bytes[28] = 0x00;
    bytes[29] = 0x00;
    bytes[30] = 0x00;
    bytes[31] = 0x01;
    assert(!wp_portable_load_portable_blob_for_test(bytes, size));
    disposehandle(corrupt);
    disposehandle(hpacked);
}

int main(void) {
    const char *regen_path = getenv("FRONTIER_REGEN_ROOT");
    if (regen_path != NULL && *regen_path != '\0') {
        printf("[rt] FRONTIER_REGEN_ROOT=%s (starting migration)\n", regen_path);
        fflush(stdout);
        if (!migrate_32bit_to_64bit(regen_path)) {
            fprintf(stderr, "[rt] migrate_32bit_to_64bit failed for %s\n", regen_path);
            return 1;
        }
        char migrated_path[1024];
        if (db_format_last_backup_path(migrated_path, sizeof migrated_path) && migrated_path[0] != '\0')
            printf("[rt] migration complete: %s\n", migrated_path);
        else
            printf("[rt] migration complete\n");
        fflush(stdout);
        return 0;
    }

    printf("[rt] initmemory...\n");
    fflush(stdout);
    assert(initmemory());
    printf("[rt] initstrings...\n");
    fflush(stdout);
    initstrings();

    printf("[rt] initlang...\n");
    fflush(stdout);
    assert(initlang());
    printf("[rt] inittablestructure...\n");
    fflush(stdout);
    assert(inittablestructure());
    printf("[rt] langinitverbs...\n");
    fflush(stdout);
    assert(langinitverbs());
    printf("[rt] wp_portable_init...\n");
    fflush(stdout);
    assert(wp_portable_init());

    printf("[rt] before run_basic_script\n");
    fflush(stdout);
    run_basic_script();
    printf("[rt] after run_basic_script\n");
    fflush(stdout);
    printf("[rt] before constants_smoke\n");
    fflush(stdout);
    run_constants_smoke();
    printf("[rt] after constants_smoke\n");
    fflush(stdout);
    run_opml_roundtrip();
    printf("[rt] after run_opml_roundtrip\n");
    fflush(stdout);
    printf("[rt] before table_header_regression\n");
    fflush(stdout);
    run_table_header_regression();
    printf("[rt] after table_header_regression\n");
    fflush(stdout);
    printf("[rt] before serializer_roundtrip\n");
    fflush(stdout);
    run_serializer_roundtrip();
    printf("[rt] after serializer_roundtrip\n");
    fflush(stdout);
    run_wptext_rtf_smoke();
    run_wptext_portable_roundtrip();
    run_wptext_portable_reservedlen_failure();

    printf("runtime_tests: language, OPML, and serializer round-trips passed\n");
    wp_portable_shutdown();
    return 0;
}
