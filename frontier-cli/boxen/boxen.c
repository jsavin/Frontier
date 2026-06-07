/*
 * boxen.c -- window manager core: primitive, clipping, z-order, redraw,
 *            focus, input dispatch, modal, event loop, resize/move state
 *            machine (A.2 + A.3 + A.4).
 *
 * Implements (A.2):
 *   boxen_init / boxen_shutdown         -- library lifecycle
 *   boxen_window_open / close           -- window lifecycle
 *   boxen_window_set_* / get_*          -- window property accessors
 *   boxen_window_content_width/height   -- content dimensions (no chrome at A.2)
 *   boxen_set_cell                      -- window-local -> terminal coord + clipping
 *   boxen_draw_text                     -- UTF-8 decode + per-codepoint set_cell
 *   boxen_fill_rect                     -- filled rect via set_cell
 *   boxen_window_at                     -- screen -> window-local coord mapping
 *   boxen_last_error_str                -- last error string accessor
 *
 * Implements (A.3):
 *   boxen_present                       -- back-to-front draw + backend->present
 *   boxen_window_raise / lower          -- z-order manipulation
 *   boxen_window_focus                  -- focus model + raise
 *   boxen_window_set_modal              -- modal flag
 *   boxen_window_invalidate             -- no-op (always-redraw model; API for future)
 *   boxen_poll_event                    -- forwards to backend->poll_event
 *   boxen_dispatch_event                -- focus/modal/at-point input routing
 *   boxen_run / boxen_quit              -- convenience main loop with quit flag
 *
 * Implements (A.4):
 *   boxen_window_set_resizable          -- enable/disable resize via mouse/keyboard
 *   boxen_window_set_movable            -- enable/disable move via mouse/keyboard
 *   boxen_window_set_min_size           -- minimum window dimensions for clamping
 *   hit_test_window (static)            -- classify screen click relative to window
 *   g_drag state machine                -- NONE / MOVING / RESIZING / MOVING_KB / RESIZING_KB
 *   BOXEN_EV_RESIZE handling            -- clamp all windows to new terminal size
 *
 * Stubs (later milestones):
 *   scrolling -- A.5
 *   chrome / borders / layout -- A.6
 *
 * Coordinate convention: 0-based, origin top-left.
 *
 * Thread contract: single-threaded; caller must hold the GIL (Frontier) or
 * equivalent external lock. No internal synchronization.
 */

#include <stdlib.h>
#include <string.h>

#include "boxen.h"
#include "boxen_internal.h"

/* -------------------------------------------------------------------------
 * Library state
 * ---------------------------------------------------------------------- */

#ifndef BOXEN_MAX_WINDOWS
#define BOXEN_MAX_WINDOWS 64
#endif

static const boxen_backend_t *g_backend    = NULL;
static bool                   g_initialized = false;

/* Window list -- simple fixed array; index 0 = bottommost, index
 * g_window_count-1 = topmost. open appends (topmost); close swaps with last. */
static boxen_window_t        *g_windows[BOXEN_MAX_WINDOWS];
static int                    g_window_count = 0;

/* A.3 focus and event loop state.
 *
 * Threading: both are plain (non-_Atomic) and assume single-threaded access
 * under the caller's external lock (Frontier GIL). Concurrent focus mutation
 * + dispatch read would be a torn-pointer race on g_focused_window;
 * concurrent quit + run-loop read would be a data race on g_should_quit.
 * If a future caller needs to set g_should_quit from a signal handler or
 * other non-GIL context (e.g. SIGINT in a standalone consumer), upgrade
 * to `_Atomic bool` with relaxed store/load. Same guidance applies to
 * g_focused_window if cross-thread focus changes ever become a requirement. */
static boxen_window_t        *g_focused_window = NULL;
static bool                   g_should_quit     = false;

/* -------------------------------------------------------------------------
 * A.4 drag/resize state machine
 *
 * DRAG_NONE: normal event dispatch, no drag in progress.
 * DRAG_MOVING: mouse is held on a movable window's title bar; motion events
 *   update win->rect.x/y by (current - anchor) delta.
 * DRAG_RESIZING: mouse is held on a resizable window's corner; motion events
 *   update win->rect.w/h (and possibly x/y for non-BR corners), clamped to
 *   win->min_w / win->min_h.
 * DRAG_MOVING_KB: Ctrl-M was pressed on the focused window; arrow keys shift
 *   win->rect.x/y by 1; Enter/Escape ends the mode.
 * DRAG_RESIZING_KB: Ctrl-R was pressed on the focused window; arrow keys
 *   shift win->rect.w/h by 1, clamped to min; Enter/Escape ends the mode.
 *
 * corner encoding (for DRAG_RESIZING):
 *   0 = BR (bottom-right): drag changes w and h; origin fixed.
 *   1 = BL (bottom-left):  drag changes w, h, and x; y fixed.
 *   2 = TR (top-right):    drag changes w, h, and y; x fixed.
 *   3 = TL (top-left):     drag changes all four: x, y, w, h.
 * ---------------------------------------------------------------------- */

typedef enum {
	DRAG_NONE,
	DRAG_MOVING,
	DRAG_RESIZING,
	DRAG_MOVING_KB,
	DRAG_RESIZING_KB,
} drag_state_t;

/* Hit-test region codes returned by hit_test_window(). */
#define HIT_NONE       0
#define HIT_TITLE      1
#define HIT_BODY       2
#define HIT_CORNER_BR  3
#define HIT_CORNER_BL  4
#define HIT_CORNER_TR  5
#define HIT_CORNER_TL  6

static struct {
	drag_state_t   state;
	boxen_window_t *win;
	int             anchor_x;    /* mouse x when drag started */
	int             anchor_y;    /* mouse y when drag started */
	boxen_rect_t    start_rect;  /* window rect when drag started */
	int             corner;      /* which corner (0=BR,1=BL,2=TR,3=TL) */
	bool            was_pressed; /* tracks press-to-release transition so
	                              * the drag ends on any release event,
	                              * regardless of whether the terminal
	                              * preserves button info on release */
} g_drag;

/* Forward declaration: defined later in the Z-order section but used by the
 * resize/move property setters above it in the file. */
