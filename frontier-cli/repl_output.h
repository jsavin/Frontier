/*
 * repl_output.h - REPL output formatter interface
 *
 * Responsible for displaying UserTalk values, errors, prompts, and help text
 * in the REPL interactive mode.
 *
 * Phase 1 implementation: Basic value display using coercetostring()
 * Phase 4 addition: Async output for event loop (linenoiseHide/Show)
 */

#ifndef REPL_OUTPUT_H
#define REPL_OUTPUT_H

#include <stdint.h>
#include <stdio.h>
#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"

/* Mac Roman to Unicode lookup table for bytes 0x80-0xFF.
 * Index with (byte - 0x80) to get the Unicode code point. */
extern const uint16_t kMacRomanHighToUnicode[128];

/* Write a Unicode code point as UTF-8 to a stream. */
void putc_utf8(uint16_t cp, FILE *stream);

/* Forward declaration for linenoise state */
struct linenoiseState;

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

/* Display contents of a table (for /list command)
 * Shows name, type, and display string (N items or "on disk") for each entry.
 * Output is formatted in aligned columns.
 * If htable is nil, displays the current REPL table.
 * path_label is displayed as a header (e.g., "user.prefs:"). If NULL, uses current path.
 */
void repl_output_list(hdlhashtable htable, const char *path_label);

/* Display a single scalar value (for /list with index syntax).
 * Used when /list resolves to a non-table value via [n] indexing.
 * Shows: path = value (type)
 */
void repl_output_single_value(const char *path_label, tyvaluerecord *val);

/* --- Event Loop Support (Phase 4) --- */

/* Set the active linenoise state for async output.
 * Call this when entering/leaving the event loop.
 * Pass NULL to disable async output mode.
 */
void repl_set_active_linenoisestate(struct linenoiseState *ls);

/* Display async output while user is typing at prompt.
 * Uses linenoiseHide/Show to preserve the user's current input.
 * If not in event loop mode (linenoisestate is NULL), prints directly.
 */
void repl_async_output(const char *message);

#endif /* REPL_OUTPUT_H */
