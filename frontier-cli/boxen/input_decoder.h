/*
 * input_decoder.h -- private terminal input decoder for the boxen tb2 backend.
 *
 * Owns: TTY fd read loop, byte buffering, escape-sequence state machine,
 * Unicode (UTF-8) decoding, mouse-mode enable/disable.
 *
 * Does NOT own: rendering, alternate screen, SIGWINCH, cell buffer,
 * color, cursor positioning. termbox2 retains all of those.  SIGWINCH
 * does not touch any decoder field; termbox2 handles screen resize via
 * its own signal pipe, separate from the decoder's tty_fd, so no
 * async-signal coordination is needed here.
 *
 * Threading contract -- READ THIS BEFORE EDITING:
 *
 *   Single-owner invariant: there is exactly one input_decoder_t per
 *   process (the tb2 backend's static g_decoder, post-M6).  No other
 *   Frontier thread holds a pointer to it.  Safety relies on that
 *   invariant -- the decoder has no internal locks and no atomics.
 *
 *   The owner thread is the GIL holder when entering and leaving any
 *   decoder API.  input_decoder_poll is the one function that releases
 *   the GIL around its blocking select(2) / read(2) (M6 wires this);
 *   the decoder's fields are mutated inside that GIL-dropped window.
 *   This is safe because the single-owner invariant means no other
 *   thread can call into the decoder during that window.
 *
 *   set_mouse / kitty_enable must be called only from GIL-held context
 *   and only when no poll is in progress -- i.e., from slash-command
 *   dispatch, init, or shutdown, NEVER from inside the GIL-drop window
 *   around boxen_poll_event.  Future writes to tty_fd from set_mouse
 *   (M3+) would race the in-flight read(2) otherwise.  The REPL event
 *   loop satisfies this naturally: slash commands run between polls,
 *   not during them.
 *
 *   Memory ordering: the GIL release/reacquire pair around poll
 *   provides the happens-before edge for any cross-call observation
 *   of decoder state (e.g., the REPL reading mouse_enabled after a
 *   prior set_mouse call).  No additional barriers are required as
 *   long as the single-owner invariant holds.
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
 * obtained from tb_get_fds() or /dev/tty).  The fd must be open READ-WRITE:
 * the decoder reads input bytes from it AND writes control sequences to it
 * (mouse-mode toggles, bracketed-paste enable, Kitty enable) -- and those
 * writes are deliberately (void)-discarded, so a read-only fd fails
 * silently.  A tty_fd of -1 is permitted and
 * is the convention used by the PTY-replay test harness -- the decoder will
 * never call read(2) in that case; bytes are supplied exclusively through
 * input_decoder_inject_bytes().
 *
 * Ownership: the decoder does NOT take ownership of tty_fd, does NOT close
 * it on destroy, and does NOT validate it at create time.  The caller is
 * responsible for the fd's lifecycle (tb2 init/shutdown in M6).  A garbage
 * non-negative integer will not be caught here; the future read(2) call
 * will surface it as a normal I/O error returned from input_decoder_poll.
 *
 * Returns NULL on allocation failure (ENOMEM).  Mouse reporting starts
 * DISABLED; call input_decoder_set_mouse(true) to enable SGR + bracketed
 * paste (M3 / M4 scope -- the M1 stub records the bit but writes nothing
 * to the fd). */
input_decoder_t *input_decoder_create(int tty_fd);

/* Free all resources. Does NOT close tty_fd. Safe to pass NULL.
 *
 * 2026-08-09 JES C M7: if mouse reporting is still enabled and tty_fd >= 0,
 * destroy writes the mouse/paste disable triplet first so the terminal's
 * native input model is restored at teardown (termbox2 cannot do this --
 * it never saw the decoder's mouse-mode writes).  Callers must therefore
 * destroy the decoder while the tty fd is still open and in raw mode
 * (tb2_shutdown destroys BEFORE tb_shutdown for exactly this reason). */
void             input_decoder_destroy(input_decoder_t *dec);

/* 2026-06-29 JES #809 M1: mouse-mode policy seam.
 *
 * Enable or disable SGR mouse reporting (mode 1006) and bracketed paste
 * (mode 2004).  When mouse is disabled the terminal's native selection is
 * active.  When mouse is enabled, selections require Shift-click
 * (Terminal.app) or the configured pass-through modifier (iTerm2 / Alacritty).
 *
 * M1 stub: records the requested state and returns; no write(2) to the TTY.
 * M3 wires the actual escape-sequence writes (\e[?1000h \e[?1006h \e[?2004h
 * and their disable counterparts) when tty_fd >= 0.  Test-seam paths still
 * use tty_fd == -1 and skip the writes.
 *
 * 2026-06-29 JES #811 M3 gate-fix (concurrency reviewer #818): the threading
 * contract above (lines 27-38) is now load-bearing.  set_mouse performs a
 * write(2) to tty_fd; if this races with a concurrent read(2) on the same
 * fd from inside the M6 poll loop, the writes can interleave and corrupt
 * either side.  CALLERS MUST NOT call set_mouse while another thread is
 * blocked in input_decoder_poll on the same decoder.  M6 will enforce this
 * with an explicit poll_active flag and an assert in set_mouse; for now the
 * single-owner invariant is the only safeguard. */
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

