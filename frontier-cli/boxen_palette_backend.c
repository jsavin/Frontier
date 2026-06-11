/*
 * boxen_palette_backend.c -- 2026-06-10 JES #691 Phase C.0.3b: boxen-window
 * palette render backend.
 *
 * Implements palette_render_backend_t for the boxen TUI.  Renders the
 * slash-menu palette to a boxen_window_t using cell primitives instead of
 * pane-compositor ANSI sequences.
 *
 * Design contrast with the C.0.2 completion popup:
 *   - boxen_completion_popup: uses boxen_window_raise() (z-order only);
 *     deliberately does NOT call set_modal because key events must still
 *     reach the REPL input_fn.
 *   - THIS backend: calls boxen_window_set_modal(true) because the palette
 *     IS an input consumer.  The palette state machine needs every keystroke.
 *     set_modal gives it exclusive key capture.
 *
 * Color mapping
 * -------------
 * PALETTE_COLOR_* and BOXEN_COLOR_* share the same numeric values
 * (both are 16-color-aware, same index scheme).  Direct cast is safe.
 *
 * Attribute mapping
 * -----------------
 * PALETTE_ATTR_BOLD / DIM / UNDERLINE / INVERSE map to BOXEN_ATTR_BOLD /
 * DIM / UNDERLINE / REVERSE (different names, same bit positions are NOT
 * guaranteed; use explicit mapping instead of a cast).
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.  See pane.h for license text.
 */

#include "boxen_palette_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Color and attribute helpers
 * ---------------------------------------------------------------------- */

/* Map palette color index to boxen color.  Both share the same 0..16 range
 * (0 = default, 1..8 = standard, 9..16 = bright), so direct cast is safe.
 * Defined as a function to make the intent explicit and to allow future
 * remapping if the encodings diverge. */
static uint16_t pal_color_to_boxen(uint8_t c) {
	return (uint16_t)c;
}

/* Map palette attribute bitmask to boxen attribute bitmask.
 * The bit positions differ between PALETTE_ATTR_* and BOXEN_ATTR_* so an
 * explicit per-bit mapping is required rather than a cast. */
static uint16_t pal_attr_to_boxen(uint8_t pa) {
	uint16_t ba = BOXEN_ATTR_NONE;
	if (pa & PALETTE_ATTR_BOLD)      ba |= (uint16_t)BOXEN_ATTR_BOLD;
	if (pa & PALETTE_ATTR_DIM)       ba |= (uint16_t)BOXEN_ATTR_DIM;
	if (pa & PALETTE_ATTR_UNDERLINE) ba |= (uint16_t)BOXEN_ATTR_UNDERLINE;
	if (pa & PALETTE_ATTR_INVERSE)   ba |= (uint16_t)BOXEN_ATTR_REVERSE;
	return ba;
}

/* Draw a NUL-terminated string one character at a time to a boxen window at
 * content row `y`, starting at column `x`.  Clips silently at window edge. */
static void draw_str(boxen_window_t *win, int x, int y,
                     const char *s, uint8_t pal_fg, uint8_t pal_bg,
                     uint8_t pal_attr) {
	uint16_t fg   = pal_color_to_boxen(pal_fg);
	uint16_t bg   = pal_color_to_boxen(pal_bg);
	uint16_t attr = pal_attr_to_boxen(pal_attr);
	int cw = boxen_window_content_width(win);
	for (const char *p = s; *p && x < cw; ++p, ++x) {
		boxen_set_cell(win, x, y, (uint32_t)(unsigned char)*p, fg, bg, attr);
	}
}

/* Fill a horizontal run in a boxen window with a single character. */
static void fill_row(boxen_window_t *win, int x, int y, int w,
                     uint32_t ch, uint8_t pal_fg, uint8_t pal_bg,
                     uint8_t pal_attr) {
	if (w <= 0) return;
	uint16_t fg   = pal_color_to_boxen(pal_fg);
	uint16_t bg   = pal_color_to_boxen(pal_bg);
	uint16_t attr = pal_attr_to_boxen(pal_attr);
	int cw = boxen_window_content_width(win);
	for (int i = 0; i < w && (x + i) < cw; ++i) {
		boxen_set_cell(win, x + i, y, ch, fg, bg, attr);
	}
}

