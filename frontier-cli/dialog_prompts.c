/*
 * dialog_prompts.c - Interactive dialog prompt implementations
 *
 * Stdio-based prompts using terminal control utilities.
 * Thread-safe: saves/restores terminal state for each operation.
 */

#include "dialog_prompts.h"
#include "terminal_control.h"
#include "cli_utils.h"
#include "boxen_ui.h"		/* 2026-06-23 JES #691 Phase C.0.7g */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* Returns true when the terminal is "dumb" (e.g., pexpect PTY for
 * integration testing).  On dumb terminals we skip raw mode and use
 * simple line-buffered fgets instead — raw mode's character-by-character
 * reads don't work well with automated expect/send test drivers. */
static bool is_dumb_terminal(void) {
	char *term = getenv("TERM");
	return (term && strcasecmp(term, "dumb") == 0);
}

/* Simple fgets-based line reader for dumb terminals and testing.
 * Used when raw mode is unavailable or unnecessary (e.g., TERM=dumb
 * for pexpect-based integration tests). */
static char* read_line_simple(void) {
	char buf[4096];
	if (fgets(buf, sizeof(buf), stdin) == NULL) {
		return NULL;
	}
	/* Strip trailing newline */
	size_t len = strlen(buf);
	while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
		buf[--len] = '\0';
	}
	return strdup(buf);
}

/* Internal helper: read a line with line editing support */
static char* read_line_with_editing(void) {
	char *buffer = NULL;
	size_t bufsize = 0;
	size_t len = 0;
	terminal_state *term_state = NULL;

	if (is_dumb_terminal()) {
		return read_line_simple();
	}

	/* Save terminal state and enable raw mode */
	term_state = terminal_save_state();
	if (!term_state || !terminal_set_raw_mode()) {
		if (term_state) {
			terminal_restore_state(term_state);
			terminal_free_state(term_state);
		}
		return read_line_simple();
	}

	bufsize = 128;
	buffer = (char*)malloc(bufsize);
	if (!buffer) {
		terminal_restore_state(term_state);
		terminal_free_state(term_state);
		return NULL;
	}

	while (1) {
		key_input key = terminal_read_key();

		switch (key.type) {
			case KEY_ENTER:
				/* Add null terminator and return */
				buffer[len] = '\0';
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return buffer;

			case KEY_ESCAPE:
			case KEY_CTRL_C:
			case KEY_CTRL_D:
				/* Cancel input */
				fputs("\n", stderr);
				free(buffer);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return NULL;

			case KEY_BACKSPACE:
				/* Remove last character */
				if (len > 0) {
					len--;
					fputs("\b \b", stderr);
					fflush(stderr);
				}
				break;

			case KEY_CHAR:
				/* Add character to buffer */
				if (len + 1 >= bufsize) {
					/* Grow buffer */
					bufsize *= 2;
					char *newbuf = (char*)realloc(buffer, bufsize);
					if (!newbuf) {
						free(buffer);
						terminal_restore_state(term_state);
						terminal_free_state(term_state);
						return NULL;
					}
					buffer = newbuf;
				}

				buffer[len++] = key.ch;
				fputc(key.ch, stderr);
				fflush(stderr);
				break;

			default:
				/* Ignore other keys */
				break;
		}
	}
}

/* Displays a yes/no prompt with arrow key selection, returns true for Yes. */
bool dialog_ask(const char *prompt) {
	return dialog_twoway(prompt, "Yes", "No");
}

/* Prompts for an integer value with a default; validates input before returning.
   Returns true if user entered a value, false if cancelled (Esc/Ctrl+C).
   On success, *out_value is set. On cancel, *out_value is unchanged. */
bool dialog_get_int(const char *prompt, long default_value, long *out_value) {
	char *input = NULL;
	long result = 0;
	char *endptr;

	/* 2026-06-23 JES #691 Phase C.0.7g: when running under the boxen
	 * REPL, route the prompt through the boxen modal bridge instead of
	 * raw terminal IO (which the boxen compositor would render
	 * incorrectly via the captured stderr pipe). */
	if (boxen_ui_is_active()) {
		return boxen_ui_get_int(prompt, default_value, out_value);
	}

	if (!isInteractiveMode()) {
		return false;
	}

	while (1) {
		/* Display prompt with default */
		fprintf(stderr, "%s [%ld]: ", prompt, default_value);
		fflush(stderr);

		input = read_line_with_editing();
		if (!input) {
			return false;  /* Esc/Ctrl+C - cancelled */
		}

		/* Empty input - accept default */
		if (input[0] == '\0') {
			free(input);
			*out_value = default_value;
			return true;
		}

		/* Try to parse integer */
		errno = 0;
		result = strtol(input, &endptr, 10);

		/* Check for valid integer */
		if (errno == 0 && *endptr == '\0') {
			free(input);
			*out_value = result;
			return true;
		}

		/* Invalid input - re-prompt */
		fputs("Invalid integer. Try again.\n", stderr);
		free(input);
	}
}

