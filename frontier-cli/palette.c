/*
 * palette.c - REPL slash-menu palette state machine and cascade renderer.
 *
 * See palette.h for the public API contract and §1.3b of the plan
 * (humming-painting-hejlsberg.md) for the full design rationale.
 *
 * Design summary
 * --------------
 *  - Menus and items are not held in the ODB by this module — they come
 *    in via palette_menu_source_t (a small vtable). PR 6 will provide the
 *    ODB-backed adapter; tests use a synthetic in-memory source.
 *  - The cache is built once per palette_open() and never re-walked
 *    during navigation. That keeps the ODB GIL out of the input loop.
 *  - Each open level (menubar = 0, top menu = 1, ...) owns a pane_t
 *    registered with the global compositor. Closing a level destroys
 *    its pane; opening pushes a new one and raises it.
 *  - Rendering is split: writes go into per-pane cell buffers via
 *    pane_putc/pane_puts. The CALLER calls compositor_render() to flush
 *    to the terminal, so palette can be unit-tested by reading back
 *    framebuffer cells without touching real I/O.
 *
 * Cascade positioning
 * -------------------
 * Default: submenu.x = parent.x + parent.w
 *          submenu.y = parent.y + 1 + parent.cursor   (item rows = pane.y+1+i)
 *
 * Right-edge overflow: if submenu.x + submenu.w > term_cols, open LEFT:
 *   submenu.x = parent.x - submenu.w
 *
 * Bottom-edge overflow: if submenu.y + submenu.h > term_rows, shift UP:
 *   submenu.y = term_rows - submenu.h  (clamped to >= 0)
 *
 * Both can apply simultaneously; the LEFT and UP adjustments are
 * independent. If even after both shifts the submenu can't fit on
 * screen, we clamp to (0, 0) — the floor case is graceful degradation,
 * not faithful UX.
 *
 * Attribute encoding
 * ------------------
 * Mirrors pane.c's emit_attr() bit assignments so the same byte can be
 * passed to pane_putc:
 *   0x01 BOLD       — hotkey letters in labels; focused-pane border
 *   0x02 DIM        — disabled items; unfocused borders
 *   0x04 UNDERLINE  — (reserved)
 *   0x08 INVERSE    — cursor (selected item / menubar entry)
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors. See pane.h for the full
 * license text.
 */

#include "palette.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Internal helpers ---------- */

static int imax(int a, int b) { return a > b ? a : b; }
static int imin(int a, int b) { return a < b ? a : b; }

/* Case-insensitive substring match: returns true if `needle` (length
 * needle_len, no NUL required) appears anywhere in `hay` as a
 * case-insensitive substring. Empty needle always matches. */
static bool ci_substr(const char *hay, const char *needle, int needle_len) {
	if (needle_len <= 0) return true;
	if (!hay) return false;
	int haylen = (int)strlen(hay);
	if (needle_len > haylen) return false;
	for (int i = 0; i + needle_len <= haylen; ++i) {
		bool match = true;
		for (int k = 0; k < needle_len; ++k) {
			unsigned char a = (unsigned char)hay[i + k];
			unsigned char b = (unsigned char)needle[k];
			if (tolower(a) != tolower(b)) { match = false; break; }
		}
		if (match) return true;
	}
	return false;
}

/* Recompute the visible[] index map for the given level using the
 * supplied filter. When filter_len == 0 every non-hidden item is
 * visible; otherwise only items whose label contains the filter as a
 * case-insensitive substring are visible. After rebuilding visible[],
 * the cursor is adjusted so it always points to a visible item (or to
 * the first visible item if its old position was filtered out). The
 * scroll_top is also clamped — if the new visible_count shrinks, the
 * scroll window slides up so the cursor remains in view. */
static void level_recompute_visible(palette_level_t *lvl,
                                    const char *filter, int filter_len) {
	int n = 0;
	for (int i = 0; i < lvl->item_count; ++i) {
		if (filter_len <= 0 ||
		    ci_substr(lvl->items[i].label, filter, filter_len)) {
			if (n < (int)(sizeof(lvl->visible) / sizeof(lvl->visible[0]))) {
				lvl->visible[n++] = i;
			}
		}
	}
	lvl->visible_count = n;

	/* Adjust cursor: if the prior cursor item is still visible, keep
	 * it; else snap to the first visible item (or 0 if none). */
	int new_cursor = (n > 0) ? lvl->visible[0] : 0;
	for (int i = 0; i < n; ++i) {
		if (lvl->visible[i] == lvl->cursor) {
			new_cursor = lvl->cursor;
			break;
		}
	}
	lvl->cursor = new_cursor;

	if (lvl->scroll_top > imax(0, n - 1)) lvl->scroll_top = imax(0, n - 1);
	if (lvl->scroll_top < 0) lvl->scroll_top = 0;
}

/* Find the row index in visible[] for the level's current cursor (the
 * source-item index). Returns -1 if the cursor's item is not currently
 * visible (shouldn't happen post-recompute, but defensive). */
static int level_visible_row_for_cursor(const palette_level_t *lvl) {
	for (int i = 0; i < lvl->visible_count; ++i) {
		if (lvl->visible[i] == lvl->cursor) return i;
	}
	return -1;
}

/* Adjust scroll_top so the cursor's visible row falls within the
 * scrollable window of the given pane height. window_rows is the
 * number of item rows displayable inside the pane (h minus borders
 * minus any reserved input row). */
static void level_scroll_to_cursor(palette_level_t *lvl, int window_rows) {
	if (window_rows <= 0) {
		lvl->scroll_top = 0;
		return;
	}
	int row = level_visible_row_for_cursor(lvl);
	if (row < 0) row = 0;
	if (row < lvl->scroll_top) {
		lvl->scroll_top = row;
	} else if (row >= lvl->scroll_top + window_rows) {
		lvl->scroll_top = row - window_rows + 1;
	}
	if (lvl->scroll_top < 0) lvl->scroll_top = 0;
}

/* Move cursor to the visible item at row `vis_row` in visible[].
 * Clamps. Returns true if cursor changed. */
static bool level_set_cursor_to_visible_row(palette_level_t *lvl, int vis_row) {
	if (lvl->visible_count <= 0) return false;
	if (vis_row < 0) vis_row = 0;
	if (vis_row >= lvl->visible_count) vis_row = lvl->visible_count - 1;
	int old = lvl->cursor;
	lvl->cursor = lvl->visible[vis_row];
	return old != lvl->cursor;
}

/* Returns the number of inner item rows usable by this level after
 * accounting for borders and (when applicable) the reserved
 * accepts_args input row. The input row is reserved on the deepest
 * open level when its cursor item has accepts_args == true. */
