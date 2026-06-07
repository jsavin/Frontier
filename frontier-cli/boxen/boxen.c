/*
 * boxen.c -- window primitive, clipping, and library lifecycle (A.2).
 *
 * Implements:
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
 * Out of scope at A.2 (stubs return gracefully):
 *   z-order (raise/lower/focus) -- A.3
 *   redraw / present / poll_event -- A.3
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

/* Window list -- simple fixed array; open appends, close swaps with last. */
static boxen_window_t        *g_windows[BOXEN_MAX_WINDOWS];
static int                    g_window_count = 0;

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
	boxen__set_last_error(NULL);

	return BOXEN_OK;
}

void boxen_shutdown(void) {
	if (!g_initialized) {
		return;
	}

	/* Close all open windows in reverse order to avoid swap-with-last skipping. */
	while (g_window_count > 0) {
		boxen_window_t *w = g_windows[g_window_count - 1];
		free(w->title);
		free(w);
		g_window_count--;
	}

	if (g_backend != NULL) {
		g_backend->shutdown();
	}

	g_backend     = NULL;
	g_initialized = false;
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

/* Phase C stubs -- declared in boxen.h; no-ops at A.2. */
void boxen_window_set_resizable(boxen_window_t *win, bool resizable) {
	if (win != NULL) win->resizable = resizable;
}

void boxen_window_set_movable(boxen_window_t *win, bool movable) {
	if (win != NULL) win->movable = movable;
}

void boxen_window_set_min_size(boxen_window_t *win, int min_w, int min_h) {
	if (win != NULL) { win->min_w = min_w; win->min_h = min_h; }
}

void boxen_window_set_pinned(boxen_window_t *win, boxen_pin_edge_t edge) {
	if (win != NULL) win->pinned = edge;
}

/* A.3 stubs */
void boxen_window_raise(boxen_window_t *win)              { (void)win; }
void boxen_window_lower(boxen_window_t *win)              { (void)win; }
void boxen_window_focus(boxen_window_t *win)              { (void)win; }
void boxen_window_set_modal(boxen_window_t *win, bool m)  { (void)win; (void)m; }
void boxen_window_invalidate(boxen_window_t *win)         { (void)win; }

void boxen_window_set_draw(boxen_window_t *win, boxen_draw_fn fn) {
	if (win != NULL) win->draw_fn = fn;
}
void boxen_window_set_input(boxen_window_t *win, boxen_input_fn fn) {
	if (win != NULL) win->input_fn = fn;
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

/* A.3 event loop stubs */
boxen_result_t boxen_poll_event(boxen_event_t *out, int timeout_ms) {
	(void)out; (void)timeout_ms;
	return BOXEN_ERR_INVALID;
}
void boxen_present(void)  {}
void boxen_run(void)      {}
void boxen_quit(void)     {}

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
 * Iterates the window list from last (topmost at A.2) to first (bottommost).
 * Returns the first window whose rect contains (sx, sy).
 * If cx/cy are non-NULL, fills them with window-local coords.
 * ---------------------------------------------------------------------- */

boxen_window_t *boxen_window_at(int sx, int sy, int *cx, int *cy) {
	/* Scan from last (topmost in insertion order) to first. */
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