static int find_live_window_index(const boxen_window_t *win);
static void clamp_rect_to_bounds(boxen_rect_t *r);

/* -------------------------------------------------------------------------
 * Library lifecycle
 * ---------------------------------------------------------------------- */

boxen_result_t boxen_init(const boxen_backend_t *backend,
                          void *backend_config,
                          const boxen_allocator_t *allocator) {
	(void)allocator;  /* allocator vtable deferred per OVERVIEW; use system malloc */

	if (g_initialized) {
		boxen__set_last_error("boxen already initialized");
		return BOXEN_ERR_ALREADY;
	}
	if (backend == NULL) {
		boxen__set_last_error("backend must not be NULL");
		return BOXEN_ERR_INVALID;
	}

	int rc = backend->init(backend_config);
	if (rc != 0) {
		boxen__set_last_error("backend init failed");
		return BOXEN_ERR_INIT;
	}

	g_backend      = backend;
	g_initialized  = true;
	g_window_count = 0;
	g_focused_window = NULL;
	g_should_quit  = false;
	/* Reset drag state for symmetry with shutdown. BSS-init guarantees this
	 * on first call, but explicit reset handles the re-init-after-error case
	 * where an embedder calls init() without a prior shutdown(). */
	g_drag.state       = DRAG_NONE;
	g_drag.win         = NULL;
	g_drag.was_pressed = false;
	boxen__set_last_error(NULL);

	return BOXEN_OK;
}

void boxen_shutdown(void) {
	if (!g_initialized) {
		return;
	}

	/* Close all open windows in reverse order to avoid swap-with-last skipping.
	 * Zero the magic field before free so a stale caller pointer to a freed
	 * window struct is rejected by find_live_window_index (if the heap
	 * memory still reads back the zeroed magic). Matches the close-path
	 * mark-dead step. */
	while (g_window_count > 0) {
		boxen_window_t *w = g_windows[g_window_count - 1];
		w->magic = 0;
		free(w->title);
		free(w);
		g_window_count--;
	}

	if (g_backend != NULL) {
		g_backend->shutdown();
	}

	g_backend        = NULL;
	g_initialized    = false;
	g_focused_window = NULL;
	g_should_quit    = false;

	/* Reset drag state so a re-init starts clean. */
	g_drag.state = DRAG_NONE;
	g_drag.win   = NULL;
}

/* -------------------------------------------------------------------------
 * Window lifecycle
 * ---------------------------------------------------------------------- */

/* Sanity cap on window dimensions to prevent signed-int overflow during
 * clip arithmetic (`win->rect.x + x` etc.) on pathological inputs. Real
 * terminals are bounded by COLS/LINES (typically <= a few hundred); this
 * bound is generous and catches misuse without limiting realistic usage. */
#define BOXEN_MAX_DIMENSION  16384

boxen_window_t *boxen_window_open(const char *title, boxen_rect_t rect,
                                  void *user_data) {
	if (!g_initialized) {
		boxen__set_last_error("boxen not initialized");
		return NULL;
	}
	if (g_window_count >= BOXEN_MAX_WINDOWS) {
		boxen__set_last_error("window count limit reached");
		return NULL;
	}
	if (rect.w < 0 || rect.h < 0 ||
	    rect.w > BOXEN_MAX_DIMENSION || rect.h > BOXEN_MAX_DIMENSION ||
	    rect.x < -BOXEN_MAX_DIMENSION || rect.x > BOXEN_MAX_DIMENSION ||
	    rect.y < -BOXEN_MAX_DIMENSION || rect.y > BOXEN_MAX_DIMENSION) {
		boxen__set_last_error("window rect out of bounds");
		return NULL;
	}

	boxen_window_t *w = (boxen_window_t *)calloc(1, sizeof(boxen_window_t));
	if (w == NULL) {
		boxen__set_last_error("allocation failed");
		return NULL;
	}

	/* strdup may fail under OOM; if the caller asked for a title, we must
	 * not silently leave it as NULL. Free the struct and surface the error. */
	if (title != NULL) {
		w->title = strdup(title);
		if (w->title == NULL) {
			free(w);
			boxen__set_last_error("title allocation failed");
			return NULL;
		}
	} else {
		w->title = NULL;
	}
	w->magic     = BOXEN_WINDOW_MAGIC;
	w->rect      = rect;
	w->user_data = user_data;

	/* Initialize fields for future phases to known defaults. */
	w->z_index       = g_window_count;
	w->scroll_x      = 0;
	w->scroll_y      = 0;
	w->content_w     = 0;
	w->content_h     = 0;
	w->highlight_row = -1;

	g_windows[g_window_count++] = w;
	return w;
}

void boxen_window_close(boxen_window_t *win) {
	if (win == NULL) {
		return;
	}
	/* Magic-field guard: catches double-close and stale-pointer reuse for
	 * the common case (already-closed window struct, freed memory not yet
	 * reallocated). Not bulletproof -- a freed-then-realloc'd-as-window
	 * struct could match -- but cheap enough to be worth doing. */
	if (win->magic != BOXEN_WINDOW_MAGIC) {
		boxen__set_last_error("close of non-live window (double-close or stale pointer)");
		return;
	}

	/* Find the window in the list. */
	int idx = -1;
	for (int i = 0; i < g_window_count; i++) {
		if (g_windows[i] == win) {
			idx = i;
			break;
		}
	}
	if (idx < 0) {
		/* Not in our list -- caller error; handle gracefully. */
		return;
	}

	/* Swap with last and decrement. */
	g_windows[idx] = g_windows[--g_window_count];
	g_windows[g_window_count] = NULL;

	/* Clear focus pointer if this was the focused window. */
	if (g_focused_window == win) {
		g_focused_window = NULL;
	}

	/* Clear drag state if this window is the drag target. Without this,
	 * the next mouse-motion or arrow-key event would call drag_*_update
	 * with g_drag.win pointing at freed memory -- UAF write. Mirrors the
	 * focus-clear pattern above. */
	if (g_drag.win == win) {
		g_drag.state = DRAG_NONE;
		g_drag.win   = NULL;
	}

	win->magic = 0;  /* Mark as dead BEFORE freeing so a use-after-free read
	                    sees a non-magic value if memory is read back. */
	free(win->title);
	free(win);
}

