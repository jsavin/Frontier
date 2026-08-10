/*
 * input_decoder.c -- private terminal input decoder for the boxen tb2 backend.
 *
 * 2026-06-29 JES #809 M1: PTY-replay harness + decoder stub.
 * 2026-06-30 JES #810 M2: cursor-key / SS3 / modifier-param / control-char
 *   / UTF-8 state machine.  This is the first milestone that emits real
 *   events from the buffered byte stream.
 * 2026-06-29 JES #811 M3: SGR mouse parsing, X10 fallback, double-click
 *   synthesis, burst-read contract.
 * 2026-06-29 JES #812 M4: bracketed-paste handling (PASTE_ACTIVE state +
 *   heap-grown paste buffer + BOXEN_EV_PASTE emission with CR/LF
 *   normalization, 256 KiB size cap, and BOXEN_LOG_W on truncation).
 * 2026-06-29 JES #813 M5: Kitty keyboard protocol -- input_decoder_kitty_enable
 *   writes "\x1b[=1u" (progressive enhancement: disambiguate escape codes)
 *   on the TTY fd, and decode_csi gains a 'u' case that decodes the CSI-u
 *   form (\e[KEYCODE;MODIFIER u) to boxen_event_t.  Terminals that don't
 *   support kitty (Terminal.app, iTerm2) silently ignore the enable and
 *   send no CSI-u traffic -- the standard CSI path from M2 still handles
 *   their input correctly.
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
#include "boxen_internal.h"   /* BOXEN_DOUBLE_CLICK_MS, BOXEN_DOUBLE_CLICK_RADIUS */

#include <assert.h>
#include <errno.h>            /* 2026-06-29 JES #805 M6: EINTR / EAGAIN handling on read(2) */
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>       /* 2026-06-29 JES #805 M6: select(2) for blocking poll wait */
#include <time.h>             /* clock_gettime for double-click synthesis */
#include <unistd.h>           /* write(2) for mouse-enable sequences (M3); read(2) for M6 */

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

/* 2026-06-29 JES #812 M4: bracketed-paste sizing.
 *
 * INPUT_DECODER_PASTE_CAP is the hard ceiling on a single paste's accepted
 * byte count.  Past this size we keep PARSING the paste body (so the
 * \e[201~ marker still terminates cleanly and we return to GROUND) but
 * STOP appending bytes to the heap buffer.  The cap fires once per paste;
 * BOXEN_LOG_W is emitted the first time we drop a byte on this path so
 * the operator sees a single deterministic warning per truncated paste.
 *
 * INPUT_DECODER_PASTE_INITIAL is the first heap allocation size.  Reads
 * grow geometrically (doubling) up to the cap; the initial size matters
 * only for short pastes (the common case -- a clipboard snippet is
 * typically <1 KiB) so we don't waste pages on a single keypress-worth
 * of data.  256 bytes covers most identifier / URL pastes without a
 * realloc; longer pastes pay one or two realloc costs to reach 256 KiB.
 *
 * Plan reference: section 3.8 (size limits + truncation contract),
 * section 7 M4 (deliverable scope), risk R5 (size-cap overflow). */
#define INPUT_DECODER_PASTE_CAP     (256 * 1024)
#define INPUT_DECODER_PASTE_INITIAL 256

/* 2026-06-29 JES #812 M4: bracketed-paste close-marker length.
 *
 * The closing sequence "\x1b[201~" is 6 bytes.  We scan a 6-byte suffix
 * window of the paste buffer after each appended byte to detect the
 * close marker.  Keeping the constant near the buffer cap and the
 * append site makes the suffix-scan invariant easy to audit. */
#define INPUT_DECODER_PASTE_CLOSE_LEN 6

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

/* 2026-06-29 JES #811 M3: SGR mouse coordinate saturation cap.
 *
 * Used by decode_sgr_mouse to clamp multi-digit params (col / row / button)
 * before they overflow `int`.  9999 is intentionally well below INT_MAX/10
 * (no overflow possible on the next *10 step) and well above any realistic
 * terminal dimension (modern terminals top out near 4000 cols).  Kept smaller
 * than CSI_PARAM_CAP (100000, used by the generic CSI parser) because the
 * mouse path has no need for the larger range and a tighter bound is a
 * smaller hostile-input surface.  If a future ultra-wide terminal needs more
 * room, raise both caps together and re-audit the generic parser. */
#define SGR_COORD_CAP 9999

/* 2026-06-29 JES #811 M3: SGR mouse modifier bitmask.
 *
 * Bits in the SGR button byte that encode keyboard modifiers (plan section 3.6
 * and xterm ctlseqs):
 *   bit 2 (value  4) = Shift
 *   bit 3 (value  8) = Meta / Alt
 *   bit 4 (value 16) = Ctrl
 * Same encoding for X10 (3.7).  Centralizing the mask keeps the SGR and X10
 * code paths visibly symmetric and prevents drift if the mapping changes. */
#define SGR_MOUSE_MOD_MASK ((unsigned)(4 | 8 | 16))

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
	/* 2026-06-30 JES #810 M2 review-followup: drain a malformed/oversized CSI
	 * sequence to its final byte without emitting any event.  Entered when
	 * csi_buf overflows or when csi_parse_params rejects a non-numeric byte.
	 * Prevents the post-overflow parameter tail from being emitted as
	 * synthetic printable keystrokes (the original M2 implementation reset to
	 * GROUND on overflow, which let `\e[ <65+ digits> A` inject the trailing
	 * digits into the input stream after the cap was hit). */
	DEC_STATE_CSI_SWALLOW,
	/* 2026-06-29 JES #812 M4: inside a bracketed paste.
	 *
	 * Entered when decode_csi sees \e[200~; left when the trailing
	 * "\x1b[201~" byte sequence is observed in the paste content.  While
	 * in this state EVERY byte (including ESC and CSI introducers) is
	 * treated as literal paste content -- the state machine NEVER falls
	 * back into CSI parsing for bytes that arrive during a paste, even
	 * if they would be valid escape sequences in GROUND.  This is the
	 * point of bracketed paste: a Cmd-V containing "\e[A" must appear as
	 * literal text in the paste buffer, not as a KEY_UP event. */
	DEC_STATE_PASTE_ACTIVE,
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
	 *   INPUT_DECODER_CSI_BUF_SIZE; overflow transitions to CSI_SWALLOW so
	 *   the rest of the sequence is consumed without emitting.
	 * utf8_lead / utf8_remaining / utf8_codepoint -- UTF-8 lead byte +
	 *   continuation accumulator.  remaining == 0 in GROUND state.  The
	 *   lead byte is retained so the overlong-encoding check (see
	 *   decode_utf8_codepoint_valid) can compare the assembled codepoint
	 *   against the minimum-encoding boundary for its byte length. */
	dec_state_t state;
	uint8_t     csi_buf[INPUT_DECODER_CSI_BUF_SIZE];
	size_t      csi_len;

	uint8_t     utf8_lead;
	uint8_t     utf8_remaining;
	uint32_t    utf8_codepoint;

	/* 2026-06-30 JES #810 M2 review-followup: parse-error counter.
	 *
	 * Incremented on every silent rejection path (CSI overflow, unknown CSI
	 * final byte, malformed CSI parameter, invalid SS3 final, invalid UTF-8
	 * codepoint -- surrogate, overlong, out-of-range).  Surfaced via the
	 * test-seam accessor input_decoder_parse_errors() so tests can assert
	 * the rejection path actually fired (otherwise a "no event emitted"
	 * outcome is indistinguishable from a benign buffer-empty timeout).
	 *
	 * Monotonic; never decreases.  The M6 production path will use the same
	 * counter as a runaway-terminal detection signal alongside
	 * dropped_bytes. */
	size_t      parse_errors;

	/* 2026-06-29 JES #811 M3: double-click synthesis state.
	 *
	 * Ported from backend_tb2.c:46-83 (tb2_maybe_set_double_click).  The
	 * decoder owns this state from M3 onward; backend_tb2.c will drop its
	 * own copy when M6 cuts over (tracked: see backend_tb2.c comment marker
	 * "M6 DELETE: g_tb2_last_press").  The invariant: last_press.valid is
	 * true after any mouse press event is emitted; cleared when a double-
	 * click is synthesized or when the position/button no longer matches.
	 * The time window and radius are defined in boxen_internal.h:
	 *   BOXEN_DOUBLE_CLICK_MS     = 500
	 *   BOXEN_DOUBLE_CLICK_RADIUS = 1
	 *
	 * NOT thread-safe; single-owner invariant required (see input_decoder.h
	 * threading contract, lines 27-38).  If a future milestone adds a second
	 * concurrent caller to mouse_maybe_double_click (e.g., a background paste
	 * reader thread), this field must be protected or moved to a per-thread
	 * context before that work lands.
	 *
	 * x10_pending: set when the X10 \e[M introducer has been seen but the
	 * three payload bytes have not yet arrived.  The poll loop transitions
	 * to consuming 3 raw bytes instead of running the state machine.  Same
	 * single-owner constraint applies. */
	struct {
		uint64_t time_ms;
		int      x;
		int      y;
		uint8_t  button;
		bool     valid;
	} last_press;

	/* X10 mouse pending: after seeing the \e[M introducer with no '<' prefix
	 * (i.e., csi_buf was empty when final byte 'M' arrived), we need to read
	 * 3 raw payload bytes from the ring before emitting the event.  This flag
	 * suspends normal state-machine processing until the bytes arrive. */
	bool x10_pending;

	/* 2026-06-29 JES #812 M4: bracketed-paste accumulator.
	 *
	 * paste_buf is heap-allocated lazily on entry to PASTE_ACTIVE (when
	 * decode_csi sees \e[200~).  paste_len tracks the number of bytes
	 * appended so far.  paste_cap tracks the current allocated capacity,
	 * grown geometrically up to INPUT_DECODER_PASTE_CAP.  paste_truncated
	 * latches true the first time we drop a byte (one BOXEN_LOG_W per
	 * paste, not one per dropped byte).
	 *
	 * paste_close_match is the count of consecutive bytes of the
	 * "\x1b[201~" close marker matched so far.  Tracked independently of
	 * the paste buffer because, after the truncation cap is reached, we
	 * STOP appending to paste_buf but must KEEP scanning the byte stream
	 * for the close marker -- otherwise a paste larger than the cap
	 * never terminates and the decoder stays wedged in PASTE_ACTIVE
	 * forever.  On a partial mismatch (e.g., "\x1b[20" followed by 'x'),
	 * the previously matched bytes are committed to paste_buf and the
	 * matcher resets (see flush_partial_close_match in poll).
	 *
	 * On a successful close (\x1b[201~ seen), ownership of paste_buf
	 * transfers to the emitted BOXEN_EV_PASTE event (ev.paste.data); the
	 * decoder zeroes its own pointer and the caller owns the free().  On
	 * input_decoder_destroy with an in-progress paste, paste_buf is freed
	 * to prevent a leak.  See plan section 3.8 + risk R7 (caller-free
	 * contract).
	 *
	 * Memory model: paste_buf can be NULL if the paste body is empty (no
	 * bytes between markers); in that case the emitted event has
	 * data=NULL, len=0.  Otherwise paste_buf is a single contiguous
	 * malloc'd region of paste_cap bytes, of which the first paste_len
	 * are populated.
	 *
	 * Single-owner / GIL invariant from input_decoder.h applies; no
	 * locking. */
	char    *paste_buf;
	size_t   paste_len;
	size_t   paste_cap;
	bool     paste_truncated;
	uint8_t  paste_close_match;   /* 0..INPUT_DECODER_PASTE_CLOSE_LEN */
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
	dec->utf8_lead = 0;
	dec->utf8_remaining = 0;
	dec->utf8_codepoint = 0;
	dec->parse_errors = 0;
	/* 2026-06-29 JES #812 M4: paste buffer is lazy-allocated on entry to
	 * PASTE_ACTIVE.  All fields zeroed by calloc above; explicit reset
	 * here documents the post-condition. */
	dec->paste_buf         = NULL;
	dec->paste_len         = 0;
	dec->paste_cap         = 0;
	dec->paste_truncated   = false;
	dec->paste_close_match = 0;
	return dec;
}

