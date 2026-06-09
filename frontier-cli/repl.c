/*
    repl.c - Main REPL loop with linenoise integration for line editing and history

    Uses linenoise (https://github.com/antirez/linenoise) for cross-platform
    line editing, history, and tab completion support.

    Phase 4: Event loop architecture for concurrent REPL + webserver + agents.
    Uses non-blocking linenoise API with poll() for multiplexing.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>

#include "linenoise.h"

#include "terminal_control.h"
#include "repl.h"
#include "repl_eval.h"
#include "repl_variables.h"
#include "repl_output.h"
#include "repl_slash_resolver.h"
#include "repl_verbs.h"
#include "completion.h"
#include "pane.h"
#include "palette.h"
#include "palette_arg_inject.h"
#include "repl_palette_source.h"
#include "scrollback_pane.h"
#include "repl_output_async.h"
#include "window_registry.h"	/* window_registry_init, on_frontmost_changed */
#include "../Common/headers/menudata_headless.h" /* meuserselected_headless */
#include "../Common/headers/oplist.h"		/* opcountlistitems */
#include "../Common/headers/langsystem7.h"	/* getnthlistval */
#include "../Common/headers/frontier.h"
#include "../Common/headers/logging.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/langexternal.h" /* langexternalvaltotable */
#include "../Common/headers/memory.h"		/* newemptyhandle, sethandlesize */
#include "../Common/headers/tcpverbs.h"		/* tcp_process_callbacks */
#include "../Common/headers/process.h"		/* agentsenabled, agentscheduler_tick */
#include "../Common/headers/langinternal.h" /* flreplmode */
#include "../Common/headers/threadregistry.h" /* headless_backgroundtask */
#include "ws_server.h"						 /* ws_server_t, ws_server_* */
#include "ut_scan.h"						 /* ut_sync_scan_and_create */

/* cli_get_ut_sync_dir and cli_get_system_root_basename are defined in main.c.
 * Forward-declared here to avoid a circular header dependency -- same pattern
 * as ut_scan.c::cli_record_ut_import. */
extern const char *cli_get_ut_sync_dir(void);
extern const char *cli_get_system_root_basename(void);

// History configuration
#define HISTORY_FILE ".frontier_history"
#define HISTORY_SIZE 1000
#define MAX_SESSION_COMMANDS 1000
#define MAX_COMMAND_LEN 4096

// Event loop configuration
#define POLL_TIMEOUT_MS 10	// 10ms = responsive while allowing ~100Hz callback processing

// REPL prompt settings
#define g_repl_prompt_MAX_LEN 256
/* Moved to repl.h so other REPL modules can use it */

// REPL navigation state - tracks current table like CWD in a shell
static hdlhashtable g_repl_current_table = nil;
static char g_repl_current_path[REPL_PATH_MAX_LEN] = "";
static char g_repl_prompt[g_repl_prompt_MAX_LEN] = "[root]> ";

// Guest database navigation state
/* TODO: These guest DB globals are not thread-safe. If the REPL ever supports
 * concurrent access (e.g., background script evaluation while navigating),
 * migrate to thread-local storage or an explicit REPL context struct. */
static boolean g_repl_in_guest_db = false;
static char g_repl_guest_db_name[REPL_PATH_MAX_LEN] = "";
/* NOTE: g_repl_guest_db_root holds a borrowed handle to the guest database's
 * root table. This handle becomes dangling if the guest database is closed
 * (e.g., via window.close() or file.close()) while the REPL is still navigated
 * into it. validate_guest_db_state() checks filewindowtable to detect this
 * and resets navigation to root when the handle is stale. */
static hdlhashtable g_repl_guest_db_root = nil;

// REPL active flag - set when REPL event loop is running
static boolean g_repl_active = false;

/*
 * Palette argument injection (PR 8 / Rung 2 accepts_args).
 *
 * When the slash-menu palette dispatches a leaf with accepts_args=true,
 * the user's typed argument is captured in palette_state_t::exec_arg.
 * The handler scripts under system.menus.handlers.repl.* are stored
 * in the menubar as call expressions like "system.menus.handlers.repl.
 * list ()" — the call frame has empty parens.
 *
 * To bind the typed arg into that call, the dispatch path synthesizes
 * a NEW UserTalk source string per dispatch by replacing the empty
 * parens with a single quoted string literal:
 *
 *     system.menus.handlers.repl.list ("typed-arg")
 *
 * The synthesized source is fed to meuserselected_headless / langbuildtree
 * just like the original — the parser sees a fully-formed call with the
 * arg bound at compile time. There is NO process-global hand-off state
 * between the dispatcher and any host verb adapter, so a sibling
 * thread.new() child running its own palette dispatch cannot consume
 * the wrong arg via langruncode's GIL-yield window.
 *
 * Implementation: palette_arg_escape + palette_arg_inject_into_script
 * in palette_arg_inject.c. Pure helpers — no ODB / handle dependencies
 * — exercised by tests/palette_arg_inject_tests.c.
 */

/*
 * Exit-requested flag set by the repl.exit() kernel verb (via the host
 * adapter). Both REPL loops (event-loop and blocking) poll this each
 * iteration and break out cleanly. Distinct from g_repl_interrupt_requested,
 * which is set asynchronously by the SIGINT handler. Reset to 0 on REPL
 * startup so a stale flag from a previous session can't pre-exit a new one.
 *
 * Storage class: volatile sig_atomic_t — same pattern as
 * g_repl_interrupt_requested below. The flag is written by the verb
 * dispatcher (which runs on whichever thread holds the GIL: the REPL
 * main thread under normal use, or any thread.new() child that
 * acquires the GIL and calls repl.exit()) and read by the main REPL
 * loop. Even though writes today happen with the GIL held — so the
 * GIL acquire/release barrier covers visibility — encoding the
 * invariant in storage rather than in a comment defends against
 * future refactors that might relocate the writer off the GIL, and
 * costs nothing on the read side.
 */
static volatile sig_atomic_t g_repl_exit_requested = 0;

/*
 * Set across user-script execution so the SIGINT handler (installed further
 * down) knows it should request interruption rather than tearing the REPL
 * down. Hoisted to the top of the file so callers earlier in the source
 * (e.g. repl_jump_script) can wrap their langrun() calls.
 *
 * Writers: any function on the GIL that runs a user script. Reader: the
 * SIGINT handler, which uses sig_atomic_t reads to stay async-signal-safe.
 *
 * NOTE: writers MUST clear this flag on every exit path (success and
 * failure) — a stale "1" makes Ctrl-C requests interruption against a
 * non-running script and confuses the next command's signal handling.
 */
static volatile sig_atomic_t g_script_running = 0;

/*
 * Set to true for the duration of a slash-command dispatch so UserTalk
 * handler scripts can branch on repl.fromSlash() to distinguish
 * "user typed /jump" from "user activated Jump via the palette menu".
 *
 * Lifetime contract: set to true immediately before calling
 * meuserselected_headless (or dispatch_synthesized_script) from
 * dispatch_slash_command, cleared to false on every exit path including
 * error paths.  Because the REPL modal is GIL-held throughout, and
 * dispatch_slash_command is non-reentrant within a single REPL session,
 * a plain boolean (no volatile) is sufficient -- the GIL acquire/release
 * barrier covers visibility to any GIL-acquired thread that reads it via
 * the repl.fromSlash() verb during the same dispatch call.
 *
 * Writers: dispatch_slash_command only.
 * Readers: replverbhost_from_slash (via the repl.fromSlash() verb).
 */
static boolean g_repl_dispatching_from_slash = false;

// Session command tracking for merge-before-save
static char *session_commands[MAX_SESSION_COMMANDS];
static size_t session_command_count = 0;

/*
 * safe_print_user_string - terminal-injection hardener.
 *
 * Print a user-controlled string to stdout with control bytes scrubbed.
 * Mirrors the policy in pane.c::safe_render_codepoint(): \t \n \r are
 * legitimate inside user content (paths can't contain them legally,
 * but error messages may); every other byte < 0x20 plus 0x7F (DEL) is
 * replaced with '?'.
 *
 * Threat: error/diagnostic printf paths interpolate user-supplied
 * strings (table paths, menu item names, etc.). An adversary who can
 * choose a name containing ESC '[' '2' 'J' could clear the screen by
 * triggering "/list <evil>" — or worse, OSC 52 to write the user's
 * clipboard. Filtering at the print site short-circuits that surface.
 *
 * The helper is byte-oriented: high bytes (>= 0x80) pass through
 * unmodified (legitimate UTF-8 continuation), and the caller does NOT
 * need to NUL-terminate the buffer of the helper allocates a stack
 * temporary capped at SAFE_PRINT_MAX. Truncation appends "..." so the
 * user can tell something was dropped.
 */
#define SAFE_PRINT_MAX 1024

static void safe_print_user_string(const char *s) {
	if (s == NULL) {
		fputs("(null)", stdout);
		return;
	}
	char buf[SAFE_PRINT_MAX];
	size_t out = 0;
	const size_t cap = sizeof(buf) - 4; /* leave room for "..." + NUL */
	while (*s != '\0' && out < cap) {
		unsigned char c = (unsigned char)*s++;
		if (c == '\t' || c == '\n' || c == '\r') {
			buf[out++] = (char)c;
		} else if (c < 0x20 || c == 0x7F) {
			buf[out++] = '?';
		} else {
			buf[out++] = (char)c;
		}
	}
	if (*s != '\0') {
		buf[out++] = '.';
		buf[out++] = '.';
		buf[out++] = '.';
	}
	buf[out] = '\0';
	fputs(buf, stdout);
}

/* Clears all guest database navigation state */
static void clear_guest_db_state(void) {
	g_repl_in_guest_db = false;
	g_repl_guest_db_name[0] = '\0';
	g_repl_guest_db_root = nil;
}

/* Validates that g_repl_guest_db_root is still a live entry in filewindowtable.
 * If the guest database has been closed (handle is dangling), clears guest DB
 * state and resets navigation to root. Returns true if still valid (or not in
 * guest DB mode), false if state was cleared due to a stale handle. */
static boolean validate_guest_db_state(void) {
	if (!g_repl_in_guest_db || g_repl_guest_db_root == nil)
		return true;  /* Not in guest DB mode, nothing to validate */

	if (filewindowtable == nil) {
		/* No filewindowtable at all — guest DB can't be valid */
		log_debug(LOG_COMP_GENERAL, "REPL: filewindowtable gone, clearing guest DB state");
		clear_guest_db_state();
		g_repl_current_table = roottable;
		g_repl_current_path[0] = '\0';
		return false;
	}

	/* Walk filewindowtable entries to see if any resolve to our stored root */
	hdlhashnode fwnomad;
	for (fwnomad = (**filewindowtable).hfirstsort; fwnomad != nil; fwnomad = (**fwnomad).sortedlink) {
		hdlhashtable htable;
		if (langexternalvaltotable((**fwnomad).val, &htable, fwnomad) && htable == g_repl_guest_db_root) {
			return true;  /* Found it — handle is still valid */
		}
	}

	/* Guest DB root not found in filewindowtable — it was closed */
	log_debug(LOG_COMP_GENERAL, "REPL: guest DB '%s' no longer in filewindowtable, resetting to root",
			  g_repl_guest_db_name);
	clear_guest_db_state();
	g_repl_current_table = roottable;
	g_repl_current_path[0] = '\0';
	return false;
}

/* Updates the prompt string based on current path */
static void update_prompt(void) {
	/* Validate guest DB is still open before rendering prompt */
	validate_guest_db_state();

	if (g_repl_in_guest_db) {
		snprintf(g_repl_prompt, g_repl_prompt_MAX_LEN, "[%s::%s]> ",
				 g_repl_guest_db_name, g_repl_current_path);
	} else if (g_repl_current_path[0] == '\0') {
		snprintf(g_repl_prompt, g_repl_prompt_MAX_LEN, "[root]> ");
	} else {
		snprintf(g_repl_prompt, g_repl_prompt_MAX_LEN, "[%s]> ", g_repl_current_path);
	}
}

/* Get the current REPL table (like CWD) */
hdlhashtable repl_get_current_table(void) {
	if (g_repl_current_table == nil) {
		return roottable;
	}
	return g_repl_current_table;
}

/* Get the current REPL path */
const char *repl_get_current_path(void) {
	return g_repl_current_path;
}

/* Check if REPL mode is active */
boolean repl_is_active(void) {
	return g_repl_active;
}

/*
 * 2026-06-08 JES #691 Phase C.0 round 3 P1: repl_set_active.
 *
 * Called by boxen_repl_main to publish g_repl_active so that
 * repl.isActive() returns true during the boxen REPL session.
 * repl_main sets g_repl_active directly inline; this wrapper exposes the
 * same capability to boxen_repl_main without widening the public surface.
 */
void repl_set_active(boolean flag) {
	g_repl_active = flag;
}

/*
 * 2026-06-08 JES #691 Phase C.0 round 3 P1: repl_is_exit_requested.
 *
 * Getter for g_repl_exit_requested.  boxen_repl_main polls this each event
 * loop iteration so that repl.exit() invoked from non-slash UserTalk code
 * (which calls replverbhost_exit -> sets g_repl_exit_requested) terminates
 * the boxen REPL session.
 */
boolean repl_is_exit_requested(void) {
	return g_repl_exit_requested != 0;
}

/* Check if path looks like a script expression (contains ( ) or +) */
static boolean path_is_script_expression(const char *path) {
	if (path == NULL) return false;
	while (*path) {
		if (*path == '(' || *path == ')' || *path == '+') {
			return true;
		}
		path++;
	}
	return false;
}

/* Evaluate a script and jump to the resulting address.
 * Returns true if script evaluated to a valid table address.
 */
static boolean repl_jump_script(const char *script) {
	/* Create handle for script text */
	size_t script_len = strlen(script);
	Handle htext = nil;

	if (!newemptyhandle(&htext)) {
		return false;
	}
	if (!sethandlesize(htext, (long)script_len)) {
		disposehandle(htext);
		return false;
	}
	HLock(htext);
	memcpy(*htext, script, script_len);
	HUnlock(htext);

	/* Evaluate the script */
	tyvaluerecord val;
	boolean flpushpop = !flscriptrunning;

	if (flpushpop)
		flpushpop = pushprocess(nil);

	/* Mark script-running across the langrun call so the SIGINT handler
	 * requests interruption of the user expression rather than tearing the
	 * REPL down. Mirrors the wrap pattern used by process_line() (the
	 * eval-line path) and dispatch_synthesized_script() / dispatch_leaf_via_menubar()
	 * (the menubar-handler paths). The legacy `/jump expr()` path was missing
	 * this wrap (issue #591); without it Ctrl-C during a long-running jump
	 * expression would not interrupt cleanly. */
	g_script_running = 1;
	boolean fl = langrun(htext, &val);	/* langrun consumes htext */
	g_script_running = 0;

	if (flpushpop)
		popprocess();

	if (!fl) {
		return false;
	}

	/* Check if result is an address */
	if (val.valuetype != addressvaluetype) {
		/* Not an address - try to interpret as external table value */
		if (val.valuetype == externalvaluetype) {
			hdlhashtable target = nil;
			if (langexternalvaltotable(val, &target, nil) && target != nil) {
				g_repl_current_table = target;
				/* For script results, show the expression in prompt */
				strncpy(g_repl_current_path, script, REPL_PATH_MAX_LEN - 1);
				g_repl_current_path[REPL_PATH_MAX_LEN - 1] = '\0';
				update_prompt();
				repl_set_focus(target);
				return true;
			}
		}
		return false;
	}

	/* Extract table and name from address */
	hdlhashtable htable = nil;
	bigstring bsname;

	if (!getaddressvalue(val, &htable, bsname)) {
		return false;
	}

	/* Look up the final name in the table to get the target table */
	tyvaluerecord targetval;
	hdlhashnode hnode;

	if (!hashtablelookup(htable, bsname, &targetval, &hnode)) {
		return false;
	}

	/* The target must be a table */
	hdlhashtable target = nil;
	if (!langexternalvaltotable(targetval, &target, hnode) || target == nil) {
		return false;
	}

	/* Build the path string from the address */
	bigstring bspath;
	if (!getaddresspath(val, bspath)) {
		/* Fallback: use the script as the path display */
		strncpy(g_repl_current_path, script, REPL_PATH_MAX_LEN - 1);
	} else {
		/* Convert bigstring to C string, skip leading @ and "root." prefix */
		size_t pathlen = stringlength(bspath);
		const char *pathstart = (const char *)stringbaseaddress(bspath);
		if (pathlen > 0 && pathstart[0] == '@') {
			pathstart++;
			pathlen--;
		}
		/* Skip "root." prefix if present */
		if (pathlen > 5 && strncmp(pathstart, "root.", 5) == 0) {
			pathstart += 5;
			pathlen -= 5;
		}
		if (pathlen >= REPL_PATH_MAX_LEN) {
			pathlen = REPL_PATH_MAX_LEN - 1;
		}
		memcpy(g_repl_current_path, pathstart, pathlen);
		g_repl_current_path[pathlen] = '\0';
	}

	g_repl_current_table = target;
	update_prompt();
	repl_set_focus(target);
	return true;
}

/* ======================================================================
 * Index-aware path navigation helpers
 *
 * Resolution order (for navigate_path_with_index):
 *	 1. For each dot-component, try hashtablelookup() in current table
 *	 2. If first component fails lookup, try system.paths fallback
 *	 3. If component has [n] suffix, resolve name first then index into result
 *
 * All indices are 1-based (UserTalk convention). Internally converted
 * to 0-based for hashgetnthnode().
 * ====================================================================== */

#define INDEX_NODE_NAME_MAX	 COMPLETION_MAX_NAME_LEN  /* max length for node names */

/* Safely append a path component to a tracked-length buffer.
 * Returns false if the buffer would overflow (sets error_msg).
 */
static boolean path_buf_append(char *buf, size_t bufsize, size_t *len,
								const char *component,
								char *error_msg, size_t error_bufsize) {
	size_t comp_len = strlen(component);
	size_t need = *len + ((*len > 0) ? 1 : 0) + comp_len;

	if (need >= bufsize) {
		if (error_msg && error_bufsize > 0)
			snprintf(error_msg, error_bufsize, "path too long (limit %zu)", bufsize);
		return false;
	}

	if (*len > 0) {
		buf[*len] = '.';
		(*len)++;
	}

	memcpy(buf + *len, component, comp_len);
	*len += comp_len;
	buf[*len] = '\0';

	return true;
}

/* Get the C-string name of a hash node.
 * Writes into caller-provided buffer. Returns the string length.
 */
static size_t get_node_cname(hdlhashnode hnode, char *cname, size_t bufsize) {
	bigstring bsname;
	gethashkey(hnode, bsname);
	size_t nlen = stringlength(bsname);
	if (nlen > bufsize - 1) nlen = bufsize - 1;
	memcpy(cname, stringbaseaddress(bsname), nlen);
	cname[nlen] = '\0';
	return nlen;
}

/* Apply a 1-based index to a hash table, returning the indexed node.
 * On failure, sets error_msg with context (using context_name for the table name).
 * Returns true on success, with *out_node set.
 */
static boolean apply_index_to_table(hdlhashtable htable, long index,
									 const char *context_name,
									 hdlhashnode *out_node,
									 char *error_msg, size_t error_bufsize) {
	*out_node = nil;

	if (!hashgetnthnode(htable, index - 1, out_node) || *out_node == nil) {
		if (error_msg && error_bufsize > 0) {
			if (context_name && context_name[0] != '\0')
				snprintf(error_msg, error_bufsize, "index %ld out of range in '%s'", index, context_name);
			else
				snprintf(error_msg, error_bufsize, "index %ld out of range", index);
		}
		return false;
	}

	return true;
}

/* Resolve an indexed node as a final or intermediate result.
 * If is_last, fills result with the node's value (table or scalar).
 * If not is_last, extracts a table for continued navigation into *out_table.
 *
 * IMPORTANT: result->val and result->hnode are borrowed references from the ODB.
 * They remain valid only while the underlying hash table nodes are not modified
 * or disposed. Caller must not hold these across ODB mutation operations.
 *
 * Returns true on success.
 */