/* -------------------------------------------------------------------------
 * Window property accessors
 * ---------------------------------------------------------------------- */

void boxen_window_set_rect(boxen_window_t *win, boxen_rect_t rect) {
	if (win == NULL) return;
	if (rect.w < 0 || rect.h < 0 ||
	    rect.w > BOXEN_MAX_DIMENSION || rect.h > BOXEN_MAX_DIMENSION ||
	    rect.x < -BOXEN_MAX_DIMENSION || rect.x > BOXEN_MAX_DIMENSION ||
	    rect.y < -BOXEN_MAX_DIMENSION || rect.y > BOXEN_MAX_DIMENSION) {
		boxen__set_last_error("window rect out of bounds");
		return;
	}
	win->rect = rect;
}

void boxen_window_set_title(boxen_window_t *win, const char *title) {
	if (win == NULL) return;
	if (title == NULL) {
		free(win->title);
		win->title = NULL;
		return;
	}
	/* Transactional: strdup first, only free the old title on success.
	 * Under OOM the window retains its old title rather than ending up
	 * with NULL. */
	char *new_title = strdup(title);
	if (new_title == NULL) {
		boxen__set_last_error("title allocation failed");
		return;
	}
	free(win->title);
	win->title = new_title;
}

void boxen_window_set_user_data(boxen_window_t *win, void *data) {
	if (win == NULL) return;
	win->user_data = data;
}

void *boxen_window_get_user_data(const boxen_window_t *win) {
	if (win == NULL) return NULL;
	return win->user_data;
}

boxen_rect_t boxen_window_get_rect(const boxen_window_t *win) {
	if (win == NULL) {
		boxen_rect_t z = {0, 0, 0, 0};
		return z;
	}
	return win->rect;
}

/*
 * Content dimensions.
 *
 * At A.2 there is no chrome (borders, title bar, scrollbars), so content
 * dimensions equal the window rect. A.6 will subtract chrome here.
 */
int boxen_window_content_width(const boxen_window_t *win) {
	if (win == NULL) return 0;
	return win->rect.w;
}

int boxen_window_content_height(const boxen_window_t *win) {
	if (win == NULL) return 0;
	return win->rect.h;
}

/* A.4: resize/move property setters.
 * Use find_live_window_index guard to reject stale/NULL/non-live pointers.
 * The min_w/min_h default is 0 (from calloc), which the drag/clamp code
 * interprets as 1 (so a window can always be made at least 1x1). Callers
 * that want a larger minimum must call set_min_size explicitly. */
void boxen_window_set_resizable(boxen_window_t *win, bool resizable) {
	if (find_live_window_index(win) < 0) return;
	win->resizable = resizable;
}

void boxen_window_set_movable(boxen_window_t *win, bool movable) {
	if (find_live_window_index(win) < 0) return;
	win->movable = movable;
}

void boxen_window_set_min_size(boxen_window_t *win, int min_w, int min_h) {
	if (find_live_window_index(win) < 0) return;
	/* Coerce non-positive to 1 at write time so read sites don't need to
	 * compensate. Stale negative values were silently coerced at read
	 * time previously; storing the canonical form is cheaper and clearer. */
	if (min_w < 1) min_w = 1;
	if (min_h < 1) min_h = 1;
	if (min_w > BOXEN_MAX_DIMENSION) min_w = BOXEN_MAX_DIMENSION;
	if (min_h > BOXEN_MAX_DIMENSION) min_h = BOXEN_MAX_DIMENSION;
	win->min_w = min_w;
	win->min_h = min_h;
}

void boxen_window_set_pinned(boxen_window_t *win, boxen_pin_edge_t edge) {
	/* Consistent with A.4 pattern on the other window mutators: reject
	 * stale/foreign/NULL pointers via find_live_window_index. */
	if (find_live_window_index(win) < 0) return;
	win->pinned = edge;
}

/* -------------------------------------------------------------------------
 * Z-order: raise / lower / focus
 *
 * The window list is ordered bottom-to-top: index 0 is the backmost window,
 * index g_window_count-1 is the frontmost. raise() shifts a window to the
 * top (last position); lower() shifts it to the bottom (index 0). Both
 * preserve relative order of the other windows via a single-element shift.
 * ---------------------------------------------------------------------- */

/* Returns the index of win in g_windows[] if win is a live, in-list window,
 * or -1 otherwise. Used by all the window mutators to reject stale, foreign,
 * or post-shutdown pointers BEFORE writing anything -- without this guard a
 * caller passing a freed or unrelated pointer could plant a dangling pointer
 * in g_focused_window (UAF on next dispatch) or stomp arbitrary memory via
 * a stray field write. The magic-field check is best-effort; the list-scan
 * is authoritative. */
static int find_live_window_index(const boxen_window_t *win) {
	if (win == NULL) return -1;
	if (win->magic != BOXEN_WINDOW_MAGIC) return -1;
	for (int i = 0; i < g_window_count; i++) {
		if (g_windows[i] == win) return i;
	}
	return -1;
}

/* -------------------------------------------------------------------------
 * A.4 hit-testing
 *
 * Given a screen position (sx, sy), classify the click relative to a window:
 *
 *   HIT_NONE      -- outside the window entirely
 *   HIT_CORNER_TL -- single-cell at the top-left corner (rect.x, rect.y)
 *   HIT_CORNER_TR -- single-cell at the top-right corner (rect.x+w-1, rect.y)
 *   HIT_CORNER_BL -- single-cell at the bottom-left corner (rect.x, rect.y+h-1)
 *   HIT_CORNER_BR -- single-cell at the bottom-right corner (rect.x+w-1, rect.y+h-1)
 *   HIT_TITLE     -- top row (y == rect.y) excluding the two corner cells
 *   HIT_BODY      -- everything else inside the window
 *
 * Chrome is not drawn at A.4 (that is A.6), but this geometry holds because
 * the title row / corner cells are the universally-expected hit regions for
 * a terminal window manager whether or not chrome is drawn.
 * ---------------------------------------------------------------------- */