void input_decoder_destroy(input_decoder_t *dec) {
	if (dec == NULL) {
		return;
	}
	/* 2026-06-29 JES #812 M4: free the in-progress paste buffer if any.
	 * The buffer is normally consumed by the BOXEN_EV_PASTE emission
	 * (ownership transfers to the caller, who frees ev.paste.data); if
	 * the decoder is destroyed mid-paste -- e.g., shutdown during a
	 * runaway paste burst, or a test that exits early -- this prevents a
	 * leak.  free(NULL) is a no-op, so this is safe whether or not a
	 * paste was active. */
	free(dec->paste_buf);
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

	/* 2026-06-29 JES #811 M3: write the escape sequences to the TTY fd.
	 *
	 * When tty_fd == -1 (test harness) we skip the write -- the tests
	 * inject bytes directly and do not need real terminal mode changes.
	 *
	 * Enable path:  \e[?1006h  (SGR mouse mode 1006 on)
	 *               \e[?1000h  (basic mouse on -- required by some terminals
	 *                           before 1006 takes effect)
	 *               \e[?2004h  (bracketed paste on, M4 -- included here so
	 *                           the enable/disable pair stays symmetric;
	 *                           the M4 paste parser handles the markers)
	 *
	 * Disable path: \e[?1006l  (SGR off)
	 *               \e[?1000l  (basic mouse off)
	 *               \e[?2004l  (bracketed paste off)
	 *
	 * Per plan section 5.3 / 5.5: bracketed paste (mode 2004) is always
	 * toggled in lockstep with mouse to keep the terminal's input model
	 * consistent -- enabling mouse without paste breaks Cmd-V.
	 *
	 * write(2) errors are silently ignored: a write failure means the fd
	 * is closed or the terminal closed -- the next read(2) will surface
	 * the same condition and let the poll loop handle it cleanly.  Retrying
	 * a partial write is unnecessary for short control sequences (they fit
	 * in one pipe buffer page) and adds noise in the test-seam path. */
	if (dec->tty_fd >= 0) {
		if (enable) {
			static const char ON[] = "\x1b[?1000h\x1b[?1006h\x1b[?2004h";
			(void)write(dec->tty_fd, ON, sizeof(ON) - 1);
		} else {
			static const char OFF[] = "\x1b[?1006l\x1b[?1000l\x1b[?2004l";
			(void)write(dec->tty_fd, OFF, sizeof(OFF) - 1);
		}
	}
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
	/* 2026-06-29 JES #813 M5: idempotent.  If kitty was already enabled
	 * (a second slash command, a re-init after a SIGWINCH-driven probe,
	 * etc.) skip the duplicate write so we don't pile up enable sequences
	 * in the terminal's input queue.  The decode path in decode_csi
	 * doesn't actually consult dec->kitty_enabled -- a terminal that
	 * sends CSI-u sequences without us asking is decoded just the same --
	 * so the bit is documentary plus the gate for the one-shot write
	 * below. */
	if (dec->kitty_enabled) {
		return;
	}
	dec->kitty_enabled = true;

	/* 2026-06-29 JES #813 M5: write the progressive-enhancement enable.
	 *
	 * "\x1b[=1u" sets the "disambiguate escape codes" flag, which is the
	 * minimum kitty progressive-enhancement level that gives us
	 * unambiguous reports for Escape, Tab, Enter, and the modifier-
	 * disambiguated keys we care about for issue #805.  Higher flag
	 * values (2 = report event types, 4 = report alternate keys) are
	 * post-launch -- the M5 decoder only handles the two-part
	 * KEYCODE;MODIFIER form.  Asking for more flags than we can decode
	 * would be a protocol-conformance footgun: the terminal would send
	 * the three-part form and the decoder would silently drop it via
	 * the count==0 / unknown-keycode branches in decode_csi 'u'.
	 *
	 * Probe-and-read for terminal acknowledgement (the optional
	 * \e[=FLAGS u response) is deferred to M6, where the read(2) loop
	 * lives -- M5 ships only the decoder's ability to handle CSI-u when
	 * it arrives, per the milestone scope in plan section 7.  If the
	 * terminal does NOT support kitty (Terminal.app, iTerm2 as of this
	 * writing), the enable sequence is silently ignored by the terminal
	 * and no CSI-u traffic arrives -- the decoder operates as if
	 * kitty_enable had never been called.  Standard CSI parsing from M2
	 * continues to handle the keys those terminals do send.
	 *
	 * Test-seam path (tty_fd == -1) skips the write -- mirrors the
	 * pattern from input_decoder_set_mouse (lines 409..417 above).
	 * write(2) errors are silently ignored for the same reason as in
	 * set_mouse: a write failure means the fd is closed and the next
	 * read will surface the same condition cleanly via the poll loop.
	 * Short control sequences fit in a single pipe buffer page so
	 * partial-write handling is not required. */
	if (dec->tty_fd >= 0) {
		static const char ENABLE[] = "\x1b[=1u";
		(void)write(dec->tty_fd, ENABLE, sizeof(ENABLE) - 1);
	}
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
	/* 2026-06-30 JES #810 M2 review-followup: defense-in-depth on the
	 * head <= fill_len invariant.  Without the assert, a hypothetical future
	 * off-by-one in the parser that pushed head past fill_len would silently
	 * underflow `live` (size_t subtraction) and produce a catastrophic OOB
	 * memmove.  The state machine has 5+ head++ sites; cheap to pin the
	 * invariant at the choke point. */
	assert(dec->head <= dec->fill_len);
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

/* 2026-06-29 JES #811 M3: mouse event emitter.
 *
 * Fills *out as a BOXEN_EV_MOUSE event with the decoded fields.
 * All M3 emit sites funnel through here for consistency. */
static void emit_mouse(boxen_event_t *out, uint8_t button, bool pressed,
                       int x, int y, uint16_t mod, uint16_t flags) {
	memset(out, 0, sizeof(*out));
	out->type         = BOXEN_EV_MOUSE;
	out->mouse.button  = button;
	out->mouse.pressed = pressed;
	out->mouse.x       = x;
	out->mouse.y       = y;
	out->mouse.mod     = mod;
	out->mouse.flags   = flags;
}

/* 2026-06-29 JES #811 M3: monotonic millisecond clock.
 *
 * Same implementation as backend_tb2.c:tb2_now_ms().  Returns milliseconds
 * since an arbitrary epoch (CLOCK_MONOTONIC).  Used only for double-click
 * synthesis; accuracy to 1 ms is sufficient.
 *
 * 2026-06-29 JES #811 M3 gate-fix: per concurrency-reviewer PR #818, note that
 * CLOCK_MONOTONIC on macOS 10.12+ is immune to NTP and sleep/wake adjustments.
 * On pre-10.12 macOS it can regress across sleep; the elapsed computation
 * below uses unsigned subtraction, so a backward jump produces a huge
 * wrap-around value which always fails the "<= BOXEN_DOUBLE_CLICK_MS" test
 * cleanly -- no double-click is synthesized, and last_press is replaced as
 * if this were a fresh first press.  Behavior is safe (no UB, no spurious
 * double-click); the only observable effect is a missed double-click that
 * straddles a sleep cycle.  Documented rather than worked around because the
 * Frontier build minimum has been macOS 10.13+ for years.
 *
 * 2026-06-29 JES #811 M3 gate-fix: also adds a test-only clock seam.  When
 * dec_clock_for_testing is non-NULL, dec_now_ms returns its value instead of
 * the wall-clock.  This lets double-click tests assert both the positive
 * (within window) and negative (past window) paths deterministically without
 * depending on real-time scheduling.  Set via input_decoder_set_clock_for_testing
 * (INPUT_DECODER_TEST_SEAM only). */
#ifdef INPUT_DECODER_TEST_SEAM
static bool     dec_clock_override_set = false;
static uint64_t dec_clock_override_ms  = 0;
#endif

static uint64_t dec_now_ms(void) {
#ifdef INPUT_DECODER_TEST_SEAM
	if (dec_clock_override_set) {
		return dec_clock_override_ms;
	}
#endif
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* 2026-06-29 JES #811 M3: double-click synthesis.
 *
 * Ported from backend_tb2.c:54-83 (tb2_maybe_set_double_click).
 * Called after filling *ev with a BOXEN_EV_MOUSE press event.
 * If the press matches the previous press in position, button, and time,
 * sets BOXEN_MOUSE_DOUBLE_CLICK on ev->mouse.flags and clears last_press.
 * Otherwise records this press as the new candidate.
 *
 * Non-press events (pressed==false, motion) are passed through unchanged
 * without updating last_press. */
static void mouse_maybe_double_click(input_decoder_t *dec, boxen_event_t *ev) {
	if (!ev->mouse.pressed) {
		return;
	}

	uint64_t now = dec_now_ms();

	if (dec->last_press.valid) {
		uint64_t elapsed = now - dec->last_press.time_ms;
		int dx = ev->mouse.x - dec->last_press.x;
		int dy = ev->mouse.y - dec->last_press.y;
		if (dx < 0) dx = -dx;
		if (dy < 0) dy = -dy;
		int dist = dx > dy ? dx : dy;

		if (elapsed <= BOXEN_DOUBLE_CLICK_MS
		    && dist  <= BOXEN_DOUBLE_CLICK_RADIUS
		    && ev->mouse.button == dec->last_press.button) {
			ev->mouse.flags |= BOXEN_MOUSE_DOUBLE_CLICK;
			dec->last_press.valid = false;
			return;
		}
	}

	dec->last_press.time_ms = now;
	dec->last_press.x       = ev->mouse.x;
	dec->last_press.y       = ev->mouse.y;
	dec->last_press.button  = ev->mouse.button;
	dec->last_press.valid   = true;
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
	/* 2026-06-30 JES #810 M2 review-followup: cap accumulator below INT_MAX
	 * to defeat signed-integer-overflow UB on adversarial input.  No
	 * legitimate CSI parameter exceeds ~10000 (largest in spec is the SGR
	 * mouse coord, bounded by terminal width/height).  Saturating at
	 * CSI_PARAM_CAP and refusing to grow it further keeps the accumulator
	 * far from INT_MAX even on a runaway terminal that fills the entire
	 * 64-byte CSI buffer with digits.  modifier_from_param's `n > 16`
	 * guard rejects the saturated value cleanly. */
	enum { CSI_PARAM_CAP = 100000 };

	size_t count = 0;
	int    cur = 0;
	bool   in_param = false;

	for (size_t i = 0; i < csi_len; i++) {
		uint8_t b = csi_buf[i];
		if (b >= '0' && b <= '9') {
			int digit = b - '0';
			if (cur >= CSI_PARAM_CAP) {
				/* Already saturated; ignore further digits to avoid
				 * approaching INT_MAX. */
				cur = CSI_PARAM_CAP;
			} else {
				cur = (cur * 10) + digit;
				if (cur > CSI_PARAM_CAP) {
					cur = CSI_PARAM_CAP;
				}
			}
			in_param = true;
		} else if (b == ';') {
			if (count < max_params) {
				out_params[count] = in_param ? cur : 0;
				count++;
			} else {
				/* 2026-06-30 JES #810 M2 review-followup: clamp count at
				 * max_params to keep the contract honest -- the caller
				 * reads params[0..count); allowing count to grow past
				 * max_params would let a future caller read uninitialized
				 * stack memory. */
			}
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
			if (in_param) {
				count++;
			}
		}
	}
	*out_count = count;
	return true;
}

/* 2026-06-29 JES #811 M3: SGR mouse decoder.
 *
 * Called when the CSI collector saw a '<' intermediate byte and the final byte
 * is 'M' (press) or 'm' (release).  The csi_buf at that point holds
 * '<button;col;row' (the '<' is in csi_buf[0] because it's an intermediate
 * byte in the 0x20..0x3F range).
 *
 * SGR button encoding (plan section 3.6):
 *   button raw value | meaning
 *   0                | left press
 *   1                | middle press
 *   2                | right press
 *   32               | motion (drag)
 *   64               | wheel up
 *   65               | wheel down
 *   +4               | shift modifier
 *   +8               | meta modifier
 *   +16              | ctrl modifier
 *
 * Produces boxen button numbers: left=1, middle=2, right=3, wheel-up=4,
 * wheel-down=5, motion=0 (no button active).
 *
 * Returns true on success, false if the param string is malformed. */
static bool decode_sgr_mouse(input_decoder_t *dec, const uint8_t *csi_buf,
                             size_t csi_len, bool pressed, boxen_event_t *out) {
	/* csi_buf[0] must be '<'; skip it for numeric parsing. */
	if (csi_len == 0 || csi_buf[0] != '<') {
		return false;
	}

	/* Parse three semicolon-separated params from csi_buf+1.
	 *
	 * 2026-06-29 JES #811 M3 gate-fix: per bar-raiser PR #818 review, the
	 * parser must REJECT malformed sequences rather than coerce them into
	 * plausible-looking events.  Three rejection rules below:
	 *   (a) empty param (";;" or leading/trailing ";")  -> false
	 *   (b) more than 3 params (extra ";N" or junk after the third)  -> false
	 *   (c) zero coord (col=0 or row=0)  -> false (1-based; col-1 underflow)
	 * Without these, e.g. "\e[<;5;3M" became a left-click at (4,2), and
	 * "\e[<0;10;5;99M" silently dropped the fourth param and accepted (9,4).
	 * Both are confused-deputy hazards for any UI that trusts the event. */
	int params[3] = {0, 0, 0};
	size_t count = 0;
	int cur = 0;
	bool in_param = false;

	for (size_t i = 1; i < csi_len; i++) {
		uint8_t b = csi_buf[i];
		if (b >= '0' && b <= '9') {
			cur = cur * 10 + (b - '0');
			/* Saturate at SGR_COORD_CAP to prevent integer overflow on a
			 * pathological multi-digit param.  9999 is well above any
			 * realistic terminal width/height (modern terminals top out
			 * near 4000 cols) and well below INT_MAX/10. */
			if (cur > SGR_COORD_CAP) cur = SGR_COORD_CAP;
			in_param = true;
		} else if (b == ';') {
			/* Empty param (no digit before ';') -> malformed. */
			if (!in_param) {
				return false;
			}
			/* Too many params (would write past params[2]) -> malformed. */
			if (count >= 3) {
				return false;
			}
			params[count++] = cur;
			cur = 0;
			in_param = false;
		} else {
			return false;  /* unexpected byte */
		}
	}
	/* Last param (no trailing ';'). */
	if (in_param) {
		if (count >= 3) {
			return false;  /* too many params */
		}
		params[count++] = cur;
	}

	if (count < 3) {
		return false;  /* need button;col;row */
	}

	int raw_button = params[0];
	int col        = params[1];  /* 1-based; 0 is malformed */
	int row        = params[2];  /* 1-based; 0 is malformed */

	/* SGR mouse coords are 1-based per the protocol; col=0 or row=0 is
	 * malformed (would underflow to -1 when converted to 0-based below).
	 * Reject rather than emit a negative-coord event. */
	if (col < 1 || row < 1) {
		return false;
	}

	/* Extract modifier bits from the button byte (plan section 3.6).
	 * See SGR_MOUSE_MOD_MASK; bits 2/3/4 = Shift/Meta/Ctrl. */
	uint16_t mod = 0;
	if (raw_button & 4)  mod |= BOXEN_MOD_SHIFT;
	if (raw_button & 8)  mod |= BOXEN_MOD_META;
	if (raw_button & 16) mod |= BOXEN_MOD_CTRL;

	/* Strip modifier bits to get the base button. */
	int base = (int)((unsigned)raw_button & ~SGR_MOUSE_MOD_MASK);

	/* Map to boxen button numbers.
	 * base 32 = motion (drag); mask off the motion bit. */
	uint8_t button;
	bool    is_motion = false;

	if (base & 32) {
		/* Drag / motion: strip the motion bit and map the underlying button.
		 * SGR button=32 alone = pure motion (no button pressed).
		 * SGR button=32+0=32 = left drag, 32+1=33 = middle drag, etc.
		 * BUT: the convention used by xterm/Terminal.app is that the motion
		 * bit is added ON TOP of the button number.  So base=32 with
		 * modifier bits stripped:
		 *   32 alone (base_no_motion=0): pure motion, no button -> button=0
		 *   33 (base_no_motion=1): left drag -> button=1
		 *   34 (base_no_motion=2): middle drag -> button=2
		 *   35 (base_no_motion=3): right drag -> button=3
		 * Plan section 3.6: \e[<32;10;5M = motion, button=0.
		 * This is what Terminal.app sends for cursor movement with no button. */
		int base_no_motion = base & ~32;
		is_motion = true;
		if (base_no_motion == 0)      button = 0;   /* pure motion, no button */
		else if (base_no_motion == 1) button = 1;   /* left drag */
		else if (base_no_motion == 2) button = 2;   /* middle drag */
		else if (base_no_motion == 3) button = 3;   /* right drag */
		else                          button = 0;   /* unknown -> no button */
	} else if (base == 64) {
		button = 4;   /* wheel up */
	} else if (base == 65) {
		button = 5;   /* wheel down */
	} else {
		/* Standard buttons: 0=left, 1=middle, 2=right -> 1=left, 2=middle, 3=right */
		button = (uint8_t)(base + 1);
	}

	/* Motion events don't have press/release semantics; always not-pressed. */
	bool ev_pressed = is_motion ? false : pressed;

	/* Convert 1-based terminal coords to 0-based. */
	emit_mouse(out, button, ev_pressed, col - 1, row - 1, mod, 0);

	/* Double-click synthesis: only for non-motion press events. */
	if (!is_motion) {
		mouse_maybe_double_click(dec, out);
	}

	return true;
}

/* 2026-06-29 JES #812 M4: paste-buffer helpers.
 *
 * paste_buf_append: append a single content byte to dec->paste_buf, growing
 *   the heap allocation geometrically (doubling) up to INPUT_DECODER_PASTE_CAP.
 *   Past the cap, byte is dropped and paste_truncated is latched (one
 *   BOXEN_LOG_W per paste).  Returns nothing -- success vs. truncation is
 *   reflected only in dec->paste_truncated; the caller continues parsing
 *   the rest of the paste regardless.
 *
 * paste_buf_reset: zero the paste accumulator fields without freeing.  Used
 *   after the BOXEN_EV_PASTE event takes ownership of the buffer (so we
 *   don't double-free).
 *
 * Growth strategy: 256 -> 512 -> 1024 -> ... -> 256 KiB.  Realloc on each
 * doubling; ~10 reallocs total to reach the cap.  Cheap; not the hot path.
 * The previous content survives realloc by definition.
 *
 * On allocation failure inside paste_buf_append we latch paste_truncated
 * and stop growing (we keep whatever bytes we already buffered).  This
 * matches the "truncate + warn" contract: a paste that hits ENOMEM
 * mid-flight still emits the partial event so the user sees something
 * rather than nothing. */
static void paste_buf_append(input_decoder_t *dec, uint8_t b) {
	/* 2026-06-29 JES #812 M4 gate-fix: once truncation is latched (cap
	 * reached OR realloc failed) we are committed to dropping further
	 * bytes for the rest of this paste -- don't re-enter the realloc
	 * branch on every subsequent byte (which under sustained OOM would
	 * call malloc thousands of times for nothing).  The early return
	 * also keeps the BOXEN_LOG_W call sites single-use per paste. */
	if (dec->paste_truncated) {
		return;
	}
	if (dec->paste_len >= INPUT_DECODER_PASTE_CAP) {
		dec->paste_truncated = true;
		BOXEN_LOG_W("bracketed paste exceeds %zu-byte cap; truncated",
		            (size_t)INPUT_DECODER_PASTE_CAP);
		return;
	}
	if (dec->paste_len >= dec->paste_cap) {
		size_t new_cap = dec->paste_cap == 0
		                 ? (size_t)INPUT_DECODER_PASTE_INITIAL
		                 : dec->paste_cap * 2;
		if (new_cap > (size_t)INPUT_DECODER_PASTE_CAP) {
			new_cap = (size_t)INPUT_DECODER_PASTE_CAP;
		}
		char *grown = (char *)realloc(dec->paste_buf, new_cap);
		if (grown == NULL) {
			/* OOM: keep what we have, latch truncation, stop appending.
			 * realloc preserves dec->paste_buf on failure, so the
			 * already-buffered bytes remain valid for the eventual emit. */
			dec->paste_truncated = true;
			BOXEN_LOG_W("bracketed paste realloc failed at %zu bytes; "
			            "truncated", dec->paste_len);
			return;
		}
		dec->paste_buf = grown;
		dec->paste_cap = new_cap;
	}
	dec->paste_buf[dec->paste_len++] = (char)b;
}

static void paste_buf_reset(input_decoder_t *dec) {
	dec->paste_buf         = NULL;
	dec->paste_len         = 0;
	dec->paste_cap         = 0;
	dec->paste_truncated   = false;
	dec->paste_close_match = 0;
}

/* 2026-06-29 JES #812 M4: close-marker bytes.
 *
 * The terminating sequence "\x1b[201~" (6 bytes).  Stored as a file-scope
 * constant so the streaming matcher and a future audit reader share one
 * source of truth. */
static const uint8_t PASTE_CLOSE_MARKER[INPUT_DECODER_PASTE_CLOSE_LEN] = {
	0x1B, '[', '2', '0', '1', '~'
};

/* 2026-06-30 JES #810 M2: dispatch a complete CSI sequence to a boxen_event.
 * 2026-06-29 JES #811 M3: added dec param for SGR mouse / double-click state;
 *   added 'M' (SGR press / X10 indicator) and 'm' (SGR release) final bytes.
 *
 * Returns true if an event was emitted, false if the sequence is unknown
 * (silently discarded -- never emitted as printable; plan section 3.12).
 *
 * The CSI dispatch table follows plan section 4.3.  M, m dispatch:
 *   - If csi_buf starts with '<': SGR mouse (decode_sgr_mouse).
 *   - If csi_buf is empty and final='M': X10 mouse introducer -- the caller
 *     sets dec->x10_pending=true so the poll loop reads 3 raw bytes next.
 *
 * For the ~ form, plan section 3.3 / 3.4 specify the first param identifies
 * the key (2=Ins, 3=Del, 5=PgUp, 6=PgDn, 11..24=function keys) and the
 * second param (when present) is the modifier.  For the letter form, the
 * standard convention is \e[1;NLetter where N is the modifier; the M2
 * tests reflect this. */
static bool decode_csi(input_decoder_t *dec, const uint8_t *csi_buf,
                       size_t csi_len, uint8_t final_byte, boxen_event_t *out) {
	/* 2026-06-29 JES #811 M3: SGR and X10 mouse routing.
	 *
	 * Must be checked BEFORE csi_parse_params because the '<' byte in
	 * csi_buf would fail the numeric-only parser.
	 *
	 * Case 1: SGR mouse (final='M' or final='m', csi_buf[0]=='<').
	 * Case 2: X10 mouse introducer (final='M', csi_buf empty).
	 *   Set x10_pending so the poll loop reads 3 raw bytes next.
	 *   Return false here (no event yet); the poll loop emits it. */
	if (final_byte == 'M' || final_byte == 'm') {
		if (csi_len > 0 && csi_buf[0] == '<') {
			/* SGR press or release. */
			bool pressed = (final_byte == 'M');
			return decode_sgr_mouse(dec, csi_buf, csi_len, pressed, out);
		}
		if (csi_len == 0 && final_byte == 'M') {
			/* X10 mouse: \e[M followed by 3 raw bytes.  Signal the poll
			 * loop to consume them on the next iteration. */
			dec->x10_pending = true;
			return false;   /* no event yet; poll loop handles payload */
		}
		/* 'M' with non-empty non-'<' prefix (unusual): discard. */
		return false;
	}

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
		case 200:
			/* 2026-06-29 JES #812 M4: bracketed paste opens.  Enter
			 * PASTE_ACTIVE; the poll loop's per-state branch buffers
			 * subsequent bytes into dec->paste_buf until the close
			 * marker (\x1b[201~) is seen.  Note: we do NOT emit an
			 * event here -- the open marker is silent; only the
			 * complete paste (on close) produces BOXEN_EV_PASTE.
			 * Return false (no event yet) so poll continues. */
			dec->state = DEC_STATE_PASTE_ACTIVE;
			paste_buf_reset(dec);
			return false;
		case 201:
			/* 2026-06-29 JES #812 M4: spurious close marker outside a
			 * paste body.  Ignore -- a stray \e[201~ from a buggy
			 * terminal or a mid-stream desync must not break the input
			 * stream.  Returning false flows into the poll loop's
			 * "no event" branch which bumps parse_errors. */
			return false;
		default: return false;
		}
	}
	case 'u': {
		/* 2026-06-29 JES #813 M5: Kitty keyboard protocol CSI-u decode.
		 *
		 * Form: \e[KEYCODE u             (no modifier)
		 *       \e[KEYCODE;MODIFIER u    (with modifier; 1=none, 2=Shift,
		 *                                 3=Alt, 5=Ctrl, ... same encoding
		 *                                 as the standard CSI cursor-key
		 *                                 modifier param -- see
		 *                                 modifier_from_param).
		 *
		 * Plan section 3.9: KEYCODE for printable keys is the Unicode
		 * code point of the character produced ('A' = 65, 'a' = 97, ...).
		 * Functional identifiers reuse the Unicode control-code values
		 * for keys that have one: 13 (CR) = Enter, 27 (ESC) = Escape,
		 * 9 (HT) = Tab, 127 (DEL) = Backspace.  This M5 milestone
		 * implements the two-part form; the three-part form with event
		 * type and alternate key is post-launch (plan section 3.9,
		 * "For M5 scope...").
		 *
		 * params[0] is required (no keycode => malformed; drop quietly
		 * via the false-return + caller's parse_errors bump).  params[1]
		 * may be absent -- modifier_from_param treats 0 / absent as
		 * BOXEN_MOD_NONE so the no-modifier path coalesces cleanly with
		 * the single-param form above. */
		if (count == 0) {
			return false;
		}
		uint16_t kitty_mod = (count >= 2) ? modifier_from_param(params[1])
		                                  : BOXEN_MOD_NONE;
		int keycode = params[0];

		/* Functional identifiers first.  Each maps to a dedicated
		 * BOXEN_KEY_* so downstream consumers can keymap them
		 * separately from the printable form.  Codes that aren't on
		 * this list (and aren't printable) fall through to the
		 * "unknown -- drop" branch at the bottom; this keeps the M5
		 * surface narrow.  A future post-launch milestone can extend
		 * the table with kitty's full functional set (arrows, F-keys,
		 * etc.) without changing the dispatch shape. */
		switch (keycode) {
		case 9:    /* Tab */
			emit_key(out, BOXEN_KEY_TAB, 0, kitty_mod);
			return true;
		case 13:   /* Enter */
			emit_key(out, BOXEN_KEY_ENTER, 0, kitty_mod);
			return true;
		case 27:   /* Escape */
			emit_key(out, BOXEN_KEY_ESCAPE, 0, kitty_mod);
			return true;
		case 127:  /* Backspace */
			emit_key(out, BOXEN_KEY_BACKSPACE, 0, kitty_mod);
			return true;
		default:
			break;
		}

		/* Printable keycode.  Accept any Unicode scalar value in the
		 * BMP-and-supplementary range, excluding the C0 / C1 control
		 * ranges that didn't match a functional identifier above.
		 * Surrogates (U+D800..U+DFFF) and out-of-range codepoints
		 * (> U+10FFFF) are rejected -- the same UTF-8 validation
		 * boundary the GROUND-state decoder applies, so the input
		 * surface stays consistent regardless of which protocol path
		 * the keystroke arrived on. */
		if (keycode <= 0 || keycode > 0x10FFFF) {
			return false;
		}
		if (keycode >= 0xD800 && keycode <= 0xDFFF) {
			return false;
		}
		if (keycode < 0x20 || (keycode >= 0x7F && keycode < 0xA0)) {
			/* Unmapped C0 (other than the functional codes handled
			 * above) and C1 control range.  Reject -- emitting these
			 * as printable ch would let a terminal smuggle raw
			 * control bytes through the CSI-u path. */
			return false;
		}
		emit_key(out, BOXEN_KEY_NONE, (uint32_t)keycode, kitty_mod);
		return true;
	}
	default:
		/* Unknown finals land here.  M / m (mouse) are handled above
		 * before reaching this switch; u (Kitty) is handled by its own
		 * case immediately above.  Silently discarded -- never emit as
		 * printable. */
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
	/* Anything else (0x1B is handled in poll before we reach here;
	 * high-bit bytes 0x80+ are also dropped in poll's GROUND branch
	 * before reaching us; only 0x1D / 0x1E / 0x1F group separator
	 * controls remain).  Conservative: surfaces unexpected bytes as
	 * raw ch rather than silently dropping. */
	emit_key(out, BOXEN_KEY_NONE, (uint32_t)b, BOXEN_MOD_NONE);
}