static boolean resolve_indexed_node(hdlhashtable htable, hdlhashnode hnode, long index,
									 const char *context_name,
									 boolean is_last,
									 typathlookupresult *result,
									 hdlhashtable *out_table,
									 char *name_path, size_t name_path_bufsize, size_t *name_path_len,
									 char *resolved_name_path, size_t resolved_bufsize,
									 char *error_msg, size_t error_bufsize) {
	/* Get the actual name of the indexed node and append to path */
	char cname[INDEX_NODE_NAME_MAX];
	get_node_cname(hnode, cname, sizeof(cname));

	if (!path_buf_append(name_path, name_path_bufsize, name_path_len, cname, error_msg, error_bufsize))
		return false;

	/* apply_index_to_table() guarantees hnode != nil && *hnode != nil on success.
	 * Assert this contract rather than silently handling a violation. */
	assert(hnode != nil && *hnode != nil);

	/* Resolve on-disk values before reading. Without this, nodes with
	 * unresolved external handles (e.g. when using --skip-startup) cause
	 * SIGSEGV when langexternalvaltotable dereferences the stale handle. */
	if (!hashresolvevalue(htable, hnode)) {
		if (error_msg && error_bufsize > 0)
			snprintf(error_msg, error_bufsize, "failed to resolve value at index %ld ('%s') in '%s'",
					 index, cname[0] ? cname : "?", context_name ? context_name : "?");
		return false;
	}

	tyvaluerecord nodeval = (**hnode).val;

	if (is_last) {
		hdlhashtable subtable = nil;
		if (langexternalvaltotable(nodeval, &subtable, hnode) && subtable != nil) {
			result->htable = subtable;
			result->is_table = true;
		} else {
			result->is_table = false;
		}
		result->val = nodeval;
		result->hnode = hnode;
		if (resolved_name_path && resolved_bufsize > 0) {
			strncpy(resolved_name_path, name_path, resolved_bufsize - 1);
			resolved_name_path[resolved_bufsize - 1] = '\0';
		}
		return true;
	}

	/* Not last - must be a table to continue */
	hdlhashtable subtable = nil;
	if (!langexternalvaltotable(nodeval, &subtable, hnode) || subtable == nil) {
		if (error_msg && error_bufsize > 0) {
			if (context_name && context_name[0] != '\0')
				snprintf(error_msg, error_bufsize, "item at index %ld in '%s' is not a table", index, context_name);
			else
				snprintf(error_msg, error_bufsize, "item at index %ld is not a table", index);
		}
		return false;
	}

	*out_table = subtable;
	return true;
}

/* Parse a component for [n] index syntax.
 * Splits "name[n]" into name_part and *out_index (1-based, or -1 if no index).
 * Validates: brackets match, no trailing chars after ], index >= 1.
 * Returns true if parsing succeeded.
 */
static boolean parse_index_component(const char *component,
									  char *name_part, size_t name_part_size,
									  long *out_index,
									  char *error_msg, size_t error_bufsize) {
	*out_index = -1;

	char *bracket = strchr(component, '[');
	if (bracket == NULL) {
		strncpy(name_part, component, name_part_size - 1);
		name_part[name_part_size - 1] = '\0';
		return true;
	}

	/* Extract name part before bracket */
	size_t name_len = (size_t)(bracket - component);
	if (name_len >= name_part_size) name_len = name_part_size - 1;
	memcpy(name_part, component, name_len);
	name_part[name_len] = '\0';

	/* Parse index from [n] */
	char *endptr = NULL;
	long parsed = strtol(bracket + 1, &endptr, 10);
	if (endptr == NULL || *endptr != ']') {
		if (error_msg && error_bufsize > 0)
			snprintf(error_msg, error_bufsize, "invalid index syntax in '%s'", component);
		return false;
	}

	/* Reject trailing characters after closing bracket (e.g. "system[1]foo") */
	if (*(endptr + 1) != '\0') {
		if (error_msg && error_bufsize > 0)
			snprintf(error_msg, error_bufsize, "invalid index syntax in '%s' (unexpected characters after ']')", component);
		return false;
	}

	if (parsed < 1) {
		if (error_msg && error_bufsize > 0)
			snprintf(error_msg, error_bufsize, "index must be >= 1 (got %ld)", parsed);
		return false;
	}

	/* Sanity-check upper bound. hashgetnthnode() takes long, but no hash table
	 * will realistically have more than INT_MAX entries. Reject obviously
	 * out-of-range values early to avoid potential overflow in downstream code. */
	if (parsed > INT_MAX) {
		if (error_msg && error_bufsize > 0)
			snprintf(error_msg, error_bufsize, "index %ld exceeds maximum (%d)", parsed, INT_MAX);
		return false;
	}

	*out_index = parsed;
	return true;
}

/* Apply index to a table and resolve the resulting node.
 * Combines apply_index_to_table() + resolve_indexed_node() into one call.
 * If is_last and the node is resolved, returns true with result populated.
 * If not is_last, sets *out_table for continued navigation.
 * Sets *finished to true if caller should return immediately (final component resolved).
 */
static boolean apply_and_resolve_index(hdlhashtable htable, long index,
										const char *context_name, boolean is_last,
										typathlookupresult *result, hdlhashtable *out_table,
										char *name_path, size_t name_path_bufsize, size_t *name_path_len,
										char *resolved_name_path, size_t resolved_bufsize,
										char *error_msg, size_t error_bufsize,
										boolean *finished) {
	*finished = false;

	hdlhashnode hnode = nil;
	if (!apply_index_to_table(htable, index, context_name, &hnode, error_msg, error_bufsize))
		return false;

	if (!resolve_indexed_node(htable, hnode, index, context_name, is_last, result, out_table,
							   name_path, name_path_bufsize, name_path_len,
							   resolved_name_path, resolved_bufsize,
							   error_msg, error_bufsize))
		return false;

	if (is_last) *finished = true;
	return true;
}

/* Navigate a dot-path with [n] index syntax support, starting from a given table.
 * Tokenizes by '.' and for each component checks for [n] suffix.
 * Indices are 1-based (UserTalk convention); internally converted to 0-based for hashgetnthnode.
 * When start_table != roottable, the system.paths fallback for the first component is skipped.
 *
 * Returns true if path resolved successfully.
 * On success, result contains either a table or scalar value.
 * On failure, error_msg describes the problem (if non-NULL).
 */
static boolean navigate_path_with_index_from(hdlhashtable start_table, const char *path,
											  typathlookupresult *result,
											  char *resolved_name_path, size_t resolved_bufsize,
											  char *error_msg, size_t error_bufsize) {
	if (path == NULL || path[0] == '\0' || result == NULL) {
		return false;
	}

	/* Initialize result */
	result->htable = nil;
	result->hnode = nil;
	result->is_table = false;
	clearbytes(&result->val, sizeof(result->val));

	/* Make a mutable copy for tokenization */
	char path_copy[REPL_PATH_MAX_LEN];
	strncpy(path_copy, path, REPL_PATH_MAX_LEN - 1);
	path_copy[REPL_PATH_MAX_LEN - 1] = '\0';

	/* Build a resolved name path (using actual names, not indices) */
	char name_path[REPL_PATH_MAX_LEN] = "";
	size_t name_path_len = 0;

	hdlhashtable current = start_table;
	size_t depth = 0;
	size_t components_consumed = 0;
	size_t total_components = 0;
	boolean first_component = true;

	/* Count total components for depth-cap validation.
	 * Simply counts dots; n dots = n+1 components. Brackets can't contain dots
	 * (they hold numeric indices), so no special handling is needed. */
	for (const char *p = path; *p; p++) {
		if (*p == '.') total_components++;
	}
	total_components++; /* n dots = n+1 components */

	char *saveptr = NULL;
	char *component = strtok_r(path_copy, ".", &saveptr);

	while (component != NULL && current != nil && depth < (size_t)COMPLETION_MAX_PATH_DEPTH) {
		/* Parse [n] index suffix */
		long index = -1;
		char name_part[INDEX_NODE_NAME_MAX];

		if (!parse_index_component(component, name_part, sizeof(name_part), &index, error_msg, error_bufsize))
			return false;

		char *next_component = strtok_r(NULL, ".", &saveptr);
		boolean is_last = (next_component == NULL);

		if (name_part[0] == '\0' && index > 0) {
			/* [n] alone with no name - index directly into current table */
			hdlhashtable next_table = nil;
			boolean finished = false;
			if (!apply_and_resolve_index(current, index, NULL, is_last, result, &next_table,
										  name_path, sizeof(name_path), &name_path_len,
										  resolved_name_path, resolved_bufsize,
										  error_msg, error_bufsize, &finished))
				return false;
			if (finished) return true;
			current = next_table;
		} else {
			/* Named component - look it up */
			bigstring bs;
			copyctopstring(name_part, bs);

			tyvaluerecord val;
			hdlhashnode node;
			if (!hashtablelookup(current, bs, &val, &node)) {
				/* Try system.paths fallback for first component (only from roottable) */
				if (first_component && start_table == roottable) {
					hdlhashtable path_result = completion_search_paths(name_part);
					if (path_result != nil) {
						current = path_result;

						/* Use the actual resolved path from system.paths */
						char paths_resolved[REPL_PATH_MAX_LEN];
						hdlhashtable check = completion_search_paths_ex(name_part, paths_resolved, sizeof(paths_resolved),
																		  NULL, NULL, 0);
						if (check != nil && paths_resolved[0] != '\0') {
							strncpy(name_path, paths_resolved, sizeof(name_path) - 1);
							name_path[sizeof(name_path) - 1] = '\0';
							name_path_len = strlen(name_path);
						} else {
							strncpy(name_path, name_part, sizeof(name_path) - 1);
							name_path[sizeof(name_path) - 1] = '\0';
							name_path_len = strlen(name_path);
						}

						/* If there's an index, apply it to the resolved table */
						if (index > 0) {
							hdlhashtable next_table = nil;
							boolean finished = false;
							if (!apply_and_resolve_index(current, index, name_part, is_last, result, &next_table,
														  name_path, sizeof(name_path), &name_path_len,
														  resolved_name_path, resolved_bufsize,
														  error_msg, error_bufsize, &finished))
								return false;
							if (finished) return true;
							current = next_table;
						}

						component = next_component;
						depth++;
						components_consumed++;
						first_component = false;
						continue;
					}
				}
				if (error_msg && error_bufsize > 0)
					snprintf(error_msg, error_bufsize, "'%s' not found", name_part);
				return false;
			}

			/* Append name to path (with bounds checking) */
			if (!path_buf_append(name_path, sizeof(name_path), &name_path_len, name_part, error_msg, error_bufsize))
				return false;

			if (index > 0) {
				/* Has index - the named part must be a table, then index into it */
				hdlhashtable subtable = nil;
				if (!langexternalvaltotable(val, &subtable, node) || subtable == nil) {
					if (error_msg && error_bufsize > 0)
						snprintf(error_msg, error_bufsize, "'%s' is not a table (cannot index)", name_part);
					return false;
				}

				hdlhashtable next_table = nil;
				boolean finished = false;
				if (!apply_and_resolve_index(subtable, index, name_part, is_last, result, &next_table,
											  name_path, sizeof(name_path), &name_path_len,
											  resolved_name_path, resolved_bufsize,
											  error_msg, error_bufsize, &finished))
					return false;
				if (finished) return true;
				current = next_table;
			} else {
				/* No index - standard path component */
				if (is_last) {
					/* Final component - could be table or scalar */
					hdlhashtable subtable = nil;
					if (langexternalvaltotable(val, &subtable, node) && subtable != nil) {
						result->htable = subtable;
						result->is_table = true;
					} else {
						result->is_table = false;
					}
					result->val = val;
					result->hnode = node;
					if (resolved_name_path && resolved_bufsize > 0) {
						strncpy(resolved_name_path, name_path, resolved_bufsize - 1);
						resolved_name_path[resolved_bufsize - 1] = '\0';
					}
					return true;
				}

				/* Not last - must be a table to continue */
				if (!langexternalvaltotable(val, &current, node) || current == nil) {
					if (error_msg && error_bufsize > 0)
						snprintf(error_msg, error_bufsize, "'%s' is not a table", name_part);
					return false;
				}
			}
		}

		component = next_component;
		depth++;
		components_consumed++;
		first_component = false;
	}

	/* Check for depth cap: if we stopped because of depth limit but still have
	 * unconsumed components, reject as error rather than returning partial result */
	if (depth >= (size_t)COMPLETION_MAX_PATH_DEPTH && components_consumed + 1 < total_components) {
		if (error_msg && error_bufsize > 0)
			snprintf(error_msg, error_bufsize, "path too deep (limit %d components)", COMPLETION_MAX_PATH_DEPTH);
		return false;
	}

	/* Reached the end - return current table */
	result->htable = current;
	result->is_table = true;
	result->hnode = nil;
	clearbytes(&result->val, sizeof(result->val));
	if (resolved_name_path && resolved_bufsize > 0) {
		strncpy(resolved_name_path, name_path, resolved_bufsize - 1);
		resolved_name_path[resolved_bufsize - 1] = '\0';
	}
	return true;
}

/* Convenience wrapper: navigate from roottable with index syntax support. */
static boolean navigate_path_with_index(const char *path, typathlookupresult *result,
										 char *resolved_name_path, size_t resolved_bufsize,
										 char *error_msg, size_t error_bufsize) {
	return navigate_path_with_index_from(roottable, path, result,
										  resolved_name_path, resolved_bufsize,
										  error_msg, error_bufsize);
}

/* Check if a path contains [n] index syntax */
static boolean path_has_index_syntax(const char *path) {
	if (path == NULL) return false;
	return strchr(path, '[') != NULL;
}

/* Try resolving a path relative to the current table.
 * When in guest DB mode, navigates from the current table directly using the
 * index-aware resolver. Otherwise builds an absolute path and navigates from root.
 *
 * On success, sets *out_target and fills resolved_path. Returns true.
 * On failure, returns false (out_target unchanged).
 */
static boolean try_resolve_relative(const char *path_buf,
									 hdlhashtable *out_target, boolean *out_is_result,
									 typathlookupresult *result,
									 char *resolved_path, size_t path_bufsize,
									 char *error_msg, size_t error_bufsize) {
	/* Validate guest DB handle is still live before attempting relative resolution */
	if (g_repl_in_guest_db && !validate_guest_db_state())
		return false;  /* Guest DB was closed; state reset to root, skip relative */

	hdlhashtable current = repl_get_current_table();

	if (current == nil || current == roottable)
		return false;

	/* For system tables, empty path means root — skip relative resolution.
	 * For guest DBs, empty path means we're at the DB root, which is a valid
	 * starting point for relative navigation. */
	if (g_repl_current_path[0] == '\0' && !g_repl_in_guest_db)
		return false;

	if (g_repl_in_guest_db) {
		/* Navigate from current table directly (guest DB paths can't walk from roottable).
		 * Uses index-aware resolver so [n] syntax works inside guest DBs. */
		typathlookupresult local_result;
		char local_resolved[REPL_PATH_MAX_LEN] = "";
		char local_error[256] = "";

		if (navigate_path_with_index_from(current, path_buf, &local_result,
										   local_resolved, sizeof(local_resolved),
										   local_error, sizeof(local_error))) {
			if (result != NULL) {
				*result = local_result;
			}
			if (out_target != NULL && local_result.is_table) {
				*out_target = local_result.htable;
			}
			if (out_is_result != NULL) {
				*out_is_result = true;
			}
			if (resolved_path != NULL && path_bufsize > 0) {
				/* Build display path: current_path.resolved_inner_path */
				const char *inner = (local_resolved[0] != '\0') ? local_resolved : path_buf;
				if (g_repl_current_path[0] != '\0') {
					snprintf(resolved_path, path_bufsize, "%s.%s", g_repl_current_path, inner);
				} else {
					strncpy(resolved_path, inner, path_bufsize - 1);
					resolved_path[path_bufsize - 1] = '\0';
				}
			}
			return true;
		}
		return false;
	}

	/* System table: build absolute path and navigate from root */
	char abs_path[REPL_PATH_MAX_LEN];
	int wrote = snprintf(abs_path, sizeof(abs_path), "%s.%s", g_repl_current_path, path_buf);

	if (wrote <= 0 || (size_t)wrote >= sizeof(abs_path)) {
		log_debug(LOG_COMP_GENERAL, "REPL: relative path truncated, trying absolute: %s", path_buf);
		return false;
	}

	if (result != NULL) {
		/* Use index-aware navigation for full result */
		char local_error[256] = "";
		if (navigate_path_with_index(abs_path, result, resolved_path, path_bufsize,
									  local_error, sizeof(local_error))) {
			if (out_target != NULL && result->is_table) {
				*out_target = result->htable;
			}
			if (out_is_result != NULL) {
				*out_is_result = true;
			}
			return true;
		}
	} else {
		/* Simple table-only navigation */
		hdlhashtable target = completion_navigate_path(abs_path);
		if (target != nil) {
			if (out_target != NULL) *out_target = target;
			if (resolved_path != NULL && path_bufsize > 0) {
				strncpy(resolved_path, abs_path, path_bufsize - 1);
				resolved_path[path_bufsize - 1] = '\0';
			}
			return true;
		}
	}
	return false;
}

/* Extended path resolution supporting [n] index syntax and relative paths.
 *
 * Resolution order:
 *	 1. Empty path → return current focused table
 *	 2. Script expressions → delegate to repl_resolve_path()
 *	 3. Relative to current table (if focused on non-root table)
 *	 4. Single component without index → roottable lookup, then system.paths
 *	 5. Absolute path with navigate_path_with_index()
 *
 * Returns true if the path resolved successfully.
 */
boolean repl_resolve_path_ex(const char *path, typathlookupresult *result,
							  char *resolved_path, size_t path_bufsize,
							  char *error_msg, size_t error_bufsize) {
	if (result == NULL) return false;

	/* Initialize result */
	result->htable = nil;
	result->hnode = nil;
	result->is_table = false;
	clearbytes(&result->val, sizeof(result->val));

	/* Handle empty path - return current table */
	if (path == NULL || path[0] == '\0') {
		hdlhashtable current = repl_get_current_table();
		result->htable = current;
		result->is_table = true;
		if (resolved_path != NULL && path_bufsize > 0) {
			const char *cp = repl_get_current_path();
			if (cp != NULL && cp[0] != '\0') {
				strncpy(resolved_path, cp, path_bufsize - 1);
				resolved_path[path_bufsize - 1] = '\0';
			} else {
				resolved_path[0] = '\0';
			}
		}
		return true;
	}

	/* Script expressions are not supported with index syntax */
	if (path_is_script_expression(path)) {
		/* Delegate to existing repl_resolve_path for script expressions */
		hdlhashtable htable = repl_resolve_path(path, resolved_path, path_bufsize);
		if (htable != nil) {
			result->htable = htable;
			result->is_table = true;
			return true;
		}
		if (error_msg && error_bufsize > 0)
			snprintf(error_msg, error_bufsize, "script expression did not resolve to a table");
		return false;
	}

	/* Skip leading @ if present */
	const char *clean_path = path;
	if (path[0] == '@') {
		clean_path = path + 1;
	}

	/* Make a mutable copy */
	char path_buf[REPL_PATH_MAX_LEN];
	strncpy(path_buf, clean_path, REPL_PATH_MAX_LEN - 1);
	path_buf[REPL_PATH_MAX_LEN - 1] = '\0';

	/* Strip trailing dot (from tab completion) */
	size_t len = strlen(path_buf);
	if (len > 0 && path_buf[len - 1] == '.') {
		path_buf[len - 1] = '\0';
	}

	/* Check if single-component (no dots, skipping bracketed sections) */
	boolean is_single_component = true;
	for (const char *p = path_buf; *p; p++) {
		if (*p == '[') {
			while (*p && *p != ']') p++;
			if (*p == '\0') break;	/* malformed brackets; caught later by parser */
			continue;  /* p points to ']'; outer for will advance past it */
		}
		if (*p == '.') { is_single_component = false; break; }
	}

	boolean found = false;

	/* Try resolving relative to the focused table first (if not at root) */
	found = try_resolve_relative(path_buf, NULL, &found, result,
								  resolved_path, path_bufsize, NULL, 0);

	if (!found && is_single_component && !path_has_index_syntax(path_buf)) {
		/* Single component without index - try roottable, then system.paths */
		bigstring bs;
		copyctopstring(path_buf, bs);
		tyvaluerecord val;
		hdlhashnode node;

		if (roottable != nil && hashtablelookup(roottable, bs, &val, &node)) {
			hdlhashtable target = nil;
			if (langexternalvaltotable(val, &target, node) && target != nil) {
				result->htable = target;
				result->is_table = true;
				result->val = val;
				result->hnode = node;
				if (resolved_path != NULL && path_bufsize > 0) {
					strncpy(resolved_path, path_buf, path_bufsize - 1);
					resolved_path[path_bufsize - 1] = '\0';
				}
				found = true;
			}
		}

		if (!found) {
			char rp[REPL_PATH_MAX_LEN] = "";
			hdlhashtable target = completion_search_paths_ex(path_buf, rp, sizeof(rp),
															  NULL, NULL, 0);
			if (target != nil) {
				result->htable = target;
				result->is_table = true;
				if (resolved_path != NULL && path_bufsize > 0) {
					strncpy(resolved_path, rp, path_bufsize - 1);
					resolved_path[path_bufsize - 1] = '\0';
				}
				found = true;
			}
		}
	}

	if (!found) {
		/* Try as absolute path (with or without index syntax) */
		found = navigate_path_with_index(path_buf, result, resolved_path, path_bufsize,
										  error_msg, error_bufsize);
	}

	return found;
}