static int hit_test_window(const boxen_window_t *win, int sx, int sy) {
	if (win == NULL) return HIT_NONE;
	int x0 = win->rect.x;
	int y0 = win->rect.y;
	int x1 = win->rect.x + win->rect.w - 1;  /* right edge column */
	int y1 = win->rect.y + win->rect.h - 1;  /* bottom edge row */

	/* Outside entirely */
	if (sx < x0 || sx > x1 || sy < y0 || sy > y1) return HIT_NONE;

	/* Corner cells (highest priority -- checked before title/body) */
	if (sx == x0 && sy == y0) return HIT_CORNER_TL;
	if (sx == x1 && sy == y0) return HIT_CORNER_TR;
	if (sx == x0 && sy == y1) return HIT_CORNER_BL;
	if (sx == x1 && sy == y1) return HIT_CORNER_BR;

	/* Title row: topmost row, excluding corners already handled above */
	if (sy == y0) return HIT_TITLE;

	return HIT_BODY;
}

/* -------------------------------------------------------------------------
 * A.4 terminal-resize clamping
 *
 * Called when a BOXEN_EV_RESIZE event arrives. Iterates every window and
 * ensures its rect fits within the new (new_w x new_h) terminal:
 *
 *   1. Shrink w to fit if rect.x + w > new_w. Floor at min_w (default 1).
 *   2. If rect.x >= new_w (window entirely off screen), shift left until
 *      the left edge is just inside the terminal.
 *   3. Same logic for h / rect.y / min_h.
 *
 * min_w and min_h default to 1 when not set (zero-valued from calloc).
 * ---------------------------------------------------------------------- */

static void clamp_windows_to_terminal(int new_w, int new_h) {
	/* Defensive: an untrusted backend or synthetic event could supply
	 * pathological dimensions. Cap at BOXEN_MAX_DIMENSION and floor at 0
	 * before deriving rect-shift arithmetic that would otherwise overflow. */
	if (new_w < 0) new_w = 0;
	if (new_w > BOXEN_MAX_DIMENSION) new_w = BOXEN_MAX_DIMENSION;
	if (new_h < 0) new_h = 0;
	if (new_h > BOXEN_MAX_DIMENSION) new_h = BOXEN_MAX_DIMENSION;

	for (int i = 0; i < g_window_count; i++) {
		boxen_window_t *w = g_windows[i];
		if (w == NULL) continue;

		int min_w = (w->min_w > 0) ? w->min_w : 1;
		int min_h = (w->min_h > 0) ? w->min_h : 1;

		/* Horizontal: shrink width first, then shift origin if needed. */
		if (w->rect.w > new_w) w->rect.w = new_w;
		if (w->rect.w < min_w) w->rect.w = min_w;
		if (w->rect.x + w->rect.w > new_w) {
			w->rect.x = new_w - w->rect.w;
		}
		if (w->rect.x < 0) w->rect.x = 0;

		/* Vertical: same pattern. */
		if (w->rect.h > new_h) w->rect.h = new_h;
		if (w->rect.h < min_h) w->rect.h = min_h;
		if (w->rect.y + w->rect.h > new_h) {
			w->rect.y = new_h - w->rect.h;
		}
		if (w->rect.y < 0) w->rect.y = 0;

		/* Final invariant restoration. */
		clamp_rect_to_bounds(&w->rect);
	}
}

/* -------------------------------------------------------------------------
 * A.4 drag update helpers
 * ---------------------------------------------------------------------- */

/* Clamp a rect's fields to BOXEN_MAX_DIMENSION bounds (A.2 invariant).
 * boxen_window_set_rect enforces this on the public path; A.4's drag and
 * keyboard-step paths write rect fields directly, bypassing that check, so
 * this helper restores the invariant at the end of every direct write.
 * Without this, an attacker-controlled or buggy mouse stream could drive
 * a window dimension to multi-GB values and cause signed-int overflow in
 * downstream arithmetic (hit_test rect.x + rect.w - 1, etc.). */
static void clamp_rect_to_bounds(boxen_rect_t *r) {
	if (r->x < -BOXEN_MAX_DIMENSION) r->x = -BOXEN_MAX_DIMENSION;
	if (r->x >  BOXEN_MAX_DIMENSION) r->x =  BOXEN_MAX_DIMENSION;
	if (r->y < -BOXEN_MAX_DIMENSION) r->y = -BOXEN_MAX_DIMENSION;
	if (r->y >  BOXEN_MAX_DIMENSION) r->y =  BOXEN_MAX_DIMENSION;
	if (r->w < 0) r->w = 0;
	if (r->w > BOXEN_MAX_DIMENSION) r->w = BOXEN_MAX_DIMENSION;
	if (r->h < 0) r->h = 0;
	if (r->h > BOXEN_MAX_DIMENSION) r->h = BOXEN_MAX_DIMENSION;
}

/* Apply a move drag delta to the active window. Final position is clamped
 * to BOXEN_MAX_DIMENSION bounds; terminal-extent clamping is NOT applied
 * here so a user can briefly drag a window off-screen before releasing
 * (matches typical TUI/desktop window manager behavior). */
static void drag_move_update(int sx, int sy) {
	if (g_drag.win == NULL) return;
	int dx = sx - g_drag.anchor_x;
	int dy = sy - g_drag.anchor_y;
	int new_x = g_drag.start_rect.x + dx;
	int new_y = g_drag.start_rect.y + dy;
	g_drag.win->rect.x = new_x;
	g_drag.win->rect.y = new_y;
	clamp_rect_to_bounds(&g_drag.win->rect);
}

/* Apply a resize drag delta to the active window for a BR corner drag.
 * Only BR is implemented for A.4; TL/TR/BL expansion is straightforward
 * using the same pattern if needed for A.6+. */
