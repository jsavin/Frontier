/*
 * input_decoder_tests.c -- PTY-replay harness for the boxen input decoder.
 *
 * 2026-06-29 JES #809 M1: harness infrastructure + create/destroy/inject smoke
 * tests.  Protocol coverage tests (cursor keys, mouse, paste, Kitty) are listed
 * here as SKIP stubs that will be un-skipped one milestone at a time
 * (M2 through M5; see planning/phase_c/INPUT_DECODER_PLAN.md section 6.2).
 *
 * 2026-06-30 JES #810 M2: un-SKIP the M2-scope behavioral tests --
 * CSI cursor keys, SS3 cursor keys, modifier params 2-16, nav keys with
 * modifiers, function keys (SS3 + CSI forms with modifiers), ESC-alone,
 * ESC-prefix Meta, partial-sequence split, control characters 0x01-0x1A,
 * UTF-8 multi-byte decode.  M3/M4/M5 stubs remain SKIP.
 *
 * SKIP mechanism note (no TR_SKIP exists in test_report.h):
 *   The TR_RUN macro records a test as "passed" if its body returns without
 *   firing assert().  Each protocol stub below prints a SKIP line to stderr
 *   and returns; TR_RUN records a green pass.  Two accounting signals keep
 *   the deferral visible despite the green tally:
 *     (a) Every SKIP-stub function is named "test_skip_*" so a grep over
 *         tests/tmp/unit/input_decoder_tests.json discloses the deferred
 *         set without parsing stderr.
 *     (b) main() prints "[SUMMARY] N stub tests deferred to M2-M5" before
 *         the TR_SUMMARY line, sourced from the g_skip_count counter that
 *         tr_skip() increments.
 *   Each SKIP test will be rewritten as a behavioral assert when its
 *   milestone lands; that's the un-skip.  At that point the test_skip_
 *   prefix should be dropped along with the tr_skip() call.
 *
 * The harness uses the INPUT_DECODER_TEST_SEAM compile flag to expose
 * input_decoder_inject_bytes() and input_decoder_buffered_bytes(), so the
 * tests can drive bytes into the decoder without opening a real PTY.  This
 * is the "PTY-replay" pattern from plan section 6.1: a recorded byte
 * sequence (xxd -i'd into a C array) is replayed through inject_bytes;
 * poll() then advances the state machine and emits a boxen_event_t.
 *
 * Pattern mirrors other boxen unit-test binaries (boxen_backend_tests.c,
 * boxen_repl_tests.c): TR_INIT / TR_RUN / TR_SUMMARY / TR_EXIT_CODE.
 *
 * Link slice: input_decoder.c only (no termbox2, no Frontier runtime).  See
 * tests/Makefile.
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../frontier-cli/boxen/boxen.h"
#include "../frontier-cli/boxen/input_decoder.h"

/* Test harness. */
#include "test_report.h"

/* -------------------------------------------------------------------------
 * SKIP helper -- documented in the file header.
 *
 * Prints "[SKIP] <reason>" to stderr and returns.  TR_RUN then records the
 * surrounding test function as passing.  Caller is expected to invoke this
 * as the first statement of the test body and then return immediately
 * (early-return pattern, not setjmp) so the stub does NOT touch any decoder
 * state that the M2+ implementation has not yet built.
 *
 * Accounting: g_skip_count is bumped on every tr_skip() call so the
 * end-of-run summary can report "N of M tests were stubs deferred to
 * milestones M2-M5" rather than a flat "M/M green" that conceals
 * unimplemented behavior.  Naming convention: every skip-stub test
 * function is named test_skip_* -- a grep over the JSON tally for
 * "test_skip_" produces the deferred set without parsing stderr.
 *
 * 2026-06-29 JES #809 M1. */
static int g_skip_count = 0;
static void tr_skip(const char *reason) {
	g_skip_count++;
	fprintf(stderr, "[SKIP] %s\n", reason);
}

/* 2026-06-30 JES #810 M2: shared helper -- inject a byte sequence and poll
 * once, asserting the return code.  Keeps each behavioral test below short
 * and uniform.  The non-blocking timeout (0) is intentional: every M2 test
 * supplies a complete sequence (or deliberately partial sequence) up front,
 * and the state machine must drain the buffer synchronously. */
static void inject_and_poll(input_decoder_t *dec,
                            const uint8_t *bytes, size_t len,
                            int expected_rc, boxen_event_t *out) {
	if (len > 0) {
		size_t accepted = input_decoder_inject_bytes(dec, bytes, len);
		assert(accepted == len);
	}
	memset(out, 0x7f, sizeof(*out));   /* poison to verify zero-on-timeout */
	int rc = input_decoder_poll(dec, out, 0);
	assert(rc == expected_rc);
}

/* -------------------------------------------------------------------------
 * M1 harness smoke tests (PASS today)
 *
 * These exercise the API shape we landed in M1.  They do NOT depend on the
 * state machine and must keep passing through every subsequent milestone --
 * a regression here means the lifecycle / inject contract broke.
 * ---------------------------------------------------------------------- */

/* 2026-06-29 JES #809 M1: create/destroy round trip with a -1 fd.
 *
 * The PTY-replay harness uses tty_fd=-1 to signal "do not open a real
 * descriptor".  The decoder must accept that and not attempt any I/O. */
static void test_create_destroy_smoke(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	assert(dec != NULL);
	input_decoder_destroy(dec);
}

/* 2026-06-29 JES #809 M1: destroying NULL is a no-op (defensive contract). */
static void test_destroy_null_is_noop(void) {
	input_decoder_destroy(NULL);
	/* Reaching this line without crashing is the assertion. */
}

/* 2026-06-29 JES #809 M1: mouse-enable bit starts false and round-trips.
 *
 * Per plan section 5.1: "Mouse mode is disabled on startup."  The M1 stub
 * records the requested state but writes nothing; M3 / M4 wire the real
 * escape-sequence writes. */
static void test_mouse_disabled_by_default(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	assert(dec != NULL);
	assert(input_decoder_mouse_enabled(dec) == false);

	input_decoder_set_mouse(dec, true);
	assert(input_decoder_mouse_enabled(dec) == true);

	input_decoder_set_mouse(dec, false);
	assert(input_decoder_mouse_enabled(dec) == false);

	input_decoder_destroy(dec);
}

/* 2026-06-29 JES #809 M1: mouse-enabled query on NULL returns false. */
static void test_mouse_enabled_null_safe(void) {
	assert(input_decoder_mouse_enabled(NULL) == false);
}

/* 2026-06-29 JES #809 M1: poll on a freshly-created decoder returns TIMEOUT
 * and zeroes the out parameter.
 *
 * This is the M1 contract from plan section 7: "poll returns
 * BOXEN_ERR_TIMEOUT" always, until M2 lands the state machine. */
static void test_poll_empty_buffer_returns_timeout(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	assert(dec != NULL);

	boxen_event_t ev;
	/* Pre-fill with junk so we can verify poll zeroed it. */
	memset(&ev, 0x7f, sizeof(ev));

	int r = input_decoder_poll(dec, &ev, 0);
	assert(r == BOXEN_ERR_TIMEOUT);
	assert(ev.type == BOXEN_EV_NONE);

	input_decoder_destroy(dec);
}

