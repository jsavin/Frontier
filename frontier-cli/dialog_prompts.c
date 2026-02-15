/*
 * dialog_prompts.c - Interactive dialog prompt implementations
 *
 * Stdio-based prompts using terminal control utilities.
 * Thread-safe: saves/restores terminal state for each operation.
 */

#include "dialog_prompts.h"
#include "terminal_control.h"
#include "cli_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* Internal helper: read a line with line editing support */
static char* read_line_with_editing(void) {
	char *buffer = NULL;
	size_t bufsize = 0;
	size_t len = 0;
	terminal_state *term_state = NULL;

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

/* Prompts for an integer value with a default; validates input before returning. */
long dialog_get_int(const char *prompt, long default_value) {
	char *input = NULL;
	long result = 0;
	char *endptr;

	if (!isInteractiveMode()) {
		return 0;
	}

	while (1) {
		/* Display prompt with default */
		fprintf(stderr, "%s [%ld]: ", prompt, default_value);
		fflush(stderr);

		input = read_line_with_editing();
		if (!input) {
			return default_value;  /* Ctrl+C or error - return default */
		}

		/* Empty input - accept default */
		if (input[0] == '\0') {
			free(input);
			return default_value;
		}

		/* Try to parse integer */
		errno = 0;
		result = strtol(input, &endptr, 10);

		/* Check for valid integer */
		if (errno == 0 && *endptr == '\0') {
			free(input);
			return result;
		}

		/* Invalid input - re-prompt */
		fputs("Invalid integer. Try again.\n", stderr);
		free(input);
	}
}

/* Prompts for a string value with a default; caller must free returned string. */
char* dialog_get_string(const char *prompt, const char *default_value) {
	char *input = NULL;

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

	if (!isInteractiveMode()) {
		return NULL;
	}

	/* Display prompt */
	fprintf(stderr, "%s: ", prompt);
	fflush(stderr);

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
	if (!isInteractiveMode()) {
		return false;
	}

	/* Display message */
	fprintf(stderr, "%s\n", message);
	fprintf(stderr, "[Press Enter to continue]");
	fflush(stderr);

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

	if (!isInteractiveMode()) {
		return true;  /* Default to button1 in batch mode */
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

			case KEY_CTRL_C:
			case KEY_CTRL_D:
				/* Cancel = last button (No/Cancel).  Intentional:
				 * Ctrl+C means "abort/cancel", not "accept default". */
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return false;

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

	if (!isInteractiveMode()) {
		return 1;  /* Default to button1 in batch mode */
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
				/* Esc = last button (Cancel) */
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return 3;

			case KEY_ARROW_LEFT:
				if (selection > 0) selection--;
				break;

			case KEY_ARROW_RIGHT:
			case KEY_TAB:
				if (selection < 2) selection++;
				break;

			case KEY_CTRL_C:
			case KEY_CTRL_D:
				/* Cancel = last button.  Intentional:
				 * Ctrl+C means "abort/cancel", not "accept default". */
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return 3;

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