/* Set the current REPL table by navigating to a path.
 * Returns true on success, false if path is invalid.
 * Supports:
 *	 - Empty path or "@" to go to root
 *	 - ".." to go to parent
 *	 - Dot-paths like "system.verbs"
 *	 - [n] index syntax like "system.verbs[1]" (1-based)
 *	 - Script expressions like "parentOf(@user.inetd)" if path contains ( ) or +
 *	 - Trailing dots are stripped (from tab completion)
 */
boolean repl_jump_path(const char *path) {
	/* Handle empty path, "@", "root", or "@root" - go to root */
	if (path == NULL || path[0] == '\0' ||
		(path[0] == '@' && path[1] == '\0') ||
		strcasecmp(path, "root") == 0 ||
		strcasecmp(path, "@root") == 0) {
		g_repl_current_table = roottable;
		g_repl_current_path[0] = '\0';
		clear_guest_db_state();
		update_prompt();
		repl_set_focus(roottable);
		return true;
	}

	/* Check if this looks like a script expression */
	if (path_is_script_expression(path)) {
		return repl_jump_script(path);
	}

	/* Skip leading @ if present */
	const char *clean_path = path;
	if (path[0] == '@') {
		clean_path = path + 1;
	}

	/* Make a mutable copy */
	char path_buf[REPL_PATH_MAX_LEN];
	strncpy(path_buf, clean_path, REPL_PATH_MAX_LEN - 1);
	path_buf[REPL_PATH_MAX_LEN - 1] = '\0';

	/* Handle ".." - go to parent (before stripping trailing dot) */
	if (strcmp(path_buf, "..") == 0) {
		if (g_repl_current_path[0] == '\0') {
			/* Already at root, stay at root */
			return true;
		}

		if (g_repl_in_guest_db) {
			/* Validate handle before navigating within guest DB */
			if (!validate_guest_db_state()) {
				update_prompt();
				return true;  /* State was reset to root */
			}
			if (g_repl_current_path[0] == '\0') {
				/* At guest DB root level — exit guest DB, go to system root */
				clear_guest_db_state();
				g_repl_current_table = roottable;
			} else {
				char *last_dot = strrchr(g_repl_current_path, '.');
				if (last_dot == NULL) {
					/* At top-level entry in guest DB — go up to guest DB root */
					g_repl_current_table = g_repl_guest_db_root;
					g_repl_current_path[0] = '\0';
				} else {
					/* Go up one level within guest DB */
					*last_dot = '\0';
					g_repl_current_table = completion_navigate_dotpath_from(g_repl_guest_db_root, g_repl_current_path);
					if (g_repl_current_table == nil) {
						/* Shouldn't happen, but handle gracefully */
						clear_guest_db_state();
						g_repl_current_table = roottable;
						g_repl_current_path[0] = '\0';
					}
				}
			}
		} else {
			/* Find last dot in current path */
			char *last_dot = strrchr(g_repl_current_path, '.');
			if (last_dot == NULL) {
				/* No dot means we're one level deep - go to root */
				g_repl_current_table = roottable;
				g_repl_current_path[0] = '\0';
			} else {
				/* Truncate at the last dot to get parent path */
				*last_dot = '\0';
				/* Navigate to the parent path */
				hdlhashtable parent = completion_navigate_path(g_repl_current_path);
				if (parent == nil) {
					/* Shouldn't happen, but handle gracefully */
					g_repl_current_table = roottable;
					g_repl_current_path[0] = '\0';
				} else {
					g_repl_current_table = parent;
				}
			}
		}
		update_prompt();
		repl_set_focus(g_repl_current_table);
		return true;
	}

	/* Strip trailing dot (from tab completion) for regular paths */
	size_t len = strlen(path_buf);
	if (len > 0 && path_buf[len - 1] == '.') {
		path_buf[len - 1] = '\0';
	}

	/* If path has index syntax, delegate to repl_resolve_path_ex()
	 * which handles relative paths, system.paths, and index navigation */
	if (path_has_index_syntax(path_buf)) {
		typathlookupresult result;
		char resolved_path[REPL_PATH_MAX_LEN] = "";
		char error_msg[256] = "";

		if (!repl_resolve_path_ex(path_buf, &result, resolved_path, sizeof(resolved_path),
								   error_msg, sizeof(error_msg))) {
			if (error_msg[0] != '\0') {
				/* error_msg interpolates user-supplied path
				 * components — scrub control bytes before print.
				 * See safe_print_user_string for threat model. */
				fputs("Error: ", stdout);
				safe_print_user_string(error_msg);
				fputc('\n', stdout);
			}
			return false;
		}

		if (!result.is_table) {
			printf("Error: cannot jump to a non-table value\n");
			return false;
		}

		g_repl_current_table = result.htable;
		strncpy(g_repl_current_path, resolved_path, REPL_PATH_MAX_LEN - 1);
		g_repl_current_path[REPL_PATH_MAX_LEN - 1] = '\0';
		clear_guest_db_state();
		update_prompt();
		repl_set_focus(result.htable);
		return true;
	}

	/* Check if this is a single-component path (no dots) */
	boolean is_single_component = (strchr(path_buf, '.') == NULL);

	hdlhashtable target = nil;
	char resolved_path[REPL_PATH_MAX_LEN] = "";

	/* Try resolving relative to the focused table first (if not at root) */
	try_resolve_relative(path_buf, &target, NULL, NULL,
						  resolved_path, sizeof(resolved_path), NULL, 0);

	if (target == nil && is_single_component) {
		/* Single component - try roottable, then system.paths (including guest DBs) */
		bigstring bs;
		copyctopstring(path_buf, bs);
		tyvaluerecord val;
		hdlhashnode node;

		if (roottable != nil && hashtablelookup(roottable, bs, &val, &node)) {
			if (!langexternalvaltotable(val, &target, node)) {
				target = nil;
			} else {
				strncpy(resolved_path, path_buf, REPL_PATH_MAX_LEN - 1);
				resolved_path[REPL_PATH_MAX_LEN - 1] = '\0';
				clear_guest_db_state();
			}
		}

		if (target == nil) {
			hdlhashtable guest_root = nil;
			char guest_name[REPL_PATH_MAX_LEN] = "";
			target = completion_search_paths_ex(path_buf, resolved_path, sizeof(resolved_path),
												 &guest_root, guest_name, sizeof(guest_name));
			if (target != nil && guest_root != nil) {
				/* Entering a guest database */
				g_repl_in_guest_db = true;
				g_repl_guest_db_root = guest_root;
				strncpy(g_repl_guest_db_name, guest_name, REPL_PATH_MAX_LEN - 1);
				g_repl_guest_db_name[REPL_PATH_MAX_LEN - 1] = '\0';
			} else if (target != nil) {
				/* System path match, not guest DB */
				clear_guest_db_state();
			}
		}
	} else if (target == nil) {
		/* Multi-component path - try absolute navigation from root */
		target = completion_navigate_path(path_buf);
		if (target != nil) {
			strncpy(resolved_path, path_buf, REPL_PATH_MAX_LEN - 1);
			resolved_path[REPL_PATH_MAX_LEN - 1] = '\0';
			clear_guest_db_state();
		}
	}

	/* If still not found, check if path matches a guest DB display name
	 * (e.g., "/jump mainResponder.root" → jump to that DB's root table) */
	if (target == nil && filewindowtable != nil) {
		hdlhashnode fwnomad;
		for (fwnomad = (**filewindowtable).hfirstsort; fwnomad != nil; fwnomad = (**fwnomad).sortedlink) {
			/* Extract display name from filewindowtable key (full filesystem path) */
			bigstring bskey;
			gethashkey(fwnomad, bskey);
			char fullpath[COMPLETION_MAX_NAME_LEN];
			size_t keylen = stringlength(bskey);
			if (keylen >= COMPLETION_MAX_NAME_LEN) keylen = COMPLETION_MAX_NAME_LEN - 1;
			memcpy(fullpath, stringbaseaddress(bskey), keylen);
			fullpath[keylen] = '\0';

			const char *slash = strrchr(fullpath, '/');
			const char *bslash = strrchr(fullpath, '\\');
			const char *dbname;
			if (slash != NULL && bslash != NULL)
				dbname = (bslash > slash ? bslash : slash) + 1;
			else if (slash != NULL)
				dbname = slash + 1;
			else if (bslash != NULL)
				dbname = bslash + 1;
			else
				dbname = fullpath;

			if (strcmp(path_buf, dbname) == 0) {
				hdlhashtable db_root;
				if (langexternalvaltotable((**fwnomad).val, &db_root, fwnomad) && db_root != nil) {
					/* Jump to guest DB root level */
					g_repl_in_guest_db = true;
					g_repl_guest_db_root = db_root;
					strncpy(g_repl_guest_db_name, dbname, REPL_PATH_MAX_LEN - 1);
					g_repl_guest_db_name[REPL_PATH_MAX_LEN - 1] = '\0';
					g_repl_current_table = db_root;
					g_repl_current_path[0] = '\0';
					update_prompt();
					repl_set_focus(db_root);
					return true;
				}
			}
		}
	}

	if (target == nil) {
		return false;
	}

	/* Update state with the resolved path */
	g_repl_current_table = target;
	strncpy(g_repl_current_path, resolved_path, REPL_PATH_MAX_LEN - 1);
	g_repl_current_path[REPL_PATH_MAX_LEN - 1] = '\0';
	update_prompt();
	repl_set_focus(target);
	return true;
}

/* Resolve a path to a table without changing the current REPL table.
 * Supports the same path formats as repl_jump_path():
 *	 - Dot-paths like "system.verbs"
 *	 - Single names resolved via system.paths (e.g., "fileMenu")
 *	 - Script expressions like "parentOf(@user.inetd)"
 *	 - Addresses with leading @ like "@user.prefs"
 *
 * If resolved_path is non-NULL, fills it with the actual resolved path
 * (e.g., "system.verbs.builtins" for "parentOf(fileMenu)").
 *
 * Returns the resolved table, or nil if path is invalid.
 */
hdlhashtable repl_resolve_path(const char *path, char *resolved_path, size_t path_bufsize) {
	/* Handle empty path - return current table */
	if (path == NULL || path[0] == '\0') {
		if (resolved_path != NULL && path_bufsize > 0) {
			const char *current = repl_get_current_path();
			if (current != NULL && current[0] != '\0') {
				strncpy(resolved_path, current, path_bufsize - 1);
				resolved_path[path_bufsize - 1] = '\0';
			} else {
				resolved_path[0] = '\0';
			}
		}
		return repl_get_current_table();
	}

	/* Check if this looks like a script expression */
	if (path_is_script_expression(path)) {
		/* Evaluate script and extract table from result */
		size_t script_len = strlen(path);
		Handle htext = nil;

		if (!newemptyhandle(&htext)) {
			return nil;
		}
		if (!sethandlesize(htext, (long)script_len)) {
			disposehandle(htext);
			return nil;
		}
		HLock(htext);
		memcpy(*htext, path, script_len);
		HUnlock(htext);

		tyvaluerecord val;
		boolean flpushpop = !flscriptrunning;

		if (flpushpop)
			flpushpop = pushprocess(nil);

		boolean fl = langrun(htext, &val);

		if (flpushpop)
			popprocess();

		if (!fl) {
			return nil;
		}

		/* Extract table from result */
		hdlhashtable result = nil;

		if (val.valuetype == addressvaluetype) {
			hdlhashtable htable = nil;
			bigstring bsname;

			if (getaddressvalue(val, &htable, bsname)) {
				tyvaluerecord targetval;
				hdlhashnode hnode;

				if (hashtablelookup(htable, bsname, &targetval, &hnode)) {
					langexternalvaltotable(targetval, &result, hnode);
				}

				/* Get the resolved path from the address */
				if (resolved_path != NULL && path_bufsize > 0) {
					bigstring bspath;
					if (getaddresspath(val, bspath)) {
						const char *pathstart = (const char *)stringbaseaddress(bspath);
						size_t pathlen = stringlength(bspath);
						/* Skip leading @ if present */
						if (pathlen > 0 && pathstart[0] == '@') {
							pathstart++;
							pathlen--;
						}
						/* Skip "root." prefix if present */
						if (pathlen > 5 && strncmp(pathstart, "root.", 5) == 0) {
							pathstart += 5;
							pathlen -= 5;
						}
						if (pathlen >= path_bufsize) {
							pathlen = path_bufsize - 1;
						}
						memcpy(resolved_path, pathstart, pathlen);
						resolved_path[pathlen] = '\0';
					} else {
						/* Fallback to input path */
						strncpy(resolved_path, path, path_bufsize - 1);
						resolved_path[path_bufsize - 1] = '\0';
					}
				}
			}
		} else if (val.valuetype == externalvaluetype) {
			langexternalvaltotable(val, &result, nil);
			/* For direct external values, we can't easily get the path */
			if (resolved_path != NULL && path_bufsize > 0) {
				strncpy(resolved_path, path, path_bufsize - 1);
				resolved_path[path_bufsize - 1] = '\0';
			}
		}

		return result;
	}

	/* Skip leading @ if present */
	const char *clean_path = path;
	if (path[0] == '@') {
		clean_path = path + 1;
	}

	/* Make a mutable copy */
	char path_buf[REPL_PATH_MAX_LEN];
	strncpy(path_buf, clean_path, REPL_PATH_MAX_LEN - 1);
	path_buf[REPL_PATH_MAX_LEN - 1] = '\0';

	/* Strip trailing dot (from tab completion) */
	size_t len = strlen(path_buf);
	if (len > 0 && path_buf[len - 1] == '.') {
		path_buf[len - 1] = '\0';
	}

	/* Check if this is a single-component path (no dots) */
	boolean is_single_component = (strchr(path_buf, '.') == NULL);

	hdlhashtable target = nil;

	/* Try resolving relative to the focused table first (if not at root) */
	(void)try_resolve_relative(path_buf, &target, NULL, NULL,
								resolved_path, path_bufsize, NULL, 0);

	if (target == nil && is_single_component) {
		/* Single component - try roottable first, then system.paths */
		bigstring bs;
		copyctopstring(path_buf, bs);
		tyvaluerecord val;
		hdlhashnode node;

		if (roottable != nil && hashtablelookup(roottable, bs, &val, &node)) {
			langexternalvaltotable(val, &target, node);
			if (target != nil && resolved_path != NULL && path_bufsize > 0) {
				strncpy(resolved_path, path_buf, path_bufsize - 1);
				resolved_path[path_bufsize - 1] = '\0';
			}
		}

		if (target == nil) {
			/* Try system.paths - this gives us the resolved path */
			target = completion_search_paths_ex(path_buf, resolved_path, path_bufsize,
												 NULL, NULL, 0);
		}
	} else if (target == nil) {
		/* Multi-component path - use standard navigation */
		target = completion_navigate_path(path_buf);
		if (target != nil && resolved_path != NULL && path_bufsize > 0) {
			strncpy(resolved_path, path_buf, path_bufsize - 1);
			resolved_path[path_bufsize - 1] = '\0';
		}
	}

	return target;
}

// Global interrupt flag (signal-safe)
// Declared as non-static so lang.c can check it for script interruption
volatile sig_atomic_t g_repl_interrupt_requested = 0;

/* g_script_running lives at the top of the file (near g_repl_exit_requested)
 * so its readers and writers can be hoisted ahead of this point without
 * forward-declaration churn. See the comment block on the declaration. */

/* SIGWINCH (window-size change) flag.
 *
 * Set by sigwinch_handler() and consumed by repl_sigwinch_consumed().  The
 * flag exists so that the upcoming slash-menu palette renderer (PR 5) and
 * other render-tick consumers can react to terminal resize without each
 * needing to install their own sigaction.
 *
 * Coexists with file_browser.c's local SIGWINCH handler — file_browser saves
 * and restores the previous sigaction around its modal session, so this
 * REPL-level handler is the long-lived one. */
static volatile sig_atomic_t g_sigwinch = 0;

// Global linenoise state for terminal cleanup and async output
static struct linenoiseState *g_active_linenoisestate = NULL;

// Forward declaration for completion callback
static void linenoise_completion_callback(const char *buf, linenoiseCompletions *lc);

/* linenoiseEditInsert is declared in third_party/linenoise/linenoise.h
 * (Frontier modification). Used by the slash-menu disambiguator below
 * to re-inject a follow-up byte that we consumed from stdin to decide
 * whether to open the menu — see the comment block at the call site
 * (search for "slash-menu disambiguator"). */

/*
 * Async-signal-safe mouse-tracking disable.
 *
 * terminal_disable_mouse() uses fputs/fflush, which are NOT async-signal-safe
 * (POSIX.1-2017 §2.4.3).  When a signal handler needs to leave mouse mode
 * before _exit(), it must talk to the kernel directly via write(2), which is
 * on the AS-safe list.
 *
 * The byte sequence matches terminal_disable_mouse() exactly: 16 bytes
 * "ESC [ ? 1 0 0 0 l ESC [ ? 1 0 0 6 l".  Kept in sync by the matching
 * unit tests.
 */
static void async_signal_safe_disable_mouse(void) {
	static const char DISABLE_SEQ[] = "\x1b[?1000l\x1b[?1006l";
	/* Best effort — if write() fails (closed fd, EINTR), there is nothing
	 * sensible a signal handler can do about it.  Ignore the result
	 * deliberately to avoid a noisy compile warning. */
	ssize_t r = write(STDERR_FILENO, DISABLE_SEQ, sizeof(DISABLE_SEQ) - 1);
	(void)r;
}

/* Signal handler for SIGINT (Ctrl-C) - must be async-signal-safe.
 *
 * The interrupt flag is consumed in user context by handle_interrupt(); the
 * one new responsibility here is to leave the terminal in a sane mouse mode
 * if the palette had enabled tracking.  Calling write(2) directly keeps this
 * AS-safe even though terminal_disable_mouse() itself is not. */
static void sigint_handler(int sig) {
	(void)sig;
	async_signal_safe_disable_mouse();
	g_repl_interrupt_requested = 1;
}

/* Signal handler for SIGTERM - exit immediately
 * NOTE: We don't call linenoiseEditStop() here because it's not async-signal-safe.
 * Terminal mode is automatically restored by the OS when the process exits.
 * Mouse tracking, however, is NOT restored by the OS — it's a remote terminal
 * mode set by escape sequence.  We emit the disable sequence inline here
 * (write(2) is async-signal-safe) before _exit so the user does not get left
 * with a terminal that streams mouse coordinates as garbage characters.
 * Using _exit() to avoid calling atexit handlers from signal context (unsafe). */