/* 2026-06-29 JES #809 M1: poll with NULL decoder is INVALID. */
static void test_poll_null_decoder_is_invalid(void) {
	boxen_event_t ev;
	int r = input_decoder_poll(NULL, &ev, 0);
	assert(r == BOXEN_ERR_INVALID);
}

/* 2026-06-29 JES #809 M1: poll with NULL out param is INVALID. */
static void test_poll_null_out_is_invalid(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	assert(dec != NULL);

	int r = input_decoder_poll(dec, NULL, 0);
	assert(r == BOXEN_ERR_INVALID);

	input_decoder_destroy(dec);
}

/* 2026-06-29 JES #809 M1: inject_bytes deposits exactly the requested bytes
 * into the ring buffer.  Verified via the test-only buffered_bytes accessor.
 *
 * This is the contract M2 will build the state machine against: a sequence
 * injected by the test must end up in the buffer in the order it was given,
 * with no implicit drain. */
static void test_inject_bytes_appends_to_buffer(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	assert(dec != NULL);
	assert(input_decoder_buffered_bytes(dec) == 0);
	assert(input_decoder_dropped_bytes(dec) == 0);

	static const uint8_t seq[] = {0x1b, '[', '1', ';', '3', 'D'};
	size_t accepted = input_decoder_inject_bytes(dec, seq, sizeof(seq));
	assert(accepted == sizeof(seq));
	assert(input_decoder_buffered_bytes(dec) == sizeof(seq));
	assert(input_decoder_dropped_bytes(dec) == 0);

	/* Inject more: count accumulates, no drops on a non-full buffer. */
	static const uint8_t more[] = {'a', 'b', 'c'};
	accepted = input_decoder_inject_bytes(dec, more, sizeof(more));
	assert(accepted == sizeof(more));
	assert(input_decoder_buffered_bytes(dec) == sizeof(seq) + sizeof(more));
	assert(input_decoder_dropped_bytes(dec) == 0);

	input_decoder_destroy(dec);
}

/* 2026-06-29 JES #809 M1: inject_bytes with len=0 or NULL bytes is a no-op,
 * not a crash.  Defensive against future callers that compute (ptr, len)
 * from a parsed buffer slice. */
static void test_inject_bytes_noop_on_empty(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	assert(dec != NULL);

	assert(input_decoder_inject_bytes(dec, NULL, 0) == 0);
	assert(input_decoder_buffered_bytes(dec) == 0);

	static const uint8_t nothing[1] = {0};
	assert(input_decoder_inject_bytes(dec, nothing, 0) == 0);
	assert(input_decoder_buffered_bytes(dec) == 0);

	input_decoder_destroy(dec);
}

/* 2026-06-29 JES #809 M1: inject_bytes on a NULL decoder is a no-op and
 * returns 0 accepted. */
static void test_inject_bytes_null_decoder_safe(void) {
	static const uint8_t seq[] = {0x1b};
	assert(input_decoder_inject_bytes(NULL, seq, sizeof(seq)) == 0);
}

/* 2026-06-29 JES #809 M1: buffered_bytes / dropped_bytes on NULL return 0. */
static void test_buffered_bytes_null_safe(void) {
	assert(input_decoder_buffered_bytes(NULL) == 0);
	assert(input_decoder_dropped_bytes(NULL) == 0);
}

/* 2026-06-29 JES #809 M1: kitty_enable round-trip via the symmetric query
 * (mirrors mouse_enabled round-trip).  Verifies the bit flip is observable,
 * the buffer is unaffected (no loop-back via inject), and NULL safety. */
static void test_kitty_enable_round_trip(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	assert(dec != NULL);
	assert(input_decoder_kitty_enabled(dec) == false);

	input_decoder_kitty_enable(dec);
	assert(input_decoder_kitty_enabled(dec) == true);
	assert(input_decoder_buffered_bytes(dec) == 0);

	/* NULL-safety on both sides. */
	input_decoder_kitty_enable(NULL);
	assert(input_decoder_kitty_enabled(NULL) == false);

	input_decoder_destroy(dec);
}

/* 2026-06-29 JES #809 M1: inject overflow surfaces via dropped_bytes.
 *
 * The M1 back-pressure contract: inject_bytes returns the number of bytes
 * accepted, and any remainder is counted in input_decoder_dropped_bytes().
 * This regression-guards the test seam (and the future M2/M3 read(2) loop
 * that will share the counter) against silent overflow desync.  See plan
 * section 3.12 (burst-read robustness). */
static void test_inject_overflow_increments_dropped_counter(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	assert(dec != NULL);
	assert(input_decoder_dropped_bytes(dec) == 0);

	/* The ring is 4 KiB.  Inject 5 KiB: 4 KiB lands, 1 KiB drops. */
	enum { ATTEMPT = 5 * 1024 };
	static uint8_t flood[ATTEMPT];
	memset(flood, 'X', sizeof(flood));

	size_t accepted = input_decoder_inject_bytes(dec, flood, sizeof(flood));
	assert(accepted <= sizeof(flood));
	assert(accepted == input_decoder_buffered_bytes(dec));
	size_t dropped = input_decoder_dropped_bytes(dec);
	assert(dropped > 0);
	assert(accepted + dropped == sizeof(flood));

	/* Subsequent inject when buffer is full: every byte drops, counter
	 * accumulates monotonically. */
	static const uint8_t more[] = {'Y', 'Z'};
	size_t accepted2 = input_decoder_inject_bytes(dec, more, sizeof(more));
	assert(accepted2 == 0);
	assert(input_decoder_dropped_bytes(dec) == dropped + sizeof(more));

	input_decoder_destroy(dec);
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: CSI plain cursor keys (plan section 3.1)
 * ---------------------------------------------------------------------- */

static void test_csi_plain_arrow_up(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', 'A'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_UP);
	assert(ev.key.mod == BOXEN_MOD_NONE);
	assert(ev.key.ch == 0);
	input_decoder_destroy(dec);
}

static void test_csi_plain_arrow_down(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', 'B'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_DOWN);
	assert(ev.key.mod == BOXEN_MOD_NONE);
	input_decoder_destroy(dec);
}

static void test_csi_plain_arrow_right(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', 'C'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_RIGHT);
	assert(ev.key.mod == BOXEN_MOD_NONE);
	input_decoder_destroy(dec);
}

static void test_csi_plain_arrow_left(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', 'D'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_LEFT);
	assert(ev.key.mod == BOXEN_MOD_NONE);
	input_decoder_destroy(dec);
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: SS3 cursor keys (plan section 3.1)
 * ---------------------------------------------------------------------- */

static void test_ss3_arrow_up(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 'O', 'A'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_UP);
	assert(ev.key.mod == BOXEN_MOD_NONE);
	input_decoder_destroy(dec);
}

static void test_ss3_arrow_down(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 'O', 'B'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_DOWN);
	input_decoder_destroy(dec);
}

static void test_ss3_arrow_right(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 'O', 'C'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_RIGHT);
	input_decoder_destroy(dec);
}

static void test_ss3_arrow_left(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 'O', 'D'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_LEFT);
	input_decoder_destroy(dec);
}

static void test_ss3_home(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 'O', 'H'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_HOME);
	input_decoder_destroy(dec);
}

