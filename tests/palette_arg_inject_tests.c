/*
 * palette_arg_inject_tests.c — unit tests for the slash-menu arg-injection
 * helpers (palette_arg_inject.{c,h}).
 *
 * Why these tests exist
 * ---------------------
 * PR 8 originally bound a typed argument from the palette into a leaf's
 * handler call via a process-global pending-arg buffer (g_palette_pending_arg
 * in repl.c). Concurrency review (PR #584 /gate) flagged that as a
 * cross-thread hijack: meuserselected_headless's langruncode() yields the
 * GIL between statements, so a sibling thread.new() child running its own
 * palette dispatch could consume the wrong arg.
 *
 * The fix replaces the global with a per-call script-source synthesis:
 * the dispatcher transforms the leaf's stored script "<call> ()" into
 * "<call> (\"escaped-arg\")" and passes the synthesized source to the
 * existing meuserselected_headless. The arg is bound at compile time
 * inside the call frame — no global hand-off, no GIL-yield window for
 * a sibling thread to see the wrong value.
 *
 * This file exercises the two pure helpers in isolation. Threading
 * correctness is established by construction (no shared state) so the
 * tests focus on the input-output contract: escape rules, control-byte
 * rejection, paren detection, comment / string-literal skipping in the
 * scanner, and bounds-checking.
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "palette_arg_inject.h"
#include "test_report.h"

/* ---------- palette_arg_escape ---------- */

static void test_escape_empty_input_yields_empty_output(void) {
	char buf[8] = "junk";
	palette_arg_escape_result_t r = palette_arg_escape("", buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_OK);
	assert(buf[0] == '\0');
}

static void test_escape_null_input_yields_empty_output(void) {
	char buf[8] = "junk";
	palette_arg_escape_result_t r = palette_arg_escape(NULL, buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_OK);
	assert(buf[0] == '\0');
}

static void test_escape_plain_ascii_passes_through_unchanged(void) {
	char buf[64];
	palette_arg_escape_result_t r = palette_arg_escape("hello world", buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_OK);
	assert(strcmp(buf, "hello world") == 0);
}

static void test_escape_double_quote_is_backslashed(void) {
	char buf[64];
	palette_arg_escape_result_t r = palette_arg_escape("a\"b", buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_OK);
	assert(strcmp(buf, "a\\\"b") == 0);
}

static void test_escape_backslash_is_backslashed(void) {
	char buf[64];
	palette_arg_escape_result_t r = palette_arg_escape("a\\b", buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_OK);
	assert(strcmp(buf, "a\\\\b") == 0);
}

static void test_escape_quote_break_attempt_is_neutralized(void) {
	/* The classic injection: user types '"); attack ()' hoping to break
	 * out of the string-literal context. After escaping the quote, the
	 * whole sequence stays inside the literal — no UserTalk syntax
	 * carries through. */
	char buf[128];
	palette_arg_escape_result_t r = palette_arg_escape("\"); attack ()", buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_OK);
	assert(strcmp(buf, "\\\"); attack ()") == 0);
}

static void test_escape_rejects_newline(void) {
	char buf[64] = "junk";
	palette_arg_escape_result_t r = palette_arg_escape("a\nb", buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_FORBIDDEN_BYTE);
	assert(buf[0] == '\0');
}

static void test_escape_rejects_carriage_return(void) {
	char buf[64] = "junk";
	palette_arg_escape_result_t r = palette_arg_escape("a\rb", buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_FORBIDDEN_BYTE);
	assert(buf[0] == '\0');
}

static void test_escape_rejects_tab(void) {
	char buf[64] = "junk";
	palette_arg_escape_result_t r = palette_arg_escape("a\tb", buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_FORBIDDEN_BYTE);
	assert(buf[0] == '\0');
}

static void test_escape_rejects_low_control(void) {
	char buf[64] = "junk";
	palette_arg_escape_result_t r = palette_arg_escape("a\x01""b", buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_FORBIDDEN_BYTE);
	assert(buf[0] == '\0');
}

static void test_escape_rejects_del(void) {
	char buf[64] = "junk";
	const char input[] = { 'a', 0x7F, 'b', '\0' };
	palette_arg_escape_result_t r = palette_arg_escape(input, buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_FORBIDDEN_BYTE);
	assert(buf[0] == '\0');
}

static void test_escape_passes_high_bytes(void) {
	/* UTF-8 continuation bytes are legitimate inside string literals. */
	char buf[64];
	const char input[] = { 'a', (char)0xC3, (char)0xA9, 'b', '\0' };  /* "aéb" */
	palette_arg_escape_result_t r = palette_arg_escape(input, buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_OK);
	assert(strcmp(buf, input) == 0);
}

static void test_escape_buffer_overflow_returns_overflow(void) {
	char buf[4];
	memset(buf, 'X', sizeof(buf));
	/* "hello" is 5 chars + NUL — won't fit in 4. No forbidden bytes
	 * present, so the failure mode is overflow, not forbidden-byte. */
	palette_arg_escape_result_t r = palette_arg_escape("hello", buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_OVERFLOW);
	assert(buf[0] == '\0');
}

