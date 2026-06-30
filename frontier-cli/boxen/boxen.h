/*
 * boxen.h -- public API for the boxen TUI window manager substrate.
 *
 * This is the ONLY public header. All consumers of boxen include only this file.
 * boxen_internal.h is private; backend_mock.h is test-only.
 *
 * Threading contract:
 *   Boxen is single-threaded. All public API calls must be made from a single
 *   thread. In Frontier, that thread is the GIL holder. Boxen has no internal
 *   locks; concurrent access from multiple threads produces undefined behavior.
 *
 *   boxen_poll_event() may block. Release the GIL before calling it with a
 *   non-zero timeout, following the pattern in frontier-cli/protocol_handler.c.
 *
 * Coordinate convention: 0-based throughout. Origin (0,0) is top-left.
 *
 * Error reporting: fallible functions return a boxen_result_t (negative = error).
 *   Use boxen_last_error_str() for a human-readable description.
 */

#ifndef BOXEN_H
#define BOXEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * Result codes
 * ---------------------------------------------------------------------- */

typedef int boxen_result_t;

#define BOXEN_OK           0
#define BOXEN_ERR_INVALID (-1)  /* NULL or bad argument */
#define BOXEN_ERR_INIT    (-2)  /* backend init failed */
#define BOXEN_ERR_ALREADY (-3)  /* already initialized */
#define BOXEN_ERR_NOMEM   (-4)  /* allocation failed */
#define BOXEN_ERR_TIMEOUT (-5)  /* poll_event timed out (not a fatal error) */
#define BOXEN_ERR_IO      (-6)  /* terminal I/O error */
#define BOXEN_ERR_OVERFLOW (-7) /* window count or buffer limit exceeded */

/* Returns a human-readable description of the last error on this thread.
 * The string is owned by boxen; valid until the next boxen API call.
 * Never returns NULL (returns "" for BOXEN_OK). */
const char *boxen_last_error_str(void);

/* -------------------------------------------------------------------------
 * Key / modifier enums
 * ---------------------------------------------------------------------- */

typedef enum {
	/* Printable range: not enumerated; use ev.key.ch (Unicode codepoint). */
	BOXEN_KEY_NONE = 0,

	/* Function keys */
	BOXEN_KEY_F1  = 0x10000,
	BOXEN_KEY_F2,
	BOXEN_KEY_F3,
	BOXEN_KEY_F4,
	BOXEN_KEY_F5,
	BOXEN_KEY_F6,
	BOXEN_KEY_F7,
	BOXEN_KEY_F8,
	BOXEN_KEY_F9,
	BOXEN_KEY_F10,
	BOXEN_KEY_F11,
	BOXEN_KEY_F12,

	/* Navigation */
	BOXEN_KEY_UP,
	BOXEN_KEY_DOWN,
	BOXEN_KEY_LEFT,
	BOXEN_KEY_RIGHT,
	BOXEN_KEY_HOME,
	BOXEN_KEY_END,
	BOXEN_KEY_PGUP,
	BOXEN_KEY_PGDN,
	BOXEN_KEY_INSERT,
	BOXEN_KEY_DELETE,

	/* Editing -- listed BEFORE CTRL range so translation handles these first */
	BOXEN_KEY_BACKSPACE,
	BOXEN_KEY_TAB,
	BOXEN_KEY_ENTER,
	BOXEN_KEY_ESCAPE,

	/* Control characters (named aliases: CTRL_I = Tab, CTRL_M = Enter, CTRL_[ = Esc) */
	BOXEN_KEY_CTRL_A,
	BOXEN_KEY_CTRL_B,
	BOXEN_KEY_CTRL_C,
	BOXEN_KEY_CTRL_D,
	BOXEN_KEY_CTRL_E,
	BOXEN_KEY_CTRL_F,
	BOXEN_KEY_CTRL_G,
	BOXEN_KEY_CTRL_H,
	BOXEN_KEY_CTRL_I,     /* = Tab alias */
	BOXEN_KEY_CTRL_J,
	BOXEN_KEY_CTRL_K,
	BOXEN_KEY_CTRL_L,
	BOXEN_KEY_CTRL_M,     /* = Enter alias */
	BOXEN_KEY_CTRL_N,
	BOXEN_KEY_CTRL_O,
	BOXEN_KEY_CTRL_P,
	BOXEN_KEY_CTRL_Q,
	BOXEN_KEY_CTRL_R,
	BOXEN_KEY_CTRL_S,
	BOXEN_KEY_CTRL_T,
	BOXEN_KEY_CTRL_U,
	BOXEN_KEY_CTRL_V,
	BOXEN_KEY_CTRL_W,
	BOXEN_KEY_CTRL_X,
	BOXEN_KEY_CTRL_Y,
	BOXEN_KEY_CTRL_Z,
	BOXEN_KEY_CTRL_BACKSLASH,
	BOXEN_KEY_CTRL_BRACKET,  /* CTRL_[ = Esc alias */
	BOXEN_KEY_CTRL_SPACE,
} boxen_key_t;

