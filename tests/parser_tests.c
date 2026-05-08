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

/*
 * Multi-line / arbitrary-byte runner: build a Handle from a raw byte buffer
 * (so embedded '\n', '\r', or high-bit UTF-8 bytes survive verbatim), compile
 * via langbuildtree (line-based scan, like production), and evaluate the
 * resulting tree. Returns true if the script ran cleanly. On success, copies
 * the coerced result into result_buf (size at least 256) for assertions.
 *
 * This is the test analogue of langrunhandle but with separate Handle vs.
 * bigstring sourcing, and value-record return (so we can also assert the
 * type of the result, not just its string coercion).
 */
static boolean run_multiline(const char *src, size_t src_len, char *result_buf,
	size_t result_buf_size, int *out_valuetype) {
	Handle htext = nil;
	hdltreenode hcode = nil;
	tyvaluerecord vresult;
	boolean fl;

	if (out_valuetype) *out_valuetype = -1;
	if (result_buf && result_buf_size > 0) result_buf[0] = '\0';

	if (!newemptyhandle(&htext))
		return false;

	if (!sethandlesize(htext, (long)src_len)) {
		disposehandle(htext);
		return false;
	}
	memcpy(*htext, src, src_len);

	if (!langbuildtree(htext, true, &hcode))
		return false;
	langerrorclear();

	initvalue(&vresult, novaluetype);
	fl = langruncode(hcode, nil, &vresult);
	if (out_valuetype) *out_valuetype = (int)vresult.valuetype;

	if (fl && result_buf && result_buf_size > 0) {
		bigstring bs;
		if (coercetostring(&vresult)) {
			texthandletostring(vresult.data.stringvalue, bs);
			copyptocstring(bs, result_buf);
			if (strlen(result_buf) >= result_buf_size)
				result_buf[result_buf_size - 1] = '\0';
		}
	}

	disposevaluerecord(vresult, false);
	langdisposetree(hcode);
	return fl;
}

/*
 * Quirk 1 (issue #586): `// comment\nreturn v` should return v's value
 * (an integer 42), not boolean true and not a syntax error. With LF line
 * endings (which the REPL wrapper produces), parsepopcomment needs to
 * recognize \n as end-of-comment.
 */
static void test_quirk1_slash_slash_comment_with_lf_then_return(void) {
	/* Three statements separated by LF: declare local, then a // comment,
	 * then a return. Whole script is line-based (langbuildtree
	 * fllinebased=true). */
	static const char src[] =
		"local (x = 42);\n"
		"// this comment line should not swallow the following return\n"
		"return (x)";
	char buf[256];
	int vt = -1;
	boolean ok = run_multiline(src, sizeof(src) - 1, buf, sizeof(buf), &vt);
	if (!ok) {
		printf("FAILED to compile/run quirk1 source\n");
		fflush(stdout);
	}
	assert(ok);
	/* Coerced result must be the integer 42, not "true" or "false". */
	if (strcmp(buf, "42") != 0) {
		printf("quirk1 LF: expected '42', got '%s' (vt=%d)\n", buf, vt);
		fflush(stdout);
	}
	assert(strcmp(buf, "42") == 0);
}

/*
 * Quirk 1 control: when the // comment is omitted, the same script returns
 * 42 — proves the comment is the differentiator and not some unrelated bug.
 */
static void test_quirk1_control_no_comment_returns_value(void) {
	static const char src[] =
		"local (x = 42);\n"
		"return (x)";
	char buf[256];
	int vt = -1;
	boolean ok = run_multiline(src, sizeof(src) - 1, buf, sizeof(buf), &vt);
	assert(ok);
	assert(strcmp(buf, "42") == 0);
}

/*
 * Quirk 1 with CR line endings: legacy outline form. parsepopcomment already
 * terminates on \r, so this passes even before the fix — captures the
 * pre-existing correct behavior so a future regression is caught.
 */
static void test_quirk1_slash_slash_comment_with_cr_then_return(void) {
	static const char src[] =
		"local (x = 42);\r"
		"// this comment line should not swallow the following return\r"
		"return (x)";
	char buf[256];
	int vt = -1;
	boolean ok = run_multiline(src, sizeof(src) - 1, buf, sizeof(buf), &vt);
	assert(ok);
	assert(strcmp(buf, "42") == 0);
}

/*
 * Quirk 2 (issue #586): UTF-8 em-dash (U+2014, bytes 0xE2 0x80 0x94) inside
 * a // comment must not break compilation. Compile must succeed and the
 * script must return the trailing expression's value verbatim. Same root
 * cause as Quirk 1 — the comment text contained any high-bit char and the
 * LF-eat side effect of parsepopchar made the comment swallow the following
 * line(s) until EOF.
 */
