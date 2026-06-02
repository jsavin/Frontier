/*
 * palette.h - REPL slash-menu palette state machine on top of pane compositor.
 *
 * Host-anchored horizontal cascade: a single-row menubar strip painted
 * immediately below the REPL prompt (rather than at row 0), with each
 * open submenu rendering as another full-width single-row strip stacked
 * directly below.  Keyboard navigation, hotkey type-to-activate, mouse
 * click + wheel.  Implements plan §1.3b "palette.c on top of pane
 * compositor"; see
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

/* Maximum length of the filter / type-ahead buffer, including NUL. */
#define PALETTE_FILTER_MAX 64

/* Maximum length of the accepts_args input row, including NUL. */
#define PALETTE_ARG_MAX 256

/* Attribute bits used for rendering. Mirrors pane.c's emit_attr() bit
 * assignments so the values composite directly into cell.attr. */
#define PALETTE_ATTR_BOLD     0x01u
#define PALETTE_ATTR_DIM      0x02u
#define PALETTE_ATTR_UNDERLINE 0x04u
#define PALETTE_ATTR_INVERSE  0x08u

/* ANSI 16-color palette indices used in cell.fg / cell.bg.
 *
 * The compositor's emit_attr() encodes the low 3 bits as an SGR base color
 * and the 4th bit as the "bright" modifier (SGR 9x for fg, 10x for bg).
 * Color value 0 means "default" — the compositor emits no SGR color code,
 * letting the terminal's own defaults take effect.
 *
 * Encoding:
 *   0          = default (no SGR color)
 *   1..8       = standard colors (SGR 30..37 / 40..47, offset by -1 because
 *                value 0 is reserved for "default")
 *   9..16      = bright variants (SGR 90..97 / 100..107)
 *
 * Values >16 are reserved for 256-color extension. The palette uses only
 * the 16-color subset so it works on classic terminals without 256-color
 * support. */
#define PALETTE_COLOR_DEFAULT       0u
#define PALETTE_COLOR_BLACK         1u
#define PALETTE_COLOR_RED           2u
#define PALETTE_COLOR_GREEN         3u
#define PALETTE_COLOR_YELLOW        4u
#define PALETTE_COLOR_BLUE          5u
#define PALETTE_COLOR_MAGENTA       6u
#define PALETTE_COLOR_CYAN          7u
#define PALETTE_COLOR_WHITE         8u
#define PALETTE_COLOR_BRIGHT_BLACK  9u
#define PALETTE_COLOR_BRIGHT_RED    10u
#define PALETTE_COLOR_BRIGHT_GREEN  11u
#define PALETTE_COLOR_BRIGHT_YELLOW 12u
#define PALETTE_COLOR_BRIGHT_BLUE   13u
#define PALETTE_COLOR_BRIGHT_MAGENTA 14u
#define PALETTE_COLOR_BRIGHT_CYAN   15u
#define PALETTE_COLOR_BRIGHT_WHITE  16u

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
	/* `label` and `description` are interpreted as 7-bit ASCII. The palette
	 * renderer byte-counts these strings for width calculations and column
	 * alignment, so UTF-8 multi-byte sequences, wide characters (CJK), and
	 * combining marks will misrender (column drift, wrap mispositioning).
	 * Right-to-left scripts are not supported. Stick to printable ASCII. */
	char label[PALETTE_LABEL_MAX];
	char description[PALETTE_DESC_MAX];
	char shortcut;        /* uppercase hotkey letter, '\0' if none */
	bool enabled;
	bool hidden;
	bool is_submenu;      /* true → has children, false → leaf with script */
	bool accepts_args;    /* true → ENTER passes input-row text as argument */
	bool checked;         /* true: draw a checkmark glyph before the label
	                       *        (legacy '!' menu-text prefix). */
	bool is_separator;    /* true: render a divider, not a label; the item is
	                       *        non-selectable (cursor skips it) and is
	                       *        disabled (legacy '-' menu-text line). */
	/* Opaque per-item handle that the source uses to identify this item
	 * for subsequent open_submenu / describe_leaf calls. The palette
	 * treats this as an uninterpreted token. For the ODB adapter this
	 * will be an hdlhashtable; for tests it's a simple integer cast. */
	void *opaque;
	/* For leaves only: the script body to dispatch on EXECUTE. The
	 * palette copies this pointer into palette_state.exec_script and
	 * the caller (PR 6) hands it to meuserselected_headless. NULL for
	 * submenu items.
	 *
	 * Ownership and lifetime — STRICT contract:
	 *   - The palette does NOT own `script_handle`.
	 *   - It MUST remain valid across GIL yields for the entire lifetime
	 *     of the open palette AND survive palette_close so the caller can
	 *     dispatch on PALETTE_DONE_EXECUTE without a re-fetch.
	 *   - Because the palette is open across many keystrokes (and thus
	 *     across multiple GIL yield points — see
	 *     Common/headers/menudata_headless.h), the source MUST guarantee
	 *     stability by either:
	 *       (a) pre-copying the handle into source-private storage when
	 *           populating its internal cache (e.g. copyhandle /
	 *           copyvaluerecord), OR
	 *       (b) advertising the handle as already-immutable (e.g. fields
	 *           of a deep-copied record returned by
	 *           menudata_describe_leaf, each independently allocated).
	 *   - The palette will NOT call back into the source at dispatch
	 *     time; it aliases this pointer into palette_state.exec_script
	 *     and the caller is responsible for dispatching BEFORE tearing
	 *     down the source's backing storage. palette_close does NOT
	 *     invalidate exec_script. */
	void *script_handle;
} palette_item_t;