typedef enum {
	BOXEN_MOD_NONE  = 0,
	BOXEN_MOD_ALT   = 1 << 0,
	BOXEN_MOD_CTRL  = 1 << 1,
	BOXEN_MOD_SHIFT = 1 << 2,
	BOXEN_MOD_META  = 1 << 3,  /* macOS Cmd / Windows Meta */
} boxen_mod_t;

/* -------------------------------------------------------------------------
 * Mouse flags (bitmask on ev.mouse.flags)
 * ---------------------------------------------------------------------- */

#define BOXEN_MOUSE_DOUBLE_CLICK 0x01  /* second press in a rapid sequence */
#define BOXEN_MOUSE_TRIPLE_CLICK 0x02  /* reserved */

/* -------------------------------------------------------------------------
 * Events
 * ---------------------------------------------------------------------- */

typedef enum {
	BOXEN_EV_NONE = 0,
	BOXEN_EV_KEY,
	BOXEN_EV_MOUSE,
	BOXEN_EV_RESIZE,
	/* 2026-06-29 JES #812 Phase C M4: bracketed-paste event.
	 *
	 * Emitted by the input decoder when a paste sequence (\e[200~ ... \e[201~)
	 * is fully buffered.  The data field is a heap-allocated UTF-8 byte
	 * string (NOT NUL-terminated -- len is authoritative); the consumer of
	 * the event takes ownership and MUST free(ev.paste.data) after handling
	 * the event.  This is the only event variant whose union arm transfers
	 * heap ownership to the caller; all other event variants are POD.
	 *
	 * See planning/phase_c/INPUT_DECODER_PLAN.md sections 3.8 and 5.3 for
	 * the rationale (Cmd-V into the REPL under mouse mode must not be
	 * shredded into per-keystroke events). */
	BOXEN_EV_PASTE,
} boxen_event_type_t;

typedef struct boxen_event {
	boxen_event_type_t type;
	union {
		struct {
			boxen_key_t key;   /* BOXEN_KEY_* for special keys; 0 for printable */
			uint32_t    ch;    /* Unicode codepoint for printable; 0 for special */
			uint16_t    mod;   /* BOXEN_MOD_* bitmask */
		} key;
		struct {
			int      x, y;
			uint8_t  button;   /* 1=left, 2=middle, 3=right, 4/5=scroll */
			bool     pressed;
			uint16_t mod;      /* BOXEN_MOD_* bitmask */
			uint16_t flags;    /* BOXEN_MOUSE_* bitmask */
		} mouse;
		struct {
			int w, h;
		} resize;
		/* 2026-06-29 JES #812 Phase C M4: bracketed-paste payload.
		 *
		 * data: heap-allocated, owned by the event consumer.  NOT NUL-
		 *   terminated -- len is the authoritative byte count.  CR/LF
		 *   line endings inside the paste are normalized to LF (\n) by
		 *   the decoder; embedded NULs are passed through unchanged
		 *   (callers that treat the buffer as a C string must filter).
		 * len: byte length of data (0 if data is NULL).
		 *
		 * If len == 0 and data == NULL the event represents an empty paste
		 * (the user pasted nothing between the markers).  Consumers should
		 * still call free(data) defensively -- free(NULL) is a no-op.
		 *
		 * The decoder caps paste size at 256 KiB; oversize pastes are
		 * truncated to the cap and a BOXEN_LOG_W is emitted at the
		 * truncation point. */
		struct {
			char   *data;
			size_t  len;
		} paste;
	};
} boxen_event_t;

