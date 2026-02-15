/*
 * terminal_control.c - POSIX terminal control implementation
 *
 * Low-level terminal manipulation using termios and ANSI escape codes.
 * Thread-safe: terminal state is saved/restored per operation.
 */

#include "terminal_control.h"
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <poll.h>

/* Internal terminal state storage */
typedef struct {
	struct termios original_termios;
	bool saved;
} internal_terminal_state;

/* Global signal handler (restored on cleanup) */
static terminal_sigint_handler original_sigint_handler = NULL;

/* Saves current terminal settings; caller must free with terminal_free_state(). */
terminal_state* terminal_save_state(void) {
	internal_terminal_state *state = (internal_terminal_state*)malloc(sizeof(internal_terminal_state));
	if (!state) {
		return NULL;
	}

	state->saved = (tcgetattr(STDIN_FILENO, &state->original_termios) == 0);
	return (terminal_state*)state;
}

/* Restores terminal settings from a previously saved state. */
void terminal_restore_state(terminal_state *state) {
	internal_terminal_state *istate = (internal_terminal_state*)state;
	if (!istate || !istate->saved) {
		return;
	}

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &istate->original_termios);
}

/* Frees memory allocated by terminal_save_state(). */
void terminal_free_state(terminal_state *state) {
	if (state) {
		free(state);
	}
}

/* Initializes terminal state struct and saves current settings for later restore. */
bool terminal_init(terminal_state *state) {
	if (!state) {
		return false;
	}

	state->saved_termios = terminal_save_state();
	if (!state->saved_termios) {
		return false;
	}

	state->raw_mode_enabled = false;
	return true;
}

/* Restores terminal to original state and frees resources. */
void terminal_cleanup(terminal_state *state) {
	if (!state) {
		return;
	}

	if (state->saved_termios) {
		terminal_restore_state((terminal_state*)state->saved_termios);
		terminal_free_state((terminal_state*)state->saved_termios);
		state->saved_termios = NULL;
	}

	state->raw_mode_enabled = false;
}

/* Enables raw mode for the given terminal state; tracks mode for cleanup. */
bool terminal_enable_raw_mode(terminal_state *state) {
	if (!state) {
		return false;
	}

	if (terminal_set_raw_mode()) {
		state->raw_mode_enabled = true;
		return true;
	}

	return false;
}

/* Disables raw mode and restores previous terminal settings. */
void terminal_disable_raw_mode(terminal_state *state) {
	if (!state || !state->raw_mode_enabled) {
		return;
	}

	if (state->saved_termios) {
		terminal_restore_state((terminal_state*)state->saved_termios);
	}

	state->raw_mode_enabled = false;
}

/* Sets terminal to raw mode globally; disables canonical mode and echo. */
bool terminal_set_raw_mode(void) {
	struct termios raw;

	if (tcgetattr(STDIN_FILENO, &raw) != 0) {
		/* If stdin is not a TTY (piped input for testing), that's OK
		 * when FRONTIER_FORCE_INTERACTIVE=1 is set. We'll use line-buffered
		 * input instead of raw mode. */
		if (getenv("FRONTIER_FORCE_INTERACTIVE")) {
			return true;  /* Pretend success - we'll read line-buffered */
		}
		return false;
	}

	/* Disable canonical mode and echo */
	raw.c_lflag &= ~(ICANON | ECHO);

	/* Read minimum 1 byte, no timeout */
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	return tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0;
}

/* Disables character echo on terminal input. */
bool terminal_disable_echo(void) {
	struct termios term;

	if (tcgetattr(STDIN_FILENO, &term) != 0) {
		return false;
	}

	term.c_lflag &= ~ECHO;

	return tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) == 0;
}

/* Re-enables character echo on terminal input. */
bool terminal_enable_echo(void) {
	struct termios term;

	if (tcgetattr(STDIN_FILENO, &term) != 0) {
		return false;
	}

	term.c_lflag |= ECHO;

	return tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) == 0;
}

/* Queries terminal dimensions via ioctl; falls back to 80x24 on failure. */
bool terminal_get_size(int *rows, int *cols) {
	struct winsize ws;

	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
		if (rows) *rows = ws.ws_row;
		if (cols) *cols = ws.ws_col;
		return true;
	}

	/* Fallback */
	if (rows) *rows = 24;
	if (cols) *cols = 80;
	return false;
}

/* Clears entire screen and moves cursor to top-left. */
void terminal_clear_screen(void) {
	fputs("\x1b[2J\x1b[H", stderr);
	fflush(stderr);
}

/* Hides the text cursor. */
void terminal_hide_cursor(void) {
	fputs("\x1b[?25l", stderr);
	fflush(stderr);
}

/* Shows the text cursor. */
void terminal_show_cursor(void) {
	fputs("\x1b[?25h", stderr);
	fflush(stderr);
}

