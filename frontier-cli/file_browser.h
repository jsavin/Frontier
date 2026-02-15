/*
 * file_browser.h - Two-pane ranger-style file browser for interactive mode
 *
 * Provides a full-screen visual file browser with:
 * - Left pane: current directory listing with arrow key navigation
 * - Right pane: preview of selected entry (dir contents or file info)
 * - Type-ahead selection (like Finder list view)
 * - Mode-specific behavior for getFile, putFile, getFolder, getDisk
 *
 * Architecture:
 * - Replaces type-a-path dialogs when stdin is a TTY
 * - Falls back to file_dialog.c interactive_file_loop when piped
 * - Reuses tab_completion.c for directory enumeration
 * - Reuses terminal_control.c for input/display
 *
 * Platform: POSIX (macOS, Linux)
 * Dependencies: tab_completion.h, terminal_control.h
 */

#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H

#include "file_dialog.h"
#include "tab_completion.h"
#include "terminal_control.h"
#include <limits.h>
#include <time.h>

/* Browser mode determines selection behavior */
typedef enum {
	BROWSER_GET_FILE,    /* Select existing file */
	BROWSER_PUT_FILE,    /* Choose save location (allows new filename) */
	BROWSER_GET_FOLDER,  /* Select existing directory */
	BROWSER_GET_DISK     /* Select volume/mount point */
} browser_mode;

/* Browser state for the two-pane UI */
typedef struct {
	browser_mode mode;
	char prompt[256];
	char type_filter[64];          /* Extension filter (e.g., "txt") */
	char current_dir[PATH_MAX];

	/* Left pane: directory listing */
	file_entry entries[MAX_COMPLETION_CANDIDATES];
	size_t entry_count;
	size_t cursor;
	size_t scroll_offset;

	/* Right pane: preview of selected entry */
	file_entry preview_entries[MAX_COMPLETION_CANDIDATES];
	size_t preview_count;

	/* Type-ahead selection */
	char typeahead_buf[32];
	size_t typeahead_len;
	struct timespec typeahead_time;

	/* putFile filename input */
	char filename_buf[256];
	size_t filename_len;
	size_t filename_cursor_pos;
	bool filename_editing;
	bool confirm_overwrite;         /* Awaiting y/n overwrite confirmation */

	/* Terminal dimensions and layout */
	int term_rows;
	int term_cols;
	int list_height;
	int left_width;
	int right_width;

	/* Terminal state */
	terminal_state terminal;
	bool running;
} browser_state;

/* Select an existing file via two-pane browser.
 * type_filter: extension to filter by (e.g., "txt"), or NULL for all files. */
file_dialog_result file_browser_get_file(const char *prompt,
                                          const char *start_path,
                                          const char *type_filter);

/* Choose a save location via two-pane browser with filename input. */
file_dialog_result file_browser_put_file(const char *prompt,
                                          const char *start_path);

/* Select an existing directory via two-pane browser. */
file_dialog_result file_browser_get_folder(const char *prompt,
                                            const char *start_path);

/* Select a mounted volume/disk via two-pane browser. */
file_dialog_result file_browser_get_disk(const char *prompt);

#endif /* FILE_BROWSER_H */