static void sigterm_handler(int sig) {
	(void)sig;
	async_signal_safe_disable_mouse();
	_exit(0);
}

/* SIGWINCH handler — terminal window resized.
 *
 * Only the flag set; consumers poll repl_sigwinch_consumed() in user context.
 * Coexists with file_browser.c's bounded-scope handler. */
static void sigwinch_handler(int sig) {
	(void)sig;
	g_sigwinch = 1;
}

/* Terminal cleanup for atexit() */
static void cleanup_terminal(void) {
	if (g_active_linenoisestate) {
		linenoiseEditStop(g_active_linenoisestate);
		g_active_linenoisestate = NULL;
	}
}

/* Install signal handlers for Ctrl-C, SIGTERM, and SIGWINCH */
static void install_signal_handlers(void) {
	struct sigaction sa;

	// SIGINT handler (Ctrl-C)
	sa.sa_handler = sigint_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

	// SIGTERM handler (for clean shutdown)
	sa.sa_handler = sigterm_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, NULL);

	// SIGWINCH handler (terminal resize) — used by the future slash-menu
	// palette renderer (PR 5).  SA_RESTART so blocking syscalls (read on
	// stdin, etc.) auto-resume after the signal is delivered.
	sa.sa_handler = sigwinch_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGWINCH, &sa, NULL);

	// Register terminal cleanup for atexit
	atexit(cleanup_terminal);
}

/* Public consumer for the SIGWINCH flag — see repl.h. */
boolean repl_sigwinch_consumed(void) {
	if (g_sigwinch) {
		g_sigwinch = 0;
		return true;
	}
	return false;
}

/* Guard to prevent re-entrant interrupt handling */
static volatile sig_atomic_t g_handling_interrupt = 0;

/* Handle interrupt in event loop */
static void handle_interrupt(struct linenoiseState *ls, char *buf, size_t buflen) {
	/* Prevent double-handling if user mashes Ctrl-C rapidly */
	if (g_handling_interrupt) {
		g_repl_interrupt_requested = 0;
		return;
	}
	g_handling_interrupt = 1;
	g_repl_interrupt_requested = 0;

	if (g_script_running) {
		// Script is running - set interrupt flag for interpreter to check
		// The interpreter's background task handler will see this
		printf("\n^C (interrupting script...)\n");
		fflush(stdout);
	} else {
		// At prompt - clear line and redisplay
		linenoiseEditStop(ls);
		printf("^C\n");
		fflush(stdout);
		// Restart with fresh prompt using caller's buffer (not ls->buf which
		// may be invalid after linenoiseEditStop)
		if (linenoiseEditStart(ls, STDIN_FILENO, STDOUT_FILENO,
							  buf, buflen, g_repl_prompt) == -1) {
			// Failed to restart - log error and set flag for main loop to exit
			log_error(LOG_COMP_GENERAL, "Failed to restart linenoise after interrupt");
			// Note: Main loop will exit on next iteration since ls is invalid
		}
	}

	g_handling_interrupt = 0;
}

/* Adds a command to the session tracking list for history merge-before-save. */
static void track_session_command(const char *cmd) {
	if (session_command_count >= MAX_SESSION_COMMANDS) {
		// Shift out oldest command
		free(session_commands[0]);
		memmove(session_commands, session_commands + 1,
				(MAX_SESSION_COMMANDS - 1) * sizeof(char *));
		session_command_count = MAX_SESSION_COMMANDS - 1;
	}
	session_commands[session_command_count] = strdup(cmd);
	if (session_commands[session_command_count]) {
		session_command_count++;
	}
}

/* Frees all tracked session commands and resets the counter. */
static void free_session_commands(void) {
	for (size_t i = 0; i < session_command_count; i++) {
		free(session_commands[i]);
		session_commands[i] = NULL;
	}
	session_command_count = 0;
}

/* Merges session commands with existing history file, deduplicating and trimming to HISTORY_SIZE. */
static void merge_and_save_history(const char *history_path) {
	// Read existing history file
	char **file_commands = NULL;
	size_t file_count = 0;
	size_t file_capacity = 0;

	FILE *f = fopen(history_path, "r");
	if (f) {
		char line[MAX_COMMAND_LEN];
		while (fgets(line, sizeof(line), f)) {
			// Remove trailing newline
			size_t len = strlen(line);
			if (len > 0 && line[len - 1] == '\n') {
				line[len - 1] = '\0';
				len--;
			}
			if (len == 0) continue;

			// Grow array if needed
			if (file_count >= file_capacity) {
				file_capacity = file_capacity ? file_capacity * 2 : 256;
				char **new_commands = realloc(file_commands, file_capacity * sizeof(char *));
				if (!new_commands) break;
				file_commands = new_commands;
			}

			file_commands[file_count] = strdup(line);
			if (file_commands[file_count]) {
				file_count++;
			}
		}
		fclose(f);
	}

	// Merge: file commands first, then session commands
	// Deduplicate by keeping last occurrence of each command
	char **merged = malloc((file_count + session_command_count) * sizeof(char *));
	size_t merged_count = 0;

	if (!merged) {
		// Malloc failed - cleanup file_commands entries to avoid memory leak
		for (size_t i = 0; i < file_count; i++) {
			free(file_commands[i]);
		}
		free(file_commands);
		return;
	}

	// Add file commands (skip if duplicate of session command)
	for (size_t i = 0; i < file_count; i++) {
		int is_dup = 0;
		for (size_t j = 0; j < session_command_count; j++) {
			if (strcmp(file_commands[i], session_commands[j]) == 0) {
				is_dup = 1;
				break;
			}
		}
		if (!is_dup) {
			merged[merged_count++] = file_commands[i];
		} else {
			free(file_commands[i]);
		}
	}

	// Add all session commands (they're the most recent)
	for (size_t i = 0; i < session_command_count; i++) {
		merged[merged_count++] = session_commands[i];
	}

	// Trim to HISTORY_SIZE (keep most recent)
	size_t start = 0;
	if (merged_count > HISTORY_SIZE) {
		start = merged_count - HISTORY_SIZE;
		for (size_t i = 0; i < start; i++) {
			free(merged[i]);
		}
	}

	// Write merged history
	f = fopen(history_path, "w");
	if (f) {
		for (size_t i = start; i < merged_count; i++) {
			fprintf(f, "%s\n", merged[i]);
		}
		fclose(f);
	}

	// Free merged entries (session_commands are now owned by merged)
	for (size_t i = start; i < merged_count; i++) {
		free(merged[i]);
	}
	free(merged);

	// Clear session tracking - NULL pointers to prevent double-free
	for (size_t i = 0; i < session_command_count; i++) {
		session_commands[i] = NULL;
	}
	session_command_count = 0;

	// Free file_commands array (entries already freed or moved to merged)
	free(file_commands);
}

/* Initializes linenoise with history, tab completion, and multi-line mode. */
static boolean init_linenoise(void) {
	// Set history size
	linenoiseHistorySetMaxLen(HISTORY_SIZE);

	// Load history from file
	const char *home = getenv("HOME");
	if (home) {
		char history_path[1024];
		int written = snprintf(history_path, sizeof(history_path), "%s/%s", home, HISTORY_FILE);
		if (written >= (int)sizeof(history_path)) {
			log_warn(LOG_COMP_GENERAL, "HOME path too long, history disabled");
		} else {
			linenoiseHistoryLoad(history_path);
		}
	}

	// Set up tab completion callback
	linenoiseSetCompletionCallback(linenoise_completion_callback);

	// Initialize completion engine
	if (!completion_init()) {
		log_warn(LOG_COMP_GENERAL, "Tab completion initialization failed");
		// Continue without completion - not fatal
	}

	// Enable multi-line mode for long scripts
	linenoiseSetMultiLine(1);

	return true;
}

/* Cleans up linenoise resources and saves merged history to disk. */
static void cleanup_linenoise(void) {
	// Cleanup completion engine
	completion_cleanup();

	// Merge and save history (supports concurrent sessions)
	const char *home = getenv("HOME");
	if (home) {
		char history_path[1024];
		int written = snprintf(history_path, sizeof(history_path), "%s/%s", home, HISTORY_FILE);
		if (written >= (int)sizeof(history_path)) {
			log_warn(LOG_COMP_GENERAL, "HOME path too long, history not saved");
		} else {
			merge_and_save_history(history_path);
		}
	}

	// Free any remaining session commands
	free_session_commands();
}

/* List of REPL slash commands for tab completion */
static const char *repl_slash_commands[] = {
	"exit",
	"jump",
	"help",
	"keycodes",
	"list",
	NULL
};

/*
 * Helper: Complete a path argument for slash commands like /jump and /list.
 * Takes the full buffer, the offset where the path starts, and adds completions.
 * Only includes tables (navigable items) in the results.
 *
 * Search order for single-component paths:
 * 1. Current focused table (g_repl_current_table) - highest priority
 * 2. Root table entries
 * 3. system.paths entries - lowest priority
 *
 * This allows `/jump inetd` to find user.inetd when focused on user table,
 * while still falling back to system.paths if not found locally.
 */
static void complete_slash_command_path(const char *buf, size_t buf_len,
										size_t path_offset, linenoiseCompletions *lc) {
	const char *path_start = buf + path_offset;

	// Skip leading @ if present
	if (*path_start == '@') {
		path_start++;
	}

	// Parse as a dotted path for completion
	completion_context_t ctx;
	completion_parse_context(path_start, (int)strlen(path_start), &ctx);

	// Collect matches
	completion_matches_t matches;
	completion_matches_init(&matches);

	if (ctx.has_dot) {
		// Dotted path - navigate from root or current table
		// First try relative to current table
		hdlhashtable current = repl_get_current_table();
		if (current != nil && current != roottable) {
			hdlhashtable target = nil;
			// Look up first component in current table
			char path_copy[COMPLETION_MAX_NAME_LEN];
			strncpy(path_copy, ctx.table_path, COMPLETION_MAX_NAME_LEN - 1);
			path_copy[COMPLETION_MAX_NAME_LEN - 1] = '\0';

			char *first_dot = strchr(path_copy, '.');
			char *first_component = path_copy;
			if (first_dot) {
				*first_dot = '\0';
			}

			bigstring bs;
			copyctopstring(first_component, bs);
			tyvaluerecord val;
			hdlhashnode node;
			if (hashtablelookup(current, bs, &val, &node)) {
				// Found in current table - navigate from there
				if (first_dot) {
					// More components - need to navigate
					hdlhashtable first_table;
					if (langexternalvaltotable(val, &first_table, node) && first_table != nil) {
						target = completion_navigate_path(first_dot + 1);
						// If that fails, try full path from first_table
						if (target == nil) {
							// Restore and try navigating the rest
							char rest[COMPLETION_MAX_NAME_LEN];
							strncpy(rest, ctx.table_path + (first_dot - path_copy) + 1, COMPLETION_MAX_NAME_LEN - 1);
							rest[COMPLETION_MAX_NAME_LEN - 1] = '\0';
							// Navigate rest from first_table - simplified for now
						}
					}
				} else {
					// Single component that's a table
					langexternalvaltotable(val, &target, node);
				}
			}
			if (target != nil) {
				completion_add_table_entries(&matches, target, ctx.leaf_prefix);
			}
		}

		// Also try absolute path from root
		hdlhashtable target = completion_navigate_path(ctx.table_path);
		if (target != nil) {
			completion_add_table_entries(&matches, target, ctx.leaf_prefix);
		}
	} else {
		// Single-component path - search in priority order:
		// 1. Current focused table (if not root)
		hdlhashtable current = repl_get_current_table();
		if (current != nil && current != roottable) {
			completion_add_table_entries(&matches, current, ctx.leaf_prefix);
		}

		// 2. Root table entries
		completion_add_table_entries(&matches, roottable, ctx.leaf_prefix);

		// 3. system.paths entries
		completion_add_path_entries(&matches, ctx.leaf_prefix);
	}

	// Add matches - only include tables (navigable items)
	for (size_t i = 0; i < matches.count; i++) {
		if (matches.items[i].is_table) {
			char completion[1024];
			size_t leaf_len = strlen(ctx.leaf_prefix);
			size_t prefix_len = buf_len - leaf_len;

			// Copy everything before the leaf
			if (prefix_len > sizeof(completion) - 1) {
				prefix_len = sizeof(completion) - 1;
			}
			memcpy(completion, buf, prefix_len);
			completion[prefix_len] = '\0';

			// Append the match with trailing dot
			size_t remaining = sizeof(completion) - prefix_len - 1;
			strncat(completion, matches.items[i].name, remaining);
			remaining = sizeof(completion) - strlen(completion) - 1;
			strncat(completion, ".", remaining);

			linenoiseAddCompletion(lc, completion);
		}
	}
}

/* Bridges linenoise tab completion to the Frontier completion engine. */
static void linenoise_completion_callback(const char *buf, linenoiseCompletions *lc) {
	size_t buf_len = strlen(buf);

	// Handle slash command completion
	if (buf_len > 0 && buf[0] == '/') {
		// Check if this is "/jump <path>" or "/list <path>" - complete the path argument
		if (strncasecmp(buf, "/jump ", 6) == 0) {
			complete_slash_command_path(buf, buf_len, 6, lc);
			return;
		}
		if (strncasecmp(buf, "/list ", 6) == 0) {
			complete_slash_command_path(buf, buf_len, 6, lc);
			return;
		}

		// Regular slash command completion
		const char *cmd_prefix = buf + 1;  // Skip the '/'
		size_t prefix_len = buf_len - 1;

		for (int i = 0; repl_slash_commands[i] != NULL; i++) {
			const char *cmd = repl_slash_commands[i];
			if (strncasecmp(cmd, cmd_prefix, prefix_len) == 0) {
				// Build completion: "/" + command + " "
				char completion[256];
				snprintf(completion, sizeof(completion), "/%s ", cmd);
				linenoiseAddCompletion(lc, completion);
			}
		}
		return;	 // Don't do regular completion for slash commands
	}

	// Parse completion context
	completion_context_t ctx;
	int cursor_pos = (int)buf_len;	// Linenoise doesn't expose cursor position, assume end of line
	completion_parse_context(buf, cursor_pos, &ctx);

	// No completion inside strings
	if (ctx.ctx_type == COMPLETION_CTX_STRING) {
		return;
	}

	// Collect matches
	completion_matches_t matches;
	completion_matches_init(&matches);

	if (ctx.has_dot) {
		// Phase 3: Dotted path completion
		hdlhashtable target = completion_navigate_path(ctx.table_path);
		if (target != nil) {
			completion_add_table_entries(&matches, target, ctx.leaf_prefix);
		}
	} else if (ctx.ctx_type == COMPLETION_CTX_ADDRESS) {
		// Phase 4: Address completion - search from roottable
		completion_add_table_entries(&matches, roottable, ctx.token);
	} else if (ctx.ctx_type == COMPLETION_CTX_VERB_CALL) {
		// Phase 4: Verb processor context
		hdlhashtable target = completion_navigate_path(ctx.table_path);
		if (target != nil) {
			completion_add_table_entries(&matches, target, ctx.leaf_prefix);
		}
	} else {
		// Phase 1 & 2: General completion
		completion_add_keywords(&matches, ctx.token);

		if (roottable != nil) {
			completion_add_table_entries(&matches, roottable, ctx.token);
		}
		if (systemtable != nil) {
			completion_add_table_entries(&matches, systemtable, ctx.token);
		}

		// Phase 2.5: Add path-accessible names (fileMenu, new, etc.)
		completion_add_path_entries(&matches, ctx.token);
	}

	// Add matches to linenoise
	for (size_t i = 0; i < matches.count; i++) {
		// Build full completion string (prefix + match)
		char completion[1024];
		size_t buf_len = strlen(buf);
		size_t leaf_len = strlen(ctx.leaf_prefix);

		// Guard against integer underflow if leaf_prefix longer than buf
		if (leaf_len > buf_len) {
			continue;
		}
		size_t prefix_len = buf_len - leaf_len;

		// Copy the prefix part of the buffer
		if (prefix_len > sizeof(completion) - 1) {
			prefix_len = sizeof(completion) - 1;
		}
		memcpy(completion, buf, prefix_len);
		completion[prefix_len] = '\0';

		// Append the match
		size_t remaining = sizeof(completion) - prefix_len - 1;
		strncat(completion, matches.items[i].name, remaining);

		// Add trailing character based on type
		if (matches.items[i].is_table) {
			// Recalculate remaining space after first strncat
			size_t current_len = strlen(completion);
			size_t space_left = sizeof(completion) - current_len - 1;
			if (space_left > 0) {
				strncat(completion, ".", space_left);
			}
		}

		linenoiseAddCompletion(lc, completion);
	}
}

/* ------------------------------------------------------------------- */
/*  Slash-command dispatcher (PR 7 — replaces repl_commands.c)         */
/* ------------------------------------------------------------------- */
/*
 * Routes a line that begins with '/' through the installed REPL menubar
 * (system.menus.data.repl.REPL) instead of the legacy hardcoded
 * if/strcmp chain. The hardcoded chain lived in repl_commands.c; PR 7
 * deletes that file in favor of this dispatch path.
 *
 * Flow:
 *   1. Strip leading '/'. Trim leading and trailing whitespace from the
 *      remaining text.
 *   2. Split on the first run of whitespace into TOKEN and ARGS. ARGS may
 *      be empty.
 *   3. Resolve TOKEN against the menubar via repl_resolve_slash_command.
 *      The resolver is case-insensitive, internally-whitespace-insensitive,
 *      and supports unique prefix + single-letter first-letter matching.
 *      It returns the matched leaf hashtable AND the leaf's slot key.
 *   4. Dispatch:
 *
 *        - <leaf> + ARGS, leaf.accepts_args=true:
 *              Synthesize "<handler> (\"escaped-arg\")" via
 *              palette_arg_inject_into_script and dispatch the result
 *              via meuserselected_headless. The arg is bound at compile
 *              time inside the handler's call expression — no global
 *              hand-off (issue #594 collapsed the previous hybrid path).
 *        - <leaf> + ARGS, leaf.accepts_args=false:
 *              Dispatch via meuserselected_headless without the arg —
 *              the argument is silently ignored, matching legacy
 *              behavior for items that don't expect typed input.
 *        - <leaf> + "":
 *              Dispatch via meuserselected_headless with the leaf's
 *              stored script as-is.
 *
 *   5. Unresolved tokens emit "Unknown command: /<token>\nType /help for
 *      available commands\n" and continue. Empty token "/" emits the
 *      same with a literal "/" rather than crashing.
 *
 * The List and Jump leaves in the standard installed menubar advertise
 * accepts_args=true (set by installReplMenubar.ut), so typed args reach
 * the system.menus.handlers.repl.list / .jump handlers as a string
 * parameter. Those handlers forward to repl.list / repl.jumpPath. There
 * is no slot-key special case for List or Jump in this dispatcher.
 *
 * Returns true if the line was consumed (always — even unknown commands
 * are "consumed" in the sense that the REPL keeps running). Sets
 * *running to false only when the dispatched command was /exit.
 *
 * GIL: must be called while holding the GIL — calls into the resolver,
 * menudata_describe_leaf, and meuserselected_headless, all of which
 * require it.
 */

/* Forward declarations of the host adapter functions defined later in
 * this file. They wrap the same module-state mutations that the menubar
 * handlers ultimately reach through the repl.* kernel verbs. */
static void replverbhost_exit(void);
static boolean replverbhost_jump_path(const char *path);
static void replverbhost_list(const char *path);

/* Find the script-string handle inside a record returned by
 * menudata_describe_leaf. Returns nil if absent or wrong type. The
 * returned handle lives inside the record envelope and is freed when
 * the caller disposes the record. */
