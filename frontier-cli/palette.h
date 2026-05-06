/*
 * palette.h - REPL slash-menu palette state machine on top of pane compositor.
 *
 * VisiCalc-style modal menu palette: top-level menubar (row 0), cascading
 * submenus opening to the right of their parent at the selected row,
 * keyboard + mouse navigation, hotkey acceleration. Implements plan
 * §1.3b "palette.c on top of pane compositor"; see
 * planning/architectural_decision_records/ADR-016 for the data model and
 * /Users/jake/.claude/plans/humming-painting-hejlsberg.md for the scope
 * decomposition (PR 5 = module-only, no REPL integration yet).
 *
 * Layering
 * --------
 *   pane.h            - generic pane compositor (PR 4)
 *   palette.h (this)  - menu state machine + cascade renderer
 *   repl.c (PR 6)     - event loop wiring; install ODB-backed menu source
 *
 * Data source abstraction
 * -----------------------
 * Palette reads its tree through palette_menu_source_t — a small
 * function-pointer vtable that lets palette stay free of any ODB or
 * Frontier-runtime dependency. PR 6 will provide an adapter that wraps
 * menudata_list_leaves() / menudata_describe_leaf() (PR 2) to populate
 * this interface from system.menus.data.<app>.<menu>.<item>. Unit tests
 * in PR 5 use a static in-memory implementation.
 *
 * The data source is consulted ONCE on palette_open() — palette caches
 * the tree internally and does NOT re-query during navigation. This is a
 * deliberate decoupling: keystroke-time ODB walks would leak GIL latency
 * into the input loop. A future palette_refresh() (PR 6) will rebind the
 * cache when the menubar changes.
 *
 * State stack
 * -----------
 * Maximum cascade depth is 8 levels. Hard cap, no recursion. The
 * menubar lives at depth 0; opening a top-level menu makes depth=1; each
 * submenu adds one. Trying to open a 9th level is silently ignored.
 *
 * Threading
 * ---------
 * Like pane.c, all palette_* calls must be made on the REPL main thread.
 * Background producers route through the same main-thread queue used for
 * pane writes (PR 6).
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors. See pane.h for the full
 * license text.
 */

#ifndef FRONTIER_PALETTE_H
#define FRONTIER_PALETTE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pane.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hard cap on cascade depth (menubar = 0, top menu = 1, submenu = 2, ...). */
#define PALETTE_MAX_DEPTH 8

/* Generous size limits — items wider than this are truncated visually
 * but still selectable. Stored in fixed-size buffers so the palette
 * footprint is predictable. */
#define PALETTE_LABEL_MAX 64
#define PALETTE_DESC_MAX  128

/* Attribute bits used for rendering. Mirrors pane.c's emit_attr() bit
 * assignments so the values composite directly into cell.attr. */
#define PALETTE_ATTR_BOLD     0x01u
#define PALETTE_ATTR_DIM      0x02u
#define PALETTE_ATTR_UNDERLINE 0x04u
#define PALETTE_ATTR_INVERSE  0x08u

/* Result of palette_feed_byte / palette_feed_mouse. */
typedef enum {
	PALETTE_DONE_NONE = 0,    /* palette still active, keep feeding */
	PALETTE_DONE_CANCEL,      /* user cancelled (ESC at top, click outside) */
	PALETTE_DONE_EXECUTE      /* user picked a leaf — see palette_state.exec_script */
} palette_done_t;

/* One leaf described by the data source. Mirrors the
 * menudata_describe_leaf() record shape (ADR-016) but flat, copy-by-value,
 * and free of any UserTalk type dependencies. The data source fills these
 * in from the underlying ODB rows; palette never mutates them. */
typedef struct palette_item {
	char label[PALETTE_LABEL_MAX];
	char description[PALETTE_DESC_MAX];
	char shortcut;        /* uppercase hotkey letter, '\0' if none */
	bool enabled;
	bool hidden;
	bool is_submenu;      /* true → has children, false → leaf with script */
	/* Opaque per-item handle that the source uses to identify this item
	 * for subsequent open_submenu / describe_leaf calls. The palette
	 * treats this as an uninterpreted token. For the ODB adapter this
	 * will be an hdlhashtable; for tests it's a simple integer cast. */
	void *opaque;
	/* For leaves only: the script body to dispatch on EXECUTE. The
	 * palette copies this pointer into palette_state.exec_script and
	 * the caller (PR 6) hands it to meuserselected_headless. NULL for
	 * submenu items. The palette does NOT own this pointer; it must
	 * remain valid for the lifetime of the open palette. */
	void *script_handle;
} palette_item_t;

/* Source vtable — describes a menubar tree. The `ctx` pointer is passed
 * back to every callback verbatim and is opaque to the palette.
 *
 * Lifetime: all returned strings (`label`, `description`) must remain
 * valid for the entire duration of the open palette. The data source is
 * queried once on palette_open() into an internal cache, so transient
 * buffers are OK as long as they're stable across that single walk.
 */