static void test_quirk2_em_dash_in_slash_slash_comment(void) {
	/* The em-dash byte sequence sits between two ASCII words inside a //
	 * comment. Comment is followed by a normal expression line. */
	static const char src[] =
		"local (x = 7);\n"
		"// dash \xE2\x80\x94 here\n"
		"return (x + 1)";
	char buf[256];
	int vt = -1;
	boolean ok = run_multiline(src, sizeof(src) - 1, buf, sizeof(buf), &vt);
	if (!ok) {
		printf("FAILED quirk2: em-dash in // comment broke compile\n");
		fflush(stdout);
	}
	assert(ok);
	if (strcmp(buf, "8") != 0) {
		printf("quirk2: expected '8', got '%s' (vt=%d)\n", buf, vt);
		fflush(stdout);
	}
	assert(strcmp(buf, "8") == 0);
}

/*
 * Quirk 3 (issue #586): two consecutive `local(x = ...);` statements where
 * the second references something from the first must compile and run.
 * The original PR 580 reproducer was `local(x = verb_returning_record(...));`
 * followed by `local(y = x.field);`. The simple shape (sizeof, list index)
 * compiles fine even on the unfixed parser — these tests pin the GOOD
 * behavior so a regression is caught. The shape that actually triggered
 * compile-fail in PR 580 needed the full ODB context (verb returning a
 * record, table-address dot-access) and could not be reproduced in this
 * unit-test harness; if it is shape-specific to that context, file a
 * follow-up issue with the minimum repro when re-encountered.
 */
static void test_quirk3_consecutive_locals_with_field_reference_cr(void) {
	static const char src[] =
		"local (s = \"hello\");\r"
		"local (n = sizeof (s));\r"
		"return (n)";
	char buf[256];
	int vt = -1;
	boolean ok = run_multiline(src, sizeof(src) - 1, buf, sizeof(buf), &vt);
	if (!ok) {
		printf("FAILED quirk3 (CR): adjacent locals with cross-reference broke compile\n");
		fflush(stdout);
	}
	assert(ok);
	if (strcmp(buf, "5") != 0) {
		printf("quirk3 CR: expected '5', got '%s' (vt=%d)\n", buf, vt);
		fflush(stdout);
	}
	assert(strcmp(buf, "5") == 0);
}

static void test_quirk3_consecutive_locals_with_field_reference_lf(void) {
	static const char src[] =
		"local (s = \"hello\");\n"
		"local (n = sizeof (s));\n"
		"return (n)";
	char buf[256];
	int vt = -1;
	boolean ok = run_multiline(src, sizeof(src) - 1, buf, sizeof(buf), &vt);
	if (!ok) {
		printf("FAILED quirk3 (LF): adjacent locals with cross-reference broke compile\n");
		fflush(stdout);
	}
	assert(ok);
	if (strcmp(buf, "5") != 0) {
		printf("quirk3 LF: expected '5', got '%s' (vt=%d)\n", buf, vt);
		fflush(stdout);
	}
	assert(strcmp(buf, "5") == 0);
}

static void test_quirk3_local_then_local_with_list_index_cr(void) {
	static const char src[] =
		"local (lst = {10, 20, 30});\r"
		"local (n = lst[2]);\r"
		"return (n)";
	char buf[256];
	int vt = -1;
	boolean ok = run_multiline(src, sizeof(src) - 1, buf, sizeof(buf), &vt);
	if (!ok) {
		printf("FAILED quirk3 (list CR): adjacent local(list)/local(index) broke compile\n");
		fflush(stdout);
	}
	assert(ok);
	if (strcmp(buf, "20") != 0) {
		printf("quirk3 list CR: expected '20', got '%s' (vt=%d)\n", buf, vt);
		fflush(stdout);
	}
	assert(strcmp(buf, "20") == 0);
}

static void test_quirk3_local_then_local_with_list_index_lf(void) {
	static const char src[] =
		"local (lst = {10, 20, 30});\n"
		"local (n = lst[2]);\n"
		"return (n)";
	char buf[256];
	int vt = -1;
	boolean ok = run_multiline(src, sizeof(src) - 1, buf, sizeof(buf), &vt);
	if (!ok) {
		printf("FAILED quirk3 (list LF): adjacent local(list)/local(index) broke compile\n");
		fflush(stdout);
	}
	assert(ok);
	if (strcmp(buf, "20") != 0) {
		printf("quirk3 list LF: expected '20', got '%s' (vt=%d)\n", buf, vt);
		fflush(stdout);
	}
	assert(strcmp(buf, "20") == 0);
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
	/* Issue #586 — UserTalk parser quirks */
	TR_RUN(test_quirk1_control_no_comment_returns_value);
	TR_RUN(test_quirk1_slash_slash_comment_with_cr_then_return);
	TR_RUN(test_quirk1_slash_slash_comment_with_lf_then_return);
	TR_RUN(test_quirk2_em_dash_in_slash_slash_comment);
	TR_RUN(test_quirk3_consecutive_locals_with_field_reference_cr);
	TR_RUN(test_quirk3_consecutive_locals_with_field_reference_lf);
	TR_RUN(test_quirk3_local_then_local_with_list_index_cr);
	TR_RUN(test_quirk3_local_then_local_with_list_index_lf);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
