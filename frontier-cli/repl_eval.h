/*
 * repl_eval.h - REPL evaluation engine (QuickScript model)
 *
 * Part of Frontier REPL interactive mode (Phase 1).
 * Evaluates UserTalk scripts following QuickScript model from legacy Frontier.
 *
 * Reference: planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md
 *
 * Created: 2026-01-13
 * Updated: 2026-01-14 - Refactored to QuickScript model (removed workspace mechanism)
 */

#ifndef REPL_EVAL_H
#define REPL_EVAL_H

#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"

/*
 * repl_eval_script - Evaluate UserTalk script (QuickScript model)
 *
 * Compiles and executes the script. Each evaluation runs independently in its own
 * thread context. Local variables are thread-scoped and cleaned up automatically
 * after evaluation completes.
 *
 * This follows the QuickScript model from legacy Frontier - no implicit persistence
 * between evaluations. Users who want persistent data use explicit database paths:
 * - system.temp.x (session-scoped, cleared on exit)
 * - workspace.x or other root tables (disk-scoped, saved with database)
 *
 * Parameters:
 *	 script		- UserTalk script to execute (null-terminated C string)
 *	 result		- OUT: Result as string (bigstring)
 *	 error_msg	- OUT: Error message if execution failed (bigstring)
 *
 * Returns: true if evaluation succeeded, false on error
 *
 * On success: result contains string representation of return value (may be empty)
 * On error: error_msg contains error description
 *
 * Note: Uses langrunhandletraperror() for compilation and execution.
 *		 Error messages come from Frontier's error system.
 */
boolean repl_eval_script(
	const char *script,
	bigstring result,	   /* OUT: result as string */
	bigstring error_msg	   /* OUT: error message if failed */
);

#endif /* REPL_EVAL_H */