/* 2026-06-30 JES #810 M2: UTF-8 lead-byte classification.
 *
 * Returns the number of continuation bytes the lead expects (1..3) or 0
 * if the byte is not a valid UTF-8 lead (i.e., it's ASCII or invalid).
 * The lead byte's payload bits are written to *out_codepoint -- the
 * partial codepoint that subsequent continuation bytes shift in.
 *
 * 2026-06-30 JES #810 M2 review-followup: reject overlong-prone 2-byte
 * leads 0xC0 and 0xC1 (they can only encode U+0000..U+007F, which must
 * arrive as ASCII).  Also reject 0xF5..0xFF leads, which can only encode
 * codepoints > U+10FFFF (above the Unicode maximum).  These early
 * rejections close the most common UTF-8 smuggling vectors (overlong NUL
 * 0xC0 0x80 etc.) without waiting for the assembled-codepoint validator
 * to catch them post-assembly. */
static int utf8_lead_classify(uint8_t b, uint32_t *out_codepoint) {
	if ((b & 0xE0) == 0xC0) {            /* 110xxxxx */
		if (b < 0xC2) {
			/* 0xC0 / 0xC1 -- only used for overlong ASCII. */
			return 0;
		}
		*out_codepoint = b & 0x1F;
		return 1;
	}
	if ((b & 0xF0) == 0xE0) {            /* 1110xxxx */
		*out_codepoint = b & 0x0F;
		return 2;
	}
	if ((b & 0xF8) == 0xF0) {            /* 11110xxx */
		if (b > 0xF4) {
			/* 0xF5..0xF7 leads encode codepoints > U+10FFFF. */
			return 0;
		}
		*out_codepoint = b & 0x07;
		return 3;
	}
	return 0;
}

