/*
 * palette.c - REPL slash-menu palette state machine and horizontal
 * cascade renderer.
 *
 * See palette.h for the public API contract.
 *
 * Design summary
 * --------------
 *  - Menus and items come in via palette_menu_source_t (a small vtable).
 *  - The cache is built once per palette_open() and never re-walked
 *    during navigation.
 *  - The menubar and every open cascade level is a SINGLE-ROW full-width
 *    strip.  The menubar lives at row prompt_row+1; cascade level d
 *    lives at row prompt_row+1+(d+1).  No borders, no boxes.
 *  - Items are laid out left-to-right with two leading spaces, two
 *    spaces between items, and 4 cells reserved per item for the
 *    selection brackets `[ label ]` so neighbor positions do not jitter
 *    when the cursor moves.
 *  - Type-to-select-and-activate: pressing an ASCII letter that matches
 *    a hotkey on the current level activates that item (open submenu or
 *    fire leaf script) in one keystroke.
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.  See pane.h for the full
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

/* Menubar layout: every entry is " Label " -- single spaces around the
 * label so adjacent menubar entries do not visually fuse. */
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
		/* Defensive NUL-termination -- vtable spec requires sources to
		 * NUL-terminate but enforce it locally too so subsequent
		 * strlen/loops are always safe. */
		st->menu_labels[i][sizeof(st->menu_labels[i]) - 1] = '\0';
		st->menu_hotkeys[i] = hk;
		st->menu_x[i] = x;
		st->menu_w[i] = menubar_entry_width(st->menu_labels[i]);
		x += st->menu_w[i];
	}
}

/* ANSI 16-color theme for the palette.
 *
 *   - Menubar / strip background:  cyan-on-blue
 *   - Selected item:               black-on-white (terminal inverse-style)
 *   - Hotkey letter (unselected):  bright yellow on the strip bg, BOLD
 *   - Hotkey letter (selected):    BOLD only (no color override)
 *   - Disabled item:               dim bright-black on the strip bg
 */
#define PT_FG_MENUBAR       PALETTE_COLOR_BRIGHT_CYAN
#define PT_BG_MENUBAR       PALETTE_COLOR_BLUE
#define PT_FG_SELECTED      PALETTE_COLOR_BLACK
#define PT_BG_SELECTED      PALETTE_COLOR_WHITE
#define PT_FG_HOTKEY        PALETTE_COLOR_BRIGHT_YELLOW
#define PT_FG_DISABLED      PALETTE_COLOR_BRIGHT_BLACK

/* ---------- Item geometry (horizontal cascade) ---------- */

/*
 * Each item reserves 4 cells around its label: leading bracket, leading
 * space, label..., trailing space, trailing bracket.  When the item is
 * NOT selected the brackets render as spaces, so the visual is
 * "  label  "; when it IS selected the brackets render so it becomes
 * "[ label ]".  Reserving the same 4 cells either way keeps neighbour
 * items from jittering left/right as the cursor moves.
 *
 * Two leading spaces precede the first item; two spaces separate
 * adjacent items.
 */
static int item_cell_width(const char *label) {
	return (int)strlen(label) + 4;
}

/* Compute the screen-x of item `i` inside a level's strip (relative to
 * the strip pane's left edge), accounting for the 2-space lead-in and
 * the 2-space separator between items. */
static int item_screen_x(const palette_level_t *lvl, int i) {
	int x = 2;
	for (int k = 0; k < i; ++k) {
		x += item_cell_width(lvl->items[k].label) + 2;
	}
	return x;
}

/* Render the menubar pane.  Menubar entries do not use the
 * bracket-reservation scheme -- they only render " Label " around each
 * label.  The selected entry is INVERSE; the hotkey letter inside each
 * entry is BOLD (plus bright-yellow when the entry is not selected). */
static void render_menubar(palette_state_t *st) {
	pane_clear(&st->menubar);
	/* Background fill so the area between entries shows menubar bg. */
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

		pane_putc_color(&st->menubar, x, 0, ' ', fg, bg, base);
		int lx = x + 1;
		char hk = st->menu_hotkeys[i];
		bool hk_seen = false;
		for (const char *p = label; *p && lx < st->menubar.w; ++p, ++lx) {
			uint8_t a = base;
			uint8_t cell_fg = fg;
			if (!hk_seen && hk && *p == hk) {
				a |= PALETTE_ATTR_BOLD;
				if (!selected) cell_fg = PT_FG_HOTKEY;
				hk_seen = true;
			}
			pane_putc_color(&st->menubar, lx, 0,
			                (uint32_t)(unsigned char)*p, cell_fg, bg, a);
		}
		if (x + w - 1 < st->menubar.w) {
			pane_putc_color(&st->menubar, x + w - 1, 0, ' ', fg, bg, base);
		}
	}
}