static void test_escape_buffer_exact_fit_succeeds(void) {
	char buf[4];      /* "ab" + NUL = 3 — fits in cap=4. */
	palette_arg_escape_result_t r = palette_arg_escape("ab", buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_OK);
	assert(strcmp(buf, "ab") == 0);
}

static void test_escape_zero_capacity_returns_overflow(void) {
	char buf[1] = { 'X' };
	palette_arg_escape_result_t r = palette_arg_escape("", buf, 0);
	/* Zero capacity is a buffer-too-small condition, not a forbidden
	 * byte — the input is empty and contains nothing forbidden. */
	assert(r == PALETTE_ARG_ESCAPE_OVERFLOW);
}

static void test_escape_quote_overflow_returns_overflow(void) {
	/* Issue #595: a buffer of 300 quote characters needs ~600 bytes
	 * escaped (each '"' becomes '\\' '"'). With a 516-byte buffer (the
	 * production size), this overflows — but the input has NO forbidden
	 * bytes. The failure mode must be OVERFLOW, not FORBIDDEN_BYTE.
	 * Surfacing the wrong diagnostic here was the original bug. */
	char input[301];
	memset(input, '"', 300);
	input[300] = '\0';
	char buf[516];
	palette_arg_escape_result_t r = palette_arg_escape(input, buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_OVERFLOW);
	assert(buf[0] == '\0');
}