/* Prompts for a string value with a default; caller must free returned string. */
char* dialog_get_string(const char *prompt, const char *default_value) {
	char *input = NULL;

	/* 2026-06-23 JES #691 Phase C.0.7g: boxen modal bridge -- see
	 * boxen_ui.c.  When inactive (linenoise mode) falls through to the
	 * existing fprintf+read_line_with_editing path below. */
	if (boxen_ui_is_active()) {
		return boxen_ui_get_string(prompt, default_value);
	}

	if (!isInteractiveMode()) {
		return NULL;
	}

	/* Display prompt with default */
	if (default_value && default_value[0]) {
		fprintf(stderr, "%s [%s]: ", prompt, default_value);
	} else {
		fprintf(stderr, "%s: ", prompt);
	}
	fflush(stderr);

	input = read_line_with_editing();
	if (!input) {
		return NULL;  /* Ctrl+C or error */
	}

	/* Empty input - return default */
	if (input[0] == '\0') {
		free(input);
		if (default_value) {
			return strdup(default_value);
		}
		return strdup("");
	}

	return input;
}

/* Prompts for a password with masked input (asterisks); caller must free returned string. */
char* dialog_get_password(const char *prompt) {
	char *buffer = NULL;
	size_t bufsize = 0;
	size_t len = 0;
	terminal_state *term_state = NULL;

	/* 2026-06-23 JES #691 Phase C.0.7g: boxen modal bridge -- see
	 * boxen_ui.c. */
	if (boxen_ui_is_active()) {
		return boxen_ui_get_password(prompt);
	}

	if (!isInteractiveMode()) {
		return NULL;
	}

	/* Display prompt */
	fprintf(stderr, "%s: ", prompt);
	fflush(stderr);

	if (is_dumb_terminal()) {
		return read_line_simple();
	}

	/* Save terminal state and enable raw mode */
	term_state = terminal_save_state();
	if (!term_state || !terminal_set_raw_mode()) {
		if (term_state) {
			terminal_restore_state(term_state);
			terminal_free_state(term_state);
		}
		return NULL;
	}

	bufsize = 128;
	buffer = (char*)malloc(bufsize);
	if (!buffer) {
		terminal_restore_state(term_state);
		terminal_free_state(term_state);
		return NULL;
	}

	while (1) {
		key_input key = terminal_read_key();

		switch (key.type) {
			case KEY_ENTER:
				/* Add null terminator and return */
				buffer[len] = '\0';
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return buffer;

			case KEY_ESCAPE:
			case KEY_CTRL_C:
			case KEY_CTRL_D:
				/* Cancel input */
				fputs("\n", stderr);
				free(buffer);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return NULL;

			case KEY_BACKSPACE:
				/* Remove last character and bullet */
				if (len > 0) {
					len--;
					fputs("\b \b", stderr);
					fflush(stderr);
				}
				break;

			case KEY_CHAR:
				/* Add character to buffer, display bullet */
				if (len + 1 >= bufsize) {
					/* Grow buffer */
					bufsize *= 2;
					char *newbuf = (char*)realloc(buffer, bufsize);
					if (!newbuf) {
						free(buffer);
						terminal_restore_state(term_state);
						terminal_free_state(term_state);
						return NULL;
					}
					buffer = newbuf;
				}

				buffer[len++] = key.ch;

				/* Display asterisk for password masking (ASCII, safe single byte) */
				fputc('*', stderr);
				fflush(stderr);
				break;

			default:
				/* Ignore other keys */
				break;
		}
	}
}

/* Displays an alert message with audible beep, waits for Enter to continue. */
bool dialog_alert(const char *message) {
	/* 2026-06-23 JES #691 Phase C.0.7g Phase 2A: boxen UI bridge. */
	if (boxen_ui_is_active()) {
		return boxen_ui_alert(message, true /* beep */);
	}

	if (!isInteractiveMode()) {
		return false;
	}

	/* Sound beep */
	fputc('\a', stderr);
	fflush(stderr);

	/* Display message */
	fprintf(stderr, "%s\n", message);
	fprintf(stderr, "[Press Enter to continue]");
	fflush(stderr);

	if (is_dumb_terminal()) {
		getchar();
		fputs("\n", stderr);
		return true;
	}

	/* Wait for Enter */
	terminal_state *term_state = terminal_save_state();
	if (!term_state || !terminal_set_raw_mode()) {
		if (term_state) {
			terminal_restore_state(term_state);
			terminal_free_state(term_state);
		}
		/* Fall back to getchar if raw mode fails */
		getchar();
		fputs("\n", stderr);
		return true;
	}

	/* Wait for Enter key */
	while (1) {
		key_input key = terminal_read_key();
		if (key.type == KEY_ENTER) {
			fputs("\n", stderr);
			break;
		}
		if (key.type == KEY_CTRL_C || key.type == KEY_CTRL_D) {
			fputs("\n", stderr);
			break;
		}
	}

	terminal_restore_state(term_state);
	terminal_free_state(term_state);
	return true;
}

