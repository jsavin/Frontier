/*
 * file_dialog.c - Interactive file dialog implementation
 *
 * Phase 2B.3: File Dialog Engine
 *
 * Routes to file_browser.c (two-pane visual browser) when stdin is a TTY,
 * or falls back to line-buffered interactive loop when piped.
 */

#include "file_dialog.h"
#include "file_browser.h"
#include "tab_completion.h"
#include "terminal_control.h"
#include "boxen_ui.h"		/* 2026-06-24 JES #691 Phase C.0.7g Phase 2B */
#include "../Common/headers/logging.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <unistd.h>
#include <errno.h>

/* Helper: Check if path exists and is a file */
static bool is_regular_file(const char *path) {
	struct stat st;
	if (stat(path, &st) != 0) {
		return false;
	}
	return S_ISREG(st.st_mode);
}

/* Helper: Check if path exists and is a directory */
static bool is_directory(const char *path) {
	struct stat st;
	if (stat(path, &st) != 0) {
		return false;
	}
	return S_ISDIR(st.st_mode);
}

/* Helper: Get current working directory */
static bool get_cwd(char *buffer, size_t size) {
	return getcwd(buffer, size) != NULL;
}

/* Helper: Extract directory and filename from path */
static void split_path(const char *path, char *dir, size_t dir_size, char *file, size_t file_size) {
	const char *last_slash = strrchr(path, '/');

	if (!last_slash) {
		/* No directory component */
		if (dir) {
			strncpy(dir, ".", dir_size - 1);
			dir[dir_size - 1] = '\0';
		}
		if (file) {
			strncpy(file, path, file_size - 1);
			file[file_size - 1] = '\0';
		}
		return;
	}

	/* Split at last slash */
	size_t dir_len = (size_t)(last_slash - path);
	if (dir_len == 0) {
		/* Root directory */
		if (dir) {
			strncpy(dir, "/", dir_size - 1);
			dir[dir_size - 1] = '\0';
		}
	} else {
		if (dir) {
			size_t copy_len = (dir_len < dir_size - 1) ? dir_len : (dir_size - 1);
			strncpy(dir, path, copy_len);
			dir[copy_len] = '\0';
		}
	}

	if (file) {
		strncpy(file, last_slash + 1, file_size - 1);
		file[file_size - 1] = '\0';
	}
}

/* Core loop for interactive file/folder selection with tab completion and validation.
 * Used as fallback when stdin is piped (not a TTY). */
