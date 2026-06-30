/*
 * input_decoder.c -- private terminal input decoder for the boxen tb2 backend.
 *
 * 2026-06-29 JES #809 M1: PTY-replay harness + decoder stub.
 *
 * Scope of this file at M1:
 *   - Allocate / free the input_decoder_t struct.
 *   - Provide a ring buffer that input_decoder_inject_bytes() fills (test seam).
 *   - input_decoder_poll() always returns BOXEN_ERR_TIMEOUT (the state machine
 *     that drains the ring buffer into boxen_event_t lands in M2).
 *   - Mouse-enable and Kitty-enable record the request but emit nothing.
 *
 * Out of scope for M1:
 *   - The escape-sequence state machine (M2 -- GROUND / ESC_RECEIVED /
 *     CSI_COLLECTING / SS3_RECEIVED states; see INPUT_DECODER_PLAN.md s4).
 *   - SGR mouse / X10 mouse parsing (M3).
 *   - Bracketed paste (M4) including the BOXEN_EV_PASTE ABI addition.
 *   - Kitty CSI-u decode (M5).
 *   - Wiring into backend_tb2.c (M6).
 *   - The /mouse on/off slash command (M7).
 *
 * The M1 deliverable is the harness, not the decoder.  This file exists so
 * the M2 sub-agent has a place to grow the state machine and so that the
 * tests/input_decoder_tests.c harness has a real linkable surface that
 * exercises the API shape (create / inject / poll / destroy).
 *
 * Threading: single-threaded; the caller (M6: tb2_poll_event) is responsible
 * for GIL release/reacquire around any future blocking read.  The M1 stub
 * never blocks because it never reads.
 *
 * See planning/phase_c/INPUT_DECODER_PLAN.md sections 2, 6, 7 for the full
 * design.
 */

#include "input_decoder.h"

#include <stdlib.h>
#include <string.h>

/* 2026-06-29 JES #809 M1: ring-buffer sizing.
 *
 * 4 KiB is comfortably larger than any non-paste protocol burst we expect
 * (a fully-modified CSI sequence tops out at ~12 bytes; SGR mouse reports
 * are ~16; a worst-case burst of "every key on the keyboard pressed at once"
 * fits in well under 1 KiB).  Bracketed paste (M4) has its own dedicated
 * buffer with a 256 KiB cap -- it does NOT share this ring.
 *
 * The size is a compile-time constant rather than runtime-tunable because
 * the decoder is single-purpose and the M2 state machine will reason about
 * worst-case fill levels at audit time.  Power-of-two simplifies any future
 * masked-index arithmetic if we move from memmove-on-drain to a true ring. */
#define INPUT_DECODER_BUFFER_SIZE 4096

struct input_decoder {
	/* TTY fd (or -1 for the test harness).  M1: unused.  M2+ uses select(2)
	 * + read(2) on this fd inside input_decoder_poll. */
	int    tty_fd;

	/* Mouse-enable bit.  Records the request from input_decoder_set_mouse;
	 * M3 will write \e[?1006h / \e[?1006l + \e[?2004h / \e[?2004l to tty_fd. */
	bool   mouse_enabled;

	/* Kitty enable bit.  Records the request from input_decoder_kitty_enable;
	 * M5 will write \e[=1u to tty_fd and probe for a response. */
	bool   kitty_enabled;

	/* Linear scratch buffer used as a simple drain-from-head queue.
	 * head_len is the number of unread bytes starting at buf[0].
	 *
	 * Future-tense: the M2 state machine will consume from the head and
	 * compact (memmove) on drain, or be promoted to a head/tail ring if
	 * profiling shows the memmove is hot.  For M1 the buffer is filled by
	 * inject_bytes (test seam) and never drained because poll always returns
	 * BOXEN_ERR_TIMEOUT. */
	uint8_t buf[INPUT_DECODER_BUFFER_SIZE];
	size_t  head_len;
};

/* -------------------------------------------------------------------------
 * Lifecycle
 * ---------------------------------------------------------------------- */

