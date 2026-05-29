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
 * terminal_control_dsr_tests.c - Behavioural tests for the DSR
 * (Device Status Report) cursor-position query added by the REPL menu
 * horizontal-cascade rework.
 *
 * Strategy: replace STDIN_FILENO with a pipe, write a synthetic
 * terminal response into the write end, and verify
 * terminal_get_cursor_pos parses it correctly.  Three cases:
 *   1. Well-formed reply -- true, out row/col populated.
 *   2. No reply (pipe stays empty) -- false, out params untouched,
 *      returns within the configured 50ms timeout window.
 *   3. Malformed reply -- false, out params untouched.
 */

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "terminal_control.h"
#include "test_report.h"

/* Replace STDIN_FILENO with a pipe's read end and return the write fd.
 * Saves the original stdin fd into *saved so the test can restore it
 * after running. */
static int redirect_stdin_to_pipe(int *saved) {
	int pipefd[2];
	int rc = pipe(pipefd);
	assert(rc == 0);

	*saved = dup(STDIN_FILENO);
	assert(*saved >= 0);

	rc = dup2(pipefd[0], STDIN_FILENO);
	assert(rc >= 0);
	close(pipefd[0]);

	return pipefd[1];
}

static void restore_stdin(int saved) {
	if (saved >= 0) {
		dup2(saved, STDIN_FILENO);
		close(saved);
	}
}

/* Elapsed milliseconds between two timeval samples (a, b). */
static long elapsed_ms(struct timeval *a, struct timeval *b) {
	long secs = b->tv_sec - a->tv_sec;
	long usec = b->tv_usec - a->tv_usec;
	return secs * 1000 + usec / 1000;
}

/* Suppress stderr so the DSR write (\x1b[6n) doesn't pollute test
 * output.  Returns the saved fd. */
static int suppress_stderr(void) {
	fflush(stderr);
	int saved = dup(STDERR_FILENO);
	assert(saved >= 0);
	int devnull = open("/dev/null", O_WRONLY);
	assert(devnull >= 0);
	dup2(devnull, STDERR_FILENO);
	close(devnull);
	return saved;
}

static void restore_stderr(int saved) {
	if (saved >= 0) {
		fflush(stderr);
		dup2(saved, STDERR_FILENO);
		close(saved);
	}
}

static void test_dsr_well_formed_reply(void) {
	printf("test_dsr_well_formed_reply\n");
	int saved_stdin = -1;
	int wfd = redirect_stdin_to_pipe(&saved_stdin);
	int saved_stderr = suppress_stderr();

	/* Prime the pipe with a well-formed DSR reply. */
	const char *reply = "\x1b[12;5R";
	ssize_t w = write(wfd, reply, strlen(reply));
	assert(w == (ssize_t)strlen(reply));

	int row = -1, col = -1;
	bool ok = terminal_get_cursor_pos(&row, &col);

	restore_stderr(saved_stderr);
	restore_stdin(saved_stdin);
	close(wfd);

	if (!ok) { printf("  FAIL: expected true, got false\n"); fflush(stdout); }
	assert(ok);
	if (row != 12) { printf("  FAIL: row=%d (want 12)\n", row); fflush(stdout); }
	assert(row == 12);
	if (col != 5) { printf("  FAIL: col=%d (want 5)\n", col); fflush(stdout); }
	assert(col == 5);
	printf("  PASS: well-formed reply parsed (row=12 col=5)\n");
}

static void test_dsr_no_reply_returns_false_quickly(void) {
	printf("test_dsr_no_reply_returns_false_quickly\n");
	int saved_stdin = -1;
	int wfd = redirect_stdin_to_pipe(&saved_stdin);
	int saved_stderr = suppress_stderr();

	int row = 99, col = 77;
	struct timeval t0, t1;
	gettimeofday(&t0, NULL);
	bool ok = terminal_get_cursor_pos(&row, &col);
	gettimeofday(&t1, NULL);

	restore_stderr(saved_stderr);
	restore_stdin(saved_stdin);
	close(wfd);

	long ms = elapsed_ms(&t0, &t1);
	if (ok) { printf("  FAIL: expected false on no reply, got true\n"); fflush(stdout); }
	assert(!ok);
	if (row != 99 || col != 77) {
		printf("  FAIL: out params clobbered (row=%d col=%d)\n", row, col);
		fflush(stdout);
	}
	assert(row == 99);
	assert(col == 77);
	/* 50ms timeout + slop for slow CI. */
	if (ms > 300) {
		printf("  FAIL: too slow: %ldms (want <300ms)\n", ms);
		fflush(stdout);
	}
	assert(ms < 300);
	printf("  PASS: no-reply returned false in %ldms (out params untouched)\n", ms);
}

static void test_dsr_malformed_reply_returns_false(void) {
	printf("test_dsr_malformed_reply_returns_false\n");
	int saved_stdin = -1;
	int wfd = redirect_stdin_to_pipe(&saved_stdin);
	int saved_stderr = suppress_stderr();

	/* Garbage that contains an ESC but does NOT terminate with 'R'. */
	const char *reply = "\x1b[XX$";
	ssize_t w = write(wfd, reply, strlen(reply));
	assert(w == (ssize_t)strlen(reply));
	close(wfd);     /* close so the read sees EOF after the garbage */

	int row = 42, col = 24;
	bool ok = terminal_get_cursor_pos(&row, &col);

	restore_stderr(saved_stderr);
	restore_stdin(saved_stdin);

	if (ok) { printf("  FAIL: expected false on malformed reply, got true\n"); fflush(stdout); }
	assert(!ok);
	if (row != 42 || col != 24) {
		printf("  FAIL: out params clobbered (row=%d col=%d)\n", row, col);
		fflush(stdout);
	}
	assert(row == 42);
	assert(col == 24);
	printf("  PASS: malformed reply returned false (out params untouched)\n");
}

int main(void) {
	TR_INIT("terminal_control_dsr_tests");
	TR_RUN(test_dsr_well_formed_reply);
	TR_RUN(test_dsr_no_reply_returns_false_quickly);
	TR_RUN(test_dsr_malformed_reply_returns_false);
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