static file_dialog_result interactive_file_loop(const char *prompt,
                                                const char *start_path,
                                                bool require_exists,
                                                bool require_file,
                                                bool require_dir) {
	file_dialog_result result;
	result.success = false;
	result.path[0] = '\0';

	/* Determine starting directory */
	char current_dir[PATH_MAX];
	if (start_path && start_path[0] != '\0') {
		if (is_directory(start_path)) {
			size_t len = strlen(start_path);
			if (len >= sizeof(current_dir)) {
				log_warn(LOG_COMP_GENERAL, "file_dialog: Start path truncated (length %zu >= %zu)",
				         len, sizeof(current_dir));
			}
			strncpy(current_dir, start_path, sizeof(current_dir) - 1);
		} else {
			split_path(start_path, current_dir, sizeof(current_dir), NULL, 0);
		}
	} else {
		if (!get_cwd(current_dir, sizeof(current_dir))) {
			log_error(LOG_COMP_GENERAL, "interactive_file_loop: getcwd failed");
			return result;
		}
	}

	/* Initialize terminal state */
	terminal_state terminal;
	if (!terminal_init(&terminal)) {
		log_error(LOG_COMP_GENERAL, "interactive_file_loop: terminal_init failed");
		return result;
	}

	/* Input buffer */
	char input[1024] = {0};
	bool done = false;

	/* Check if we're in line-buffered mode (piped stdin for testing) */
	bool line_buffered_mode = (getenv("FRONTIER_FORCE_INTERACTIVE") != NULL && !isatty(STDIN_FILENO));

	while (!done) {
		/* Display prompt (use stderr to avoid interfering with stdin when piped) */
		fprintf(stderr, "\n%s\n", prompt);
		fprintf(stderr, "Current directory: %s\n", current_dir);
		fprintf(stderr, "Enter path (or press Tab for completion, Ctrl+C to cancel): ");
		fflush(stderr);

		/* Read input line */
		size_t input_pos = 0;
		bool tab_pressed = false;

		if (line_buffered_mode) {
			/* Line-buffered mode: use fgets for piped input */
			char *fgets_result = fgets(input, sizeof(input), stdin);

			if (fgets_result == NULL) {
				/* EOF reached - user cancelled or no more input */
				fputs("\n", stderr);
				log_info(LOG_COMP_GENERAL, "File dialog cancelled (EOF on stdin)");
				terminal_cleanup(&terminal);
				return result;
			}

			/* Remove trailing newline */
			size_t len = strlen(input);
			if (len > 0 && input[len - 1] == '\n') {
				input[len - 1] = '\0';
				len--;
			}

			input_pos = len;
			/* Tab completion not supported in line-buffered mode */

		} else {
			/* Interactive mode: read character-by-character with tab completion */
			while (true) {
				int c = getchar();

				if (c == EOF || c == 3) {  /* Ctrl+C */
					fputs("\n", stderr);
					log_info(LOG_COMP_GENERAL, "File dialog cancelled (Ctrl+C or EOF)");
					terminal_cleanup(&terminal);
					return result;
				}

				if (c == '\t') {
					/* Tab completion */
					tab_pressed = true;
					input[input_pos] = '\0';
					break;
				}

				if (c == '\n') {
					/* Enter key */
					input[input_pos] = '\0';
					break;
				}

				if (c == 127 || c == '\b') {
					/* Backspace */
					if (input_pos > 0) {
						input_pos--;
						fprintf(stderr, "\b \b");
						fflush(stderr);
					}
					continue;
				}

				/* Regular character */
				if (input_pos < sizeof(input) - 1) {
					input[input_pos++] = (char)c;
					fprintf(stderr, "%c", c);
					fflush(stderr);
				}
			}
		}

		/* Handle tab completion */
		if (tab_pressed) {
			/* Find matches in current directory */
			file_entry entries[MAX_COMPLETION_CANDIDATES];
			size_t count = tab_completion_find_matches(current_dir, input,
			                                           entries, MAX_COMPLETION_CANDIDATES,
			                                           true);  /* show_hidden = true */

			if (count == 0) {
				fputs("\n", stderr);
				log_debug(LOG_COMP_GENERAL, "No tab completion matches found for input: %s", input);
				input_pos = 0;
				input[0] = '\0';
				continue;
			}

			if (count == 1) {
				/* Single match: auto-complete */
				size_t name_len = strlen(entries[0].name);
				if (name_len >= sizeof(input)) {
					log_warn(LOG_COMP_GENERAL, "file_dialog: Entry name truncated (length %zu >= %zu)",
					         name_len, sizeof(input));
				}
				strncpy(input, entries[0].name, sizeof(input) - 1);
				input[sizeof(input) - 1] = '\0';
				input_pos = strlen(input);

				/* If directory, update current_dir and clear input */
				if (entries[0].is_directory) {
					int ret = snprintf(current_dir, sizeof(current_dir), "%s/%s", current_dir, entries[0].name);
					if (ret >= (int)sizeof(current_dir)) {
						log_warn(LOG_COMP_GENERAL, "file_dialog: Path truncated when navigating to directory: %s/%s",
						         current_dir, entries[0].name);
					}
					input_pos = 0;
					input[0] = '\0';
				}
				continue;
			}

			/* Multiple matches: show menu */
			tab_completion_result tab_result = tab_completion_show_menu(entries, count,
			                                                             current_dir, &terminal);

			if (!tab_result.completed) {
				/* User cancelled menu */
				input_pos = 0;
				input[0] = '\0';
				continue;
			}

			/* Check if selected entry is a directory */
			if (is_directory(tab_result.selected_path)) {
				/* Navigate into directory */
				size_t path_len = strlen(tab_result.selected_path);
				if (path_len >= sizeof(current_dir)) {
					log_warn(LOG_COMP_GENERAL, "file_dialog: Selected directory path truncated (length %zu >= %zu)",
					         path_len, sizeof(current_dir));
				}
				strncpy(current_dir, tab_result.selected_path, sizeof(current_dir) - 1);
				current_dir[sizeof(current_dir) - 1] = '\0';
				input_pos = 0;
				input[0] = '\0';
			} else {
				/* File selected */
				size_t path_len = strlen(tab_result.selected_path);
				if (path_len >= sizeof(result.path)) {
					log_warn(LOG_COMP_GENERAL, "file_dialog: Selected file path truncated (length %zu >= %zu)",
					         path_len, sizeof(result.path));
				}
				strncpy(result.path, tab_result.selected_path, sizeof(result.path) - 1);
				result.path[sizeof(result.path) - 1] = '\0';
				result.success = true;
				done = true;
			}
			continue;
		}

		/* User pressed Enter - validate input */
		if (input_pos == 0) {
			/* Empty input: return current directory (for folder selection) */
			if (!require_file) {
				size_t dir_len = strlen(current_dir);
				if (dir_len >= sizeof(result.path)) {
					log_warn(LOG_COMP_GENERAL, "file_dialog: Current directory path truncated (length %zu >= %zu)",
					         dir_len, sizeof(result.path));
				}
				strncpy(result.path, current_dir, sizeof(result.path) - 1);
				result.path[sizeof(result.path) - 1] = '\0';
				result.success = true;
				done = true;
			} else {
				log_error(LOG_COMP_GENERAL, "File dialog: No file specified");
			}
			continue;
		}

		/* Build full path */
		char full_path[PATH_MAX];
		if (input[0] == '/') {
			/* Absolute path */
			size_t input_len = strlen(input);
			if (input_len >= sizeof(full_path)) {
				log_warn(LOG_COMP_GENERAL, "file_dialog: Absolute input path truncated (length %zu >= %zu)",
				         input_len, sizeof(full_path));
			}
			strncpy(full_path, input, sizeof(full_path) - 1);
		} else {
			/* Relative to current_dir */
			int ret = snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, input);
			if (ret >= (int)sizeof(full_path)) {
				log_warn(LOG_COMP_GENERAL, "file_dialog: Path truncated when building full path: %s/%s",
				         current_dir, input);
			}
		}
		full_path[sizeof(full_path) - 1] = '\0';

		/* Validate requirements */
		if (require_exists && access(full_path, F_OK) != 0) {
			log_error(LOG_COMP_GENERAL, "Path does not exist: %s", full_path);
			input_pos = 0;
			input[0] = '\0';
			continue;
		}

		if (require_file && !is_regular_file(full_path)) {
			log_error(LOG_COMP_GENERAL, "Not a regular file: %s", full_path);
			input_pos = 0;
			input[0] = '\0';
			continue;
		}

		if (require_dir && !is_directory(full_path)) {
			log_error(LOG_COMP_GENERAL, "Not a directory: %s", full_path);
			input_pos = 0;
			input[0] = '\0';
			continue;
		}

		/* Success */
		size_t path_len = strlen(full_path);
		if (path_len >= sizeof(result.path)) {
			log_warn(LOG_COMP_GENERAL, "file_dialog: Full path truncated (length %zu >= %zu)",
			         path_len, sizeof(result.path));
		}
		strncpy(result.path, full_path, sizeof(result.path) - 1);
		result.path[sizeof(result.path) - 1] = '\0';
		result.success = true;
		done = true;
	}

	terminal_cleanup(&terminal);
	return result;
}

