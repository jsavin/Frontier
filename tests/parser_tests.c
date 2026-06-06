#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
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

/*
 * eval_handle_expect — evaluate a (possibly multi-line, > 255-byte) program
 * passed as a raw C string. Allows arbitrary line endings (CR / LF / CRLF) in
 * the source text — characterizes how the scanner handles each one.
 *
 * Compares the coerced string result against `expected` (NULL means "any").
 */
static void eval_handle_expect(const char *prog, const char *expected) {
    Handle htext = NULL;
    long len = (long) strlen(prog);
    bigstring bsempty;
    setstringlength(bsempty, 0);
    if (!newtexthandle(bsempty, &htext)) {
        printf("FAILED to allocate handle\n");
        assert(0);
        return;
    }
    /* Replace the empty handle's bytes with our raw C string (including any
       embedded \n / \r). */
    if (!sethandlesize(htext, len)) {
        printf("FAILED to size handle\n");
        assert(0);
        return;
    }
    memcpy(*htext, prog, (size_t) len);

    bigstring bsresult;
    /* langrunhandle disposes the handle — match the contract used elsewhere. */
    boolean ok = langrunhandle(htext, bsresult);
    if (!ok) {
        printf("FAILED to evaluate (compile or run error):\n----\n%.*s\n----\n",
               (int) len, prog);
        fflush(stdout);
    }
    assert(ok);
    char c_result[512];
    copyptocstring(bsresult, c_result);
    if (expected != NULL) {
        if (strcmp(c_result, expected) != 0) {
            printf("FAILED: got %s, expected %s\n  program:\n----\n%.*s\n----\n",
                   c_result, expected, (int) len, prog);
            fflush(stdout);
        }
        assert(strcmp(c_result, expected) == 0);
    }
}

/* Compile-only check: returns true if the program compiles, false otherwise.
   Doesn't run the program. Useful for characterizing pure parser bugs. */
static boolean compile_only(const char *prog) {
    Handle htext = NULL;
    long len = (long) strlen(prog);
    bigstring bsempty;
    setstringlength(bsempty, 0);
    if (!newtexthandle(bsempty, &htext)) return false;
    if (!sethandlesize(htext, len)) { disposehandle(htext); return false; }
    memcpy(*htext, prog, (size_t) len);
    hdltreenode hcode = NULL;
    boolean ok = langcompiletext(htext, false, &hcode); /* disposes htext */
    if (hcode != NULL) langdisposetree(hcode);
    return ok;
}

static void test_boolean_unary_not(void) {
    eval_expect("! false", "true");
    eval_expect("! true", "false");
}

