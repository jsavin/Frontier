/*
 * repl_eval.h - REPL evaluation engine (workspace management and script execution)
 *
 * Part of Frontier REPL interactive mode (Phase 1).
 * Manages workspace table lifecycle and executes UserTalk scripts in REPL context.
 *
 * Reference: planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md
 *
 * Created: 2026-01-13
 */

#ifndef REPL_EVAL_H
#define REPL_EVAL_H

#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"

/*
 * repl_workspace_t - Persistent workspace for REPL variables
 *
 * The workspace table persists across all REPL evaluations in a session.
 * Variables declared at the REPL prompt are stored here.
 *
 * NOT thread-safe - CLI is single-threaded in Phase 1.
 */
typedef struct repl_workspace_t {
    hdlhashtable workspace_table;  /* Persistent workspace table */
    boolean initialized;            /* Workspace initialized flag */
} repl_workspace;

/*
 * repl_workspace_init - Initialize workspace (call once at REPL startup)
 *
 * Creates 'workspace' as a top-level table in the system root.
 * If no system root is loaded, this will fail.
 *
 * Returns: true on success, false on error
 */
boolean repl_workspace_init(repl_workspace *ws);

/*
 * repl_workspace_cleanup - Clean up workspace (call at REPL exit)
 *
 * Does NOT dispose the workspace table (it's part of system root).
 * Just marks workspace as uninitialized.
 */
void repl_workspace_cleanup(repl_workspace *ws);

/*
 * repl_workspace_clear - Clear workspace (for /clear command)
 *
 * Removes all variables from workspace table without disposing the table itself.
 *
 * Returns: true on success, false on error
 */
boolean repl_workspace_clear(repl_workspace *ws);

/*
 * repl_eval_script - Evaluate UserTalk script in workspace context
 *
 * Compiles and executes the script with workspace as the current table context.
 * Variables declared in the script are stored in workspace.
 *
 * Parameters:
 *   ws         - Workspace context
 *   script     - UserTalk script to execute (null-terminated C string)
 *   result     - OUT: Result as string (bigstring)
 *   error_msg  - OUT: Error message if execution failed (bigstring)
 *
 * Returns: true if evaluation succeeded, false on error
 *
 * On success: result contains string representation of return value (may be empty)
 * On error: error_msg contains error description
 *
 * Note: Uses langrunhandle() for compilation and execution.
 *       Error messages come from Frontier's error system.
 */
boolean repl_eval_script(
    repl_workspace *ws,
    const char *script,
    bigstring result,      /* OUT: result as string */
    bigstring error_msg    /* OUT: error message if failed */
);

#endif /* REPL_EVAL_H */