static int level_window_rows(const palette_level_t *lvl,
                             bool reserve_arg_row) {
	int rows = lvl->pane.h - 2;          /* top + bottom border */
	if (reserve_arg_row) rows -= 1;
	if (rows < 0) rows = 0;
	return rows;
}

/* True if the level's deepest cursor item has accepts_args set. */
static bool level_cursor_accepts_args(const palette_level_t *lvl) {
	if (lvl->visible_count <= 0) return false;
	if (lvl->cursor < 0 || lvl->cursor >= lvl->item_count) return false;
	return lvl->items[lvl->cursor].accepts_args;
}

/* Items are rendered with two columns of label-padding inside the
 * border, so the pane width = max(label widths) + 2 (padding) + 2
 * (border). The minimum width is 4 (border + 2 chars). */
static int compute_menu_pane_width(const palette_item_t *items, int n) {
	int maxlen = 0;
	for (int i = 0; i < n; ++i) {
		int len = (int)strlen(items[i].label);
		if (len > maxlen) maxlen = len;
	}
	if (maxlen < 4) maxlen = 4;
	return maxlen + 4;          /* 1 left border + 1 left pad + label
	                             * + 1 right pad + 1 right border */
}

/* Compute pane height for `n` items. Adds 1 row for the arg input row
 * if any item in the level accepts arguments (so cursor-on-args
 * doesn't shrink the visible item count by one when reserve_arg
 * trims the window).
 *
 * The level may still be capped to terminal height — when the natural
 * height exceeds available rows, render_level will scroll items
 * through the smaller window. */
static int compute_menu_pane_height(int n, bool any_accepts_args) {
	if (n < 1) n = 1;
	int h = n + 2;              /* top/bottom border */
	if (any_accepts_args) h += 1;
	return h;
}

static bool items_any_accepts_args(const palette_item_t *items, int n) {
	for (int i = 0; i < n; ++i) {
		if (items[i].accepts_args) return true;
	}
	return false;
}

/* Menubar layout: every entry is " Label " — surrounded by single
 * spaces so neighbours don't visually fuse. Width = label + 2. */
static int menubar_entry_width(const char *label) {
	return (int)strlen(label) + 2;
}

/* Capture the menubar entries' labels and screen-x positions into st. */
static void layout_menubar(palette_state_t *st) {
	int n = st->source->count_menus(st->source->ctx);
	if (n > (int)(sizeof(st->menu_labels) / sizeof(st->menu_labels[0]))) {
		n = (int)(sizeof(st->menu_labels) / sizeof(st->menu_labels[0]));
	}
	st->menu_count = n;

	int x = 0;
	for (int i = 0; i < n; ++i) {
		char hk = '\0';
		st->source->menu_describe(st->source->ctx, i,
		                          st->menu_labels[i],
		                          sizeof(st->menu_labels[i]), &hk);
		/* Defensive NUL-termination — vtable spec requires sources to
		 * NUL-terminate but this enforces the invariant locally so
		 * subsequent strlen/loops are always safe even against a
		 * misbehaving source. */
		st->menu_labels[i][sizeof(st->menu_labels[i]) - 1] = '\0';
		st->menu_hotkeys[i] = hk;
		st->menu_x[i] = x;
		st->menu_w[i] = menubar_entry_width(st->menu_labels[i]);
		x += st->menu_w[i];
	}
}

/* ANSI 16-color theme for the palette (Rung 2 / PR 8).
 *
 * Convention:
 *   - Menubar:           cyan-on-blue (matches REPL prompt convention)
 *   - Selected item:     black-on-white (terminal-default inverse-style)
 *   - Hotkey letter:     bright yellow (drops out against menubar bg)
 *   - Disabled item:     dim gray (bright black)
 *   - Border (focused):  bright white, BOLD
 *   - Border (un-focus): white, DIM
 *   - Description:       bright cyan, DIM
 *   - Filter footer:     bright cyan
 *   - Arg input row:     bright white-on-black
 *
 * Colors use the palette_color values defined in palette.h (1..16).
 */
#define PT_FG_MENUBAR       PALETTE_COLOR_BRIGHT_CYAN
#define PT_BG_MENUBAR       PALETTE_COLOR_BLUE
#define PT_FG_SELECTED      PALETTE_COLOR_BLACK
#define PT_BG_SELECTED      PALETTE_COLOR_WHITE
#define PT_FG_HOTKEY        PALETTE_COLOR_BRIGHT_YELLOW
#define PT_FG_DISABLED      PALETTE_COLOR_BRIGHT_BLACK
#define PT_FG_BORDER        PALETTE_COLOR_BRIGHT_WHITE
#define PT_FG_BORDER_DIM    PALETTE_COLOR_WHITE
#define PT_FG_DESCRIPTION   PALETTE_COLOR_BRIGHT_CYAN
#define PT_FG_FILTER        PALETTE_COLOR_BRIGHT_CYAN
#define PT_FG_ARG           PALETTE_COLOR_BRIGHT_WHITE
#define PT_BG_ARG           PALETTE_COLOR_BLACK
#define PT_FG_ITEM          PALETTE_COLOR_DEFAULT
#define PT_BG_ITEM          PALETTE_COLOR_DEFAULT

/* Render the menubar pane. The cursor entry is INVERSE; the hotkey
 * character within each entry is BOLD. */
static void render_menubar(palette_state_t *st) {
	pane_clear(&st->menubar);
	/* Paint the entire menubar row with the menubar bg first so the
	 * region between menu entries doesn't show through to terminal
	 * default. */
	for (int x = 0; x < st->menubar.w; ++x) {
		pane_putc_color(&st->menubar, x, 0, ' ',
		                PT_FG_MENUBAR, PT_BG_MENUBAR, 0);
	}
	for (int i = 0; i < st->menu_count; ++i) {
		const char *label = st->menu_labels[i];
		int x = st->menu_x[i];
		int w = st->menu_w[i];
		bool selected = (i == st->menubar_cursor);
		uint8_t base = selected ? PALETTE_ATTR_INVERSE : 0;
		uint8_t fg = selected ? PT_FG_SELECTED : PT_FG_MENUBAR;
		uint8_t bg = selected ? PT_BG_SELECTED : PT_BG_MENUBAR;

		/* Leading space. */
		pane_putc_color(&st->menubar, x, 0, ' ', fg, bg, base);
		int lx = x + 1;
		char hk = st->menu_hotkeys[i];
		bool hk_seen = false;
		for (const char *p = label; *p; ++p, ++lx) {
			uint8_t a = base;
			uint8_t cell_fg = fg;
			if (!hk_seen && hk && *p == hk) {
				a |= PALETTE_ATTR_BOLD;
				/* Highlight the hotkey letter only when the menu
				 * isn't selected — when selected the inverse video
				 * already calls attention, and overriding the fg
				 * to yellow on a yellow-tinted inverse cell can
				 * make the character vanish on some terminals. */
				if (!selected) cell_fg = PT_FG_HOTKEY;
				hk_seen = true;
			}
			pane_putc_color(&st->menubar, lx, 0,
			                (uint32_t)(unsigned char)*p, cell_fg, bg, a);
		}
		/* Trailing space. */
		pane_putc_color(&st->menubar, x + w - 1, 0, ' ', fg, bg, base);
	}
}