static void drag_resize_update(int sx, int sy) {
	if (g_drag.win == NULL) return;
	boxen_window_t *w  = g_drag.win;
	int dx = sx - g_drag.anchor_x;
	int dy = sy - g_drag.anchor_y;

	int min_w = (w->min_w > 0) ? w->min_w : 1;
	int min_h = (w->min_h > 0) ? w->min_h : 1;

	int new_x = g_drag.start_rect.x;
	int new_y = g_drag.start_rect.y;
	int new_w = g_drag.start_rect.w;
	int new_h = g_drag.start_rect.h;

	switch (g_drag.corner) {
	case 0: /* BR: grow/shrink right and bottom; origin fixed */
		new_w = g_drag.start_rect.w + dx;
		new_h = g_drag.start_rect.h + dy;
		break;
	case 1: /* BL: grow/shrink left and bottom; right edge and top fixed */
		new_x = g_drag.start_rect.x + dx;
		new_w = g_drag.start_rect.w - dx;
		new_h = g_drag.start_rect.h + dy;
		break;
	case 2: /* TR: grow/shrink right and top; left edge and bottom fixed */
		new_y = g_drag.start_rect.y + dy;
		new_w = g_drag.start_rect.w + dx;
		new_h = g_drag.start_rect.h - dy;
		break;
	case 3: /* TL: all four change */
		new_x = g_drag.start_rect.x + dx;
		new_y = g_drag.start_rect.y + dy;
		new_w = g_drag.start_rect.w - dx;
		new_h = g_drag.start_rect.h - dy;
		break;
	default:
		break;
	}

	/* Clamp to minimum size. For non-BR corners, adjust origin to keep the
	 * fixed edge in place when the minimum kicks in. */
	if (new_w < min_w) {
		if (g_drag.corner == 1 || g_drag.corner == 3) {
			/* BL or TL: left edge was moving; fix the right edge. */
			new_x = g_drag.start_rect.x + g_drag.start_rect.w - min_w;
		}
		new_w = min_w;
	}
	if (new_h < min_h) {
		if (g_drag.corner == 2 || g_drag.corner == 3) {
			/* TR or TL: top edge was moving; fix the bottom edge. */
			new_y = g_drag.start_rect.y + g_drag.start_rect.h - min_h;
		}
		new_h = min_h;
	}

	w->rect.x = new_x;
	w->rect.y = new_y;
	w->rect.w = new_w;
	w->rect.h = new_h;
	clamp_rect_to_bounds(&w->rect);
}

void boxen_window_raise(boxen_window_t *win) {
	if (g_window_count <= 1) return;
	int idx = find_live_window_index(win);
	if (idx < 0 || idx == g_window_count - 1) return;  /* not found or already top */

	/* Shift everything above idx down by one, place win at the top. */
	for (int i = idx; i < g_window_count - 1; i++) {
		g_windows[i] = g_windows[i + 1];
	}
	g_windows[g_window_count - 1] = win;
}

void boxen_window_lower(boxen_window_t *win) {
	if (g_window_count <= 1) return;
	int idx = find_live_window_index(win);
	if (idx < 0 || idx == 0) return;  /* not found or already bottom */

	/* Shift everything below idx up by one, place win at index 0. */
	for (int i = idx; i > 0; i--) {
		g_windows[i] = g_windows[i - 1];
	}
	g_windows[0] = win;
}

void boxen_window_focus(boxen_window_t *win) {
	/* Validate BEFORE writing anything. Without this, a caller passing a
	 * stale pointer could leave g_focused_window pointing at freed memory,
	 * which boxen_dispatch_event would then deref on the next key/resize
	 * event -- UAF. */
	if (find_live_window_index(win) < 0) return;

	/* Clear focused flag on all windows, then set it on win. */
	for (int i = 0; i < g_window_count; i++) {
		if (g_windows[i] != NULL) {
			g_windows[i]->focused = false;
		}
	}
	win->focused     = true;
	g_focused_window = win;

	/* Raise the newly focused window to the top. */
	boxen_window_raise(win);
}

void boxen_window_set_modal(boxen_window_t *win, bool modal) {
	if (find_live_window_index(win) < 0) return;
	win->modal = modal;
}

/* boxen_window_invalidate: in the A.3 always-redraw model, every present()
 * call redraws all windows regardless. This function is a no-op API surface
 * for the future case where dirty-rect tracking is added (A.N+). */
void boxen_window_invalidate(boxen_window_t *win) {
	(void)win;
	/* no-op: always-redraw model in A.3 */
}

void boxen_window_set_draw(boxen_window_t *win, boxen_draw_fn fn) {
	if (find_live_window_index(win) < 0) return;
	win->draw_fn = fn;
}

void boxen_window_set_input(boxen_window_t *win, boxen_input_fn fn) {
	if (find_live_window_index(win) < 0) return;
	win->input_fn = fn;
}

/* A.5 stubs */
void boxen_window_set_content_size(boxen_window_t *win, int w, int h) {
	if (win != NULL) { win->content_w = w; win->content_h = h; }
}
void boxen_window_set_scroll(boxen_window_t *win, int x, int y) {
	if (win != NULL) { win->scroll_x = x; win->scroll_y = y; }
}
void boxen_window_get_scroll(const boxen_window_t *win, int *x, int *y) {
	if (win == NULL) { if (x) *x = 0; if (y) *y = 0; return; }
	if (x) *x = win->scroll_x;
	if (y) *y = win->scroll_y;
}
void boxen_window_scroll_by(boxen_window_t *win, int dx, int dy) {
	if (win != NULL) { win->scroll_x += dx; win->scroll_y += dy; }
}
void boxen_window_ensure_visible(boxen_window_t *win, int cx, int cy) {
	(void)win; (void)cx; (void)cy;
}
void boxen_window_set_row_highlight(boxen_window_t *win, int content_row,
                                    uint8_t highlight_attr,
                                    boxen_color_t fg, boxen_color_t bg) {
	if (win == NULL) return;
	win->highlight_row  = content_row;
	win->highlight_attr = highlight_attr;
	win->highlight_fg   = fg;
	win->highlight_bg   = bg;
}

/* -------------------------------------------------------------------------
 * present -- back-to-front redraw + backend->present()
 *
 * Walks g_windows[0..g_window_count-1] (back to front) and calls each
 * window's draw_fn callback if: (a) draw_fn is non-NULL, and (b) the
 * window has non-zero width AND height. After all callbacks, calls
 * backend->present() once to flush the composited frame.
 * ---------------------------------------------------------------------- */

void boxen_present(void) {
	if (!g_initialized || g_backend == NULL) return;

	for (int i = 0; i < g_window_count; i++) {
		boxen_window_t *w = g_windows[i];
		if (w == NULL) continue;
		if (w->rect.w == 0 || w->rect.h == 0) continue;  /* skip degenerate/hidden */
		if (w->draw_fn != NULL) {
			w->draw_fn(w, w->user_data);
		}
	}

	g_backend->present();
}