static void test_ss3_end(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 'O', 'F'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_END);
	input_decoder_destroy(dec);
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: CSI modifier params 2..16 -- the #805 fix.
 *
 * Each test exercises one slot of the 16-entry modifier table in plan
 * section 3.2.  Param N maps to a bitmask via modifier_from_param: subtract
 * 1, then bit 0=SHIFT, bit 1=ALT, bit 2=CTRL, bit 3=META.  N=9 (META) is
 * the bit that termbox2 silently dropped on Terminal.app and is the
 * specific root cause behind issue #805.
 * ---------------------------------------------------------------------- */

/* Helper: run one CSI cursor-key test with given modifier param N and
 * direction letter, asserting the expected modifier bitmask. */
static void check_csi_arrow_modifier(uint8_t direction, boxen_key_t key,
                                     int n, uint16_t expected_mod) {
	input_decoder_t *dec = input_decoder_create(-1);
	uint8_t seq[16];
	int len = snprintf((char *)seq, sizeof(seq), "\x1b[1;%d%c", n, direction);
	assert(len > 0 && (size_t)len < sizeof(seq));
	boxen_event_t ev;
	inject_and_poll(dec, seq, (size_t)len, BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == key);
	assert(ev.key.mod == expected_mod);
	input_decoder_destroy(dec);
}

/* The 16 modifier slots for all four arrow directions.  Plan section 3.2
 * declares the full table; this validates every entry to prevent silent
 * regressions on the bits termbox2 used to drop (notably N=9 META). */
static void test_csi_modifier_all_params_arrow_up(void) {
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 2, BOXEN_MOD_SHIFT);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 3, BOXEN_MOD_ALT);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 4, BOXEN_MOD_ALT | BOXEN_MOD_SHIFT);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 5, BOXEN_MOD_CTRL);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 6, BOXEN_MOD_CTRL | BOXEN_MOD_SHIFT);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 7, BOXEN_MOD_ALT | BOXEN_MOD_CTRL);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 8,
	                         BOXEN_MOD_ALT | BOXEN_MOD_CTRL | BOXEN_MOD_SHIFT);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 9, BOXEN_MOD_META);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 10, BOXEN_MOD_META | BOXEN_MOD_SHIFT);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 11, BOXEN_MOD_META | BOXEN_MOD_ALT);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 12,
	                         BOXEN_MOD_META | BOXEN_MOD_ALT | BOXEN_MOD_SHIFT);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 13, BOXEN_MOD_META | BOXEN_MOD_CTRL);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 14,
	                         BOXEN_MOD_META | BOXEN_MOD_CTRL | BOXEN_MOD_SHIFT);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 15,
	                         BOXEN_MOD_META | BOXEN_MOD_ALT | BOXEN_MOD_CTRL);
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 16,
	                         BOXEN_MOD_META | BOXEN_MOD_ALT | BOXEN_MOD_CTRL |
	                             BOXEN_MOD_SHIFT);
}

static void test_csi_modifier_all_params_arrow_down(void) {
	check_csi_arrow_modifier('B', BOXEN_KEY_DOWN, 2, BOXEN_MOD_SHIFT);
	check_csi_arrow_modifier('B', BOXEN_KEY_DOWN, 9, BOXEN_MOD_META);
	check_csi_arrow_modifier('B', BOXEN_KEY_DOWN, 16,
	                         BOXEN_MOD_META | BOXEN_MOD_ALT | BOXEN_MOD_CTRL |
	                             BOXEN_MOD_SHIFT);
}

static void test_csi_modifier_all_params_arrow_right(void) {
	check_csi_arrow_modifier('C', BOXEN_KEY_RIGHT, 2, BOXEN_MOD_SHIFT);
	check_csi_arrow_modifier('C', BOXEN_KEY_RIGHT, 3, BOXEN_MOD_ALT);
	check_csi_arrow_modifier('C', BOXEN_KEY_RIGHT, 9, BOXEN_MOD_META);
}

static void test_csi_modifier_all_params_arrow_left(void) {
	check_csi_arrow_modifier('D', BOXEN_KEY_LEFT, 2, BOXEN_MOD_SHIFT);
	check_csi_arrow_modifier('D', BOXEN_KEY_LEFT, 3, BOXEN_MOD_ALT);
	check_csi_arrow_modifier('D', BOXEN_KEY_LEFT, 9, BOXEN_MOD_META);
}

/* Spot tests for the well-known #805 sequences -- explicitly named so
 * grep'ing for "issue_805" surfaces the smoking-gun cases. */
static void test_csi_modifier_alt_left_issue_805(void) {
	check_csi_arrow_modifier('D', BOXEN_KEY_LEFT, 3, BOXEN_MOD_ALT);
}

static void test_csi_modifier_alt_right_issue_805(void) {
	check_csi_arrow_modifier('C', BOXEN_KEY_RIGHT, 3, BOXEN_MOD_ALT);
}

static void test_csi_modifier_shift_up(void) {
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 2, BOXEN_MOD_SHIFT);
}

static void test_csi_modifier_ctrl_down(void) {
	check_csi_arrow_modifier('B', BOXEN_KEY_DOWN, 5, BOXEN_MOD_CTRL);
}

static void test_csi_modifier_alt_shift_left(void) {
	check_csi_arrow_modifier('D', BOXEN_KEY_LEFT, 4, BOXEN_MOD_ALT | BOXEN_MOD_SHIFT);
}

static void test_csi_modifier_meta_up(void) {
	check_csi_arrow_modifier('A', BOXEN_KEY_UP, 9, BOXEN_MOD_META);
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: CSI nav keys -- Home/End/Ins/Del/PgUp/PgDn
 * (plan section 3.3), with modifiers.
 * ---------------------------------------------------------------------- */

static void test_csi_nav_home(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', 'H'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_HOME);
	assert(ev.key.mod == BOXEN_MOD_NONE);
	input_decoder_destroy(dec);
}

static void test_csi_nav_end(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', 'F'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_END);
	input_decoder_destroy(dec);
}

static void test_csi_nav_home_with_alt(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '1', ';', '3', 'H'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_HOME);
	assert(ev.key.mod == BOXEN_MOD_ALT);
	input_decoder_destroy(dec);
}

static void test_csi_nav_end_with_ctrl(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '1', ';', '5', 'F'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_END);
	assert(ev.key.mod == BOXEN_MOD_CTRL);
	input_decoder_destroy(dec);
}

static void test_csi_nav_insert(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '2', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_INSERT);
	assert(ev.key.mod == BOXEN_MOD_NONE);
	input_decoder_destroy(dec);
}

static void test_csi_nav_delete(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '3', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_DELETE);
	input_decoder_destroy(dec);
}

static void test_csi_nav_pgup(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '5', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_PGUP);
	input_decoder_destroy(dec);
}

static void test_csi_nav_pgdn(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '6', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_PGDN);
	input_decoder_destroy(dec);
}

static void test_csi_nav_delete_with_shift(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '3', ';', '2', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_DELETE);
	assert(ev.key.mod == BOXEN_MOD_SHIFT);
	input_decoder_destroy(dec);
}

static void test_csi_nav_pgup_with_meta(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '5', ';', '9', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_PGUP);
	assert(ev.key.mod == BOXEN_MOD_META);
	input_decoder_destroy(dec);
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: function keys -- SS3 (F1-F4) + CSI (F1-F12)
 * with modifiers (plan section 3.4).
 * ---------------------------------------------------------------------- */