/* -------------------------------------------------------------------------
 * Cell attributes and colors
 * ---------------------------------------------------------------------- */

typedef enum {
	BOXEN_ATTR_NONE      = 0,
	BOXEN_ATTR_BOLD      = 1 << 0,
	BOXEN_ATTR_UNDERLINE = 1 << 1,
	BOXEN_ATTR_REVERSE   = 1 << 2,
	BOXEN_ATTR_ITALIC    = 1 << 3,
	BOXEN_ATTR_DIM       = 1 << 4,
} boxen_attr_t;

typedef uint16_t boxen_color_t;  /* terminal color index or RGB packed value */

/* Common color constants (16-color palette) */
#define BOXEN_COLOR_DEFAULT  0
#define BOXEN_COLOR_BLACK    1
#define BOXEN_COLOR_RED      2
#define BOXEN_COLOR_GREEN    3
#define BOXEN_COLOR_YELLOW   4
#define BOXEN_COLOR_BLUE     5
#define BOXEN_COLOR_MAGENTA  6
#define BOXEN_COLOR_CYAN     7
#define BOXEN_COLOR_WHITE    8

/* -------------------------------------------------------------------------
 * Rectangle
 * ---------------------------------------------------------------------- */

typedef struct boxen_rect {
	int x, y;  /* top-left corner (0-based) */
	int w, h;  /* width, height */
} boxen_rect_t;

/* -------------------------------------------------------------------------
 * Backend vtable (11 functions)
 * Only backend_tb2.c and backend_mock.c implement this interface.
 * No other file may reference termbox2 symbols directly.
 * ---------------------------------------------------------------------- */

typedef struct boxen_backend {
	int  (*init)(void *config);
	void (*shutdown)(void);
	int  (*width)(void);
	int  (*height)(void);
	int  (*color_depth)(void);  /* returns 1, 16, 256, or 16777216 */
	void (*set_cell)(int x, int y, uint32_t ch,
	                 uint16_t fg, uint16_t bg, uint16_t attr);
	void (*set_cursor)(int x, int y);
	void (*set_cursor_visible)(bool visible);
	void (*present)(void);
	int  (*poll_event)(boxen_event_t *out, int timeout_ms);
	void (*clear)(void);
} boxen_backend_t;

/* -------------------------------------------------------------------------
 * Logging hook
 * ---------------------------------------------------------------------- */

typedef enum {
	BOXEN_LOG_ERROR = 0,
	BOXEN_LOG_WARN  = 1,
	BOXEN_LOG_DEBUG = 2,
	BOXEN_LOG_TRACE = 3,
} boxen_log_level_t;

typedef void (*boxen_log_fn)(
	boxen_log_level_t level,
	const char *file,
	int line,
	const char *msg,
	void *user_data
);

/* Install a log hook. Pass NULL to suppress all output (the default).
 * The hook is called synchronously; do not call boxen APIs from inside it.
 *
 * Threading: per boxen's overall threading contract, the caller must hold
 * any external lock (e.g. Frontier's GIL) that serializes concurrent access.
 * The function pointer is stored with a plain write; concurrent install + log
 * from a non-GIL thread is a torn-read race. If a future caller needs to log
 * from a signal handler or non-GIL context, the function pointer storage
 * must be upgraded to _Atomic with acquire/release semantics. */
void boxen_set_log_hook(boxen_log_fn fn, void *user_data);

/* -------------------------------------------------------------------------
 * Allocator vtable
 * ---------------------------------------------------------------------- */

