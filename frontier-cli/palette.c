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

/* Glyphs for legacy menu-text prefix codes (mereducemenucodes parity).
 *
 * CHECK_GLYPH is U+2714 HEAVY CHECK MARK, emitted as UTF-8 by the pane
 * compositor. It occupies a single terminal cell. SEP_GLYPH is the divider
 * drawn in place of a separator item ('-' line); a plain ASCII '|'. */
#define PALETTE_CHECK_GLYPH 0x2714u
#define PALETTE_SEP_GLYPH   ((uint32_t)'|')

/* Cells a checked item reserves to the LEFT of its label for the checkmark
 * glyph plus a trailing space (e.g. "[ X Wrap ]"). Unchecked items reserve
 * the same cells as blanks so neighbours don't jitter as checks toggle. */
#define PALETTE_CHECK_CELLS 2

/* Per-layer hotkey auto-derivation. Per the plan ("Hotkey auto-derivation
 * rule" in planning/discussions/repl-slash-menu-implementation-plan.md):
 * within a sibling list, each item's hotkey is the first letter of its
 * label that hasn't already been claimed by an earlier sibling at the
 * same level. Explicit overrides (non-zero `existing`) win and reserve
 * their letter against later siblings.
 *
 * `claimed` is a 256-byte bitmap of uppercase letters claimed within this
 * sibling list. `existing` is the source-provided shortcut (0 if none).
 * Returns the shortcut to use (uppercase letter, or 0 if none available).
 *
 * Why in palette.c rather than the source adapter: the per-layer scope
 * means we need the entire sibling list visible at once to compute
 * collisions. palette.c already iterates the siblings (menubar entries
 * at layout time, level items at open time); doing it here keeps the
 * data source free of UI-layer knowledge and works for ANY source
 * implementation, not just the ODB adapter. */
