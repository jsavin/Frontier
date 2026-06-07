/*
 * boxen_internal.h -- private definitions for the boxen substrate.
 *
 * NOT part of the public API. Never included by tests or consumers
 * outside frontier-cli/boxen/. Tests use backend_mock.h instead.
 */

#ifndef BOXEN_INTERNAL_H
#define BOXEN_INTERNAL_H

#include "boxen.h"

/* -------------------------------------------------------------------------
 * Double-click tuning constants
 * ---------------------------------------------------------------------- */

/* Maximum ms between two press events to qualify as a double-click. */
#define BOXEN_DOUBLE_CLICK_MS     500

/* Maximum cell distance (Chebyshev) between two clicks to qualify. */
#define BOXEN_DOUBLE_CLICK_RADIUS  1

/* -------------------------------------------------------------------------
 * Internal logging
 * ---------------------------------------------------------------------- */

/* Internal log dispatcher -- calls the installed hook if any. */
void boxen__log(boxen_log_level_t level,
                const char *file, int line,
                const char *fmt, ...);

/* Internal last-error setter -- records a human-readable error string in
 * thread-local storage retrievable by boxen_last_error_str(). Called from
 * boxen API functions that detect a failure they want to expose to callers. */
void boxen__set_last_error(const char *msg);

#define BOXEN_LOG_E(fmt, ...) \
	boxen__log(BOXEN_LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BOXEN_LOG_W(fmt, ...) \
	boxen__log(BOXEN_LOG_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BOXEN_LOG_D(fmt, ...) \
	boxen__log(BOXEN_LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BOXEN_LOG_T(fmt, ...) \
	boxen__log(BOXEN_LOG_TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* -------------------------------------------------------------------------
 * Window struct (opaque to consumers; concrete here for internal use)
 * ---------------------------------------------------------------------- */

/* Magic value stored at the head of every live window struct. Cleared on
 * boxen_window_close so a double-close (or a stale pointer reused as a
 * window) can be detected by boxen_window_close and rejected silently.
 * Not perfect (a freed-then-realloced-as-window struct could match), but
 * catches the common double-close mistake without runtime cost beyond
 * an integer compare. */
#define BOXEN_WINDOW_MAGIC  0x424F584Eu  /* "BOXN" */

struct boxen_window {
	uint32_t magic;       /* BOXEN_WINDOW_MAGIC when live; 0 after close */
	char    *title;
	boxen_rect_t rect;
	void    *user_data;

	/* Z-order: managed by the boxen core (A.3+) */
	int      z_index;

	/* Scroll state (A.5+) */
	int      scroll_x;
	int      scroll_y;
	int      content_w;
	int      content_h;

	/* Flags */
	bool     modal;
	bool     focused;
	bool     resizable;   /* Phase C */
	bool     movable;     /* Phase C */
	int      min_w;       /* Phase C */
	int      min_h;       /* Phase C */
	boxen_pin_edge_t pinned;  /* A.6+ */

	/* Callbacks */
	boxen_draw_fn  draw_fn;
	boxen_input_fn input_fn;

	/* Row highlight (A.5+) */
	int            highlight_row;  /* -1 = none */
	uint8_t        highlight_attr;
	boxen_color_t  highlight_fg;
	boxen_color_t  highlight_bg;

	/* Linked list linkage for window manager (A.3+) */
	struct boxen_window *next;
	struct boxen_window *prev;
};

#endif /* BOXEN_INTERNAL_H */
