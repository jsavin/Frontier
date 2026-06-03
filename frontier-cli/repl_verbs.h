/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 2026 Frontier contributors

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

/*
 * repl_verbs.h - Five REPL kernel verbs surfaced into UserTalk:
 *
 *   repl.exit()
 *   repl.clearVariables()
 *   repl.jumpPath(path)
 *   repl.printKeyCodes()
 *   repl.list([path])
 *
 * These are bound to the host (frontier-cli) via a small function-pointer
 * adapter (repl_verbs_host_t). The verbs themselves live in the kernel
 * verb tree — registered once via replinitverbs() at startup — and dispatch
 * to whichever host is currently installed.
 *
 * Why an adapter instead of direct calls into repl.c?
 * ---------------------------------------------------
 *   1. Unit-testable. tests/repl_verbs_tests.c installs a fake adapter
 *      and observes side effects without booting the full REPL stack.
 *   2. Layering hygiene. The verb implementation lives in the runtime
 *      / verb-tree side; repl.c is a host. Direct calls into repl.c
 *      from the verb tree would create a cyclic dependency.
 *   3. Future hosts. A future GUI host can install a different adapter
 *      without forking the verb implementation.
 */

#ifndef REPL_VERBS_INCLUDE
#define REPL_VERBS_INCLUDE

#ifndef shelltypesinclude
	#include "shelltypes.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Host adapter. NULL function pointers are safe — verbs that find a NULL
 * hook return false at the script level rather than crash.
 *
 * Fields are documented by the corresponding verb above.
 */
