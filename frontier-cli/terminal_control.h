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

/* Display formatting */
void terminal_start_inverted(void);
void terminal_end_inverted(void);
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
