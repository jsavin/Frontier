/*
 * repl_output.h - REPL output formatter interface
 *
 * Responsible for displaying UserTalk values, errors, prompts, and help text
 * in the REPL interactive mode.
 *
 * Phase 1 implementation: Basic value display using coercetostring()
 */

#ifndef REPL_OUTPUT_H
#define REPL_OUTPUT_H

#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"

/* Display welcome message at REPL startup */
void repl_output_welcome(void);

/* Display goodbye message at REPL exit */
void repl_output_goodbye(void);

/* Display prompt (e.g., "[root]> ")
 * system_root_name: name of loaded system root, or NULL if none
 */
void repl_output_prompt(const char *system_root_name);

/* Display value result from evaluation
 * Uses coercetostring() to convert value to string
 */
void repl_output_value(tyvaluerecord *val);

/* Display result from evaluation (as bigstring)
 * This is the primary result display function for REPL.
 * Empty strings are not displayed (like Python REPL).
 */
void repl_output_result(bigstring result);

/* Display error message */
void repl_output_error(const char *error_msg);

/* Display help text (for /help command) */
void repl_output_help(void);

/* Display workspace variables (for /vars command)
 * workspace: hash table containing workspace variables
 */
void repl_output_vars(hdlhashtable workspace);

#endif /* REPL_OUTPUT_H */
