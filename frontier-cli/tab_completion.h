/*
 * tab_completion.h - Interactive file path completion with visual menu
 *
 * Phase 2B.2: Tab Completion Engine
 *
 * Provides tab-completion functionality for file dialogs:
 * - Directory enumeration (readdir + stat)
 * - File entry metadata (name, is_directory, size, mtime)
 * - Visual completion menu with arrow key navigation
 * - Multi-column display with file metadata
 *
 * Architecture:
 * - Layer 3 of 5-layer file dialog stack
 * - Uses terminal_control.c for cursor movement and key detection
 * - Called by file_dialog.c when user presses Tab
 *
 * Platform: POSIX (readdir, stat)
 * Dependencies: terminal_control.h, dirent.h, sys/stat.h
 */

#ifndef TAB_COMPLETION_H
#define TAB_COMPLETION_H

#include "terminal_control.h"
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <time.h>

/* Maximum number of completion candidates to show */
#define MAX_COMPLETION_CANDIDATES 100

/* File entry metadata for completion menu */
typedef struct file_entry {
	char name[256];         /* File/directory name */
	bool is_directory;      /* true if directory, false if file */
	off_t size;             /* File size in bytes */
	time_t mtime;           /* Modification time */
} file_entry;

/* Tab completion result structure */
typedef struct tab_completion_result {
	char selected_path[1024];  /* Full path to selected entry */
	bool completed;            /* true if user selected an entry, false if cancelled */
} tab_completion_result;

/* Directory enumeration and matching
 *
 * Finds all entries in directory that match prefix.
 * Returns number of matches found (0 if directory doesn't exist or can't be read).
 *
 * Parameters:
 *   directory: Full path to directory to search
 *   prefix: Filename prefix to match (NULL or empty = all files)
 *   entries: Output array to store matching entries
 *   max_entries: Maximum number of entries to return
 *   show_hidden: If true, include hidden files (starting with '.')
 *
 * Returns: Number of entries written to entries array
 */
size_t tab_completion_find_matches(const char *directory,
                                   const char *prefix,
                                   file_entry *entries,
                                   size_t max_entries,
                                   bool show_hidden);

/* Interactive completion menu
 *
 * Displays a visual menu of file entries and allows user to navigate with arrow keys.
 * Returns the selected entry or indicates cancellation.
 *
 * Parameters:
 *   entries: Array of file entries to display
 *   count: Number of entries in array
 *   directory: Directory path (prepended to selected entry name)
 *   terminal: Terminal state for raw mode control
 *
 * Returns: Result structure with selected path and completion status
 */
tab_completion_result tab_completion_show_menu(const file_entry *entries,
                                               size_t count,
                                               const char *directory,
                                               terminal_state *terminal);

#endif /* TAB_COMPLETION_H */
