/*
 * input_decoder.h -- private terminal input decoder for the boxen tb2 backend.
 *
 * Owns: TTY fd read loop, byte buffering, escape-sequence state machine,
 * Unicode (UTF-8) decoding, mouse-mode enable/disable.
 *
 * Does NOT own: rendering, alternate screen, SIGWINCH, cell buffer,
 * color, cursor positioning. termbox2 retains all of those.
 *
 * Threading: same contract as the rest of boxen -- single-threaded, GIL-held.
 * The caller (tb2_poll_event) releases the GIL before entering the read loop
 * and reacquires before calling back into Frontier.
 *
 * This header is PRIVATE to the boxen subsystem (sibling to boxen_internal.h).
 * Only boxen.h is the public boxen header; do NOT include input_decoder.h
 * from outside frontier-cli/boxen/.
 *
 * 2026-06-29 JES #809 M1: PTY-replay harness + decoder stub.
 * See planning/phase_c/INPUT_DECODER_PLAN.md sections 2 and 7.
 */

#ifndef INPUT_DECODER_H
#define INPUT_DECODER_H

#include "boxen.h"   /* for boxen_event_t, boxen_result_t */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque decoder state. Allocated by input_decoder_create(). */
typedef struct input_decoder input_decoder_t;

/* 2026-06-29 JES #809 M1: lifecycle.
 *
 * Create a decoder attached to the given file descriptor (usually the TTY fd
 * obtained from tb_get_fds() or /dev/tty). A tty_fd of -1 is permitted and
 * is the convention used by the PTY-replay test harness -- the decoder will
 * never call read(2) in that case; bytes are supplied exclusively through
 * input_decoder_inject_bytes().
 *
 * Returns NULL on allocation failure (ENOMEM).  Mouse reporting starts
 * DISABLED; call input_decoder_set_mouse(true) to enable SGR + bracketed
 * paste (M3 / M4 scope -- the M1 stub records the bit but writes nothing
 * to the fd). */
input_decoder_t *input_decoder_create(int tty_fd);

/* Free all resources. Does NOT close tty_fd. Safe to pass NULL. */
void             input_decoder_destroy(input_decoder_t *dec);

/* 2026-06-29 JES #809 M1: mouse-mode policy seam.
 *
 * Enable or disable SGR mouse reporting (mode 1006) and bracketed paste
 * (mode 2004).  When mouse is disabled the terminal's native selection is
 * active.  When mouse is enabled, selections require Shift-click
 * (Terminal.app) or the configured pass-through modifier (iTerm2 / Alacritty).
 *
 * M1 stub: records the requested state and returns; no write(2) to the TTY.
 * M3 / M4 will wire the actual escape-sequence writes once the parser side
 * is in place. */
void             input_decoder_set_mouse(input_decoder_t *dec, bool enable);

/* Query current mouse-enable state. Safe to call on a freshly-created
 * decoder (returns false). */
bool             input_decoder_mouse_enabled(const input_decoder_t *dec);

/* 2026-06-29 JES #809 M1: poll contract.
 *
 * Block until one event is decoded or timeout_ms elapses.
 *   On success: writes *out and returns BOXEN_OK.
 *   On timeout: returns BOXEN_ERR_TIMEOUT; *out is zeroed.
 *   On read error: returns BOXEN_ERR_IO.
 *
 * M1 stub: always returns BOXEN_ERR_TIMEOUT with *out zeroed. The state
 * machine that consumes the ring buffer lands in M2.  The buffer itself is
 * already wired so M2 tests can split sequences across multiple inject_bytes
 * calls (see section 3.12 of the plan: burst-read robustness). */
int  input_decoder_poll(input_decoder_t *dec, boxen_event_t *out, int timeout_ms);

/* 2026-06-29 JES #809 M1: Kitty keyboard protocol enable (deferred to M5).
 *
 * Sends the enable sequence (\e[=1u) and records that progressive
 * enhancement was requested.  Responses arrive via normal
 * input_decoder_poll.  Idempotent.  No-op if already enabled or if the
 * terminal does not respond to the probe.
 *
 * M1 stub: records the request but emits nothing.  M5 wires the write
 * and decodes CSI-u replies. */
void input_decoder_kitty_enable(input_decoder_t *dec);

/* 2026-06-29 JES #809 M1: test seam.
 *
 * Inject raw bytes into the decoder's ring buffer.  Compile-guarded by
 * INPUT_DECODER_TEST_SEAM so the symbol is invisible to production builds
 * (and any well-meaning future caller cannot accidentally smuggle bytes
 * past the TTY read loop).  Normal production code never calls this.
 *
 * On overflow, the excess bytes are dropped silently for now -- M2 will
 * formalize the back-pressure contract.  The M1 buffer is sized for the
 * largest single protocol burst we expect (worst case: bracketed paste,
 * which M4 caps at 256 KB but for the M1 stub a much smaller value is
 * adequate to exercise the state machine that M2 lands).
 *
 * Safe to call with len == 0 (no-op) or bytes == NULL when len == 0. */
#ifdef INPUT_DECODER_TEST_SEAM
void input_decoder_inject_bytes(input_decoder_t *dec,
                                const uint8_t *bytes, size_t len);

/* Test-only accessor: number of bytes currently held in the ring buffer.
 * Returns 0 for a NULL decoder.  Used by M1 harness tests to assert the
 * inject path actually deposited bytes (the M1 poll stub does not drain
 * them, so the count after inject equals the number injected up to buffer
 * capacity). */
size_t input_decoder_buffered_bytes(const input_decoder_t *dec);
#endif

#ifdef __cplusplus
}
#endif

#endif /* INPUT_DECODER_H */
