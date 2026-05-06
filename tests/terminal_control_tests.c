/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 2026 Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/*
 * terminal_control_tests.c - Unit tests for slash-menu palette UI substrate
 *
 * Covers the new emitter functions added to terminal_control for the REPL
 * slash-menu palette (PR 3 of the slash-menu series):
 *   - terminal_move_to(row, col)         CSI CUP
 *   - terminal_set_attr(fg, bg, flags)   SGR colour + bold/inverse/underline
 *   - terminal_emit_box(x, y, w, h, dbl) Unicode box-drawing borders
 *   - terminal_enable_mouse()            xterm SGR 1006 + 1000 enable
 *   - terminal_disable_mouse()           xterm 1000 + SGR 1006 disable
 *
 * Strategy: redirect stderr to a temp file, invoke the function, fflush, then
 * read back the bytes and compare against the expected escape sequence. This
 * is a behavioural test of the wire-format output, which is what consumers
 * (palette renderer, file_browser, etc.) actually see.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include "terminal_control.h"
#include "test_report.h"

/* ---------- output capture ---------- */

static FILE *g_saved_stderr = NULL;
static int g_saved_stderr_fd = -1;
static char g_capture_path[256];

static void capture_begin(void) {
	/* Build a unique temp path */
	snprintf(g_capture_path, sizeof(g_capture_path),
		"/tmp/term_ctrl_test_%d_%ld.bin", (int)getpid(), (long)random());

	fflush(stderr);
	g_saved_stderr_fd = dup(STDERR_FILENO);
	if (g_saved_stderr_fd < 0) {
		perror("dup stderr");
		exit(1);
	}

	FILE *f = freopen(g_capture_path, "w+", stderr);
	if (!f) {
		perror("freopen stderr");
		exit(1);
	}
	g_saved_stderr = f;
}

/* Returns a malloc'd buffer of captured bytes; *out_len receives length. */
static char *capture_end(size_t *out_len) {
	fflush(stderr);

	/* Read back captured bytes */
	FILE *r = fopen(g_capture_path, "rb");
	if (!r) {
		perror("fopen capture");
		exit(1);
	}
	fseek(r, 0, SEEK_END);
	long sz = ftell(r);
	fseek(r, 0, SEEK_SET);

	char *buf = malloc((size_t)sz + 1);
	if (!buf) {
		fclose(r);
		exit(1);
	}
	size_t n = fread(buf, 1, (size_t)sz, r);
	buf[n] = '\0';
	fclose(r);

	/* Restore stderr */
	if (g_saved_stderr_fd >= 0) {
		dup2(g_saved_stderr_fd, STDERR_FILENO);
		close(g_saved_stderr_fd);
		g_saved_stderr_fd = -1;
		clearerr(stderr);
	}
	unlink(g_capture_path);

	if (out_len) *out_len = n;
	return buf;
}

/* ---------- assertion helpers ----------
 *
 * These are thin wrappers around assert() so that the test_report.h SIGABRT
 * trampoline catches a failure and continues with the next TR_RUN().  We
 * print a labelled PASS line (visible in the per-test output) and let
 * assert() surface the FAIL with file/line. */

static void expect_eq_str(const char *label, const char *actual, size_t actual_len,
                          const char *expected) {
	size_t exp_len = strlen(expected);
	bool ok = (actual_len == exp_len && memcmp(actual, expected, exp_len) == 0);
	if (!ok) {
		fprintf(stdout, "  FAIL: %s\n    expected (%zu bytes): ", label, exp_len);
		for (size_t i = 0; i < exp_len; i++) {
			unsigned char c = (unsigned char)expected[i];
			if (c >= 0x20 && c < 0x7f) printf("%c", c);
			else printf("\\x%02x", c);
		}
		fprintf(stdout, "\n    actual   (%zu bytes): ", actual_len);
		for (size_t i = 0; i < actual_len; i++) {
			unsigned char c = (unsigned char)actual[i];
			if (c >= 0x20 && c < 0x7f) printf("%c", c);
			else printf("\\x%02x", c);
		}
		fprintf(stdout, "\n");
		fflush(stdout);
	} else {
		printf("  PASS: %s\n", label);
	}
	assert(ok);
}

static void expect_contains(const char *label, const char *actual, size_t actual_len,
                            const char *needle) {
	size_t nlen = strlen(needle);
	bool found = false;
	if (actual_len >= nlen) {
		for (size_t i = 0; i + nlen <= actual_len; i++) {
			if (memcmp(actual + i, needle, nlen) == 0) { found = true; break; }
		}
	}
	if (!found) {
		fprintf(stdout, "  FAIL: %s — needle not found\n", label);
		fflush(stdout);
	} else {
		printf("  PASS: %s\n", label);
	}
	assert(found);
}

