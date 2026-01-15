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
 * repl_workspace_t - Ephemeral workspace for REPL variables
 *
 * The workspace table is ephemeral (thread-local, not persisted).
 * Variables declared at the REPL prompt are stored here.
 *
 * The ephemeral workspace is isolated from root.workspace - /clear never
 * touches persisted data.
 *
 * NOT thread-safe - CLI is single-threaded in Phase 1.
 */
typedef struct repl_workspace_t {
    hdlhashtable workspace_table;  /* Ephemeral workspace table */
    boolean initialized;            /* Workspace initialized flag */
} repl_workspace;

/*
 * repl_workspace_init - Initialize workspace (call once at REPL startup)
 *
 * Creates ephemeral thread-local hash table for REPL variables.
 * This table is NOT persisted and is isolated from root.workspace.
 *
 * Returns: true on success, false on error
 */
boolean repl_workspace_init(repl_workspace *ws);

/*
 * repl_workspace_cleanup - Clean up workspace (call at REPL exit)
 *
 * Disposes the ephemeral workspace table and marks as uninitialized.
 */
void repl_workspace_cleanup(repl_workspace *ws);

/*
 * repl_workspace_clear - Clear workspace (for /clear command)
 *
 * Removes all variables from the ephemeral workspace table.
 * Refuses to clear non-local tables (safety check to prevent data loss).
 *
 * Returns: true on success, false on error
 */
boolean repl_workspace_clear(repl_workspace *ws);

/*
 * repl_eval_script - Evaluate UserTalk script in workspace context
 *
 * Compiles and executes the script with ephemeral workspace as the current table context.
 * Variables declared in the script (simple names) are stored in ephemeral workspace.
 * Dotted paths (root.workspace.x) access persisted database tables.
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
 * Note: Uses langrunhandletraperror() for compilation and execution.
 *       Error messages come from Frontier's error system.
 */
boolean repl_eval_script(
    repl_workspace *ws,
    const char *script,
    bigstring result,      /* OUT: result as string */
    bigstring error_msg    /* OUT: error message if failed */
);

#endif /* REPL_EVAL_H */