static void test_function_key_f1_ss3(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 'O', 'P'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_F1);
	assert(ev.key.mod == BOXEN_MOD_NONE);
	input_decoder_destroy(dec);
}

static void test_function_key_f2_ss3(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 'O', 'Q'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_F2);
	input_decoder_destroy(dec);
}

static void test_function_key_f3_ss3(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 'O', 'R'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_F3);
	input_decoder_destroy(dec);
}

static void test_function_key_f4_ss3(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 'O', 'S'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_F4);
	input_decoder_destroy(dec);
}

/* CSI F1..F4 via \e[1;NP|Q|R|S form -- modifier-only variants. */
static void test_function_key_f1_shift(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '1', ';', '2', 'P'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_F1);
	assert(ev.key.mod == BOXEN_MOD_SHIFT);
	input_decoder_destroy(dec);
}

static void test_function_key_f4_alt(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '1', ';', '3', 'S'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_F4);
	assert(ev.key.mod == BOXEN_MOD_ALT);
	input_decoder_destroy(dec);
}

/* CSI ~-form F-keys: F1 legacy (\e[11~), F5 (\e[15~), F12 (\e[24~). */
static void test_function_key_f1_csi_legacy(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '1', '1', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_F1);
	input_decoder_destroy(dec);
}

static void test_function_key_f5_csi(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '1', '5', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_F5);
	input_decoder_destroy(dec);
}

static void test_function_key_f6_csi(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '1', '7', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_F6);
	input_decoder_destroy(dec);
}

static void test_function_key_f11_csi(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '2', '3', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_F11);
	input_decoder_destroy(dec);
}

static void test_function_key_f12_csi(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '2', '4', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_F12);
	input_decoder_destroy(dec);
}

/* CSI ~-form F-key with modifier: \e[15;5~ -> F5 + CTRL. */
static void test_function_key_f5_with_ctrl(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '1', '5', ';', '5', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_F5);
	assert(ev.key.mod == BOXEN_MOD_CTRL);
	input_decoder_destroy(dec);
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: ESC-prefix Meta keys (plan section 3.5)
 *
 * Policy: \ea / \eb / \ef emit ch=letter, mod=ALT.  The decoder does NOT
 * conflate \eb with \e[1;3D (word-left); the REPL keymap can bind both
 * to the same action if desired but the decoder keeps them distinct.
 * ---------------------------------------------------------------------- */

static void test_meta_prefix_alt_a(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 'a'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_NONE);
	assert(ev.key.ch == 'a');
	assert(ev.key.mod == BOXEN_MOD_ALT);
	input_decoder_destroy(dec);
}

static void test_meta_prefix_alt_b(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 'b'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.ch == 'b');
	assert(ev.key.mod == BOXEN_MOD_ALT);
	input_decoder_destroy(dec);
}

static void test_meta_prefix_alt_f(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 'f'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.ch == 'f');
	assert(ev.key.mod == BOXEN_MOD_ALT);
	input_decoder_destroy(dec);
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: ESC-alone disambiguation (plan section 4.4)
 *
 * Test-seam contract: when poll is called with timeout_ms==0 and the buffer
 * holds only 0x1B with nothing following, the decoder treats it as a lone
 * Escape and emits KEY_ESCAPE.  The production read(2) path will refine
 * this with a non-blocking drain (M6 wiring) but the semantic is the same:
 * "no more bytes are coming after the ESC" -> emit ESCAPE.
 * ---------------------------------------------------------------------- */

static void test_esc_alone_emits_escape(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_ESCAPE);
	assert(ev.key.mod == BOXEN_MOD_NONE);
	input_decoder_destroy(dec);
}

/* Double ESC: the first 0x1B with another 0x1B following is the case where
 * the user pressed Escape twice in rapid succession.  The decoder emits one
 * KEY_ESCAPE for the first; the second 0x1B becomes a new ESC_RECEIVED that
 * resolves on the next poll (no follow-up byte) to a second ESCAPE. */
static void test_esc_then_esc_emits_two_escapes(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, 0x1b};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_ESCAPE);

	/* Second poll resolves the trailing ESC. */
	memset(&ev, 0x7f, sizeof(ev));
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_ESCAPE);

	input_decoder_destroy(dec);
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: partial-sequence split across inject boundary
 * (plan sections 3.12, 6.1).  The decoder MUST hold a partial CSI sequence
 * in its buffer and emit nothing until the final byte arrives.  This is
 * the contract that fixes the read(2)-boundary fall-through identified in
 * the pre-plan investigation.
 * ---------------------------------------------------------------------- */

static void test_partial_sequence_across_inject_boundary(void) {
	input_decoder_t *dec = input_decoder_create(-1);

	/* First half: \e[1;  -- a clearly-partial CSI sequence. */
	static const uint8_t half1[] = {0x1b, '[', '1', ';'};
	size_t accepted = input_decoder_inject_bytes(dec, half1, sizeof(half1));
	assert(accepted == sizeof(half1));

	boxen_event_t ev;
	memset(&ev, 0x7f, sizeof(ev));
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_ERR_TIMEOUT);
	assert(ev.type == BOXEN_EV_NONE);

	/* Second half: 3D -- completes \e[1;3D = LEFT + ALT. */
	static const uint8_t half2[] = {'3', 'D'};
	accepted = input_decoder_inject_bytes(dec, half2, sizeof(half2));
	assert(accepted == sizeof(half2));

	memset(&ev, 0x7f, sizeof(ev));
	rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_LEFT);
	assert(ev.key.mod == BOXEN_MOD_ALT);

	input_decoder_destroy(dec);
}

/* Partial split inside an SS3 sequence: \eO then poll (TIMEOUT) then A. */
static void test_partial_ss3_across_inject_boundary(void) {
	input_decoder_t *dec = input_decoder_create(-1);

	static const uint8_t half1[] = {0x1b, 'O'};
	input_decoder_inject_bytes(dec, half1, sizeof(half1));

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_ERR_TIMEOUT);

	static const uint8_t half2[] = {'A'};
	input_decoder_inject_bytes(dec, half2, sizeof(half2));
	rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_UP);

	input_decoder_destroy(dec);
}

/* Partial split right after ESC: inject \e alone -- this is the ambiguous
 * case.  With the test seam (no read loop), poll with timeout=0 must NOT
 * eagerly emit ESCAPE -- doing so would race a still-arriving CSI prefix
 * on the production fd path.
 *
 * The contract: when the buffer's tail is a lone ESC and the poll caller
 * supplied timeout_ms==0, the decoder DOES emit KEY_ESCAPE.  This matches
 * plan section 4.4: the production read loop drains the pipe non-blocking
 * after seeing ESC; if zero bytes follow, emit ESCAPE.  In the test seam,
 * the equivalent semantic is "the caller's poll boundary is the end of
 * available bytes."  test_esc_alone_emits_escape pins this.
 *
 * This separate test pins the inverse case: ESC followed by '[' but no
 * final byte yet must NOT emit anything (the '[' commits to CSI_COLLECTING,
 * which is unambiguously a sequence-in-progress). */