/* Source vtable — describes a menubar tree. The `ctx` pointer is passed
 * back to every callback verbatim and is opaque to the palette.
 *
 * String lifetime: returned strings (`label`, `description`) and any
 * `script_handle` / `opaque` pointers stored in palette_item_t MUST
 * remain valid for the entire lifetime of the open palette AND across
 * GIL yields. The data source is queried once on palette_open() into
 * an internal cache; transient buffers are OK during the open walk only
 * if the palette's per-item `label`/`description` byte copies (made via
 * snprintf into fixed-size buffers below) are sufficient for rendering.
 * For pointer-typed fields (`opaque`, `script_handle`), see palette_item_t
 * — the source bears the entire stability burden because the palette
 * never re-queries.
 *
 * Strings written by `menu_describe` and `item_describe` are NUL-
 * terminated by the source. The palette additionally force-NUL-
 * terminates the trailing byte after every callback as defense in
 * depth, so subsequent strlen()/loops are always safe.
 *
 * `menu_index` semantics in callbacks:
 *   - For top-level menus (parent_opaque == NULL): canonical menu
 *     index, in [0, count_menus()).
 *   - For submenu levels (parent_opaque != NULL): set to the ROOT
 *     menu's index for caller convenience, but sources MUST resolve
 *     items via `parent_opaque`. Sources must not key state off
 *     `menu_index` when `parent_opaque != NULL`.
 */
typedef struct palette_menu_source {
	void *ctx;

	/* Number of top-level menus on the menubar (e.g. REPL, File, Help).
	 *
	 * GIL: caller must hold the GIL when this is invoked from
	 * palette_open / palette_feed_byte. Source implementations may
	 * touch ODB. */
	int (*count_menus)(void *ctx);

	/* Fill `out_label` with the menu's label and `out_hotkey` with its
	 * single-letter hotkey ('\0' if none). Returns false if `index` is
	 * out of range. `label_cap` is the buffer capacity. The source must
	 * NUL-terminate `out_label`; the palette additionally enforces this
	 * locally.
	 *
	 * GIL: caller must hold the GIL. */
	bool (*menu_describe)(void *ctx, int menu_index,
	                      char *out_label, size_t label_cap,
	                      char *out_hotkey);

	/* Open a menu (or submenu) for enumeration. `parent_opaque` is the
	 * opaque token from the parent item, or NULL when querying a
	 * top-level menu (in which case `menu_index` selects which one).
	 * Returns the item count. The same handle stays open until
	 * the palette closes — sources may cache state keyed off
	 * (parent_opaque, menu_index).
	 *
	 * GIL: caller must hold the GIL. */
	int (*item_count)(void *ctx, int menu_index, void *parent_opaque);

	/* Fill `out` with item `item_index` under (menu_index, parent_opaque).
	 * The source must NUL-terminate `out->label` and `out->description`;
	 * the palette additionally enforces this locally. The source is
	 * responsible for stability of `out->opaque` and `out->script_handle`
	 * across GIL yields — see palette_item_t for the full contract.
	 *
	 * GIL: caller must hold the GIL. */
	bool (*item_describe)(void *ctx, int menu_index, void *parent_opaque,
	                      int item_index, palette_item_t *out);
} palette_menu_source_t;