input_decoder_t *input_decoder_create(int tty_fd) {
	input_decoder_t *dec = (input_decoder_t *)calloc(1, sizeof(*dec));
	if (dec == NULL) {
		return NULL;
	}
	dec->tty_fd = tty_fd;
	dec->mouse_enabled = false;
	dec->kitty_enabled = false;
	dec->head_len = 0;
	return dec;
}

void input_decoder_destroy(input_decoder_t *dec) {
	if (dec == NULL) {
		return;
	}
	/* M1: no resources beyond the struct itself.  M3 / M4 will own paste
	 * buffers and possibly an escape-write retry queue; both will be freed
	 * here. */
	free(dec);
}

/* -------------------------------------------------------------------------
 * Mouse-mode policy seam
 * ---------------------------------------------------------------------- */

void input_decoder_set_mouse(input_decoder_t *dec, bool enable) {
	if (dec == NULL) {
		return;
	}
	dec->mouse_enabled = enable;
	/* M3 / M4: write \e[?1006h / \e[?1006l and \e[?2004h / \e[?2004l to
	 * dec->tty_fd. M1 stub records the bit only. */
}

bool input_decoder_mouse_enabled(const input_decoder_t *dec) {
	if (dec == NULL) {
		return false;
	}
	return dec->mouse_enabled;
}

/* -------------------------------------------------------------------------
 * Poll (M1 stub)
 * ---------------------------------------------------------------------- */

int input_decoder_poll(input_decoder_t *dec, boxen_event_t *out, int timeout_ms) {
	(void)timeout_ms;   /* M1: ignored; M2+ uses it in select(2). */

	if (dec == NULL || out == NULL) {
		return BOXEN_ERR_INVALID;
	}

	/* Always-zeroed out parameter on timeout, per the plan section 2.2:
	 *   "On timeout returns BOXEN_ERR_TIMEOUT (*out is zeroed)." */
	memset(out, 0, sizeof(*out));

	/* M1 stub: the state machine that consumes the ring buffer is M2's
	 * deliverable.  Until then we always report timeout so the only event
	 * a caller can observe is "nothing happened".  The buffered bytes
	 * remain in place for the M2 implementation to drain. */
	return BOXEN_ERR_TIMEOUT;
}

/* -------------------------------------------------------------------------
 * Kitty enable (M1 stub; full impl in M5)
 * ---------------------------------------------------------------------- */

void input_decoder_kitty_enable(input_decoder_t *dec) {
	if (dec == NULL) {
		return;
	}
	dec->kitty_enabled = true;
	/* M5: write "\e[=1u" to dec->tty_fd; subsequent polls decode the
	 * \e[CODE;MOD u replies. M1 stub records the bit only. */
}

/* -------------------------------------------------------------------------
 * Test seam (compile-guarded)
 * ---------------------------------------------------------------------- */

#ifdef INPUT_DECODER_TEST_SEAM

void input_decoder_inject_bytes(input_decoder_t *dec,
                                const uint8_t *bytes, size_t len) {
	if (dec == NULL || len == 0) {
		return;
	}
	if (bytes == NULL) {
		return;
	}

	/* M1: drop bytes silently on overflow.  M2 will refine the contract --
	 * once the state machine is draining the buffer, the realistic failure
	 * mode is "a paste larger than 256 KiB" (handled by the paste-specific
	 * cap in M4) rather than "the ring filled because nobody read it".
	 *
	 * For the M1 harness, the smoke tests inject a handful of bytes; a
	 * future test that floods the buffer past INPUT_DECODER_BUFFER_SIZE
	 * is documenting an unwanted behavior, not an expected one. */
	size_t available = INPUT_DECODER_BUFFER_SIZE - dec->head_len;
	size_t to_copy = (len < available) ? len : available;

	if (to_copy > 0) {
		memcpy(dec->buf + dec->head_len, bytes, to_copy);
		dec->head_len += to_copy;
	}
}

size_t input_decoder_buffered_bytes(const input_decoder_t *dec) {
	if (dec == NULL) {
		return 0;
	}
	return dec->head_len;
}

#endif /* INPUT_DECODER_TEST_SEAM */
