/*
 * backend_tb2.c -- termbox2 backend shim for boxen.
 *
 * This is the ONLY file in the boxen subsystem that may reference tb_* symbols
 * or include termbox2.h. All other boxen source files work through the
 * boxen_backend_t vtable and must never use termbox2 directly.
 *
 * Responsibilities (post-M6):
 *   - Bring up termbox2 for the rendering side (alternate screen, raw mode,
 *     SIGWINCH, cell buffer, color, cursor positioning).
 *   - Bring up the input_decoder for the read side -- it owns the TTY fd
 *     read loop, escape-sequence parsing, mouse-mode writes, bracketed
 *     paste, Kitty protocol enable, and double-click synthesis.  All key
 *     and mouse translation that used to live in this file is now inside
 *     the decoder; this file just forwards bytes between the two layers.
 *   - Translate BOXEN_ATTR_* -> termbox2 attribute bits (rendering-side).
 *   - Translate BOXEN_COLOR_* -> termbox2 color values (rendering-side).
 *
 * 2026-06-29 JES #805 M6: Cutover -- replaced tb_peek_event with
 *   input_decoder_poll.  Removed translate_key, translate_mod,
 *   translate_mouse, and the g_tb2_last_press double-click state struct --
 *   the input decoder subsumes all of that.  The rendering path
 *   (translate_attr / translate_color / tb_set_cell / tb_present / etc.)
 *   is untouched.  Mouse mode is NOT enabled in tb2_init; the decoder
 *   keeps it off by default per plan section 5.1, and the M7 /mouse
 *   slash command will toggle it.  See planning/phase_c/INPUT_DECODER_PLAN.md
 *   sections 2.3, 5, 7 (M6 spec), and 8 (R1, R3, R4).
 */

#include "termbox2.h"
#include "boxen_internal.h"
#include "input_decoder.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/* -------------------------------------------------------------------------
 * 2026-06-29 JES #805 M6: input decoder lifetime.
 *
 * Created in tb2_init after tb_init() (which sets up raw mode and the
 * alternate screen on the TTY).  Destroyed in tb2_shutdown BEFORE
 * tb_shutdown() so the decoder's writes (mouse-disable / paste-disable on
 * destroy paths in future milestones) land while the TTY is still in raw
 * mode and the fd is still valid.
 *
 * Single instance per process -- the boxen_backend_t vtable is global and
 * boxen_repl owns the only init/shutdown pair.  This matches the single-
 * owner threading invariant documented in input_decoder.h.
 * ---------------------------------------------------------------------- */
static input_decoder_t *g_decoder = NULL;

/* -------------------------------------------------------------------------
 * 2026-06-29 JES #805 M6 / risk R1: obtain TTY fd from termbox2 if possible,
 * otherwise open /dev/tty directly.
 *
 * The vendored termbox2 (frontier-cli/third_party/termbox2/termbox2.h) does
 * export tb_get_fds() -- verified at M6 implementation time.  If a future
 * vendored update drops or renames the symbol, the fallback opens /dev/tty
 * which is what termbox2 itself opens internally when no fd is passed to
 * tb_init.  Risk R1 disposition: LOW; both paths are well-trodden.
 *
 * Returns -1 on failure -- caller must surface as BOXEN_ERR_INIT so the
 * REPL can refuse to start cleanly instead of running with a wedged input
 * path.  fd_was_opened_out is set true when we opened /dev/tty ourselves
 * (caller closes on shutdown) and false when we borrowed termbox2's fd
 * (caller leaves alone -- tb_shutdown() closes it).
 * ---------------------------------------------------------------------- */
static int tb2_obtain_tty_fd(bool *fd_was_opened_out) {
	*fd_was_opened_out = false;

	int ttyfd = -1;
	int resizefd = -1;
	if (tb_get_fds(&ttyfd, &resizefd) == TB_OK && ttyfd >= 0) {
		return ttyfd;
	}

	/* Fallback: open /dev/tty directly.  O_RDWR, not O_RDONLY: the decoder
	 * WRITES control sequences to this fd (mouse-mode toggles in
	 * input_decoder_set_mouse, the Kitty enable in
	 * input_decoder_kitty_enable) and those writes are (void)-discarded,
	 * so a read-only fd would make them fail silently with EBADF.
	 * O_NONBLOCK is NOT set because the decoder uses select(2) + read(2)
	 * with explicit timeouts; a non-blocking fd would cause spurious
	 * EAGAIN returns inside the decoder's blocking-poll path. */
	int fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		return -1;
	}
	*fd_was_opened_out = true;
	return fd;
}

/* Track whether we own (and must close) the TTY fd.  False when borrowed
 * from termbox2 (tb_shutdown closes it); true when we opened /dev/tty. */
