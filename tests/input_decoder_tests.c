/*
 * input_decoder_tests.c -- PTY-replay harness for the boxen input decoder.
 *
 * 2026-06-29 JES #809 M1: harness infrastructure + create/destroy/inject smoke
 * tests.  Protocol coverage tests (cursor keys, mouse, paste, Kitty) are listed
 * here as SKIP stubs that will be un-skipped one milestone at a time
 * (M2 through M5; see planning/phase_c/INPUT_DECODER_PLAN.md section 6.2).
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
 * M2 SKIP stubs -- cursor keys + modifiers + ESC disambiguation.
 *
 * Per plan section 7, M2 un-skips and implements these.  The names below
 * mirror the sequences in section 3 (Protocol Coverage Matrix) so the M2
 * sub-agent can grep "M2 SKIP" to find its TDD entry points.
 * ---------------------------------------------------------------------- */

static void test_skip_csi_plain_arrow_up(void) {
	tr_skip("M2: \\e[A -> KEY UP, mod=NONE (plan section 3.1)");
}

static void test_skip_csi_plain_arrow_down(void) {
	tr_skip("M2: \\e[B -> KEY DOWN, mod=NONE (plan section 3.1)");
}

static void test_skip_csi_plain_arrow_right(void) {
	tr_skip("M2: \\e[C -> KEY RIGHT, mod=NONE (plan section 3.1)");
}

static void test_skip_csi_plain_arrow_left(void) {
	tr_skip("M2: \\e[D -> KEY LEFT, mod=NONE (plan section 3.1)");
}

static void test_skip_ss3_arrow_up(void) {
	tr_skip("M2: \\eOA -> KEY UP, mod=NONE (plan section 3.1)");
}

static void test_skip_csi_modifier_alt_left_issue_805(void) {
	tr_skip("M2: \\e[1;3D -> KEY LEFT, mod=ALT -- the #805 root-cause case "
	        "(plan section 3.2)");
}

static void test_skip_csi_modifier_alt_right_issue_805(void) {
	tr_skip("M2: \\e[1;3C -> KEY RIGHT, mod=ALT -- the #805 root-cause case "
	        "(plan section 3.2)");
}

static void test_skip_csi_modifier_shift_up(void) {
	tr_skip("M2: \\e[1;2A -> KEY UP, mod=SHIFT (plan section 3.2)");
}

static void test_skip_csi_modifier_ctrl_down(void) {
	tr_skip("M2: \\e[1;5B -> KEY DOWN, mod=CTRL (plan section 3.2)");
}

static void test_skip_csi_modifier_alt_shift_left(void) {
	tr_skip("M2: \\e[1;4D -> KEY LEFT, mod=ALT|SHIFT (plan section 3.2)");
}

static void test_skip_csi_modifier_meta_up(void) {
	tr_skip("M2: \\e[1;9A -> KEY UP, mod=META (plan section 3.2)");
}

static void test_skip_csi_nav_home(void) {
	tr_skip("M2: \\e[H -> KEY HOME (plan section 3.3)");
}

static void test_skip_csi_nav_end(void) {
	tr_skip("M2: \\e[F -> KEY END (plan section 3.3)");
}

static void test_skip_csi_nav_pgup(void) {
	tr_skip("M2: \\e[5~ -> KEY PGUP (plan section 3.3)");
}

static void test_skip_csi_nav_pgdn(void) {
	tr_skip("M2: \\e[6~ -> KEY PGDN (plan section 3.3)");
}

static void test_skip_csi_nav_delete(void) {
	tr_skip("M2: \\e[3~ -> KEY DELETE (plan section 3.3)");
}

static void test_skip_function_key_f1_ss3(void) {
	tr_skip("M2: \\eOP -> KEY F1 (plan section 3.4)");
}

static void test_skip_function_key_f5_csi(void) {
	tr_skip("M2: \\e[15~ -> KEY F5 (plan section 3.4)");
}

static void test_skip_function_key_f1_shift(void) {
	tr_skip("M2: \\e[1;2P -> KEY F1, mod=SHIFT (plan section 3.4)");
}

static void test_skip_meta_prefix_alt_a(void) {
	tr_skip("M2: \\ea -> ch='a', mod=ALT (plan section 3.5)");
}

static void test_skip_meta_prefix_alt_b(void) {
	tr_skip("M2: \\eb -> ch='b', mod=ALT (plan section 3.5 policy: emit as "
	        "letter+ALT, not KEY LEFT)");
}

