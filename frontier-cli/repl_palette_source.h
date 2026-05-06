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
 * stability requirement by route (b) per palette.h: extracting the script
 * handle from the deeply-copied record returned by menudata_describe_leaf().
 * That record's fields are independently allocated by push_field_or() (it
 * always copies via copyvaluerecord), so the handle survives both the
 * disposal of the surrounding record AND any subsequent GIL yield.
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
 * Initialise a palette_menu_source_t pointing at the default menubar
 * (system.menus.data.repl). The adapter holds its own private context
 * referenced by out->ctx; the caller does not need to allocate or free
 * anything between repl_palette_source_init() and the corresponding
 * repl_palette_source_dispose() — the adapter manages its cache lifecycle.
 *
 * Returns true on success, false on allocation failure.
 *
 * GIL: must be called while holding the GIL. Touches roottable to validate
 * the menubar exists; on first call lazy-creates system.menus.data via
 * menudata_ensure_root().
 */
bool repl_palette_source_init(palette_menu_source_t *out);

/*
 * Same, but for a specific menubar name (e.g. "test_menubar" in unit tests).
 * Stores the name internally; caller need not retain the string.
 *
 * GIL: must be held.
 */
bool repl_palette_source_init_for(palette_menu_source_t *out,
                                  const char *menubar_name);

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