static void expect_true(const char *label, bool cond) {
	if (cond) printf("  PASS: %s\n", label);
	else { fprintf(stdout, "  FAIL: %s\n", label); fflush(stdout); }
	assert(cond);
}

/* ---------- tests ---------- */

static void test_move_to_basic(void) {
	printf("test_move_to_basic\n");
	capture_begin();
	terminal_move_to(5, 10);
	size_t n;
	char *buf = capture_end(&n);
	expect_eq_str("move_to(5,10) emits CSI 5;10 H", buf, n, "\x1b[5;10H");
	free(buf);
}

static void test_move_to_origin(void) {
	printf("test_move_to_origin\n");
	capture_begin();
	terminal_move_to(1, 1);
	size_t n;
	char *buf = capture_end(&n);
	expect_eq_str("move_to(1,1) emits CSI 1;1 H", buf, n, "\x1b[1;1H");
	free(buf);
}

static void test_set_attr_default(void) {
	printf("test_set_attr_default\n");
	capture_begin();
	/* fg=0, bg=0, no flags → should reset (SGR 0) */
	terminal_set_attr(0, 0, 0);
	size_t n;
	char *buf = capture_end(&n);
	expect_eq_str("set_attr(0,0,0) emits SGR 0 reset", buf, n, "\x1b[0m");
	free(buf);
}

static void test_set_attr_fg_only(void) {
	printf("test_set_attr_fg_only\n");
	capture_begin();
	/* fg=red(1), bg=0, no flags → SGR 0;31 (reset + fg red) */
	terminal_set_attr(1, 0, 0);
	size_t n;
	char *buf = capture_end(&n);
	expect_contains("set_attr(1,0,0) contains 31 (fg red)", buf, n, "31");
	expect_contains("set_attr(1,0,0) starts with ESC [", buf, n, "\x1b[");
	expect_contains("set_attr(1,0,0) ends with m", buf, n, "m");
	free(buf);
}

static void test_set_attr_bg(void) {
	printf("test_set_attr_bg\n");
	capture_begin();
	/* fg=0, bg=blue(4) → contains 44 */
	terminal_set_attr(0, 4, 0);
	size_t n;
	char *buf = capture_end(&n);
	expect_contains("set_attr bg=4 contains 44", buf, n, "44");
	free(buf);
}

static void test_set_attr_bold(void) {
	printf("test_set_attr_bold\n");
	capture_begin();
	terminal_set_attr(0, 0, TERM_ATTR_BOLD);
	size_t n;
	char *buf = capture_end(&n);
	expect_contains("set_attr bold contains ;1 or 1m", buf, n, "1");
	free(buf);
}

static void test_set_attr_inverse(void) {
	printf("test_set_attr_inverse\n");
	capture_begin();
	terminal_set_attr(0, 0, TERM_ATTR_INVERSE);
	size_t n;
	char *buf = capture_end(&n);
	expect_contains("set_attr inverse contains 7", buf, n, "7");
	free(buf);
}

static void test_set_attr_underline(void) {
	printf("test_set_attr_underline\n");
	capture_begin();
	terminal_set_attr(0, 0, TERM_ATTR_UNDERLINE);
	size_t n;
	char *buf = capture_end(&n);
	expect_contains("set_attr underline contains 4", buf, n, "4");
	free(buf);
}

static void test_emit_box_single(void) {
	printf("test_emit_box_single\n");
	capture_begin();
	/* 3x3 box at (1,1): corners + horizontals + verticals */
	terminal_emit_box(1, 1, 3, 3, false);
	size_t n;
	char *buf = capture_end(&n);
	/* Single-line UTF-8: ┌ E2 94 8C  ─ E2 94 80  ┐ E2 94 90
	                      │ E2 94 82  └ E2 94 94  ┘ E2 94 98 */
	expect_contains("emit_box single contains top-left ┌", buf, n, "\xe2\x94\x8c");
	expect_contains("emit_box single contains top-right ┐", buf, n, "\xe2\x94\x90");
	expect_contains("emit_box single contains bottom-left └", buf, n, "\xe2\x94\x94");
	expect_contains("emit_box single contains bottom-right ┘", buf, n, "\xe2\x94\x98");
	expect_contains("emit_box single contains horizontal ─", buf, n, "\xe2\x94\x80");
	expect_contains("emit_box single contains vertical │", buf, n, "\xe2\x94\x82");
	/* Should NOT contain double-line glyphs */
	bool has_double = false;
	for (size_t i = 0; i + 2 < n; i++) {
		if ((unsigned char)buf[i] == 0xe2 && (unsigned char)buf[i+1] == 0x95 &&
		    (unsigned char)buf[i+2] == 0x94) { has_double = true; break; }  /* ╔ */
	}
	expect_true("single-line box has no double-line chars", !has_double);
	free(buf);
}