/* Render one cascade level as a single-row horizontal strip.  Pane is
 * full-width, height 1.  Items are stamped left-to-right with bracket
 * reservation so neighbour cells stay put as selection moves. */
static void render_level(palette_state_t *st, int depth) {
	palette_level_t *lvl = &st->levels[depth];
	pane_t *p = &lvl->pane;
	pane_clear(p);

	/* Background fill for the whole strip. */
	for (int x = 0; x < p->w; ++x) {
		pane_putc_color(p, x, 0, ' ', PT_FG_MENUBAR, PT_BG_MENUBAR, 0);
	}

	for (int i = 0; i < lvl->item_count; ++i) {
		const palette_item_t *it = &lvl->items[i];
		bool selected = (i == lvl->cursor);
		uint8_t base = selected ? PALETTE_ATTR_INVERSE : 0;
		uint8_t fg = selected ? PT_FG_SELECTED
		                      : (it->enabled ? PT_FG_MENUBAR : PT_FG_DISABLED);
		uint8_t bg = selected ? PT_BG_SELECTED : PT_BG_MENUBAR;
		if (!it->enabled) base |= PALETTE_ATTR_DIM;

		int lx = item_screen_x(lvl, i);
		if (lx >= p->w) break;       /* overflow: stop, no pagination yet */
		int label_len = (int)strlen(it->label);
		int total_w = label_len + 4;
		/* Truncation: if this item would run past the strip's right edge
		 * just stop here.  Pagination is a follow-up. */
		if (lx + total_w > p->w) break;

		/* Cell 0: opening bracket (space when not selected). */
		pane_putc_color(p, lx, 0, selected ? '[' : ' ', fg, bg, base);
		/* Cell 1: leading space. */
		pane_putc_color(p, lx + 1, 0, ' ', fg, bg, base);
		/* Cells 2..2+label_len: label, with hotkey letter styling. */
		char hk = it->shortcut;
		bool hk_seen = false;
		for (int k = 0; k < label_len; ++k) {
			uint8_t a = base;
			uint8_t cell_fg = fg;
			char ch = it->label[k];
			if (!hk_seen && hk && ch == hk) {
				a |= PALETTE_ATTR_BOLD;
				if (!selected && it->enabled) cell_fg = PT_FG_HOTKEY;
				hk_seen = true;
			}
			pane_putc_color(p, lx + 2 + k, 0,
			                (uint32_t)(unsigned char)ch, cell_fg, bg, a);
		}
		/* Trailing space. */
		pane_putc_color(p, lx + 2 + label_len, 0, ' ', fg, bg, base);
		/* Closing bracket (space when not selected). */
		pane_putc_color(p, lx + 3 + label_len, 0,
		                selected ? ']' : ' ', fg, bg, base);
	}
}

/* Place a cascade level: full-width strip one row below the menubar
 * (for depth 0) or one row below the parent level (for depth >= 1).
 * Returns false if the strip cannot fit (y >= term_rows). */
static bool place_cascade_strip(palette_state_t *st, int depth,
                                int *out_x, int *out_y,
                                int *out_w, int *out_h) {
	int y = st->menubar.y + 1 + depth;
	if (y >= st->term_rows) return false;
	*out_x = 0;
	*out_y = y;
	*out_w = st->term_cols;
	*out_h = 1;
	return true;
}

/* Open a level and populate it from the data source.  depth 0 = top
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

	int total = st->source->item_count(st->source->ctx, menu_index,
	                                   parent_opaque);
	if (total < 0) total = 0;
	int n = total < PALETTE_LEVEL_MAX_ITEMS ? total : PALETTE_LEVEL_MAX_ITEMS;
	int kept = 0;
	for (int i = 0; i < n; ++i) {
		palette_item_t tmp;
		if (!st->source->item_describe(st->source->ctx, menu_index,
		                               parent_opaque, i, &tmp)) {
			continue;
		}
		tmp.label[sizeof(tmp.label) - 1] = '\0';
		tmp.description[sizeof(tmp.description) - 1] = '\0';
		if (tmp.hidden) continue;
		lvl->items[kept++] = tmp;
	}
	lvl->item_count = kept;
	lvl->cursor = 0;
	lvl->scroll_top = 0;
	/* visible[] held over from the legacy filter UI is unused by the
	 * horizontal renderer but we keep the field populated as an identity
	 * map for any external inspector. */
	for (int i = 0; i < kept; ++i) lvl->visible[i] = i;
	lvl->visible_count = kept;

	int px = 0, py = 0, pw = 0, ph = 0;
	if (!place_cascade_strip(st, depth, &px, &py, &pw, &ph)) {
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
}

