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
static int iclamp(int v, int lo, int hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
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

static int compute_menu_pane_height(int n) {
	if (n < 1) n = 1;
	return n + 2;               /* top/bottom border */
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
		st->menu_hotkeys[i] = hk;
		st->menu_x[i] = x;
		st->menu_w[i] = menubar_entry_width(st->menu_labels[i]);
		x += st->menu_w[i];
	}
}

/* Render the menubar pane. The cursor entry is INVERSE; the hotkey
 * character within each entry is BOLD. */
static void render_menubar(palette_state_t *st) {
	pane_clear(&st->menubar);
	for (int i = 0; i < st->menu_count; ++i) {
		const char *label = st->menu_labels[i];
		int x = st->menu_x[i];
		int w = st->menu_w[i];
		uint8_t base = (i == st->menubar_cursor) ? PALETTE_ATTR_INVERSE : 0;

		/* Leading space. */
		pane_putc(&st->menubar, x, 0, ' ', base);
		int lx = x + 1;
		char hk = st->menu_hotkeys[i];
		for (const char *p = label; *p; ++p, ++lx) {
			uint8_t a = base;
			if (hk && *p == hk) a |= PALETTE_ATTR_BOLD;
			pane_putc(&st->menubar, lx, 0, (uint32_t)(unsigned char)*p, a);
		}
		/* Trailing space. */
		pane_putc(&st->menubar, x + w - 1, 0, ' ', base);
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

static void render_level(palette_state_t *st, int depth) {
	palette_level_t *lvl = &st->levels[depth];
	pane_t *p = &lvl->pane;
	pane_clear(p);

	/* Border attribute: BOLD if this is the deepest open level (focused),
	 * DIM otherwise. The deepest level is index (open_depth - 1). */
	bool focused = (depth == st->open_depth - 1);
	uint8_t border_attr = focused ? PALETTE_ATTR_BOLD : PALETTE_ATTR_DIM;

	/* Top + bottom borders. */
	pane_putc(p, 0, 0, BORDER_TL, border_attr);
	pane_putc(p, p->w - 1, 0, BORDER_TR, border_attr);
	pane_putc(p, 0, p->h - 1, BORDER_BL, border_attr);
	pane_putc(p, p->w - 1, p->h - 1, BORDER_BR, border_attr);
	for (int x = 1; x < p->w - 1; ++x) {
		pane_putc(p, x, 0, BORDER_H, border_attr);
		pane_putc(p, x, p->h - 1, BORDER_H, border_attr);
	}
	for (int y = 1; y < p->h - 1; ++y) {
		pane_putc(p, 0, y, BORDER_V, border_attr);
		pane_putc(p, p->w - 1, y, BORDER_V, border_attr);
	}

	/* Items. */
	int max_visible = p->h - 2;
	int n = imin(lvl->item_count, max_visible);
	for (int i = 0; i < n; ++i) {
		const palette_item_t *it = &lvl->items[i];
		int row = i + 1;        /* inside top border */
		uint8_t base = 0;
		if (i == lvl->cursor) base |= PALETTE_ATTR_INVERSE;
		if (!it->enabled)     base |= PALETTE_ATTR_DIM;

		/* Pad the row with spaces (under base attr) so the inverse
		 * highlight extends across the full inside-border width. */
		for (int x = 1; x < p->w - 1; ++x) {
			pane_putc(p, x, row, ' ', base);
		}

		/* Label, with the hotkey letter rendered BOLD. We left-pad by
		 * one column so labels don't bump up against the border. */
		int lx = 2;
		char hk = it->shortcut;
		bool hk_seen = false;
		for (const char *q = it->label; *q && lx < p->w - 1; ++q, ++lx) {
			uint8_t a = base;
			/* First match-only of the hotkey letter is bolded. */
			if (!hk_seen && hk && *q == hk) {
				a |= PALETTE_ATTR_BOLD;
				hk_seen = true;
			}
			pane_putc(p, lx, row, (uint32_t)(unsigned char)*q, a);
		}

		/* Submenu indicator on the right side. */
		if (it->is_submenu) {
			pane_putc(p, p->w - 2, row, '>', base);
		}
	}
}

/* Compute the natural placement of a cascade level given its parent
 * geometry and term bounds. Implements the LEFT / UP overflow rules. */
static void place_cascade_pane(palette_state_t *st, int depth,
                               int w, int h, int *out_x, int *out_y) {
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
		*out_x = ax;
		*out_y = ay;
		return;
	}

	/* Submenu: cascade off the parent at the cursor row. */
	const palette_level_t *parent = &st->levels[depth - 1];
	int default_x = parent->pane.x + parent->pane.w;
	int default_y = parent->pane.y + 1 + parent->cursor;

	int x = default_x;
	int y = default_y;

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
	*out_x = x;
	*out_y = y;
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
		if (tmp.hidden) continue;
		lvl->items[kept++] = tmp;
	}
	lvl->item_count = kept;
	lvl->cursor = 0;

	int w = compute_menu_pane_width(lvl->items, lvl->item_count);
	int h = compute_menu_pane_height(lvl->item_count);
	int px = 0, py = 0;
	/* Use the prospective open_depth so place_cascade_pane sees this
	 * level as the current focus context. */
	int saved_depth = st->open_depth;
	st->open_depth = depth + 1;
	place_cascade_pane(st, depth, w, h, &px, &py);
	st->open_depth = saved_depth;

	pane_init(&lvl->pane, px, py, w, h);
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

	/* Initialise menubar pane: row 0, full width, 1 row tall, no border. */
	pane_init(&st->menubar, 0, 0, st->term_cols, 1);
	compositor_register(&st->menubar);
	st->menubar_cursor = 0;
	st->open_depth = 0;
	st->active = true;
	st->esc_pending = false;
	st->csi_len = 0;
	st->exec_script = NULL;
	return true;
}

