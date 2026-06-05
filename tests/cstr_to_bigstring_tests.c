/*
 * cstr_to_bigstring_tests.c - issue #685
 *
 * Behavioral tests for cstr_to_bigstring() truncation detection.
 *
 * Pre-#685: cstr_to_bigstring() silently truncated C strings longer than
 * 255 bytes (the Pascal-bigstring length limit) with no diagnostic. This
 * caused PR #683 bug 1: a 411-byte UserTalk script got cut mid-token at
 * boot and produced "syntax error at line 1". The fix at the time split
 * the script; the helper still silently truncated for any future caller.
 *
 * Post-#685: cstr_to_bigstring() returns bool; true on a complete copy,
 * false when the input was truncated. Callers can branch on the return
 * to log a warning, abort the operation, or otherwise surface the bug.
 *
 * The companion compile-time guard is the CSTR_TO_BIGSTRING_LIT(literal,
 * bs) macro for string-literal inputs - it _Static_assert()s that
 * sizeof(literal) <= 256, so a too-large literal fails to compile rather
 * than silently truncating at runtime. The macro is exercised by the
 * production code (frontier-cli/window_registry.c init path) and proven
 * by build; testing it here would require a separate must-fail-to-compile
 * infrastructure, which is overkill.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "frontier.h"
#include "window_registry.h"
#include "test_report.h"

static void test_empty_returns_true(void) {
	bigstring bs;
	bool ok = cstr_to_bigstring("", bs);
	assert(ok == true);
	assert(bs[0] == 0);
	printf("PASS\n");
	fflush(stdout);
}

static void test_short_returns_true(void) {
	bigstring bs;
	const char *src = "hello, world";
	bool ok = cstr_to_bigstring(src, bs);
	assert(ok == true);
	assert(bs[0] == strlen(src));
	assert(memcmp(bs + 1, src, strlen(src)) == 0);
	printf("PASS\n");
	fflush(stdout);
}

static void test_at_limit_returns_true(void) {
	/* Exactly 255 bytes fits without truncation. */
	bigstring bs;
	char src[256];
	memset(src, 'A', 255);
	src[255] = '\0';
	bool ok = cstr_to_bigstring(src, bs);
	assert(ok == true);
	assert(bs[0] == 255);
	assert(memcmp(bs + 1, src, 255) == 0);
	printf("PASS\n");
	fflush(stdout);
}

static void test_one_over_limit_returns_false(void) {
	/* 256-byte input: cstr_to_bigstring truncates to 255 and returns false. */
	bigstring bs;
	char src[257];
	memset(src, 'B', 256);
	src[256] = '\0';
	bool ok = cstr_to_bigstring(src, bs);
	assert(ok == false);
	assert(bs[0] == 255);
	assert(memcmp(bs + 1, src, 255) == 0);
	printf("PASS\n");
	fflush(stdout);
}

static void test_pr683_bug1_shape_returns_false(void) {
	/* Mirror the actual PR #683 bug 1 input length: a 411-byte UserTalk
	 * script. Pre-fix this would have silently been chopped at 255 bytes
	 * mid-content with no return value to check. */
	bigstring bs;
	char src[412];
	memset(src, 'X', 411);
	src[411] = '\0';
	bool ok = cstr_to_bigstring(src, bs);
	assert(ok == false);
	assert(bs[0] == 255);
	printf("PASS\n");
	fflush(stdout);
}

int main(void) {
	TR_INIT("cstr_to_bigstring_tests");
	printf("\n=== cstr_to_bigstring truncation detection (#685) ===\n");
	fflush(stdout);

	TR_RUN(test_empty_returns_true);
	TR_RUN(test_short_returns_true);
	TR_RUN(test_at_limit_returns_true);
	TR_RUN(test_one_over_limit_returns_false);
	TR_RUN(test_pr683_bug1_shape_returns_false);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
