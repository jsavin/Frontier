/*
    repl_variables.h - REPL persistent variable management

    Manages the system.temp.FrontierREPL table structure for:
    - variables: Persistent variables across REPL evaluations
    - target: Current target address for target.set()/target.clear()
    - focus: Mirrors /jump location, exposed to UserTalk
    - commands: Custom slash command scripts

    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-present Frontier contributors

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
 *	 with system.temp.FrontierREPL.variables { <user code> }
 *
 * After evaluation, syncs any new/modified variables from the
 * execution frame back to system.temp.FrontierREPL.variables.
 *
 * Parameters:
 *	 script		- UserTalk script to execute (null-terminated C string)
 *	 result		- OUT: Result as string (bigstring)
 *	 error_msg	- OUT: Error message if execution failed (bigstring)
 *
 * Returns: true if evaluation succeeded, false on error
 */
boolean repl_eval_with_variables(
	const char *script,
	bigstring result,
	bigstring error_msg
);

/*
 * Execute a UserTalk script and return the result as a tyvaluerecord.
 * Unlike repl_eval_with_variables(), this does not truncate strings at 255 chars.
 *
 * Parameters:
 *	 script	   - IN:  The UserTalk script to execute (C string)
 *	 vreturned - OUT: The result value (caller must dispose with disposevaluerecord)
 *	 error_msg - OUT: Error message if execution failed (bigstring)
 *
 * Returns: true if evaluation succeeded, false on error
 */
boolean repl_eval_with_variables_value(
	const char *script,
	tyvaluerecord *vreturned,
	bigstring error_msg
);

/*
 * Update the focus address to match the current /jump table.
 * Called by /jump command after successful navigation.
 *
 * Parameters:
 *	 htable - The table to set as focus. If nil, sets focus to roottable.
 */
void repl_set_focus(hdlhashtable htable);

#endif /* REPL_VARIABLES_H */