static void test_partial_esc_bracket_does_not_emit(void) {
	input_decoder_t *dec = input_decoder_create(-1);

	static const uint8_t partial[] = {0x1b, '['};
	input_decoder_inject_bytes(dec, partial, sizeof(partial));

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_ERR_TIMEOUT);
	assert(ev.type == BOXEN_EV_NONE);

	/* Now finish with 'A'. */
	static const uint8_t finish[] = {'A'};
	input_decoder_inject_bytes(dec, finish, sizeof(finish));
	rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.key.key == BOXEN_KEY_UP);

	input_decoder_destroy(dec);
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: control characters 0x01..0x1A and DEL (plan
 * section 3.11).
 * ---------------------------------------------------------------------- */

static void test_control_char_ctrl_a(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x01};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_CTRL_A);
	assert(ev.key.ch == 0);
	input_decoder_destroy(dec);
}

static void test_control_char_ctrl_c(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x03};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_CTRL_C);
	input_decoder_destroy(dec);
}

static void test_control_char_ctrl_e(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x05};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_CTRL_E);
	input_decoder_destroy(dec);
}

static void test_control_char_ctrl_z(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1a};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_CTRL_Z);
	input_decoder_destroy(dec);
}

static void test_control_char_tab(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x09};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_TAB);
	input_decoder_destroy(dec);
}

static void test_control_char_enter(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x0d};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_ENTER);
	input_decoder_destroy(dec);
}

static void test_control_char_backspace_ctrl_h(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x08};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_BACKSPACE);
	input_decoder_destroy(dec);
}

static void test_control_char_backspace_del(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x7f};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_BACKSPACE);
	input_decoder_destroy(dec);
}

/* Printable ASCII -- the 7-bit non-control path.  ch=letter, mod=NONE,
 * key=BOXEN_KEY_NONE so the consumer distinguishes "printable" from
 * "special". */
static void test_printable_ascii(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {'A'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_NONE);
	assert(ev.key.ch == 'A');
	assert(ev.key.mod == BOXEN_MOD_NONE);
	input_decoder_destroy(dec);
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: UTF-8 multi-byte assembly (plan section 3.10).
 *
 * The decoder accumulates UTF-8 continuation bytes in a per-codepoint
 * scratch and emits one event with ev.key.ch = assembled codepoint when
 * complete.  Lead-then-cont split across inject must NOT emit until the
 * final byte arrives.
 * ---------------------------------------------------------------------- */

/* U+00A9 = COPYRIGHT SIGN, encoded as 0xC2 0xA9. */
static void test_utf8_two_byte_copyright(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0xC2, 0xA9};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_NONE);
	assert(ev.key.ch == 0xA9);
	input_decoder_destroy(dec);
}

/* U+2014 = EM DASH, encoded as 0xE2 0x80 0x94. */
static void test_utf8_three_byte_em_dash(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0xE2, 0x80, 0x94};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.ch == 0x2014);
	input_decoder_destroy(dec);
}

/* U+1F600 = GRINNING FACE, encoded as 0xF0 0x9F 0x98 0x80. */
static void test_utf8_four_byte_emoji(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0xF0, 0x9F, 0x98, 0x80};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.ch == 0x1F600);
	input_decoder_destroy(dec);
}

/* UTF-8 continuation split across inject calls.  Inject 0xE2 0x80 (two of
 * three em-dash bytes); poll returns TIMEOUT.  Inject final 0x94; poll
 * returns ch=0x2014. */
static void test_utf8_continuation_split_across_inject(void) {
	input_decoder_t *dec = input_decoder_create(-1);

	static const uint8_t half1[] = {0xE2, 0x80};
	input_decoder_inject_bytes(dec, half1, sizeof(half1));

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_ERR_TIMEOUT);
	assert(ev.type == BOXEN_EV_NONE);

	static const uint8_t half2[] = {0x94};
	input_decoder_inject_bytes(dec, half2, sizeof(half2));
	rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.ch == 0x2014);

	input_decoder_destroy(dec);
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2 review-followup: gate review fixes.
 *
 * P1 hardening tests (CSI overflow byte-injection, UTF-8 overlong /
 * surrogate / out-of-range smuggling) and the bar-raiser-flagged
 * coverage gaps (F7-F10 individual tests, full CTRL sweep, two-event
 * burst in one inject, 0x00/0x1C controls, stray UTF-8 continuation).
 * ---------------------------------------------------------------------- */

/* P1-1: CSI buffer overflow used to reset to GROUND with the remaining
 * sequence bytes still in the buffer, which got emitted as printable
 * keystrokes.  The fix transitions to CSI_SWALLOW and drops bytes until
 * the final byte arrives.  Inject a CSI with > 64 param bytes followed by
 * a single 'B' printable, then poll: the only event should be 'B', and
 * the parse-error counter should be non-zero. */
static void test_csi_overflow_does_not_inject_keystrokes(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	/* \e[ + 80 ';' + 'X' (final byte) + 'B' (post-sequence printable) */
	uint8_t seq[256];
	size_t n = 0;
	seq[n++] = 0x1b;
	seq[n++] = '[';
	for (int i = 0; i < 80; i++) {
		seq[n++] = ';';
	}
	seq[n++] = 'X';   /* final byte for the malformed CSI */
	seq[n++] = 'B';   /* a real printable; should be the only emit */
	size_t accepted = input_decoder_inject_bytes(dec, seq, n);
	assert(accepted == n);

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	/* First emit must be the 'B' printable -- not any of the swallowed
	 * sequence bytes. */
	assert(rc == BOXEN_OK);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_NONE);
	assert(ev.key.ch == 'B');

	/* No further events queued. */
	memset(&ev, 0x7f, sizeof(ev));
	rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_ERR_TIMEOUT);

	/* The rejection path was observable via the counter. */
	assert(input_decoder_parse_errors(dec) > 0);

	input_decoder_destroy(dec);
}

/* P1-1 variant: CSI overflow split across inject boundaries -- swallow
 * state must persist across polls.  Inject \e[ + 70 ';' (forces overflow
 * mid-stream); poll (TIMEOUT, swallow active); inject 'X' (final);
 * poll (TIMEOUT, swallow ended); inject 'C' (printable); poll -> 'C'. */
static void test_csi_swallow_persists_across_polls(void) {
	input_decoder_t *dec = input_decoder_create(-1);

	uint8_t seq[80];
	size_t n = 0;
	seq[n++] = 0x1b;
	seq[n++] = '[';
	for (int i = 0; i < 70; i++) {
		seq[n++] = ';';
	}
	input_decoder_inject_bytes(dec, seq, n);

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_ERR_TIMEOUT);
	assert(input_decoder_parse_errors(dec) > 0);

	static const uint8_t final[] = {'X'};
	input_decoder_inject_bytes(dec, final, sizeof(final));
	rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_ERR_TIMEOUT);   /* swallow drained, no event */

	static const uint8_t printable[] = {'C'};
	input_decoder_inject_bytes(dec, printable, sizeof(printable));
	rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.key.ch == 'C');

	input_decoder_destroy(dec);
}

/* P1-1: a runaway long parameter (~63 digits) inside the CSI buffer used
 * to feed a signed-int accumulator overflow (UB).  The fix saturates
 * cur at CSI_PARAM_CAP (100000) before it can approach INT_MAX.  Verify
 * the sequence still decodes safely (modifier > 16 -> NONE) and no
 * crash. */
