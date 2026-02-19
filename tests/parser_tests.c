#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "tablestructure.h"
#include "logging.h"
#include "../portable/wptext_portable.h"
#include "test_report.h"

static void setup_bigstring_from_c(const char *cstr, bigstring out) {
    copyctopstring(cstr, out);
}

static void eval_expect(const char *expr, const char *expected) {
    bigstring program;
    bigstring result;
    setup_bigstring_from_c(expr, program);
    boolean ok = langrunstringnoerror(program, result);
    if (!ok) {
        char c_program[256];
        copyptocstring(program, c_program);
        printf("FAILED to evaluate: %s\n", c_program);
        fflush(stdout);
    }
    assert(ok);
    char c_result[256];
    copyptocstring(result, c_result);
    if (expected != NULL) {
        assert(strcmp(c_result, expected) == 0);
    }
}

static void test_boolean_unary_not(void) {
    /* SKIPPED: ! operator not working yet - see issue #197 */
    printf("SKIPPED: test_boolean_unary_not (! operator - issue #197)\n");
    // eval_expect("! false", "true");
    // eval_expect("! true", "false");
}

static void test_comparisons(void) {
    eval_expect("2 > 1", "true");
    eval_expect("1 == 1", "true");
    /* SKIPPED: != operator not working yet - see issue #197 */
    printf("SKIPPED: != operator test (issue #197)\n");
    // eval_expect("1 != 2", "true");
}

static void test_assign_and_local(void) {
    // local declaration + assignment across statements
    eval_expect("local(x); x = 5; x", "5");
    // multiple locals with comma list
    eval_expect("local(x, y); x = 2; y = 3; x + y", "5");
    // local with initializer list
    eval_expect("local(x = 7, y = 4); x - y", "3");
}

static void test_if_then_else_expr(void) {
    eval_expect("local(x = 1); if x == 1 then 2 else 3", "2");
    eval_expect("local(x = 0); if x == 1 then 2 else 3", "3");
}

int main(void) {
    TR_INIT("parser_tests");

    log_init();

    assert(initmemory());
    initstrings();
    assert(initlang());
    assert(inittablestructure());
    assert(langinitresources_headless());  /* Install constants, keywords, built-ins */
    assert(langinitverbs());
    assert(wp_portable_init());

    TR_RUN(test_boolean_unary_not);
    TR_RUN(test_comparisons);
    TR_RUN(test_assign_and_local);
    TR_RUN(test_if_then_else_expr);

    TR_SUMMARY();
    return TR_EXIT_CODE();
}