static void close_all_levels(palette_state_t *st) {
	while (st->open_depth > 0) close_deepest_level(st);
}

/* ---------- Public API ---------- */

bool palette_open(palette_state_t *st, int term_rows, int term_cols,
                  int prompt_row,
                  const palette_menu_source_t *source) {
	if (!st || !source) return false;
	if (term_cols < 4 || term_rows < 2) return false;

	memset(st, 0, sizeof(*st));
	st->source = source;
	st->term_rows = term_rows;
	st->term_cols = term_cols;

	/* Clamp prompt_row into the visible range first. */
	if (prompt_row < 0) prompt_row = 0;
	if (prompt_row >= term_rows) prompt_row = term_rows - 1;

	/* Edge case: prompt sits on the bottom-most row, so there is no
	 * room below it for the menubar.  Emit a scroll-up so the prompt
	 * moves up one row, then anchor the menubar to the freed row. */
	if (prompt_row + 1 >= term_rows) {
		fputs("\x1b[1S", stderr);
		fflush(stderr);
		prompt_row -= 1;
		if (prompt_row < 0) prompt_row = 0;
	}
	st->prompt_row = prompt_row;

	int n = source->count_menus(source->ctx);
	if (n <= 0) return false;

	layout_menubar(st);
	if (st->menu_count <= 0) return false;

	/* Menubar strip: full-width, single row, one row below the prompt. */
	pane_init(&st->menubar, 0, prompt_row + 1, st->term_cols, 1);
	compositor_register(&st->menubar);
	st->active = true;
	st->menubar_cursor = 0;
	st->open_depth = 0;
	st->esc_pending = false;
	st->csi_len = 0;
	st->exec_script = NULL;
	st->exec_arg[0] = '\0';
	return true;
}

void palette_paint_teardown(palette_state_t *st) {
	if (!st) return;
	if (!st->active) return;
	/* pane_clear zeroes every cell (ch=0, fg=0, bg=0, attr=0). The
	 * compositor renders ch=0 as ' ' and emits no SGR for fg/bg/attr
	 * == 0 (default), so the next compositor_render() emits a "go back
	 * to terminal default" frame for every cell these panes cover. */
	for (int d = 0; d < st->open_depth && d < PALETTE_MAX_DEPTH; ++d) {
		pane_clear(&st->levels[d].pane);
	}
	pane_clear(&st->menubar);
}

void palette_close(palette_state_t *st) {
	if (!st) return;
	if (!st->active) return;
	close_all_levels(st);
	compositor_unregister(&st->menubar);
	pane_destroy(&st->menubar);
	memset(st->menu_labels, 0, sizeof(st->menu_labels));
	st->active = false;
}

/* Find an item index on `level` whose hotkey matches `letter` (case
 * insensitive).  `depth` == 0 searches the menubar; `depth` >= 1
 * searches that cascade level's items[]. Returns -1 on no match. */
static int find_hotkey_in_level(const palette_state_t *st, int depth,
                                char letter) {
	char want = (char)toupper((unsigned char)letter);
	if (depth == 0) {
		for (int i = 0; i < st->menu_count; ++i) {
			char hk = st->menu_hotkeys[i];
			if (!hk) continue;
			if ((char)toupper((unsigned char)hk) == want) return i;
		}
		return -1;
	}
	if (depth < 1 || depth > st->open_depth) return -1;
	const palette_level_t *lvl = &st->levels[depth - 1];
	for (int i = 0; i < lvl->item_count; ++i) {
		char hk = lvl->items[i].shortcut;
		if (!hk) continue;
		if ((char)toupper((unsigned char)hk) == want) return i;
	}
	return -1;
}

/* Activate the cursor item of the deepest open level. */
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
	/* arg_buf input UI removed with the h=1 strip layout; exec_arg is
	 * always empty for now.  Future work may reintroduce an arg row. */
	st->exec_arg[0] = '\0';
	return PALETTE_DONE_EXECUTE;
}

