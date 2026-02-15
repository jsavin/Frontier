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
 * - Routes to file_browser.c (TTY) or fallback interactive loop (pipe/batch)
 * - Uses tab_completion.c for Tab key handling (fallback path)
 * - Uses terminal_control.c for input/output
 * - Called by fileverbs_portable.c for interactive file selection
 *
 * Platform: POSIX (macOS, Linux)
 * Dependencies: tab_completion.h, terminal_control.h, file_browser.h
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
 * When stdin is a TTY, opens a two-pane visual browser.
 * When piped, falls back to line-buffered path input with tab completion.
 *
 * Parameters:
 *   prompt: Dialog prompt text (displayed to user)
 *   start_path: Starting directory (NULL = current working directory)
 *   type_filter: File extension filter (e.g., "txt"), NULL for all files
 *
 * Returns: Result structure with selected file path and success status
 */
file_dialog_result file_dialog_get_file(const char *prompt,
                                         const char *start_path,
                                         const char *type_filter);

/* Put file dialog
 *
 * When stdin is a TTY, opens a two-pane visual browser with filename input.
 * When piped, falls back to line-buffered path input.
 *
 * Parameters:
 *   prompt: Dialog prompt text (displayed to user)
 *   start_path: Starting directory or default filename (NULL = cwd)
 *
 * Returns: Result structure with selected file path and success status
 */
file_dialog_result file_dialog_put_file(const char *prompt,
                                         const char *start_path);

/* Get folder dialog
 *
 * When stdin is a TTY, opens a two-pane visual browser for directory selection.
 * When piped, falls back to line-buffered path input.
 *
 * Parameters:
 *   prompt: Dialog prompt text (displayed to user)
 *   start_path: Starting directory (NULL = current working directory)
 *
 * Returns: Result structure with selected folder path and success status
 */
file_dialog_result file_dialog_get_folder(const char *prompt,
                                           const char *start_path);

/* Get disk/volume dialog
 *
 * When stdin is a TTY, opens a two-pane visual browser showing mount points.
 * When piped, falls back to numbered volume list.
 *
 * Parameters:
 *   prompt: Dialog prompt text (displayed to user)
 *
 * Returns: Result structure with selected volume path and success status
 */
file_dialog_result file_dialog_get_disk(const char *prompt);

#endif /* FILE_DIALOG_H */