/* Border characters — using ASCII rather than box-drawing so framebuffer
 * tests can match characters directly without UTF-8 codepoint
 * juggling. PR 8 may swap these out for U+2500-family glyphs once the
 * snapshot tests learn to handle them. */
static const uint32_t BORDER_TL = '+';
static const uint32_t BORDER_TR = '+';
static const uint32_t BORDER_BL = '+';
static const uint32_t BORDER_BR = '+';
static const uint32_t BORDER_H  = '-';
static const uint32_t BORDER_V  = '|';

/* Render one cascade level pane. Rung 2 features:
 *   - Filter-aware item enumeration (visible[] not items[])
 *   - Scroll arrows on the right border when items overflow
 *   - Filter footer ("> filter_") in the bottom border when filter active
 *   - Reserved arg input row above the bottom border for accepts_args items
 *   - 16-color theme (selected = black-on-white, hotkey = bright yellow,
 *     disabled = dim gray, border colored by focus state). */
static void render_level(palette_state_t *st, int depth) {
	palette_level_t *lvl = &st->levels[depth];
	pane_t *p = &lvl->pane;
	pane_clear(p);

	/* Border attribute / color: BOLD bright-white if this is the deepest
	 * open level (focused), DIM white otherwise. */
	bool focused = (depth == st->open_depth - 1);
	uint8_t border_attr = focused ? PALETTE_ATTR_BOLD : PALETTE_ATTR_DIM;
	uint8_t border_fg = focused ? PT_FG_BORDER : PT_FG_BORDER_DIM;

	/* Decide whether this level reserves a row for arg input. Only the
	 * deepest level can show the input row (filter / arg are state on
	 * the palette, scoped to deepest), and only when the cursor item
	 * has accepts_args. */
	bool reserve_arg = focused && level_cursor_accepts_args(lvl);
	int item_rows = level_window_rows(lvl, reserve_arg);

	/* Top + bottom borders. */
	pane_putc_color(p, 0, 0, BORDER_TL, border_fg, 0, border_attr);
	pane_putc_color(p, p->w - 1, 0, BORDER_TR, border_fg, 0, border_attr);
	pane_putc_color(p, 0, p->h - 1, BORDER_BL, border_fg, 0, border_attr);
	pane_putc_color(p, p->w - 1, p->h - 1, BORDER_BR, border_fg, 0, border_attr);
	for (int x = 1; x < p->w - 1; ++x) {
		pane_putc_color(p, x, 0, BORDER_H, border_fg, 0, border_attr);
		pane_putc_color(p, x, p->h - 1, BORDER_H, border_fg, 0, border_attr);
	}
	for (int y = 1; y < p->h - 1; ++y) {
		pane_putc_color(p, 0, y, BORDER_V, border_fg, 0, border_attr);
		pane_putc_color(p, p->w - 1, y, BORDER_V, border_fg, 0, border_attr);
	}

	/* Filter footer: when the filter is non-empty AND this is the focused
	 * level, draw "> filter_" centered in the bottom border (overwriting
	 * the dashes). The cursor character ('_') visually anchors the input.
	 * Only drawn on the focused level — non-focused panes show a clean
	 * border. */
	if (focused && st->filter_len > 0) {
		char footer[PALETTE_FILTER_MAX + 8];
		int n = snprintf(footer, sizeof(footer), "> %s_", st->filter_buf);
		if (n < 0) n = 0;
		if (n > p->w - 2) n = p->w - 2;
		int fx = (p->w - n) / 2;
		if (fx < 1) fx = 1;
		for (int k = 0; k < n; ++k) {
			pane_putc_color(p, fx + k, p->h - 1,
			                (uint32_t)(unsigned char)footer[k],
			                PT_FG_FILTER, 0, PALETTE_ATTR_BOLD);
		}
	}

	/* Scroll arrows: when not all visible items fit in the item-rows
	 * window, draw '^' at the top-right inside the right border (just
	 * below the top border) and 'v' at the bottom-right (just above
	 * the bottom border). The arrows render only when actual scrolling
	 * is possible in that direction.
	 *
	 * The arrows replace the right-border vertical bar at those rows. */
	if (lvl->visible_count > item_rows) {
		int can_up = lvl->scroll_top > 0;
		int can_down = lvl->scroll_top + item_rows < lvl->visible_count;
		if (can_up) {
			pane_putc_color(p, p->w - 1, 1, '^',
			                PT_FG_BORDER, 0, PALETTE_ATTR_BOLD);
		}
		if (can_down) {
			int row = 1 + item_rows - 1;
			pane_putc_color(p, p->w - 1, row, 'v',
			                PT_FG_BORDER, 0, PALETTE_ATTR_BOLD);
		}
	}

	/* "(no matches)" placeholder when filter is non-empty and no item
	 * matched. Drawn centered on row 1 (just below top border) and the
	 * remaining rows are left blank. */
	if (lvl->visible_count == 0) {
		const char *msg = "(no matches)";
		int mlen = (int)strlen(msg);
		int mx = (p->w - mlen) / 2;
		if (mx < 1) mx = 1;
		for (int k = 0; k < mlen && mx + k < p->w - 1; ++k) {
			pane_putc_color(p, mx + k, 1,
			                (uint32_t)(unsigned char)msg[k],
			                PT_FG_DISABLED, 0, PALETTE_ATTR_DIM);
		}
		return;
	}

	/* Items. We walk visible[scroll_top .. scroll_top+item_rows) and
	 * draw each at row (1 + i - scroll_top) inside the pane. */
	int first = lvl->scroll_top;
	int last = imin(first + item_rows, lvl->visible_count);
	for (int vis_i = first; vis_i < last; ++vis_i) {
		int item_idx = lvl->visible[vis_i];
		const palette_item_t *it = &lvl->items[item_idx];
		int row = 1 + (vis_i - first);
		bool selected = (item_idx == lvl->cursor);
		uint8_t base = 0;
		uint8_t fg = it->enabled ? PT_FG_ITEM : PT_FG_DISABLED;
		uint8_t bg = PT_BG_ITEM;
		if (selected) {
			base |= PALETTE_ATTR_INVERSE;
			fg = PT_FG_SELECTED;
			bg = PT_BG_SELECTED;
		}
		if (!it->enabled) base |= PALETTE_ATTR_DIM;

		/* Pad the row with spaces (under base attr/colors) so the
		 * inverse highlight extends across the full inside-border
		 * width. */
		for (int x = 1; x < p->w - 1; ++x) {
			pane_putc_color(p, x, row, ' ', fg, bg, base);
		}

		/* Label, with the hotkey letter rendered BOLD + bright yellow
		 * when the item is not selected (yellow on the inverse cell
		 * looks washed out). */
		int lx = 2;
		char hk = it->shortcut;
		bool hk_seen = false;
		for (const char *q = it->label; *q && lx < p->w - 1; ++q, ++lx) {
			uint8_t a = base;
			uint8_t cell_fg = fg;
			if (!hk_seen && hk && *q == hk) {
				a |= PALETTE_ATTR_BOLD;
				if (!selected && it->enabled) cell_fg = PT_FG_HOTKEY;
				hk_seen = true;
			}
			pane_putc_color(p, lx, row, (uint32_t)(unsigned char)*q,
			                cell_fg, bg, a);
		}

		/* Submenu indicator on the right side, just inside the right
		 * border. */
		if (it->is_submenu && p->w >= 3) {
			pane_putc_color(p, p->w - 2, row, '>', fg, bg, base);
		}
	}

	/* Arg input row: drawn one row above the bottom border when
	 * reserve_arg is true. Format is "[ <typed>_ ]" — the trailing '_'
	 * indicates the cursor position. */
	if (reserve_arg) {
		int row = p->h - 2;
		/* Background fill. */
		for (int x = 1; x < p->w - 1; ++x) {
			pane_putc_color(p, x, row, ' ',
			                PT_FG_ARG, PT_BG_ARG, 0);
		}
		const char *prefix = "> ";
		int px = 1;
		for (const char *q = prefix; *q && px < p->w - 1; ++q, ++px) {
			pane_putc_color(p, px, row, (uint32_t)(unsigned char)*q,
			                PT_FG_ARG, PT_BG_ARG, 0);
		}
		for (int k = 0; k < st->arg_len && px < p->w - 2; ++k, ++px) {
			pane_putc_color(p, px, row,
			                (uint32_t)(unsigned char)st->arg_buf[k],
			                PT_FG_ARG, PT_BG_ARG, 0);
		}
		/* Cursor underscore at the end of the typed text. */
		if (px < p->w - 1) {
			pane_putc_color(p, px, row, '_',
			                PT_FG_ARG, PT_BG_ARG, PALETTE_ATTR_BOLD);
		}
	}
}

