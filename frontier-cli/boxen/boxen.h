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

/* Phase C -- declared but not implemented until A.4 / C */
void boxen_window_set_resizable(boxen_window_t *win, bool resizable);
void boxen_window_set_movable(boxen_window_t *win, bool movable);
void boxen_window_set_min_size(boxen_window_t *win, int min_w, int min_h);
void boxen_window_set_pinned(boxen_window_t *win, boxen_pin_edge_t edge);

/* -------------------------------------------------------------------------
 * Z-order + focus (A.3+)
 * ---------------------------------------------------------------------- */

void boxen_window_raise(boxen_window_t *win);
void boxen_window_lower(boxen_window_t *win);
void boxen_window_focus(boxen_window_t *win);
void boxen_window_set_modal(boxen_window_t *win, bool modal);
void boxen_window_invalidate(boxen_window_t *win);

/* -------------------------------------------------------------------------
 * Draw callbacks (A.3+)
 * ---------------------------------------------------------------------- */

typedef void (*boxen_draw_fn)(boxen_window_t *win, void *user_data);
typedef void (*boxen_input_fn)(boxen_window_t *win, const boxen_event_t *ev, void *user_data);

void boxen_window_set_draw(boxen_window_t *win, boxen_draw_fn fn);
void boxen_window_set_input(boxen_window_t *win, boxen_input_fn fn);

/* -------------------------------------------------------------------------
 * Cell writing (A.2+)
 * Coordinates are window-local (content space), 0-based.
 * Clips silently if out of the window's content area.
 * ---------------------------------------------------------------------- */

void boxen_set_cell(boxen_window_t *win, int x, int y,
                    uint32_t ch, uint16_t fg, uint16_t bg, uint16_t attr);

void boxen_draw_text(boxen_window_t *win, int x, int y,
                     const char *utf8, uint16_t fg, uint16_t bg, uint16_t attr);

void boxen_fill_rect(boxen_window_t *win, boxen_rect_t r,
                     uint32_t ch, uint16_t fg, uint16_t bg, uint16_t attr);

/* -------------------------------------------------------------------------
 * Screen-to-window coordinate mapping (A.2+)
 * Given screen position (sx, sy), find the topmost window there and
 * return window-local content coordinates in (*cx, *cy).
 * Returns NULL if no window is at (sx, sy).
 * ---------------------------------------------------------------------- */

boxen_window_t *boxen_window_at(int sx, int sy, int *cx, int *cy);

/* -------------------------------------------------------------------------
 * Scroll state (A.5+)
 * ---------------------------------------------------------------------- */

void boxen_window_set_content_size(boxen_window_t *win, int w, int h);
void boxen_window_set_scroll(boxen_window_t *win, int x, int y);
void boxen_window_get_scroll(const boxen_window_t *win, int *x, int *y);
void boxen_window_scroll_by(boxen_window_t *win, int dx, int dy);
void boxen_window_ensure_visible(boxen_window_t *win, int cx, int cy);

/* Mark a content row with a highlight attribute (A.5+; debugger current-line). */
void boxen_window_set_row_highlight(boxen_window_t *win, int content_row,
                                    uint8_t highlight_attr,
                                    boxen_color_t fg, boxen_color_t bg);

/* -------------------------------------------------------------------------
 * Event loop (A.3+)
 * ---------------------------------------------------------------------- */

/* Low-level: poll for one event. Returns BOXEN_OK or BOXEN_ERR_TIMEOUT. */
boxen_result_t boxen_poll_event(boxen_event_t *out, int timeout_ms);

/* Composite all windows and flush to the terminal. */
void boxen_present(void);

/* Convenience main loop for standalone programs. */
void boxen_run(void);
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