static Handle dispatch_script_handle_from_record(tyvaluerecord rec) {
	if (rec.valuetype != recordvaluetype)
		return nil;
	hdllistrecord hlist = rec.data.recordvalue;

	bigstring bskey;
	copyctopstring("script", bskey);
	long n = opcountlistitems(hlist);
	for (long i = 1; i <= n; i++) {
		bigstring bsfound;
		tyvaluerecord val;
		if (!getnthlistval(hlist, i, bsfound, &val))
			continue;
		if (!equalstrings(bsfound, bskey))
			continue;
		if (val.valuetype != stringvaluetype)
			return nil;
		return (Handle)val.data.stringvalue;
	}
	return nil;
}

/*
 * Result of dispatch_leaf_via_menubar. Distinguishes "succeeded" from the
 * two failure modes the caller wants to surface differently:
 *
 *   DISPATCH_OK              - meuserselected_headless ran to completion.
 *   DISPATCH_NO_SCRIPT_FIELD - the leaf record has no script string field
 *                              (or it's the wrong type, or the leaf record
 *                              itself couldn't be built — i.e. configuration
 *                              error rather than a runtime failure).
 *   DISPATCH_SCRIPT_FAILED   - meuserselected_headless returned false, so
 *                              UserTalk surfaced an error via langerror.
 */
typedef enum {
	DISPATCH_OK = 0,
	DISPATCH_NO_SCRIPT_FIELD,
	DISPATCH_SCRIPT_FAILED
} ty_dispatch_result;

/*
 * Build a Handle of UserTalk source bytes from a C string. The caller
 * owns the returned handle (or nil on failure) and must disposehandle it
 * — except that meuserselected_headless does NOT consume the handle
 * (it copyhandle's internally per Common/source/menudata_headless.c),
 * so the standard pattern is "build → dispatch → disposehandle" on every
 * exit path.
 *
 * No NUL terminator is written into the handle — the parser keys off
 * the handle size, not a sentinel. (gethandlesize is the source of
 * truth for langbuildtree.)
 */
static Handle build_script_handle_from_c_string(const char *src, size_t src_len) {
	Handle htext = nil;
	if (!newemptyhandle(&htext))
		return nil;
	if (src_len == 0)
		return htext;          /* zero-length handle is valid */
	if (!sethandlesize(htext, (long)src_len)) {
		disposehandle(htext);
		return nil;
	}
	HLock(htext);
	memcpy(*htext, src, src_len);
	HUnlock(htext);
	return htext;
}

/*
 * Dispatch a synthesized script source string. Used by the accepts_args
 * dispatch paths (slash-with-args and palette-with-arg) to bind a typed
 * argument into a leaf's call expression at compile time. See
 * palette_arg_inject.h for the synthesis algorithm and the threading
 * rationale.
 *
 * Wraps the build-handle / meuserselected_headless / dispose dance and
 * returns the same DISPATCH_* status as dispatch_leaf_via_menubar so
 * call sites can produce uniform diagnostics. The script_running flag
 * is set across the call for SIGINT consistency.
 */
static ty_dispatch_result dispatch_synthesized_script(const char *src, size_t src_len) {
	Handle htext = build_script_handle_from_c_string(src, src_len);
	if (htext == nil)
		return DISPATCH_SCRIPT_FAILED;
	g_script_running = 1;
	boolean ok = meuserselected_headless(htext);
	g_script_running = 0;
	disposehandle(htext);
	return ok ? DISPATCH_OK : DISPATCH_SCRIPT_FAILED;
}

/*
 * Read the script bytes out of a Handle (returned by
 * dispatch_script_handle_from_record or palette_state.exec_script) into
 * a heap-allocated NUL-terminated C buffer. Caller frees with free().
 *
 * Returns NULL on OOM or an empty handle. The NUL terminator is appended
 * after the handle's raw bytes; the parser doesn't need it (it uses
 * gethandlesize) but the arg-injection helpers expect a length-explicit
 * read-only string.
 */
static char *script_handle_to_cstr(Handle h, size_t *out_len) {
	if (h == nil) return NULL;
	long n = gethandlesize(h);
	if (n < 0) return NULL;
	char *buf = (char *)malloc((size_t)n + 1);
	if (buf == NULL) return NULL;
	if (n > 0) {
		HLock(h);
		memcpy(buf, *h, (size_t)n);
		HUnlock(h);
	}
	buf[n] = '\0';
	if (out_len) *out_len = (size_t)n;
	return buf;
}

/*
 * Dispatch a resolved leaf via meuserselected_headless. Reads the
 * leaf's "script" field via menudata_describe_leaf (which copyvaluerecord's
 * every field, satisfying the handle-privacy contract documented in
 * menudata_headless.h). Makes a private copy of the script handle so
 * meuserselected_headless's internal copyhandle can run safely against a
 * source whose lifetime we control.
 *
 * Returns one of the ty_dispatch_result values so the caller can produce
 * a precise diagnostic. Caller does NOT need to dispose anything —
 * record disposal happens here, and meuserselected_headless does not
 * consume the script-handle copy we hand it (we dispose ours after the
 * call).
 */
static ty_dispatch_result dispatch_leaf_via_menubar(hdlhashtable hleaf) {
	if (hleaf == nil)
		return DISPATCH_NO_SCRIPT_FIELD;

	tyvaluerecord rec;
	if (!menudata_describe_leaf(hleaf, &rec))
		return DISPATCH_NO_SCRIPT_FIELD;

	Handle hscript_in_record = dispatch_script_handle_from_record(rec);
	if (hscript_in_record == nil) {
		disposevaluerecord(rec, false);
		return DISPATCH_NO_SCRIPT_FIELD;
	}

	/* Privacy: copy the script handle so meuserselected_headless's
	 * internal copyhandle has a stable source independent of the
	 * record envelope's lifetime. (The record lives on the stack here,
	 * so disposing it after the call would invalidate the handle in
	 * the middle of langruncode if we passed it directly.) */
	Handle hscript_copy = nil;
	if (!copyhandle(hscript_in_record, &hscript_copy)) {
		disposevaluerecord(rec, false);
		return DISPATCH_SCRIPT_FAILED; /* handle-copy OOM is a runtime failure */
	}

	g_script_running = 1;
	boolean ok = meuserselected_headless(hscript_copy);
	g_script_running = 0;

	disposehandle(hscript_copy);
	disposevaluerecord(rec, false);
	return ok ? DISPATCH_OK : DISPATCH_SCRIPT_FAILED;
}

/*
 * Local stack-buffer size for the slash-command working copy. Chosen
 * intentionally smaller than MAX_COMMAND_LEN: a slash command is a
 * single token plus an optional path argument, both of which fit
 * comfortably in 512 bytes. The full-line buffer (MAX_COMMAND_LEN) is
 * upstream of this dispatcher; oversize lines are rejected here with
 * an explicit error so the user gets a clear diagnostic instead of a
 * silent truncation.
 */
#define SLASH_CMD_BUF 512

/*
 * Process a slash-prefixed line. See the comment block above for the full
 * dispatch flow. Returns true if the REPL should keep running; sets
 * *running = false on /exit dispatch.
 *
 * 2026-06-08 JES Phase C.0 #691: de-static'd so boxen_repl.c can call it
 * directly via repl_slash_dispatch.h.
 */
boolean dispatch_slash_command(const char *line, boolean *running) {
	/* Self-healing reset: if a prior dispatch longjmp'd out of the
	 * UserTalk runtime past the clear sites below, this resets the
	 * flag on re-entry so subsequent reads can't be poisoned. The
	 * UserTalk runtime's exception path doesn't run C cleanup. */
	g_repl_dispatching_from_slash = false;

	/* Defensive copy + trim. The caller has already verified line[0] == '/'. */
	char buf[SLASH_CMD_BUF];
	size_t inlen = strlen(line);
	if (inlen >= sizeof(buf)) {
		fputs("Error: command too long\n", stdout);
		fflush(stdout);
		return true;
	}
	memcpy(buf, line, inlen + 1);

	char *p = buf;
	while (*p != '\0' && isspace((unsigned char)*p)) p++;
	/* skip leading slash */
	if (*p == '/') p++;

	/* Trim trailing whitespace from the full body. We need this BEFORE
	 * the token split so that "/help   \n" with spaces and a newline at
	 * the end doesn't get misclassified as "/help" + " " args. */
	{
		size_t blen = strlen(p);
		while (blen > 0 && isspace((unsigned char)p[blen - 1])) {
			p[--blen] = '\0';
		}
	}

	/*
	 * The unknown-command error message wants the FULL token including
	 * any whitespace+args, so save the body before we split. This
	 * matches the legacy repl_process_command behavior where typing
	 * "/tmp/some/path with space" produced
	 * "Unknown command: /tmp/some/path with space".
	 *
	 * The existing integration test "file.getFileDialog - filename with
	 * spaces" feeds a path like "/tmp/dialogtest/my file.txt" as input
	 * — the test runner type-ahead's it as a slash-prefixed line. Even
	 * though it isn't a real command, we want the FULL path to appear
	 * in the diagnostic so the test's expected_output_contains:
	 * "my file.txt" assertion still hits.
	 *
	 * Copy into a separate buffer because the in-place split below
	 * writes NUL bytes into `p` that would truncate a pointer into it.
	 */
	char body_copy[sizeof(buf)];
	{
		size_t bcap = sizeof(body_copy);
		size_t bn = strlen(p);
		if (bn >= bcap) bn = bcap - 1;
		memcpy(body_copy, p, bn);
		body_copy[bn] = '\0';
	}
	const char *full_body = body_copy;

	/* Split into TOKEN and ARGS at the first whitespace run. */
	char *token_start = p;
	while (*p != '\0' && !isspace((unsigned char)*p)) p++;
	char *args_start;
	if (*p == '\0') {
		args_start = p; /* points at NUL */
	} else {
		*p = '\0';
		p++;
		while (*p != '\0' && isspace((unsigned char)*p)) p++;
		args_start = p;
	}

	/* args_start is now NUL-terminated at the input end; trailing
	 * whitespace was stripped above so no further trim needed. */

	/* Empty token: "/" or "/   ". */
	if (token_start[0] == '\0') {
		fputs("Unknown command: /\nType /help for available commands\n", stdout);
		fflush(stdout);
		return true;
	}

	/* Resolve the token against the menubar. */
	hdlhashtable hleaf = nil;
	char slot_name[REPL_SLASH_TOKEN_MAX];
	if (!repl_resolve_slash_command(token_start, &hleaf,
	                                slot_name, sizeof(slot_name))) {
		/*
		 * Unknown command: print the FULL body (including any args)
		 * with a leading "/" so the user sees what they typed. The
		 * body is scrubbed via safe_print_user_string because it may
		 * contain control bytes from a paste accident or a malicious
		 * type-ahead.
		 */
		fputs("Unknown command: /", stdout);
		safe_print_user_string(full_body);
		fputs("\nType /help for available commands\n", stdout);
		fflush(stdout);
		return true;
	}

	/*
	 * Unified arg-bearing dispatch (issue #594).
	 *
	 * If the leaf advertises accepts_args=true, synthesize a new script
	 * source by injecting the typed arg as a string literal into the
	 * leaf's empty () call frame, then dispatch the synthesized source
	 * via meuserselected_headless. The arg is bound at compile time
	 * inside the call expression — no global hand-off, no GIL-yield
	 * window for a sibling thread.new() child to read the wrong arg.
	 * See palette_arg_inject.h for the synthesis algorithm and threat
	 * model.
	 *
	 * For leaves that don't accept args (e.g. /clear, /exit) the arg
	 * is silently dropped — same legacy behavior.
	 *
	 * Prior to issue #594 a "hybrid" fallthrough special-cased List /
	 * Jump by slot key to call the host adapter directly, because the
	 * Virgin.root install script didn't set accepts_args=true on those
	 * leaves. The install script now does (see installReplMenubar.ut),
	 * so the synthesis path handles them uniformly with every other
	 * leaf — no slot-key special cases, no second dispatcher.
	 */
	if (args_start[0] != '\0') {
		tyvaluerecord rec;
		bool leaf_accepts = false;
		if (menudata_describe_leaf(hleaf, &rec)) {
			if (rec.valuetype == recordvaluetype) {
				hdllistrecord hlist = rec.data.recordvalue;
				bigstring bskey;
				copyctopstring("accepts_args", bskey);
				long n = opcountlistitems(hlist);
				for (long i = 1; i <= n; i++) {
					bigstring bsfound;
					tyvaluerecord val;
					if (!getnthlistval(hlist, i, bsfound, &val)) continue;
					if (!equalstrings(bsfound, bskey)) continue;
					if (val.valuetype == booleanvaluetype) {
						leaf_accepts = val.data.flvalue ? true : false;
					}
					break;
				}
			}
			disposevaluerecord(rec, false);
		}
		if (leaf_accepts) {
			/* Per-call script-source synthesis: rewrite "<handler> ()"
			 * as "<handler> (\"typed-arg\")" and dispatch via the
			 * menubar handler. */
			char escaped[PALETTE_ARG_MAX * 2 + 4];
			palette_arg_escape_result_t esc_r =
				palette_arg_escape(args_start, escaped, sizeof(escaped));
			if (esc_r != PALETTE_ARG_ESCAPE_OK) {
				/* Issue #595: distinguish the two failure modes so the
				 * user sees an actionable diagnostic. Forbidden-byte
				 * means the input contained a control char or DEL;
				 * overflow means the argument was too long. */
				switch (esc_r) {
					case PALETTE_ARG_ESCAPE_FORBIDDEN_BYTE:
						fputs("Error: argument contains a forbidden character "
						      "(control bytes, newlines, and tabs are not allowed)\n",
						      stdout);
						break;
					case PALETTE_ARG_ESCAPE_OVERFLOW:
						fputs("Error: argument is too long\n", stdout);
						break;
					case PALETTE_ARG_ESCAPE_OK:
						break;       /* unreachable */
				}
				fflush(stdout);
				return true;
			}
			/* Read the leaf's stored script bytes. */
			tyvaluerecord rec2;
			if (!menudata_describe_leaf(hleaf, &rec2)) {
				fputs("(menu item is missing script field)\n", stdout);
				fflush(stdout);
				return true;
			}
			Handle hscript = dispatch_script_handle_from_record(rec2);
			if (hscript == nil) {
				disposevaluerecord(rec2, false);
				fputs("(menu item is missing script field)\n", stdout);
				fflush(stdout);
				return true;
			}
			size_t script_len = 0;
			char *script_cstr = script_handle_to_cstr(hscript, &script_len);
			disposevaluerecord(rec2, false);
			if (script_cstr == NULL) {
				fputs("(out of memory reading menu script)\n", stdout);
				fflush(stdout);
				return true;
			}
			char *synth = NULL;
			size_t synth_len = 0;
			bool inj_ok = palette_arg_inject_into_script(script_cstr, script_len,
			                                             escaped, &synth, &synth_len);
			free(script_cstr);
			if (!inj_ok) {
				/* Leaf's script doesn't match the simple "name ()" shape
				 * the injector handles. Fall back to dispatching the
				 * unmodified leaf — handler runs without the arg, which
				 * matches accepts_args=false behavior. Log so a future
				 * menubar maintainer can spot the mismatch. */
				log_warn(LOG_COMP_GENERAL,
				         "slash-command: leaf script could not be transformed "
				         "to inject arg; dispatching without arg.");
				g_repl_dispatching_from_slash = true;
				ty_dispatch_result dr_fallback = dispatch_leaf_via_menubar(hleaf);
				g_repl_dispatching_from_slash = false;
				switch (dr_fallback) {
					case DISPATCH_OK: break;
					case DISPATCH_NO_SCRIPT_FIELD:
						fputs("(menu item is missing script field)\n", stdout);
						fflush(stdout);
						break;
					case DISPATCH_SCRIPT_FAILED:
						fputs("(menu script failed)\n", stdout);
						fflush(stdout);
						break;
				}
				if (slot_key_eq(slot_name, "Exit")) {
					replverbhost_exit();
					*running = false;
				}
				return true;
			}
			g_repl_dispatching_from_slash = true;
			ty_dispatch_result dr_inj = dispatch_synthesized_script(synth, synth_len);
			g_repl_dispatching_from_slash = false;
			free(synth);
			switch (dr_inj) {
				case DISPATCH_OK: break;
				case DISPATCH_NO_SCRIPT_FIELD:
					/* Synthesis path doesn't produce this — keep arm
					 * for type-completeness. */
					fputs("(menu item is missing script field)\n", stdout);
					fflush(stdout);
					break;
				case DISPATCH_SCRIPT_FAILED:
					fputs("(menu script failed)\n", stdout);
					fflush(stdout);
					break;
			}
			if (slot_key_eq(slot_name, "Exit")) {
				replverbhost_exit();
				*running = false;
			}
			return true;
		}
	}

	/* Default: menubar dispatch. /exit is detected by slot-key match
	 * BEFORE the dispatch, because meuserselected_headless runs the
	 * handler synchronously and the handler sets g_repl_exit_requested
	 * via repl.exit() — the main loop's poll of that flag handles the
	 * actual exit. We also short-circuit here so callers that test for
	 * *running == false on /exit observe the change without waiting for
	 * the next event-loop tick. */
	g_repl_dispatching_from_slash = true;
	ty_dispatch_result dr = dispatch_leaf_via_menubar(hleaf);
	g_repl_dispatching_from_slash = false;
	switch (dr) {
		case DISPATCH_OK:
			break;
		case DISPATCH_NO_SCRIPT_FIELD:
			/* Configuration error: the leaf is in the menubar but lacks
			 * a usable script field. Distinct from a runtime failure so
			 * the user can spot a corrupted/incomplete install. */
			fputs("(menu item is missing script field)\n", stdout);
			fflush(stdout);
			break;
		case DISPATCH_SCRIPT_FAILED:
			/* Runtime failure: meuserselected_headless returned false.
			 * The UserTalk error message has already been surfaced via
			 * langerror; this line is just the dispatch summary. */
			fputs("(menu script failed)\n", stdout);
			fflush(stdout);
			break;
	}

	if (slot_key_eq(slot_name, "Exit")) {
		/* Belt-and-braces: also flag exit at this level. The menubar
		 * handler's repl.exit() also sets g_repl_exit_requested via
		 * replverbhost_exit, but a stale host adapter (NULL pointer
		 * after teardown, or a future host that no-ops) shouldn't
		 * leave the REPL running on a /exit slash. */
		replverbhost_exit();
		*running = false;
	}
	return true;
}


/* Process a single command line (extracted for use in event loop) */
static boolean process_line(const char *line, boolean *running) {
	// Skip empty lines
	size_t len = strlen(line);
	if (len == 0) {
		return true;
	}

	// Add to history
	linenoiseHistoryAdd(line);
	track_session_command(line);  // Track for merge-before-save

	// Process command
	if (line[0] == '/') {
		return dispatch_slash_command(line, running);
	}

	// Evaluate as UserTalk with persistent variables
	g_script_running = 1;  // Mark script as running for interrupt handling

	tyvaluerecord val;
	bigstring error_msg;

	/*
	 * PR2 of REPL error context chain (2026-05-27 JES): tell the runtime
	 * to subtract the REPL wrapper's 1-line prefix
	 * ("with system.temp.FrontierREPL.variables {\r") when reporting
	 * line numbers on the <eval> frame. Mirrors the protocol-mode
	 * eval offset install in op_handler.c::handle_script_eval.
	 *
	 * Gated on the variables table existing because
	 * repl_eval_with_variables_value falls through to a non-wrapped
	 * direct eval when it doesn't -- in that path the raw line number
	 * IS the user line and the offset would over-subtract.
	 */
	hdlhashtable repl_vars = repl_get_variables_table();
	if (repl_vars != nil)
		langsetevalinputoffset(1);
	else
		langclearevalinputoffset();

	boolean success = repl_eval_with_variables_value(line, &val, error_msg);

	g_script_running = 0;  // Script finished

	if (success) {
		// Success - display result (handles strings >255 chars)
		repl_output_value(&val);
		disposevaluerecord(val, false);
		langclearevalinputoffset();
	} else {
		// Error - display error.
		// PR2: route through the structured renderer; pass the user's
		// input line as the <eval>-frame source so the source-window
		// has something to point at. The structured renderer reads the
		// snapshot via langgetlasterror/langgetstackframe (set by PR1)
		// and falls back to the legacy single-line repl_output_error
		// when no snapshot is available.
		char error_buf[256];
		size_t error_len = stringlength(error_msg);
		if (error_len > sizeof(error_buf) - 1) {
			error_len = sizeof(error_buf) - 4;
			memcpy(error_buf, stringbaseaddress(error_msg), error_len);
			memcpy(error_buf + error_len, "...", 4);
		} else {
			memcpy(error_buf, stringbaseaddress(error_msg), error_len);
			error_buf[error_len] = '\0';
		}
		repl_output_structured_error(error_buf, line);
		/* Clear the offset AFTER rendering: the structured renderer
		 * reads langgetlasterror, which applies the offset internally
		 * to <eval> frames. Clearing before would leave the offset
		 * unset for a follow-up renderer call (none today, but defense
		 * in depth). */
		langclearevalinputoffset();
	}

	return true;
}

