/*
 * input_decoder.c -- private terminal input decoder for the boxen tb2 backend.
 *
 * 2026-06-29 JES #809 M1: PTY-replay harness + decoder stub.
 * 2026-06-30 JES #810 M2: cursor-key / SS3 / modifier-param / control-char
 *   / UTF-8 state machine.  This is the first milestone that emits real
 *   events from the buffered byte stream.
 *
 * Scope of this file at M2:
 *   - Allocate / free the input_decoder_t struct.
 *   - Buffer bytes via input_decoder_inject_bytes() (test seam) -- M6 will
 *     replace inject with a real read(2) loop on tty_fd.
 *   - State machine through GROUND / ESC_RECEIVED / CSI_COLLECTING /
 *     SS3_RECEIVED / UTF8_CONT.
 *   - Emit boxen_event_t for: plain CSI cursor keys, SS3 cursor + nav keys,
 *     CSI cursor / nav / function keys with modifier params 2..16, ~-form
 *     nav (Ins/Del/PgUp/PgDn) and ~-form F-keys (F1 legacy, F5..F12), CSI
 *     P/Q/R/S function keys with modifiers, ESC-prefix Meta (Alt+letter),
 *     ESC-alone, control chars 0x01..0x1A and 0x7F, printable ASCII, UTF-8
 *     multi-byte codepoints.
 *   - Partial-sequence robustness: parsing state survives across poll calls
 *     so a sequence split mid-stream (read(2) boundary, inject_bytes
 *     boundary) completes correctly on the next poll.
 *
 * Out of scope for M2:
 *   - SGR mouse / X10 mouse parsing (M3 -- final byte M/m on a CSI<-prefixed
 *     sequence).
 *   - Bracketed paste (M4 -- final byte ~ on params 200/201).
 *   - Kitty CSI-u decode (M5 -- final byte u).
 *   - The real read(2) / select(2) loop on tty_fd (M6).
 *   - Mouse-enable escape-sequence WRITE side (M3/M4 wire the bytes).
 *   - The /mouse on/off slash command (M7).
 *
 * Plan reference: planning/phase_c/INPUT_DECODER_PLAN.md sections 3.1, 3.2,
 *   3.3, 3.4, 3.5, 3.10, 3.11, 3.12, 4 (state machine), 7 (M2 scope).
 *
 * Threading: see the contract block in input_decoder.h.  Summary: single
 * owner thread; GIL-held at every API entry/exit; the GIL is dropped only
 * inside the M6 blocking read inside input_decoder_poll, and the
 * single-owner invariant means no other thread can touch the decoder
 * during that window.  No locks, no atomics -- by design.
 */

#include "input_decoder.h"

#include <assert.h>
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

/* 2026-06-30 JES #810 M2: CSI parameter buffer.
 *
 * A modifier-rich CSI sequence holds at most ~3 semicolon-separated params
 * (\e[1;3D, \e[15;5~, etc.).  M3's SGR mouse adds button/x/y to that, still
 * well under 5.  16 bytes of parameter text -- digits + ';' -- is comfortably
 * over the longest sequence we expect (the legacy F-keys reach 2 digits;
 * SGR mouse coords reach ~3 digits per param; total ~10 chars).  Bound is
 * load-bearing: it caps stack writes inside csi_parse_params and prevents
 * a malicious terminal from steering us into UB by emitting infinite
 * parameter bytes (CSI params are 0x30..0x3F per the standard; a stream of
 * '9999;9999;...;9999' is a DoS vector if uncapped).  When the buffer is
 * full we treat further parameter bytes as a parse error and reset to
 * GROUND -- safe, conservative, and the M3 SGR mouse code will inherit the
 * same bound. */
#define INPUT_DECODER_CSI_BUF_SIZE 64

/* 2026-06-30 JES #810 M2: parser state machine states.
 *
 * Names match plan section 4.1 one-to-one for grep'ability.  PASTE_ACTIVE,
 * OSC_COLLECTING, KITTY_COLLECTING are reserved for M4 / M5 and not used
 * by this milestone's transitions -- the M2 implementation never sets them. */
typedef enum {
	DEC_STATE_GROUND = 0,
	DEC_STATE_ESC_RECEIVED,
	DEC_STATE_CSI_COLLECTING,
	DEC_STATE_SS3_RECEIVED,
	DEC_STATE_UTF8_CONT,
} dec_state_t;

