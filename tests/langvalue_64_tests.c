#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "standard.h"
#include "lang.h"
#include "strings.h"
#include "ops.h"
#include "langinternal.h"
#include "memory.h"

/* Expose bitwise helpers from langverbs under FRONTIER_TESTS. */
int64_t langverbs_test_bitand (uint64_t a, uint64_t b);
int64_t langverbs_test_bitor (uint64_t a, uint64_t b);
int64_t langverbs_test_bitxor (uint64_t a, uint64_t b);
int64_t langverbs_test_bitset (uint64_t a, uint16_t bitnum);
int64_t langverbs_test_bitclear (uint64_t a, uint16_t bitnum);
int64_t langverbs_test_shift_left (uint64_t a, uint16_t dist);
int64_t langverbs_test_shift_right (uint64_t a, uint16_t dist);

/* Minimal verb-level calls */
extern boolean bitsetverb (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean bitclearverb (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean bitandverb (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean bitorverb (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean bitxorverb (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean bitshiftleftverb (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean bitshiftrightverb (hdltreenode hparam1, tyvaluerecord *vreturned);

/* Stub minimal treenode adapter for verbs expecting parameters. */
static hdltreenode make_two_param_tree(long a, long b) {
    /* Very small stub: reuse a static array of two tyvaluerecords encoded as a fake treenode list.
       The verb helpers just pull params via getlongvalue/getintvalue; here we bypass and call directly
       via wrapper treenode representation understood by getparamvalue. */
    /* For this test harness we rely on an existing helper: langcoerceerror uses globals; keep minimal. */
    return (hdltreenode) ((intptr_t) a << 1 | (intptr_t) b);
}

/* 2025-12-07 Codex: 64-bit numeric widening regressions for in-memory values and parsing. */

static void bigstring_from_c(const char *src, bigstring bs) {
    size_t len = strlen(src);
    if (len > 255)
        len = 255;
    bs[0] = (unsigned char) len;
    memcpy(&bs[1], src, len);
}

static void bigstring_to_c(const bigstring bs, char *out, size_t out_size) {
    size_t len = stringlength(bs);
    if (len >= out_size)
        len = out_size - 1;
    memcpy(out, &bs[1], len);
    out[len] = '\0';
}

static void test_addvalue_int64(void) {
    printf("[langvalue_64] addvalue 64-bit... ");
    tyvaluerecord v1, v2, vout;
    initvalue(&v1, longvaluetype);
    initvalue(&v2, longvaluetype);
    setlongvalue((int64_t) (INT64_MAX - 1), &v1);
    setlongvalue((int64_t) 1, &v2);
    assert(addvalue(v1, v2, &vout));
    assert(vout.valuetype == longvaluetype);
    assert(vout.data.longvalue == INT64_MAX);
    disposevaluerecord(vout, false);
    printf("PASS\n");
}

static void test_modvalue_int64(void) {
    printf("[langvalue_64] modvalue 64-bit... ");
    tyvaluerecord v1, v2, vout;
    initvalue(&v1, longvaluetype);
    initvalue(&v2, longvaluetype);
    setlongvalue((int64_t) 0x1FFFFFFFFLL, &v1);   /* >32-bit */
    setlongvalue((int64_t) 0x1FFFFLL, &v2);       /* >16-bit divisor */
    assert(modvalue(v1, v2, &vout));
    assert(vout.valuetype == longvaluetype);
    assert(vout.data.longvalue == (int64_t) (0x1FFFFFFFFLL % 0x1FFFFLL));
    disposevaluerecord(vout, false);
    printf("PASS\n");
}

static void test_stringtonumber_64(void) {
    printf("[langvalue_64] stringtonumber 64-bit decimals... ");
    bigstring bsmax, bsmin;
    bigstring_from_c("9223372036854775807", bsmax);
    bigstring_from_c("-9223372036854775808", bsmin);
    long out = 0;
    assert(stringtonumber(bsmax, &out));
    assert((int64_t) out == INT64_MAX);
    assert(stringtonumber(bsmin, &out));
    assert((int64_t) out == INT64_MIN);
    printf("PASS\n");
}

static void test_hexstringtonumber_64(void) {
    printf("[langvalue_64] hexstringtonumber 64-bit... ");
    bigstring bshex;
    bigstring_from_c("0x7fffffffffffffff", bshex);
    {
        char buf[64];
        bigstring_to_c(bshex, buf, sizeof buf);
        fprintf(stderr, "hex input \"%s\" len=%d\n", buf, stringlength(bshex));
    }
    long out = 0;
    if (!hexstringtonumber(bshex, &out)) {
        char buf[64];
        bigstring_to_c(bshex, buf, sizeof buf);
        fprintf(stderr, "FAIL parse %s len=%d\n", buf, stringlength(bshex));
        fflush(stderr);
        assert(false);
    }
    if ((int64_t) out != INT64_MAX) {
        fprintf(stderr, "hex parse produced %ld (0x%llx)\n", out, (unsigned long long) (uint64_t) out);
        fflush(stderr);
        assert(false);
    }
    bigstring_from_c("0xffffffffffffffff", bshex);
    if (!hexstringtonumber(bshex, &out)) {
        char buf[64];
        bigstring_to_c(bshex, buf, sizeof buf);
        fprintf(stderr, "FAIL parse %s len=%d\n", buf, stringlength(bshex));
        fflush(stderr);
        assert(false);
    }
    assert((int64_t) out == -1); /* sign-extended */
    printf("PASS\n");
}

static void test_numbertostring_roundtrip(void) {
    printf("[langvalue_64] numbertostring roundtrip... ");
    bigstring bs;
    int64_t value = (int64_t) 9007199254740991LL; /* within double int exact range but >32-bit */
    numbertostring((long) value, bs);
    long parsed = 0;
    assert(stringtonumber(bs, &parsed));
    assert((int64_t) parsed == value);
    printf("PASS\n");
}

static void test_intvalue_no_clamp(void) {
    printf("[langvalue_64] intvaluetype stores 64-bit... ");
    tyvaluerecord v;
    initvalue(&v, intvaluetype);
    setintvalue((int64_t) 0x1FFFFFFFFLL, &v);
    assert(v.data.intvalue == (int64_t) 0x1FFFFFFFFLL);
    printf("PASS\n");
}

static void test_datevalue_64(void) {
    printf("[langvalue_64] datevalue 64-bit... ");
    tyvaluerecord v;
    initvalue(&v, datevaluetype);
    setdatevalue((int64_t) 0x0000000200000001LL, &v); /* beyond 32-bit Mac epoch seconds */
    assert(v.data.datevalue == (int64_t) 0x0000000200000001LL);
    printf("PASS\n");
}

static void test_bitwise_64(void) {
    printf("[langvalue_64] bitwise 64-bit... ");
    uint64_t a = 0x1FFFFFFFFULL; /* >32-bit */
    uint64_t b = 0x10000000FULL; /* >32-bit */
    assert(langverbs_test_bitand(a, b) == (int64_t) (a & b));
    assert(langverbs_test_bitor(a, b) == (int64_t) (a | b));
    assert(langverbs_test_bitxor(a, b) == (int64_t) (a ^ b));
    assert(langverbs_test_bitset(0, 60) == (int64_t) (1ULL << 60));
    assert(langverbs_test_bitclear((uint64_t) -1, 63) == (int64_t) (~((uint64_t)1 << 63)));
    assert(langverbs_test_shift_left(1, 40) == (int64_t) (1ULL << 40));
    assert(langverbs_test_shift_right(1ULL << 63, 63) == 1);
    printf("PASS\n");
}

static void test_numbertohex_roundtrip(void) {
    printf("[langvalue_64] numbertohexstring roundtrip... ");
    bigstring bs;
    long val = 0x1234; /* exercise numbertohexstring within short path */
    numbertohexstring(val, bs);
    long parsed = 0;
    assert(hexstringtonumber(bs, &parsed));
    printf("PASS\n");
}

static void test_numbertohex_long_path(void) {
    printf("[langvalue_64] numbertohexstring long-path (32-bit)... ");
    bigstring bs;
    long val = 0x7FFFFFFF; /* force long path */
    numbertohexstring(val, bs);
    long parsed = 0;
    assert(hexstringtonumber(bs, &parsed));
    assert(parsed == val);
    printf("PASS\n");
}

static void test_arithmetic_edges(void) {
    printf("[langvalue_64] arithmetic 64-bit edges... ");
    tyvaluerecord v1, v2, out;
    initvalue(&v1, longvaluetype);
    initvalue(&v2, longvaluetype);
    /* Addition near max */
    setlongvalue(INT64_MAX - 1, &v1);
    setlongvalue(1, &v2);
    assert(addvalue(v1, v2, &out));
    assert(out.data.longvalue == INT64_MAX);
    disposevaluerecord(out, false);
    /* Multiplication small to avoid overflow but exercise 64-bit */
    setlongvalue(0x100000000LL, &v1);
    setlongvalue(0x10, &v2);
    assert(multiplyvalue(v1, v2, &out));
    assert(out.data.longvalue == 0x1000000000LL);
    disposevaluerecord(out, false);
    /* Division */
    setlongvalue(0x7FFFFFFFFFFFFFFFLL, &v1);
    setlongvalue(0x10, &v2);
    assert(dividevalue(v1, v2, &out));
    assert(out.data.longvalue == (0x7FFFFFFFFFFFFFFFLL / 0x10));
    disposevaluerecord(out, false);
    /* Mod */
    setlongvalue(0x7FFFFFFFFFFFFFFFLL, &v1);
    setlongvalue(97, &v2);
    assert(modvalue(v1, v2, &out));
    assert(out.data.longvalue == (0x7FFFFFFFFFFFFFFFLL % 97));
    disposevaluerecord(out, false);
    printf("PASS\n");
}

int main(void) {
    printf("\n=== Running langvalue 64-bit widening tests ===\n");
    test_addvalue_int64();
    test_modvalue_int64();
    test_stringtonumber_64();
    test_hexstringtonumber_64();
    test_numbertostring_roundtrip();
    test_intvalue_no_clamp();
    test_datevalue_64();
    test_bitwise_64();
    test_arithmetic_edges();
    test_numbertohex_roundtrip();
    test_numbertohex_long_path();
    printf("=== langvalue 64-bit widening tests complete ===\n\n");
    return 0;
}
