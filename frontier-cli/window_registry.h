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

#include <stdbool.h>
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
 * Copy a C string into a Pascal bigstring (length byte + up to 255 payload).
 *
 * Returns true on a complete copy, false when the input exceeded 255 bytes
 * and was truncated to fit. Pre-#685 the function returned void and
 * truncation was silent; that masked PR #683 bug 1 where a 411-byte
 * UserTalk script was cut mid-token at boot ("syntax error at line 1"
 * with no obvious root cause). Callers that pass dynamically-sized data
 * (e.g. ODB-derived window paths) MUST check the return and decide what
 * to do on truncation -- log a warning and abort the op in most cases,
 * since a truncated path or script is not safely recoverable.
 *
 * For compile-time-known string literals, prefer the
 * CSTR_TO_BIGSTRING_LIT() macro below: it adds a _Static_assert that
 * catches over-length literals at build time, with zero runtime cost.
 */
bool cstr_to_bigstring(const char *cstr, bigstring bs);

/*
 * Compile-time-checked wrapper around cstr_to_bigstring() for string-literal
 * inputs. The _Static_assert fires at build time if the literal (including
 * its NUL terminator) exceeds 256 bytes, which is the maximum that fits in
 * a Pascal bigstring (1 length byte + 255 payload). This would have caught
 * PR #683 bug 1 at compile time rather than at boot.
 *
 * Use this when the source argument is a string-literal expression; use the
 * bare cstr_to_bigstring() (and check its return) when the source is a
 * runtime-supplied C string. Callers that pass a string literal AND check
 * the return will get both a compile-time guarantee and a redundant runtime
 * check -- that's fine; the macro discards the return.
 *
 * Note: sizeof(literal) includes the trailing NUL, so a 255-character
 * payload occupies sizeof == 256. The bound is "<= 256" rather than "< 256".
 */
#define CSTR_TO_BIGSTRING_LIT(literal, bs)                                       \
	do {                                                                         \
		_Static_assert(sizeof(literal) <= 256,                                   \
		               "literal exceeds 255-byte Pascal bigstring limit");       \
		(void)cstr_to_bigstring((literal), (bs));                                \
	} while (0)

#endif /* WINDOW_REGISTRY_H */
