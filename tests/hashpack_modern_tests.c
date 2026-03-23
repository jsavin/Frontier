#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "frontier.h"
#include "standard.h"
#include "lang.h"
#include "langhash_test.h"
#include "memory.h"
#include "db_format.h"
#include "test_report.h"

/* 2025-12-06 Codex: Modern hashpack BE64 scalar round-trips. */

static void init_valuerecord(tyvaluerecord *v, tyvaluetype t) {
    memset(v, 0, sizeof(*v));
    v->valuetype = t;
}

static void setup_mode_modern(void) {
    db_format_mode mode = { true, false };
    db_format_mode_push(&mode);
}

static void teardown_mode(void) {
    db_format_mode_pop();
}

/* Minimal mirror of modern disk symbol for byte checks (matches langhash.c). */
/* Helper to pack/unpack a single scalar via modern value converters (no handles). */
static void pack_value(const tyvaluerecord *vin, langhash_test_disksymbolrecord_v7 *rec) {
    langhash_test_value_to_disk_v7(vin, rec);
}

static void unpack_value(const langhash_test_disksymbolrecord_v7 *rec, tyvaluerecord *vout) {
    memset(vout, 0, sizeof(*vout));
    langhash_test_value_from_disk_v7(rec, vout);
}

static void test_be64_int_roundtrip(void) {
    printf("[hashpack] int64 BE roundtrip... ");
    setup_mode_modern();

    bigstring bsname = "\004test";
    tyvaluerecord vin; init_valuerecord(&vin, longvaluetype);
    vin.data.longvalue = 0x0123456789ABCDEFull;

    langhash_test_disksymbolrecord_v7 rec;
    pack_value(&vin, &rec);

    /* Verify on-disk bytes are BE64 */
    unsigned char *recbytes = (unsigned char *)&rec;
    /* ixkey big-endian index at offset 0; skip to payload */
    /* Payload starts at offset 8 in tydisksymbolrecord_v7 (ixkey + valuetype + version + pad) */
    size_t payload_off = 8;
    uint64_t stored = db_format_read_be64(recbytes + payload_off);
    assert(stored == (uint64_t)vin.data.longvalue);

    tyvaluerecord vout; bigstring bsout;
    unpack_value(&rec, &vout);
    assert(vout.valuetype == longvaluetype);
    assert(vout.data.longvalue == vin.data.longvalue);

    teardown_mode();
    printf("PASS\n");
}

static void test_be64_date_roundtrip(void) {
    printf("[hashpack] date BE roundtrip... ");
    setup_mode_modern();

    bigstring bsname = "\004date";
    tyvaluerecord vin; init_valuerecord(&vin, datevaluetype);
    vin.data.datevalue = (unsigned long)0x0000000200000001ULL; /* ~Mac epoch + offset */

    langhash_test_disksymbolrecord_v7 rec;
    pack_value(&vin, &rec);

    unsigned char *recbytes = (unsigned char *)&rec;
    size_t payload_off = 8;
    uint64_t stored = db_format_read_be64(recbytes + payload_off);
    assert(stored == (uint64_t)vin.data.datevalue);

    tyvaluerecord vout; bigstring bsout;
    unpack_value(&rec, &vout);
    assert(vout.valuetype == datevaluetype);
    assert(vout.data.datevalue == vin.data.datevalue);

    teardown_mode();
    printf("PASS\n");
}

static void test_be64_double_bits(void) {
    printf("[hashpack] double BE roundtrip... ");
    setup_mode_modern();

    tyvaluerecord vin; init_valuerecord(&vin, doublevaluetype);
    double d = 12345.6789;
    Handle hdouble = nil;
    assert(newclearhandle(sizeof(double), &hdouble));
    vin.data.doublevalue = (double **) hdouble;
    **vin.data.doublevalue = d;

    langhash_test_disksymbolrecord_v7 rec;
    pack_value(&vin, &rec);

    unsigned char *recbytes = (unsigned char *)&rec;
    size_t payload_off = 8;
    uint64_t stored = db_format_read_be64(recbytes + payload_off);
    double reread = 0.0; memcpy(&reread, &stored, sizeof(reread));
    assert(reread == d || (reread - d) < 1e-12);

    tyvaluerecord vout; bigstring bsout;
    unpack_value(&rec, &vout);
    assert(vout.valuetype == doublevaluetype);
    uint64_t outbits = 0;
    memcpy(&outbits, *vout.data.doublevalue, sizeof(outbits));
    uint64_t inbits = 0;
    memcpy(&inbits, &d, sizeof(inbits));
    assert(outbits == inbits);

    disposehandle((Handle)vin.data.doublevalue);
    disposehandle((Handle)vout.data.doublevalue);
    teardown_mode();
    printf("PASS\n");
}

int main(void) {
    TR_INIT("hashpack_modern_tests");
    printf("\n=== Running hashpack modern BE64 tests ===\n");
    TR_RUN(test_be64_int_roundtrip);
    TR_RUN(test_be64_date_roundtrip);
    TR_RUN(test_be64_double_bits);
    printf("=== hashpack modern BE64 tests complete ===\n\n");
    TR_SUMMARY();
    return TR_EXIT_CODE();
}