static void test_csi_param_overflow_saturates_safely(void) {
	input_decoder_t *dec = input_decoder_create(-1);

	/* \e[1;<60 nines>A -- modifier param huge; should clamp to NONE. */
	uint8_t seq[80];
	size_t n = 0;
	seq[n++] = 0x1b;
	seq[n++] = '[';
	seq[n++] = '1';
	seq[n++] = ';';
	for (int i = 0; i < 60; i++) {
		seq[n++] = '9';
	}
	seq[n++] = 'A';
	input_decoder_inject_bytes(dec, seq, n);

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.key == BOXEN_KEY_UP);
	assert(ev.key.mod == BOXEN_MOD_NONE);   /* saturated -> no modifier */

	input_decoder_destroy(dec);
}

/* P1-3: overlong NUL encoding (\xC0\x80) -- the canonical UTF-8 smuggling
 * primitive.  Must NOT decode to U+0000; must emit U+FFFD and bump
 * parse_errors.  Belt-and-suspenders: lead-byte rejection (0xC0 is now
 * rejected outright in utf8_lead_classify) drops this before the
 * continuation byte even matters, so the second byte falls through as a
 * stray continuation that ALSO bumps parse_errors. */
static void test_utf8_overlong_nul_rejected(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0xC0, 0x80};
	input_decoder_inject_bytes(dec, seq, sizeof(seq));

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	/* Both bytes were rejected silently (0xC0 invalid lead, 0x80 stray
	 * continuation in GROUND); no event emits. */
	assert(rc == BOXEN_ERR_TIMEOUT);
	assert(input_decoder_parse_errors(dec) >= 2);

	input_decoder_destroy(dec);
}

/* P1-3: overlong slash (\xC0\xAF would encode U+002F as 2-byte).
 * Symmetric to overlong NUL but pinning the lead-byte rejection. */
static void test_utf8_overlong_slash_rejected(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0xC0, 0xAF};
	input_decoder_inject_bytes(dec, seq, sizeof(seq));

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_ERR_TIMEOUT);
	assert(input_decoder_parse_errors(dec) >= 2);

	input_decoder_destroy(dec);
}

/* P1-3: overlong 3-byte encoding of U+007F (\xE0\x81\xBF).  This one has
 * a valid 3-byte lead (0xE0), so it passes the lead check; the
 * codepoint validator must catch it as overlong. */
static void test_utf8_overlong_3byte_emits_fffd(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0xE0, 0x81, 0xBF};
	input_decoder_inject_bytes(dec, seq, sizeof(seq));

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.ch == 0xFFFD);   /* replacement character */
	assert(input_decoder_parse_errors(dec) >= 1);

	input_decoder_destroy(dec);
}

/* P1-3: UTF-16 surrogate U+D800 encoded as 3-byte UTF-8 (\xED\xA0\x80).
 * Lead 0xED is valid; codepoint validator rejects the surrogate range. */
static void test_utf8_surrogate_rejected(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0xED, 0xA0, 0x80};
	input_decoder_inject_bytes(dec, seq, sizeof(seq));

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.key.ch == 0xFFFD);
	assert(input_decoder_parse_errors(dec) >= 1);

	input_decoder_destroy(dec);
}

/* P1-3: codepoint above Unicode max -- 0xF4 0x90 0x80 0x80 = U+110000.
 * Lead 0xF4 is valid (< 0xF5 rejection threshold), but the assembled
 * codepoint exceeds 0x10FFFF. */
static void test_utf8_above_max_rejected(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0xF4, 0x90, 0x80, 0x80};
	input_decoder_inject_bytes(dec, seq, sizeof(seq));

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.key.ch == 0xFFFD);
	assert(input_decoder_parse_errors(dec) >= 1);

	input_decoder_destroy(dec);
}

/* P1-3: invalid 5-byte UTF-8 lead 0xF8 should be silently dropped (not
 * emitted as a raw ch=0xF8 -- that would let Latin-1 garbage flow
 * downstream). */
static void test_utf8_invalid_5byte_lead_dropped(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0xF8, 'X'};
	input_decoder_inject_bytes(dec, seq, sizeof(seq));

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	/* 0xF8 dropped; 'X' is the next event. */
	assert(rc == BOXEN_OK);
	assert(ev.key.ch == 'X');
	assert(input_decoder_parse_errors(dec) >= 1);

	input_decoder_destroy(dec);
}

/* P1-3: stray UTF-8 continuation byte (0x80-0xBF) in GROUND used to be
 * emitted as a raw ch=byte (Latin-1 smuggling).  Must now drop with
 * parse-error bump. */
static void test_utf8_stray_continuation_in_ground_dropped(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x80, 0xBF, 'Y'};
	input_decoder_inject_bytes(dec, seq, sizeof(seq));

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.key.ch == 'Y');   /* 0x80 and 0xBF were dropped */
	assert(input_decoder_parse_errors(dec) >= 2);

	input_decoder_destroy(dec);
}

/* P1-3: premature non-continuation byte mid-UTF-8.  \xC2 (2-byte lead
 * expecting 1 continuation) followed by 'A' (not a continuation): partial
 * codepoint discarded, 'A' decoded fresh in GROUND. */
static void test_utf8_invalid_continuation_fallback(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0xC2, 'A'};
	input_decoder_inject_bytes(dec, seq, sizeof(seq));

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.key.ch == 'A');
	assert(input_decoder_parse_errors(dec) >= 1);

	input_decoder_destroy(dec);
}

/* Polish: F7..F10 (~-form), each with one dedicated assertion.  The
 * decode table covers them but the per-key behavioral pin was missing. */
static void test_function_key_f7_csi(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '1', '8', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.key.key == BOXEN_KEY_F7);
	input_decoder_destroy(dec);
}

static void test_function_key_f8_csi(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '1', '9', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.key.key == BOXEN_KEY_F8);
	input_decoder_destroy(dec);
}

static void test_function_key_f9_csi(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '2', '0', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.key.key == BOXEN_KEY_F9);
	input_decoder_destroy(dec);
}

static void test_function_key_f10_csi(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', '2', '1', '~'};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.key.key == BOXEN_KEY_F10);
	input_decoder_destroy(dec);
}

/* Polish: full CTRL sweep.  The decode_byte_ground table maps 0x01..0x1A
 * to BOXEN_KEY_CTRL_A..CTRL_Z via a single enum arithmetic; one regression
 * there would break all 26 silently.  This loop pins every slot. */
static void test_control_chars_full_sweep(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	for (uint8_t b = 0x01; b <= 0x1A; b++) {
		uint8_t seq[] = {b};
		boxen_event_t ev;
		size_t accepted = input_decoder_inject_bytes(dec, seq, 1);
		assert(accepted == 1);
		memset(&ev, 0x7f, sizeof(ev));
		int rc = input_decoder_poll(dec, &ev, 0);
		assert(rc == BOXEN_OK);
		assert(ev.type == BOXEN_EV_KEY);
		/* TAB (0x09) and ENTER (0x0D) are surfaced via their dedicated
		 * key codes per the boxen header (which alias CTRL_I and
		 * CTRL_M); BACKSPACE (0x08) likewise.  Skip those slots here --
		 * they have dedicated tests above. */
		if (b == 0x08) {
			assert(ev.key.key == BOXEN_KEY_BACKSPACE);
		} else if (b == 0x09) {
			assert(ev.key.key == BOXEN_KEY_TAB);
		} else if (b == 0x0D) {
			assert(ev.key.key == BOXEN_KEY_ENTER);
		} else {
			boxen_key_t expected = (boxen_key_t)(BOXEN_KEY_CTRL_A + (b - 0x01));
			assert(ev.key.key == expected);
		}
	}
	input_decoder_destroy(dec);
}