/* -------------------------------------------------------------------------
 * Per-backend instance context (file-static singleton)
 *
 * Per C.0.3a contract: the backend pointer (and its ctx) must remain valid
 * until palette_close.  We use a file-static ctx struct (one palette modal
 * at a time per process -- the GIL serialises this).
 * ---------------------------------------------------------------------- */

typedef struct {
	boxen_window_t       *modal_win;
	boxen_palette_done_cb done_cb;
	void                 *repl_state;
} boxen_backend_ctx_t;

static boxen_backend_ctx_t s_ctx;

/* -------------------------------------------------------------------------
 * Painting helpers
 * ---------------------------------------------------------------------- */

/* Palette color constants -- mirrors the PT_* defines in palette.c. */
#define BPB_FG_MENUBAR   PALETTE_COLOR_BRIGHT_CYAN
#define BPB_BG_MENUBAR   PALETTE_COLOR_BLUE
#define BPB_FG_SELECTED  PALETTE_COLOR_BLACK
#define BPB_BG_SELECTED  PALETTE_COLOR_WHITE
#define BPB_FG_HOTKEY    PALETTE_COLOR_BRIGHT_YELLOW
#define BPB_FG_DISABLED  PALETTE_COLOR_BRIGHT_BLACK

/* Paint the menubar strip onto row 0 of the modal window.
 *
 * Mirrors render_menubar() from palette.c but writes to boxen_set_cell
 * instead of pane_putc_color. */
static void paint_menubar(palette_state_t *st, boxen_window_t *win) {
	int cw = boxen_window_content_width(win);

	/* Background fill. */
	fill_row(win, 0, 0, cw, ' ',
	         BPB_FG_MENUBAR, BPB_BG_MENUBAR, 0);

	for (int i = 0; i < st->menu_count; ++i) {
		const char *label = st->menu_labels[i];
		int x   = st->menu_x[i];
		int w   = st->menu_w[i];
		bool sel = (i == st->menubar_cursor);
		uint8_t base    = sel ? PALETTE_ATTR_INVERSE : 0;
		uint8_t fg      = sel ? BPB_FG_SELECTED : BPB_FG_MENUBAR;
		uint8_t bg      = sel ? BPB_BG_SELECTED : BPB_BG_MENUBAR;
		char    hk      = st->menu_hotkeys[i];
		bool    hk_seen = false;

		/* Leading space. */
		if (x < cw) {
			boxen_set_cell(win, x, 0, ' ',
			               pal_color_to_boxen(fg), pal_color_to_boxen(bg),
			               pal_attr_to_boxen(base));
		}

		int lx = x + 1;
		for (const char *p = label; *p && lx < cw; ++p, ++lx) {
			uint8_t a        = base;
			uint8_t cell_fg  = fg;
			if (!hk_seen && hk && *p == hk) {
				a |= PALETTE_ATTR_BOLD;
				if (!sel) cell_fg = BPB_FG_HOTKEY;
				hk_seen = true;
			}
			boxen_set_cell(win, lx, 0,
			               (uint32_t)(unsigned char)*p,
			               pal_color_to_boxen(cell_fg),
			               pal_color_to_boxen(bg),
			               pal_attr_to_boxen(a));
		}

		/* Trailing space. */
		int tx = x + w - 1;
		if (tx >= 0 && tx < cw) {
			boxen_set_cell(win, tx, 0, ' ',
			               pal_color_to_boxen(fg), pal_color_to_boxen(bg),
			               pal_attr_to_boxen(base));
		}
	}
}

/* Compute logical x of item `i` inside a palette level strip.
 * Mirrors item_screen_x() from palette.c. */
static int bpb_item_x(const palette_level_t *lvl, int i) {
	int x = 2;
	for (int k = 0; k < i; ++k) {
		const palette_item_t *it = &lvl->items[k];
		int w = (it->is_separator) ? 1 : (int)strlen(it->label) + 4 + (it->checked ? 2 : 0);
		x += w + 2;
	}
	return x;
}