typedef struct ty_repl_verbs_host {
	void (*exit)(void);
	void (*clear_variables)(void);
	/*
	 * jump_path: the host SHOULD return true on a successful navigation,
	 * false if the path is invalid or unreachable. The verb passes this
	 * boolean back as the script-level return value.
	 *
	 * SECURITY (input contract): the path string is forwarded to the
	 * host's repl_jump_path() implementation. If `path` contains any
	 * of the characters '(', ')', or '+', the host MAY interpret it
	 * as a UserTalk SCRIPT EXPRESSION (path_is_script_expression in
	 * repl.c) and evaluate it via langrun() to obtain the destination
	 * table. This is convenient for interactive use ("repl.jumpPath
	 * (\"foo() + bar\")") but means the verb is NOT safe to feed
	 * untrusted strings: an adversary controlling `path` can execute
	 * arbitrary UserTalk by including a '(' character. Callers
	 * forwarding user-supplied strings MUST validate or sanitize
	 * before passing.
	 */
	boolean (*jump_path)(const char *path);
	/*
	 * print_key_codes: dump pressed keys for diagnostic purposes.
	 *
	 * GIL/blocking note: linenoisePrintKeyCodes() takes exclusive control
	 * of stdin in raw mode and reads bytes in a tight loop. While it is
	 * running, the calling thread cannot release the GIL — every other
	 * GIL-dependent thread blocks until the user presses ESC+ESC+ESC. The
	 * host MUST therefore refuse the call when there is no interactive
	 * REPL session attached (non-TTY stdin, or REPL not active) — otherwise
	 * a script run via -e or via a webserver verb dispatch could
	 * accidentally hang the entire process waiting for keystrokes that
	 * cannot arrive.
	 *
	 * Returns true if the diagnostic actually ran, false if the host
	 * refused the call (no interactive context). The verb forwards this
	 * boolean back as the script-level return value.
	 */
	boolean (*print_key_codes)(void);
	/*
	 * list: NULL path means "current table" (the host decides how to
	 * resolve that). A non-NULL path is a NUL-terminated UTF-8 C string
	 * forwarded verbatim.
	 */
	void (*list)(const char *path);
	/*
	 * help: print the REPL slash-command reference. Backs repl.help() —
	 * the kernel verb the menubar /help handler invokes. The host
	 * implementation typically calls repl_output_help() (the C-side
	 * help-text emitter that the legacy /help slash handler used).
	 *
	 * Why a kernel verb instead of letting the UserTalk handler print
	 * its own text? UserTalk in headless mode lacks a raw-stdout
	 * primitive — `msg` prepends "msg: " in REPL mode, and there is no
	 * `print` / `stdout` verb in the kernel verb tree. Until PR 8 (or
	 * a follow-up) adds a proper raw-stdout verb, the help text must
	 * be emitted from C. See the help.ut handler at
	 * usertalk_scripts/Frontier.root/system/menus/handlers/repl/help.ut
	 * for the call site.
	 */
	void (*help)(void);
	/*
	 * from_slash: returns true iff the currently-executing handler script
	 * was dispatched from a slash command (e.g. the user typed /jump or
	 * /list at the REPL prompt) rather than activated via the palette
	 * menu. UserTalk scripts call repl.fromSlash() to branch on this
	 * flag so they can show interactive prompts (dialog.ask) when the
	 * user picks a menu item without an argument, while slash commands
	 * preserve their existing no-prompt semantics.
	 *
	 * The host sets g_repl_dispatching_from_slash true immediately before
	 * calling meuserselected_headless (or dispatch_synthesized_script) in
	 * dispatch_slash_command and clears it false on every exit path. The
	 * GIL is held for the full dispatch, so the boolean is visible to any
	 * UserTalk code running in that call chain without additional locking.
	 *
	 * Returns false when no host is installed (scripts not running inside
	 * a REPL dispatch return false, matching "not a slash command").
	 */
	boolean (*from_slash)(void);
	/*
	 * is_active: returns true iff a REPL session is currently running
	 * (the host has entered the REPL main loop and not yet exited).
	 * UserTalk handler scripts use repl.isActive() to guard interactive
	 * operations (like dialog.ask prompts) so they do not fire when the
	 * handler is called from non-REPL contexts (integration tests,
	 * protocol mode, -e evaluation).
	 *
	 * Backed by g_repl_active in repl.c, which is set true at REPL entry
	 * and false on every exit path.
	 *
	 * Returns false when no host is installed.
	 */
	boolean (*is_active)(void);
	/*
	 * sync_scan: walk the ut-sync tree and auto-create any ODB nodes whose
	 * .ut file exists on disk but is absent from the in-memory hashtable.
	 * Delegates to ut_sync_scan_and_create(). Backs repl.syncScan().
	 *
	 * Returns the count of newly created ODB nodes (>= 0), or 0 when
	 * ut-sync mode is not active (--ut-sync-dir was not supplied) so the
	 * verb is a safe no-op in non-sync sessions.
	 *
	 * Return contract: the verb returns the count as a UserTalk number
	 * (longvalue). A return of 0 means "no orphans found" OR "not in
	 * ut-sync mode"; a return >= 1 means that many nodes were created.
	 * Returns 0 (not a crash) when no host is installed.
	 */
	int (*sync_scan)(void);
} repl_verbs_host_t;


/*
 * Register the 5 REPL verbs into the kernel verb tree under the "repl"
 * namespace. Idempotent — safe to call multiple times if needed.
 *
 * Returns true on success. Failure modes: out-of-memory in
 * newfunctionprocessor / langaddkeyword.
 *
 * GIL: must be called while holding the GIL — manipulates the shared
 * hashtable stack via pushhashtable / pophashtable.
 *
 * Call from main.c after langinitverbs() and before any user script can
 * reference `repl.*`.
 */
extern boolean replinitverbs(void);


/*
 * Install the host adapter. Pass NULL to clear (verbs then return false
 * at the script level rather than crash). The adapter struct is COPIED
 * by value internally, so the caller may free or stack-allocate `host`.
 *
 * Thread-safety note: host installation is protected by no mutex. The
 * intended pattern is "install once at REPL startup, never change" —
 * the host pointer is read by the verb dispatcher on the main thread,
 * and it is the caller's responsibility to install before any user
 * script can run.
 */
extern void repl_verbs_set_host(const repl_verbs_host_t *host);


#ifdef __cplusplus
}
#endif

#endif /* REPL_VERBS_INCLUDE */