typedef struct boxen_allocator {
	void *(*malloc)(size_t size, void *user_data);
	void  (*free)(void *ptr, void *user_data);
	void  *user_data;
} boxen_allocator_t;

/* -------------------------------------------------------------------------
 * Pinned-window edge (Phase A.6+)
 * ---------------------------------------------------------------------- */

typedef enum {
	BOXEN_PIN_NONE   = 0,
	BOXEN_PIN_TOP    = 1,
	BOXEN_PIN_BOTTOM = 2,
} boxen_pin_edge_t;

/* -------------------------------------------------------------------------
 * Unicode width
 * Vendored from Markus Kuhn 2007 mk_wcwidth, public domain.
 * Returns 0 for NUL, 0 for non-printable controls (C0/C1), 1 for ASCII,
 * 2 for CJK wide characters.
 * ---------------------------------------------------------------------- */

int boxen_wcwidth(uint32_t ucs);

/* -------------------------------------------------------------------------
 * Library lifecycle (A.2+)
 * ---------------------------------------------------------------------- */

/* Initialize boxen with the given backend. Pass NULL allocator for system
 * malloc/free. Must be called before any other boxen API. */
boxen_result_t boxen_init(const boxen_backend_t *backend,
                          void *backend_config,
                          const boxen_allocator_t *allocator);

/* Shut down boxen and release all resources. */
void boxen_shutdown(void);

/* Query terminal dimensions from the current backend.
 * Writes the current width and height into *w and *h.
 * Falls back to 80x24 if boxen is not initialized or the backend returns <= 0.
 * 2026-06-09 JES #691 Phase C.1 round 2 P1: terminal dimensions for
 * boxen_outline_open (avoid hardcoded 80x24). */
void boxen_get_screen_size(int *w, int *h);

/* -------------------------------------------------------------------------
 * Window type (opaque; defined in boxen_internal.h)
 * ---------------------------------------------------------------------- */

typedef struct boxen_window boxen_window_t;

/* -------------------------------------------------------------------------
 * Window lifecycle (A.2+)
 * ---------------------------------------------------------------------- */

boxen_window_t *boxen_window_open(const char *title, boxen_rect_t rect,
                                  void *user_data);
void            boxen_window_close(boxen_window_t *win);

void boxen_window_set_rect(boxen_window_t *win, boxen_rect_t rect);
void boxen_window_set_title(boxen_window_t *win, const char *title);
void boxen_window_set_user_data(boxen_window_t *win, void *data);
void *boxen_window_get_user_data(const boxen_window_t *win);

boxen_rect_t boxen_window_get_rect(const boxen_window_t *win);
int boxen_window_content_width(const boxen_window_t *win);
int boxen_window_content_height(const boxen_window_t *win);

/* A.4+: resize/move property setters. */
/* Enable/disable mouse and keyboard drag for moving this window. Default: false. */
void boxen_window_set_movable(boxen_window_t *win, bool movable);
/* Enable/disable mouse and keyboard drag for resizing this window. Default: false. */
void boxen_window_set_resizable(boxen_window_t *win, bool resizable);
/* Set minimum dimensions enforced during drag resize and terminal-resize clamping.
 * Values <= 0 are treated as 1 internally. Default is (0, 0) (i.e., min 1x1). */
void boxen_window_set_min_size(boxen_window_t *win, int min_w, int min_h);
/* Phase A.6+: pin a window to a screen edge (e.g., a keybind footer). */
void boxen_window_set_pinned(boxen_window_t *win, boxen_pin_edge_t edge);
/* Phase A.6+: enable/disable border chrome (corners, edges, title, scrollbar).
 * Default: ON. Disable for borderless surfaces like pinned footers or raw overlays. */
void boxen_window_set_borders(boxen_window_t *win, bool borders);

/* -------------------------------------------------------------------------
 * Z-order + focus (A.3+)
 * ---------------------------------------------------------------------- */

void boxen_window_raise(boxen_window_t *win);
void boxen_window_lower(boxen_window_t *win);
void boxen_window_focus(boxen_window_t *win);
void boxen_window_set_modal(boxen_window_t *win, bool modal);
void boxen_window_invalidate(boxen_window_t *win);

