/*
    Frontier CLI - REPL Interface
    Phase 1: Basic REPL Foundation

    repl.h - Main entry point for the Read-Eval-Print Loop interactive mode

    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#ifndef REPL_H
#define REPL_H

#include "cli_parser.h"
#include "ws_server.h"
#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"	 /* For hdlhashtable */

/* Maximum length for REPL navigation paths */
#define REPL_PATH_MAX_LEN 512

// Main REPL entry point
// Returns: exit code (0 for success, 1 for error)
int repl_main(cli_options_t *options, ws_server_t *ws_server);

/* Install/uninstall the repl.* verb host adapter (backs repl.syncScan etc.).
 * Called by repl_main; also called by protocol_main so that repl.* verbs
 * work in --protocol mode.  Safe to call without the GIL. */
void repl_install_verb_host(void);
void repl_uninstall_verb_host(void);

/* 2026-06-08 JES #691 Phase C.0 round 2 P1-8: reset the g_repl_exit_requested
 * flag to 0 so a stale value from a prior REPL session cannot pre-exit a new
 * one.  Call at the start of any REPL entry point, before install_verb_host. */
void repl_reset_exit_flag(void);

/* Result from index-aware path navigation (repl_navigate_path_ex).
 * Can represent either a table or a scalar value at the end of a path. */
typedef struct {
	hdlhashtable htable;	/* non-nil if result is a table */
	tyvaluerecord val;		/* the value (valid for both table and scalar results) */
	hdlhashnode hnode;		/* the node containing the value */
	boolean is_table;		/* true if result is a navigable table */
} typathlookupresult;

/* REPL Navigation - similar to CWD in a shell */

/* Get the current REPL table (like CWD). Returns roottable if at root. */
hdlhashtable repl_get_current_table(void);

/* Get the current REPL path as a string. Empty string means root. */
const char *repl_get_current_path(void);

/* Navigate to a path. Accepts dot-paths with or without leading @.
 * Returns true on success, false if path doesn't exist or isn't a table.
 */
boolean repl_jump_path(const char *path);

/* Resolve a path to a table without changing current table.
 * Accepts dot-paths, addresses, system.paths names, or script expressions.
 * Also supports relative paths when focused on a non-root table.
 * If resolved_path is non-NULL, fills it with the actual resolved path
 * (e.g., "system.verbs.builtins" for "parentOf(fileMenu)").
 * Returns the resolved table, or nil if path is invalid.
 */
hdlhashtable repl_resolve_path(const char *path, char *resolved_path, size_t path_bufsize);

/* Extended path resolution that returns both tables and scalar values.
 * Supports [n] index syntax (1-based) and relative paths.
 *
 * Resolution precedence (differs from repl_resolve_path for tables-only):
 *	 1. Empty path → current focused table
 *	 2. Script expressions → delegated to repl_resolve_path()
 *	 3. Relative to current focused table (if not at root) — tried FIRST
 *	 4. Single component without index → roottable lookup, then system.paths
 *	 5. Absolute path via navigate_path_with_index()
 *
 * Note: repl_resolve_path() uses the same order for multi-component paths
 * but tries roottable BEFORE system.paths for single components. Both
 * functions try relative resolution first when focused on a non-root table.
 *
 * If result->is_table is true, the path resolved to a table (in result->htable).
 * If result->is_table is false, the path resolved to a scalar (in result->val).
 * If resolved_path is non-NULL, fills it with the resolved path string.
 * Returns true if path resolved successfully, false on error.
 * On error, sets error_msg (if non-NULL) to describe the problem.
 *
 * Thread safety: Uses global roottable and g_repl_current_path. Not thread-safe;
 * must be called from the REPL thread only.
 *
 * Result values are borrowed references to the ODB; they remain valid as long as
 * the underlying database nodes are not modified or disposed. Callers should
 * deep-copy (via copyvaluerecord) if the value will be held across any
 * operations that could mutate the ODB.
 */
boolean repl_resolve_path_ex(const char *path, typathlookupresult *result,
							  char *resolved_path, size_t path_bufsize,
							  char *error_msg, size_t error_bufsize);

/* Check if REPL mode is currently active.
 * Used by msg() to add "msg: " prefix in interactive mode.
 */
boolean repl_is_active(void);

/*
 * 2026-06-08 JES #691 Phase C.0 round 3 P1: set/clear the g_repl_active flag.
 *
 * Used by boxen_repl_main to publish active status without direct access to
 * the file-static g_repl_active variable.  Call repl_set_active(true) after
 * repl_install_verb_host() and repl_set_active(false) before
 * repl_uninstall_verb_host() at teardown.
 */
void repl_set_active(boolean flag);

/*
 * 2026-06-08 JES #691 Phase C.0 round 3 P1: poll exit-requested flag.
 *
 * Returns true if g_repl_exit_requested has been set (by replverbhost_exit
 * via repl.exit() from UserTalk).  Used by boxen_repl_main to detect
 * repl.exit() calls that arrive outside the slash-command path.
 */
boolean repl_is_exit_requested(void);

/*
 * SIGWINCH (window-size change) consumer.
 *
 * install_signal_handlers() registers a SIGWINCH handler that sets a
 * process-wide sig_atomic_t flag.  Callers that need to react to terminal
 * resize (the upcoming slash-menu palette renderer in PR 5, etc.) poll this
 * accessor each render tick: it returns true exactly once per SIGWINCH
 * delivery and resets the flag.
 *
 * Coexists with file_browser.c's own SIGWINCH handling — file_browser saves
 * the previous sigaction and restores it on exit, so its scope is bounded to
 * the file-browser modal.  The REPL handler installed here is the
 * long-lived handler that file_browser's restore returns to.
 *
 * Returns false (without side effect) if no SIGWINCH has occurred.  Returning
 * true is destructive — only one consumer should poll this per frame.
 */
boolean repl_sigwinch_consumed(void);

#endif // REPL_H