/* ------------------------------------------------------------------- */
/*  Slash-menu palette modal runner                                   */
/* ------------------------------------------------------------------- */
/*
 * The REPL event loop hands off to run_palette_modal() the moment the
 * user types '/' at column 1 of an empty prompt. The modal runner
 * brackets the palette session in its own raw-mode + mouse-tracking
 * window:
 *
 *   1. linenoiseEditStop  — release linenoise's grip on the terminal
 *   2. terminal_init / terminal_enable_raw_mode — re-enter raw mode
 *      under our own termios save (linenoiseEditStop has already
 *      restored the saved cooked-mode termios, so we save again here
 *      so terminal_disable_raw_mode can return to it cleanly).
 *   3. terminal_enable_mouse — turn on SGR 1006 mouse tracking
 *   4. repl_palette_source_init — bind the palette to
 *      system.menus.data.repl
 *   5. palette_open + render
 *   6. byte loop: poll(STDIN) with POLL_TIMEOUT_MS, feed bytes to
 *      palette_feed_byte; intercept SGR mouse (ESC '[' '<') and route
 *      via mouse_parse + palette_feed_mouse; honor SIGWINCH via
 *      repl_sigwinch_consumed + palette_on_resize; honor the ESC
 *      disambiguation timer via palette_feed_esc_timeout.
 *   7. on PALETTE_DONE_EXECUTE: snapshot exec_script, palette_close,
 *      copy the handle into our own ownership (so the caller's
 *      lifetime is independent of the adapter's cache, which is
 *      freed at repl_palette_source_dispose). Dispose the adapter,
 *      then dispatch. The caller of run_palette_modal is responsible
 *      for disposehandle'ing the returned handle after dispatch —
 *      meuserselected_headless does its own internal copyhandle and
 *      does NOT consume ours.
 *      On PALETTE_DONE_CANCEL: palette_close + dispose + return.
 *   8. terminal_disable_mouse + terminal_disable_raw_mode +
 *      terminal_cleanup, then linenoiseEditStart to resume editing.
 *
 * Async output during palette mode: the modal's idle branch yields the
 * GIL via headless_backgroundtask(true) so that thread.new() children,
 * agent ticks, and TCP callback threads can run while the user is
 * mid-keystroke. Without the yield those threads block for the entire
 * lifetime of the open palette — unacceptable for any non-trivial
 * background workload.
 *
 * Tradeoff: TCP callback threads writing to stdout during the modal
 * can scroll cells out from under the palette and corrupt the render
 * for a frame. The compositor's diff-render on the NEXT keystroke
 * repaints over the corrupted area, so the visual artifact is
 * transient. Compositor-aware async output (a scrollback pane) is a
 * follow-up; the GIL-yield correctness fix is in place now to stop
 * the worse problem of a fully deadlocked process.
 *
 * GIL: the palette source vtable callbacks (count_menus, item_describe)
 * touch the ODB, so the GIL must be held across palette_open and every
 * palette_feed_byte. The REPL event loop already holds the GIL.
 */

/*
 * Detect whether the byte stream starting with ESC '[' '<' is the SGR
 * mouse intro. If so, accumulate up through the trailing M/m and
 * dispatch via palette_feed_mouse. Returns true if consumed (caller
 * should not feed b to palette_feed_byte directly).
 *
 * Stateful — uses the static buffer below to accumulate across reads.
 * Reset whenever a non-mouse path is taken so we never carry stale
 * bytes between palette sessions.
 */
#define PALETTE_MOUSE_BUF_MAX 32

typedef enum {
	MOUSE_SGR_IDLE = 0,
	MOUSE_SGR_GOT_ESC,
	MOUSE_SGR_GOT_BRACKET,
	MOUSE_SGR_BUFFERING
} mouse_sgr_state_t;

typedef struct {
	mouse_sgr_state_t state;
	char buf[PALETTE_MOUSE_BUF_MAX];
	int len;
} mouse_sgr_parser_t;

static void mouse_sgr_reset(mouse_sgr_parser_t *p) {
	p->state = MOUSE_SGR_IDLE;
	p->len = 0;
	p->buf[0] = '\0';
}

/*
 * Returns:
 *   < 0 — not a mouse byte (or aborted parse); caller should treat the
 *         original byte as a normal palette byte. The parser may have
 *         buffered preceding bytes that the caller already consumed —
 *         those are lost (non-issue for ESC '[' '<' which is
 *         unambiguously mouse-intro).
 *   = 0 — byte consumed but mouse sequence still in progress.
 *   > 0 — full SGR mouse sequence parsed; *out_ev populated.
 */
static int mouse_sgr_feed(mouse_sgr_parser_t *p, unsigned char b,
                          mouse_event_t *out_ev) {
	switch (p->state) {
	case MOUSE_SGR_IDLE:
		if (b == 0x1b) {
			p->state = MOUSE_SGR_GOT_ESC;
			return 0;
		}
		return -1;
	case MOUSE_SGR_GOT_ESC:
		if (b == '[') {
			p->state = MOUSE_SGR_GOT_BRACKET;
			return 0;
		}
		mouse_sgr_reset(p);
		return -1;
	case MOUSE_SGR_GOT_BRACKET:
		if (b == '<') {
			p->state = MOUSE_SGR_BUFFERING;
			p->len = 0;
			return 0;
		}
		mouse_sgr_reset(p);
		return -1;
	case MOUSE_SGR_BUFFERING:
		if (p->len < PALETTE_MOUSE_BUF_MAX - 1) {
			p->buf[p->len++] = (char)b;
			p->buf[p->len] = '\0';
		}
		if (b == 'M' || b == 'm') {
			/* Reconstruct full SGR sequence for mouse_parse, which
			 * expects "<...M" or "<...m". */
			char seq[PALETTE_MOUSE_BUF_MAX + 2];
			seq[0] = '<';
			memcpy(seq + 1, p->buf, (size_t)p->len);
			seq[1 + p->len] = '\0';
			int parsed = mouse_parse(seq, (size_t)(1 + p->len), out_ev) ? 1 : -1;
			mouse_sgr_reset(p);
			return parsed;
		}
		return 0;
	}
	mouse_sgr_reset(p);
	return -1;
}

/*
 * Drive a complete palette session. Returns:
 *   NULL — palette was cancelled (or never opened due to error). No
 *          dispatch needed.
 *   non-NULL — script handle that should be passed to
 *              meuserselected_headless. Caller owns this handle (it
 *              was copyhandle'd out of adapter-private storage) and
 *              MUST disposehandle it after dispatch on every path.
 *              meuserselected_headless does its own internal copyhandle
 *              (Common/source/menudata_headless.c:1056) — it does NOT
 *              consume the caller's handle.
 *
 * GIL: caller must hold it. See the file-level note above for the
 * yield policy inside the byte loop.
 */
/*
 * out_arg, if non-NULL, receives a copy of the palette's arg input
 * buffer on PALETTE_DONE_EXECUTE for items where accepts_args was
 * true. The caller should pre-zero out_arg or treat it as undefined
 * unless run_palette_modal returns non-nil. arg_cap is the buffer
 * capacity including NUL.
 *
 * Lifetime: out_arg is filled in synchronously before run_palette_modal
 * returns. The caller does not need to free it (stack-allocated by the
 * caller).
 */
static Handle run_palette_modal(struct linenoiseState *ls,
                                char *line_buf, size_t line_buflen,
                                char *out_arg, size_t arg_cap) {
	Handle script_to_run = nil;
	if (out_arg && arg_cap > 0) out_arg[0] = '\0';

	/* 1. Bracket linenoise. */
	linenoiseEditStop(ls);

	/* 2. Re-enter raw mode under our own termios save. */
	terminal_state ts;
	if (!terminal_init(&ts) || !terminal_enable_raw_mode(&ts)) {
		log_error(LOG_COMP_GENERAL, "palette: failed to enter raw mode");
		terminal_cleanup(&ts);
		(void)linenoiseEditStart(ls, STDIN_FILENO, STDOUT_FILENO,
		                         line_buf, line_buflen, g_repl_prompt);
		return nil;
	}
	terminal_enable_mouse();
	/* Hide the terminal cursor for the modal lifetime. Without this the
	 * cursor parks next to whichever cell the compositor most recently
	 * wrote, producing a stray "]" next to the selected menu item in
	 * terminals that render the block cursor on top of menu paint. The
	 * show-cursor counterpart fires in the cleanup_terminal path below
	 * BEFORE terminal_disable_raw_mode so the SGR write goes out while
	 * the terminal is still in raw mode. */
	terminal_hide_cursor();

	/* 3. Build the ODB-backed palette source (all installed bars). */
	palette_menu_source_t src;
	bool src_ok = repl_palette_source_init_all(&src);
	if (!src_ok) {
		printf("(no menubar installed: system.menus.data.*)\n");
		fflush(stdout);
		goto cleanup_terminal;
	}

	/* 4. Open palette at current terminal geometry. */
	int rows = 24, cols = 80;
	(void)terminal_get_size(&rows, &cols);
	compositor_on_resize(rows, cols);

	/* Query the cursor row via DSR so the palette can anchor the menubar
	 * directly below the REPL prompt instead of at row 0.
	 *
	 * Cursor vs prompt: linenoiseEditStop above emitted '\n', which moved
	 * the cursor down one row. When the prompt was on the bottom row the
	 * '\n' also scrolled the terminal up by one, so post-EditStop the
	 * cursor is always on a fresh blank row immediately BELOW the prompt
	 * (whether the prompt is mid-screen or just got scrolled up by one).
	 * We therefore subtract another row from the DSR cursor reading to
	 * land on the prompt itself; the menubar then anchors at
	 * prompt_row + 1 = cursor_row, the freed blank row.
	 *
	 * DSR-fails fallback: when stderr is redirected (test harness) or
	 * the terminal doesn't echo a DSR reply, default to "prompt sits one
	 * above the bottom row" -- i.e. cursor (post linenoise '\n' scroll)
	 * is on the bottom row, prompt is the row above. This keeps the
	 * fallback consistent with the success path's accounting.
	 *
	 * Test override: FRONTIER_PALETTE_PROMPT_ROW (1-based) lets the L4
	 * harness pin the prompt row deterministically without depending on
	 * a working DSR round-trip (the harness redirects stderr away from
	 * the PTY to keep diagnostic chatter out of pyte's framebuffer,
	 * which also eats the DSR query response). Production never sets
	 * this env var. */
	int cursor_row = rows;          /* 1-based default: bottom row */
	int cursor_col = 1;
	const char *prow_env = getenv("FRONTIER_PALETTE_PROMPT_ROW");
	bool prompt_row_from_env = false;
	int prompt_row = 0;
	if (prow_env && prow_env[0] != '\0') {
		int v = atoi(prow_env);
		if (v >= 1 && v <= rows) {
			prompt_row = v - 1;       /* env var is 1-based */
			prompt_row_from_env = true;
		}
	}
	if (!prompt_row_from_env) {
		(void)terminal_get_cursor_pos(&cursor_row, &cursor_col);
		/* terminal_get_cursor_pos returns 1-based; the palette uses
		 * 0-based. cursor_row - 1 is the cursor's 0-based row; the
		 * prompt is one row above that (linenoise's '\n' moved the
		 * cursor off the prompt line). */
		prompt_row = cursor_row - 2;
	}
	if (prompt_row < 0) prompt_row = 0;
	if (prompt_row >= rows) prompt_row = rows - 1;

	/* Reserve rows for the cascade so submenu drills don't silently
	 * fail when the prompt is near the bottom of the terminal.
	 *
	 * Geometry: menubar lives at prompt_row+1; cascade level d lives
	 * at prompt_row+1+(d+1). To open all levels through depth `reserve`
	 * we need prompt_row + 1 + reserve <= rows - 1, i.e.
	 * prompt_row <= rows - 2 - reserve.
	 *
	 * Frontier's headless menubar tree is exactly 3 deep (bar/menu/
	 * leaf), so a typical session never needs more than 2 cascade rows
	 * below the menubar. Reserve PALETTE_CASCADE_RESERVE_ROWS so the
	 * common case of "open the REPL menu" never silently fails because
	 * the terminal lacks one free row. This is smaller than
	 * PALETTE_MAX_DEPTH (8) on purpose — reserving 8 rows would feel
	 * intrusive on small terminals, and most menus are 2-3 deep in
	 * practice.
	 *
	 * If reserve is required, emit '\n' enough times to scroll the
	 * terminal up; each scroll bumps prompt_row down by 1 from the
	 * cursor's perspective without changing the absolute row count.
	 * Because '\n' originates from the bottom row (where the cursor
	 * sits post-linenoise '\n'), each emit shifts the visible
	 * scrollback up by one and frees a row at the bottom. */
	#define PALETTE_CASCADE_RESERVE_ROWS 4
	int reserve = PALETTE_CASCADE_RESERVE_ROWS;
	if (reserve > PALETTE_MAX_DEPTH) reserve = PALETTE_MAX_DEPTH;
	int max_prompt = rows - 2 - reserve;
	if (max_prompt < 0) max_prompt = 0;
	if (prompt_row > max_prompt) {
		int need = prompt_row - max_prompt;
		/* Raw mode is in effect (terminal_enable_raw_mode above), so
		 * OPOST is off — '\n' is literal LF, not CR+LF. Pair each LF
		 * with a CR so the cursor lands at column 1 of the scrolled-in
		 * row; the absolute row of the cursor doesn't change at the
		 * bottom (the terminal scrolls the buffer up instead), which
		 * is exactly the effect we want. */
		for (int i = 0; i < need; ++i) {
			fputs("\r\n", stdout);
		}
		fflush(stdout);
		prompt_row = max_prompt;
	}

	/* Scrollback pane (issue #593) — covers rows 1..rows-1 (everything
	 * below the menubar at row 0). Registered FIRST so it sits at the
	 * bottom of the z-stack; palette panes register later and composite
	 * over it. While the modal is active, the async-output router
	 * appends scrollback lines here instead of writing stdout, so
	 * background producers (TCP callbacks, agent ticks, msg() calls)
	 * never corrupt the framebuffer.
	 *
	 * SCROLLBACK_LINES_CAP: 256 lines is enough for any realistic
	 * mid-modal log burst; older lines wrap off the bottom of the
	 * ring and are still flushed on close so nothing is lost. */
	#define SCROLLBACK_LINES_CAP 256
	scrollback_pane_t scrollback;
	int sb_rows = (rows > 1) ? (rows - 1) : 1;
	scrollback_pane_init(&scrollback, 0, 1, cols, sb_rows, SCROLLBACK_LINES_CAP);
	compositor_register(&scrollback.pane);
	repl_async_output_set_palette_active(true, &scrollback);

	palette_state_t st;
	memset(&st, 0, sizeof(st));
	if (!palette_open(&st, rows, cols, prompt_row, &src)) {
		printf("(menubar empty or terminal too small)\n");
		fflush(stdout);
		/* Tear down scrollback we just set up — the modal is
		 * aborting before its byte loop ever runs. */
		repl_async_output_set_palette_active(false, NULL);
		compositor_unregister(&scrollback.pane);
		scrollback_pane_destroy(&scrollback);
		repl_palette_source_dispose(&src);
		goto cleanup_terminal;
	}
	palette_render_state(&st);
	compositor_render();

	/* L4 harness fast-timer notice. The env var compresses the
	 * bare-ESC vs ESC-CSI disambiguation window from 10ms to 1ms so
	 * pexpect-driven integration tests do not stall on each ESC. It
	 * also changes the modal's idle poll cadence, which is harmless
	 * outside tests but worth a one-time stderr warning so a user
	 * with this env var accidentally set understands why their
	 * terminal feels twitchy. */
	/* Single-thread modal entry invariant; see GIL note above
	 * (run_palette_modal runs only on the GIL-holding REPL thread,
	 * so the static flag does not need an atomic guard). */
	static bool fast_timers_logged = false;
	if (!fast_timers_logged) {
		fast_timers_logged = true;
		const char *ft = getenv("FRONTIER_PALETTE_FAST_TIMERS");
		if (ft && ft[0] != '\0') {
			fprintf(stderr,
				"[palette] FRONTIER_PALETTE_FAST_TIMERS=%s -- "
				"using %dms ESC timeout (test mode)\n",
				ft, palette_esc_timeout_ms());
		}
	}

	/* 5. Modal byte loop. */
	mouse_sgr_parser_t mouse;
	mouse_sgr_reset(&mouse);
	bool palette_running = true;
	palette_done_t done = PALETTE_DONE_NONE;

	while (palette_running) {
		struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
		int ready = poll(&pfd, 1, palette_esc_timeout_ms());

		if (ready > 0 && (pfd.revents & POLLIN)) {
			unsigned char b;
			ssize_t n = read(STDIN_FILENO, &b, 1);
			if (n <= 0) {
				/* EOF or error — treat as cancel. */
				done = PALETTE_DONE_CANCEL;
				palette_running = false;
				break;
			}

			/* SGR mouse intercept. We try mouse first ONLY when we're
			 * already in mid-sequence OR the byte is ESC at idle and
			 * the next two bytes complete '[<'. The mouse parser is
			 * conservative — it returns -1 (not consumed) on the very
			 * first non-mouse byte so we can fall through to palette.
			 *
			 * Snapshot the parser state BEFORE the feed so that on a
			 * non-mouse abort (mouse_rc < 0) we can correctly replay
			 * the bytes the parser had buffered. mouse_sgr_feed
			 * resets state before returning -1, so a post-call read
			 * of mouse.state is always MOUSE_SGR_IDLE — useless for
			 * deciding what to replay. */
			mouse_sgr_state_t prev_state = mouse.state;
			mouse_event_t ev;
			int mouse_rc = mouse_sgr_feed(&mouse, b, &ev);
			if (mouse_rc > 0) {
				done = palette_feed_mouse(&st, &ev);
			} else if (mouse_rc == 0) {
				/* Byte buffered as part of mouse sequence — do NOT
				 * also feed it to palette; fall through to render. */
				done = PALETTE_DONE_NONE;
			} else {
				/* mouse_rc < 0: byte not part of a mouse sequence.
				 * The parser had buffered some prefix the palette
				 * never saw; we replay it here, then feed the
				 * current byte. Two miss-cases:
				 *   prev_state == GOT_ESC: parser ate ESC, then
				 *     `b` was not '['. We feed ESC, then `b` (b is
				 *     the bare-ESC's follow byte — palette gets a
				 *     full ESC + b sequence, exactly as typed).
				 *   prev_state == GOT_BRACKET: parser ate ESC and
				 *     '[', then `b` was not '<'. Palette gets ESC
				 *     '[' + b.
				 *   prev_state == IDLE: parser didn't buffer
				 *     anything (b is the first byte and not ESC,
				 *     so mouse_sgr_feed returned -1 directly).
				 *     Feed b alone.
				 *   prev_state == BUFFERING: parser's mid-sequence
				 *     bail (e.g. SGR sequence got corrupted). The
				 *     buffered bytes are lost; feed b as best-
				 *     effort recovery. This is only reachable on
				 *     malformed input — expected to be rare.
				 */
				if (prev_state == MOUSE_SGR_GOT_ESC) {
					(void)palette_feed_byte(&st, 0x1b);
					done = palette_feed_byte(&st, b);
				} else if (prev_state == MOUSE_SGR_GOT_BRACKET) {
					(void)palette_feed_byte(&st, 0x1b);
					(void)palette_feed_byte(&st, '[');
					done = palette_feed_byte(&st, b);
				} else {
					done = palette_feed_byte(&st, b);
				}
			}
		} else if (ready == 0) {
			/* Timeout — fire ESC disambiguation if pending.
			 *
			 * Lone-ESC rescue: if the mouse SGR parser greedily
			 * buffered a bare ESC waiting for '[<...M' and no
			 * follow-up byte arrived within the poll window, the
			 * ESC was a real bare ESC (not a mouse intro). Reset
			 * the parser and deliver the ESC to palette_feed_byte
			 * BEFORE the timeout call, so the palette sees
			 * esc_pending and the subsequent palette_feed_esc_timeout
			 * can cancel correctly. Without this, the buffered
			 * ESC was lost and the menubar was undismissable by
			 * a single ESC keystroke.
			 *
			 * Only MOUSE_SGR_GOT_ESC is handled here. The other
			 * mid-sequence states (MOUSE_SGR_GOT_BRACKET,
			 * MOUSE_SGR_BUFFERING) require a real terminal to send
			 * a partial CSI and then pause for >poll_timeout_ms —
			 * terminals ship CSI sequences atomically in one TTY
			 * write, so this is practically unreachable. If a future
			 * test or pathological client surfaces it, mirror the
			 * pattern: reset parser, replay the buffered bytes via
			 * palette_feed_byte. palette_feed_byte(0x1b) on a fresh
			 * palette always returns PALETTE_DONE_NONE today (it
			 * just sets esc_pending), so we let control fall through
			 * to palette_feed_esc_timeout below — keeps the byte-read
			 * branch and timeout branch sharing the same post-dispatch
			 * bookkeeping (SIGWINCH check, GIL yield). */
			if (mouse.state == MOUSE_SGR_GOT_ESC) {
				mouse_sgr_reset(&mouse);
				(void)palette_feed_byte(&st, 0x1b);
			}
			done = palette_feed_esc_timeout(&st);
			/* GIL yield: while the palette is open the main thread
			 * holds the GIL exclusively. Without a yield in the idle
			 * branch, every thread.new() child / TCP callback
			 * thread / agent tick blocks for the entire duration of
			 * a user mid-keystroke session. Calling
			 * headless_backgroundtask() at the poll-timeout point
			 * mirrors the pattern in the main REPL event loop and
			 * the blocking REPL loop — one yield per idle tick. */
			headless_backgroundtask(true);
			/* Background work above may have appended lines to the
			 * scrollback ring (e.g. a TCP callback called msg()).
			 * Project those lines onto the scrollback pane buffer so
			 * the next compositor_render emits them. The render
			 * itself happens via the PALETTE_DONE_NONE branch below. */
			scrollback_pane_render_to_pane(&scrollback);
		} else if (ready < 0) {
			if (errno == EINTR) {
				/* SIGWINCH or SIGINT during poll. SIGINT is fatal for
				 * the palette session — close and propagate.
				 *
				 * Consume-on-handle: the palette uses the SIGINT to
				 * exit the modal. The main loop polls the same flag
				 * and would otherwise call handle_interrupt(), which
				 * prints "^C\n" — producing a double echo (one from
				 * the palette teardown, one from the main loop) for
				 * the same Ctrl-C. Clear the flag here so only the
				 * palette gets the consumed signal. */
				if (g_repl_interrupt_requested) {
					g_repl_interrupt_requested = 0;
					done = PALETTE_DONE_CANCEL;
					palette_running = false;
					break;
				}
				/* Otherwise (likely SIGWINCH) loop and let the resize
				 * check below handle it. */
				done = PALETTE_DONE_NONE;
			} else {
				log_error(LOG_COMP_GENERAL, "palette: poll() failed: %s",
				          strerror(errno));
				done = PALETTE_DONE_CANCEL;
				palette_running = false;
				break;
			}
		}

		/* Honor SIGWINCH after every poll cycle. */
		if (repl_sigwinch_consumed()) {
			int new_rows = 24, new_cols = 80;
			(void)terminal_get_size(&new_rows, &new_cols);
			compositor_on_resize(new_rows, new_cols);
			palette_on_resize(&st, new_rows, new_cols);
			/* Resize scrollback pane to match the new geometry so it
			 * keeps covering everything below row 0. The ring's stored
			 * lines are unaffected — only the projection area changes. */
			int new_sb_rows = (new_rows > 1) ? (new_rows - 1) : 1;
			pane_resize(&scrollback.pane, new_cols, new_sb_rows);
			scrollback_pane_render_to_pane(&scrollback);
		}

		switch (done) {
		case PALETTE_DONE_NONE:
			palette_render_state(&st);
			compositor_render();
			break;
		case PALETTE_DONE_EXECUTE:
		case PALETTE_DONE_CANCEL:
			palette_running = false;
			break;
		}
	}

	/* 6. Capture exec_script BEFORE close (the palette doc says close
	 *    does not invalidate it, but the adapter's dispose will).
	 *    Then copyhandle into our own ownership so meuserselected_headless
	 *    can consume it via langbuildtree without yanking the rug out
	 *    from under the adapter's later dispose. */
	if (done == PALETTE_DONE_EXECUTE && st.exec_script != NULL) {
		Handle hsrc = (Handle)st.exec_script;
		Handle hcopy = nil;
		if (copyhandle(hsrc, &hcopy)) {
			script_to_run = hcopy;
			/* Snapshot the arg buffer if caller asked for it. The
			 * arg is empty unless the dispatched item had
			 * accepts_args=true and the user typed in the arg row. */
			if (out_arg && arg_cap > 0) {
				strncpy(out_arg, st.exec_arg, arg_cap - 1);
				out_arg[arg_cap - 1] = '\0';
			}
		} else {
			log_error(LOG_COMP_GENERAL,
			          "palette: copyhandle failed for exec_script");
		}
	}

	/* Paint every palette pane back to terminal-default attributes and
	 * flush ONE compositor frame so the cleared cells reach the
	 * terminal BEFORE we unregister the panes. Without this, the
	 * compositor's diff render sees no change to cells that still
	 * carry menubar / selection attributes in the previous-frame
	 * buffer, and subsequent writes (a dispatched leaf script's stdout,
	 * the restarted linenoise prompt) inherit those attributes —
	 * producing the cyan-on-blue bleed observed before this fix.
	 * Covers both PALETTE_DONE_EXECUTE and PALETTE_DONE_CANCEL exit
	 * paths; the helper is a no-op when the palette is not active. */
	palette_paint_teardown(&st);
	compositor_render();

	palette_close(&st);
	repl_palette_source_dispose(&src);

	/* Tear down the scrollback pane (#593). Order matters:
	 *   1. Disarm the router FIRST so any in-flight async writer
	 *      transitions back to stdout cleanly. After this, no new
	 *      appends can land in the scrollback.
	 *   2. Unregister the pane from the compositor. Subsequent renders
	 *      (including the linenoise restart below) will not include
	 *      its cells.
	 *   3. Flush queued lines to stdout — the user expects to see
	 *      whatever happened during the modal in their normal terminal
	 *      scrollback. The flush happens BEFORE destroy frees the ring,
	 *      and AFTER unregister so writes go to stdout cleanly without
	 *      the compositor's diff render contending. */
	repl_async_output_set_palette_active(false, NULL);
	compositor_unregister(&scrollback.pane);
	scrollback_pane_flush_to_stdout(&scrollback);
	scrollback_pane_destroy(&scrollback);

cleanup_terminal:
	terminal_disable_mouse();
	/* Restore the cursor BEFORE leaving raw mode so the SGR escape is
	 * emitted via the same raw stream that hid it. Mirrors the
	 * terminal_hide_cursor() call right after raw_mode was enabled. */
	terminal_show_cursor();
	terminal_disable_raw_mode(&ts);
	terminal_cleanup(&ts);

	/* 7. Anchor the cursor at column 1 of a fresh row before returning.
	 *
	 * Why not linenoiseEditStart here? Restarting line editing inside
	 * run_palette_modal would print the prompt at whatever cell the
	 * compositor left the cursor on. The caller still has to dispatch
	 * the chosen leaf script after we return (see the run_palette_modal
	 * call site near "leaf dispatch" in repl_main_loop), and that
	 * script's stdout would then collide with our freshly-printed
	 * prompt -- producing the garbled output that this fix addresses.
	 *
	 * Instead, emit \r\n to position the cursor at column 1 of a fresh
	 * blank row. The caller dispatches the leaf (which writes its
	 * output starting on that row), then restarts linenoise itself --
	 * matching the discipline of the Enter-key path further down in
	 * repl_main_loop, where linenoiseEditStop's own newline emit
	 * already provides this anchor and the new linenoiseEditStart
	 * happens AFTER the line has been processed. */
	printf("\r\n");
	fflush(stdout);

	return script_to_run;
}