/* -------------------------------------------------------------------------
 * 2026-06-10 JES #691 C.1.x: application-global key pre-dispatch.
 *
 * Application-level keys like Ctrl-C (quit) and `/` (slash-menu palette)
 * should reach the REPL even when an editor window has focus.  Otherwise
 * the user types Ctrl-C in an outline editor and nothing happens because
 * the editor's input_fn doesn't recognize it.
 *
 * Usage: the application designates one window as the "global key target"
 * (typically the REPL's input window) and a predicate that decides which
 * key events qualify as global.  Before dispatching a key event to the
 * focused window (or a modal), boxen_dispatch_event checks the predicate;
 * if it returns true, the event goes to the global target instead.
 *
 * Mouse events are NEVER routed to the global target -- they always go
 * to the at-point window or focused window per the standard rules.
 *
 * Pass NULL for either to clear the registration.  Reading both as NULL
 * (the default) disables global pre-dispatch entirely.
 *
 * Threading: like all boxen state, this is GIL-protected at the
 * application layer.  No internal locking.
 * ---------------------------------------------------------------------- */

typedef bool (*boxen_global_key_filter_fn)(const boxen_event_t *ev);

void boxen_set_global_key_handler(boxen_window_t *target,
                                  boxen_global_key_filter_fn filter);

/* -------------------------------------------------------------------------
 * Draw callbacks (A.3+)
 * ---------------------------------------------------------------------- */

typedef void (*boxen_draw_fn)(boxen_window_t *win, void *user_data);
typedef void (*boxen_input_fn)(boxen_window_t *win, const boxen_event_t *ev, void *user_data);

void boxen_window_set_draw(boxen_window_t *win, boxen_draw_fn fn);
void boxen_window_set_input(boxen_window_t *win, boxen_input_fn fn);

/* -------------------------------------------------------------------------
 * Cell writing (A.2+, content-coord semantics)
 *
 * Coordinates are window-local CONTENT coordinates, 0-based. The content
 * area is the cells INSIDE any window chrome (borders, scrollbar).
 *
 * Chrome-aware translation (A.6+):
 *
 *   With borders enabled (default):
 *     - content (0, 0) maps to terminal (rect.x + 1, rect.y + 1)
 *     - content_width  = rect.w - 2  (left + right border consumed)
 *     - content_height = rect.h - 2  (top + bottom border consumed)
 *     - If content_h > content_height a vertical scrollbar is drawn on
 *       the rightmost interior column. content_width is reduced by 1
 *       further in that case so callers don't draw into the scrollbar.
 *
 *   With borders disabled (call boxen_window_set_borders(win, false)):
 *     - content (0, 0) maps to terminal (rect.x, rect.y)
 *     - content_width  = rect.w
 *     - content_height = rect.h
 *     - No scrollbar is drawn.
 *
 * Consumers that draw against the full rect (pre-A.6 behavior) must call
 * boxen_window_set_borders(win, false) at window-open time. The default
 * change from "no chrome" (A.2-A.5) to "borders on" (A.6) is the
 * acknowledged pre-1.0 API break.
 *
 * All three functions clip silently if (x, y) is out of the content area.
 * ---------------------------------------------------------------------- */

void boxen_set_cell(boxen_window_t *win, int x, int y,
                    uint32_t ch, uint16_t fg, uint16_t bg, uint16_t attr);

void boxen_draw_text(boxen_window_t *win, int x, int y,
                     const char *utf8, uint16_t fg, uint16_t bg, uint16_t attr);

void boxen_fill_rect(boxen_window_t *win, boxen_rect_t r,
                     uint32_t ch, uint16_t fg, uint16_t bg, uint16_t attr);