/* Displays a notification message without beep, waits for Enter to continue. */
bool dialog_notify(const char *message) {
	/* 2026-06-23 JES #691 Phase C.0.7g Phase 2A: boxen UI bridge. */
	if (boxen_ui_is_active()) {
		return boxen_ui_alert(message, false /* no beep */);
	}

	if (!isInteractiveMode()) {
		return false;
	}

	/* Display message */
	fprintf(stderr, "%s\n", message);
	fprintf(stderr, "[Press Enter to continue]");
	fflush(stderr);

	if (is_dumb_terminal()) {
		getchar();
		fputs("\n", stderr);
		return true;
	}

	/* Wait for Enter */
	terminal_state *term_state = terminal_save_state();
	if (!term_state || !terminal_set_raw_mode()) {
		if (term_state) {
			terminal_restore_state(term_state);
			terminal_free_state(term_state);
		}
		/* Fall back to getchar if raw mode fails */
		getchar();
		fputs("\n", stderr);
		return true;
	}

	/* Wait for Enter key */
	while (1) {
		key_input key = terminal_read_key();
		if (key.type == KEY_ENTER) {
			fputs("\n", stderr);
			break;
		}
		if (key.type == KEY_CTRL_C || key.type == KEY_CTRL_D) {
			fputs("\n", stderr);
			break;
		}
	}

	terminal_restore_state(term_state);
	terminal_free_state(term_state);
	return true;
}

/* Helper: draw a numbered button, inverted if selected */
static void draw_button(int number, const char *label, bool selected) {
	if (selected) {
		fprintf(stderr, "\033[7m%d:%s\033[0m", number, label);
	} else {
		fprintf(stderr, "%d:%s", number, label);
	}
}

/* Helper: find which button index (0-based) matches a typed character.
 * Matches if the first letter of the button label (case-insensitive)
 * equals the typed character.  Returns -1 if no match. */
static int match_button_char(char ch, const char **buttons, int count) {
	char upper = toupper((unsigned char)ch);
	for (int i = 0; i < count; i++) {
		if (buttons[i] && toupper((unsigned char)buttons[i][0]) == upper) {
			return i;
		}
	}
	return -1;
}

/* Displays a two-button choice dialog; returns true if first button selected. */
bool dialog_twoway(const char *prompt, const char *button1, const char *button2) {
	int selection = 0;  /* 0 = button1, 1 = button2 */
	terminal_state *term_state = NULL;
	const char *buttons[2] = { button1, button2 };

	/* 2026-06-23 JES #691 Phase C.0.7g Phase 2A: boxen UI bridge.
	 * Returns 1 for button1, 2 for button2, 0 on Esc/Ctrl-C cancel.
	 * Map back to legacy bool: true iff button1.  Cancel (0) and
	 * button2 (2) both map to false -- this matches the legacy
	 * interactive dialog_twoway behavior, which returns false on Esc
	 * (see dialog_prompts.c:525-530 below). */
	if (boxen_ui_is_active()) {
		int r = boxen_ui_button_select(prompt, buttons, 2);
		return (r == 1);
	}

	if (!isInteractiveMode()) {
		return true;  /* Default to button1 in batch mode */
	}

	if (is_dumb_terminal()) {
		fprintf(stderr, "%s? 1:%s 2:%s [1]: ", prompt, button1, button2);
		fflush(stderr);
		char *input = read_line_simple();
		if (!input || input[0] == '\0' || input[0] == '1') {
			free(input);
			return true;
		}
		bool result = (input[0] != '2');
		free(input);
		return result;
	}

	/* Save terminal state and enable raw mode */
	term_state = terminal_save_state();
	if (!term_state || !terminal_set_raw_mode()) {
		if (term_state) {
			terminal_restore_state(term_state);
			terminal_free_state(term_state);
		}
		return true;  /* Fall back to button1 */
	}

	while (1) {
		fputs("\r\033[K", stderr);
		fprintf(stderr, "%s? ", prompt);
		draw_button(1, button1, selection == 0);
		fputs(" ", stderr);
		draw_button(2, button2, selection == 1);
		fflush(stderr);

		key_input key = terminal_read_key();

		switch (key.type) {
			case KEY_ENTER:
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return (selection == 0);

			case KEY_ESCAPE:
				/* Esc = last button (No/Cancel) */
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return false;

			case KEY_ARROW_LEFT:
			case KEY_ARROW_RIGHT:
			case KEY_TAB:
				selection = 1 - selection;
				break;

			case KEY_CHAR:
				/* Number keys: instant select */
				if (key.ch == '1') {
					fputs("\n", stderr);
					terminal_restore_state(term_state);
					terminal_free_state(term_state);
					return true;
				} else if (key.ch == '2') {
					fputs("\n", stderr);
					terminal_restore_state(term_state);
					terminal_free_state(term_state);
					return false;
				} else {
					/* First-letter match: instant select */
					int match = match_button_char(key.ch, buttons, 2);
					if (match >= 0) {
						fputs("\n", stderr);
						terminal_restore_state(term_state);
						terminal_free_state(term_state);
						return (match == 0);
					}
				}
				break;

			default:
				break;
		}
	}
}