/* ------------------------------------------------------------------- */
/*  repl.* kernel-verb host adapter                                    */
/* ------------------------------------------------------------------- */
/*
 * These are the host-side bindings registered with repl_verbs.c via
 * repl_verbs_set_host(). They translate the verb-level callbacks into
 * the same module-private state that the existing /exit, /clear,
 * /jump, /keycodes, /list slash-command handlers manipulate, so a
 * UserTalk script calling repl.exit() ends up in exactly the same
 * place as a user typing "/exit" at the prompt.
 *
 * GIL: every callback here runs under the verb dispatcher, which is
 * invoked from langruncode while the GIL is held. The REPL main
 * thread is the typical caller, but UserTalk thread.new() children
 * can also call into these adapters once they have acquired the GIL
 * — so writes to module state are NOT necessarily on the main
 * thread. We rely on (a) the GIL acquire/release barrier for
 * visibility and (b) volatile sig_atomic_t storage for the exit
 * flag (see g_repl_exit_requested above) so the read in the main
 * loop is well-defined even if the GIL story changes.
 */

static void replverbhost_exit(void) {
	g_repl_exit_requested = 1;
}

static void replverbhost_help(void) {
	repl_output_help();
}

static void replverbhost_clear_variables(void) {
	hdlhashtable vars = repl_get_variables_table();
	if (vars != nil)
		emptyhashtable(vars, true);
	repl_jump_path("");
}

/*
 * NOTE (security): repl_jump_path() interprets `path` as a UserTalk
 * script expression if it contains '(', ')', or '+'
 * (path_is_script_expression at the top of this file → repl_jump_script
 * → langrun). This is fine for interactive REPL use but unsafe for any
 * caller forwarding untrusted strings. Document at the verb header
 * (repl_verbs.h::jump_path) so script authors know.
 *
 * Failure reporting: when repl_jump_path() returns false the host adapter
 * emits the legacy "Error: '<path>' is not a valid table path" diagnostic
 * to stdout. This used to live in dispatch_slash_command's direct-dispatch
 * branch for /jump; issue #594 collapsed that branch in favor of the
 * unified menubar synthesis path, so the error message had to migrate
 * with the failure-detection responsibility. Centralizing it in the host
 * adapter (matching replverbhost_list, which already prints its own error)
 * gives every caller — slash-command, palette, programmatic UserTalk —
 * uniform error text. The verb still returns the boolean result so
 * UserTalk callers that want silent failure can branch on the return
 * value (they will see the message but can ignore it).
 */
static boolean replverbhost_jump_path(const char *path) {
	/* The verb's path argument is the source of truth. When the palette
	 * dispatches a leaf with accepts_args=true, the typed arg is bound
	 * into the call expression at compile time (see palette_arg_inject.h),
	 * so it arrives here as a normal `path` parameter — no global
	 * hand-off, no GIL-yield window for sibling threads to consume the
	 * wrong value. */
	if (path == NULL)
		return repl_jump_path("");
	boolean ok = repl_jump_path(path);
	if (!ok) {
		/* `path` is the unfiltered C string from the verb arg and may
		 * contain control bytes from a paste accident or a malicious
		 * type-ahead. Scrub via safe_print_user_string before reaching
		 * the terminal — see repl_output.c safe_print_user_string for
		 * the threat model. */
		fputs("Error: '", stdout);
		safe_print_user_string(path);
		fputs("' is not a valid table path\n", stdout);
		fflush(stdout);
	}
	return ok;
}

/*
 * linenoisePrintKeyCodes() takes exclusive control of stdin in raw mode
 * and only returns once the user types ESC three times. While it runs,
 * the calling thread holds the GIL and cannot release it — so a
 * misplaced repl.printKeyCodes() call from a non-interactive context
 * (e.g. -e mode, webserver verb, agent script) would freeze every
 * GIL-dependent thread until something poked stdin three times.
 *
 * Defense: refuse the call when the REPL is not active OR stdin is not
 * a TTY. The verb will return false at the script level and the caller
 * gets a clean recoverable error rather than a hung process. See
 * repl_verbs.h::print_key_codes for the contract.
 */
static boolean replverbhost_print_key_codes(void) {
	if (!g_repl_active)
		return false;
	if (!isatty(STDIN_FILENO))
		return false;
	linenoisePrintKeyCodes();
	return true;
}

/*
 * Returns true iff the current script call was dispatched from a slash
 * command (e.g. /jump, /list) rather than from palette menu activation.
 *
 * Reads g_repl_dispatching_from_slash, which dispatch_slash_command sets
 * true around every meuserselected_headless (and dispatch_synthesized_script)
 * call and clears false on every exit path. Because the GIL is held for the
 * full duration of the dispatch, no locking is needed.
 */
static boolean replverbhost_from_slash(void) {
	return g_repl_dispatching_from_slash;
}

/*
 * Returns true iff a REPL session is currently active (the host has entered
 * the REPL main loop and not yet exited). Handler scripts use repl.isActive()
 * to gate interactive operations (dialog.ask prompts) so they do not fire
 * in non-REPL contexts (integration tests, protocol mode, -e evaluation).
 */
static boolean replverbhost_is_active(void) {
	return g_repl_active;
}

static void replverbhost_list(const char *path) {
	/* The verb's path argument is the source of truth — see
	 * replverbhost_jump_path for the threading rationale. */
	if (path == NULL || path[0] == '\0') {
		repl_output_list(nil, NULL);
		return;
	}

	typathlookupresult result;
	char resolved_path[REPL_PATH_MAX_LEN];
	char error_msg[256] = "";

	if (!repl_resolve_path_ex(path, &result, resolved_path, sizeof(resolved_path),
	                          error_msg, sizeof(error_msg))) {
		/* `path` is the unfiltered C string from the verb arg, and
		 * error_msg may interpolate user-supplied path components.
		 * Both must be scrubbed before reaching the terminal — see
		 * safe_print_user_string for threat model. */
		if (error_msg[0] != '\0') {
			fputs("Error: '", stdout);
			safe_print_user_string(path);
			fputs("' is not a valid table path (", stdout);
			safe_print_user_string(error_msg);
			fputs(")\n", stdout);
		} else {
			fputs("Error: '", stdout);
			safe_print_user_string(path);
			fputs("' is not a valid table path\n", stdout);
		}
		return;
	}

	if (result.is_table)
		repl_output_list(result.htable, resolved_path);
	else
		repl_output_single_value(resolved_path, &result.val);
}

/*
 * Walk the ut-sync tree and create any ODB nodes whose .ut file exists on
 * disk but is absent from the in-memory hashtable.  Delegates to
 * ut_sync_scan_and_create().  Backs repl.syncScan().
 *
 * GIL: called on the UserTalk thread that evaluates repl.syncScan(), which
 * holds the GIL at the point of call (all UserTalk evaluation is GIL-held).
 * ut_sync_scan_and_create and all kernel primitives it calls are safe.
 *
 * Returns the count of newly created nodes (>= 0), or 0 when ut-sync mode
 * is not active (--ut-sync-dir was not supplied) so the verb is a safe
 * no-op in non-sync sessions.
 */
static int replverbhost_sync_scan(void) {
	const char *sync_dir = cli_get_ut_sync_dir();
	if (sync_dir == NULL)
		return 0;
	int n = ut_sync_scan_and_create(sync_dir, cli_get_system_root_basename(),
	                                0 /* record_for_redirty: post-boot, dirty bits persist naturally */);
	return (n < 0) ? 0 : n;
}

/*
 * Build and install the host adapter struct. Called once from repl_main
 * before either loop runs. Pairs with repl_verbs_set_host(NULL) on
 * cleanup so a follow-on call to a repl.* verb after the REPL exits
 * fails closed (returns false at the script level) rather than calling
 * stale function pointers.
 */
static void install_repl_verbs_host(void) {
	repl_verbs_host_t host;
	host.exit = replverbhost_exit;
	host.clear_variables = replverbhost_clear_variables;
	host.jump_path = replverbhost_jump_path;
	host.print_key_codes = replverbhost_print_key_codes;
	host.list = replverbhost_list;
	host.help = replverbhost_help;
	host.from_slash = replverbhost_from_slash;
	host.is_active = replverbhost_is_active;
	host.sync_scan = replverbhost_sync_scan;
	repl_verbs_set_host(&host);
}

static void uninstall_repl_verbs_host(void) {
	repl_verbs_set_host(NULL);
}

/*
 * repl_install_verb_host / repl_uninstall_verb_host - public wrappers so
 * protocol_main (protocol_handler.c) can install the same host adapter that
 * repl_main installs.  This ensures that repl.* verbs (including
 * repl.syncScan()) work in both --repl and --protocol modes.
 */
void repl_install_verb_host(void) {
	install_repl_verbs_host();
}
void repl_uninstall_verb_host(void) {
	uninstall_repl_verbs_host();
}

/*
 * 2026-06-08 JES #691 Phase C.0 round 2 P1-8: reset stale exit flag.
 *
 * Clears g_repl_exit_requested so a prior REPL session's exit state does
 * not pre-exit a new session.  Called by boxen_repl_main at entry, before
 * repl_install_verb_host.  repl_main already calls this inline at line 3814.
 */
void repl_reset_exit_flag(void) {
	g_repl_exit_requested = 0;
}


/*
 * Boot the REPL menubar by invoking the UserTalk install script, then
 * run the legacy dynamic-composition verb system.menus.buildMenuBar to
 * fold in any additional menus (user.menus.*, system.menus.helpMenu,
 * the frontmost-keyed modal menu, etc.) the way the legacy GUI did.
 *
 * Design note: the menubar lives in the ODB (system.menus.data.repl), and
 * its install logic + handler scripts live in UserTalk (under
 * system.menus.installReplMenubar and system.menus.handlers.repl.*).
 * Invoking those scripts is therefore a one-shot langrun() of the
 * expression "system.menus.installReplMenubar()". The script itself is
 * idempotent — guarded by menu.isInstalled — so re-invocation across
 * sessions is safe and cheap.
 *
 * system.menus.buildMenuBar is the legacy UserTalk composer. Its
 * headless effect is currently a no-op for the bar projection: the
 * composer's menu.install calls target addresses outside
 * system.menus.data (e.g. @user.menus.menubar, @system.menus.helpMenu),
 * which menudata_resolve_bar_path treats as depth=0 and does not flip
 * the .installed flag for. menu.clearMenuBar and kernel menu.buildMenuBar
 * are also headless no-ops. Wiring the composer in now is a
 * mechanism-fidelity step: as soon as editor windows exist and the
 * frontmost-keyed modal menu has somewhere to land, the same code path
 * will produce the dynamic per-window behavior. The empty-frontmost
 * path is handled gracefully by the composer itself (try/else around
 * the modal install).
 *
 * Boot failure mode (per ADR-016 / planning doc): a missing or broken
 * install or composer script must NOT take down the REPL. Log a
 * prominent warning and continue. The user can still operate via legacy
 * /commands and the palette will simply show no items if it's opened.
 *
 * GIL: must be called with the GIL held. Called once from repl_main
 * before either loop runs.
 */