/* Compute the natural placement of a cascade level given its parent
 * geometry and term bounds. Implements the LEFT / UP overflow rules.
 *
 * After the LEFT/UP fallback, applies a final width/height clamp so the
 * pane never extends past term_cols / term_rows. Returns false if the
 * post-clamp pane is too small to be useful (w < 4 or h < 3) — caller
 * should refuse to open this level. *out_w / *out_h hold the
 * (possibly-clamped) final dimensions. */
static bool place_cascade_pane(palette_state_t *st, int depth,
                               int w, int h,
                               int *out_x, int *out_y,
                               int *out_w, int *out_h) {
	int x, y;
	if (depth == 0) {
		/* Top-level menu: anchored beneath its menubar entry. */
		int mi = st->menubar_cursor;
		int ax = st->menu_x[mi];
		int ay = 1;             /* row directly under menubar */
		/* Right-overflow: shift left so the pane fits. We can't open
		 * "LEFT of" the menubar entry meaningfully — just clamp. */
		if (ax + w > st->term_cols) ax = st->term_cols - w;
		if (ax < 0) ax = 0;
		if (ay + h > st->term_rows) ay = st->term_rows - h;
		if (ay < 1) ay = 1;     /* never overlap the menubar */
		x = ax;
		y = ay;
	} else {
		/* Submenu: cascade off the parent at the cursor row. */
		const palette_level_t *parent = &st->levels[depth - 1];
		int default_x = parent->pane.x + parent->pane.w;
		int default_y = parent->pane.y + 1 + parent->cursor;

		x = default_x;
		y = default_y;

		if (x + w > st->term_cols) {
			/* Open LEFT instead. */
			x = parent->pane.x - w;
			if (x < 0) {
				/* Even leftward doesn't fit — pick whichever side leaves
				 * more pane on screen. Fall back to clamp at 0. */
				x = 0;
			}
		}
		if (y + h > st->term_rows) {
			y = st->term_rows - h;
			if (y < 1) y = 1;       /* never overlap menubar */
		}
		if (y < 1) y = 1;
	}

	/* Final clamp — even after LEFT/UP fallbacks, narrow terminals can
	 * still produce a pane that extends past the screen. Trim w/h to fit.
	 * If trimming makes the pane too small to render usefully, refuse. */
	if (x + w > st->term_cols) w = st->term_cols - x;
	if (y + h > st->term_rows) h = st->term_rows - y;
	if (w < 4 || h < 3) return false;

	*out_x = x;
	*out_y = y;
	*out_w = w;
	*out_h = h;
	return true;
}

/* Open a level and populate it from the data source. depth 0 = top
 * menu under menubar_cursor; depth >= 1 = submenu off levels[depth-1]'s
 * cursor item. */