static void test_emit_box_double(void) {
	printf("test_emit_box_double\n");
	capture_begin();
	terminal_emit_box(1, 1, 4, 3, true);
	size_t n;
	char *buf = capture_end(&n);
	/* Double-line UTF-8: ╔ E2 95 94  ═ E2 95 90  ╗ E2 95 97
	                      ║ E2 95 91  ╚ E2 95 9A  ╝ E2 95 9D */
	expect_contains("emit_box double contains top-left ╔", buf, n, "\xe2\x95\x94");
	expect_contains("emit_box double contains top-right ╗", buf, n, "\xe2\x95\x97");
	expect_contains("emit_box double contains bottom-left ╚", buf, n, "\xe2\x95\x9a");
	expect_contains("emit_box double contains bottom-right ╝", buf, n, "\xe2\x95\x9d");
	expect_contains("emit_box double contains horizontal ═", buf, n, "\xe2\x95\x90");
	expect_contains("emit_box double contains vertical ║", buf, n, "\xe2\x95\x91");
	free(buf);
}

static void test_emit_box_positions_cursor(void) {
	printf("test_emit_box_positions_cursor\n");
	capture_begin();
	/* Box at (col=10, row=5), 4x3 — must include CSI CUP for row 5,col 10 */
	terminal_emit_box(10, 5, 4, 3, false);
	size_t n;
	char *buf = capture_end(&n);
	expect_contains("emit_box positions at row 5", buf, n, "\x1b[5;10H");
	free(buf);
}

static void test_emit_box_too_small(void) {
	printf("test_emit_box_too_small\n");
	capture_begin();
	/* Degenerate box (w<2 or h<2) should not emit corners */
	terminal_emit_box(1, 1, 1, 1, false);
	size_t n;
	char *buf = capture_end(&n);
	/* Either zero output or no corner glyph — just ensure no crash and
	 * no top-left corner appears */
	bool has_corner = false;
	for (size_t i = 0; i + 2 < n; i++) {
		if ((unsigned char)buf[i] == 0xe2 && (unsigned char)buf[i+1] == 0x94 &&
		    (unsigned char)buf[i+2] == 0x8c) { has_corner = true; break; }
	}
	expect_true("degenerate box emits no corner glyph", !has_corner);
	free(buf);
}

static void test_enable_mouse_emits_sgr_and_button_event(void) {
	printf("test_enable_mouse_emits_sgr_and_button_event\n");
	capture_begin();
	terminal_enable_mouse();
	size_t n;
	char *buf = capture_end(&n);
	expect_contains("enable_mouse contains SGR 1006 enable",
	                buf, n, "\x1b[?1006h");
	expect_contains("enable_mouse contains button-event 1000 enable",
	                buf, n, "\x1b[?1000h");
	free(buf);
}

static void test_disable_mouse_emits_disable_sequences(void) {
	printf("test_disable_mouse_emits_disable_sequences\n");
	capture_begin();
	terminal_disable_mouse();
	size_t n;
	char *buf = capture_end(&n);
	expect_contains("disable_mouse contains 1000 disable",
	                buf, n, "\x1b[?1000l");
	expect_contains("disable_mouse contains 1006 disable",
	                buf, n, "\x1b[?1006l");
	free(buf);
}

static void test_enable_disable_round_trip(void) {
	printf("test_enable_disable_round_trip\n");
	capture_begin();
	terminal_enable_mouse();
	terminal_disable_mouse();
	size_t n;
	char *buf = capture_end(&n);
	/* Should contain both enable and disable sequences */
	expect_contains("round-trip enable 1006", buf, n, "\x1b[?1006h");
	expect_contains("round-trip disable 1006", buf, n, "\x1b[?1006l");
	free(buf);
}

int main(void) {
	TR_INIT("terminal_control_tests");
	srandom((unsigned)getpid());

	TR_RUN(test_move_to_basic);
	TR_RUN(test_move_to_origin);
	TR_RUN(test_set_attr_default);
	TR_RUN(test_set_attr_fg_only);
	TR_RUN(test_set_attr_bg);
	TR_RUN(test_set_attr_bold);
	TR_RUN(test_set_attr_inverse);
	TR_RUN(test_set_attr_underline);
	TR_RUN(test_emit_box_single);
	TR_RUN(test_emit_box_double);
	TR_RUN(test_emit_box_positions_cursor);
	TR_RUN(test_emit_box_too_small);
	TR_RUN(test_enable_mouse_emits_sgr_and_button_event);
	TR_RUN(test_disable_mouse_emits_disable_sequences);
	TR_RUN(test_enable_disable_round_trip);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