static void install_repl_menubar(void) {
	const char *expr =
	    "if defined (@system.menus.installReplMenubar) "
	    "{system.menus.installReplMenubar ()}; "
	    "if defined (@system.menus.buildMenuBar) "
	    "{system.menus.buildMenuBar ()}";
	size_t expr_len = strlen(expr);
	Handle htext = nil;

	if (!newemptyhandle(&htext))
		return;
	if (!sethandlesize(htext, (long)expr_len)) {
		disposehandle(htext);
		return;
	}
	HLock(htext);
	memcpy(*htext, expr, expr_len);
	HUnlock(htext);

	tyvaluerecord val;
	boolean flpushpop = !flscriptrunning;

	initvalue(&val, novaluetype);

	if (flpushpop)
		flpushpop = pushprocess(nil);

	boolean ok = langrun(htext, &val);	/* consumes htext */

	if (flpushpop)
		popprocess();

	/* langrun populates val on both success and failure paths. The
	 * pattern mirrors langrunhandle_value (Common/source/lang.c:920+):
	 * caller must disposevaluerecord regardless of return. Without this,
	 * any string/handle/list payload in val leaks across REPL session
	 * boots. */
	disposevaluerecord(val, false);

	if (!ok) {
		log_warn(LOG_COMP_GENERAL,
		         "REPL menubar install script failed; palette will be empty. "
		         "Legacy /commands continue to work.");
	}
}


/* Blocking REPL loop for non-TTY input (fallback mode).
 * Used when stdin is not a terminal (e.g., piped input).
 *
 * Uses poll()-based input loop that yields the GIL between input checks,
 * mirroring the event loop mode's yield pattern. This allows background
 * threads (TCP callbacks, agents) to run while waiting for input.
 */
static int repl_main_blocking(void) {
	boolean running = true;
	char line_buf[MAX_COMMAND_LEN];

	while (running) {
		// Print prompt to stderr to avoid corrupting captured stdout in piped mode
		fputs(g_repl_prompt, stderr);
		fflush(stderr);

		// Poll-based input loop: yield GIL while waiting for input
		char *line = NULL;

		while (1) {
			struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
			int ready = poll(&pfd, 1, POLL_TIMEOUT_MS);

			if (ready > 0 && (pfd.revents & (POLLIN | POLLHUP | POLLERR))) {
				// Input available, EOF, or error — fgets handles all cases
				line = fgets(line_buf, sizeof(line_buf), stdin);
				break;
			}

			if (ready < 0 && errno != EINTR) {
				// Unrecoverable poll() error
				log_error(LOG_COMP_GENERAL, "poll() failed in blocking REPL: %s", strerror(errno));
				return 1;
			}

			// No input ready — yield GIL so background threads can run
			tcp_process_callbacks();
			headless_backgroundtask(true);

			if (agentsenabled()) {
				agentscheduler_tick();
			}
		}

		if (line == NULL) {
			if (ferror(stdin)) {
				log_error(LOG_COMP_GENERAL, "stdin read error in blocking REPL: %s", strerror(errno));
			}
			break;
		}

		// Strip trailing newline
		size_t len = strlen(line_buf);
		if (len > 0 && line_buf[len - 1] == '\n')
			line_buf[len - 1] = '\0';

		// Process the line (process_line handles history via linenoiseHistoryAdd)
		process_line(line_buf, &running);

		// Process callbacks after each command
		tcp_process_callbacks();

		// Honor exit requested by repl.exit() via the kernel-verb host adapter
		if (g_repl_exit_requested) {
			running = false;
		}
	}

	return 0;
}

/* Main REPL entry point: runs event loop with non-blocking linenoise.
 * Falls back to blocking mode if stdin is not a TTY.
 */
int repl_main(cli_options_t *options, ws_server_t *ws_server) {
	(void)options;	/* Unused in Phase 1 */

	boolean running = true;
	struct linenoiseState ls;
	char line_buf[MAX_COMMAND_LEN];
	boolean use_event_loop;

	// 1. Initialize linenoise
	if (!init_linenoise()) {
		log_error(LOG_COMP_GENERAL, "Failed to initialize linenoise");
		return 1;
	}

	// 2. Install signal handlers for Ctrl-C and terminal cleanup
	install_signal_handlers();

	// 3. Initialize persistent variables subsystem
	if (!repl_variables_init()) {
		log_warn(LOG_COMP_GENERAL, "Failed to initialize REPL variables, continuing without persistence");
		// Not fatal - we can continue without persistence
	}

	// 3.1 Install host adapter for repl.* kernel verbs (repl.exit etc.).
	//     Reset exit-requested flag so a stale value from a prior session
	//     can't pre-exit this one. Uninstalled on the cleanup paths below.
	g_repl_exit_requested = 0;
	install_repl_verbs_host();

	// 3.2 Boot the REPL menubar via UserTalk. Idempotent -- the install
	//     script guards with menu.isInstalled. Failure logs a warning and
	//     continues; the legacy /commands still work.
	install_repl_menubar();

	// 3.3 Phase C: initialize the static REPL window sentinel in the ODB
	//     and fire the initial "window opened" event.
	//
	//     window_registry_init() creates system.temp.windowTypes.windows.repl
	//     with type = "ReplWindow" so the windowTypes framework can resolve
	//     this window's type entry.
	//
	//     on_frontmost_changed(nil, REPL_PATH) fires idopenwindowscript, which
	//     calls system.callbacks.openWindow(repl_path).  The windowTypes
	//     framework (Frontier.tools.windowTypes.callbacks.openWindow) handles
	//     this call and composes the menu for the REPL window type.
	//
	//     Both calls are boot-failure-safe (ADR-016): they log and continue
	//     on error rather than aborting the REPL.
	if (!window_registry_init()) {
		log_warn(LOG_COMP_GENERAL,
		         "repl_main: window_registry_init failed -- "
		         "REPL window sentinel not created; windowTypes bridge is no-op");
	}
	on_frontmost_changed(nil, WINDOW_BRIDGE_REPL_PATH);

	// 4. Display welcome message and mark REPL as active
	repl_output_welcome();
	g_repl_active = true;
	flreplmode = true;	/* Set runtime flag for msg() prefix behavior */

	// 5. Check if we can use the event loop (requires TTY)
	// The non-blocking linenoise API requires a real terminal for raw mode.
	// FRONTIER_PLAIN_REPL forces the blocking path even on a TTY — used by
	// pexpect-based integration tests that combine this with TERM=dumb to
	// get clean line-buffered I/O without ANSI escape sequences.
	use_event_loop = isatty(STDIN_FILENO) && !getenv("FRONTIER_PLAIN_REPL");

	if (!use_event_loop) {
		// Fall back to blocking mode for non-TTY input
		log_debug(LOG_COMP_GENERAL, "Non-TTY input detected, using blocking REPL mode");
		int result = repl_main_blocking();
		g_repl_active = false;
		flreplmode = false;	 /* Clear runtime flag */
		uninstall_repl_verbs_host();
		repl_variables_cleanup();
		cleanup_linenoise();
		repl_output_goodbye();
		return result;
	}

	// 5. Start non-blocking line editing (only for TTY)
	if (linenoiseEditStart(&ls, STDIN_FILENO, STDOUT_FILENO,
						   line_buf, sizeof(line_buf), g_repl_prompt) == -1) {
		log_error(LOG_COMP_GENERAL, "Failed to start linenoise editing");
		/* Symmetry with the non-TTY return path above (and with the
		 * normal-exit cleanup at the bottom of this function): a host
		 * adapter that survives this exit-1 path would leave verbs
		 * resolving against stale function pointers if the next
		 * caller doesn't re-install. Tear it down here too. */
		g_repl_active = false;
		flreplmode = false;
		uninstall_repl_verbs_host();
		repl_variables_cleanup();
		cleanup_linenoise();
		return 1;
	}

	// Store global pointer for terminal cleanup and async output
	g_active_linenoisestate = &ls;
	repl_set_active_linenoisestate(&ls);

	// 6. Event loop
	while (running) {
		// 6.1 Poll stdin (+ WebSocket sockets if active) with timeout
		struct pollfd pfds[WS_POLL_FDS_COUNT + 1];
		memset(pfds, 0, sizeof(pfds));
		pfds[0].fd = STDIN_FILENO;
		pfds[0].events = POLLIN;
		pfds[0].revents = 0;
		int nfds = 1;
		int ws_start = -1;

		if (ws_server != NULL) {
			ws_start = nfds;
			nfds += ws_server_pollfds(ws_server, pfds, nfds);
		}

		int ready = poll(pfds, (nfds_t)nfds, POLL_TIMEOUT_MS);

		// 6.1.1 Handle WebSocket events (before stdin to avoid latency)
		// GIL is held here — ws_server_handle_events calls op_dispatch which requires it.
		if (ready > 0 && ws_server != NULL && ws_start >= 0) {
			ws_server_handle_events(ws_server, pfds, ws_start);
		}

		// 6.2 Feed input to linenoise if available
		if (ready > 0 && (pfds[0].revents & POLLIN)) {
			char *result = linenoiseEditFeed(&ls);

			if (result == linenoiseEditMore) {
				/* User is still editing — but check the slash-menu
				 * trigger: '/' typed at column 1 of an empty buffer
				 * (the editor's len jumped to 1 with buf[0] == '/'
				 * exactly). This is the only condition that opens the
				 * palette; '/' typed mid-line is left as a literal
				 * character.
				 *
				 * slash-menu disambiguator (PR #674): we cannot open
				 * the menu immediately on '/' alone, because the user
				 * may be typing a slash command like "/help" or
				 * "/keycodes". Wait up to slash_menu_trigger_delay_ms()
				 * for a follow-up byte:
				 *   - timeout (no byte)            -> open menu
				 *   - follow-up byte is '/'        -> open menu (fast
				 *     path for power users: "//" skips the wait). The
				 *     leading '/' is removed from the buffer before
				 *     the modal opens, since the modal expects a clean
				 *     prompt row.
				 *   - any other follow-up byte     -> NOT a menu
				 *     trigger; this is the second char of a slash
				 *     command. Inject the byte into the linenoise
				 *     edit buffer (linenoiseEditInsert, see forward
				 *     decl) so it echoes and the buffer state matches
				 *     what the user sees. The leading '/' stays put.
				 *
				 * The fast-timer env var FRONTIER_PALETTE_FAST_TIMERS
				 * compresses 350ms -> 5ms for the L4 test harness, so
				 * pyte-driven tests don't pay the full disambiguation
				 * cost on each '/' send. */
				bool open_menu = false;
				if (ls.len == 1 && ls.buf[0] == '/') {
					struct pollfd dfd = { STDIN_FILENO, POLLIN, 0 };
					int dready = poll(&dfd, 1, slash_menu_trigger_delay_ms());
					if (dready == 0) {
						/* Timeout — user typed '/' and stopped. */
						open_menu = true;
					} else if (dready > 0 && (dfd.revents & POLLIN)) {
						unsigned char nb;
						ssize_t nn = read(STDIN_FILENO, &nb, 1);
						if (nn == 1 && nb == '/') {
							/* '//' fast-path: clear leading '/' from
							 * buffer so the modal opens against a
							 * clean prompt row, matching the timeout
							 * path's invariant. */
							ls.buf[0] = '\0';
							ls.len = 0;
							ls.pos = 0;
							open_menu = true;
						} else if (nn == 1) {
							/* Some other byte arrived during the
							 * disambig window. We've consumed it from
							 * stdin and need to give it back to the
							 * user via linenoise's edit state. Not all
							 * bytes are insertable — backspace must
							 * delete (not insert as literal 0x7f);
							 * control chars (Enter, Ctrl-C, ESC, etc.)
							 * have semantics linenoise's normal switch
							 * handles, which we can't fully replay
							 * here without duplicating that switch.
							 * Handle the common typing-correction case
							 * (backspace) explicitly; for any other
							 * control char, drop the byte and leave
							 * the leading '/' in the buffer so the
							 * user can finish typing or backspace it
							 * away. Printable chars insert normally. */
							if (nb == 0x7f || nb == 0x08) {
								/* Backspace / Ctrl-H: delete the
								 * leading '/'. linenoiseEditBackspace
								 * handles the echo + buffer update. */
								linenoiseEditBackspace(&ls);
							} else if (nb >= 0x20 && nb != 0x7f) {
								/* Printable byte — second char of a
								 * slash command. Inject into linenoise
								 * so it echoes and the buffer state
								 * matches what the user sees. */
								char cb = (char)nb;
								(void)linenoiseEditInsert(&ls, &cb, 1);
							}
							/* else: control byte (Enter, Ctrl-C, ESC,
							 * tab, etc.). Dropping is the safe choice
							 * — leaves '/' in the buffer; user can
							 * backspace or retype. Rare in practice
							 * (who types '/' then Enter?). */
						}
						/* nn <= 0 (EOF or error): treat as timeout —
						 * fall through with open_menu = false; the
						 * next loop iteration will hit the EOF path
						 * in linenoiseEditFeed and exit cleanly. */
					} else if (dready < 0 && errno == EINTR) {
						/* Signal interrupted the poll. Don't open the
						 * menu; let the outer loop's signal handling
						 * (Ctrl-C, SIGWINCH) run on the next pass. */
					}
				}
				if (open_menu) {
					char palette_arg[PALETTE_ARG_MAX] = "";
					/* run_palette_modal returns with linenoise STOPPED
					 * (it called linenoiseEditStop on entry) and the
					 * cursor anchored at column 1 of a fresh row via the
					 * \r\n emit at the end of its cleanup. Leaf
					 * dispatch -- if any -- writes its output starting
					 * there, then we restart linenoise below so the next
					 * prompt redraws cleanly. Mirrors the Enter-key
					 * path: EditStop -> process line -> EditStart. */
					Handle script = run_palette_modal(&ls, line_buf,
					                                  sizeof(line_buf),
					                                  palette_arg,
					                                  sizeof(palette_arg));
					if (script != nil) {
						/* Dispatch the chosen menu item.
						 *
						 * Ownership: meuserselected_headless does its own
						 * internal copyhandle() (see Common/source/
						 * menudata_headless.c:1056) — it does NOT consume
						 * the caller's handle. The caller therefore
						 * retains ownership and MUST disposehandle on
						 * BOTH the success and failure paths.
						 *
						 * Argument injection: if the palette captured a
						 * non-empty arg (item had accepts_args=true and
						 * the user typed in the input row), synthesize a
						 * new script source with the typed arg bound as
						 * a string literal in the leaf's call expression.
						 * The arg is bound at compile time inside the
						 * call frame — no global hand-off, no GIL-yield
						 * window for sibling threads. See
						 * palette_arg_inject.h for the algorithm and
						 * threat model. */
						boolean ok = false;
						bool arg_present = (palette_arg[0] != '\0');
						bool synth_used = false;
						if (arg_present) {
							char escaped[PALETTE_ARG_MAX * 2 + 4];
							palette_arg_escape_result_t esc_r =
								palette_arg_escape(palette_arg, escaped, sizeof(escaped));
							if (esc_r == PALETTE_ARG_ESCAPE_OK) {
								size_t script_len = 0;
								char *script_cstr = script_handle_to_cstr(script, &script_len);
								if (script_cstr != NULL) {
									char *synth = NULL;
									size_t synth_len = 0;
									if (palette_arg_inject_into_script(script_cstr,
									                                   script_len, escaped,
									                                   &synth, &synth_len)) {
										ty_dispatch_result dr = dispatch_synthesized_script(synth, synth_len);
										ok = (dr == DISPATCH_OK);
										free(synth);
										synth_used = true;
									}
									free(script_cstr);
								}
							} else {
								/* Issue #595: distinguish the two
								 * failure modes so the user sees an
								 * actionable diagnostic. Skip dispatch
								 * either way — the user can re-open
								 * the palette and re-type. */
								switch (esc_r) {
									case PALETTE_ARG_ESCAPE_FORBIDDEN_BYTE:
										printf("(palette argument contains a forbidden character)\n");
										break;
									case PALETTE_ARG_ESCAPE_OVERFLOW:
										printf("(palette argument is too long)\n");
										break;
									case PALETTE_ARG_ESCAPE_OK:
										break;       /* unreachable */
								}
								fflush(stdout);
								synth_used = true;       /* skip the no-arg fallback */
								ok = true;               /* not a script failure per se */
							}
						}
						if (!synth_used) {
							/* No arg or synthesis declined — dispatch
							 * the leaf's stored script as-is. */
							g_script_running = 1;
							ok = meuserselected_headless(script);
							g_script_running = 0;
						}
						if (!ok) {
							printf("(menu script failed)\n");
							fflush(stdout);
						}
						disposehandle(script);
					}
					/* Restart linenoise so the prompt redraws after the
					 * modal (and any leaf dispatch above) is fully done.
					 * Failure mode mirrors the Enter-key path's restart
					 * below: a partially-initialised ls is unsafe to feed,
					 * so we set g_repl_exit_requested and break out before
					 * the next linenoiseEditFeed dereferences invalid
					 * state. */
					if (running) {
						if (linenoiseEditStart(&ls, STDIN_FILENO, STDOUT_FILENO,
						                       line_buf, sizeof(line_buf),
						                       g_repl_prompt) == -1) {
							log_error(LOG_COMP_GENERAL,
							          "palette: failed to restart linenoise after modal");
							g_repl_exit_requested = 1;
						}
					}
					if (g_repl_exit_requested) {
						running = false;
						break;
					}
				}
			} else if (result != NULL) {
				// User pressed Enter - stop line editing first (prints newline)
				linenoiseEditStop(&ls);

				// Process the line
				process_line(result, &running);
				linenoiseFree(result);

				if (running) {
					// Restart line editing for next command
					if (linenoiseEditStart(&ls, STDIN_FILENO, STDOUT_FILENO,
										   line_buf, sizeof(line_buf), g_repl_prompt) == -1) {
						log_error(LOG_COMP_GENERAL, "Failed to restart linenoise editing");
						running = false;
						break;	// Exit immediately - linenoise state is invalid
					}
				}
			} else {
				// EOF (Ctrl-D) or error
				running = false;
			}
		} else if (ready < 0) {
			// poll() error - check errno to determine if recoverable
			if (errno == EINTR) {
				// Interrupted by signal - check if it was Ctrl-C
				if (g_repl_interrupt_requested) {
					handle_interrupt(&ls, line_buf, sizeof(line_buf));
				}
				// Otherwise continue (e.g., SIGWINCH for terminal resize)
			} else {
				// Unrecoverable poll() error (EBADF, ENOMEM, etc.)
				log_error(LOG_COMP_GENERAL, "poll() failed: %s", strerror(errno));
				linenoiseEditStop(&ls);	 // Restore terminal before exiting
				running = false;
				break;	// Exit immediately
			}
		}

		// 6.3 Process TCP callbacks (webserver)
		tcp_process_callbacks();

		// 6.3.1 Yield GIL so callback threads spawned above can execute.
		// Without this, the main thread holds the GIL for the entire idle
		// loop and spawned callback threads deadlock on GIL acquisition.
		headless_backgroundtask(true);

		// 6.4 Run agent scheduler tick (if agents enabled)
		if (agentsenabled()) {
			agentscheduler_tick();
		}

		// 6.5 Check for Ctrl-C flag (in case signal arrived during poll)
		if (g_repl_interrupt_requested) {
			handle_interrupt(&ls, line_buf, sizeof(line_buf));
		}

		// 6.6 Honor exit requested by repl.exit() via the kernel-verb host
		//     adapter. The flag is volatile sig_atomic_t (see declaration),
		//     so the read is well-defined even if a non-main thread (a
		//     thread.new() child holding the GIL) called repl.exit().
		if (g_repl_exit_requested) {
			running = false;
		}
	}

	// 7. Cleanup
	g_repl_active = false;
	flreplmode = false;	 /* Clear runtime flag */
	g_active_linenoisestate = NULL;
	repl_set_active_linenoisestate(NULL);
	linenoiseEditStop(&ls);
	uninstall_repl_verbs_host();
	repl_variables_cleanup();
	cleanup_linenoise();
	repl_output_goodbye();
	return 0;
}