static char derive_hotkey(const char *label, char existing, bool *claimed) {
	if (existing) {
		unsigned char up = (unsigned char)toupper((unsigned char)existing);
		claimed[up] = true;
		return (char)up;
	}
	if (label == NULL) return '\0';
	for (const char *p = label; *p; ++p) {
		unsigned char c = (unsigned char)*p;
		if (!isalpha(c) && !isdigit(c)) continue;
		unsigned char up = (unsigned char)toupper(c);
		if (claimed[up]) continue;
		claimed[up] = true;
		return (char)up;
	}
	return '\0';
}

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
	/* First pass: collect labels + source-provided explicit hotkeys. */
	char src_hk[16];
	for (int i = 0; i < n; ++i) {
		char hk = '\0';
		st->source->menu_describe(st->source->ctx, i,
		                          st->menu_labels[i],
		                          sizeof(st->menu_labels[i]), &hk);
		/* Defensive NUL-termination -- vtable spec requires sources to
		 * NUL-terminate but enforce it locally too so subsequent
		 * strlen/loops are always safe. */
		st->menu_labels[i][sizeof(st->menu_labels[i]) - 1] = '\0';
		src_hk[i] = hk;
		st->menu_x[i] = x;
		st->menu_w[i] = menubar_entry_width(st->menu_labels[i]);
		x += st->menu_w[i];
	}
	/* Second pass: auto-derive per-layer. Explicit overrides claim
	 * first so later auto-derivations skip those letters. */
	bool claimed[256];
	memset(claimed, 0, sizeof(claimed));
	for (int i = 0; i < n; ++i) {
		if (src_hk[i]) {
			unsigned char up = (unsigned char)toupper((unsigned char)src_hk[i]);
			claimed[up] = true;
		}
	}
	for (int i = 0; i < n; ++i) {
		st->menu_hotkeys[i] = derive_hotkey(st->menu_labels[i], src_hk[i],
		                                    claimed);
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
 * A CHECKED item reserves PALETTE_CHECK_CELLS extra cells inside the
 * brackets for the checkmark glyph + space ("[ X label ]"). An UNCHECKED
 * item does NOT reserve them, since within a single menu either no items
 * are checkable or the rendering is per-item; the width is computed
 * per-item from its own `checked` flag so screen_x stays consistent.
 *
 * A SEPARATOR is not a label at all: it reserves a single divider cell.
 *
 * Two leading spaces precede the first item; two spaces separate
 * adjacent items.
 */
static int item_cell_width(const palette_item_t *it) {
	if (it->is_separator)
		return 1; /* single divider glyph */
	int w = (int)strlen(it->label) + 4;
	if (it->checked)
		w += PALETTE_CHECK_CELLS;
	return w;
}

/* Compute the screen-x of item `i` inside a level's strip (relative to
 * the strip pane's left edge), accounting for the 2-space lead-in and
 * the 2-space separator between items. */
static int item_screen_x(const palette_level_t *lvl, int i) {
	int x = 2;
	for (int k = 0; k < i; ++k) {
		x += item_cell_width(&lvl->items[k]) + 2;
	}
	return x;
}

/* Total laid-out width of a level's strip in cells (lead-in + all items +
 * inter-item separators), before any horizontal scroll is applied. */
static int level_content_width(const palette_level_t *lvl) {
	if (lvl->item_count <= 0) return 0;
	int last = lvl->item_count - 1;
	return item_screen_x(lvl, last) + item_cell_width(&lvl->items[last]);
}

/* Adjust lvl->hscroll so the cursor item is fully visible within a strip
 * `strip_w` cells wide.  Reserves one cell on each overflowing edge for the
 * '<' / '>' indicators so the indicator never hides part of the item.  Also
 * clamps hscroll so the strip never scrolls past its content. */
static void ensure_cursor_visible(palette_level_t *lvl, int strip_w) {
	if (lvl->item_count <= 0 || strip_w <= 0) {
		lvl->hscroll = 0;
		return;
	}
	int c = lvl->cursor;
	if (c < 0 || c >= lvl->item_count) return;

	int item_lx = item_screen_x(lvl, c);
	int item_w = item_cell_width(&lvl->items[c]);
	int content_w = level_content_width(lvl);

	/* Left edge: if the item starts before the visible window, scroll left.
	 * Reserve a cell for the '<' indicator when not at the very start. */
	int left_margin = (item_lx > 0) ? 1 : 0;
	if (item_lx - lvl->hscroll < left_margin) {
		lvl->hscroll = item_lx - left_margin;
	}

	/* Right edge: if the item ends past the visible window, scroll right.
	 * Reserve a cell for the '>' indicator when content extends past it. */
	int right_margin = ((item_lx + item_w) < content_w) ? 1 : 0;
	int item_right = item_lx + item_w;
	if (item_right - lvl->hscroll > strip_w - right_margin) {
		lvl->hscroll = item_right - (strip_w - right_margin);
	}

	/* Reconcile when the item is wider than the visible window: the
	 * right-edge branch above may have scrolled far enough to reveal the
	 * item's tail while pushing its head (and hotkey letter) off-screen.
	 * An over-wide item cannot fit either way, so prefer showing its head
	 * -- anchor the left edge -- rather than a headless middle slice. */
	if (item_lx - lvl->hscroll < left_margin) {
		lvl->hscroll = item_lx - left_margin;
	}

	/* Clamp: never scroll past the content's end, never below zero. */
	int max_scroll = content_w - strip_w;
	if (max_scroll < 0) max_scroll = 0;
	if (lvl->hscroll > max_scroll) lvl->hscroll = max_scroll;
	if (lvl->hscroll < 0) lvl->hscroll = 0;
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
 * reservation so neighbour cells stay put as selection moves.  The whole
 * strip is shifted left by lvl->hscroll cells so the cursor item stays
 * visible when the content overflows the terminal width; '<' / '>' edge
 * indicators are drawn when items remain off-screen in either direction. */

/* Place a glyph at content-x `cx` shifted by hscroll, clipped to the pane.
 * Cells that scroll off either edge are simply dropped. */
static void strip_putc(pane_t *p, int cx, int hscroll, uint32_t ch,
                       uint8_t fg, uint8_t bg, uint8_t attr) {
	int sx = cx - hscroll;
	if (sx < 0 || sx >= p->w) return;
	pane_putc_color(p, sx, 0, ch, fg, bg, attr);
}

static void render_level(palette_state_t *st, int depth) {
	palette_level_t *lvl = &st->levels[depth];
	pane_t *p = &lvl->pane;
	pane_clear(p);

	/* Keep hscroll in sync with the current cursor/pane width even when this
	 * render was not triggered by a cursor_step (e.g. resize, first open). */
	ensure_cursor_visible(lvl, p->w);
	int hs = lvl->hscroll;

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
		int total_w = item_cell_width(it);
		/* Skip items entirely off either edge of the visible window; partial
		 * items are clipped per-cell by strip_putc. */
		if (lx + total_w - hs <= 0) continue;
		if (lx - hs >= p->w) break;

		/* Separator: a single dim divider glyph, never selected, never the
		 * cursor (cursor_step skips it). It is always disabled, so it draws
		 * dim regardless of the `selected` math above. */
		if (it->is_separator) {
			strip_putc(p, lx, hs, PALETTE_SEP_GLYPH,
			           PT_FG_DISABLED, PT_BG_MENUBAR, PALETTE_ATTR_DIM);
			continue;
		}

		int label_len = (int)strlen(it->label);
		/* Inner offset where the label starts: past the opening bracket and
		 * leading space, plus the checkmark cells when the item is checked. */
		int label_x = lx + 2 + (it->checked ? PALETTE_CHECK_CELLS : 0);

		/* Cell 0: opening bracket (space when not selected). */
		strip_putc(p, lx, hs, selected ? '[' : ' ', fg, bg, base);
		/* Cell 1: leading space. */
		strip_putc(p, lx + 1, hs, ' ', fg, bg, base);
		/* Checkmark glyph + trailing space (only when checked). */
		if (it->checked) {
			strip_putc(p, lx + 2, hs, PALETTE_CHECK_GLYPH, fg, bg, base);
			strip_putc(p, lx + 3, hs, ' ', fg, bg, base);
		}
		/* Label, with hotkey letter styling. */
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
			strip_putc(p, label_x + k, hs,
			           (uint32_t)(unsigned char)ch, cell_fg, bg, a);
		}
		/* Trailing space. */
		strip_putc(p, label_x + label_len, hs, ' ', fg, bg, base);
		/* Closing bracket (space when not selected). */
		strip_putc(p, label_x + label_len + 1, hs,
		           selected ? ']' : ' ', fg, bg, base);
	}

	/* Edge indicators: '<' when content is scrolled off the left, '>' when
	 * content extends past the right edge.  Drawn last so they sit on top of
	 * any partially clipped item cell. */
	int content_w = level_content_width(lvl);
	if (hs > 0) {
		pane_putc_color(p, 0, 0, '<', PT_FG_HOTKEY, PT_BG_MENUBAR,
		                PALETTE_ATTR_BOLD);
	}
	if (content_w - hs > p->w) {
		pane_putc_color(p, p->w - 1, 0, '>', PT_FG_HOTKEY, PT_BG_MENUBAR,
		                PALETTE_ATTR_BOLD);
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
/* Cursor-skipping helpers (defined below cursor_step). Forward-declared so
 * open_level can place the initial cursor on the first selectable item. */
static int find_selectable(const palette_level_t *lvl, int start, int step);

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
	/* Per-layer hotkey auto-derivation: walk siblings in declared order,
	 * claim explicit shortcuts first, then fill in unclaimed letters from
	 * each item's label. See derive_hotkey() for the algorithm. Without
	 * this, the ODB source returns shortcut='\0' for any item without an
	 * explicit cmdkey field and type-to-activate breaks at runtime even
	 * though the unit tests (which set shortcuts directly in fixtures)
	 * pass. */
	{
		bool claimed[256];
		memset(claimed, 0, sizeof(claimed));
		for (int i = 0; i < kept; ++i) {
			if (lvl->items[i].shortcut) {
				unsigned char up = (unsigned char)toupper(
					(unsigned char)lvl->items[i].shortcut);
				claimed[up] = true;
			}
		}
		for (int i = 0; i < kept; ++i) {
			lvl->items[i].shortcut = derive_hotkey(lvl->items[i].label,
			                                       lvl->items[i].shortcut,
			                                       claimed);
		}
	}
	/* Land the initial cursor on the first selectable (non-separator) item.
	 * If the level somehow contains only separators, fall back to 0. */
	{
		int first = find_selectable(lvl, 0, 1);
		lvl->cursor = first >= 0 ? first : 0;
	}
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

/* ---------- Pane render backend (default) ----------
 *
 * The five static functions below are TEXT-MOVE extractions of the
 * rendering logic that previously lived inline in palette_open,
 * palette_paint_teardown, palette_close, palette_render_state, and
 * palette_on_resize. No logic has been changed; the functions are
 * only moved here so they can be named in the s_pane_backend vtable.
 *
 * 2026-06-10 JES #691 Phase C.0.3a: render backend abstraction.
 */

static bool pane_backend_open(palette_state_t *st, void *ctx) {
	(void)ctx;
	/* Menubar strip: full-width, single row, one row below the prompt. */
	pane_init(&st->menubar, 0, st->prompt_row + 1, st->term_cols, 1);
	compositor_register(&st->menubar);
	return true;
}

static void pane_backend_paint(palette_state_t *st, void *ctx) {
	(void)ctx;
	render_menubar(st);
	for (int d = 0; d < st->open_depth; ++d) {
		render_level(st, d);
	}
}

static void pane_backend_paint_teardown(palette_state_t *st, void *ctx) {
	(void)ctx;
	/* pane_clear zeroes every cell (ch=0, fg=0, bg=0, attr=0). The
	 * compositor renders ch=0 as ' ' and emits no SGR for fg/bg/attr
	 * == 0 (default), so the next compositor_render() emits a "go back
	 * to terminal default" frame for every cell these panes cover. */
	for (int d = 0; d < st->open_depth && d < PALETTE_MAX_DEPTH; ++d) {
		pane_clear(&st->levels[d].pane);
	}
	pane_clear(&st->menubar);
}

static void pane_backend_on_resize(palette_state_t *st, int term_rows,
                                   int term_cols, void *ctx) {
	(void)ctx;
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

static void pane_backend_close(palette_state_t *st, void *ctx) {
	(void)ctx;
	compositor_unregister(&st->menubar);
	pane_destroy(&st->menubar);
}

static const palette_render_backend_t s_pane_backend = {
	NULL,                      /* ctx */
	pane_backend_open,
	pane_backend_paint,
	pane_backend_paint_teardown,
	pane_backend_on_resize,
	pane_backend_close,
};

const palette_render_backend_t *palette_render_pane_backend(void) {
	return &s_pane_backend;
}

/* ---------- Public API ---------- */

bool palette_open_ex(palette_state_t *st, int term_rows, int term_cols,
                     int prompt_row,
                     const palette_menu_source_t *source,
                     const palette_render_backend_t *backend) {
	if (!st || !source) return false;
	if (term_cols < 4 || term_rows < 2) return false;

	memset(st, 0, sizeof(*st));
	st->source = source;
	st->term_rows = term_rows;
	st->term_cols = term_cols;

	/* Clamp prompt_row into the visible range first. */
	if (prompt_row < 0) prompt_row = 0;
	if (prompt_row >= term_rows) prompt_row = term_rows - 1;

	/* The caller is responsible for ensuring there is a free row below
	 * the prompt before we open: run_palette_modal in repl.c calls
	 * linenoiseEditStop() first, which emits '\n' and (when the prompt
	 * is on the bottom row) scrolls the terminal up by one. The cursor
	 * post-EditStop is always on the row immediately below the prompt,
	 * which is exactly where the menubar will anchor (prompt_row + 1).
	 *
	 * Defensive fallback: if prompt_row + 1 would still land outside the
	 * visible area (impossible after EditStop's '\n' scroll), pull
	 * prompt_row back into range so menubar.y = prompt_row + 1 is at
	 * most term_rows - 1. This branch is unreachable in practice but
	 * keeps the pane_init below from constructing an off-screen menubar
	 * if a future caller skips the EditStop pre-step. */
	if (prompt_row + 1 >= term_rows) {
		prompt_row = term_rows - 2;
		if (prompt_row < 0) prompt_row = 0;
	}
	st->prompt_row = prompt_row;

	int n = source->count_menus(source->ctx);
	if (n <= 0) return false;

	layout_menubar(st);
	if (st->menu_count <= 0) return false;

	st->backend = backend ? backend : &s_pane_backend;
	st->active = true;
	st->menubar_cursor = 0;
	st->open_depth = 0;
	st->esc_pending = false;
	st->csi_len = 0;
	st->exec_script = NULL;
	st->exec_arg[0] = '\0';

	if (!st->backend->open(st, st->backend->ctx)) {
		st->active = false;
		return false;
	}
	return true;
}

bool palette_open(palette_state_t *st, int term_rows, int term_cols,
                  int prompt_row,
                  const palette_menu_source_t *source) {
	return palette_open_ex(st, term_rows, term_cols, prompt_row, source, NULL);
}

void palette_paint_teardown(palette_state_t *st) {
	if (!st || !st->active) return;
	st->backend->paint_teardown(st, st->backend->ctx);
}

void palette_close(palette_state_t *st) {
	if (!st) return;
	if (!st->active) return;
	close_all_levels(st);
	st->backend->close(st, st->backend->ctx);
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

/* Return true if item index `i` in `lvl` is a landable cursor target, i.e.
 * in range and not a separator. Separators are visual dividers only; the
 * cursor never rests on them. */
static bool is_selectable(const palette_level_t *lvl, int i) {
	if (i < 0 || i >= lvl->item_count) return false;
	return !lvl->items[i].is_separator;
}

/* Find the first selectable index at or after `start` (when step>0) or at or
 * before `start` (when step<0). Returns -1 if none exists in that direction. */
static int find_selectable(const palette_level_t *lvl, int start, int step) {
	for (int i = start; i >= 0 && i < lvl->item_count; i += step) {
		if (is_selectable(lvl, i)) return i;
	}
	return -1;
}

/* Move the deepest level's cursor by `delta`, skipping separators and
 * clamping. A separator is never a valid resting place, so we keep walking
 * in the direction of travel until we hit a selectable item; if there is
 * none past the current position, the cursor stays put. */
static void cursor_step(palette_state_t *st, int delta) {
	if (st->open_depth <= 0) return;
	palette_level_t *lvl = &st->levels[st->open_depth - 1];
	if (lvl->item_count <= 0) return;
	if (delta == 0) return;

	int step = delta > 0 ? 1 : -1;
	int c = lvl->cursor;
	for (int n = (delta > 0 ? delta : -delta); n > 0; --n) {
		int next = find_selectable(lvl, c + step, step);
		if (next < 0) break; /* no selectable item further in this direction */
		c = next;
	}
	lvl->cursor = c;
	ensure_cursor_visible(lvl, lvl->pane.w);
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

int slash_menu_trigger_delay_ms(void) {
	const char *v = getenv("FRONTIER_PALETTE_FAST_TIMERS");
	if (v && v[0] != '\0') return SLASH_MENU_TRIGGER_DELAY_FAST_MS;
	return SLASH_MENU_TRIGGER_DELAY_DEFAULT_MS;
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
		/* Edge indicators are scroll affordances, not items.  render_level
		 * draws '<' at col 0 when scrolled off the left and '>' at the last
		 * col when content extends past the right.  A click there must NOT
		 * fall through to the item hit-test below: the cell under the
		 * indicator maps (via content-x = screen-x + hscroll) to a real but
		 * visually-occluded item, so without this guard clicking '<' would
		 * silently fire a hidden command.  Instead, step the cursor toward
		 * the off-screen side, which scrolls the strip in that direction. */
		int content_w = level_content_width(lvl);
		if (mx == 0 && lvl->hscroll > 0) {
			cursor_step(st, -1);
			return PALETTE_DONE_NONE;
		}
		if (mx == p->w - 1 && content_w - lvl->hscroll > p->w) {
			cursor_step(st, +1);
			return PALETTE_DONE_NONE;
		}
		/* Convert the screen click to content space: the strip is shifted
		 * left by hscroll, so content-x = screen-x + hscroll. */
		int cx = mx + lvl->hscroll;
		/* Find which item the click landed on. */
		for (int i = 0; i < lvl->item_count; ++i) {
			int lx = item_screen_x(lvl, i);
			int total_w = item_cell_width(&lvl->items[i]);
			/* Separators are non-interactive: a click on the divider does
			 * nothing rather than selecting a phantom item. */
			if (lvl->items[i].is_separator)
				continue;
			if (cx >= lx && cx < lx + total_w) {
				/* Collapse any deeper levels first. */
				while (st->open_depth - 1 > d) close_deepest_level(st);
				lvl->cursor = i;
				ensure_cursor_visible(lvl, p->w);
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
	st->backend->paint(st, st->backend->ctx);
}

void palette_on_resize(palette_state_t *st, int term_rows, int term_cols) {
	if (!st || !st->active) return;
	if (term_cols < 4 || term_rows < 2) return;
	st->term_rows = term_rows;
	st->term_cols = term_cols;
	st->backend->on_resize(st, term_rows, term_cols, st->backend->ctx);
}