/* Displays a three-button choice dialog; returns 1, 2, or 3 for the selected button. */
int dialog_threeway(const char *prompt, const char *button1, const char *button2, const char *button3) {
	int selection = 0;  /* 0 = button1, 1 = button2, 2 = button3 */
	terminal_state *term_state = NULL;
	const char *buttons[3] = { button1, button2, button3 };

	/* 2026-06-23 JES #691 Phase C.0.7g Phase 2A: boxen UI bridge.
	 * Returns 1/2/3 for the chosen button, or 0 on Esc/Ctrl-C cancel.
	 *
	 * Legacy interactive dialog_threeway has no cancel return path (Esc
	 * beeps and the user must pick a button).  The boxen modal cancel
	 * is a new addition; we surface it as 0 so safety-sensitive scripts
	 * can detect "user bailed" without mis-coercing it into button1
	 * (which by convention is often the destructive default like
	 * "Save").  Callers that always expect 1/2/3 should test `if r > 0`
	 * before branching, or treat 0 as a no-op.  Button labels are
	 * caller-defined, so the bridge cannot infer which button is the
	 * "Cancel" by convention; 0-on-cancel is the only signal-preserving
	 * mapping. */
	if (boxen_ui_is_active()) {
		return boxen_ui_button_select(prompt, buttons, 3);
	}

	if (!isInteractiveMode()) {
		return 1;  /* Default to button1 in batch mode */
	}

	if (is_dumb_terminal()) {
		fprintf(stderr, "%s? 1:%s 2:%s 3:%s [1]: ", prompt, button1, button2, button3);
		fflush(stderr);
		char *input = read_line_simple();
		int result = 1;
		if (input && input[0] >= '1' && input[0] <= '3') {
			result = input[0] - '0';
		}
		free(input);
		return result;
	}

	/* Save terminal state and enable raw mode */
	term_state = terminal_save_state();
	if (!term_state || !terminal_set_raw_mode()) {
		if (term_state) {
			terminal_restore_state(term_state);
			terminal_free_state(term_state);
		}
		return 1;  /* Fall back to button1 */
	}

	while (1) {
		fputs("\r\033[K", stderr);
		fprintf(stderr, "%s? ", prompt);
		draw_button(1, button1, selection == 0);
		fputs(" ", stderr);
		draw_button(2, button2, selection == 1);
		fputs(" ", stderr);
		draw_button(3, button3, selection == 2);
		fflush(stderr);

		key_input key = terminal_read_key();

		switch (key.type) {
			case KEY_ENTER:
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return (selection + 1);

			case KEY_ESCAPE:
				/* No clear cancel target with 3 buttons — beep */
				fputc('\a', stderr);
				fflush(stderr);
				break;

			case KEY_ARROW_LEFT:
				if (selection > 0) selection--;
				break;

			case KEY_ARROW_RIGHT:
			case KEY_TAB:
				if (selection < 2) selection++;
				break;

			case KEY_CHAR:
				/* Number keys: instant select */
				if (key.ch >= '1' && key.ch <= '3') {
					fputs("\n", stderr);
					terminal_restore_state(term_state);
					terminal_free_state(term_state);
					return (key.ch - '0');
				} else {
					/* First-letter match: instant select */
					int match = match_button_char(key.ch, buttons, 3);
					if (match >= 0) {
						fputs("\n", stderr);
						terminal_restore_state(term_state);
						terminal_free_state(term_state);
						return (match + 1);
					}
				}
				break;

			default:
				break;
		}
	}
}