static bool open_level(palette_state_t *st, int depth) {
	if (depth < 0 || depth >= PALETTE_MAX_DEPTH) return false;

	palette_level_t *lvl = &st->levels[depth];
	memset(lvl, 0, sizeof(*lvl));

	int menu_index;
	void *parent_opaque;
	if (depth == 0) {
		menu_index = st->menubar_cursor;
		parent_opaque = NULL;
	} else {
		const palette_level_t *parent = &st->levels[depth - 1];
		const palette_item_t *anchor = &parent->items[parent->cursor];
		if (!anchor->is_submenu) return false;
		/* For submenu levels we propagate the ROOT menu index down for
		 * caller convenience, but the source MUST resolve items via
		 * `parent_opaque` — `menu_index` is advisory when
		 * `parent_opaque != NULL`. See palette_menu_source_t doc. */
		menu_index = parent->menu_index;
		parent_opaque = anchor->opaque;
	}
	lvl->menu_index = menu_index;
	lvl->parent_opaque = parent_opaque;

	int total = st->source->item_count(st->source->ctx, menu_index, parent_opaque);
	if (total < 0) total = 0;
	int n = imin(total, PALETTE_LEVEL_MAX_ITEMS);
	int kept = 0;
	for (int i = 0; i < n; ++i) {
		palette_item_t tmp;
		if (!st->source->item_describe(st->source->ctx, menu_index,
		                               parent_opaque, i, &tmp)) {
			continue;
		}
		/* Defensive NUL-termination — vtable spec requires sources to
		 * NUL-terminate but this enforces the invariant locally so
		 * subsequent strlen/loops are always safe even against a
		 * misbehaving source. */
		tmp.label[sizeof(tmp.label) - 1] = '\0';
		tmp.description[sizeof(tmp.description) - 1] = '\0';
		if (tmp.hidden) continue;
		lvl->items[kept++] = tmp;
	}
	lvl->item_count = kept;
	lvl->cursor = 0;
	lvl->scroll_top = 0;
	/* New level always starts with an empty filter — populate visible[]
	 * with the identity map. Filter state on st applies to the deepest
	 * open level only and is cleared on level-open / level-close
	 * transitions (see open_level / close_deepest_level). */
	st->filter_len = 0;
	st->filter_buf[0] = '\0';
	st->arg_len = 0;
	st->arg_buf[0] = '\0';
	level_recompute_visible(lvl, NULL, 0);

	int w = compute_menu_pane_width(lvl->items, lvl->item_count);
	bool any_args = items_any_accepts_args(lvl->items, lvl->item_count);
	int h = compute_menu_pane_height(lvl->item_count, any_args);
	int px = 0, py = 0;
	int pw = 0, ph = 0;
	/* Use the prospective open_depth so place_cascade_pane sees this
	 * level as the current focus context. */
	int saved_depth = st->open_depth;
	st->open_depth = depth + 1;
	bool fits = place_cascade_pane(st, depth, w, h, &px, &py, &pw, &ph);
	st->open_depth = saved_depth;
	if (!fits) {
		/* Terminal too small to render this cascade level usefully —
		 * refuse to open. Caller observes no change in open_depth. */
		memset(lvl, 0, sizeof(*lvl));
		return false;
	}

	pane_init(&lvl->pane, px, py, pw, ph);
	compositor_register(&lvl->pane);
	pane_raise(&lvl->pane);

	st->open_depth = depth + 1;
	return true;
}

static void close_deepest_level(palette_state_t *st) {
	if (st->open_depth <= 0) return;
	int d = st->open_depth - 1;
	compositor_unregister(&st->levels[d].pane);
	pane_destroy(&st->levels[d].pane);
	memset(&st->levels[d], 0, sizeof(st->levels[d]));
	st->open_depth = d;
	/* Filter / arg row is owned by the deepest level — clear when the
	 * deepest level changes. */
	st->filter_len = 0;
	st->filter_buf[0] = '\0';
	st->arg_len = 0;
	st->arg_buf[0] = '\0';
}

static void close_all_levels(palette_state_t *st) {
	while (st->open_depth > 0) close_deepest_level(st);
}

/* ---------- Public API ---------- */

bool palette_open(palette_state_t *st, int term_rows, int term_cols,
                  const palette_menu_source_t *source) {
	if (!st || !source) return false;
	if (term_cols < 4 || term_rows < 2) return false;

	memset(st, 0, sizeof(*st));
	st->source = source;
	st->term_rows = term_rows;
	st->term_cols = term_cols;

	int n = source->count_menus(source->ctx);
	if (n <= 0) return false;

	layout_menubar(st);
	if (st->menu_count <= 0) return false;

	/* Initialise menubar pane: row 0, full width, 1 row tall, no border.
	 * Set active=true immediately after the first compositor_register
	 * call so that palette_close cleans up consistently even if a future
	 * palette_open variant fails partway through (e.g. during a follow-up
	 * source query). Today this is simply belt-and-suspenders, but it
	 * prevents resource leaks if open ever grows additional fallible
	 * steps. */
	pane_init(&st->menubar, 0, 0, st->term_cols, 1);
	compositor_register(&st->menubar);
	st->active = true;
	st->menubar_cursor = 0;
	st->open_depth = 0;
	st->esc_pending = false;
	st->csi_len = 0;
	st->exec_script = NULL;
	return true;
}

void palette_close(palette_state_t *st) {
	if (!st) return;
	if (!st->active) {
		/* Already closed (or never opened) — idempotent no-op. */
		return;
	}
	close_all_levels(st);
	compositor_unregister(&st->menubar);
	pane_destroy(&st->menubar);
	memset(st->menu_labels, 0, sizeof(st->menu_labels));
	st->active = false;
}

/* Note: PR 5 had a sibling find_hotkey_in_deepest() used by
 * palette_feed_byte to map letters to item hotkeys inside an open menu.
 * PR 8 (Rung 2) replaces that with the filter / type-ahead behavior —
 * letters inside an open menu now build a substring filter rather than
 * jumping to a hotkey. Hotkey acceleration is preserved on the menubar
 * itself (no menu open) via find_hotkey_in_menubar below. */

static int find_hotkey_in_menubar(const palette_state_t *st, char letter) {
	char want = (char)toupper((unsigned char)letter);
	for (int i = 0; i < st->menu_count; ++i) {
		char hk = st->menu_hotkeys[i];
		if (!hk) continue;
		if ((char)toupper((unsigned char)hk) == want) return i;
	}
	return -1;
}

/* Activate the cursor item of the deepest open level. Either drills
 * into a submenu or sets exec_script + returns DONE_EXECUTE for a leaf.
 * Disabled items are no-ops. For accepts_args items, also copies arg_buf
 * into exec_arg before returning DONE_EXECUTE. */
static palette_done_t activate_cursor_item(palette_state_t *st) {
	if (st->open_depth <= 0) return PALETTE_DONE_NONE;
	palette_level_t *lvl = &st->levels[st->open_depth - 1];
	if (lvl->cursor < 0 || lvl->cursor >= lvl->item_count) return PALETTE_DONE_NONE;
	/* No-op when nothing is visible (e.g. filter excluded all items).
	 * Without this guard, ENTER would dispatch the still-pinned cursor
	 * item even though it's filtered out — surprising and bad UX. */
	if (lvl->visible_count <= 0) return PALETTE_DONE_NONE;
	if (level_visible_row_for_cursor(lvl) < 0) return PALETTE_DONE_NONE;
	const palette_item_t *it = &lvl->items[lvl->cursor];
	if (!it->enabled) return PALETTE_DONE_NONE;

	if (it->is_submenu) {
		open_level(st, st->open_depth);
		return PALETTE_DONE_NONE;
	}
	st->exec_script = it->script_handle;
	if (it->accepts_args) {
		size_t n = (size_t)st->arg_len;
		if (n >= sizeof(st->exec_arg)) n = sizeof(st->exec_arg) - 1;
		memcpy(st->exec_arg, st->arg_buf, n);
		st->exec_arg[n] = '\0';
	} else {
		st->exec_arg[0] = '\0';
	}
	return PALETTE_DONE_EXECUTE;
}