static void test_skip_esc_alone_emits_escape(void) {
	tr_skip("M2: lone 0x1B with no follow-up -> KEY ESCAPE (plan section 4.4)");
}

static void test_skip_partial_sequence_across_inject_boundary(void) {
	tr_skip("M2: inject \\e[1; then poll (timeout, no event), inject 3D then "
	        "poll (LEFT+ALT) -- burst-read robustness (plan section 3.12 + 6.1)");
}

static void test_skip_control_char_ctrl_a(void) {
	tr_skip("M2: 0x01 -> KEY CTRL_A (plan section 3.11)");
}

static void test_skip_control_char_tab(void) {
	tr_skip("M2: 0x09 -> KEY TAB (plan section 3.11)");
}

static void test_skip_control_char_enter(void) {
	tr_skip("M2: 0x0D -> KEY ENTER (plan section 3.11)");
}

static void test_skip_control_char_backspace_del(void) {
	tr_skip("M2: 0x7F -> KEY BACKSPACE (plan section 3.11)");
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
 * UTF-8 SKIP stubs -- multi-byte decode.  Slated for M2 (the codepoint
 * accumulator is part of the GROUND-state path).
 * ---------------------------------------------------------------------- */

static void test_skip_utf8_two_byte_codepoint(void) {
	tr_skip("M2: 2-byte UTF-8 (U+00A9 copyright) -> ev.key.ch=0xA9 "
	        "(plan section 3.10)");
}

static void test_skip_utf8_three_byte_codepoint(void) {
	tr_skip("M2: 3-byte UTF-8 (U+2014 em-dash) -> ev.key.ch=0x2014 "
	        "(plan section 3.10)");
}

static void test_skip_utf8_four_byte_codepoint(void) {
	tr_skip("M2: 4-byte UTF-8 (U+1F600 emoji) -> ev.key.ch=0x1F600 "
	        "(plan section 3.10)");
}

static void test_skip_utf8_continuation_split_across_inject(void) {
	tr_skip("M2: UTF-8 lead byte + first cont in inject 1, remaining cont "
	        "in inject 2 -> single emit on completion (plan section 3.10)");
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

	/* M2 SKIP -- cursor keys, modifiers, ESC, control chars, UTF-8. */
	TR_RUN(test_skip_csi_plain_arrow_up);
	TR_RUN(test_skip_csi_plain_arrow_down);
	TR_RUN(test_skip_csi_plain_arrow_right);
	TR_RUN(test_skip_csi_plain_arrow_left);
	TR_RUN(test_skip_ss3_arrow_up);
	TR_RUN(test_skip_csi_modifier_alt_left_issue_805);
	TR_RUN(test_skip_csi_modifier_alt_right_issue_805);
	TR_RUN(test_skip_csi_modifier_shift_up);
	TR_RUN(test_skip_csi_modifier_ctrl_down);
	TR_RUN(test_skip_csi_modifier_alt_shift_left);
	TR_RUN(test_skip_csi_modifier_meta_up);
	TR_RUN(test_skip_csi_nav_home);
	TR_RUN(test_skip_csi_nav_end);
	TR_RUN(test_skip_csi_nav_pgup);
	TR_RUN(test_skip_csi_nav_pgdn);
	TR_RUN(test_skip_csi_nav_delete);
	TR_RUN(test_skip_function_key_f1_ss3);
	TR_RUN(test_skip_function_key_f5_csi);
	TR_RUN(test_skip_function_key_f1_shift);
	TR_RUN(test_skip_meta_prefix_alt_a);
	TR_RUN(test_skip_meta_prefix_alt_b);
	TR_RUN(test_skip_esc_alone_emits_escape);
	TR_RUN(test_skip_partial_sequence_across_inject_boundary);
	TR_RUN(test_skip_control_char_ctrl_a);
	TR_RUN(test_skip_control_char_tab);
	TR_RUN(test_skip_control_char_enter);
	TR_RUN(test_skip_control_char_backspace_del);
	TR_RUN(test_skip_utf8_two_byte_codepoint);
	TR_RUN(test_skip_utf8_three_byte_codepoint);
	TR_RUN(test_skip_utf8_four_byte_codepoint);
	TR_RUN(test_skip_utf8_continuation_split_across_inject);

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