/* Polish: 0x00 -> CTRL_SPACE, 0x1C -> CTRL_BACKSLASH (the two
 * special-cased control-code mappings outside the 0x01..0x1A range). */
static void test_control_char_ctrl_space(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x00};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.key.key == BOXEN_KEY_CTRL_SPACE);
	input_decoder_destroy(dec);
}

static void test_control_char_ctrl_backslash(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1c};
	boxen_event_t ev;
	inject_and_poll(dec, seq, sizeof(seq), BOXEN_OK, &ev);
	assert(ev.key.key == BOXEN_KEY_CTRL_BACKSLASH);
	input_decoder_destroy(dec);
}

/* Polish: two complete sequences in one inject.  The burst-read
 * contract requires the decoder to dequeue them across two poll calls
 * in order.  M3 will extend this to the wheel-spam scenario; M2 pins
 * the basic case so the M3 SKIP-stub remains the new-scope guard. */
static void test_burst_two_events_one_inject(void) {
	input_decoder_t *dec = input_decoder_create(-1);
	static const uint8_t seq[] = {0x1b, '[', 'A', 0x1b, '[', 'B'};
	size_t accepted = input_decoder_inject_bytes(dec, seq, sizeof(seq));
	assert(accepted == sizeof(seq));

	boxen_event_t ev;
	int rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.key.key == BOXEN_KEY_UP);

	memset(&ev, 0x7f, sizeof(ev));
	rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_OK);
	assert(ev.key.key == BOXEN_KEY_DOWN);

	/* No more events. */
	rc = input_decoder_poll(dec, &ev, 0);
	assert(rc == BOXEN_ERR_TIMEOUT);

	input_decoder_destroy(dec);
}

/* Polish: parse_errors accessor NULL-safety and starts at zero. */
static void test_parse_errors_accessor_smoke(void) {
	assert(input_decoder_parse_errors(NULL) == 0);
	input_decoder_t *dec = input_decoder_create(-1);
	assert(input_decoder_parse_errors(dec) == 0);
	input_decoder_destroy(dec);
}

/* -------------------------------------------------------------------------
 * M3 SKIP stubs -- SGR mouse + X10 fallback + burst-read.
 * ---------------------------------------------------------------------- */

static void test_skip_sgr_mouse_left_press(void) {
	tr_skip("M3: \\e[<0;10;5M -> MOUSE button=1 pressed=true x=9 y=4 "
	        "(plan section 3.6)");
}

static void test_skip_sgr_mouse_left_release(void) {
	tr_skip("M3: \\e[<0;10;5m -> MOUSE button=1 pressed=false (plan section 3.6)");
}

static void test_skip_sgr_mouse_wheel_up(void) {
	tr_skip("M3: \\e[<64;10;5M -> MOUSE button=4 (wheel-up) -- the #805 "
	        "mousewheel case (plan section 3.6)");
}

static void test_skip_sgr_mouse_wheel_down(void) {
	tr_skip("M3: \\e[<65;10;5M -> MOUSE button=5 (wheel-down) (plan section 3.6)");
}

static void test_skip_sgr_mouse_motion(void) {
	tr_skip("M3: \\e[<32;10;5M -> MOUSE motion (plan section 3.6)");
}

static void test_skip_sgr_mouse_shift_modifier(void) {
	tr_skip("M3: SGR button bit 4 -> mod=SHIFT (plan section 3.6)");
}

static void test_skip_x10_mouse_left_press(void) {
	tr_skip("M3: \\e[M\\x20\\x2a\\x19 -> MOUSE left press (plan section 3.7)");
}

static void test_skip_x10_mouse_wheel_up(void) {
	tr_skip("M3: \\e[M\\x60\\x2a\\x19 -> MOUSE wheel-up (plan section 3.7)");
}

static void test_skip_burst_read_multiple_events_one_inject(void) {
	tr_skip("M3: inject 3 complete events in one inject_bytes call; poll() "
	        "dequeues all three in order (plan section 7 M3)");
}

static void test_skip_double_click_synthesis(void) {
	tr_skip("M3: two SGR left-press within window -> BOXEN_MOUSE_DOUBLE_CLICK "
	        "flag set on second (plan section 7 M3)");
}

/* -------------------------------------------------------------------------
 * M4 SKIP stubs -- bracketed paste + BOXEN_EV_PASTE.
 * ---------------------------------------------------------------------- */

static void test_skip_bracketed_paste_simple(void) {
	tr_skip("M4: \\e[200~hello\\e[201~ -> BOXEN_EV_PASTE data=\"hello\" "
	        "(plan section 3.8); requires boxen.h ABI add for BOXEN_EV_PASTE");
}

static void test_skip_bracketed_paste_with_newlines(void) {
	tr_skip("M4: paste with embedded \\n / \\r\\n -> single PASTE event "
	        "(plan section 7 M4)");
}

static void test_skip_bracketed_paste_size_cap_truncation(void) {
	tr_skip("M4: paste > 256 KiB -> truncated + BOXEN_LOG_W "
	        "(plan section 3.8, R5)");
}

static void test_skip_bracketed_paste_embedded_esc_is_literal(void) {
	tr_skip("M4: ESC sequences inside paste markers -> treated as literal "
	        "text, not parsed (plan section 7 M4)");
}

/* -------------------------------------------------------------------------
 * M5 SKIP stubs -- Kitty keyboard protocol.
 * ---------------------------------------------------------------------- */

static void test_skip_kitty_csi_u_letter(void) {
	tr_skip("M5: \\e[65u -> ch='A', mod=NONE (plan section 3.9)");
}

static void test_skip_kitty_csi_u_letter_with_alt(void) {
	tr_skip("M5: \\e[65;3u -> ch='A', mod=ALT (plan section 3.9)");
}

static void test_skip_kitty_csi_u_enter_with_ctrl(void) {
	tr_skip("M5: \\e[13;5u -> KEY_ENTER, mod=CTRL (plan section 3.9)");
}