/* -------------------------------------------------------------------------
 * Cursor positioning (A.7+, content-coord semantics)
 *
 * Position the terminal cursor at a content-relative coordinate inside the
 * given window. Mirrors boxen_set_cell's translation pipeline -- chrome
 * (border) offset, scroll offset, and content-viewport clipping all apply
 * identically. Beyond the set_cell pipeline these wrappers add two checks:
 *
 *   Modal-occlusion check:
 *     If a modal window is present and covers the target screen cell, the
 *     cursor is hidden (set_cursor_visible(false)) instead of drawn under
 *     the modal. The modal itself bypasses the check for its own writes.
 *
 *   Focus gate:
 *     Only the focused window OR the topmost modal window may drive the
 *     terminal cursor. Calls from a non-focused, non-modal window are
 *     silently ignored. This is deliberate footgun prevention -- two
 *     windows fighting over a single terminal cursor produces undefined
 *     behavior. Surfaces that need a non-focused indicator should draw a
 *     cell-based caret with boxen_set_cell + BOXEN_ATTR_REVERSE.
 *
 * Out-of-viewport target:
 *   When the translated screen coord falls outside the content viewport
 *   (scroll-adjusted, clip-checked), the cursor is hidden rather than
 *   left at a stale position. The call still succeeds.
 *
 * Visibility toggle:
 *   boxen_window_set_cursor_visible is independent of position -- a hide
 *   followed by a show restores the cursor at the last set_cursor coord.
 *   It also obeys the focus gate: a non-focused, non-modal caller is
 *   silently ignored.
 *   If no prior boxen_window_set_cursor call successfully placed the cursor,
 *   set_cursor_visible(true) makes the cursor visible at the backend's
 *   default position (typically (0, 0) after init).
 *
 * Coordinates are validated against BOXEN_MAX_DIMENSION to prevent signed-
 * int overflow in the translation arithmetic.
 *
 * First consumer: Phase B debugger TUI (issue #733) -- the breakpoint
 * condition modal (B.4) and scratch-eval pane (B.7) both need cursor
 * positioning in content coords.
 * ---------------------------------------------------------------------- */

void boxen_window_set_cursor(boxen_window_t *win, int cx, int cy);
void boxen_window_set_cursor_visible(boxen_window_t *win, bool visible);

/* -------------------------------------------------------------------------
 * Screen-to-window coordinate mapping (A.2+)
 * Given screen position (sx, sy), find the topmost window there and
 * return window-local content coordinates in (*cx, *cy).
 * Returns NULL if no window is at (sx, sy).
 *
 * A.6+ chrome handling: when (sx, sy) lands on the border or scrollbar
 * of a window, cx and cy are set to BOXEN_HIT_CHROME. This sentinel is
 * distinct from any valid content coordinate (which lives in
 * [-BOXEN_MAX_DIMENSION, BOXEN_MAX_DIMENSION]) regardless of the window's
 * current scroll offset. Callers that act on content coords (e.g. the
 * cmd-2-click identifier resolver) should check for BOXEN_HIT_CHROME
 * before treating cx and cy as row/column indices.
 * ---------------------------------------------------------------------- */

#define BOXEN_HIT_CHROME (-2147483647 - 1)  /* INT_MIN, sentinel for chrome hits */

boxen_window_t *boxen_window_at(int sx, int sy, int *cx, int *cy);

/* -------------------------------------------------------------------------
 * Scroll state (A.5+)
 * ---------------------------------------------------------------------- */

void boxen_window_set_content_size(boxen_window_t *win, int w, int h);
void boxen_window_set_scroll(boxen_window_t *win, int x, int y);
void boxen_window_get_scroll(const boxen_window_t *win, int *x, int *y);
void boxen_window_scroll_by(boxen_window_t *win, int dx, int dy);
void boxen_window_ensure_visible(boxen_window_t *win, int cx, int cy);