/* Maximum items captured per cascade level. Defined ahead of the
 * struct so the items[] / visible[] array dimensions can use the
 * constant directly rather than a duplicated literal — keeps the
 * cap a single source of truth (cf. open_level's `imin(total,
 * PALETTE_LEVEL_MAX_ITEMS)`). */
#define PALETTE_LEVEL_MAX_ITEMS 64

/* Per-level cascade frame. Externally visible so tests can inspect, but
 * fields are read-only from the caller's perspective. */
typedef struct palette_level {
	pane_t pane;            /* the rendered pane for this level */
	int menu_index;         /* which top-level menu this branch belongs to */
	void *parent_opaque;    /* opaque token of the item that opened this level
	                         * (NULL for the top-level menu itself) */
	int item_count;
	int cursor;             /* selected item index, indexed into items[] */
	/* Cached items for this level — populated on level open from the
	 * data source. Capped at PALETTE_LEVEL_MAX_ITEMS. */
	palette_item_t items[PALETTE_LEVEL_MAX_ITEMS];
	/* Filtered visibility map: visible[0..visible_count) holds the
	 * indices into items[] that match the current filter. When the
	 * filter is empty, visible_count == item_count and visible[i] == i.
	 * `cursor` always indexes items[]; render walks visible[] to map
	 * row positions back to source items. The display cursor row
	 * (i.e. which row inside the pane is highlighted) is the i where
	 * visible[i] == cursor. */
	int visible[PALETTE_LEVEL_MAX_ITEMS];
	int visible_count;
	/* Scroll offset into visible[]: the first row of the pane shows
	 * visible[scroll_top]. Adjusted automatically when cursor moves
	 * outside the visible window or by explicit page/wheel scrolls. */
	int scroll_top;
} palette_level_t;

/* Palette state. The whole struct is value-typed (no internal mallocs
 * outside the pane buffers, which pane.c manages). */
