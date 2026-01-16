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

/* Yes/No prompt */

bool dialog_ask(const char *prompt) {
	terminal_state *term_state = NULL;
	bool selected_yes = true;  /* Default to Yes */

	if (!isInteractiveMode()) {
		return false;
	}

	/* Save terminal state and enable raw mode */
	term_state = terminal_save_state();
	if (!term_state || !terminal_set_raw_mode()) {
		if (term_state) {
			terminal_restore_state(term_state);
			terminal_free_state(term_state);
		}
		return false;
	}

	/* Display initial prompt */
	fprintf(stderr, "%s? ", prompt);
	terminal_start_inverted();
	fputs("Yes", stderr);
	terminal_end_inverted();
	fputs(" No", stderr);
	fflush(stderr);

	while (1) {
		key_input key = terminal_read_key();

		switch (key.type) {
			case KEY_ENTER:
				/* Confirm selection */
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return selected_yes;

			case KEY_ARROW_LEFT:
			case KEY_ARROW_RIGHT:
			case KEY_TAB:
				/* Toggle selection */
				selected_yes = !selected_yes;

				/* Redraw prompt */
				terminal_clear_line();
				fprintf(stderr, "%s? ", prompt);
				if (selected_yes) {
					terminal_start_inverted();
					fputs("Yes", stderr);
					terminal_end_inverted();
					fputs(" No", stderr);
				} else {
					fputs("Yes ", stderr);
					terminal_start_inverted();
					fputs("No", stderr);
					terminal_end_inverted();
				}
				fflush(stderr);
				break;

			case KEY_CTRL_C:
			case KEY_CTRL_D:
				/* Cancel - return false */
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return false;

			case KEY_CHAR:
				/* 'y' or 'n' key shortcuts */
				if (key.ch == 'y' || key.ch == 'Y') {
					fputs("\n", stderr);
					terminal_restore_state(term_state);
					terminal_free_state(term_state);
					return true;
				} else if (key.ch == 'n' || key.ch == 'N') {
					fputs("\n", stderr);
					terminal_restore_state(term_state);
					terminal_free_state(term_state);
					return false;
				}
				break;

			default:
				/* Ignore other keys */
				break;
		}
	}
}

/* Integer input prompt */

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

/* String input prompt */

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

/* Password input prompt */

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

/* Alert prompt with beep */

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

/* Notify prompt (no beep) */

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

/* Two-way button choice */

bool dialog_twoway(const char *prompt, const char *button1, const char *button2) {
	int selection = 0;  /* 0 = button1, 1 = button2 */
	terminal_state *term_state = NULL;

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

	/* Initial display */
	while (1) {
		/* Clear line and move to start */
		fputs("\r\033[K", stderr);
		
		/* Display prompt and buttons */
		fprintf(stderr, "%s? ", prompt);
		
		/* Display button1 (inverted if selected) */
		if (selection == 0) {
			fprintf(stderr, "\033[7m%s\033[0m ", button1);
		} else {
			fprintf(stderr, "%s ", button1);
		}
		
		/* Display button2 (inverted if selected) */
		if (selection == 1) {
			fprintf(stderr, "\033[7m%s\033[0m", button2);
		} else {
			fprintf(stderr, "%s", button2);
		}
		
		fflush(stderr);

		/* Read key */
		key_input key = terminal_read_key();

		switch (key.type) {
			case KEY_ENTER:
				/* Confirm selection */
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return (selection == 0);

			case KEY_ARROW_LEFT:
			case KEY_ARROW_RIGHT:
			case KEY_TAB:
				/* Toggle selection */
				selection = 1 - selection;
				break;

			case KEY_CTRL_C:
			case KEY_CTRL_D:
				/* Cancel - default to button1 */
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return true;

			case KEY_CHAR:
				/* Also allow 1/2 keys */
				if (key.ch == '1') {
					selection = 0;
				} else if (key.ch == '2') {
					selection = 1;
				}
				break;

			default:
				break;
		}
	}
}

/* Three-way button choice */

int dialog_threeway(const char *prompt, const char *button1, const char *button2, const char *button3) {
	int selection = 0;  /* 0 = button1, 1 = button2, 2 = button3 */
	terminal_state *term_state = NULL;

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

	/* Initial display */
	while (1) {
		/* Clear line and move to start */
		fputs("\r\033[K", stderr);
		
		/* Display prompt and buttons */
		fprintf(stderr, "%s? ", prompt);
		
		/* Display button1 (inverted if selected) */
		if (selection == 0) {
			fprintf(stderr, "\033[7m%s\033[0m ", button1);
		} else {
			fprintf(stderr, "%s ", button1);
		}
		
		/* Display button2 (inverted if selected) */
		if (selection == 1) {
			fprintf(stderr, "\033[7m%s\033[0m ", button2);
		} else {
			fprintf(stderr, "%s ", button2);
		}
		
		/* Display button3 (inverted if selected) */
		if (selection == 2) {
			fprintf(stderr, "\033[7m%s\033[0m", button3);
		} else {
			fprintf(stderr, "%s", button3);
		}
		
		fflush(stderr);

		/* Read key */
		key_input key = terminal_read_key();

		switch (key.type) {
			case KEY_ENTER:
				/* Confirm selection (return 1, 2, or 3) */
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return (selection + 1);

			case KEY_ARROW_LEFT:
				/* Move left */
				if (selection > 0) {
					selection--;
				}
				break;

			case KEY_ARROW_RIGHT:
			case KEY_TAB:
				/* Move right */
				if (selection < 2) {
					selection++;
				}
				break;

			case KEY_CTRL_C:
			case KEY_CTRL_D:
				/* Cancel - default to button1 */
				fputs("\n", stderr);
				terminal_restore_state(term_state);
				terminal_free_state(term_state);
				return 1;

			case KEY_CHAR:
				/* Also allow 1/2/3 keys */
				if (key.ch == '1') {
					selection = 0;
				} else if (key.ch == '2') {
					selection = 1;
				} else if (key.ch == '3') {
					selection = 2;
				}
				break;

			default:
				break;
		}
	}
}