struct input_decoder {
	/* TTY fd (or -1 for the test harness).  M2: unused.  M6 uses select(2)
	 * + read(2) on this fd inside input_decoder_poll. */
	int    tty_fd;

	/* Mouse-enable bit.  Records the request from input_decoder_set_mouse;
	 * M3 will write \e[?1006h / \e[?1006l + \e[?2004h / \e[?2004l to tty_fd. */
	bool   mouse_enabled;

	/* Kitty enable bit.  Records the request from input_decoder_kitty_enable;
	 * M5 will write \e[=1u to tty_fd and probe for a response. */
	bool   kitty_enabled;

	/* 2026-06-30 JES #810 M2: byte buffer with head/tail cursors.
	 *
	 * Bytes are deposited at buf[fill_len] by inject_bytes (M6: read(2)).
	 * They are consumed from buf[head] by the parser inside poll.  When the
	 * parser drains a complete event we may have head > 0 with unparsed tail
	 * bytes (next event in a burst, or a partial sequence we'll resume on
	 * the next poll); see compact_buffer() for the memmove that reclaims
	 * the prefix when inject_bytes needs room.
	 *
	 * Invariants:
	 *   0 <= head <= fill_len <= INPUT_DECODER_BUFFER_SIZE
	 *   buf[head..fill_len) is the live byte stream to parse next.
	 *
	 * For M2 the only producer is inject_bytes; M6 adds the read(2) path
	 * to the same buf without changing this contract. */
	uint8_t buf[INPUT_DECODER_BUFFER_SIZE];
	size_t  head;
	size_t  fill_len;

	/* Cumulative count of bytes dropped because the ring was full when
	 * inject_bytes (or future M2 / M3 read(2) loop) tried to deposit them.
	 * Exposed via input_decoder_dropped_bytes() so tests and (post-M2)
	 * production callers can detect silent desync.  Monotonic; never
	 * decreases.  This is the security-relevant back-pressure signal
	 * called out by plan section 3.12 (burst-read robustness). */
	size_t  dropped_bytes;

	/* 2026-06-30 JES #810 M2: parser state.
	 *
	 * state -- current state-machine node (DEC_STATE_*).  Persistent across
	 *   poll calls so partial sequences resume correctly.
	 * csi_buf / csi_len -- parameter + intermediate bytes between the CSI
	 *   introducer (\e[) and the final byte (0x40..0x7E).  Capped at
	 *   INPUT_DECODER_CSI_BUF_SIZE; overflow forces a reset to GROUND.
	 * utf8_buf / utf8_remaining / utf8_codepoint -- UTF-8 lead byte +
	 *   continuation accumulator.  remaining == 0 in GROUND state. */
	dec_state_t state;
	uint8_t     csi_buf[INPUT_DECODER_CSI_BUF_SIZE];
	size_t      csi_len;

