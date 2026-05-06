/*
 * terminal_control.h - POSIX terminal control utilities for interactive mode
 *
 * Provides low-level terminal manipulation functions for headless interactive dialogs.
 * POSIX-only implementation using termios and ANSI escape codes.
 *
 * Functions:
 * - Terminal settings (save/restore, raw mode, echo control)
 * - Cursor control (move, save/restore)
 * - Display formatting (clear line, inverted text)
 * - Key input (arrow keys, special keys, raw character reading)
 */

#ifndef TERMINAL_CONTROL_H
#define TERMINAL_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

/* Terminal state structure */
typedef struct terminal_state {
	void *saved_termios;  /* Saved terminal attributes */
	bool raw_mode_enabled;
} terminal_state;

/*
 * Terminal State Usage Patterns
 *
 * There are two ways to use terminal_state:
 *
 * Pattern 1: Heap-allocated (simple save/restore)
 *   terminal_state *term_state = terminal_save_state();  // Returns malloc'd pointer
 *   // ... perform operations with terminal state saved
 *   terminal_restore_state(term_state);
 *   terminal_free_state(term_state);  // MUST free or leaks memory
 *
 * Pattern 2: Stack-allocated (full lifecycle control)
 *   terminal_state terminal;  // Stack-allocated
 *   terminal_init(&terminal);
 *   // ... perform operations with terminal control
 *   terminal_cleanup(&terminal);  // MUST call or leaks memory from saved_termios
 *
 * IMPORTANT: Both patterns allocate memory that must be freed:
 * - Pattern 1: Call terminal_free_state() when done
 * - Pattern 2: Call terminal_cleanup() when done (frees internal saved_termios)
 */

/* Terminal initialization and cleanup */
terminal_state* terminal_save_state(void);
void terminal_restore_state(terminal_state *state);
void terminal_free_state(terminal_state *state);
bool terminal_init(terminal_state *state);
void terminal_cleanup(terminal_state *state);

/* Terminal mode control */
bool terminal_set_raw_mode(void);
bool terminal_disable_echo(void);
bool terminal_enable_echo(void);
bool terminal_enable_raw_mode(terminal_state *state);
void terminal_disable_raw_mode(terminal_state *state);

/* Cursor control */
void terminal_save_cursor(void);
void terminal_restore_cursor(void);
void terminal_move_cursor(int row, int col);
void terminal_move_cursor_up(int lines);
void terminal_move_cursor_down(int lines);
void terminal_clear_line(void);

/* Screen and cursor visibility */
bool terminal_get_size(int *rows, int *cols);
void terminal_clear_screen(void);
void terminal_hide_cursor(void);
void terminal_show_cursor(void);

/*
 * Slash-menu palette UI substrate (PR 3).
 *
 * These primitives back the future REPL slash-command palette renderer
 * (terminal_emit_box, terminal_set_attr, terminal_move_to) and its mouse
 * input mode (terminal_enable_mouse / terminal_disable_mouse).
 *
 * All emitters write to stderr and fflush, matching the existing convention
 * in this header.
 */

/* Attribute flag bits for terminal_set_attr().  Combine with bitwise OR. */
#define TERM_ATTR_BOLD      0x01u
#define TERM_ATTR_INVERSE   0x02u
#define TERM_ATTR_UNDERLINE 0x04u

/*
 * Move cursor to absolute position (1-based row, 1-based column) using the
 * ANSI CUP sequence (ESC [ row ; col H).  Synonym of terminal_move_cursor()
 * with the slash-menu palette naming convention.
 */
void terminal_move_to(int row, int col);

/*
 * Set foreground and background colour plus a bitmask of TERM_ATTR_* flags.
 * fg / bg use the standard 8-colour palette indices (0..7); 0 in both with
 * no flags emits a plain SGR reset.  Always begins with SGR 0 so callers do
 * not have to track previous attribute state.
 */
void terminal_set_attr(uint8_t fg, uint8_t bg, uint8_t flags);

/*
 * Draw a rectangular border at column x, row y with width w and height h
 * (both 1-based) using Unicode box-drawing characters.  When double_line is
 * true, uses ╔═╗║╚╝ glyphs; otherwise ┌─┐│└┘.  No-ops if w<2 or h<2.
 * Does not clear the box interior — caller paints contents separately.
 */
void terminal_emit_box(int x, int y, int w, int h, bool double_line);

/*
 * Enable xterm SGR-encoded mouse tracking (DEC private mode 1006) plus
 * button-event tracking (mode 1000).  On the first call in a process, also
 * registers an atexit() hook that calls terminal_disable_mouse() — this
 * prevents the terminal from being left in mouse-tracking mode if the
 * process exits via a normal exit() path (Ctrl-D, quit command, etc.).
 *
 * NOTE: atexit handlers do NOT run on _exit(), so signal handlers that
 * terminate via _exit must call terminal_disable_mouse() directly.
 *
 * NOTE: SGR 1006 is xterm 277+ (2012) and is widely supported (iTerm2,
 * Terminal.app, alacritty, kitty, gnome-terminal).  Older terminals or
 * mouse-over-ssh without TERM forwarding may ignore it; capability probing
 * is deferred to a later PR (P-low risk).
 */
void terminal_enable_mouse(void);

/*
 * Disable mouse tracking (modes 1000 and 1006).  Idempotent and safe to
 * call even if terminal_enable_mouse() was never invoked.
 */
void terminal_disable_mouse(void);

/* Display formatting */
void terminal_start_inverted(void);
void terminal_end_inverted(void);
void terminal_start_dim(void);
void terminal_end_dim(void);
void terminal_bold_text(const char *text);

/* Key input */
typedef enum {
	KEY_UNKNOWN = 0,
	KEY_CHAR,        /* Regular character */
	KEY_ENTER,       /* Enter/Return */
	KEY_TAB,         /* Tab */
	KEY_BACKSPACE,   /* Backspace */
	KEY_DELETE,      /* Delete */
	KEY_ARROW_UP,    /* Up arrow */
	KEY_ARROW_DOWN,  /* Down arrow */
	KEY_ARROW_LEFT,  /* Left arrow */
	KEY_ARROW_RIGHT, /* Right arrow */
	KEY_CTRL_C,      /* Ctrl+C */
	KEY_CTRL_D,      /* Ctrl+D */
	KEY_ESCAPE       /* Escape */
} key_type;

typedef struct {
	key_type type;
	char ch;  /* For KEY_CHAR, the actual character */
} key_input;

key_input terminal_read_key(void);

/* Signal handling */
typedef void (*terminal_sigint_handler)(int);
terminal_sigint_handler terminal_set_sigint_handler(terminal_sigint_handler handler);

#endif /* TERMINAL_CONTROL_H */