/* Routes to two-pane browser (TTY) or fallback loop (piped input). */
file_dialog_result file_dialog_get_file(const char *prompt,
                                         const char *start_path,
                                         const char *type_filter) {
	const char *effective_prompt = (prompt && prompt[0]) ? prompt : "Select an existing file:";

	/* 2026-06-24 JES #691 Phase C.0.7g Phase 2B: route through the
	 * boxen-native file picker when the boxen REPL is active.  The
	 * picker IS interactive even when isatty() would return false (tmux
	 * subshells, non-TTY pipes), so the bridge check must precede the
	 * isatty() branch below. */
	if (boxen_ui_is_active()) {
		file_dialog_result r;
		r.path[0] = '\0';
		r.success = boxen_ui_pick_file(BOXEN_UI_PICK_GET_FILE,
		                                effective_prompt, start_path,
		                                type_filter,
		                                r.path, sizeof(r.path));
		return r;
	}

	if (isatty(STDIN_FILENO))
		return file_browser_get_file(effective_prompt, start_path, type_filter);

	return interactive_file_loop(effective_prompt,
	                             start_path,
	                             true,   /* require_exists */
	                             true,   /* require_file */
	                             false); /* require_dir */
}

/* Routes to two-pane browser (TTY) or fallback loop (piped input). */
file_dialog_result file_dialog_put_file(const char *prompt,
                                         const char *start_path) {
	const char *effective_prompt = (prompt && prompt[0]) ? prompt : "Choose location to save file:";

	/* 2026-06-24 JES #691 Phase C.0.7g Phase 2B: boxen picker. */
	if (boxen_ui_is_active()) {
		file_dialog_result r;
		r.path[0] = '\0';
		r.success = boxen_ui_pick_file(BOXEN_UI_PICK_PUT_FILE,
		                                effective_prompt, start_path, NULL,
		                                r.path, sizeof(r.path));
		return r;
	}

	if (isatty(STDIN_FILENO))
		return file_browser_put_file(effective_prompt, start_path);

	return interactive_file_loop(effective_prompt,
	                             start_path,
	                             false,  /* require_exists */
	                             false,  /* require_file */
	                             false); /* require_dir */
}

