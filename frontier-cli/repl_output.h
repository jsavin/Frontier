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

/*
 * Display a rich, structured error message (PR2 of REPL error context chain).
 *
 * Reads the snapshot captured by PR1's langseterrorcallbackline (via
 * langgetlasterror / langgetstackdepth / langgetstackframe) and renders:
 *
 *   - header line with the error message
 *   - one "at <script> line N" line per stack frame, failure site first
 *   - a source-context window for each frame: the failing line plus up
 *     to 7 lines of leading/trailing context when the source is multi-line
 *   - the error line marked: ANSI bold + underline of the failing token
 *     range in TTY mode, ">>>" line prefix + "^^^" caret line in plain
 *     mode
 *
 * TTY detection is isatty(STDERR_FILENO), with FRONTIER_FORCE_COLOR=1 as
 * an override (lets users force color when piping to a pager and lets
 * tests cover both modes from non-interactive runs).
 *
 * Parameters:
 *   error_msg          - human-readable error string (may be empty/NULL;
 *                        a "(no message)" placeholder is used)
 *   eval_source_text   - the user's source text for the outermost <eval>
 *                        frame (the line the user just typed). May be
 *                        NULL if unavailable; in that case the eval
 *                        frame renders with the header line only.
 *
 * If no structured snapshot is available (langgetlasterror returns
 * false), this falls back to the plain repl_output_error behavior so
 * callers can safely use this entry point unconditionally on the error
 * path.
 */
void repl_output_structured_error(const char *error_msg,
                                  const char *eval_source_text);

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