/* Paint one cascade level strip at content row `row`.
 *
 * Mirrors render_level() from palette.c but targets boxen_set_cell. */
static void paint_level(palette_state_t *st, int depth, boxen_window_t *win,
                        int row) {
	const palette_level_t *lvl = &st->levels[depth];
	int cw = boxen_window_content_width(win);
	int hs = lvl->hscroll;

	/* Background fill for the strip row. */
	fill_row(win, 0, row, cw, ' ',
	         BPB_FG_MENUBAR, BPB_BG_MENUBAR, 0);

	/* '<' indicator when content scrolled off the left. */
	if (hs > 0) {
		boxen_set_cell(win, 0, row, '<',
		               pal_color_to_boxen(PALETTE_COLOR_BRIGHT_WHITE),
		               pal_color_to_boxen(BPB_BG_MENUBAR),
		               pal_attr_to_boxen(PALETTE_ATTR_BOLD));
	}

	for (int i = 0; i < lvl->item_count; ++i) {
		const palette_item_t *it = &lvl->items[i];
		int lx_logical = bpb_item_x(lvl, i);

		/* Separator glyph. */
		if (it->is_separator) {
			int sx = lx_logical - hs;
			if (sx >= 0 && sx < cw) {
				boxen_set_cell(win, sx, row, '|',
				               pal_color_to_boxen(BPB_FG_DISABLED),
				               pal_color_to_boxen(BPB_BG_MENUBAR), 0);
			}
			continue;
		}

		bool sel  = (i == lvl->cursor);
		uint8_t base   = sel ? PALETTE_ATTR_INVERSE : 0;
		uint8_t fg     = sel ? BPB_FG_SELECTED : BPB_FG_MENUBAR;
		uint8_t bg     = sel ? BPB_BG_SELECTED : BPB_BG_MENUBAR;
		if (!it->enabled) base |= PALETTE_ATTR_DIM;
		char    hk     = it->shortcut;
		bool    hk_seen = false;

		/* Item width. */
		int item_w = (int)strlen(it->label) + 4 + (it->checked ? 2 : 0);

		/* Opening bracket (or space if not selected). */
		int bx0 = lx_logical - hs;
		if (bx0 >= 0 && bx0 < cw) {
			boxen_set_cell(win, bx0, row,
			               sel ? (uint32_t)'[' : (uint32_t)' ',
			               pal_color_to_boxen(fg), pal_color_to_boxen(bg),
			               pal_attr_to_boxen(base));
		}

		int cx = lx_logical + 1;
		/* Checkmark glyph (U+2714) or blank. */
		if (it->checked) {
			int sx = cx - hs;
			if (sx >= 0 && sx < cw) {
				boxen_set_cell(win, sx, row,
				               0x2714u /* HEAVY CHECK MARK */,
				               pal_color_to_boxen(fg), pal_color_to_boxen(bg),
				               pal_attr_to_boxen(base | PALETTE_ATTR_BOLD));
			}
			cx++;
			/* Space after checkmark. */
			sx = cx - hs;
			if (sx >= 0 && sx < cw) {
				boxen_set_cell(win, sx, row, ' ',
				               pal_color_to_boxen(fg), pal_color_to_boxen(bg),
				               pal_attr_to_boxen(base));
			}
			cx++;
		}

		/* Label characters. */
		for (const char *p = it->label; *p; ++p, ++cx) {
			int sx = cx - hs;
			if (sx < 0) continue;
			if (sx >= cw) break;

			uint8_t a       = base;
			uint8_t cell_fg = !it->enabled ? BPB_FG_DISABLED : fg;
			if (!hk_seen && hk && *p == hk) {
				a |= PALETTE_ATTR_BOLD;
				if (!sel && it->enabled) cell_fg = BPB_FG_HOTKEY;
				hk_seen = true;
			}
			boxen_set_cell(win, sx, row,
			               (uint32_t)(unsigned char)*p,
			               pal_color_to_boxen(cell_fg),
			               pal_color_to_boxen(bg),
			               pal_attr_to_boxen(a));
		}

		/* Space before closing bracket. */
		int sx_sp = cx - hs;
		if (sx_sp >= 0 && sx_sp < cw) {
			boxen_set_cell(win, sx_sp, row, ' ',
			               pal_color_to_boxen(fg), pal_color_to_boxen(bg),
			               pal_attr_to_boxen(base));
		}
		cx++;

		/* Closing bracket. */
		int bxN = lx_logical + item_w - 1 - hs;
		if (bxN >= 0 && bxN < cw) {
			boxen_set_cell(win, bxN, row,
			               sel ? (uint32_t)']' : (uint32_t)' ',
			               pal_color_to_boxen(fg), pal_color_to_boxen(bg),
			               pal_attr_to_boxen(base));
		}
	}

	/* '>' indicator when content extends past the right edge. */
	{
		int total_w = 0;
		for (int i = 0; i < lvl->item_count; ++i) {
			const palette_item_t *it2 = &lvl->items[i];
			int w2 = it2->is_separator ? 1
			         : (int)strlen(it2->label) + 4 + (it2->checked ? 2 : 0);
			total_w += w2 + 2;
		}
		if (total_w - hs > cw) {
			boxen_set_cell(win, cw - 1, row, '>',
			               pal_color_to_boxen(PALETTE_COLOR_BRIGHT_WHITE),
			               pal_color_to_boxen(BPB_BG_MENUBAR),
			               pal_attr_to_boxen(PALETTE_ATTR_BOLD));
		}
	}
}