/* 2026-06-30 JES #810 M2 review-followup: assembled-codepoint validator.
 *
 * Belt-and-suspenders to the lead-byte rejection above.  Catches:
 *   - Surrogates (U+D800..U+DFFF) -- invalid in UTF-8 by RFC 3629.
 *   - Above-max codepoints (> U+10FFFF) -- only reachable via the
 *     borderline 4-byte lead 0xF4 with high continuation bytes
 *     (0xF4 0x90 0x80 0x80 = U+110000).
 *   - Overlong encodings -- codepoint smaller than the minimum the byte
 *     length can encode (lead_len is the number of CONTINUATION bytes,
 *     so 1=2-byte sequence, 2=3-byte, 3=4-byte; the minimum codepoints
 *     are 0x80, 0x800, 0x10000 respectively).
 *
 * Returns true if the codepoint is a valid Unicode scalar value for its
 * UTF-8 byte length, false otherwise.  Callers treat false as "emit
 * U+FFFD replacement character and bump parse_errors". */
static bool utf8_codepoint_valid(uint32_t cp, int lead_len) {
	if (cp >= 0xD800 && cp <= 0xDFFF) {
		return false;
	}
	if (cp > 0x10FFFF) {
		return false;
	}
	if (lead_len == 1 && cp < 0x80) {
		return false;
	}
	if (lead_len == 2 && cp < 0x800) {
		return false;
	}
	if (lead_len == 3 && cp < 0x10000) {
		return false;
	}
	return true;
}