/* Saves cursor position using ANSI escape sequence. */
void terminal_save_cursor(void) {
	fputs("\x1b[s", stderr);
	fflush(stderr);
}

/* Restores cursor to previously saved position. */
void terminal_restore_cursor(void) {
	fputs("\x1b[u", stderr);
	fflush(stderr);
}

/* Moves cursor to absolute row and column position (1-based). */
void terminal_move_cursor(int row, int col) {
	fprintf(stderr, "\x1b[%d;%dH", row, col);
	fflush(stderr);
}

/* Clears the current line and moves cursor to beginning. */
void terminal_clear_line(void) {
	fputs("\x1b[2K\r", stderr);
	fflush(stderr);
}

/* Moves cursor up by specified number of lines. */
void terminal_move_cursor_up(int lines) {
	if (lines > 0) {
		fprintf(stderr, "\x1b[%dA", lines);
		fflush(stderr);
	}
}

/* Moves cursor down by specified number of lines. */
void terminal_move_cursor_down(int lines) {
	if (lines > 0) {
		fprintf(stderr, "\x1b[%dB", lines);
		fflush(stderr);
	}
}

/* Starts inverted (reverse video) text mode. */
void terminal_start_inverted(void) {
	fputs("\x1b[7m", stderr);
	fflush(stderr);
}

/* Ends inverted text mode and restores normal video. */
void terminal_end_inverted(void) {
	fputs("\x1b[27m", stderr);
	fflush(stderr);
}

/* Starts dim (faint) text mode. */
void terminal_start_dim(void) {
	fputs("\x1b[2m", stderr);
	fflush(stderr);
}

/* Ends dim text mode and restores normal intensity. */
void terminal_end_dim(void) {
	fputs("\x1b[22m", stderr);
	fflush(stderr);
}

/* Outputs text in bold, then restores normal weight. */
void terminal_bold_text(const char *text) {
	if (!text) {
		return;
	}

	/* Bold on */
	fputs("\x1b[1m", stderr);
	fputs(text, stderr);
	/* Bold off */
	fputs("\x1b[22m", stderr);
	fflush(stderr);
}

/* Reads a single keystroke, parsing escape sequences for special keys. */
key_input terminal_read_key(void) {
	key_input result = {KEY_UNKNOWN, 0};
	char ch;
	ssize_t nread;

	nread = read(STDIN_FILENO, &ch, 1);
	if (nread != 1) {
		return result;
	}

	/* Handle escape sequences.  A bare Esc sends just 0x1b, while
	 * arrow keys send 0x1b '[' <letter>.  Use poll() with a short
	 * timeout to tell them apart. */
	if (ch == '\x1b') {
		char seq[3];
		struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };

		/* Wait up to 50 ms for a follow-up byte */
		if (poll(&pfd, 1, 50) <= 0 || read(STDIN_FILENO, &seq[0], 1) != 1) {
			result.type = KEY_ESCAPE;
			return result;
		}

		if (seq[0] == '[') {
			if (read(STDIN_FILENO, &seq[1], 1) != 1) {
				result.type = KEY_ESCAPE;
				return result;
			}

			/* Arrow keys */
			switch (seq[1]) {
				case 'A': result.type = KEY_ARROW_UP; break;
				case 'B': result.type = KEY_ARROW_DOWN; break;
				case 'C': result.type = KEY_ARROW_RIGHT; break;
				case 'D': result.type = KEY_ARROW_LEFT; break;
				case '3':
					/* Delete key: ESC[3~ */
					if (read(STDIN_FILENO, &seq[2], 1) == 1 && seq[2] == '~') {
						result.type = KEY_DELETE;
					}
					break;
				default:
					result.type = KEY_UNKNOWN;
					break;
			}
		} else {
			result.type = KEY_ESCAPE;
		}

		return result;
	}

	/* Handle control characters */
	if (ch == 3) {  /* Ctrl+C */
		result.type = KEY_CTRL_C;
		return result;
	}

	if (ch == 4) {  /* Ctrl+D */
		result.type = KEY_CTRL_D;
		return result;
	}

	if (ch == '\n' || ch == '\r') {
		result.type = KEY_ENTER;
		return result;
	}

	if (ch == '\t') {
		result.type = KEY_TAB;
		return result;
	}

	if (ch == 127 || ch == '\b') {  /* Backspace */
		result.type = KEY_BACKSPACE;
		return result;
	}

	/* Regular character */
	result.type = KEY_CHAR;
	result.ch = ch;
	return result;
}

/* Sets a custom SIGINT handler; returns the previous handler. */
terminal_sigint_handler terminal_set_sigint_handler(terminal_sigint_handler handler) {
	terminal_sigint_handler old_handler = original_sigint_handler;

	if (handler) {
		original_sigint_handler = handler;
		signal(SIGINT, handler);
	} else {
		/* Restore default */
		signal(SIGINT, SIG_DFL);
		original_sigint_handler = NULL;
	}

	return old_handler;
}