/* Move the deepest level's cursor by `delta` rows in visible[] order,
 * clamping to the visible range. Auto-scrolls so the cursor stays in
 * view. Resets the arg buffer if the cursor moves off an accepts_args
 * item — typing on a different item should not retain the prior
 * argument string. */
static void cursor_step(palette_state_t *st, int delta) {
	if (st->open_depth <= 0) return;
	palette_level_t *lvl = &st->levels[st->open_depth - 1];
	int row = level_visible_row_for_cursor(lvl);
	if (row < 0) row = 0;
	int new_row = row + delta;
	if (new_row < 0) new_row = 0;
	if (new_row >= lvl->visible_count) new_row = lvl->visible_count - 1;
	bool was_args = level_cursor_accepts_args(lvl);
	level_set_cursor_to_visible_row(lvl, new_row);
	bool now_args = level_cursor_accepts_args(lvl);
	if (was_args && !now_args) {
		st->arg_len = 0;
		st->arg_buf[0] = '\0';
	}
	if (was_args != now_args) {
		st->arg_len = 0;
		st->arg_buf[0] = '\0';
	}
	int win = level_window_rows(lvl, level_cursor_accepts_args(lvl));
	level_scroll_to_cursor(lvl, win);
}

/* Process a CSI terminator. csi_buf holds the bytes between '[' and the
 * terminator; this function consumes the buffer.
 *
 * For arrow keys csi_buf is empty (just '['). For PgUp/PgDn it's '5~' or
 * '6~' — `~` is the terminator and `5`/`6` is in csi_buf as the param. */
static palette_done_t process_csi(palette_state_t *st, char term) {
	/* Capture the parameter (if any) before clearing the buffer. The
	 * buffer's first byte is always '[' (the CSI introducer); subsequent
	 * bytes are parameter / intermediate bytes. */
	char param = (st->csi_len > 1) ? st->csi_buf[1] : '\0';
	st->csi_len = 0;
	switch (term) {
	case 'A': /* UP */
		if (st->open_depth > 0) {
			cursor_step(st, -1);
		}
		return PALETTE_DONE_NONE;
	case 'B': /* DOWN */
		if (st->open_depth > 0) {
			cursor_step(st, +1);
		}
		return PALETTE_DONE_NONE;
	case '~': /* Page Up (param=5), Page Down (param=6), Home (param=1/7),
	          * End (param=4/8). We handle Page Up / Page Down. */
		if (st->open_depth > 0) {
			palette_level_t *lvl = &st->levels[st->open_depth - 1];
			int win = level_window_rows(lvl, level_cursor_accepts_args(lvl));
			int page = imax(1, win - 1);
			if (param == '5') {
				cursor_step(st, -page);
			} else if (param == '6') {
				cursor_step(st, +page);
			}
		}
		return PALETTE_DONE_NONE;
	case 'C': /* RIGHT */
		if (st->open_depth == 0) {
			if (st->menubar_cursor < st->menu_count - 1) st->menubar_cursor++;
		} else {
			/* If on a submenu item, open the cascade. Otherwise:
			 *   - At depth == 1 (top-level menu open) on a non-submenu
			 *     item: collapse and advance to the next menubar entry
			 *     (VisiCalc convention — RIGHT scans the menubar).
			 *   - At depth > 1 (inside a cascade) on a non-submenu item:
			 *     intentional no-op. Top-level RIGHT is the only path
			 *     that navigates the menubar; deeper cascades treat
			 *     RIGHT as "drill in if possible, else nothing" so the
			 *     user doesn't lose their place by accident. */
			palette_level_t *lvl = &st->levels[st->open_depth - 1];
			if (lvl->cursor >= 0 && lvl->cursor < lvl->item_count &&
			    lvl->items[lvl->cursor].is_submenu &&
			    lvl->items[lvl->cursor].enabled) {
				open_level(st, st->open_depth);
			} else if (st->open_depth == 1 &&
			           st->menubar_cursor < st->menu_count - 1) {
				close_all_levels(st);
				st->menubar_cursor++;
				open_level(st, 0);
			}
			/* depth > 1 non-submenu: no-op (see comment above). */
		}
		return PALETTE_DONE_NONE;
	case 'D': /* LEFT */
		if (st->open_depth > 1) {
			close_deepest_level(st);
		} else if (st->open_depth == 1) {
			/* Close the open menu, returning to menubar. (Choice:
			 * the plan also allows "move to previous menubar entry";
			 * the test_left_on_open_top_level_closes_to_menubar
			 * pins this behavior. PR 8 may extend with the
			 * VisiCalc-style "auto-open previous menu".) */
			close_all_levels(st);
		} else {
			if (st->menubar_cursor > 0) st->menubar_cursor--;
		}
		return PALETTE_DONE_NONE;
	default:
		/* Unknown CSI — ignore. */
		return PALETTE_DONE_NONE;
	}
}