/* Routes to two-pane browser (TTY) or fallback loop (piped input). */
file_dialog_result file_dialog_get_folder(const char *prompt,
                                           const char *start_path) {
	const char *effective_prompt = (prompt && prompt[0]) ? prompt : "Select a directory:";

	/* 2026-06-24 JES #691 Phase C.0.7g Phase 2B: boxen picker. */
	if (boxen_ui_is_active()) {
		file_dialog_result r;
		r.path[0] = '\0';
		r.success = boxen_ui_pick_file(BOXEN_UI_PICK_GET_FOLDER,
		                                effective_prompt, start_path, NULL,
		                                r.path, sizeof(r.path));
		return r;
	}

	if (isatty(STDIN_FILENO))
		return file_browser_get_folder(effective_prompt, start_path);

	return interactive_file_loop(effective_prompt,
	                             start_path,
	                             true,   /* require_exists */
	                             false,  /* require_file */
	                             true);  /* require_dir */
}

/* Routes to two-pane browser (TTY) or fallback numbered list (piped input). */
file_dialog_result file_dialog_get_disk(const char *prompt) {
	const char *effective_prompt = (prompt && prompt[0]) ? prompt : "Select a volume:";

	/* 2026-06-24 JES #691 Phase C.0.7g Phase 2B: boxen picker.
	 * The boxen picker browses /Volumes/ on macOS (where mounted disks
	 * appear as directory entries) rather than calling getfsstat. */
	if (boxen_ui_is_active()) {
		file_dialog_result r;
		r.path[0] = '\0';
		r.success = boxen_ui_pick_file(BOXEN_UI_PICK_GET_DISK,
		                                effective_prompt, NULL, NULL,
		                                r.path, sizeof(r.path));
		return r;
	}

	if (isatty(STDIN_FILENO))
		return file_browser_get_disk(effective_prompt);

	/* Fallback: numbered list for piped input */
	file_dialog_result result;
	result.success = false;
	result.path[0] = '\0';

#ifdef __APPLE__
	/* macOS: Use getfsstat to enumerate mounted volumes */
	struct statfs *mounts;
	int count = getfsstat(NULL, 0, MNT_NOWAIT);
	if (count < 0) {
		log_error(LOG_COMP_GENERAL, "file_dialog_get_disk: getfsstat failed");
		return result;
	}

	/* Check for integer overflow in malloc size calculation */
	if (count > 0 && (SIZE_MAX / sizeof(struct statfs)) < (size_t)count) {
		log_error(LOG_COMP_GENERAL, "file_dialog_get_disk: malloc size would overflow");
		return result;
	}

	mounts = malloc((size_t)count * sizeof(struct statfs));
	if (!mounts) {
		log_error(LOG_COMP_GENERAL, "file_dialog_get_disk: malloc failed");
		return result;
	}

	count = getfsstat(mounts, (int)((size_t)count * sizeof(struct statfs)), MNT_NOWAIT);
	if (count < 0) {
		log_error(LOG_COMP_GENERAL, "file_dialog_get_disk: getfsstat failed");
		free(mounts);
		return result;
	}

	/* Display volumes */
	fprintf(stderr, "\n%s\n", effective_prompt);
	fprintf(stderr, "Available volumes:\n");
	for (int i = 0; i < count; i++) {
		fprintf(stderr, "%d. %s (type: %s)\n", i + 1, mounts[i].f_mntonname, mounts[i].f_fstypename);
	}

	/* Prompt for selection */
	fprintf(stderr, "Enter volume number (or 0 to cancel): ");
	fflush(stderr);

	char input_buf[32];
	if (!fgets(input_buf, sizeof(input_buf), stdin)) {
		log_info(LOG_COMP_GENERAL, "Volume selection cancelled (EOF on stdin)");
		free(mounts);
		return result;
	}

	char *endptr;
	errno = 0;
	long selection_long = strtol(input_buf, &endptr, 10);
	int selection = (int)selection_long;

	/* Validate: must be valid integer, in range, no overflow */
	if (errno != 0 || (*endptr != '\n' && *endptr != '\0') ||
	    selection_long != (long)selection || selection < 1 || selection > count) {
		log_info(LOG_COMP_GENERAL, "Volume selection cancelled (invalid selection)");
		free(mounts);
		return result;
	}

	/* Return selected volume path */
	size_t mount_len = strlen(mounts[selection - 1].f_mntonname);
	if (mount_len >= sizeof(result.path)) {
		log_warn(LOG_COMP_GENERAL, "file_dialog: Mount path truncated (length %zu >= %zu)",
		         mount_len, sizeof(result.path));
	}
	strncpy(result.path, mounts[selection - 1].f_mntonname, sizeof(result.path) - 1);
	result.path[sizeof(result.path) - 1] = '\0';
	result.success = true;

	free(mounts);
#else
	/* Linux/other: Simple root directory selection */
	fprintf(stderr, "\n%s\n", effective_prompt);
	fprintf(stderr, "Volume selection:\n");
	fprintf(stderr, "1. / (root)\n");
	fprintf(stderr, "Enter 1 to select root, or 0 to cancel: ");
	fflush(stderr);

	char input_buf[32];
	if (!fgets(input_buf, sizeof(input_buf), stdin)) {
		log_info(LOG_COMP_GENERAL, "Volume selection cancelled (EOF on stdin)");
		return result;
	}

	char *endptr;
	errno = 0;
	long selection_long = strtol(input_buf, &endptr, 10);
	int selection = (int)selection_long;

	/* Validate: must be valid integer, equal to 1, no overflow */
	if (errno != 0 || (*endptr != '\n' && *endptr != '\0') ||
	    selection_long != (long)selection || selection != 1) {
		log_info(LOG_COMP_GENERAL, "Volume selection cancelled (invalid selection)");
		return result;
	}

	strncpy(result.path, "/", sizeof(result.path) - 1);
	result.path[sizeof(result.path) - 1] = '\0';
	result.success = true;
#endif

	return result;
}