void palette_close(palette_state_t *st) {
	if (!st || !st->active) {
		if (st) st->active = false;
		return;
	}
	close_all_levels(st);
	compositor_unregister(&st->menubar);
	pane_destroy(&st->menubar);
	memset(st->menu_labels, 0, sizeof(st->menu_labels));
	st->active = false;
}

/* Find item index in the deepest open level matching uppercase letter. */
static int find_hotkey_in_deepest(const palette_state_t *st, char letter) {
	if (st->open_depth <= 0) return -1;
	const palette_level_t *lvl = &st->levels[st->open_depth - 1];
	char want = (char)toupper((unsigned char)letter);
	for (int i = 0; i < lvl->item_count; ++i) {
		char hk = lvl->items[i].shortcut;
		if (!hk) continue;
		if ((char)toupper((unsigned char)hk) == want) return i;
	}
	return -1;
}

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
 * Disabled items are no-ops. */
static palette_done_t activate_cursor_item(palette_state_t *st) {
	if (st->open_depth <= 0) return PALETTE_DONE_NONE;
	palette_level_t *lvl = &st->levels[st->open_depth - 1];
	if (lvl->cursor < 0 || lvl->cursor >= lvl->item_count) return PALETTE_DONE_NONE;
	const palette_item_t *it = &lvl->items[lvl->cursor];
	if (!it->enabled) return PALETTE_DONE_NONE;

	if (it->is_submenu) {
		open_level(st, st->open_depth);
		return PALETTE_DONE_NONE;
	}
	st->exec_script = it->script_handle;
	return PALETTE_DONE_EXECUTE;
}

/* Process a CSI terminator. csi_buf holds the bytes between '[' and the
 * terminator; this function consumes the buffer. */