palette_done_t palette_feed_byte(palette_state_t *st, unsigned char b) {
	if (!st || !st->active) return PALETTE_DONE_NONE;

	/* Iterative re-feed loop. The bare-ESC-followed-by-non-'[' path used
	 * to recurse to "process the trailing byte after flushing ESC".
	 * Replaced with an explicit loop so there's no implicit recursion
	 * bound and the control flow is local. The loop body either:
	 *   - returns (definitive done value), or
	 *   - sets b to a byte to re-process and `continue`s. */
	for (;;) {
		/* Mid-CSI: collect bytes until terminator.
		 *
		 * Special case: an ESC arriving mid-CSI aborts the partial
		 * sequence and begins a fresh ESC dispatch — the partial CSI
		 * bytes are silently discarded. This matches XTerm behavior and
		 * avoids stale csi_buf contamination if a runaway sequence is
		 * interrupted by a user keypress. */
		if (st->csi_len > 0 || (st->esc_pending && b == '[')) {
			if (b == 0x1b) {
				/* Abort current CSI; ESC starts fresh. */
				st->csi_len = 0;
				st->esc_pending = true;
				return PALETTE_DONE_NONE;
			}
			if (st->esc_pending && b == '[') {
				st->esc_pending = false;
				st->csi_len = 1;        /* mark as "in CSI body" */
				st->csi_buf[0] = '[';
				return PALETTE_DONE_NONE;
			}
			/* CSI body byte. Final byte is in 0x40..0x7E. */
			if (b >= 0x40 && b <= 0x7E) {
				return process_csi(st, (char)b);
			}
			/* Parameter / intermediate byte — buffer it. */
			if (st->csi_len < (int)sizeof(st->csi_buf) - 1) {
				st->csi_buf[st->csi_len++] = (char)b;
			}
			return PALETTE_DONE_NONE;
		}

		/* Pending bare ESC plus non-'[' byte: flush ESC as cancel/close,
		 * then re-process the trailing byte by looping.
		 *
		 * Filter/arg-clear-on-ESC contract: if the deepest level has a
		 * non-empty filter or arg buffer, the bare ESC clears it and
		 * stops there — the level itself is not closed. Only a
		 * subsequent ESC (or one with an empty buffer) collapses the
		 * level. This is the standard "first ESC clears, second ESC
		 * exits" pattern. */
		if (st->esc_pending && b != '[') {
			st->esc_pending = false;
			if (st->open_depth > 0) {
				if (st->arg_len > 0) {
					st->arg_len = 0;
					st->arg_buf[0] = '\0';
					continue;
				}
				if (st->filter_len > 0) {
					st->filter_len = 0;
					st->filter_buf[0] = '\0';
					palette_level_t *lvl = &st->levels[st->open_depth - 1];
					level_recompute_visible(lvl, NULL, 0);
					int win = level_window_rows(
						lvl, level_cursor_accepts_args(lvl));
					level_scroll_to_cursor(lvl, win);
					continue;
				}
				close_deepest_level(st);
				/* Continue loop to process `b` as a fresh byte. */
				continue;
			}
			/* ESC at top — cancel takes precedence; the trailing byte
			 * is dropped. (Matches the historical recursive behavior
			 * when the inner call returned DONE_CANCEL.) */
			return PALETTE_DONE_CANCEL;
		}

		if (b == 0x1b) {
			st->esc_pending = true;
			return PALETTE_DONE_NONE;
		}

		if (b == '\r' || b == '\n') {
			if (st->open_depth == 0) {
				/* ENTER on menubar opens highlighted menu. */
				open_level(st, 0);
				return PALETTE_DONE_NONE;
			}
			return activate_cursor_item(st);
		}

		/* Backspace (DEL = 0x7f, BS = 0x08): when a menu is open and
		 * arg/filter buffer is non-empty, remove the last character.
		 * Arg buffer takes precedence when the cursor is on an
		 * accepts_args item — typing on such an item builds the arg
		 * row, not the filter. */
		if (b == 0x7f || b == 0x08) {
			if (st->open_depth > 0) {
				palette_level_t *lvl = &st->levels[st->open_depth - 1];
				if (level_cursor_accepts_args(lvl) && st->arg_len > 0) {
					st->arg_len--;
					st->arg_buf[st->arg_len] = '\0';
				} else if (st->filter_len > 0) {
					st->filter_len--;
					st->filter_buf[st->filter_len] = '\0';
					level_recompute_visible(lvl, st->filter_buf, st->filter_len);
					int win = level_window_rows(
						lvl, level_cursor_accepts_args(lvl));
					level_scroll_to_cursor(lvl, win);
				}
			}
			return PALETTE_DONE_NONE;
		}

		/* Menubar (no menu open): letters trigger hotkey acceleration
		 * to open the matching top-level menu. This preserves the
		 * VisiCalc-style "press a letter to fly to a menu" UX while
		 * keeping the filter / arg-row mechanism scoped to inside an
		 * open menu. */
		if (st->open_depth == 0) {
			if (isalpha((unsigned char)b)) {
				int idx = find_hotkey_in_menubar(st, (char)b);
				if (idx >= 0) {
					st->menubar_cursor = idx;
					open_level(st, 0);
				}
			}
			return PALETTE_DONE_NONE;
		}

		/* Inside an open menu: printable bytes go to the arg buffer
		 * (when cursor is on an accepts_args item) or to the filter
		 * buffer (otherwise). Both buffers accept any printable ASCII
		 * — alphanumerics, punctuation, space — but the arg buffer
		 * doesn't strip whitespace because the dispatched argument
		 * may legitimately contain spaces (e.g. `/list a/b c`).
		 *
		 * Filter input is restricted to alphanumerics + a few common
		 * label characters (space, period, dash, underscore) so
		 * pressing a stray punctuation key doesn't enter the filter.
		 * This list intentionally matches the characters we expect
		 * inside menu labels. */
		if (b >= 0x20 && b < 0x7f) {
			palette_level_t *lvl = &st->levels[st->open_depth - 1];
			if (level_cursor_accepts_args(lvl)) {
				if (st->arg_len < (int)sizeof(st->arg_buf) - 1) {
					st->arg_buf[st->arg_len++] = (char)b;
					st->arg_buf[st->arg_len] = '\0';
				}
				return PALETTE_DONE_NONE;
			}
			/* Filter accepts alphanum + a small allow-list of label
			 * characters. */
			bool ok_for_filter = isalnum((unsigned char)b) ||
			                     b == ' ' || b == '.' ||
			                     b == '-' || b == '_';
			if (!ok_for_filter) return PALETTE_DONE_NONE;
			if (st->filter_len < (int)sizeof(st->filter_buf) - 1) {
				st->filter_buf[st->filter_len++] = (char)b;
				st->filter_buf[st->filter_len] = '\0';
				level_recompute_visible(lvl, st->filter_buf, st->filter_len);
				int win = level_window_rows(
					lvl, level_cursor_accepts_args(lvl));
				level_scroll_to_cursor(lvl, win);
			}
			return PALETTE_DONE_NONE;
		}

		/* Anything else: ignore. */
		return PALETTE_DONE_NONE;
	}
}

palette_done_t palette_feed_esc_timeout(palette_state_t *st) {
	if (!st || !st->active) return PALETTE_DONE_NONE;
	if (!st->esc_pending) return PALETTE_DONE_NONE;
	st->esc_pending = false;

	if (st->open_depth > 0) {
		/* Mirrors the byte-feed ESC contract: clear arg buffer or
		 * filter buffer first, only collapse the deepest level if
		 * both are empty. */
		if (st->arg_len > 0) {
			st->arg_len = 0;
			st->arg_buf[0] = '\0';
			return PALETTE_DONE_NONE;
		}
		if (st->filter_len > 0) {
			st->filter_len = 0;
			st->filter_buf[0] = '\0';
			palette_level_t *lvl = &st->levels[st->open_depth - 1];
			level_recompute_visible(lvl, NULL, 0);
			int win = level_window_rows(
				lvl, level_cursor_accepts_args(lvl));
			level_scroll_to_cursor(lvl, win);
			return PALETTE_DONE_NONE;
		}
		close_deepest_level(st);
		return PALETTE_DONE_NONE;
	}
	return PALETTE_DONE_CANCEL;
}