/* -------------------------------------------------------------------------
 * 2026-06-29 JES #805 M6: TTY fill helpers.
 *
 * The M2 poll body consumed only bytes already in the ring buffer (deposited
 * by inject_bytes in the test seam).  M6 wires the real read(2) path: when
 * tty_fd >= 0 and the parser exhausts the buffer without emitting an event,
 * we block (via select(2)) up to timeout_ms for more bytes, drain the pipe
 * with one or more non-blocking reads, then re-run the parser.
 *
 * Plan reference: section 4.4 (ESC disambiguation strategy).  The contract:
 *   - Initial wait uses select(2) with timeout_ms.
 *   - When the first read arrives, do a non-blocking follow-up read to drain
 *     the pipe.  This collapses a multi-byte escape sequence that arrived
 *     as one OS burst into a single parse cycle, and gives sub-millisecond
 *     ESC-alone disambiguation (no extra delay when ESC really is alone).
 *   - Bytes that don't fit are counted in dropped_bytes (same back-pressure
 *     contract as inject_bytes overflow).
 *
 * Threading: input_decoder_poll runs with the Frontier GIL NOT held.
 * The REPL event loop (boxen_repl.c:2147-2152) releases the GIL via
 * `pthread_mutex_unlock(&frontier_gil)` BEFORE calling boxen_poll_event
 * (which forwards through tb2_poll_event into this function), and
 * reacquires it after.  That's by design -- it lets other Frontier
 * threads run while the REPL is parked in select(2).  The single-owner
 * invariant from input_decoder.h still holds because the REPL thread
 * is the only caller; other GIL-acquiring threads run UserTalk code
 * that never touches *dec.  No locks or atomics needed.
 * ---------------------------------------------------------------------- */