static void test_skip_kitty_csi_u_escape_with_alt(void) {
	tr_skip("M5: \\e[27;3u -> KEY_ESCAPE, mod=ALT (plan section 3.9)");
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void) {
	TR_INIT("input_decoder_tests");

	/* M1 smoke -- must pass today and stay passing through every milestone. */
	TR_RUN(test_create_destroy_smoke);
	TR_RUN(test_destroy_null_is_noop);
	TR_RUN(test_mouse_disabled_by_default);
	TR_RUN(test_mouse_enabled_null_safe);
	TR_RUN(test_poll_empty_buffer_returns_timeout);
	TR_RUN(test_poll_null_decoder_is_invalid);
	TR_RUN(test_poll_null_out_is_invalid);
	TR_RUN(test_inject_bytes_appends_to_buffer);
	TR_RUN(test_inject_bytes_noop_on_empty);
	TR_RUN(test_inject_bytes_null_decoder_safe);
	TR_RUN(test_buffered_bytes_null_safe);
	TR_RUN(test_kitty_enable_round_trip);
	TR_RUN(test_inject_overflow_increments_dropped_counter);

	/* 2026-06-30 JES #810 M2: behavioral coverage.
	 * Plain CSI arrows. */
	TR_RUN(test_csi_plain_arrow_up);
	TR_RUN(test_csi_plain_arrow_down);
	TR_RUN(test_csi_plain_arrow_right);
	TR_RUN(test_csi_plain_arrow_left);

	/* SS3 cursor and nav keys. */
	TR_RUN(test_ss3_arrow_up);
	TR_RUN(test_ss3_arrow_down);
	TR_RUN(test_ss3_arrow_right);
	TR_RUN(test_ss3_arrow_left);
	TR_RUN(test_ss3_home);
	TR_RUN(test_ss3_end);

	/* CSI modifier params (the #805 fix). */
	TR_RUN(test_csi_modifier_all_params_arrow_up);
	TR_RUN(test_csi_modifier_all_params_arrow_down);
	TR_RUN(test_csi_modifier_all_params_arrow_right);
	TR_RUN(test_csi_modifier_all_params_arrow_left);
	TR_RUN(test_csi_modifier_alt_left_issue_805);
	TR_RUN(test_csi_modifier_alt_right_issue_805);
	TR_RUN(test_csi_modifier_shift_up);
	TR_RUN(test_csi_modifier_ctrl_down);
	TR_RUN(test_csi_modifier_alt_shift_left);
	TR_RUN(test_csi_modifier_meta_up);

	/* CSI nav keys with modifiers. */
	TR_RUN(test_csi_nav_home);
	TR_RUN(test_csi_nav_end);
	TR_RUN(test_csi_nav_home_with_alt);
	TR_RUN(test_csi_nav_end_with_ctrl);
	TR_RUN(test_csi_nav_insert);
	TR_RUN(test_csi_nav_delete);
	TR_RUN(test_csi_nav_pgup);
	TR_RUN(test_csi_nav_pgdn);
	TR_RUN(test_csi_nav_delete_with_shift);
	TR_RUN(test_csi_nav_pgup_with_meta);

	/* Function keys SS3 + CSI with modifiers. */
	TR_RUN(test_function_key_f1_ss3);
	TR_RUN(test_function_key_f2_ss3);
	TR_RUN(test_function_key_f3_ss3);
	TR_RUN(test_function_key_f4_ss3);
	TR_RUN(test_function_key_f1_shift);
	TR_RUN(test_function_key_f4_alt);
	TR_RUN(test_function_key_f1_csi_legacy);
	TR_RUN(test_function_key_f5_csi);
	TR_RUN(test_function_key_f6_csi);
	TR_RUN(test_function_key_f11_csi);
	TR_RUN(test_function_key_f12_csi);
	TR_RUN(test_function_key_f5_with_ctrl);

	/* ESC-prefix Meta and ESC-alone disambiguation. */
	TR_RUN(test_meta_prefix_alt_a);
	TR_RUN(test_meta_prefix_alt_b);
	TR_RUN(test_meta_prefix_alt_f);
	TR_RUN(test_esc_alone_emits_escape);
	TR_RUN(test_esc_then_esc_emits_two_escapes);

	/* Partial-sequence splits (burst-read robustness). */
	TR_RUN(test_partial_sequence_across_inject_boundary);
	TR_RUN(test_partial_ss3_across_inject_boundary);
	TR_RUN(test_partial_esc_bracket_does_not_emit);

	/* Control characters and printable ASCII. */
	TR_RUN(test_control_char_ctrl_a);
	TR_RUN(test_control_char_ctrl_c);
	TR_RUN(test_control_char_ctrl_e);
	TR_RUN(test_control_char_ctrl_z);
	TR_RUN(test_control_char_tab);
	TR_RUN(test_control_char_enter);
	TR_RUN(test_control_char_backspace_ctrl_h);
	TR_RUN(test_control_char_backspace_del);
	TR_RUN(test_printable_ascii);

	/* UTF-8 multi-byte assembly. */
	TR_RUN(test_utf8_two_byte_copyright);
	TR_RUN(test_utf8_three_byte_em_dash);
	TR_RUN(test_utf8_four_byte_emoji);
	TR_RUN(test_utf8_continuation_split_across_inject);

	/* 2026-06-30 JES #810 M2 review-followup: P1 hardening + polish. */
	TR_RUN(test_csi_overflow_does_not_inject_keystrokes);
	TR_RUN(test_csi_swallow_persists_across_polls);
	TR_RUN(test_csi_param_overflow_saturates_safely);
	TR_RUN(test_utf8_overlong_nul_rejected);
	TR_RUN(test_utf8_overlong_slash_rejected);
	TR_RUN(test_utf8_overlong_3byte_emits_fffd);
	TR_RUN(test_utf8_surrogate_rejected);
	TR_RUN(test_utf8_above_max_rejected);
	TR_RUN(test_utf8_invalid_5byte_lead_dropped);
	TR_RUN(test_utf8_stray_continuation_in_ground_dropped);
	TR_RUN(test_utf8_invalid_continuation_fallback);
	TR_RUN(test_function_key_f7_csi);
	TR_RUN(test_function_key_f8_csi);
	TR_RUN(test_function_key_f9_csi);
	TR_RUN(test_function_key_f10_csi);
	TR_RUN(test_control_chars_full_sweep);
	TR_RUN(test_control_char_ctrl_space);
	TR_RUN(test_control_char_ctrl_backslash);
	TR_RUN(test_burst_two_events_one_inject);
	TR_RUN(test_parse_errors_accessor_smoke);

	/* M3 SKIP -- SGR mouse, X10 fallback, burst-read. */
	TR_RUN(test_skip_sgr_mouse_left_press);
	TR_RUN(test_skip_sgr_mouse_left_release);
	TR_RUN(test_skip_sgr_mouse_wheel_up);
	TR_RUN(test_skip_sgr_mouse_wheel_down);
	TR_RUN(test_skip_sgr_mouse_motion);
	TR_RUN(test_skip_sgr_mouse_shift_modifier);
	TR_RUN(test_skip_x10_mouse_left_press);
	TR_RUN(test_skip_x10_mouse_wheel_up);
	TR_RUN(test_skip_burst_read_multiple_events_one_inject);
	TR_RUN(test_skip_double_click_synthesis);

	/* M4 SKIP -- bracketed paste. */
	TR_RUN(test_skip_bracketed_paste_simple);
	TR_RUN(test_skip_bracketed_paste_with_newlines);
	TR_RUN(test_skip_bracketed_paste_size_cap_truncation);
	TR_RUN(test_skip_bracketed_paste_embedded_esc_is_literal);

	/* M5 SKIP -- Kitty keyboard protocol. */
	TR_RUN(test_skip_kitty_csi_u_letter);
	TR_RUN(test_skip_kitty_csi_u_letter_with_alt);
	TR_RUN(test_skip_kitty_csi_u_enter_with_ctrl);
	TR_RUN(test_skip_kitty_csi_u_escape_with_alt);

	/* 2026-06-29 JES #809 M1: skip-stub visibility (see file header).
	 * Printed BEFORE TR_SUMMARY so the count appears alongside the green
	 * tally in the test log without polluting the JSON tally itself.
	 * tr_count was updated by TR_RUN as each test ran; g_skip_count tracks
	 * the subset that called tr_skip() instead of asserting behavior. */
	printf("[skip-summary] %d of %d tests are SKIP stubs deferred to M2-M5\n",
	       g_skip_count, tr_count);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