/* -------------------------------------------------------------------------
 * Input handler
 * ---------------------------------------------------------------------- */

/* Translates a boxen_event_t key into palette state-machine byte(s).
 * Returns the last palette_done_t seen.  May drive the done_cb on
 * DONE_EXECUTE / DONE_CANCEL. */
static void palette_modal_on_input(boxen_window_t *win,
                                   const boxen_event_t *ev,
                                   void *user_data) {
	(void)win;
	palette_state_t *st = (palette_state_t *)user_data;
	if (st == NULL) return;

	palette_done_t done = boxen_palette_feed_event(st, ev);

	if (done == PALETTE_DONE_EXECUTE || done == PALETTE_DONE_CANCEL) {
		/* Snapshot before close (palette_close does not invalidate
		 * exec_script / exec_arg per palette.h contract). */
		void *exec_script = st->exec_script;
		/* exec_arg lives inside st -- copy before invoking done_cb which
		 * will call palette_close and free st. */
		char exec_arg_copy[PALETTE_ARG_MAX];
		strncpy(exec_arg_copy, st->exec_arg, sizeof(exec_arg_copy) - 1);
		exec_arg_copy[sizeof(exec_arg_copy) - 1] = '\0';

		if (s_ctx.done_cb != NULL) {
			s_ctx.done_cb(s_ctx.repl_state, done, exec_script, exec_arg_copy);
		}
		/* done_cb is responsible for calling palette_close and freeing st.
		 * If no done_cb is registered, do a defensive teardown here so the
		 * modal window can be destroyed without holding a dangling palette. */
		if (s_ctx.done_cb == NULL) {
			palette_paint_teardown(st);
			palette_close(st);
		}
	} else {
		/* State changed but palette still running -- repaint. */
		palette_render_state(st);
		boxen_present();
	}
}

/* -------------------------------------------------------------------------
 * Draw handler
 * ---------------------------------------------------------------------- */

static void palette_modal_on_draw(boxen_window_t *win, void *user_data) {
	palette_state_t *st = (palette_state_t *)user_data;
	if (st == NULL || !st->active) return;
	/* Delegate to the vtable paint -- which calls boxen_backend_paint below. */
	palette_render_state(st);
}

/* -------------------------------------------------------------------------
 * Backend vtable functions
 * ---------------------------------------------------------------------- */