/* 2026-06-29 JES #809 M1: Kitty keyboard protocol enable.
 * 2026-06-29 JES #813 M5: write side wired -- sends "\x1b[=1u" to tty_fd
 *   when tty_fd >= 0.  Test seam (tty_fd == -1) still flips the bit only.
 *
 * Sends the enable sequence (\e[=1u: "disambiguate escape codes" flag) and
 * records that progressive enhancement was requested.  CSI-u replies
 * (\e[KEYCODE;MODIFIER u) arrive via normal input_decoder_poll and are
 * decoded by the same state machine that handles standard CSI sequences.
 * Idempotent -- a second call after the first is a no-op (the bit is
 * already set; we skip the duplicate write).
 *
 * Probe-and-read for the terminal's acknowledgement (\e[=FLAGS u) is
 * deferred to M6 where the read(2) loop lives.  Terminals that don't
 * support kitty (Terminal.app, iTerm2 as of writing) silently ignore the
 * enable; no CSI-u traffic arrives and the decoder operates as if
 * kitty_enable had never been called -- standard CSI parsing from M2
 * continues to handle their input. */
void input_decoder_kitty_enable(input_decoder_t *dec);

/* Query whether kitty_enable has been called on this decoder.  Symmetric
 * to input_decoder_mouse_enabled.  Safe on a freshly-created or NULL
 * decoder (returns false).  Reflects the request, not the terminal's
 * confirmed support -- M5 will not flip the bit back if the probe times
 * out, because the decoder remains a valid no-op in that case. */
bool             input_decoder_kitty_enabled(const input_decoder_t *dec);

/* 2026-06-29 JES #809 M1: test seam.
 *
 * Inject raw bytes into the decoder's ring buffer.  Compile-guarded by
 * INPUT_DECODER_TEST_SEAM so the symbol is invisible to production builds
 * (and any well-meaning future caller cannot accidentally smuggle bytes
 * past the TTY read loop).  Normal production code never calls this --
 * the real read(2) path lands in M2.
 *
 * Returns the number of bytes actually accepted into the buffer.  This
 * is normally equal to `len`, but on overflow it is clamped to the
 * available space and the remainder is counted in
 * input_decoder_dropped_bytes() (which the M2 / M3 read loop will also
 * increment when a real read(2) burst overflows the ring).  Tests that
 * inject > INPUT_DECODER_BUFFER_SIZE bytes can therefore detect the
 * drop without relying on stderr inspection.
 *
 * Safe to call with len == 0 (no-op, returns 0).  bytes == NULL with
 * len > 0 is a programmer error and triggers an assert -- a test seam
 * is exactly where you want such bugs to surface.
 *
 * The dropped_bytes counter and the inject return value together define
 * the M2 back-pressure contract: the read loop must either keep up (no
 * drops) or surface drops to logging / metrics so silent desync is
 * impossible.  See plan section 3.12 (burst-read robustness). */
#ifdef INPUT_DECODER_TEST_SEAM
size_t input_decoder_inject_bytes(input_decoder_t *dec,
                                  const uint8_t *bytes, size_t len);

/* Test-only accessor: number of bytes currently held in the ring buffer.
 * Returns 0 for a NULL decoder.  Used by M1 harness tests to assert the
 * inject path actually deposited bytes (the M1 poll stub does not drain
 * them, so the count after inject equals the number injected up to buffer
 * capacity). */
size_t input_decoder_buffered_bytes(const input_decoder_t *dec);

/* Test-only accessor: cumulative count of bytes dropped due to ring
 * overflow since decoder creation.  Returns 0 for a NULL decoder.  M2 /
 * M3 also bump this counter when a real read(2) cannot fit into the
 * remaining ring space.  Always-zero return is the M1 / M2 happy path;
 * non-zero indicates the test deliberately exceeded buffer capacity OR
 * (in production) a runaway terminal burst that the state machine
 * failed to drain. */
size_t input_decoder_dropped_bytes(const input_decoder_t *dec);

/* 2026-06-30 JES #810 M2 review-followup: cumulative parse-error count.
 *
 * Bumped on every silent rejection path inside the state machine:
 *   - CSI buffer overflow (sequence drained via CSI_SWALLOW state)
 *   - CSI parameter byte rejected as non-numeric
 *   - Unknown CSI final byte (legitimate sequence we have no mapping for)
 *   - Unknown SS3 final byte
 *   - Stray UTF-8 continuation byte in GROUND
 *   - Invalid 5+ byte UTF-8 lead in GROUND
 *   - Premature non-continuation byte mid-UTF-8
 *   - Invalid UTF-8 codepoint (surrogate, overlong, > U+10FFFF) -- the
 *     event still emits as U+FFFD, but the counter is bumped so the
 *     rejection is observable.
 *
 * The complement of dropped_bytes: dropped_bytes counts back-pressure
 * (ring full), parse_errors counts protocol rejection.  Together they
 * cover every "input arrived but no event was produced" path so a
 * desync between bytes-in and events-out is always attributable.
 *
 * Returns 0 for a NULL decoder.  Monotonic; never decreases. */
size_t input_decoder_parse_errors(const input_decoder_t *dec);

/* 2026-06-29 JES #811 M3 gate-fix: clock override for deterministic
 * double-click tests.  When enable=true, dec_now_ms() returns value_ms on
 * every subsequent call until either a new value is set or enable=false
 * releases the override.  Allows tests to assert both the positive (within
 * window) and negative (past window) double-click paths without depending
 * on wall-clock timing.  No-op effect on production paths (test-seam only). */
void input_decoder_set_clock_for_testing(bool enable, uint64_t value_ms);
#endif

#ifdef __cplusplus
}
#endif

#endif /* INPUT_DECODER_H */