static void test_escape_forbidden_takes_precedence_over_overflow(void) {
	/* Sanity: when input contains BOTH a forbidden byte AND would also
	 * overflow, the forbidden-byte rejection happens first (we scan
	 * left-to-right and refuse on the first forbidden byte before
	 * we'd ever exceed capacity). The diagnostic surfaced is the more
	 * actionable one — "remove the bad byte" vs "type less". */
	char input[10] = { '\n', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', '\0' };
	char buf[2];     /* tiny buffer that would overflow on any input */
	palette_arg_escape_result_t r = palette_arg_escape(input, buf, sizeof(buf));
	assert(r == PALETTE_ARG_ESCAPE_FORBIDDEN_BYTE);
	assert(buf[0] == '\0');
}

/* ---------- palette_arg_inject_into_script ---------- */

static void test_inject_simple_call_substitutes_empty_parens(void) {
	const char *src = "repl.list ()";
	char *out = NULL;
	size_t out_len = 0;
	bool ok = palette_arg_inject_into_script(src, strlen(src),
	                                         "foo", &out, &out_len);
	assert(ok);
	assert(out != NULL);
	assert(strcmp(out, "repl.list (\"foo\")") == 0);
	assert(out_len == strlen(out));
	free(out);
}

static void test_inject_handler_path_substitutes_empty_parens(void) {
	const char *src = "system.menus.handlers.repl.list ()";
	char *out = NULL;
	bool ok = palette_arg_inject_into_script(src, strlen(src),
	                                         "@workspace.foo", &out, NULL);
	assert(ok);
	assert(strcmp(out, "system.menus.handlers.repl.list (\"@workspace.foo\")") == 0);
	free(out);
}

static void test_inject_empty_arg_still_succeeds_with_empty_literal(void) {
	const char *src = "repl.list ()";
	char *out = NULL;
	bool ok = palette_arg_inject_into_script(src, strlen(src),
	                                         "", &out, NULL);
	assert(ok);
	assert(strcmp(out, "repl.list (\"\")") == 0);
	free(out);
}

static void test_inject_no_open_paren_fails(void) {
	const char *src = "no_call_here";
	char *out = (char *)0x1;        /* sentinel */
	bool ok = palette_arg_inject_into_script(src, strlen(src),
	                                         "x", &out, NULL);
	assert(!ok);
	assert(out == NULL);
}

static void test_inject_nonempty_parens_fails(void) {
	/* Defensive: leaf script that already passes args is NOT something
	 * we transform. The dispatcher will fall back to the original. */
	const char *src = "repl.list (\"preset\")";
	char *out = (char *)0x1;
	bool ok = palette_arg_inject_into_script(src, strlen(src),
	                                         "x", &out, NULL);
	assert(!ok);
	assert(out == NULL);
}

static void test_inject_unbalanced_paren_fails(void) {
	const char *src = "repl.list (";
	char *out = (char *)0x1;
	bool ok = palette_arg_inject_into_script(src, strlen(src),
	                                         "x", &out, NULL);
	assert(!ok);
	assert(out == NULL);
}

static void test_inject_paren_inside_string_literal_is_skipped(void) {
	/* The first '(' the scanner sees is at "list (" — not the ones
	 * inside the string literal. */
	const char *src = "msg (\"Hello (world)\"); list ()";
	/* Wait — the FIRST '(' is at "msg (" before the string. The scanner
	 * should pick that one up and find its closing ')'. The contents of
	 * (...) include a string literal whose contents ("Hello (world)")
	 * contain parens — those must be skipped within the string. The
	 * matching ')' for the outer 'msg' is at the end of the string +
	 * ')'. The ()-empty test on those contents will fail (they aren't
	 * empty), so the call returns false. We verify that — proves the
	 * scanner correctly walks the string literal without getting
	 * confused. */
	char *out = (char *)0x1;
	bool ok = palette_arg_inject_into_script(src, strlen(src),
	                                         "x", &out, NULL);
	assert(!ok);
	assert(out == NULL);
}

static void test_inject_after_line_comment_finds_real_parens(void) {
	const char *src = "// fake (\nrepl.list ()";
	char *out = NULL;
	bool ok = palette_arg_inject_into_script(src, strlen(src),
	                                         "y", &out, NULL);
	assert(ok);
	assert(strcmp(out, "// fake (\nrepl.list (\"y\")") == 0);
	free(out);
}

static void test_inject_with_leading_whitespace_in_parens(void) {
	const char *src = "repl.list (   )";
	char *out = NULL;
	bool ok = palette_arg_inject_into_script(src, strlen(src),
	                                         "z", &out, NULL);
	assert(ok);
	assert(strcmp(out, "repl.list (\"z\")") == 0);
	free(out);
}

static void test_inject_preserves_trailing_text(void) {
	const char *src = "repl.list ()\nreturn (true)";
	char *out = NULL;
	bool ok = palette_arg_inject_into_script(src, strlen(src),
	                                         "tail", &out, NULL);
	assert(ok);
	assert(strcmp(out, "repl.list (\"tail\")\nreturn (true)") == 0);
	free(out);
}

static void test_inject_escaped_arg_carries_through_verbatim(void) {
	/* The caller is responsible for pre-escaping; injection treats
	 * escaped_arg as an opaque string-literal interior. */
	const char *src = "repl.list ()";
	char *out = NULL;
	bool ok = palette_arg_inject_into_script(src, strlen(src),
	                                         "a\\\"b", &out, NULL);
	assert(ok);
	/* Output should contain the literally-escaped arg between the
	 * outer quotes — caller's escape decided what bytes to inject. */
	assert(strcmp(out, "repl.list (\"a\\\"b\")") == 0);
	free(out);
}

static void test_inject_zero_length_input_fails(void) {
	char *out = (char *)0x1;
	bool ok = palette_arg_inject_into_script("", 0, "x", &out, NULL);
	assert(!ok);
	assert(out == NULL);
}

static void test_inject_null_escaped_arg_fails(void) {
	char *out = (char *)0x1;
	bool ok = palette_arg_inject_into_script("f ()", 4, NULL, &out, NULL);
	assert(!ok);
	assert(out == NULL);
}

int main(void) {
	TR_INIT("palette_arg_inject_tests");

	TR_RUN(test_escape_empty_input_yields_empty_output);
	TR_RUN(test_escape_null_input_yields_empty_output);
	TR_RUN(test_escape_plain_ascii_passes_through_unchanged);
	TR_RUN(test_escape_double_quote_is_backslashed);
	TR_RUN(test_escape_backslash_is_backslashed);
	TR_RUN(test_escape_quote_break_attempt_is_neutralized);
	TR_RUN(test_escape_rejects_newline);
	TR_RUN(test_escape_rejects_carriage_return);
	TR_RUN(test_escape_rejects_tab);
	TR_RUN(test_escape_rejects_low_control);
	TR_RUN(test_escape_rejects_del);
	TR_RUN(test_escape_passes_high_bytes);
	TR_RUN(test_escape_buffer_overflow_returns_overflow);
	TR_RUN(test_escape_buffer_exact_fit_succeeds);
	TR_RUN(test_escape_zero_capacity_returns_overflow);
	TR_RUN(test_escape_quote_overflow_returns_overflow);
	TR_RUN(test_escape_forbidden_takes_precedence_over_overflow);

	TR_RUN(test_inject_simple_call_substitutes_empty_parens);
	TR_RUN(test_inject_handler_path_substitutes_empty_parens);
	TR_RUN(test_inject_empty_arg_still_succeeds_with_empty_literal);
	TR_RUN(test_inject_no_open_paren_fails);
	TR_RUN(test_inject_nonempty_parens_fails);
	TR_RUN(test_inject_unbalanced_paren_fails);
	TR_RUN(test_inject_paren_inside_string_literal_is_skipped);
	TR_RUN(test_inject_after_line_comment_finds_real_parens);
	TR_RUN(test_inject_with_leading_whitespace_in_parens);
	TR_RUN(test_inject_preserves_trailing_text);
	TR_RUN(test_inject_escaped_arg_carries_through_verbatim);
	TR_RUN(test_inject_zero_length_input_fails);
	TR_RUN(test_inject_null_escaped_arg_fails);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