static bool boxen_backend_open(palette_state_t *st, void *ctx) {
	(void)ctx;

	/* Compute a centered rect for the modal window.
	 * Window height: menubar (row 0) + one strip per open cascade level.
	 * At open time no levels are open, so height = 1 (menubar only) plus
	 * borders (set_borders true adds 2 rows / 2 cols).  Reserve space for
	 * PALETTE_MAX_DEPTH cascade strips plus the menubar (1+depth rows
	 * content) so the window does not need to resize as menus open. */
	int sw = 80, sh = 24;
	boxen_get_screen_size(&sw, &sh);

	int content_w = sw - 4;
	if (content_w < 20) content_w = 20;
	if (content_w > sw - 2) content_w = sw - 2;

	/* Height: 1 (menubar) + PALETTE_MAX_DEPTH (max cascade strips). */
	int content_h = 1 + PALETTE_MAX_DEPTH;
	if (content_h > sh - 4) content_h = sh - 4;
	if (content_h < 3) content_h = 3;

	/* Full window size including 1-cell borders on each side. */
	int win_w = content_w + 2;
	int win_h = content_h + 2;

	/* Center the window.  Prefer to anchor near the top of the screen so
	 * the palette behaves like a menubar; clamp to screen bounds. */
	int win_x = (sw - win_w) / 2;
	int win_y = 1;  /* one row below the top border */
	if (win_x < 0) win_x = 0;
	if (win_y + win_h > sh) win_y = sh - win_h;
	if (win_y < 0) win_y = 0;

	boxen_rect_t rect = { win_x, win_y, win_w, win_h };

	/* Open modal window; pass palette state as user_data so input and draw
	 * callbacks can access it directly. */
	s_ctx.modal_win = boxen_window_open("/", rect, st);
	if (s_ctx.modal_win == NULL) return false;

	boxen_window_set_borders(s_ctx.modal_win, true);

	/* set_modal(true) gives the palette exclusive key capture.
	 * Contrast with C.0.2 completion popup which used raise() without
	 * set_modal because the popup has no input_fn and keys must still
	 * reach the REPL input window.  The palette IS an input consumer
	 * and must intercept every keystroke while open. */
	boxen_window_set_modal(s_ctx.modal_win, true);

	boxen_window_set_input(s_ctx.modal_win, palette_modal_on_input);
	boxen_window_set_draw(s_ctx.modal_win, palette_modal_on_draw);
	boxen_window_focus(s_ctx.modal_win);

	return true;
}

static void boxen_backend_paint(palette_state_t *st, void *ctx) {
	(void)ctx;
	if (s_ctx.modal_win == NULL) return;

	/* Fill entire content area to avoid stale cells. */
	int cw = boxen_window_content_width(s_ctx.modal_win);
	int ch = boxen_window_content_height(s_ctx.modal_win);
	boxen_rect_t all = { 0, 0, cw, ch };
	boxen_fill_rect(s_ctx.modal_win, all, ' ',
	                pal_color_to_boxen(BPB_FG_MENUBAR),
	                pal_color_to_boxen(BPB_BG_MENUBAR), 0);

	/* Row 0: menubar strip. */
	paint_menubar(st, s_ctx.modal_win);

	/* Rows 1..open_depth: cascade strips. */
	for (int d = 0; d < st->open_depth && d < PALETTE_MAX_DEPTH; ++d) {
		int row = d + 1;  /* row 0 = menubar */
		if (row >= ch) break;
		paint_level(st, d, s_ctx.modal_win, row);
	}
}

static void boxen_backend_paint_teardown(palette_state_t *st, void *ctx) {
	(void)st; (void)ctx;
	/* The window is about to be closed by boxen_backend_close; no explicit
	 * cell clearing is needed (window close removes it from the compositor).
	 * This is a deliberate no-op: the pane backend needs teardown to let
	 * the ANSI compositor diff-clear stale cells, but boxen windows vanish
	 * atomically on close, so residual attributes never bleed through. */
}

