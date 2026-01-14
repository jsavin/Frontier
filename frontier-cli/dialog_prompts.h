/*
 * dialog_prompts.h - Interactive dialog prompt implementations
 *
 * Provides stdio-based dialog prompts for headless interactive mode.
 * All prompts check isInteractiveMode() before displaying.
 *
 * Functions:
 * - dialog_ask: Yes/No prompt with arrow key selection
 * - dialog_get_int: Integer input with default value
 * - dialog_get_string: String input with default value
 * - dialog_get_password: Password input with hidden characters
 */

#ifndef DIALOG_PROMPTS_H
#define DIALOG_PROMPTS_H

#include <stdbool.h>

/* Yes/No prompt with arrow key selection
 *
 * Displays: "prompt? Yes No" with current selection inverted
 * Arrow keys/Tab switch selection, Enter confirms
 * Returns: true for Yes, false for No
 */
bool dialog_ask(const char *prompt);

/* Integer input prompt with default value
 *
 * Displays: "prompt [default]: "
 * Enter accepts default, typed input overrides
 * Re-prompts if input is not a valid integer
 * Returns: integer value, or 0 on error
 */
long dialog_get_int(const char *prompt, long default_value);

/* String input prompt with default value
 *
 * Displays: "prompt [default]: "
 * Enter accepts default, typed input overrides
 * Returns: newly allocated string (caller must free), or NULL on error
 */
char* dialog_get_string(const char *prompt, const char *default_value);

/* Password input prompt (no echo)
 *
 * Displays: "prompt: "
 * Shows bullet points (•) for each character typed
 * Backspace removes last bullet
 * Returns: newly allocated string (caller must free), or NULL on error
 */
char* dialog_get_password(const char *prompt);

#endif /* DIALOG_PROMPTS_H */