static bool g_tb2_owns_tty_fd = false;
static int  g_tb2_tty_fd      = -1;

/* -------------------------------------------------------------------------
 * Attribute translation: BOXEN_ATTR_* -> termbox2 uintattr_t bits
 *
 * Rendering-side translator -- NOT subsumed by the input decoder.  Kept
 * intact from pre-M6 backend_tb2.c.
 * ---------------------------------------------------------------------- */

static uintattr_t translate_attr(uint16_t boxen_attr) {
	uintattr_t ta = 0;
	if (boxen_attr & BOXEN_ATTR_BOLD)      ta |= TB_BOLD;
	if (boxen_attr & BOXEN_ATTR_UNDERLINE) ta |= TB_UNDERLINE;
	if (boxen_attr & BOXEN_ATTR_REVERSE)   ta |= TB_REVERSE;
	if (boxen_attr & BOXEN_ATTR_ITALIC)    ta |= TB_ITALIC;
	return ta;
}

/* -------------------------------------------------------------------------
 * Color translation: BOXEN_COLOR_* -> termbox2 uintattr_t color value
 *
 * Today BOXEN_COLOR_DEFAULT (0) and BOXEN_COLOR_BLACK..WHITE (1..8) happen to
 * have the same numeric values as TB_DEFAULT and TB_BLACK..TB_WHITE. This
 * function exists to make that coincidence explicit and local: if termbox2
 * ever renumbers its color constants, only this function needs to change.
 *
 * Rendering-side translator -- NOT subsumed by the input decoder.  Kept
 * intact from pre-M6 backend_tb2.c.
 * ---------------------------------------------------------------------- */

static uintattr_t translate_color(boxen_color_t c) {
	return (uintattr_t)c;
}

/* -------------------------------------------------------------------------
 * Backend vtable implementation
 * ---------------------------------------------------------------------- */

static int tb2_init(void *config) {
	(void)config;

	int r = tb_init();
	if (r != TB_OK) {
		return BOXEN_ERR_INIT;
	}

	/* 2026-06-29 JES #805 M6: mouse mode is NOT enabled here.  Per plan
	 * section 5.1, the decoder keeps mouse off by default so native
	 * terminal selection works on REPL startup.  The M7 /mouse slash
	 * command and palette-open auto-enable will flip it on demand via
	 * input_decoder_set_mouse(g_decoder, true).  Do NOT add a
	 * tb_set_input_mode(TB_INPUT_MOUSE) call here -- that was the PR
	 * #808 regression that broke selection for everyone. */

	bool opened_tty = false;
	int tty_fd = tb2_obtain_tty_fd(&opened_tty);
	if (tty_fd < 0) {
		tb_shutdown();
		return BOXEN_ERR_INIT;
	}

	g_decoder = input_decoder_create(tty_fd);
	if (g_decoder == NULL) {
		if (opened_tty) {
			close(tty_fd);
		}
		tb_shutdown();
		return BOXEN_ERR_INIT;
	}
	g_tb2_tty_fd      = tty_fd;
	g_tb2_owns_tty_fd = opened_tty;

	return BOXEN_OK;
}

/* Last cursor position passed to tb_set_cursor(). Used by
 * tb2_set_cursor_visible(true) to restore visibility after a hide_cursor() --
 * termbox2 has no symmetric show_cursor() API; the only way to bring it back
 * is to re-issue tb_set_cursor() with valid coordinates. */
static int g_tb2_cursor_x = 0;
static int g_tb2_cursor_y = 0;

static void tb2_shutdown(void) {
	/* 2026-06-29 JES #805 M6: destroy decoder BEFORE tb_shutdown.  Any
	 * decoder-side writes (e.g., a mouse-disable sequence the decoder
	 * may emit on destroy in a future milestone) must land while the
	 * TTY is still in raw mode and the fd is still valid.  Order
	 * matters; do not reorder these two calls. */
	input_decoder_destroy(g_decoder);
	g_decoder = NULL;

	if (g_tb2_owns_tty_fd && g_tb2_tty_fd >= 0) {
		close(g_tb2_tty_fd);
	}
	g_tb2_tty_fd      = -1;
	g_tb2_owns_tty_fd = false;

	g_tb2_cursor_x = 0;
	g_tb2_cursor_y = 0;
	tb_shutdown();
}

static int tb2_width(void) {
	return tb_width();
}

static int tb2_height(void) {
	return tb_height();
}

static int tb2_color_depth(void) {
	/* termbox2 v2.5.0 exposes color-capability via output mode queries.
	 * For A.1, return 256 as a safe default (most modern terminals support it).
	 * A future enhancement can call tb_set_output_mode and query the result. */
	return 256;
}

