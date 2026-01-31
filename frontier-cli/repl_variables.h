/*
 * repl_variables.h - REPL persistent variable management
 *
 * Manages the system.temp.FrontierREPL table structure for:
 * - variables: Persistent variables across REPL evaluations
 * - target: Current target address for target.set()/target.clear()
 * - focus: Mirrors /jump location, exposed to UserTalk
 * - commands: Custom slash command scripts
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef REPL_VARIABLES_H
#define REPL_VARIABLES_H

#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"

/*
 * Initialize the REPL variables subsystem.
 * Creates system.temp.FrontierREPL with subtables:
 * - variables: for persistent REPL variables
 * - target: for target.set()/clear()
 * - focus: mirrors /jump location
 * - commands: custom slash command scripts
 *
 * Call once at REPL startup.
 * Returns true on success.
 */
boolean repl_variables_init(void);

/*
 * Clean up the REPL variables subsystem.
 * Called at REPL exit. Currently a no-op (variables persist in system.temp).
 */
void repl_variables_cleanup(void);

/*
 * Get the variables table handle.
 * Returns system.temp.FrontierREPL.variables, or nil if not initialized.
 */
hdlhashtable repl_get_variables_table(void);

/*
 * Evaluate a script with persistent variable support.
 *
 * Wraps the user's script with:
 *   with system.temp.FrontierREPL.variables { <user code> }
 *
 * After evaluation, syncs any new/modified variables from the
 * execution frame back to system.temp.FrontierREPL.variables.
 *
 * Parameters:
 *   script     - UserTalk script to execute (null-terminated C string)
 *   result     - OUT: Result as string (bigstring)
 *   error_msg  - OUT: Error message if execution failed (bigstring)
 *
 * Returns: true if evaluation succeeded, false on error
 */
boolean repl_eval_with_variables(
    const char *script,
    bigstring result,
    bigstring error_msg
);

/*
 * Update the focus address to match the current /jump path.
 * Called by /jump command after successful navigation.
 *
 * Parameters:
 *   path - The resolved path (e.g., "system.verbs.builtins")
 *          Empty string means root.
 */
void repl_set_focus(const char *path);

#endif /* REPL_VARIABLES_H */
