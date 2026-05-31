/*
 * repl_palette_source.h - ODB-backed palette_menu_source_t adapter.
 *
 * Wraps menudata_list_leaves / menudata_describe_leaf (Common/source/menudata_headless.c)
 * to populate the palette_menu_source_t vtable from the headless menu storage
 * at system.menus.data.<menubar>.<menu>.<item>.
 *
 * Layering
 * --------
 *   menudata_headless.{c,h}   - ODB walks (PR 1+2)
 *   palette.h                 - vtable contract (PR 5)
 *   repl_palette_source.{c,h} - this adapter (PR 6)
 *   repl.c                    - event loop wiring (PR 6)
 *
 * Handle privacy contract
 * -----------------------
 * The palette holds script_handle pointers across many keystrokes — and across
 * GIL yield points. This adapter satisfies the palette_item_t::script_handle
 * stability requirement by route (a) per palette.h: it copyhandle()s the
 * script body into adapter-private storage on every cb_item_describe call,
 * keyed by the item's hashtable handle so re-describes of the same item
 * return the same cached copy (instead of allocating a fresh one each time).
 * The cache is freed wholesale by repl_palette_source_dispose. The adapter
 * never aliases the underlying ODB handle into the palette — every
 * palette_item_t.script_handle returned is an independently-allocated handle
 * owned by this adapter.
 *
 * Threading
 * ---------
 * All entry points hold the GIL — they read roottable / hashtable globals
 * and allocate lang values. Callers (palette state machine via the vtable)
 * are required by palette.h to hold the GIL when invoking these callbacks.
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors. See palette.h for the full
 * license text.
 */

#ifndef FRONTIER_REPL_PALETTE_SOURCE_H
#define FRONTIER_REPL_PALETTE_SOURCE_H

#include "palette.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The "menubar" identifier this adapter projects from. Defaults to "repl"
 * — system.menus.data.repl.<menu>.<item> — but tests / future hosts can
 * override per-instance via repl_palette_source_init_for().
 */
#define REPL_PALETTE_DEFAULT_MENUBAR "repl"

/*
 * Deprecated: use repl_palette_source_init_all. Preserved for compat.
 *
 * Initialise a palette_menu_source_t pointing at the default menubar
 * (system.menus.data.repl). Does NOT check .installed -- bypasses the
 * installed filter. Use repl_palette_source_init_all for production
 * enumeration that respects the installed flag.
 *
 * Returns true on success, false if the default menubar is absent or on
 * allocation failure.
 *
 * GIL: must be called while holding the GIL.
 */
bool repl_palette_source_init(palette_menu_source_t *out);

/*
 * Initialise for a specific menubar name (e.g. "test_bar" in unit tests).
 * Does NOT check .installed -- bypasses installed-filter for direct test
 * access. Use repl_palette_source_init_all for production enumeration.
 *
 * Stores the name internally; caller need not retain the string.
 *
 * GIL: must be held.
 */
bool repl_palette_source_init_for(palette_menu_source_t *out,
                                  const char *menubar_name);

/*
 * Enumerate ALL installed menubars under system.menus.data and compose their
 * top-level menus into a single horizontal strip (union semantics, matching
 * legacy OS menubar behavior). Only bars whose .installed field is true are
 * included. Bars are ordered alphabetically by name for deterministic output.
 *
 * Use this in production code (e.g. repl.c's run_palette_modal) instead of
 * repl_palette_source_init / repl_palette_source_init_for.
 *
 * Returns true if at least one installed bar was found; false if none are
 * installed (caller should suppress the palette in that case) or on
 * allocation failure.
 *
 * GIL: must be held. Calls menudata_ensure_root() and walks the full
 * system.menus.data subtree.
 */
bool repl_palette_source_init_all(palette_menu_source_t *out);

/*
 * Tear down the adapter. Releases any handles the adapter copied into its
 * internal cache (script_handles extracted via copyvaluerecord). Safe to
 * call after palette_close — the palette has already captured exec_script
 * by reference into the host's local variable.
 *
 * If src is null or was never initialised, this is a no-op.
 *
 * GIL: must be held — disposes ODB-allocated handles via disposehandle.
 */
void repl_palette_source_dispose(palette_menu_source_t *src);

#ifdef __cplusplus
}
#endif

#endif /* FRONTIER_REPL_PALETTE_SOURCE_H */