/* Read whatever bytes are immediately available on tty_fd without blocking.
 * Uses a select(2) with zero timeout to probe readability, then read(2) once
 * to drain.  Returns the count of bytes deposited into the ring buffer (may
 * be 0 if nothing is pending or the read got EAGAIN).  Returns SIZE_MAX on
 * a hard read error (closed fd or unexpected errno) -- caller surfaces as
 * BOXEN_ERR_IO.  EINTR is treated as "no data this time" (caller may retry).
 *
 * The read is capped at the ring's remaining space and lands directly in
 * the ring (compact_buffer has just run, so the tail is contiguous) --
 * we never read more bytes than we can store, the kernel buffers the rest
 * for the next call, and no staging copy is needed.  Slightly suboptimal
 * in pathological bursts (a paste of 8 KB arrives in two select rounds
 * instead of one) but preserves the back-pressure invariant.  Unlike the
 * inject_bytes seam there is no dropped_bytes path here: the cap means an
 * oversized burst is deferred, not dropped. */
static size_t dec_read_nonblocking(input_decoder_t *dec) {
	compact_buffer(dec);
	size_t avail = INPUT_DECODER_BUFFER_SIZE - dec->fill_len;
	if (avail == 0) {
		/* Buffer full; do not read -- caller must drain via the parser
		 * before more bytes can arrive.  Returning 0 lets the parser
		 * consume what's already buffered on the next iteration. */
		return 0;
	}

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(dec->tty_fd, &rfds);
	struct timeval tv = {0, 0};   /* non-blocking probe */
	int r = select(dec->tty_fd + 1, &rfds, NULL, NULL, &tv);
	if (r < 0) {
		if (errno == EINTR) {
			return 0;
		}
		return SIZE_MAX;
	}
	if (r == 0) {
		return 0;
	}

	ssize_t got = read(dec->tty_fd, dec->buf + dec->fill_len, avail);
	if (got < 0) {
		if (errno == EINTR || errno == EAGAIN) {
			return 0;
		}
		return SIZE_MAX;
	}
	if (got == 0) {
		/* EOF on the TTY -- closed or detached.  Treat as IO error so
		 * the caller can surface and shut down rather than spin. */
		return SIZE_MAX;
	}
	dec->fill_len += (size_t)got;
	return (size_t)got;
}

/* Block until at least one byte is readable on tty_fd or timeout_ms elapses.
 * Returns 1 if readable, 0 on timeout, -1 on hard error.  EINTR is treated
 * as "retry" -- the loop re-enters select with the remaining time deducted.
 * timeout_ms of -1 blocks indefinitely; 0 returns immediately. */
static int dec_select_readable(int fd, int timeout_ms) {
	struct timespec start;
	if (timeout_ms > 0) {
		clock_gettime(CLOCK_MONOTONIC, &start);
	}

	for (;;) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);

		struct timeval tv;
		struct timeval *ptv;
		int remaining_ms = timeout_ms;
		if (timeout_ms < 0) {
			ptv = NULL;
		} else {
			tv.tv_sec  = remaining_ms / 1000;
			tv.tv_usec = (remaining_ms % 1000) * 1000;
			ptv = &tv;
		}

		int r = select(fd + 1, &rfds, NULL, NULL, ptv);
		if (r > 0) {
			return 1;
		}
		if (r == 0) {
			return 0;
		}
		/* r < 0 */
		if (errno != EINTR) {
			return -1;
		}
		/* EINTR: recompute remaining timeout and retry. */
		if (timeout_ms > 0) {
			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);
			long elapsed_ms = (long)(now.tv_sec - start.tv_sec) * 1000
			                + (long)(now.tv_nsec - start.tv_nsec) / 1000000;
			if (elapsed_ms >= timeout_ms) {
				return 0;
			}
			timeout_ms -= (int)elapsed_ms;
			clock_gettime(CLOCK_MONOTONIC, &start);
		}
		/* timeout_ms == 0 was handled by initial select returning 0;
		 * timeout_ms < 0 just loops back into another blocking wait. */
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-30 JES #810 M2: drain helper -- run the state machine against
 *   buffered bytes until one event emits or we run out of bytes.
 * 2026-06-29 JES #805 M6: split from input_decoder_poll so the orchestrator
 *   can call us, then fill from TTY, then call us again.
 *
 * Returns BOXEN_OK + writes *out on a successful emit.
 * Returns BOXEN_ERR_TIMEOUT (with *out zeroed) when no event can be
 *   produced from the currently-buffered bytes (empty buffer OR partial
 *   sequence still waiting for more bytes).
 *
 * Pre-condition: dec and out are non-NULL.  Caller is input_decoder_poll
 * (which validates) -- not called directly from outside this file.
 * ---------------------------------------------------------------------- */