static void boxen_backend_on_resize(palette_state_t *st,
                                    int term_rows, int term_cols, void *ctx) {
	(void)st; (void)ctx;
	if (s_ctx.modal_win == NULL) return;

	int sw = term_cols, sh = term_rows;

	int content_w = sw - 4;
	if (content_w < 20) content_w = 20;
	if (content_w > sw - 2) content_w = sw - 2;

	int content_h = 1 + PALETTE_MAX_DEPTH;
	if (content_h > sh - 4) content_h = sh - 4;
	if (content_h < 3) content_h = 3;

	int win_w = content_w + 2;
	int win_h = content_h + 2;
	int win_x = (sw - win_w) / 2;
	int win_y = 1;
	if (win_x < 0) win_x = 0;
	if (win_y + win_h > sh) win_y = sh - win_h;
	if (win_y < 0) win_y = 0;

	boxen_rect_t rect = { win_x, win_y, win_w, win_h };
	boxen_window_set_rect(s_ctx.modal_win, rect);
}

static void boxen_backend_close(palette_state_t *st, void *ctx) {
	(void)st; (void)ctx;
	if (s_ctx.modal_win != NULL) {
		boxen_window_close(s_ctx.modal_win);
		s_ctx.modal_win = NULL;
	}
}

/* -------------------------------------------------------------------------
 * Vtable singleton
 * ---------------------------------------------------------------------- */

static const palette_render_backend_t s_boxen_backend = {
	NULL,   /* ctx -- the boxen backend uses file-static s_ctx instead */
	boxen_backend_open,
	boxen_backend_paint,
	boxen_backend_paint_teardown,
	boxen_backend_on_resize,
	boxen_backend_close,
};

const palette_render_backend_t *palette_render_boxen_backend(void) {
	return &s_boxen_backend;
}

/* -------------------------------------------------------------------------
 * Done-callback registration
 * ---------------------------------------------------------------------- */

void palette_render_boxen_backend_set_done_cb(boxen_palette_done_cb cb,
                                              void *repl_state) {
	s_ctx.done_cb    = cb;
	s_ctx.repl_state = repl_state;
}

/* -------------------------------------------------------------------------
 * Event translation
 * ---------------------------------------------------------------------- */

palette_done_t boxen_palette_feed_event(palette_state_t *st,
                                        const boxen_event_t *ev) {
	if (st == NULL || ev == NULL) return PALETTE_DONE_NONE;
	if (ev->type != BOXEN_EV_KEY)  return PALETTE_DONE_NONE;

	switch (ev->key.key) {
	case BOXEN_KEY_NONE:
		/* Printable character -- pass directly. */
		if (ev->key.ch >= 0x20 && ev->key.ch < 0x7F) {
			return palette_feed_byte(st, (unsigned char)ev->key.ch);
		}
		return PALETTE_DONE_NONE;

	case BOXEN_KEY_ESCAPE:
		/* In boxen mode Escape is a named key, not an ambiguous byte
		 * sequence; no disambiguation timeout needed.  Feed 0x1b then
		 * immediately fire the timeout so the palette treats it as a
		 * bare ESC and collapses one level (or cancels at top). */
		(void)palette_feed_byte(st, 0x1b);
		return palette_feed_esc_timeout(st);

	case BOXEN_KEY_ENTER:
		return palette_feed_byte(st, 0x0a);

	case BOXEN_KEY_UP:
		/* Emit CSI A. */
		(void)palette_feed_byte(st, 0x1b);
		(void)palette_feed_byte(st, '[');
		return palette_feed_byte(st, 'A');

	case BOXEN_KEY_DOWN:
		(void)palette_feed_byte(st, 0x1b);
		(void)palette_feed_byte(st, '[');
		return palette_feed_byte(st, 'B');

	case BOXEN_KEY_RIGHT:
		(void)palette_feed_byte(st, 0x1b);
		(void)palette_feed_byte(st, '[');
		return palette_feed_byte(st, 'C');

	case BOXEN_KEY_LEFT:
		(void)palette_feed_byte(st, 0x1b);
		(void)palette_feed_byte(st, '[');
		return palette_feed_byte(st, 'D');

	case BOXEN_KEY_BACKSPACE:
		return palette_feed_byte(st, 0x7f);

	default:
		return PALETTE_DONE_NONE;
	}
}