static palette_done_t process_csi(palette_state_t *st, char term) {
	st->csi_len = 0;
	switch (term) {
	case 'A': /* UP */
		if (st->open_depth > 0) {
			palette_level_t *lvl = &st->levels[st->open_depth - 1];
			if (lvl->cursor > 0) lvl->cursor--;
		}
		return PALETTE_DONE_NONE;
	case 'B': /* DOWN */
		if (st->open_depth > 0) {
			palette_level_t *lvl = &st->levels[st->open_depth - 1];
			if (lvl->cursor < lvl->item_count - 1) lvl->cursor++;
		}
		return PALETTE_DONE_NONE;
	case 'C': /* RIGHT */
		if (st->open_depth == 0) {
			if (st->menubar_cursor < st->menu_count - 1) st->menubar_cursor++;
		} else {
			/* If on a submenu item, open the cascade. Otherwise
			 * collapse current cascade(s) and move to next menubar
			 * entry, opening that menu (VisiCalc convention). */
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

	/* Mid-CSI: collect bytes until terminator. */
	if (st->csi_len > 0 || (st->esc_pending && b == '[')) {
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

	/* Pending bare ESC plus non-'[' byte: flush ESC as cancel/close. */
	if (st->esc_pending && b != '[') {
		st->esc_pending = false;
		palette_done_t r;
		if (st->open_depth > 0) {
			close_deepest_level(st);
			r = PALETTE_DONE_NONE;
		} else {
			r = PALETTE_DONE_CANCEL;
		}
		/* Re-feed the byte that wasn't '['. (Recursion bounded: we
		 * just cleared esc_pending and csi_len is 0.) */
		palette_done_t r2 = palette_feed_byte(st, b);
		if (r == PALETTE_DONE_CANCEL) return r;
		return r2;
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

	/* Printable letter — hotkey jump. Letters [A-Za-z] map to menubar
	 * (when no menu is open) or to the deepest open level. */
	if (isalpha((unsigned char)b)) {
		if (st->open_depth == 0) {
			int idx = find_hotkey_in_menubar(st, (char)b);
			if (idx >= 0) {
				st->menubar_cursor = idx;
				open_level(st, 0);
			}
			return PALETTE_DONE_NONE;
		}
		int idx = find_hotkey_in_deepest(st, (char)b);
		if (idx >= 0) {
			palette_level_t *lvl = &st->levels[st->open_depth - 1];
			if (lvl->items[idx].enabled) {
				lvl->cursor = idx;
				return activate_cursor_item(st);
			}
		}
		return PALETTE_DONE_NONE;
	}

	/* Anything else: ignore. */
	return PALETTE_DONE_NONE;
}

palette_done_t palette_feed_esc_timeout(palette_state_t *st) {
	if (!st || !st->active) return PALETTE_DONE_NONE;
	if (!st->esc_pending) return PALETTE_DONE_NONE;
	st->esc_pending = false;

	if (st->open_depth > 0) {
		close_deepest_level(st);
		return PALETTE_DONE_NONE;
	}
	return PALETTE_DONE_CANCEL;
}

palette_done_t palette_feed_mouse(palette_state_t *st, const mouse_event_t *ev) {
	if (!st || !st->active || !ev) return PALETTE_DONE_NONE;
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

		/* Inside this pane. Compute which item row was hit. Items
		 * occupy rows pane.y+1 ... pane.y+h-2 (inside borders). */
		int local_y = my - p->y;
		if (local_y <= 0 || local_y >= p->h - 1) {
			/* Border click — ignored, but pane stays open. */
			return PALETTE_DONE_NONE;
		}
		int item = local_y - 1;
		if (item < 0 || item >= lvl->item_count) return PALETTE_DONE_NONE;

		/* Close any deeper cascades that are no longer the "current"
		 * branch. */
		while (st->open_depth - 1 > d) close_deepest_level(st);

		lvl->cursor = item;
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
	 * term_rows/term_cols. We re-place each pane in turn. */
	for (int d = 0; d < st->open_depth; ++d) {
		palette_level_t *lvl = &st->levels[d];
		int w = lvl->pane.w;
		int h = lvl->pane.h;
		int saved_depth = st->open_depth;
		st->open_depth = d + 1;
		int px = 0, py = 0;
		place_cascade_pane(st, d, w, h, &px, &py);
		st->open_depth = saved_depth;
		pane_move(&lvl->pane, px, py);
		if (lvl->cursor >= lvl->item_count) {
			lvl->cursor = imax(0, lvl->item_count - 1);
		}
	}
	(void)iclamp;  /* iclamp reserved for future use */
}