	uint8_t     utf8_buf[4];
	uint8_t     utf8_remaining;
	uint32_t    utf8_codepoint;
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
	/* calloc zeroed the rest; explicit assignments below mirror the doc
	 * comments so the audit trail is unambiguous. */
	dec->mouse_enabled = false;
	dec->kitty_enabled = false;
	dec->head = 0;
	dec->fill_len = 0;
	dec->dropped_bytes = 0;
	dec->state = DEC_STATE_GROUND;
	dec->csi_len = 0;
	dec->utf8_remaining = 0;
	dec->utf8_codepoint = 0;
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

bool input_decoder_kitty_enabled(const input_decoder_t *dec) {
	if (dec == NULL) {
		return false;
	}
	return dec->kitty_enabled;
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: buffer compaction.
 *
 * When the parser has consumed bytes from buf[0..head), compact the live
 * tail buf[head..fill_len) back to the front so inject_bytes (or M6's
 * read(2)) has the full ring available again.  Cheap: typical fill is a
 * single sequence (~6-16 bytes); memmove is O(n) on a tiny n.
 *
 * Called eagerly at the bottom of poll once an event has been emitted (or
 * we've reached the end of buffered bytes), and from inject_bytes when the
 * available space would otherwise be smaller than the requested write.
 * Invariant: head == 0 on return.
 * ---------------------------------------------------------------------- */
static void compact_buffer(input_decoder_t *dec) {
	if (dec->head == 0) {
		return;
	}
	size_t live = dec->fill_len - dec->head;
	if (live > 0) {
		memmove(dec->buf, dec->buf + dec->head, live);
	}
	dec->head = 0;
	dec->fill_len = live;
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: helpers.
 *
 * emit_key initializes the out event to BOXEN_EV_KEY with the given fields;
 * the caller does not need to memset.  All M2 emit sites funnel through
 * here so the event shape stays consistent and we have one place to assert
 * invariants when reviewing.
 * ---------------------------------------------------------------------- */
static void emit_key(boxen_event_t *out, boxen_key_t key, uint32_t ch,
                     uint16_t mod) {
	memset(out, 0, sizeof(*out));
	out->type = BOXEN_EV_KEY;
	out->key.key = key;
	out->key.ch = ch;
	out->key.mod = mod;
}

/* 2026-06-30 JES #810 M2: modifier param decoder.
 *
 * Maps the modifier-param integer (1-based; 1=none, 2=Shift, 3=Alt, 4=Shift|Alt,
 * 5=Ctrl, ..., 16=Shift|Alt|Ctrl|Meta) to the BOXEN_MOD_* bitmask.  Plan
 * section 4.3 spells out the algorithm: subtract 1, then bit 0=SHIFT,
 * bit 1=ALT, bit 2=CTRL, bit 3=META.
 *
 * N <= 1 (or absent / unparseable) -> BOXEN_MOD_NONE.  N > 16 is treated
 * as NONE rather than masked-and-emitted because the protocol does not
 * define modifier params beyond 16 and a wild value is more likely a parse
 * error or hostile input than a legitimate combination. */
static uint16_t modifier_from_param(int n) {
	if (n <= 1 || n > 16) {
		return BOXEN_MOD_NONE;
	}
	int bits = n - 1;
	uint16_t mod = 0;
	if (bits & 0x1) mod |= BOXEN_MOD_SHIFT;
	if (bits & 0x2) mod |= BOXEN_MOD_ALT;
	if (bits & 0x4) mod |= BOXEN_MOD_CTRL;
	if (bits & 0x8) mod |= BOXEN_MOD_META;
	return mod;
}

/* 2026-06-30 JES #810 M2: parse the param prefix of a CSI sequence.
 *
 * The csi_buf holds the bytes that arrived between the CSI introducer
 * (\e[) and the final byte (already consumed by the caller).  It is a
 * semicolon-separated list of decimal integers, possibly with intermediate
 * bytes (0x20..0x2F) that we do not yet use at M2.
 *
 * out_params receives up to max_params parsed integers; *out_count is the
 * number of parameters actually present (which may be 0 for sequences like
 * \e[A).  Absent / empty positions default to 0 -- the caller distinguishes
 * "absent" from "0" by checking *out_count.
 *
 * Returns true on success, false if the buffer contains a non-digit /
 * non-';' byte outside the parameter alphabet (which the M2 callers treat
 * as a parse error -- reset to GROUND, no event emitted).  Non-numeric
 * intermediates are not expected on the M2 paths (\e[<...M is M3); their
 * presence is treated as failure here so we surface unknown-protocol bytes
 * as a clean drop rather than silently misparsing. */
static bool csi_parse_params(const uint8_t *csi_buf, size_t csi_len,
                             int *out_params, size_t max_params,
                             size_t *out_count) {
	size_t count = 0;
	int    cur = 0;
	bool   in_param = false;

	for (size_t i = 0; i < csi_len; i++) {
		uint8_t b = csi_buf[i];
		if (b >= '0' && b <= '9') {
			cur = (cur * 10) + (b - '0');
			in_param = true;
		} else if (b == ';') {
			if (count < max_params) {
				out_params[count] = in_param ? cur : 0;
			}
			count++;
			cur = 0;
			in_param = false;
		} else {
			/* Unexpected byte for an M2 sequence.  M3's SGR mouse will
			 * extend this to accept '<' as a leading intermediate; for
			 * now we conservatively bail. */
			return false;
		}
	}
	/* Trailing param (no terminating ';'). */
	if (in_param || csi_len == 0) {
		if (count < max_params) {
			out_params[count] = cur;
		}
		if (in_param) {
			count++;
		}
	}
	*out_count = count;
	return true;
}

/* 2026-06-30 JES #810 M2: dispatch a complete CSI sequence to a boxen_event.
 *
 * Returns true if an event was emitted, false if the sequence is unknown
 * (silently discarded -- never emitted as printable; plan section 3.12).
 *
 * The CSI dispatch table follows plan section 4.3 with M2-scope final
 * bytes only: A/B/C/D (arrows), H/F (Home/End), ~ (Ins/Del/PgUp/PgDn,
 * F1-legacy, F5..F12), P/Q/R/S (F1..F4 with modifier).  M, m, u are
 * reserved for M3 / M5 and currently fall through to "unknown".
 *
 * For the ~ form, plan section 3.3 / 3.4 specify the first param identifies
 * the key (2=Ins, 3=Del, 5=PgUp, 6=PgDn, 11..24=function keys) and the
 * second param (when present) is the modifier.  For the letter form, the
 * standard convention is \e[1;NLetter where N is the modifier; the M2
 * tests reflect this. */
static bool decode_csi(const uint8_t *csi_buf, size_t csi_len,
                       uint8_t final_byte, boxen_event_t *out) {
	int params[8];
	size_t count = 0;
	if (!csi_parse_params(csi_buf, csi_len, params, 8, &count)) {
		return false;
	}

	/* Letter-final cursor / nav: \e[A, \e[1;3A, \e[H, \e[1;5F, etc.  The
	 * modifier (when present) is in params[1].  Bare \e[A has count==0; we
	 * coerce to no-mod. */
	uint16_t letter_mod = (count >= 2) ? modifier_from_param(params[1])
	                                   : BOXEN_MOD_NONE;

	switch (final_byte) {
	case 'A':
		emit_key(out, BOXEN_KEY_UP, 0, letter_mod);
		return true;
	case 'B':
		emit_key(out, BOXEN_KEY_DOWN, 0, letter_mod);
		return true;
	case 'C':
		emit_key(out, BOXEN_KEY_RIGHT, 0, letter_mod);
		return true;
	case 'D':
		emit_key(out, BOXEN_KEY_LEFT, 0, letter_mod);
		return true;
	case 'H':
		emit_key(out, BOXEN_KEY_HOME, 0, letter_mod);
		return true;
	case 'F':
		emit_key(out, BOXEN_KEY_END, 0, letter_mod);
		return true;
	case 'P':
		emit_key(out, BOXEN_KEY_F1, 0, letter_mod);
		return true;
	case 'Q':
		emit_key(out, BOXEN_KEY_F2, 0, letter_mod);
		return true;
	case 'R':
		emit_key(out, BOXEN_KEY_F3, 0, letter_mod);
		return true;
	case 'S':
		emit_key(out, BOXEN_KEY_F4, 0, letter_mod);
		return true;
	case '~': {
		/* ~-form: first param identifies key; second (if present) is the
		 * modifier. */
		if (count == 0) {
			return false;
		}
		uint16_t tilde_mod = (count >= 2) ? modifier_from_param(params[1])
		                                  : BOXEN_MOD_NONE;
		switch (params[0]) {
		case 2:  emit_key(out, BOXEN_KEY_INSERT, 0, tilde_mod); return true;
		case 3:  emit_key(out, BOXEN_KEY_DELETE, 0, tilde_mod); return true;
		case 5:  emit_key(out, BOXEN_KEY_PGUP,   0, tilde_mod); return true;
		case 6:  emit_key(out, BOXEN_KEY_PGDN,   0, tilde_mod); return true;
		case 11: emit_key(out, BOXEN_KEY_F1,     0, tilde_mod); return true;
		case 12: emit_key(out, BOXEN_KEY_F2,     0, tilde_mod); return true;
		case 13: emit_key(out, BOXEN_KEY_F3,     0, tilde_mod); return true;
		case 14: emit_key(out, BOXEN_KEY_F4,     0, tilde_mod); return true;
		case 15: emit_key(out, BOXEN_KEY_F5,     0, tilde_mod); return true;
		case 17: emit_key(out, BOXEN_KEY_F6,     0, tilde_mod); return true;
		case 18: emit_key(out, BOXEN_KEY_F7,     0, tilde_mod); return true;
		case 19: emit_key(out, BOXEN_KEY_F8,     0, tilde_mod); return true;
		case 20: emit_key(out, BOXEN_KEY_F9,     0, tilde_mod); return true;
		case 21: emit_key(out, BOXEN_KEY_F10,    0, tilde_mod); return true;
		case 23: emit_key(out, BOXEN_KEY_F11,    0, tilde_mod); return true;
		case 24: emit_key(out, BOXEN_KEY_F12,    0, tilde_mod); return true;
		/* params[0] == 200 / 201 are bracketed paste markers (M4). */
		default: return false;
		}
	}
	default:
		/* M / m (mouse, M3), u (Kitty, M5), and unknown finals all land
		 * here.  Silently discarded -- never emit as printable. */
		return false;
	}
}

/* 2026-06-30 JES #810 M2: SS3 dispatch (\eO + one byte).
 *
 * SS3 form is used by VT100-compatible app-keypad mode for arrow keys,
 * Home/End, and F1..F4.  Modifiers do not propagate through SS3 in the
 * standard -- a modified function key arrives as \e[1;NP rather than
 * \eO<modified-letter>.  Plan section 3.4. */
static bool decode_ss3(uint8_t final_byte, boxen_event_t *out) {
	switch (final_byte) {
	case 'A': emit_key(out, BOXEN_KEY_UP,    0, BOXEN_MOD_NONE); return true;
	case 'B': emit_key(out, BOXEN_KEY_DOWN,  0, BOXEN_MOD_NONE); return true;
	case 'C': emit_key(out, BOXEN_KEY_RIGHT, 0, BOXEN_MOD_NONE); return true;
	case 'D': emit_key(out, BOXEN_KEY_LEFT,  0, BOXEN_MOD_NONE); return true;
	case 'H': emit_key(out, BOXEN_KEY_HOME,  0, BOXEN_MOD_NONE); return true;
	case 'F': emit_key(out, BOXEN_KEY_END,   0, BOXEN_MOD_NONE); return true;
	case 'P': emit_key(out, BOXEN_KEY_F1,    0, BOXEN_MOD_NONE); return true;
	case 'Q': emit_key(out, BOXEN_KEY_F2,    0, BOXEN_MOD_NONE); return true;
	case 'R': emit_key(out, BOXEN_KEY_F3,    0, BOXEN_MOD_NONE); return true;
	case 'S': emit_key(out, BOXEN_KEY_F4,    0, BOXEN_MOD_NONE); return true;
	default:  return false;
	}
}

/* 2026-06-30 JES #810 M2: control-char + printable ASCII dispatch.
 *
 * Plan section 3.11.  The ctrl-letter mapping uses the standard PC layout:
 *   0x01..0x1A -> CTRL_A..CTRL_Z
 *   0x09 (TAB) and 0x0D (ENTER) get their dedicated key codes (which alias
 *   to CTRL_I / CTRL_M in the boxen header).  We emit the dedicated codes
 *   so downstream consumers can distinguish "user pressed Tab" from "user
 *   pressed Ctrl-I" if their keymap ever needs to (the codes are aliases
 *   today but the indirection costs nothing and protects the option).
 *   0x08 (^H) -> BACKSPACE.
 *   0x7F (DEL) -> BACKSPACE -- macOS Terminal.app sends DEL for the
 *   Backspace key by default; treating both as the same logical key matches
 *   user expectation.
 *   0x1C (FS) -> CTRL_BACKSLASH; 0x00 -> CTRL_SPACE.
 *   Printable 0x20..0x7E -> ch=byte, key=NONE. */
static void decode_byte_ground(uint8_t b, boxen_event_t *out) {
	/* Special cases first; the CTRL_* range catches the rest. */
	if (b == 0x09) {
		emit_key(out, BOXEN_KEY_TAB, 0, BOXEN_MOD_NONE);
		return;
	}
	if (b == 0x0D) {
		emit_key(out, BOXEN_KEY_ENTER, 0, BOXEN_MOD_NONE);
		return;
	}
	if (b == 0x08) {
		emit_key(out, BOXEN_KEY_BACKSPACE, 0, BOXEN_MOD_NONE);
		return;
	}
	if (b == 0x7F) {
		emit_key(out, BOXEN_KEY_BACKSPACE, 0, BOXEN_MOD_NONE);
		return;
	}
	if (b == 0x00) {
		emit_key(out, BOXEN_KEY_CTRL_SPACE, 0, BOXEN_MOD_NONE);
		return;
	}
	if (b == 0x1C) {
		emit_key(out, BOXEN_KEY_CTRL_BACKSLASH, 0, BOXEN_MOD_NONE);
		return;
	}
	if (b >= 0x01 && b <= 0x1A) {
		/* CTRL_A..CTRL_Z are sequential in the enum starting at CTRL_A. */
		boxen_key_t k = (boxen_key_t)(BOXEN_KEY_CTRL_A + (b - 0x01));
		emit_key(out, k, 0, BOXEN_MOD_NONE);
		return;
	}
	/* 0x20..0x7E: printable ASCII. */
	if (b >= 0x20 && b <= 0x7E) {
		emit_key(out, BOXEN_KEY_NONE, (uint32_t)b, BOXEN_MOD_NONE);
		return;
	}
	/* Anything else (0x1B is handled in poll before we reach here; 0x1D
	 * 0x1E 0x1F have no boxen mapping today) -> printable as raw byte.
	 * Conservative: surfaces unexpected bytes rather than silently
	 * dropping. */
	emit_key(out, BOXEN_KEY_NONE, (uint32_t)b, BOXEN_MOD_NONE);
}

/* 2026-06-30 JES #810 M2: UTF-8 lead-byte classification.
 *
 * Returns the number of continuation bytes the lead expects (1..3) or 0
 * if the byte is not a valid UTF-8 lead (i.e., it's ASCII or invalid).
 * The lead byte's payload bits are written to *out_codepoint -- the
 * partial codepoint that subsequent continuation bytes shift in. */
static int utf8_lead_classify(uint8_t b, uint32_t *out_codepoint) {
	if ((b & 0xE0) == 0xC0) {            /* 110xxxxx */
		*out_codepoint = b & 0x1F;
		return 1;
	}
	if ((b & 0xF0) == 0xE0) {            /* 1110xxxx */
		*out_codepoint = b & 0x0F;
		return 2;
	}
	if ((b & 0xF8) == 0xF0) {            /* 11110xxx */
		*out_codepoint = b & 0x07;
		return 3;
	}
	return 0;
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: poll -- run the state machine against buffered
 * bytes until one event emits or we run out of bytes.
 *
 * Returns BOXEN_OK + writes *out on a successful emit.
 * Returns BOXEN_ERR_TIMEOUT (with *out zeroed) when no event can be
 *   produced from the currently-buffered bytes (empty buffer OR partial
 *   sequence still waiting for more bytes).
 * Returns BOXEN_ERR_INVALID for NULL args.
 *
 * timeout_ms is currently unused (M6 will use it for the select(2) wait
 * on tty_fd).  In the test seam the caller drives bytes via inject_bytes
 * before each poll, so a timeout has no observable effect.
 * ---------------------------------------------------------------------- */
int input_decoder_poll(input_decoder_t *dec, boxen_event_t *out, int timeout_ms) {
	(void)timeout_ms;   /* M6 wires the select(2) timeout. */

	if (dec == NULL || out == NULL) {
		return BOXEN_ERR_INVALID;
	}
	memset(out, 0, sizeof(*out));

	while (dec->head < dec->fill_len) {
		uint8_t b = dec->buf[dec->head];

		switch (dec->state) {
		case DEC_STATE_GROUND:
			/* Plan section 4.2: GROUND transitions. */
			if (b == 0x1B) {
				/* ESC: enter disambiguation.  Consume the byte and
				 * transition; the next iteration decides between ESC-alone
				 * (no more bytes), CSI, SS3, or Meta. */
				dec->head++;
				dec->state = DEC_STATE_ESC_RECEIVED;
				continue;
			}
			{
				uint32_t codepoint = 0;
				int needed = utf8_lead_classify(b, &codepoint);
				if (needed > 0) {
					/* UTF-8 multi-byte lead. */
					dec->head++;
					dec->utf8_codepoint = codepoint;
					dec->utf8_remaining = (uint8_t)needed;
					dec->utf8_buf[0] = b;
					dec->state = DEC_STATE_UTF8_CONT;
					continue;
				}
			}
			/* Single-byte: control char or printable ASCII (or high-bit
			 * non-lead).  Emit and consume. */
			dec->head++;
			decode_byte_ground(b, out);
			compact_buffer(dec);
			return BOXEN_OK;

		case DEC_STATE_ESC_RECEIVED:
			/* We have committed to consuming the ESC byte.  Now classify
			 * the follow-up.  See plan section 4.2 / 4.4. */
			if (b == '[') {
				dec->head++;
				dec->state = DEC_STATE_CSI_COLLECTING;
				dec->csi_len = 0;
				continue;
			}
			if (b == 'O') {
				dec->head++;
				dec->state = DEC_STATE_SS3_RECEIVED;
				continue;
			}
			if (b == 0x1B) {
				/* Double ESC: emit the first ESCAPE; re-enter
				 * ESC_RECEIVED for the new one without consuming the
				 * follow-up (next loop iteration handles it). */
				emit_key(out, BOXEN_KEY_ESCAPE, 0, BOXEN_MOD_NONE);
				dec->state = DEC_STATE_ESC_RECEIVED;
				dec->head++;   /* consume the second ESC, re-arm state */
				compact_buffer(dec);
				return BOXEN_OK;
			}
			/* ESC + any other byte: Meta-key.  Emit ch=byte, mod=ALT.
			 * Plan section 3.5 / 4.2: this covers \ea, \eb, \ef and any
			 * other ESC-prefixed printable.  For control chars after ESC
			 * (rare) we still apply the ALT mod and use the same single-
			 * byte decode logic, then OR in ALT.
			 *
			 * For non-control: ch=b, key=NONE, mod=ALT. */
			dec->head++;
			if (b >= 0x20 && b <= 0x7E) {
				emit_key(out, BOXEN_KEY_NONE, (uint32_t)b, BOXEN_MOD_ALT);
			} else if (b >= 0x01 && b <= 0x1A) {
				boxen_key_t k = (boxen_key_t)(BOXEN_KEY_CTRL_A + (b - 0x01));
				emit_key(out, k, 0, BOXEN_MOD_ALT);
			} else {
				emit_key(out, BOXEN_KEY_NONE, (uint32_t)b, BOXEN_MOD_ALT);
			}
			dec->state = DEC_STATE_GROUND;
			compact_buffer(dec);
			return BOXEN_OK;

		case DEC_STATE_CSI_COLLECTING: {
			/* Parameter / intermediate bytes are 0x20..0x3F per the
			 * standard.  Final bytes are 0x40..0x7E.  Anything else is a
			 * parse error -> reset to GROUND with no emit (the rogue byte
			 * itself is not re-processed; cleanest contract). */
			if (b >= 0x20 && b <= 0x3F) {
				if (dec->csi_len >= INPUT_DECODER_CSI_BUF_SIZE) {
					/* Overflow: bail to GROUND and consume so we don't
					 * loop forever on a malicious / runaway stream. */
					dec->head++;
					dec->state = DEC_STATE_GROUND;
					dec->csi_len = 0;
					continue;
				}
				dec->csi_buf[dec->csi_len++] = b;
				dec->head++;
				continue;
			}
			if (b >= 0x40 && b <= 0x7E) {
				/* Final byte: decode. */
				dec->head++;
				bool emitted = decode_csi(dec->csi_buf, dec->csi_len, b, out);
				dec->state = DEC_STATE_GROUND;
				dec->csi_len = 0;
				if (emitted) {
					compact_buffer(dec);
					return BOXEN_OK;
				}
				/* Unknown final: discard silently, keep parsing. */
				continue;
			}
			/* Garbage in the middle of a CSI: reset and drop. */
			dec->head++;
			dec->state = DEC_STATE_GROUND;
			dec->csi_len = 0;
			continue;
		}

		case DEC_STATE_SS3_RECEIVED: {
			dec->head++;
			bool emitted = decode_ss3(b, out);
			dec->state = DEC_STATE_GROUND;
			if (emitted) {
				compact_buffer(dec);
				return BOXEN_OK;
			}
			continue;
		}

		case DEC_STATE_UTF8_CONT: {
			if ((b & 0xC0) != 0x80) {
				/* Premature non-continuation: discard the partial codepoint
				 * and reprocess this byte in GROUND.  Do NOT consume here
				 * -- the next loop iteration sees it again under
				 * DEC_STATE_GROUND.  This matches plan section 4.2 (UTF8
				 * fallthrough). */
				dec->state = DEC_STATE_GROUND;
				dec->utf8_remaining = 0;
				dec->utf8_codepoint = 0;
				continue;
			}
			/* Valid continuation byte: shift in the 6 payload bits. */
			dec->head++;
			dec->utf8_codepoint = (dec->utf8_codepoint << 6) | (b & 0x3F);
			dec->utf8_remaining--;
			if (dec->utf8_remaining == 0) {
				emit_key(out, BOXEN_KEY_NONE, dec->utf8_codepoint,
				         BOXEN_MOD_NONE);
				dec->state = DEC_STATE_GROUND;
				dec->utf8_codepoint = 0;
				compact_buffer(dec);
				return BOXEN_OK;
			}
			/* Still need more continuation bytes. */
			continue;
		}
		}
	}

	/* Drained all buffered bytes without emitting an event.  Two subcases:
	 *
	 * 1. state == GROUND -- truly nothing pending.  Compact and return
	 *    TIMEOUT.
	 * 2. state == ESC_RECEIVED with no follow-up byte -- this is the lone
	 *    Escape disambiguation (plan section 4.4).  In the production read
	 *    loop (M6) we drain the pipe non-blocking after seeing ESC; if zero
	 *    bytes follow, emit ESCAPE.  In the test seam (and equivalently
	 *    when input_decoder_poll is called with timeout_ms == 0 and we've
	 *    truly exhausted the buffer) we treat the poll boundary as
	 *    "no more bytes are coming" and emit ESCAPE.
	 * 3. state == any other partial -- partial sequence; preserve state
	 *    across calls.  Return TIMEOUT; next inject_bytes + poll resumes. */
	if (dec->state == DEC_STATE_ESC_RECEIVED) {
		emit_key(out, BOXEN_KEY_ESCAPE, 0, BOXEN_MOD_NONE);
		dec->state = DEC_STATE_GROUND;
		compact_buffer(dec);
		return BOXEN_OK;
	}

	compact_buffer(dec);
	return BOXEN_ERR_TIMEOUT;
}

/* -------------------------------------------------------------------------
 * Test seam (compile-guarded)
 * ---------------------------------------------------------------------- */

#ifdef INPUT_DECODER_TEST_SEAM

size_t input_decoder_inject_bytes(input_decoder_t *dec,
                                  const uint8_t *bytes, size_t len) {
	if (dec == NULL || len == 0) {
		return 0;
	}
	/* bytes == NULL with len > 0 is a programmer error; the test seam is
	 * exactly where we want such bugs to surface (rather than silently
	 * no-op'ing and looking like the inject succeeded). */
	assert(bytes != NULL);

	/* 2026-06-30 JES #810 M2: compact before measuring available space.
	 * Without this, a buffer where head has advanced (e.g., several events
	 * have been drained but the tail wasn't compacted yet) reports less
	 * free space than actually exists.  compact_buffer is a no-op when
	 * head==0, so this is free in the common case. */
	compact_buffer(dec);

	/* Load-bearing invariant: fill_len never exceeds buffer size.  Cheap
	 * to assert here; protects M2 / M3 future drain code from a partial-
	 * compaction bug that would otherwise produce an underflow on the
	 * available-space calculation below. */
	assert(dec->fill_len <= INPUT_DECODER_BUFFER_SIZE);

	size_t available = INPUT_DECODER_BUFFER_SIZE - dec->fill_len;
	size_t to_copy = (len < available) ? len : available;

	if (to_copy > 0) {
		memcpy(dec->buf + dec->fill_len, bytes, to_copy);
		dec->fill_len += to_copy;
	}

	/* Surface overflow rather than swallow it.  Plan section 3.12 calls
	 * for burst-read robustness; the dropped_bytes counter is the test-
	 * observable signal that M2 / M3 / M4 will use to assert their drain
	 * cadences are correct.  See input_decoder.h for the back-pressure
	 * contract. */
	size_t dropped = len - to_copy;
	if (dropped > 0) {
		dec->dropped_bytes += dropped;
	}

	return to_copy;
}

size_t input_decoder_buffered_bytes(const input_decoder_t *dec) {
	if (dec == NULL) {
		return 0;
	}
	/* 2026-06-30 JES #810 M2: report only live tail (head..fill_len).
	 * The M1 contract was "bytes injected since create"; M2 introduces the
	 * head cursor (parser consumption point), so "buffered" now means
	 * "bytes the parser has not yet consumed".  Test
	 * test_inject_bytes_appends_to_buffer still passes because head is 0
	 * until poll runs. */
	return dec->fill_len - dec->head;
}

size_t input_decoder_dropped_bytes(const input_decoder_t *dec) {
	if (dec == NULL) {
		return 0;
	}
	return dec->dropped_bytes;
}

#endif /* INPUT_DECODER_TEST_SEAM */