/* Move the deepest level's cursor by `delta`, clamping. */
static void cursor_step(palette_state_t *st, int delta) {
	if (st->open_depth <= 0) return;
	palette_level_t *lvl = &st->levels[st->open_depth - 1];
	if (lvl->item_count <= 0) return;
	int c = lvl->cursor + delta;
	if (c < 0) c = 0;
	if (c >= lvl->item_count) c = lvl->item_count - 1;
	lvl->cursor = c;
}

/* Process a CSI terminator.  csi_buf holds the bytes between '[' and
 * the terminator. */
static palette_done_t process_csi(palette_state_t *st, char term) {
	st->csi_len = 0;
	switch (term) {
	case 'A': /* UP */
		/* Horizontal strip: UP collapses the deepest cascade level. */
		if (st->open_depth > 0) {
			close_deepest_level(st);
		}
		return PALETTE_DONE_NONE;
	case 'B': /* DOWN */
		/* DOWN on an open submenu item drills into it; on a leaf this
		 * is a no-op (there is no "next level" for DOWN to descend
		 * into when nothing is open). */
		if (st->open_depth == 0) {
			open_level(st, 0);
		} else {
			palette_level_t *lvl = &st->levels[st->open_depth - 1];
			if (lvl->cursor >= 0 && lvl->cursor < lvl->item_count &&
			    lvl->items[lvl->cursor].is_submenu &&
			    lvl->items[lvl->cursor].enabled) {
				open_level(st, st->open_depth);
			}
		}
		return PALETTE_DONE_NONE;
	case 'C': /* RIGHT */
		if (st->open_depth == 0) {
			if (st->menubar_cursor < st->menu_count - 1) st->menubar_cursor++;
		} else {
			cursor_step(st, +1);
		}
		return PALETTE_DONE_NONE;
	case 'D': /* LEFT */
		if (st->open_depth == 0) {
			if (st->menubar_cursor > 0) st->menubar_cursor--;
		} else {
			cursor_step(st, -1);
		}
		return PALETTE_DONE_NONE;
	default:
		/* Unknown CSI -- ignore. */
		return PALETTE_DONE_NONE;
	}
}