static int dec_drain_buffer(input_decoder_t *dec, boxen_event_t *out) {
	while (dec->head < dec->fill_len) {
		/* 2026-06-29 JES #812 M4: PASTE_ACTIVE branch.
		 *
		 * Inside a paste, EVERY byte is literal content -- ESC, [, ~,
		 * UTF-8 leads, anything.  We stream-match the close marker
		 * (\x1b[201~) byte-by-byte against the incoming byte.  Bytes
		 * that EXTEND the partial close match are buffered ONLY when
		 * the match completes (close detected) or aborts (mismatch ->
		 * those bytes are part of the paste body and get flushed to
		 * paste_buf).
		 *
		 * Independent close-match tracking is load-bearing: once the
		 * paste exceeds INPUT_DECODER_PASTE_CAP we stop appending to
		 * paste_buf but MUST keep scanning for the close marker --
		 * otherwise a paste bigger than the cap wedges the decoder in
		 * PASTE_ACTIVE forever.  See paste_close_match doc on the
		 * struct.
		 *
		 * Burst safety: this branch consumes one byte per iteration and
		 * returns to the top of the while loop on every byte, so a
		 * megabyte-sized paste burst polls forward one byte at a time.
		 * That's fine for the M2/M3 byte-at-a-time event cadence; the
		 * full paste emits as one event regardless. */
		if (dec->state == DEC_STATE_PASTE_ACTIVE) {
			uint8_t b = dec->buf[dec->head];
			dec->head++;

			if (b == PASTE_CLOSE_MARKER[dec->paste_close_match]) {
				/* Byte extends the partial close match.  Hold off on
				 * buffering -- if the full marker arrives we trim it
				 * out of the paste body. */
				dec->paste_close_match++;
				if (dec->paste_close_match <
				    (uint8_t)INPUT_DECODER_PASTE_CLOSE_LEN) {
					continue;
				}
				/* Full close match -- fall through to emit. */
			} else {
				/* Mismatch.  Flush the previously matched bytes to
				 * paste_buf (they were literal content after all),
				 * then handle the current byte.
				 *
				 * Edge case: the current byte may itself be the start
				 * of a NEW partial close match (e.g., paste body
				 * contains "\x1b[201X\x1b[201~" -- the X aborts the
				 * first match attempt, then a new ESC starts a fresh
				 * one).  Handle this by re-checking the current byte
				 * against marker[0] after flushing the partial match. */
				for (uint8_t i = 0; i < dec->paste_close_match; i++) {
					paste_buf_append(dec, PASTE_CLOSE_MARKER[i]);
				}
				dec->paste_close_match = 0;

				if (b == PASTE_CLOSE_MARKER[0]) {
					dec->paste_close_match = 1;
				} else {
					paste_buf_append(dec, b);
				}
				continue;
			}

			/* Close detected.  paste_close_match == close_len; the
			 * matched bytes were held back so paste_buf does NOT need
			 * trimming.  Just reset the matcher and emit. */
			dec->paste_close_match = 0;

			/* CR/LF normalization in place.
			 *
			 * Two-pointer scan: write_pos always <= read_pos so the
			 * compaction is safe.  Rules:
			 *   - "\r\n" collapses to "\n"
			 *   - lone "\r" becomes "\n"
			 *   - "\n" passes through unchanged
			 * Result: every line break is a single LF regardless of how
			 * the user's clipboard or source platform encoded it. */
			size_t w = 0;
			for (size_t r = 0; r < dec->paste_len; r++) {
				char c = dec->paste_buf[r];
				if (c == '\r') {
					dec->paste_buf[w++] = '\n';
					/* If the next byte is LF, skip it (CRLF -> LF). */
					if (r + 1 < dec->paste_len &&
					    dec->paste_buf[r + 1] == '\n') {
						r++;
					}
				} else {
					dec->paste_buf[w++] = c;
				}
			}
			dec->paste_len = w;

			/* Emit BOXEN_EV_PASTE.  Ownership of paste_buf transfers to
			 * the caller (ev.paste.data); decoder zeroes its own
			 * pointer so destroy doesn't double-free.
			 *
			 * Empty paste: data may be a real allocation of paste_cap
			 * bytes with len==0, or NULL (no append ever happened).
			 * Either is acceptable per the boxen.h contract; we hand
			 * back whatever we have and the consumer's free() handles
			 * both cases. */
			memset(out, 0, sizeof(*out));
			out->type       = BOXEN_EV_PASTE;
			out->paste.data = dec->paste_buf;
			out->paste.len  = dec->paste_len;

			paste_buf_reset(dec);
			dec->state = DEC_STATE_GROUND;
			compact_buffer(dec);
			return BOXEN_OK;
		}

		/* 2026-06-29 JES #811 M3: X10 mouse raw-payload consumer.
		 *
		 * After \e[M is seen (csi_buf empty, final='M'), decode_csi sets
		 * x10_pending.  We need 3 raw bytes: button-raw, x-raw, y-raw (each
		 * offset by 32).  This check runs BEFORE the state switch so the
		 * normal GROUND handler doesn't consume the payload bytes.
		 *
		 * Wait until all 3 bytes are available in the buffer; if not, return
		 * TIMEOUT and resume on the next poll call (partial-sequence safety). */
		if (dec->x10_pending) {
			if (dec->fill_len - dec->head < 3) {
				/* Not enough bytes yet; keep them in the buffer. */
				compact_buffer(dec);
				return BOXEN_ERR_TIMEOUT;
			}
			uint8_t btn_raw = dec->buf[dec->head];
			uint8_t x_raw   = dec->buf[dec->head + 1];
			uint8_t y_raw   = dec->buf[dec->head + 2];
			dec->head += 3;
			dec->x10_pending = false;

			/* 2026-06-29 JES #811 M3 gate-fix: per bar-raiser PR #818 review,
			 * validate the X10 payload before decoding.  The protocol offsets
			 * button by 32 and coords by 33 (32 offset + 1 for 1-based); any
			 * byte below those minimums is malformed and would produce a
			 * negative coordinate or button.  A garbage trailer (e.g. three
			 * NULs after a stray "\e[M") MUST NOT silently emit an event
			 * with bogus coords or pollute last_press for future double-click
			 * matching.  Bump parse_errors so tests can observe the rejection
			 * and continue parsing rather than returning a synthetic event. */
			if (btn_raw < 32 || x_raw < 33 || y_raw < 33) {
				dec->parse_errors++;
				compact_buffer(dec);
				continue;
			}

			/* Decode: subtract 32 for button and coords; coords also -1 for
			 * 0-based (terminal sends 1-based after the 32 offset).
			 * plan section 3.7: button_raw-32, x_raw-32-1, y_raw-32-1. */
			int raw_button = (int)(btn_raw) - 32;
			int x = (int)(x_raw) - 32 - 1;
			int y = (int)(y_raw) - 32 - 1;

			/* Extract modifier bits (same encoding as SGR;
			 * see SGR_MOUSE_MOD_MASK). */
			uint16_t mod = 0;
			if (raw_button & 4)  mod |= BOXEN_MOD_SHIFT;
			if (raw_button & 8)  mod |= BOXEN_MOD_META;
			if (raw_button & 16) mod |= BOXEN_MOD_CTRL;

			int base = (int)((unsigned)raw_button & ~SGR_MOUSE_MOD_MASK);
			uint8_t button;
			if (base == 64)      button = 4;           /* wheel up */
			else if (base == 65) button = 5;           /* wheel down */
			else                 button = (uint8_t)(base + 1); /* 0->1, 1->2, 2->3 */

			emit_mouse(out, button, /*pressed=*/true, x, y, mod, 0);
			mouse_maybe_double_click(dec, out);
			compact_buffer(dec);
			return BOXEN_OK;
		}

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
					dec->utf8_lead = b;
					dec->state = DEC_STATE_UTF8_CONT;
					continue;
				}
			}
			/* 2026-06-30 JES #810 M2 review-followup: any high-bit byte
			 * (>= 0x80) that wasn't classified as a valid UTF-8 lead is
			 * silently dropped.  This covers:
			 *   - 0x80..0xBF (stray continuation bytes)
			 *   - 0xC0..0xC1 (overlong-only leads, rejected above)
			 *   - 0xF5..0xF7 (encode codepoints > U+10FFFF, rejected above)
			 *   - 0xF8..0xFF (invalid 5+ byte leads)
			 * Emitting any of these as raw ch=byte would let Latin-1-
			 * looking garbage and the classic UTF-8 smuggling primitives
			 * (overlong NUL, overlong slash) flow downstream.  Bump the
			 * parse-error counter so tests can observe the rejection. */
			if (b >= 0x80) {
				dec->head++;
				dec->parse_errors++;
				continue;
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
			 * parse error -> transition to CSI_SWALLOW so the rest of the
			 * sequence (including any trailing param bytes that would
			 * otherwise be processed in GROUND as printable keystrokes)
			 * is drained quietly. */
			if (b >= 0x20 && b <= 0x3F) {
				if (dec->csi_len >= INPUT_DECODER_CSI_BUF_SIZE) {
					/* 2026-06-30 JES #810 M2 review-followup: overflow
					 * goes to CSI_SWALLOW, not GROUND.  Resetting to
					 * GROUND let post-overflow parameter bytes
					 * (typically digits and ';') be emitted as
					 * printable keystrokes -- a malicious terminal could
					 * inject ~65+ synthetic chars via one long CSI.
					 * SWALLOW drops bytes until a final byte arrives. */
					dec->head++;
					dec->state = DEC_STATE_CSI_SWALLOW;
					dec->csi_len = 0;
					dec->parse_errors++;
					continue;
				}
				dec->csi_buf[dec->csi_len++] = b;
				dec->head++;
				continue;
			}
			if (b >= 0x40 && b <= 0x7E) {
				/* Final byte: decode.
				 *
				 * 2026-06-29 JES #812 M4: decode_csi may mutate dec->state
				 * (entering DEC_STATE_PASTE_ACTIVE on \e[200~).  Therefore
				 * the post-decode state reset to GROUND must be conditional:
				 * leave the state alone if decode_csi transitioned us to a
				 * non-GROUND target.  Without this check the paste-open
				 * transition would be silently reverted and the paste body
				 * bytes would re-enter the GROUND state machine instead of
				 * being buffered.  csi_len reset is always safe -- the
				 * paste buffer is a different field. */
				dec->head++;
				bool emitted = decode_csi(dec, dec->csi_buf, dec->csi_len, b, out);
				dec->csi_len = 0;
				if (dec->state == DEC_STATE_CSI_COLLECTING) {
					dec->state = DEC_STATE_GROUND;
				}
				if (emitted) {
					compact_buffer(dec);
					return BOXEN_OK;
				}
				/* 2026-06-29 JES #811 M3: if x10_pending was set by decode_csi,
				 * continue the loop so the 3 raw X10 bytes are consumed next.
				 * Don't count this as a parse error -- it's the X10 introducer. */
				if (dec->x10_pending) {
					continue;
				}
				/* 2026-06-29 JES #812 M4: if decode_csi entered
				 * PASTE_ACTIVE (saw \e[200~), continue the loop so the
				 * paste-active branch at the top consumes subsequent
				 * bytes.  Don't count as a parse error -- it's the
				 * paste open marker, by design. */
				if (dec->state == DEC_STATE_PASTE_ACTIVE) {
					continue;
				}
				/* Unknown final: discard silently, keep parsing. */
				dec->parse_errors++;
				continue;
			}
			/* Garbage in the middle of a CSI (byte outside the
			 * parameter / intermediate / final ranges): switch to
			 * SWALLOW so any remaining param-like bytes are drained
			 * cleanly rather than reinterpreted as input. */
			dec->head++;
			dec->state = DEC_STATE_CSI_SWALLOW;
			dec->csi_len = 0;
			dec->parse_errors++;
			continue;
		}

		case DEC_STATE_CSI_SWALLOW:
			/* 2026-06-30 JES #810 M2 review-followup: drain the tail of
			 * a malformed / oversized CSI.  Consume bytes silently until
			 * the final byte (0x40..0x7E) is seen, then return to
			 * GROUND.  Non-final / non-param bytes are also tolerated
			 * here -- the goal is to NEVER emit while swallowing.  A
			 * sequence that never terminates lives in this state until
			 * a final byte or until the buffer empties (in which case
			 * the next inject continues swallowing). */
			dec->head++;
			if (b >= 0x40 && b <= 0x7E) {
				dec->state = DEC_STATE_GROUND;
			}
			continue;

		case DEC_STATE_SS3_RECEIVED: {
			dec->head++;
			bool emitted = decode_ss3(b, out);
			dec->state = DEC_STATE_GROUND;
			if (emitted) {
				compact_buffer(dec);
				return BOXEN_OK;
			}
			dec->parse_errors++;
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
				dec->parse_errors++;
				continue;
			}
			/* Valid continuation byte: shift in the 6 payload bits. */
			dec->head++;
			dec->utf8_codepoint = (dec->utf8_codepoint << 6) | (b & 0x3F);
			dec->utf8_remaining--;
			if (dec->utf8_remaining == 0) {
				/* 2026-06-30 JES #810 M2 review-followup: validate the
				 * assembled codepoint before emit.  Rejects surrogates,
				 * codepoints above U+10FFFF, and overlong encodings
				 * (the canonical UTF-8 smuggling vectors -- overlong
				 * NUL, overlong slash, etc.).  Per the Unicode standard
				 * recommendation, emit U+FFFD REPLACEMENT CHARACTER so
				 * downstream consumers see an unambiguous "this was
				 * invalid input" marker rather than a silently dropped
				 * keystroke or a smuggled control code. */
				int lead_len = (int)(dec->utf8_lead >= 0xF0 ? 3 :
				                     dec->utf8_lead >= 0xE0 ? 2 : 1);
				uint32_t cp = dec->utf8_codepoint;
				if (!utf8_codepoint_valid(cp, lead_len)) {
					cp = 0xFFFD;
					dec->parse_errors++;
				}
				emit_key(out, BOXEN_KEY_NONE, cp, BOXEN_MOD_NONE);
				dec->state = DEC_STATE_GROUND;
				dec->utf8_codepoint = 0;
				dec->utf8_lead = 0;
				compact_buffer(dec);
				return BOXEN_OK;
			}
			/* Still need more continuation bytes. */
			continue;
		}

		case DEC_STATE_PASTE_ACTIVE:
			/* 2026-06-29 JES #812 M4: handled by the early-return branch
			 * at the top of the while loop.  Unreachable here; assert
			 * the invariant so a future refactor that drops the early
			 * branch fails loudly instead of silently mis-parsing paste
			 * bytes through the GROUND state machine. */
			assert(false && "PASTE_ACTIVE handled before state switch");
			return BOXEN_ERR_IO;
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
 * 2026-06-29 JES #805 M6: public poll orchestrator.
 *
 * Contract (extends the M2 contract):
 *   - Always try to emit from the existing buffer first (handles a burst
 *     where the prior poll left additional complete events behind).
 *   - If the buffer is exhausted (or only a partial sequence remains) AND
 *     tty_fd >= 0, block on select(2) for up to timeout_ms.  When data
 *     arrives, drain the pipe with non-blocking reads (collapse a multi-
 *     byte escape sequence that arrived in one OS burst into one parse
 *     cycle).  Then re-run the parser.
 *   - If tty_fd == -1 (test seam): no read attempted; behavior matches the
 *     M2 contract -- caller drives bytes via input_decoder_inject_bytes
 *     before each poll.
 *   - timeout_ms semantics: 0 = non-blocking probe; positive = block up to
 *     that many milliseconds; negative = block indefinitely.  The tb2
 *     backend passes timeout_ms straight through from boxen_poll_event.
 *
 * Returns BOXEN_OK + writes *out on a successful emit.
 * Returns BOXEN_ERR_TIMEOUT (with *out zeroed) when no event materialized
 *   within the timeout (empty buffer, no TTY data, or partial sequence
 *   awaiting more bytes).
 * Returns BOXEN_ERR_IO on a hard read(2) error or EOF on tty_fd.
 * Returns BOXEN_ERR_INVALID for NULL args.
 *
 * Plan reference: section 2.3 (integration), section 4.4 (ESC disambig),
 *   section 3.12 (burst-read robustness).
 * ---------------------------------------------------------------------- */
