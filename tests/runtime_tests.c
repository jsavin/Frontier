#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "tablestructure.h"
#include "op.h"
#include "opinternal.h"
#include "opxml.h"
#include "ops.h"

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
    if (!ho || out_sz == 0) return;
    hdlheadrecord h = (**ho).hsummit;
    for (int i = 0; i < 16 && h; ++i) { /* limit to avoid infinite loops */
        bigstring bs;
        opgetheadstring(h, bs);
        char tmp[256];
        copyptocstring(bs, tmp);
        if (i > 0) strncat(out, "|", out_sz - strlen(out) - 1);
        strncat(out, tmp, out_sz - strlen(out) - 1);
        hdlheadrecord next = opbumpflatdown(h, true);
        if (next == h) break;
        h = next;
    }
}

static void run_opml_roundtrip(void) {
    printf("[rt] opml_roundtrip: start\n"); fflush(stdout);
    hdloutlinerecord ho1 = nil;
    printf("[rt] newoutlinerecord ho1...\n"); fflush(stdout);
    assert(newoutlinerecord(&ho1));
    oppushoutline(ho1);
    /* Set root text */
    {
        bigstring bsroot; copyctopstring("root", bsroot);
        printf("[rt] set root text...\n"); fflush(stdout);
        assert(opsetheadstring((**outlinedata).hbarcursor, bsroot));
    }
    /* Insert one child under root */
    {
        printf("[rt] insert child headline...\n"); fflush(stdout);
        Handle hchild = make_text_handle("child");
        assert(hchild != nil);
        /* opinsertheadline takes ownership or retains this handle; do not free here */
        assert(opinsertheadline(hchild, right, false));
    }

    /* Export to OPML */
    printf("[rt] export to OPML...\n"); fflush(stdout);
    Handle hname = nil, hemail = nil, hxml = nil;
    assert(newemptyhandle(&hname));
    assert(newemptyhandle(&hemail));
    hdlhashtable hto = nil, hcloud = nil;
    bigstring bso; setemptystring(bso);
    tyvaluerecord vo; initvalue(&vo, novaluetype);
    assert(opoutlinetoxml(ho1, hname, hemail, &hxml, hto, bso, vo, hcloud));

    /* Debug: dump a snippet of the OPML to stderr */
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

    /* Create destination outline and import */
    printf("[rt] create ho2/import from OPML...\n"); fflush(stdout);
    hdloutlinerecord ho2 = nil;
    assert(newoutlinerecord(&ho2));
    assert(opxmltooutline(hxml, ho2, true, hto, bso, vo, hcloud));

    /* Compare flattened texts */
    printf("[rt] collect/compare flattened text...\n"); fflush(stdout);
    char a[256], b[256];
    collect_outline_text(ho1, a, sizeof a);
    collect_outline_text(ho2, b, sizeof b);
    assert(strcmp(a, b) == 0);

    disposehandle(hxml);
    disposehandle(hname);
    disposehandle(hemail);
    printf("[rt] opml_roundtrip: done\n"); fflush(stdout);
}

static void run_constants_smoke(void) {
    printf("[rt] constants_smoke: start\n"); fflush(stdout);
    eval_expect("true != false", "true");
    eval_expect("nil == nil", "true");
    eval_expect("flatdown == flatdown", "true");
    eval_expect("infinity > 1000000", "true");
    /* Type constants exist and compare equal to themselves */
    eval_expect("stringType == stringType", "true");
    eval_expect("longType == longType", "true");
    printf("[rt] constants_smoke: done\n"); fflush(stdout);
}

int main(void) {
    printf("[rt] initmemory...\n"); fflush(stdout);
    assert(initmemory());
    printf("[rt] initstrings...\n"); fflush(stdout);
    initstrings();

    printf("[rt] initlang...\n"); fflush(stdout);
    assert(initlang());
    printf("[rt] inittablestructure...\n"); fflush(stdout);
    assert(inittablestructure());
    printf("[rt] langinitverbs...\n"); fflush(stdout);
    assert(langinitverbs());

    printf("[rt] before run_basic_script\n"); fflush(stdout);
    run_basic_script();
    printf("[rt] after run_basic_script\n"); fflush(stdout);
    printf("[rt] before constants_smoke\n"); fflush(stdout);
    run_constants_smoke();
    printf("[rt] after constants_smoke\n"); fflush(stdout);
    run_opml_roundtrip();
    printf("[rt] after run_opml_roundtrip\n"); fflush(stdout);

    printf("runtime_tests: langrunstring and OPML round-trip passed\n");
    return 0;
}