palette_done_t palette_feed_mouse(palette_state_t *st, const mouse_event_t *ev) {
	if (!st || !st->active || !ev) return PALETTE_DONE_NONE;
	/* Mouse coords are 1-based per ANSI/SGR convention. Reject anything
	 * <= 0 outright — converting to 0-based would wrap into negatives
	 * and let bogus events slip past the per-pane bounds checks below. */
	if (ev->x < 1 || ev->y < 1) return PALETTE_DONE_NONE;

	/* Wheel scroll: scrolls the deepest open level. Press-only events
	 * (xterm typically reports wheel as press without a release). The
	 * wheel scrolls one item per tick — same as one UP/DOWN arrow. */
	if (ev->btn == MOUSE_WHEEL_UP || ev->btn == MOUSE_WHEEL_DOWN) {
		if (st->open_depth <= 0) return PALETTE_DONE_NONE;
		palette_level_t *lvl = &st->levels[st->open_depth - 1];
		int win = level_window_rows(lvl, level_cursor_accepts_args(lvl));
		(void)win;
		/* Scroll without changing cursor. If the cursor leaves the
		 * window we leave it where it was — wheel scroll is for
		 * looking around without moving selection. The user can
		 * resume keyboard navigation to move the cursor back. */
		int delta = (ev->btn == MOUSE_WHEEL_UP) ? -1 : +1;
		int new_top = lvl->scroll_top + delta;
		int max_top = imax(0, lvl->visible_count - level_window_rows(
			lvl, level_cursor_accepts_args(lvl)));
		if (new_top < 0) new_top = 0;
		if (new_top > max_top) new_top = max_top;
		lvl->scroll_top = new_top;
		return PALETTE_DONE_NONE;
	}

	if (!ev->press) return PALETTE_DONE_NONE;        /* react on press only */
	if (ev->btn != MOUSE_LEFT) return PALETTE_DONE_NONE;

	int mx = ev->x - 1;        /* convert 1-based to 0-based */
	int my = ev->y - 1;

	/* Menubar hit? */
	if (my == 0) {
		for (int i = 0; i < st->menu_count; ++i) {
			if (mx >= st->menu_x[i] && mx < st->menu_x[i] + st->menu_w[i]) {
				close_all_levels(st);
				st->menubar_cursor = i;
				open_level(st, 0);
				return PALETTE_DONE_NONE;
			}
		}
	}

	/* Hit-test the open levels — front-to-back (deepest first). */
	for (int d = st->open_depth - 1; d >= 0; --d) {
		palette_level_t *lvl = &st->levels[d];
		pane_t *p = &lvl->pane;
		if (mx < p->x || mx >= p->x + p->w) continue;
		if (my < p->y || my >= p->y + p->h) continue;

		/* Inside this pane. Compute which visible-row was clicked.
		 * Item rows occupy 1..(item_rows) inside top border, where
		 * item_rows accounts for the optional arg input row reserved
		 * on the deepest level. */
		bool reserve_arg = (d == st->open_depth - 1) &&
		                   level_cursor_accepts_args(lvl);
		int item_rows = level_window_rows(lvl, reserve_arg);
		int local_y = my - p->y;
		int last_item_row = item_rows;        /* rows are 1..item_rows */
		if (local_y <= 0 || local_y > last_item_row) {
			/* Border click (or arg-row / footer click). Behavior split:
			 *   - On the deepest pane: no-op, pane stays open. The
			 *     user may have just been imprecise; collapsing on a
			 *     stray border click would feel hostile.
			 *   - On a non-deepest (ancestor) pane: collapse to that
			 *     level. The user reached past the deepest cascade to
			 *     an ancestor's frame, which reads as "I want to focus
			 *     this level again". This matches the keyboard LEFT
			 *     semantic of "close one cascade". */
			if (d < st->open_depth - 1) {
				while (st->open_depth - 1 > d) close_deepest_level(st);
			}
			return PALETTE_DONE_NONE;
		}
		int vis_row = local_y - 1 + lvl->scroll_top;
		if (vis_row < 0 || vis_row >= lvl->visible_count)
			return PALETTE_DONE_NONE;

		/* Close any deeper cascades that are no longer the "current"
		 * branch. */
		while (st->open_depth - 1 > d) close_deepest_level(st);

		lvl->cursor = lvl->visible[vis_row];
		return activate_cursor_item(st);
	}

	/* Click outside everything — cancel. */
	return PALETTE_DONE_CANCEL;
}

void palette_render_state(palette_state_t *st) {
	if (!st || !st->active) return;
	render_menubar(st);
	for (int d = 0; d < st->open_depth; ++d) {
		render_level(st, d);
	}
}

void palette_on_resize(palette_state_t *st, int term_rows, int term_cols) {
	if (!st || !st->active) return;
	if (term_cols < 4 || term_rows < 2) return;
	st->term_rows = term_rows;
	st->term_cols = term_cols;

	/* Resize menubar pane. */
	pane_resize(&st->menubar, term_cols, 1);

	/* Clamp menubar_cursor in case menu_count overflowed (it doesn't,
	 * but if a future palette_refresh shrinks the bar this matters). */
	if (st->menubar_cursor >= st->menu_count) {
		st->menubar_cursor = imax(0, st->menu_count - 1);
	}

	/* Reposition every open level — cascade geometry depends on
	 * term_rows/term_cols. We re-place each pane in turn using its
	 * INTRINSIC width/height (the natural size from item labels +
	 * padding/border) rather than its current possibly-clamped size, so
	 * that shrinking and then re-growing the terminal restores the
	 * pane's natural width.
	 *
	 * If a level no longer fits on screen (place_cascade_pane returns
	 * false because the post-clamp w<4 or h<3), close that level and
	 * everything deeper. */
	for (int d = 0; d < st->open_depth; /* incremented inside */) {
		palette_level_t *lvl = &st->levels[d];
		int w = compute_menu_pane_width(lvl->items, lvl->item_count);
		bool any_args = items_any_accepts_args(lvl->items, lvl->item_count);
		int h = compute_menu_pane_height(lvl->item_count, any_args);
		int saved_depth = st->open_depth;
		st->open_depth = d + 1;
		int px = 0, py = 0;
		int pw = 0, ph = 0;
		bool fits = place_cascade_pane(st, d, w, h, &px, &py, &pw, &ph);
		st->open_depth = saved_depth;
		if (!fits) {
			/* This level no longer fits. Close it and any deeper
			 * levels and stop. */
			while (st->open_depth > d) close_deepest_level(st);
			break;
		}
		pane_move(&lvl->pane, px, py);
		pane_resize(&lvl->pane, pw, ph);
		if (lvl->cursor >= lvl->item_count) {
			lvl->cursor = imax(0, lvl->item_count - 1);
		}
		++d;
	}
}