static void test_comparisons(void) {
    eval_expect("2 > 1", "true");
    eval_expect("1 == 1", "true");
    eval_expect("1 != 2", "true");
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

/*
 * Issue #586 Quirk 1: `// comment` followed by `return v` returns boolean true
 * instead of v. Root cause: the // line-comment terminator scan only treats
 * CR (chreturn, 0x0D) as end-of-line. With LF (0x0A) line endings, the
 * comment consumes the rest of the input (including the `return` statement),
 * leaving the script body effectively empty.
 *
 * These tests pin behavior across all three line-ending conventions.
 */
static void test_quirk1_line_comment_terminates_on_lf(void) {
    /* LF line endings (Unix / modern editors) */
    eval_handle_expect("local (x = 42);\n// some comment\nreturn (x)\n", "42");

    /* CRLF line endings (Windows) */
    eval_handle_expect("local (x = 42);\r\n// some comment\r\nreturn (x)\r\n", "42");

    /* CR line endings (the original Mac / outline canonical form — already worked) */
    eval_handle_expect("local (x = 42);\r// some comment\rreturn (x)\r", "42");

    /* Comment with no following statement (just to confirm we don't eat too much) */
    eval_handle_expect("local (x = 7);\nreturn (x);\n// trailing comment\n", "7");
}

/*
 * Issue #586 Quirk 2: A UTF-8 em-dash (U+2014, bytes E2 80 94) inside a //
 * comment causes silent compile failure on LF-terminated input. The same
 * em-dash followed by a CR-terminated line works fine.
 *
 * This test confirms that any non-ASCII byte inside a // comment must NOT
 * affect compilation, on any line-ending convention.
 */
static void test_quirk2_nonascii_in_line_comment(void) {
    /* UTF-8 em-dash inside an LF-terminated // comment */
    eval_handle_expect("local (x = 1);\n// hello \xE2\x80\x94 world\nreturn (x)\n",
                       "1");

    /* Other non-ASCII high bytes (would have collided with mac comment chars) */
    eval_handle_expect("local (x = 2);\n// quote \xC9 here\nreturn (x)\n", "2");

    /* Multiple non-ASCII chars in a comment */
    eval_handle_expect("local (x = 3);\n// \xE2\x80\x94 a \xE2\x80\x94\nreturn (x)\n",
                       "3");
}

/*
 * Issue #586 Quirk 3: Two consecutive `local(x = expr);` statements where the
 * second references a field of the first compile-fail without a blank line
 * between them. Adding a blank line fixes it.
 *
 * We don't need a verb that returns a record — we can build the structure
 * synthetically with a table reference. The bug is in the parser's handling of
 * adjacent local() declarations on consecutive lines, not in any verb.
 *
 * Reduce to the minimal repro: a local that aliases a previously declared
 * local, with the second referencing a member of the first via dot access.
 */
static void test_quirk3_consecutive_local_with_field_access(void) {
    /* The original bug surfaces with two adjacent local() lines using LF
       endings. Reproducing with a simple value substitute for the verb call:
       once Quirk 1 is fixed, the second local() should still parse correctly. */
    eval_handle_expect(
        "local (x = 10);\n"
        "local (y = x);\n"
        "return (y)\n",
        "10");

    /* With a blank line between (the existing workaround). */
    eval_handle_expect(
        "local (x = 11);\n"
        "\n"
        "local (y = x);\n"
        "return (y)\n",
        "11");

    /* CR-terminated form (canonical Frontier outline format) */
    eval_handle_expect(
        "local (x = 12);\r"
        "local (y = x);\r"
        "return (y)\r",
        "12");
}

/*
 * Issue #586 — compile-only smoke tests for common LF-source idioms that have
 * historically caused silent compile failures.
 */
static void test_quirk_compile_smoke(void) {
    /* Bare LF newlines should not break compilation of multi-statement
       scripts that include // comments. */
    assert(compile_only("local (x);\n// c\nx = 1;\nreturn (x)\n"));

    /* CRLF likewise. */
    assert(compile_only("local (x);\r\n// c\r\nx = 1;\r\nreturn (x)\r\n"));

    /* CR canonical form likewise. */
    assert(compile_only("local (x);\r// c\rx = 1;\rreturn (x)\r"));
}

/*
 * Regression test for the canonical CR-only path. Production scripts stored
 * in ODB use CR line endings and frequently look like the startupScript:
 * leading tab-indented `// comments` followed by code. This must continue
 * to work — earlier parser fixes (PR #609) broke this path and produced
 * 43 integration regressions.
 */
/*
 * Issue #716 item 1: parseerror used to take `bigstring` and the only caller
 * (yyerror in langparser.c) cast its `const char *s` to `(ptrstring) s` to
 * match. parseerror then cast it back to `(const char *)` to feed
 * copyctopstring. The cast pair laundered the real type for no reason. Post
 * fix: parseerror takes const char * directly. These tests drive the
 * parse-error path with malformed input so that the yyerror -> parseerror
 * -> copyctopstring chain is exercised end-to-end. They guard against
 * regressions where someone "tidies" the signature back to bigstring.
 */
static void test_parseerror_short_message_does_not_crash(void) {
    /* Trailing operator triggers a short bison "syntax error" message. */
    assert(!compile_only("1 + "));
}

static void test_parseerror_long_token_does_not_crash(void) {
    /*
     * A single 300-char identifier followed by garbage produces a yyerror
     * call whose message embeds the (truncated) token. The C-string-typed
     * argument flows through parseerror -> copyctopstring which post-#707
     * clamps payload to 255 bytes. We only require that compilation fails
     * cleanly without crashing or overflowing the bigstring.
     */
    char prog[512];
    memset(prog, 'a', 300);
    prog[300] = '\0';
    strcat(prog, " +");          /* trailing operator forces parse error */
    assert(!compile_only(prog));
}

static void test_parseerror_direct_long_message_well_defined(void) {
    /*
     * Call parseerror directly with a >255-byte C string. Pre-fix this
     * input would have been cast to ptrstring at the only callsite and
     * then back to const char *; the round-trip happened to be safe
     * because the original was always a real C string, but the function
     * signature lied about the type. Post-fix the signature matches
     * reality and the truncation path is taken in a well-defined manner.
     */
    char msg[400];
    memset(msg, 'x', 350);
    msg[350] = '\0';
    /*
     * langerrordisable suppresses the error-reporting callback so the
     * test doesn't print a wall of noise; the body that copies the
     * C string into a bigstring runs unconditionally.
     */
    disablelangerror();
    parseerror(msg);
    enablelangerror();
}

static void test_cr_path_with_tab_indented_comments(void) {
    /* Top-of-file comments, tab-indented like startupScript. */
    eval_handle_expect("// header\r\t// indented comment\r\t\t// more indent\rreturn (42)\r",
                       "42");

    /* Tab + comment + code, common pattern in handler bodies. */
    eval_handle_expect("\t// leading comment\r\treturn (5)\r",
                       "5");

    /* Comment line whose body is just tab+CR (blank-after-tab). */
    eval_handle_expect("// header\r\t\rreturn (1)\r",
                       "1");
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
    TR_RUN(test_quirk1_line_comment_terminates_on_lf);
    TR_RUN(test_quirk2_nonascii_in_line_comment);
    TR_RUN(test_quirk3_consecutive_local_with_field_access);
    TR_RUN(test_quirk_compile_smoke);
    TR_RUN(test_cr_path_with_tab_indented_comments);
    TR_RUN(test_parseerror_short_message_does_not_crash);
    TR_RUN(test_parseerror_long_token_does_not_crash);
    TR_RUN(test_parseerror_direct_long_message_well_defined);

    TR_SUMMARY();
    return TR_EXIT_CODE();
}