static void tb2_set_cell(int x, int y, uint32_t ch,
                         uint16_t fg, uint16_t bg, uint16_t attr) {
	uintattr_t ta  = translate_attr(attr);
	uintattr_t tfg = translate_color(fg);
	uintattr_t tbg = translate_color(bg);
	tb_set_cell(x, y, ch, tfg | ta, tbg);
}

static void tb2_set_cursor(int x, int y) {
	g_tb2_cursor_x = x;
	g_tb2_cursor_y = y;
	tb_set_cursor(x, y);
}

static void tb2_set_cursor_visible(bool visible) {
	if (visible) {
		/* Restore visibility by re-issuing the last known position.
		 * termbox2 has no dedicated tb_show_cursor(); tb_set_cursor() with
		 * valid coords is the canonical re-show idiom. */
		tb_set_cursor(g_tb2_cursor_x, g_tb2_cursor_y);
	} else {
		tb_hide_cursor();
	}
}

static void tb2_present(void) {
	tb_present();
}

/* -------------------------------------------------------------------------
 * 2026-06-29 JES #805 M6: poll event via the input decoder.
 *
 * The decoder emits boxen_event_t directly -- no translation layer needed.
 * timeout_ms flows straight through to input_decoder_poll, which uses
 * select(2) for the blocking wait and read(2) to drain the TTY pipe.
 *
 * Threading: the caller (`boxen_repl_event_loop` at boxen_repl.c:2147-2152,
 * mirroring `debugger_tui_main` per ADR-014) DROPS the Frontier GIL via
 * `pthread_mutex_unlock(&frontier_gil)` BEFORE calling boxen_poll_event,
 * and reacquires it after.  So this function -- and `input_decoder_poll`
 * underneath -- runs with the GIL NOT held during the blocking wait.
 * That's by design: it lets other Frontier threads make progress while
 * the REPL is parked in select(2).  M6 preserves the prior behavior in
 * this respect (tb_peek_event also ran with the GIL released).
 *
 * The single-owner invariant from input_decoder.h still holds because
 * the REPL thread is the only one that touches *g_decoder; the other
 * GIL-acquiring threads run UserTalk code that never reaches the
 * decoder.  No locks or atomics needed inside the decoder.
 *
 * Risk R3 (mouse-enable timing window) is mitigated inside the decoder
 * (mouse writes are synchronous + the decoder accepts both SGR and X10
 * formats during the handshake window).  Risk R4 (silent breakage) is
 * mitigated by the PTY-replay test suite (138 tests at M5; M6 adds none
 * because inject_bytes still bypasses tty_fd in the test seam).
 * ---------------------------------------------------------------------- */
static int tb2_poll_event(boxen_event_t *out, int timeout_ms) {
	if (out == NULL) {
		return BOXEN_ERR_INVALID;
	}
	if (g_decoder == NULL) {
		/* Defensive: tb2_init was not called or failed.  Return TIMEOUT
		 * rather than IO so an over-eager poll during teardown is not
		 * mistaken for a hard error. */
		memset(out, 0, sizeof(*out));
		return BOXEN_ERR_TIMEOUT;
	}
	return input_decoder_poll(g_decoder, out, timeout_ms);
}

static void tb2_clear(void) {
	tb_clear();
}

/* -------------------------------------------------------------------------
 * 2026-08-09 JES C M7: mouse-mode passthrough.
 *
 * Forwards to the input decoder, which owns the terminal writes (SGR
 * mouse + bracketed paste, plan section 5).  Deliberately does NOT call
 * tb_set_input_mode(TB_INPUT_MOUSE) -- termbox2's input path is dead
 * post-M6, and that call was the PR #808 regression.
 *
 * Threading: callers reach this via boxen_set_mouse from GIL-held,
 * between-polls context only (slash-command dispatch, palette open /
 * close) -- see the set_mouse contract in input_decoder.h.
 * ---------------------------------------------------------------------- */
static void tb2_set_mouse(bool enable) {
	/* NULL g_decoder (init failed / shutdown raced) is safe: the decoder's
	 * own NULL guard makes this a no-op. */
	input_decoder_set_mouse(g_decoder, enable);
}

/* -------------------------------------------------------------------------
 * Vtable and accessor
 * ---------------------------------------------------------------------- */

static const boxen_backend_t g_tb2_backend = {
	.init               = tb2_init,
	.shutdown           = tb2_shutdown,
	.width              = tb2_width,
	.height             = tb2_height,
	.color_depth        = tb2_color_depth,
	.set_cell           = tb2_set_cell,
	.set_cursor         = tb2_set_cursor,
	.set_cursor_visible = tb2_set_cursor_visible,
	.present            = tb2_present,
	.poll_event         = tb2_poll_event,
	.clear              = tb2_clear,
	.set_mouse          = tb2_set_mouse,
};

const boxen_backend_t *boxen_tb2_backend(void) {
	return &g_tb2_backend;
}
