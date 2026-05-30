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

/* Timeout (ms) to distinguish a bare Esc keypress from the start of an
 * ANSI escape sequence (e.g. arrow keys send ESC [ A).  50 ms is the
 * industry-standard value used by vim, tmux, etc. */
#define ESC_SEQUENCE_TIMEOUT_MS 50

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

/*
 * Query the terminal for the current cursor position via the ANSI DSR
 * (Device Status Report) sequence ESC [ 6 n.  The terminal replies with
 * ESC [ <row> ; <col> R.  Read up to 32 bytes of the reply with a 50ms
 * timeout per byte, parse, and return true only on a fully formed
 * response.  On any failure the out-params are left untouched.
 */
bool terminal_get_cursor_pos(int *row, int *col) {
	/* If stdin isn't a TTY (e.g., piped from a process feeding real
	 * data), don't emit the DSR query and don't consume bytes from
	 * the pipe.  Returns false immediately; out-params untouched. */
	if (!isatty(STDIN_FILENO)) {
		return false;
	}

	/* Emit the DSR query.  Match the existing convention of writing to
	 * stderr (other CSI emitters in this file do likewise). */
	fputs("\x1b[6n", stderr);
	fflush(stderr);

	/* Accumulate reply bytes until we hit 'R' or fill the buffer. */
	char buf[32];
	size_t n = 0;
	while (n < sizeof(buf) - 1) {
		struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
		int pr = poll(&pfd, 1, 50);
		if (pr <= 0) {
			/* Timeout or error -- no reply (or no more bytes). */
			return false;
		}
		char c;
		ssize_t r = read(STDIN_FILENO, &c, 1);
		if (r != 1) {
			return false;
		}
		buf[n++] = c;
		if (c == 'R') break;
	}
	buf[n] = '\0';

	if (n < 6) return false;        /* shortest valid reply: ESC [ 1 ; 1 R */
	if (buf[0] != '\x1b' || buf[1] != '[' || buf[n - 1] != 'R') {
		return false;
	}

	int r = 0, c = 0;
	if (sscanf(buf + 2, "%30d;%30d", &r, &c) != 2) {
		return false;
	}
	if (r <= 0 || c <= 0) return false;

	if (row) *row = r;
	if (col) *col = c;
	return true;
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

		/* Wait for a follow-up byte; timeout means bare Esc */
		if (poll(&pfd, 1, ESC_SEQUENCE_TIMEOUT_MS) <= 0 || read(STDIN_FILENO, &seq[0], 1) != 1) {
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

/* ---------------------------------------------------------------------------
 * Slash-menu palette UI substrate (PR 3)
 *
 * Cursor positioning, SGR colour/attribute, Unicode box drawing, and xterm
 * mouse tracking enable/disable.  See header for usage notes and rationale.
 * ------------------------------------------------------------------------- */

/* Move cursor to absolute (row, col) — 1-based.  CSI CUP. */
void terminal_move_to(int row, int col) {
	fprintf(stderr, "\x1b[%d;%dH", row, col);
	fflush(stderr);
}

/* Set foreground/background colour and bold/inverse/underline flags.
 * Always emits a leading SGR 0 reset so callers don't have to track state. */
void terminal_set_attr(uint8_t fg, uint8_t bg, uint8_t flags) {
	/* Build "ESC [ 0[;1][;7][;4][;3<fg>][;4<bg>] m" */
	char buf[64];
	int n = 0;
	n += snprintf(buf + n, sizeof(buf) - n, "\x1b[0");
	if (flags & TERM_ATTR_BOLD)      n += snprintf(buf + n, sizeof(buf) - n, ";1");
	if (flags & TERM_ATTR_UNDERLINE) n += snprintf(buf + n, sizeof(buf) - n, ";4");
	if (flags & TERM_ATTR_INVERSE)   n += snprintf(buf + n, sizeof(buf) - n, ";7");
	if (fg >= 1 && fg <= 7)          n += snprintf(buf + n, sizeof(buf) - n, ";3%u", (unsigned)fg);
	if (bg >= 1 && bg <= 7)          n += snprintf(buf + n, sizeof(buf) - n, ";4%u", (unsigned)bg);
	n += snprintf(buf + n, sizeof(buf) - n, "m");
	(void)n;
	fputs(buf, stderr);
	fflush(stderr);
}

/* Draw a rectangular border using Unicode box-drawing chars. */
void terminal_emit_box(int x, int y, int w, int h, bool double_line) {
	if (w < 2 || h < 2) {
		/* Degenerate — nothing meaningful to draw. */
		return;
	}

	/* UTF-8 sequences for the two glyph sets. */
	const char *tl, *tr, *bl, *br, *hz, *vt;
	if (double_line) {
		tl = "\xe2\x95\x94";  /* ╔ U+2554 */
		tr = "\xe2\x95\x97";  /* ╗ U+2557 */
		bl = "\xe2\x95\x9a";  /* ╚ U+255A */
		br = "\xe2\x95\x9d";  /* ╝ U+255D */
		hz = "\xe2\x95\x90";  /* ═ U+2550 */
		vt = "\xe2\x95\x91";  /* ║ U+2551 */
	} else {
		tl = "\xe2\x94\x8c";  /* ┌ U+250C */
		tr = "\xe2\x94\x90";  /* ┐ U+2510 */
		bl = "\xe2\x94\x94";  /* └ U+2514 */
		br = "\xe2\x94\x98";  /* ┘ U+2518 */
		hz = "\xe2\x94\x80";  /* ─ U+2500 */
		vt = "\xe2\x94\x82";  /* │ U+2502 */
	}

	/* Top edge */
	fprintf(stderr, "\x1b[%d;%dH", y, x);
	fputs(tl, stderr);
	for (int i = 0; i < w - 2; i++) fputs(hz, stderr);
	fputs(tr, stderr);

	/* Side edges */
	for (int row = 1; row < h - 1; row++) {
		fprintf(stderr, "\x1b[%d;%dH", y + row, x);
		fputs(vt, stderr);
		fprintf(stderr, "\x1b[%d;%dH", y + row, x + w - 1);
		fputs(vt, stderr);
	}

	/* Bottom edge */
	fprintf(stderr, "\x1b[%d;%dH", y + h - 1, x);
	fputs(bl, stderr);
	for (int i = 0; i < w - 2; i++) fputs(hz, stderr);
	fputs(br, stderr);

	fflush(stderr);
}

/* atexit-registered cleanup so a normal exit() always restores the terminal. */
static void terminal_mouse_atexit_cleanup(void) {
	/* Direct write to STDERR_FILENO using only async-signal-safe APIs is
	 * unnecessary here (atexit runs from normal context), but using fputs
	 * + fflush keeps it consistent with the rest of this file. */
	terminal_disable_mouse();
}

/* Enable xterm SGR (1006) + button-event (1000) mouse tracking.
 * Registers atexit cleanup on first call only (idempotent). */
void terminal_enable_mouse(void) {
	static bool atexit_registered = false;
	if (!atexit_registered) {
		atexit(terminal_mouse_atexit_cleanup);
		atexit_registered = true;
	}

	fputs("\x1b[?1006h\x1b[?1000h", stderr);
	fflush(stderr);
}

/* Disable mouse tracking.  Order matches xterm convention: turn off the
 * encoding extension after the button-event tracking that referenced it. */
void terminal_disable_mouse(void) {
	fputs("\x1b[?1000l\x1b[?1006l", stderr);
	fflush(stderr);
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