/* -------------------------------------------------------------------------
 * poll_event -- raw event fetch from the backend
 *
 * Returns BOXEN_OK on event, BOXEN_ERR_TIMEOUT when timeout_ms elapses
 * with no event, or another negative error code on backend failure.
 * Does NOT dispatch the event -- call boxen_dispatch_event() for that.
 * ---------------------------------------------------------------------- */

boxen_result_t boxen_poll_event(boxen_event_t *out, int timeout_ms) {
	if (!g_initialized || g_backend == NULL) {
		boxen__set_last_error("boxen not initialized");
		return BOXEN_ERR_INVALID;
	}
	if (out == NULL) {
		boxen__set_last_error("out must not be NULL");
		return BOXEN_ERR_INVALID;
	}
	return g_backend->poll_event(out, timeout_ms);
}

/* -------------------------------------------------------------------------
 * dispatch_event -- route an event to the right window(s)
 *
 * Key events:
 *   If a modal window exists, dispatch to that window only.
 *   Otherwise dispatch to the focused window (if any).
 *   While in a keyboard drag mode (DRAG_MOVING_KB / DRAG_RESIZING_KB),
 *   arrow keys are intercepted to update the window rect; Enter/Escape
 *   end the mode. Non-arrow keys fall through to the focused window.
 *
 * Mouse events:
 *   If in a drag state, mouse events continue to drive the drag regardless
 *   of which window the pointer is over. This is "mouse capture" -- once a
 *   drag starts, all motion/release events belong to the drag until release.
 *   If a modal window exists and no drag is active, dispatch to the modal.
 *   Otherwise dispatch to the topmost window at (ev->mouse.x, ev->mouse.y).
 *
 *   Press on a movable window's title bar: enter DRAG_MOVING.
 *   Press on a resizable window's corner: enter DRAG_RESIZING.
 *   Motion while dragging: update rect (with min_size clamping for resize).
 *   Release: end the drag.
 *
 * Resize events (BOXEN_EV_RESIZE):
 *   Clamp all windows to the new terminal dimensions (A.4 behavior), then
 *   dispatch to the focused window so the application can react (e.g.,
 *   re-layout content). This replaces the A.3 stub that only passed the
 *   event to the focused window.
 * ---------------------------------------------------------------------- */