palette_done_t palette_feed_byte(palette_state_t *st, unsigned char b) {
	if (!st || !st->active) return PALETTE_DONE_NONE;

	for (;;) {
		/* Mid-CSI: collect bytes until terminator. */
		if (st->csi_len > 0 || (st->esc_pending && b == '[')) {
			if (b == 0x1b) {
				st->csi_len = 0;
				st->esc_pending = true;
				return PALETTE_DONE_NONE;
			}
			if (st->esc_pending && b == '[') {
				st->esc_pending = false;
				st->csi_len = 1;
				st->csi_buf[0] = '[';
				return PALETTE_DONE_NONE;
			}
			if (b >= 0x40 && b <= 0x7E) {
				return process_csi(st, (char)b);
			}
			if (st->csi_len < (int)sizeof(st->csi_buf) - 1) {
				st->csi_buf[st->csi_len++] = (char)b;
			}
			return PALETTE_DONE_NONE;
		}

		/* Pending bare ESC plus a non-'[' byte: flush the ESC as a
		 * cancel/close, then re-process the trailing byte. */
		if (st->esc_pending && b != '[') {
			st->esc_pending = false;
			if (st->open_depth > 0) {
				close_deepest_level(st);
				continue;
			}
			return PALETTE_DONE_CANCEL;
		}

		if (b == 0x1b) {
			st->esc_pending = true;
			return PALETTE_DONE_NONE;
		}

		if (b == '\r' || b == '\n') {
			if (st->open_depth == 0) {
				open_level(st, 0);
				return PALETTE_DONE_NONE;
			}
			return activate_cursor_item(st);
		}

		/* Backspace: with the horizontal strip layout, BS at the
		 * deepest open cascade collapses one level (it mirrors LEFT in
		 * the vertical model). */
		if (b == 0x7f || b == 0x08) {
			if (st->open_depth > 0) {
				close_deepest_level(st);
			}
			return PALETTE_DONE_NONE;
		}

		/* Type-to-select-and-activate: at any level, a letter that
		 * matches a hotkey activates that item in one keystroke. */
		if (isalpha((unsigned char)b) || isdigit((unsigned char)b)) {
			int idx = find_hotkey_in_level(st, st->open_depth, (char)b);
			if (idx < 0) return PALETTE_DONE_NONE;
			if (st->open_depth == 0) {
				st->menubar_cursor = idx;
				open_level(st, 0);
				return PALETTE_DONE_NONE;
			}
			palette_level_t *lvl = &st->levels[st->open_depth - 1];
			lvl->cursor = idx;
			return activate_cursor_item(st);
		}

		return PALETTE_DONE_NONE;
	}
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

int palette_esc_timeout_ms(void) {
	const char *v = getenv("FRONTIER_PALETTE_FAST_TIMERS");
	if (v && v[0] != '\0') return PALETTE_ESC_TIMEOUT_FAST_MS;
	return PALETTE_ESC_TIMEOUT_DEFAULT_MS;
}

palette_done_t palette_feed_mouse(palette_state_t *st, const mouse_event_t *ev) {
	if (!st || !st->active || !ev) return PALETTE_DONE_NONE;
	if (ev->x < 1 || ev->y < 1) return PALETTE_DONE_NONE;

	/* Wheel scroll on horizontal strips: navigate the cursor on the
	 * deepest open level (or the menubar if nothing is open). */
	if (ev->btn == MOUSE_WHEEL_UP || ev->btn == MOUSE_WHEEL_DOWN) {
		int delta = (ev->btn == MOUSE_WHEEL_DOWN) ? +1 : -1;
		if (st->open_depth == 0) {
			int c = st->menubar_cursor + delta;
			if (c < 0) c = 0;
			if (c >= st->menu_count) c = st->menu_count - 1;
			st->menubar_cursor = c;
		} else {
			cursor_step(st, delta);
		}
		return PALETTE_DONE_NONE;
	}

	if (!ev->press) return PALETTE_DONE_NONE;
	if (ev->btn != MOUSE_LEFT) return PALETTE_DONE_NONE;

	int mx = ev->x - 1;
	int my = ev->y - 1;

	/* Menubar hit? */
	if (my == st->menubar.y) {
		for (int i = 0; i < st->menu_count; ++i) {
			if (mx >= st->menu_x[i] && mx < st->menu_x[i] + st->menu_w[i]) {
				close_all_levels(st);
				st->menubar_cursor = i;
				open_level(st, 0);
				return PALETTE_DONE_NONE;
			}
		}
		return PALETTE_DONE_NONE;
	}

	/* Cascade-strip hit: each strip is a single row at known y. */
	for (int d = st->open_depth - 1; d >= 0; --d) {
		palette_level_t *lvl = &st->levels[d];
		pane_t *p = &lvl->pane;
		if (my != p->y) continue;
		/* Find which item the click landed on. */
		for (int i = 0; i < lvl->item_count; ++i) {
			int lx = item_screen_x(lvl, i);
			int total_w = item_cell_width(lvl->items[i].label);
			if (mx >= lx && mx < lx + total_w) {
				/* Collapse any deeper levels first. */
				while (st->open_depth - 1 > d) close_deepest_level(st);
				lvl->cursor = i;
				return activate_cursor_item(st);
			}
		}
		/* Click on the strip's gap area: just no-op. */
		return PALETTE_DONE_NONE;
	}

	/* Click outside everything -- cancel. */
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

	/* Clamp the menubar's anchor if the new height made the old row
	 * fall off the bottom.  prompt_row never moves up automatically on
	 * resize (the REPL prompt position is not tracked), so the menubar
	 * stays at its original row unless that row would now be past the
	 * bottom -- in that case we slide it up. */
	int desired_y = st->prompt_row + 1;
	if (desired_y >= term_rows) {
		desired_y = term_rows - 1;
		st->prompt_row = desired_y - 1;
		if (st->prompt_row < 0) st->prompt_row = 0;
	}
	pane_move(&st->menubar, 0, desired_y);
	pane_resize(&st->menubar, term_cols, 1);

	if (st->menubar_cursor >= st->menu_count) {
		st->menubar_cursor = imax(0, st->menu_count - 1);
	}

	/* Reposition open cascade strips.  Each level lives at
	 * menubar.y + 1 + d; close any level that no longer fits. */
	for (int d = 0; d < st->open_depth; /* incremented inside */) {
		palette_level_t *lvl = &st->levels[d];
		int px = 0, py = 0, pw = 0, ph = 0;
		if (!place_cascade_strip(st, d, &px, &py, &pw, &ph)) {
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
