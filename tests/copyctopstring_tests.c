/*
 * copyctopstring_tests.c - issue #707
 *
 * Behavioral tests for copyctopstring()'s truncation handling.
 *
 * Pre-#707: copyctopstring() (Common/source/strings.c:1241) had two
 * silent failures:
 *
 *   1. No length clamp. `short len = strlen(src); memmove(bs+1, src, len);`
 *      A >255-byte input writes past the end of the 256-byte bigstring
 *      buffer (`unsigned char[256]`, Common/headers/timedate.h:40).
 *
 *   2. setstringlength truncates len to a single byte. For a short input
 *      of 300, the length byte gets set to 44, but 300 bytes were
 *      actually written. The bigstring is internally inconsistent.
 *
 * Post-#707: copyctopstring() returns boolean (true on complete copy,
 * false on truncation) AND clamps payload to 255 bytes BEFORE memmove.
 * The length byte and the actual payload count now agree on every path.
 * Existing callers that ignore the return get the same observable
 * behavior MINUS the buffer overflow.
 *
 * The tests check the canonical buffer-state invariants:
 *   - bs[0] equals the actual payload length
 *   - No bytes beyond bs[1 + bs[0]] are modified (this is how we
 *     detect the prior buffer overflow: with no clamp, bs[256] and
 *     beyond would be clobbered)
 *   - Return value distinguishes complete vs truncated copy
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "frontier.h"
#include "strings.h"
#include "test_report.h"

/*
 * Wrap the bigstring in a slightly larger buffer with sentinel bytes
 * on both sides so any out-of-bounds write is detectable. The
 * `unsigned char[256]` typedef can't accept an unaligned overflow
 * sentinel inline, so place sentinels outside.
 */
struct guarded_bigstring {
	unsigned char pad_before[16];
	bigstring bs;
	unsigned char pad_after[16];
};

static void init_guarded(struct guarded_bigstring *g) {
	memset(g->pad_before, 0xAA, sizeof(g->pad_before));
	memset(g->bs, 0xBB, sizeof(g->bs));
	memset(g->pad_after, 0xCC, sizeof(g->pad_after));
}

static void assert_no_overflow(const struct guarded_bigstring *g) {
	for (size_t i = 0; i < sizeof(g->pad_before); i++)
		assert(g->pad_before[i] == 0xAA);
	for (size_t i = 0; i < sizeof(g->pad_after); i++)
		assert(g->pad_after[i] == 0xCC);
}

static void test_empty_returns_true(void) {
	struct guarded_bigstring g;
	init_guarded(&g);
	boolean ok = copyctopstring("", g.bs);
	assert(ok == true);
	assert(g.bs[0] == 0);
	assert_no_overflow(&g);
	printf("PASS\n");
	fflush(stdout);
}

static void test_short_returns_true(void) {
	struct guarded_bigstring g;
	init_guarded(&g);
	const char *src = "hello, world";
	boolean ok = copyctopstring(src, g.bs);
	assert(ok == true);
	assert(g.bs[0] == strlen(src));
	assert(memcmp(g.bs + 1, src, strlen(src)) == 0);
	assert_no_overflow(&g);
	printf("PASS\n");
	fflush(stdout);
}

static void test_at_limit_returns_true(void) {
	struct guarded_bigstring g;
	init_guarded(&g);
	char src[256];
	memset(src, 'A', 255);
	src[255] = '\0';
	boolean ok = copyctopstring(src, g.bs);
	assert(ok == true);
	assert(g.bs[0] == 255);
	assert(memcmp(g.bs + 1, src, 255) == 0);
	assert_no_overflow(&g);
	printf("PASS\n");
	fflush(stdout);
}

static void test_one_over_limit_returns_false_no_overflow(void) {
	/*
	 * Issue #707 regression guard: pre-fix this test would FAIL
	 * `assert_no_overflow` because the 256-byte input would write
	 * `pad_after[0]` (i.e. bs[256]). It would also FAIL the length-
	 * agrees check because bs[0] would be 0 (length 256 cast to
	 * unsigned char) while 256 bytes were written.
	 */
	struct guarded_bigstring g;
	init_guarded(&g);
	char src[257];
	memset(src, 'B', 256);
	src[256] = '\0';
	boolean ok = copyctopstring(src, g.bs);
	assert(ok == false);
	assert(g.bs[0] == 255);
	assert(memcmp(g.bs + 1, src, 255) == 0);
	assert_no_overflow(&g);
	printf("PASS\n");
	fflush(stdout);
}

static void test_pr683_bug1_shape_no_overflow(void) {
	/*
	 * The original PR #683 bug 1 shape: 411-byte UserTalk script.
	 * Pre-fix would write 411 bytes into a 256-byte buffer, then set
	 * the length byte to (411 & 0xFF) = 155. Post-fix: 255 bytes
	 * stored, length byte 255, return false.
	 */
	struct guarded_bigstring g;
	init_guarded(&g);
	char src[412];
	memset(src, 'X', 411);
	src[411] = '\0';
	boolean ok = copyctopstring(src, g.bs);
	assert(ok == false);
	assert(g.bs[0] == 255);
	assert(memcmp(g.bs + 1, src, 255) == 0);
	assert_no_overflow(&g);
	printf("PASS\n");
	fflush(stdout);
}

static void test_length_byte_agrees_with_payload(void) {
	/*
	 * Pre-fix #707: length byte and payload could disagree because
	 * len was a `short` (300 fits) but setstringlength masked to
	 * unsigned char (300 -> 44). Post-fix: bs[0] always reflects
	 * exactly what was written.
	 */
	struct guarded_bigstring g;
	init_guarded(&g);
	char src[301];
	memset(src, 'C', 300);
	src[300] = '\0';
	(void)copyctopstring(src, g.bs);
	/* The length byte must equal the number of payload bytes that
	 * are actually in bs[1..bs[0]]. */
	for (int i = 1; i <= (int)g.bs[0]; i++)
		assert(g.bs[i] == 'C');
	/* Bytes beyond bs[0] must not have been touched (still 0xBB
	 * from init_guarded). The buffer is 256 bytes total. */
	for (int i = (int)g.bs[0] + 1; i < 256; i++)
		assert(g.bs[i] == 0xBB);
	assert_no_overflow(&g);
	printf("PASS\n");
	fflush(stdout);
}

int main(void) {
	TR_INIT("copyctopstring_tests");
	printf("\n=== copyctopstring buffer-overflow detection (#707) ===\n");
	fflush(stdout);

	TR_RUN(test_empty_returns_true);
	TR_RUN(test_short_returns_true);
	TR_RUN(test_at_limit_returns_true);
	TR_RUN(test_one_over_limit_returns_false_no_overflow);
	TR_RUN(test_pr683_bug1_shape_no_overflow);
	TR_RUN(test_length_byte_agrees_with_payload);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