void boxen_dispatch_event(const boxen_event_t *ev) {
	if (ev == NULL || !g_initialized) return;

	/* Find the modal window, if any (last one with modal=true wins). */
	boxen_window_t *modal_win = NULL;
	for (int i = g_window_count - 1; i >= 0; i--) {
		if (g_windows[i] != NULL && g_windows[i]->modal) {
			modal_win = g_windows[i];
			break;
		}
	}

	boxen_window_t *target = NULL;

	switch (ev->type) {
	case BOXEN_EV_KEY:
		/* A.4 keyboard drag mode: intercept arrows + Enter/Esc.
		 * All rect mutations are followed by clamp_rect_to_bounds() to
		 * preserve the A.2 invariant. */
		if (g_drag.state == DRAG_MOVING_KB && g_drag.win != NULL) {
			int step = 1;
			boxen_window_t *kw = g_drag.win;
			if (ev->key.key == BOXEN_KEY_UP)    { kw->rect.y -= step; clamp_rect_to_bounds(&kw->rect); return; }
			if (ev->key.key == BOXEN_KEY_DOWN)  { kw->rect.y += step; clamp_rect_to_bounds(&kw->rect); return; }
			if (ev->key.key == BOXEN_KEY_LEFT)  { kw->rect.x -= step; clamp_rect_to_bounds(&kw->rect); return; }
			if (ev->key.key == BOXEN_KEY_RIGHT) { kw->rect.x += step; clamp_rect_to_bounds(&kw->rect); return; }
			if (ev->key.key == BOXEN_KEY_ENTER || ev->key.key == BOXEN_KEY_ESCAPE) {
				g_drag.state = DRAG_NONE;
				g_drag.win   = NULL;
				return;
			}
		} else if (g_drag.state == DRAG_RESIZING_KB && g_drag.win != NULL) {
			boxen_window_t *w   = g_drag.win;
			int min_w = (w->min_w > 0) ? w->min_w : 1;
			int min_h = (w->min_h > 0) ? w->min_h : 1;
			if (ev->key.key == BOXEN_KEY_LEFT) {
				if (w->rect.w > min_w) w->rect.w--;
				return;
			}
			if (ev->key.key == BOXEN_KEY_RIGHT) { w->rect.w++; clamp_rect_to_bounds(&w->rect); return; }
			if (ev->key.key == BOXEN_KEY_UP) {
				if (w->rect.h > min_h) w->rect.h--;
				return;
			}
			if (ev->key.key == BOXEN_KEY_DOWN) { w->rect.h++; clamp_rect_to_bounds(&w->rect); return; }
			if (ev->key.key == BOXEN_KEY_ENTER || ev->key.key == BOXEN_KEY_ESCAPE) {
				g_drag.state = DRAG_NONE;
				g_drag.win   = NULL;
				return;
			}
		}

		/* Ctrl-M on focused movable window: enter keyboard move mode. */
		if (ev->key.key == BOXEN_KEY_CTRL_M && g_focused_window != NULL &&
		    g_focused_window->movable) {
			g_drag.state = DRAG_MOVING_KB;
			g_drag.win   = g_focused_window;
			return;
		}
		/* Ctrl-R on focused resizable window: enter keyboard resize mode. */
		if (ev->key.key == BOXEN_KEY_CTRL_R && g_focused_window != NULL &&
		    g_focused_window->resizable) {
			g_drag.state = DRAG_RESIZING_KB;
			g_drag.win   = g_focused_window;
			return;
		}

		target = (modal_win != NULL) ? modal_win : g_focused_window;
		break;

	case BOXEN_EV_MOUSE:
		/* Mouse capture: if a drag is active, all mouse events go to the
		 * drag state machine regardless of where the pointer is.
		 *
		 * Event taxonomy under termbox2 (and our mock):
		 *   pressed=true               -> button still held (drag continues)
		 *   pressed=false, button==0   -> motion event with no button change
		 *                                 (treated as drag continuation)
		 *   pressed=false, button!=0   -> button release (drag ends)
		 * All three cases apply the same final-position update; the release
		 * case additionally clears the drag state. Consolidated to one
		 * update call to avoid the repeated-branch-body pattern flagged by
		 * static analysis. */
		if (g_drag.state == DRAG_MOVING || g_drag.state == DRAG_RESIZING) {
			if (g_drag.state == DRAG_MOVING) {
				drag_move_update(ev->mouse.x, ev->mouse.y);
			} else {
				drag_resize_update(ev->mouse.x, ev->mouse.y);
			}
			/* End the drag on a press-to-release transition. We track
			 * g_drag.was_pressed across events so we end the drag whether
			 * the backend encodes the release as (pressed=false, button!=0)
			 * or (pressed=false, button==0) -- some terminals lose button
			 * info on release. The press that started the drag set
			 * was_pressed=true; any subsequent pressed=false ends it. */
			if (g_drag.was_pressed && !ev->mouse.pressed) {
				g_drag.state       = DRAG_NONE;
				g_drag.win         = NULL;
				g_drag.was_pressed = false;
			} else if (ev->mouse.pressed) {
				g_drag.was_pressed = true;
			}
			return;  /* drag consumes the event */
		}

		/* No active drag. Check if this press starts one. */
		if (ev->mouse.pressed && modal_win == NULL) {
			/* Find the topmost window at the press location. */
			int cx, cy;
			boxen_window_t *hit_win = boxen_window_at(ev->mouse.x, ev->mouse.y, &cx, &cy);
			if (hit_win != NULL) {
				int region = hit_test_window(hit_win, ev->mouse.x, ev->mouse.y);
				if (region == HIT_TITLE && hit_win->movable) {
					/* Start a mouse move drag. */
					g_drag.state       = DRAG_MOVING;
					g_drag.win         = hit_win;
					g_drag.anchor_x    = ev->mouse.x;
					g_drag.anchor_y    = ev->mouse.y;
					g_drag.start_rect  = hit_win->rect;
					g_drag.was_pressed = true;  /* press that started us */
					return;  /* drag start consumes the press */
				}
				if (hit_win->resizable) {
					int corner = -1;
					if (region == HIT_CORNER_BR) corner = 0;
					else if (region == HIT_CORNER_BL) corner = 1;
					else if (region == HIT_CORNER_TR) corner = 2;
					else if (region == HIT_CORNER_TL) corner = 3;
					if (corner >= 0) {
						g_drag.state       = DRAG_RESIZING;
						g_drag.win         = hit_win;
						g_drag.anchor_x    = ev->mouse.x;
						g_drag.anchor_y    = ev->mouse.y;
						g_drag.start_rect  = hit_win->rect;
						g_drag.corner      = corner;
						g_drag.was_pressed = true;
						return;  /* drag start consumes the press */
					}
				}
			}
		}

		/* Standard dispatch: modal takes priority over at-point. */
		if (modal_win != NULL) {
			target = modal_win;
		} else {
			int cx, cy;
			target = boxen_window_at(ev->mouse.x, ev->mouse.y, &cx, &cy);
		}
		break;

	case BOXEN_EV_RESIZE:
		/* A.4: clamp all windows to the new terminal dimensions, then
		 * dispatch the event to the focused window for app-level handling.
		 *
		 * Cancel any active drag: g_drag.start_rect was captured before
		 * the resize and clamping may have shifted/shrunk the target
		 * window, so subsequent motion events would compute deltas against
		 * stale geometry and cause a visible jump. Safer to drop the drag
		 * and let the user re-start. */
		if (g_drag.state != DRAG_NONE) {
			g_drag.state       = DRAG_NONE;
			g_drag.win         = NULL;
			g_drag.was_pressed = false;
		}
		clamp_windows_to_terminal(ev->resize.w, ev->resize.h);
		target = g_focused_window;
		break;

	default:
		break;
	}

	if (target != NULL && target->input_fn != NULL) {
		target->input_fn(target, ev, target->user_data);
	}
}

/* -------------------------------------------------------------------------
 * run / quit -- convenience event loop
 *
 * Polls with a 100 ms timeout, dispatches via boxen_dispatch_event, then
 * calls boxen_present. Exits when g_should_quit is true (set by boxen_quit).
 * ---------------------------------------------------------------------- */

void boxen_run(void) {
	g_should_quit = false;

	while (!g_should_quit) {
		boxen_event_t ev;
		boxen_result_t r = boxen_poll_event(&ev, 100);
		if (r == BOXEN_OK) {
			boxen_dispatch_event(&ev);
		} else if (r != BOXEN_ERR_TIMEOUT) {
			/* Persistent backend failure (terminal detached, broken pipe,
			 * etc.). Without this break the loop would busy-spin at full
			 * CPU until external quit. Surface the error to the caller by
			 * exiting; standalone consumers can inspect via the log hook. */
			boxen__set_last_error("boxen_run: backend poll failed");
			break;
		}
		if (g_should_quit) break;
		boxen_present();
	}
}

void boxen_quit(void) {
	g_should_quit = true;
}

/* A.6 layout stubs */
void boxen_layout_split_h(boxen_rect_t total, float ratio,
                           const char *left_title,  boxen_window_t **left,
                           const char *right_title, boxen_window_t **right) {
	(void)total; (void)ratio; (void)left_title; (void)right_title;
	if (left)  *left  = NULL;
	if (right) *right = NULL;
}
void boxen_layout_split_v(boxen_rect_t total, float ratio,
                           const char *top_title,    boxen_window_t **top,
                           const char *bottom_title, boxen_window_t **bottom) {
	(void)total; (void)ratio; (void)top_title; (void)bottom_title;
	if (top)    *top    = NULL;
	if (bottom) *bottom = NULL;
}

/* -------------------------------------------------------------------------
 * set_cell -- window-local (x, y) -> terminal coords; clip at window border
 * ---------------------------------------------------------------------- */