/* Mark a content row with a highlight attribute (A.5+; debugger current-line).
 * Pass content_row = -1 to disable.
 *
 * Known limitation (A.5): the overlay writes ' ' (space) over each cell in the
 * row, erasing any character the surface drew. Only the attr/fg/bg are
 * preserved as visual indicators. This is because the backend vtable has no
 * read_cell primitive to compose the highlight over existing content. A
 * future vtable extension can address this if surface-character preservation
 * becomes required.
 *
 * Z-order limitation (A.5): the highlight overlay paints onto every cell of
 * the row in the window's terminal rect, including cells that may be covered
 * by higher-z-order windows. For the debugger TUI's single-active-window case
 * this is benign; for multi-window scenarios where a focused window's
 * highlight underlies another window's content, the highlight will bleed
 * through. A future per-window post-draw pass can clip against higher-z
 * windows. */
void boxen_window_set_row_highlight(boxen_window_t *win, int content_row,
                                    uint8_t highlight_attr,
                                    boxen_color_t fg, boxen_color_t bg);

/* -------------------------------------------------------------------------
 * Event loop (A.3+)
 * ---------------------------------------------------------------------- */

/* Low-level: poll for one event. Returns BOXEN_OK or BOXEN_ERR_TIMEOUT.
 * Does NOT dispatch the event -- use boxen_dispatch_event() for that.
 * The caller is responsible for GIL release/reacquire around blocking polls. */
boxen_result_t boxen_poll_event(boxen_event_t *out, int timeout_ms);

/* Dispatch an event to the appropriate window(s) according to focus and
 * modal rules:
 *   - If a modal window is present, key events go to the modal window only.
 *   - Otherwise key events go to the focused window.
 *   - Mouse events go to the topmost window at (ev->mouse.x, ev->mouse.y),
 *     unless a modal window is present (in which case modal receives them).
 *   - Resize events are passed to the focused window.
 *
 * Surface code that wants to interleave Frontier runtime events with boxen
 * events should call boxen_poll_event() and then boxen_dispatch_event()
 * separately. boxen_run() calls both internally.
 *
 * Calling from outside a boxen_run() loop is safe and supported. */
void boxen_dispatch_event(const boxen_event_t *ev);

/* Composite all windows and flush to the terminal. */
void boxen_present(void);

/* Convenience main loop for standalone programs. Polls events with a 100 ms
 * timeout, dispatches via boxen_dispatch_event, then calls boxen_present.
 * Exits when boxen_quit() is called from an input callback, or when the
 * backend returns a persistent (non-timeout) error.
 *
 * THREADING WARNING: boxen_run() holds the caller's external lock (e.g.
 * Frontier's GIL) for its entire duration -- it does NOT release the lock
 * around boxen_poll_event(). Calling boxen_run() from a Frontier runtime
 * callback or any GIL-holding context will block all other Frontier threads
 * for as long as the loop runs (i.e., until quit is called). Frontier
 * surfaces MUST use boxen_poll_event() + boxen_dispatch_event() directly,
 * wrapping the poll with a GIL release/reacquire as documented in the
 * debugger TUI handoff. boxen_run() is intended for standalone consumers
 * (examples, future non-Frontier programs) only. */
void boxen_run(void);

/* Signal boxen_run()'s loop to exit at the next iteration boundary. Typically
 * called from an input callback. Safe to call when boxen_run() is not active
 * (the flag is reset at the start of each boxen_run() call). */
void boxen_quit(void);

/* -------------------------------------------------------------------------
 * Layout helpers (A.6+)
 * ---------------------------------------------------------------------- */

void boxen_layout_split_h(boxen_rect_t total, float ratio,
                           const char *left_title,  boxen_window_t **left,
                           const char *right_title, boxen_window_t **right);

void boxen_layout_split_v(boxen_rect_t total, float ratio,
                           const char *top_title,    boxen_window_t **top,
                           const char *bottom_title, boxen_window_t **bottom);

/* -------------------------------------------------------------------------
 * Backend accessors (used by boxen_backend_tests.c)
 * ---------------------------------------------------------------------- */

/* Returns a pointer to the tb2 backend vtable (filled by backend_tb2.c). */
const boxen_backend_t *boxen_tb2_backend(void);

/* Returns a pointer to the mock backend vtable (filled by backend_mock.c). */
const boxen_backend_t *boxen_mock_backend(void);

#endif /* BOXEN_H */