typedef struct palette_menu_source {
	void *ctx;

	/* Number of top-level menus on the menubar (e.g. REPL, File, Help). */
	int (*count_menus)(void *ctx);

	/* Fill `out_label` with the menu's label and `out_hotkey` with its
	 * single-letter hotkey ('\0' if none). Returns false if `index` is
	 * out of range. `label_cap` is the buffer capacity. */
	bool (*menu_describe)(void *ctx, int menu_index,
	                      char *out_label, size_t label_cap,
	                      char *out_hotkey);

	/* Open a menu (or submenu) for enumeration. `parent_opaque` is the
	 * opaque token from the parent item, or NULL when querying a
	 * top-level menu (in which case `menu_index` selects which one).
	 * Returns the item count. The same handle stays open until
	 * the palette closes — sources may cache state keyed off
	 * (parent_opaque, menu_index). */
	int (*item_count)(void *ctx, int menu_index, void *parent_opaque);

	/* Fill `out` with item `item_index` under (menu_index, parent_opaque). */
	bool (*item_describe)(void *ctx, int menu_index, void *parent_opaque,
	                      int item_index, palette_item_t *out);
} palette_menu_source_t;

/* Per-level cascade frame. Externally visible so tests can inspect, but
 * fields are read-only from the caller's perspective. */
typedef struct palette_level {
	pane_t pane;            /* the rendered pane for this level */
	int menu_index;         /* which top-level menu this branch belongs to */
	void *parent_opaque;    /* opaque token of the item that opened this level
	                         * (NULL for the top-level menu itself) */
	int item_count;
	int cursor;             /* selected item index */
	/* Cached items for this level — populated on level open from the
	 * data source. Capped at PALETTE_LEVEL_MAX_ITEMS; sources reporting
	 * more items get the rest hidden (PR 8 will scroll). */
	palette_item_t items[64];
} palette_level_t;

#define PALETTE_LEVEL_MAX_ITEMS 64

/* Palette state. The whole struct is value-typed (no internal mallocs
 * outside the pane buffers, which pane.c manages). */
typedef struct palette_state {
	bool active;
	int term_rows;
	int term_cols;

	pane_t menubar;             /* row 0, full-width strip */
	int menubar_cursor;         /* index of focused top-level menu */
	int menu_count;             /* cached from source.count_menus() */
	/* Menubar entries — labels + screen x positions, computed at open. */
	char menu_labels[16][PALETTE_LABEL_MAX];
	char menu_hotkeys[16];
	int menu_x[16];             /* screen-x of each menubar entry */
	int menu_w[16];             /* width of each menubar entry */

	palette_level_t levels[PALETTE_MAX_DEPTH];
	int open_depth;             /* 0 = no menu open, 1 = top, 2 = submenu, ... */

	/* Set on PALETTE_DONE_EXECUTE — caller reads this to find the script
	 * to dispatch. Aliased into the data source's storage; palette does
	 * not own it. */
	void *exec_script;

	/* ESC disambiguation: when palette_feed_byte sees a bare 0x1b, it
	 * sets esc_pending=true and returns DONE_NONE. The next byte either
	 * forms a CSI sequence (and clears the flag) or — if the caller's
	 * timeout fires first via palette_feed_esc_timeout() — is treated
	 * as a bare ESC. */
	bool esc_pending;
	/* CSI accumulator. Holds bytes after the ESC '[' prefix until a
	 * terminating letter arrives. */
	char csi_buf[16];
	int csi_len;

	const palette_menu_source_t *source;
} palette_state_t;

/* ---------- Public API ---------- */

/* Open the palette. Computes menubar layout from the source, registers
 * the menubar pane with the compositor, and leaves the palette in
 * "menubar focus, no menu open" state.
 *
 * `term_rows`/`term_cols` come from the caller's terminal_get_size()
 * (PR 3). `source` must remain valid until palette_close().
 *
 * On entry the compositor must have been initialised with
 * compositor_on_resize(term_rows, term_cols) — palette does not call it.
 *
 * Returns true on success, false if the menubar source has zero menus or
 * the terminal is too small to render the menubar at all (cols < 4). */
bool palette_open(palette_state_t *st, int term_rows, int term_cols,
                  const palette_menu_source_t *source);

/* Close the palette. Unregisters all panes from the compositor and
 * destroys their cell buffers. Idempotent. */
void palette_close(palette_state_t *st);

/* Feed one input byte. Drives both the ESC disambiguation buffer and the
 * CSI parser. Returns DONE_EXECUTE if the byte triggered a leaf dispatch
 * (with `st->exec_script` populated), DONE_CANCEL if the palette should
 * tear down, DONE_NONE otherwise. */
palette_done_t palette_feed_byte(palette_state_t *st, unsigned char b);

/* Called by the caller's event loop when the ESC disambiguation timer
 * (POLL_TIMEOUT_MS, ~10ms in the REPL — see plan §1.3b) fires without a
 * follow-up byte. If esc_pending is set, treats it as a bare ESC and
 * processes accordingly (collapse one level / DONE_CANCEL at top). */
palette_done_t palette_feed_esc_timeout(palette_state_t *st);

/* Feed a parsed mouse event. Coordinates are 1-based (matches mouse_parse
 * output and ANSI CUP convention). The palette converts to 0-based and
 * routes via compositor_pane_at(). */
palette_done_t palette_feed_mouse(palette_state_t *st, const mouse_event_t *ev);

/* Re-render every registered palette pane. Called after any state
 * change. Does NOT call compositor_render() — the caller composites
 * along with the rest of its UI (scrollback, prompt) for one
 * frame-coherent flush per tick. */
void palette_render_state(palette_state_t *st);

/* React to a SIGWINCH-style resize. Re-fetches geometry, repositions
 * cascades, and clamps any cursors out of range. */
void palette_on_resize(palette_state_t *st, int term_rows, int term_cols);

#ifdef __cplusplus
}
#endif

#endif /* FRONTIER_PALETTE_H */