void boxen_set_cell(boxen_window_t *win, int x, int y,
                    uint32_t ch, uint16_t fg, uint16_t bg, uint16_t attr) {
	if (win == NULL || g_backend == NULL) return;

	/* Clip against window content area (no chrome at A.2). */
	if (x < 0 || x >= win->rect.w) return;
	if (y < 0 || y >= win->rect.h) return;

	int tx = win->rect.x + x;
	int ty = win->rect.y + y;

	g_backend->set_cell(tx, ty, ch, fg, bg, attr);
}

/* -------------------------------------------------------------------------
 * Minimal UTF-8 decoder
 *
 * Decodes one Unicode codepoint from *p and advances *p past it.
 * Returns the codepoint, or 0xFFFD on invalid byte, or 0 at end-of-string.
 *
 * Handles 1-, 2-, 3-, and 4-byte sequences. Invalid lead bytes or
 * truncated sequences return 0xFFFD and advance by 1 byte.
 *
 * REQUIRES: *p points at a NUL-terminated byte sequence. The decoder
 * stops short of reading past the terminator because the NUL byte fails
 * the continuation-byte check (`(byte & 0xC0) != 0x80` is true for NUL)
 * before any deeper read. For non-NUL-terminated buffers (e.g. binary
 * slices from a network read), a length-bounded decoder variant is
 * needed -- not provided in A.2; file an issue when first required.
 *
 * Display-relaxed: this decoder is intended for rendering caller-
 * supplied UTF-8, not input validation. It accepts overlong encodings
 * (e.g. "\xC0\x80" -> U+0000) and surrogate codepoints (U+D800-U+DFFF).
 * Strict input validation must happen before draw_text is called.
 * ---------------------------------------------------------------------- */

static uint32_t utf8_next(const unsigned char **p) {
	uint32_t cp;
	unsigned char c = **p;

	if (c == 0) return 0;  /* end of string */

	if (c < 0x80) {
		/* 1-byte: 0xxxxxxx */
		(*p)++;
		return c;
	} else if ((c & 0xE0) == 0xC0) {
		/* 2-byte: 110xxxxx 10xxxxxx */
		if (((*p)[1] & 0xC0) != 0x80) { (*p)++; return 0xFFFD; }
		cp = ((uint32_t)(c & 0x1F) << 6) | ((*p)[1] & 0x3F);
		*p += 2;
		return cp;
	} else if ((c & 0xF0) == 0xE0) {
		/* 3-byte: 1110xxxx 10xxxxxx 10xxxxxx */
		if (((*p)[1] & 0xC0) != 0x80 || ((*p)[2] & 0xC0) != 0x80) {
			(*p)++;
			return 0xFFFD;
		}
		cp = ((uint32_t)(c & 0x0F) << 12) |
		     ((uint32_t)((*p)[1] & 0x3F) << 6) |
		     ((*p)[2] & 0x3F);
		*p += 3;
		return cp;
	} else if ((c & 0xF8) == 0xF0) {
		/* 4-byte: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
		if (((*p)[1] & 0xC0) != 0x80 || ((*p)[2] & 0xC0) != 0x80 ||
		    ((*p)[3] & 0xC0) != 0x80) {
			(*p)++;
			return 0xFFFD;
		}
		cp = ((uint32_t)(c & 0x07) << 18) |
		     ((uint32_t)((*p)[1] & 0x3F) << 12) |
		     ((uint32_t)((*p)[2] & 0x3F) << 6) |
		     ((*p)[3] & 0x3F);
		*p += 4;
		return cp;
	} else {
		/* Invalid lead byte */
		(*p)++;
		return 0xFFFD;
	}
}

/* -------------------------------------------------------------------------
 * draw_text -- decode UTF-8, write each codepoint advancing x by wcwidth
 * ---------------------------------------------------------------------- */

void boxen_draw_text(boxen_window_t *win, int x, int y,
                     const char *utf8, uint16_t fg, uint16_t bg, uint16_t attr) {
	if (win == NULL || utf8 == NULL || g_backend == NULL) return;

	const unsigned char *p = (const unsigned char *)utf8;
	int cx = x;

	while (*p != 0) {
		uint32_t cp = utf8_next(&p);
		if (cp == 0) break;

		/* Stop advancing x if we are already past the right edge. */
		if (cx >= win->rect.w) break;

		/* set_cell clips; no extra check needed for left/top/bottom. */
		boxen_set_cell(win, cx, y, cp, fg, bg, attr);

		int w = boxen_wcwidth(cp);
		if (w < 1) w = 1;  /* treat zero-width / control as 1 column advance */
		cx += w;
	}
}

/* -------------------------------------------------------------------------
 * fill_rect -- fill a region with a single character
 * ---------------------------------------------------------------------- */

void boxen_fill_rect(boxen_window_t *win, boxen_rect_t r,
                     uint32_t ch, uint16_t fg, uint16_t bg, uint16_t attr) {
	if (win == NULL || g_backend == NULL) return;

	for (int row = 0; row < r.h; row++) {
		for (int col = 0; col < r.w; col++) {
			boxen_set_cell(win, r.x + col, r.y + row, ch, fg, bg, attr);
		}
	}
}

/* -------------------------------------------------------------------------
 * boxen_window_at -- screen-to-window-local coordinate mapping
 *
 * Iterates the window list from last (topmost in z-order) to first (bottommost).
 * Returns the first window whose rect contains (sx, sy).
 * If cx/cy are non-NULL, fills them with window-local coords.
 * ---------------------------------------------------------------------- */

boxen_window_t *boxen_window_at(int sx, int sy, int *cx, int *cy) {
	/* Scan from last (topmost in z-order) to first (bottommost). A.3 raise/lower
	 * operations maintain g_windows[g_window_count-1] as the topmost window. */
	for (int i = g_window_count - 1; i >= 0; i--) {
		boxen_window_t *w = g_windows[i];
		/* Defensive: slots 0..g_window_count-1 are invariant non-NULL
		 * via the open/close protocol, but a NULL guard costs nothing
		 * and survives future refactors. */
		if (w == NULL) continue;
		if (sx >= w->rect.x && sx < w->rect.x + w->rect.w &&
		    sy >= w->rect.y && sy < w->rect.y + w->rect.h) {
			if (cx != NULL) *cx = sx - w->rect.x;
			if (cy != NULL) *cy = sy - w->rect.y;
			return w;
		}
	}
	return NULL;
}