typedef struct palette_state {
	bool active;
	int term_rows;
	int term_cols;
	/* The terminal row of the REPL prompt at palette_open() time.
	 * The menubar pane lives at prompt_row + 1; each open cascade
	 * level lives at prompt_row + 1 + depth. The caller is expected
	 * to have stopped linenoise (which emits '\n' and, when the
	 * prompt is on the bottom row, scrolls the terminal up by one)
	 * BEFORE calling palette_open, so prompt_row + 1 is always a
	 * free row inside the visible viewport. */
	int prompt_row;

	pane_t menubar;             /* full-width strip at prompt_row + 1 */
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
	 * to dispatch.
	 *
	 *   - Set by palette_feed_* when returning PALETTE_DONE_EXECUTE.
	 * 	   - Aliases source-owned storage. Per the palette_item_t
	 *     `script_handle` contract, the source has guaranteed this
	 *     pointer is stable across GIL yields and survives palette_close.
	 *   - Survives palette_close — the typical caller pattern is:
	 *       1. palette_feed_byte(...) returns PALETTE_DONE_EXECUTE
	 *       2. capture st->exec_script
	 *       3. palette_close(&st)
	 *       4. dispatch the captured handle
	 *       5. (later) tear down the source's backing storage
	 *     The palette does NOT own this pointer and palette_close does
	 *     NOT invalidate it. */
	void *exec_script;

	/* Set on PALETTE_DONE_EXECUTE for items where accepts_args is true.
	 * NUL-terminated, may be empty. Caller copies before palette_close. */
	char exec_arg[PALETTE_ARG_MAX];

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

	/* Filter / type-ahead buffer.
	 *
	 * When a menu is open (open_depth > 0), printable letters and digits
	 * append here instead of triggering hotkey acceleration. The deepest
	 * level's visible[] is recomputed against this filter on every keystroke
	 * — a case-insensitive substring match against the item label.
	 *
	 * BACKSPACE (0x7f, 0x08) removes one character. ESC clears the buffer
	 * if non-empty (first ESC); a second ESC then closes the deepest level
	 * (existing semantic). When the deepest level closes, the buffer is
	 * cleared so the new deepest level starts unfiltered.
	 *
	 * On menubar (open_depth == 0): letters retain hotkey-accelerator
	 * behavior — they jump-and-open the matching top-level menu without
	 * touching this buffer. */
	char filter_buf[PALETTE_FILTER_MAX];
	int filter_len;

	/* Argument input row (for items where accepts_args == true).
	 *
	 * When the deepest level's cursor lands on an item with accepts_args,
	 * the cascade pane reserves an extra inner row above the bottom border
	 * for typed arguments. ENTER on such an item dispatches with this
	 * buffer as the argument string (copied into exec_arg).
	 *
	 * The filter buffer and the arg buffer are mutually exclusive — when
	 * the cursor is on an accepts_args item, typing goes to arg_buf and
	 * the filter buffer is left untouched. Moving the cursor off the
	 * accepts_args item clears arg_buf so the next visit starts fresh. */
	char arg_buf[PALETTE_ARG_MAX];
	int arg_len;

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
 * the terminal is too small to render the menubar at all (cols < 4).
 *
 * GIL: must be held by the caller. The source vtable callbacks
 * (count_menus, menu_describe) run inline and may touch the ODB. */
bool palette_open(palette_state_t *st, int term_rows, int term_cols,
                  int prompt_row,
                  const palette_menu_source_t *source);

/* Close the palette. Unregisters all panes from the compositor and
 * destroys their cell buffers. Idempotent.
 *
 * GIL: not required. Closes only locally-owned compositor registrations
 * and pane buffers; does not call back into the source. Note that
 * palette_state.exec_script (if set) is NOT invalidated by close —
 * see the field doc. */
void palette_close(palette_state_t *st);

/* Paint every registered palette pane (menubar + open cascade levels)
 * to blank cells with terminal-default fg / bg / attr. Idempotent and
 * a no-op when the palette is not active.
 *
 * Why this exists: the compositor diff-renders against its previous
 * framebuffer. When the palette modal exits, the panes' cells still
 * carry the menubar/selection attributes from the last frame. If we
 * simply unregister the panes and let subsequent writes (a leaf
 * script's output, the linenoise prompt redraw) paint over them, the
 * compositor sees no change to those cells (the new cell content has
 * the same printable char and the framebuffer still says "menubar
 * attrs") and the attributes leak through.
 *
 * The contract: call this BEFORE palette_close, then call
 * compositor_render() ONCE so the cleared cells reach the terminal
 * while the panes are still registered. Only AFTER that flush is it
 * safe to call palette_close / unregister the panes.
 *
 * GIL: not required. Touches only locally-owned pane buffers; does not
 * call into the source. */
void palette_paint_teardown(palette_state_t *st);

/* Feed one input byte. Drives both the ESC disambiguation buffer and the
 * CSI parser. Returns DONE_EXECUTE if the byte triggered a leaf dispatch
 * (with `st->exec_script` populated), DONE_CANCEL if the palette should
 * tear down, DONE_NONE otherwise.
 *
 * GIL: must be held when this can re-enter the source — drilling into a
 * submenu (ENTER, RIGHT, hotkey on a submenu item) calls
 * source->item_count and source->item_describe. The conservative rule is
 * "always hold the GIL when feeding bytes". */
palette_done_t palette_feed_byte(palette_state_t *st, unsigned char b);

/* Called by the caller's event loop when the ESC disambiguation timer
 * (POLL_TIMEOUT_MS, ~10ms in the REPL — see plan §1.3b) fires without a
 * follow-up byte. If esc_pending is set, treats it as a bare ESC and
 * processes accordingly (collapse one level / DONE_CANCEL at top).
 *
 * GIL: not required. Does not call into the source — only manipulates
 * local cascade state. */
palette_done_t palette_feed_esc_timeout(palette_state_t *st);

/* ESC disambiguation timeout in milliseconds. Returns 1 when the
 * FRONTIER_PALETTE_FAST_TIMERS environment variable is set to a non-
 * empty string, otherwise 10 (the production default). Callers should
 * use this value as the poll(2) timeout for the palette modal byte
 * loop so that L4 integration tests can compress the per-ESC wait
 * from ~10ms to ~1ms without recompilation.
 *
 * The env var is re-read on each call (one getenv(3) per palette
 * poll cycle, ~100Hz while a palette is open). This keeps the test
 * surface flexible — tests can flip the env var between calls — at
 * negligible cost.
 *
 * GIL: this function touches no GIL-protected state (only process env).
 * In practice it is only called from the modal byte loop, which already
 * holds the GIL. */
int palette_esc_timeout_ms(void);

/* Default ESC disambiguation timeout (milliseconds) used when
 * FRONTIER_PALETTE_FAST_TIMERS is unset or empty. Public so callers
 * and tests share a single source of truth. */
#define PALETTE_ESC_TIMEOUT_DEFAULT_MS 10

/* Fast ESC disambiguation timeout (milliseconds) used when
 * FRONTIER_PALETTE_FAST_TIMERS is set non-empty. */
#define PALETTE_ESC_TIMEOUT_FAST_MS 1

/* Slash-vs-menu disambiguation timeout in milliseconds. When the user
 * types '/' at column 1 of an empty REPL buffer, the REPL waits this
 * long for a follow-up byte before opening the menu. If a byte arrives
 * within the window, it is treated as the second character of a slash
 * command (e.g. "/help") and the menu does NOT open. A second '/' is
 * a power-user fast-path that opens the menu instantly.
 *
 * Returns SLASH_MENU_TRIGGER_DELAY_FAST_MS when
 * FRONTIER_PALETTE_FAST_TIMERS is set non-empty (shares the env var
 * with palette_esc_timeout_ms so L4 tests have a single knob),
 * otherwise SLASH_MENU_TRIGGER_DELAY_DEFAULT_MS.
 *
 * GIL: same contract as palette_esc_timeout_ms — only touches getenv. */
int slash_menu_trigger_delay_ms(void);

/* Default slash-menu disambiguation timeout (milliseconds). 350ms is
 * the upper bound on inter-keystroke delay for a deliberate slash
 * command — short enough that a user who types '/' alone perceives
 * the menu as "instant", long enough that any second key in a typed
 * command beats the timer. The number is tuned for US QWERTY where
 * '/' is right-pinky and the second character is often a different
 * finger; the right-pinky reach + finger transition pushes typical
 * inter-key intervals into the 200-300ms range, so 250ms was too
 * tight in practice (caught the menu when typing /keycodes etc.). */
#define SLASH_MENU_TRIGGER_DELAY_DEFAULT_MS 350

/* Fast slash-menu disambiguation timeout (milliseconds) used when
 * FRONTIER_PALETTE_FAST_TIMERS is set non-empty. */
#define SLASH_MENU_TRIGGER_DELAY_FAST_MS 5

/* Feed a parsed mouse event. Coordinates are 1-based (matches mouse_parse
 * output and ANSI CUP convention). The palette converts to 0-based and
 * routes via compositor_pane_at().
 *
 * GIL: must be held when the click can drill into a submenu (clicking a
 * submenu item calls source->item_count / item_describe). The
 * conservative rule is "always hold the GIL when feeding mouse". */
palette_done_t palette_feed_mouse(palette_state_t *st, const mouse_event_t *ev);

/* Re-render every registered palette pane. Called after any state
 * change. Does NOT call compositor_render() — the caller composites
 * along with the rest of its UI (scrollback, prompt) for one
 * frame-coherent flush per tick.
 *
 * GIL: not required. Reads only local cached state. */
void palette_render_state(palette_state_t *st);

/* React to a SIGWINCH-style resize. Re-fetches geometry, repositions
 * cascades, and clamps any cursors out of range. If shrinkage causes a
 * cascade level to no longer fit on screen (width or height below the
 * minimum useful size), levels at and below that depth are closed.
 *
 * GIL: not required. Does not call back into the source. */
void palette_on_resize(palette_state_t *st, int term_rows, int term_cols);

#ifdef __cplusplus
}
#endif

#endif /* FRONTIER_PALETTE_H */