int input_decoder_poll(input_decoder_t *dec, boxen_event_t *out, int timeout_ms) {
	if (dec == NULL || out == NULL) {
		return BOXEN_ERR_INVALID;
	}
	memset(out, 0, sizeof(*out));

	/* First: drain anything already buffered.  This handles bursts where
	 * the prior poll consumed only one of several queued events, and the
	 * test-seam path where inject_bytes deposited bytes before the call. */
	int r = dec_drain_buffer(dec, out);
	if (r == BOXEN_OK) {
		return BOXEN_OK;
	}

	/* Test seam: no TTY fd, so nothing more to do -- caller will inject
	 * before the next poll.  Preserves M1..M5 test contract. */
	if (dec->tty_fd < 0) {
		return BOXEN_ERR_TIMEOUT;
	}

	/* Block (up to timeout_ms) for the first byte to arrive on the TTY.
	 * Special case: timeout_ms == 0 is a non-blocking probe; skip the
	 * blocking select and go straight to the non-blocking drain so we
	 * still pick up bytes already queued at the OS level. */
	if (timeout_ms != 0) {
		int sr = dec_select_readable(dec->tty_fd, timeout_ms);
		if (sr < 0) {
			return BOXEN_ERR_IO;
		}
		if (sr == 0) {
			memset(out, 0, sizeof(*out));
			return BOXEN_ERR_TIMEOUT;
		}
	}

	/* Drain the pipe non-blocking: collapses a multi-byte escape sequence
	 * that arrived as one OS burst into one parse cycle, and gives sub-
	 * millisecond ESC-alone disambiguation (a single ESC byte returns no
	 * follow-up bytes from the second read, so the parser's stored
	 * ESC_RECEIVED state will be flushed as KEY_ESCAPE on the next poll
	 * via the existing flush-on-no-more-bytes path in dec_drain_buffer).
	 *
	 * Loop the non-blocking reads up to a small bound to soak up any
	 * pipe bytes that arrived between our select(2) and our first read.
	 * Bound prevents starvation -- after a few rounds we surrender and
	 * let the parser run so partial sequences in the buffer get a chance
	 * to complete on the next poll. */
	for (int i = 0; i < 4; i++) {
		size_t got = dec_read_nonblocking(dec);
		if (got == SIZE_MAX) {
			return BOXEN_ERR_IO;
		}
		if (got == 0) {
			break;
		}
	}

	/* Re-run the parser against the freshly populated buffer. */
	r = dec_drain_buffer(dec, out);
	if (r == BOXEN_OK) {
		return BOXEN_OK;
	}

	/* Nothing parsed yet (partial sequence in flight, or only a lone ESC
	 * sitting in ESC_RECEIVED waiting for disambiguation).  Caller will
	 * poll again; ESC_RECEIVED will flush on the next call if no more
	 * bytes arrive within the next timeout window. */
	memset(out, 0, sizeof(*out));
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

size_t input_decoder_parse_errors(const input_decoder_t *dec) {
	if (dec == NULL) {
		return 0;
	}
	return dec->parse_errors;
}

/* 2026-06-29 JES #811 M3 gate-fix: clock override for deterministic
 * double-click tests.  Pass enable=true plus a value to lock dec_now_ms()
 * to that value (subsequent calls return the same locked value until updated
 * with another call).  Pass enable=false to release the override and return
 * to wall-clock.  Per bar-raiser PR #818 review: previously the double-click
 * test relied on two presses being injected sub-millisecond apart so real
 * CLOCK_MONOTONIC elapsed < 500ms.  Under CI load or sanitizers this was
 * brittle; the override makes both the positive (within window) and negative
 * (past window) paths deterministic. */
void input_decoder_set_clock_for_testing(bool enable, uint64_t value_ms) {
	dec_clock_override_set = enable;
	dec_clock_override_ms  = value_ms;
}

#endif /* INPUT_DECODER_TEST_SEAM */
