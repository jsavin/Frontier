/*
 * window_registry.h - Static REPL window sentinel and window-event bridge
 *
 * Phase C of docs/MENU_PORT_PLAN.md: the C-side bridge that fires
 * idopenwindowscript / idclosewindowscript when the REPL "frontmost window"
 * changes.  This is the headless equivalent of the legacy Carbon window
 * manager's focus-change events.
 *
 * DESIGN
 * ------
 * Legacy Frontier maintained a z-ordered window list; the OS called
 * shellwindow.c:shellrunwindowconfirmationscript() on focus changes.  In
 * headless mode there is no OS window manager.  Instead:
 *
 *   - A static "REPL window" sentinel is the only window that exists at
 *     boot.  It lives at the ODB path WINDOW_BRIDGE_REPL_PATH.
 *   - on_frontmost_changed(old_path, new_path) is called by repl.c whenever
 *     the conceptual "frontmost window" changes.  Each non-nil argument
 *     triggers the corresponding kernel-fired UserTalk hook:
 *       old != nil  ->  idclosewindowscript
 *       new != nil  ->  idopenwindowscript
 *   - The window path string is escaped with langdeparsestring(bs, '"') before
 *     substitution into the script template (issue #682 escaping contract).
 *     ASCII '"' is the delimiter used by the script templates; backslash-escaping
 *     embedded '"' prevents injection.
 *
 * DISPATCH PATH (per docs/MENU_PORT_PLAN.md Phase C)
 * --------------------------------------------------
 *   getsystemtablescript(idopenwindowscript)
 *     -> template: "if defined(system.callbacks){system.callbacks.openWindow(\"^0\");}"
 *   langdeparsestring(path, '"')  -- escape embedded ASCII double-quotes
 *   parsedialogstring(template, escaped_path, ...) -- substitute ^0
 *   langrunstringnoerror(substituted)           -- run synchronously
 *
 * TRUST ASSUMPTION
 * ----------------
 * In Phase C the only window path is the static REPL sentinel below
 * (WINDOW_BRIDGE_REPL_PATH), which is a compile-time constant with no
 * user-controllable content.  Future phases (Phase E editor windows) will
 * populate paths from ODB keys; those callers MUST pass the path through
 * langdeparsestring before calling on_frontmost_changed, or call this
 * function which does it internally.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors
 */

#ifndef WINDOW_REGISTRY_H
#define WINDOW_REGISTRY_H

#include "../Common/headers/frontier.h"

/*
 * ODB path for the static REPL window sentinel.
 *
 * Lives under system.temp.windowTypes.windows so that
 * Frontier.tools.windowTypes.init() (which creates the parent tables) does
 * not conflict.  "repl" is a fixed key -- the REPL is the only headless
 * window until Phase E editor windows arrive.
 *
 * This path is passed as ^0 to idopenwindowscript / idclosewindowscript.
 * The callbacks/openWindow.ut framework checks defined(adr^) and looks up
 * window.attributes.getOne("type", ...) on this address.
 */
#define WINDOW_BRIDGE_REPL_PATH "@system.temp.windowTypes.windows.repl"

/*
 * Initialize the REPL window sentinel in the ODB.
 *
 * Creates system.temp.windowTypes.windows.repl as a table and writes the
 * "type" = "ReplWindow" attribute into it so the windowTypes framework can
 * look up the window's type entry in Frontier.tools.data.windowTypes.
 *
 * Must be called after the system root is loaded (db_format_prepare_runtime)
 * and before on_frontmost_changed is first called.  Safe to call multiple
 * times (idempotent).
 *
 * Returns true on success, false on ODB error (REPL continues without the
 * sentinel -- the bridge will be a no-op for the open event).
 */
boolean window_registry_init(void);

/*
 * Fire the window-focus-change bridge scripts.
 *
 * old_path: C string ODB address of the window losing focus, or nil.
 *           If non-nil, fires idclosewindowscript with old_path as ^0.
 *
 * new_path: C string ODB address of the window gaining focus, or nil.
 *           If non-nil, fires idopenwindowscript with new_path as ^0.
 *
 * Edge cases:
 *   - Both nil: no-op (nothing fires).
 *   - old_path == new_path (strcmp equal): no-op -- same window is still
 *     frontmost; do not re-fire open/close for it.
 *   - Each path is independently escaped via langdeparsestring(path, '"') before
 *     parsedialogstring substitution (issue #682 contract).
 *
 * Errors: if getsystemtablescript or langrunstringnoerror fails, a warning
 * is logged and the function continues (boot-failure-safe per ADR-016 / the
 * REPL boot sequence contract).
 *
 * GIL: must be called with the GIL held.
 */
void on_frontmost_changed(const char *old_path, const char *new_path);

/*
 * Pascal bigstring construction
 * -----------------------------
 * Issue #707 merged this header's previously-local cstr_to_bigstring()
 * helper into the shared copyctopstring() in Common/source/strings.c
 * (declared in Common/headers/strings.h). copyctopstring() now returns
 * boolean (true on complete copy, false on truncation) and clamps the
 * payload to 255 bytes before the memmove, so it has the same safety
 * properties this header's local helper had. The CSTR_TO_BIGSTRING_LIT
 * macro is gone too -- callers with compile-time-known sizes should
 * place a `_Static_assert(sizeof(literal) <= 256, ...)` next to their
 * copyctopstring() call (see window_registry.c stmts[] for the pattern).
 *
 * Use copyctopstring() directly:
 *   #include "strings.h"
 *   if (!copyctopstring(window_path, bs_path)) { log_warn(...); return; }
 */

#endif /* WINDOW_REGISTRY_H */
