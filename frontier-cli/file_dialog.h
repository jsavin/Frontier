/*
 * file_dialog.h - Interactive file dialog functions for headless mode
 *
 * Phase 2B.3: File Dialog Engine
 *
 * Implements four file dialog types from UserTalk file.* verbs:
 * - file.getFileDialog() - Select existing file
 * - file.putFileDialog() - Choose location to save file
 * - file.getFolderDialog() - Select existing directory
 * - file.getDiskDialog() - Select volume/disk
 *
 * Architecture:
 * - Layer 4 of 5-layer file dialog stack
 * - Uses tab_completion.c for Tab key handling
 * - Uses terminal_control.c for input/output
 * - Called by fileverbs_portable.c for interactive file selection
 *
 * Platform: POSIX (macOS, Linux)
 * Dependencies: tab_completion.h, terminal_control.h
 */

#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#include <stdbool.h>

/* File dialog result structure */
typedef struct file_dialog_result {
	char path[1024];       /* Full absolute path to selected file/folder */
	bool success;          /* true if user selected a path, false if cancelled */
} file_dialog_result;

/* Get existing file dialog
 *
 * Prompts user to select an existing file with tab completion.
 * Validates that selected path exists and is a file.
 *
 * Parameters:
 *   start_path: Starting directory (NULL = current working directory)
 *
 * Returns: Result structure with selected file path and success status
 */
file_dialog_result file_dialog_get_file(const char *start_path);

/* Put file dialog
 *
 * Prompts user to choose location to save a file.
 * Allows selecting existing file (overwrites) or entering new filename.
 *
 * Parameters:
 *   start_path: Starting directory or default filename (NULL = cwd)
 *
 * Returns: Result structure with selected file path and success status
 */
file_dialog_result file_dialog_put_file(const char *start_path);

/* Get folder dialog
 *
 * Prompts user to select an existing directory with tab completion.
 * Validates that selected path exists and is a directory.
 *
 * Parameters:
 *   start_path: Starting directory (NULL = current working directory)
 *
 * Returns: Result structure with selected folder path and success status
 */
file_dialog_result file_dialog_get_folder(const char *start_path);

/* Get disk/volume dialog
 *
 * Prompts user to select a mounted volume (macOS: uses getfsstat).
 * Returns path to volume root (e.g., "/", "/Volumes/External").
 *
 * Parameters:
 *   None
 *
 * Returns: Result structure with selected volume path and success status
 */
file_dialog_result file_dialog_get_disk(void);

#endif /* FILE_DIALOG_H */
