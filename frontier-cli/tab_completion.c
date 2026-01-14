/*
 * tab_completion.c - Interactive file path completion implementation
 *
 * Phase 2B.2: Tab Completion Engine
 *
 * Implements directory enumeration and visual completion menu.
 */

#include "tab_completion.h"
#include "../Common/headers/logging.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Helper: Compare file entries for sorting (directories first, then alphabetical) */
static int compare_file_entries(const void *a, const void *b) {
	const file_entry *fa = (const file_entry *)a;
	const file_entry *fb = (const file_entry *)b;

	/* Directories come before files */
	if (fa->is_directory && !fb->is_directory) {
		return -1;
	}
	if (!fa->is_directory && fb->is_directory) {
		return 1;
	}

	/* Within same type, sort alphabetically (case-insensitive) */
	return strcasecmp(fa->name, fb->name);
}

/* Find matching files in directory */
size_t tab_completion_find_matches(const char *directory,
                                   const char *prefix,
                                   file_entry *entries,
                                   size_t max_entries,
                                   bool show_hidden) {
	if (!directory || !entries || max_entries == 0) {
		return 0;
	}

	DIR *dir = opendir(directory);
	if (!dir) {
		log_debug(LOG_COMP_GENERAL, "tab_completion_find_matches: opendir failed for '%s'", directory);
		return 0;
	}

	size_t count = 0;
	struct dirent *entry;
	size_t prefix_len = prefix ? strlen(prefix) : 0;

	while ((entry = readdir(dir)) != NULL && count < max_entries) {
		/* Skip "." and ".." */
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		/* Skip hidden files if not requested */
		if (!show_hidden && entry->d_name[0] == '.') {
			continue;
		}

		/* Check prefix match */
		if (prefix_len > 0 && strncmp(entry->d_name, prefix, prefix_len) != 0) {
			continue;
		}

		/* Build full path for stat() */
		char full_path[1024];
		snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);

		/* Get file metadata */
		struct stat st;
		if (stat(full_path, &st) != 0) {
			log_debug(LOG_COMP_GENERAL, "tab_completion_find_matches: stat failed for '%s'", full_path);
			continue;
		}

		/* Store entry */
		strncpy(entries[count].name, entry->d_name, sizeof(entries[count].name) - 1);
		entries[count].name[sizeof(entries[count].name) - 1] = '\0';
		entries[count].is_directory = S_ISDIR(st.st_mode);
		entries[count].size = st.st_size;
		entries[count].mtime = st.st_mtime;
		count++;
	}

	closedir(dir);

	/* Sort results (directories first, then alphabetical) */
	if (count > 0) {
		qsort(entries, count, sizeof(file_entry), compare_file_entries);
	}

	return count;
}

/* Format file size for display (bytes, KB, MB, GB) */
static void format_file_size(off_t size, char *buffer, size_t buffer_size) {
	if (size < 1024) {
		snprintf(buffer, buffer_size, "%lld B", (long long)size);
	} else if (size < 1024 * 1024) {
		snprintf(buffer, buffer_size, "%.1f KB", size / 1024.0);
	} else if (size < 1024 * 1024 * 1024) {
		snprintf(buffer, buffer_size, "%.1f MB", size / (1024.0 * 1024.0));
	} else {
		snprintf(buffer, buffer_size, "%.1f GB", size / (1024.0 * 1024.0 * 1024.0));
	}
}

/* Display completion menu and handle navigation */
tab_completion_result tab_completion_show_menu(const file_entry *entries,
                                               size_t count,
                                               const char *directory,
                                               terminal_state *terminal) {
	tab_completion_result result;
	result.completed = false;
	result.selected_path[0] = '\0';

	if (!entries || count == 0 || !directory || !terminal) {
		return result;
	}

	/* Enable raw mode for arrow key detection */
	if (!terminal_enable_raw_mode(terminal)) {
		log_error(LOG_COMP_GENERAL, "tab_completion_show_menu: failed to enable raw mode");
		return result;
	}

	/* Display header */
	printf("\n");
	terminal_bold_text("Tab Completion:");
	printf(" %zu match%s found\n", count, (count == 1) ? "" : "es");
	printf("Use arrow keys to navigate, Enter to select, Esc to cancel\n\n");

	size_t selected = 0;
	bool done = false;

	while (!done) {
		/* Display all entries with selection indicator */
		for (size_t i = 0; i < count; i++) {
			/* Selection indicator */
			if (i == selected) {
				printf("> ");
			} else {
				printf("  ");
			}

			/* Directory indicator */
			if (entries[i].is_directory) {
				printf("[DIR] ");
			} else {
				printf("      ");
			}

			/* Filename */
			printf("%-40s", entries[i].name);

			/* Size (for files only) */
			if (!entries[i].is_directory) {
				char size_str[32];
				format_file_size(entries[i].size, size_str, sizeof(size_str));
				printf(" %10s", size_str);
			}

			printf("\n");
		}

		/* Read navigation key */
		key_input key = terminal_read_key();

		/* Clear display for next iteration */
		terminal_move_cursor_up((int)count);
		for (size_t i = 0; i < count; i++) {
			terminal_clear_line();
			terminal_move_cursor_down(1);
		}
		terminal_move_cursor_up((int)count);

		/* Handle key event */
		switch (key.type) {
			case KEY_ARROW_UP:
				if (selected > 0) {
					selected--;
				}
				break;

			case KEY_ARROW_DOWN:
				if (selected < count - 1) {
					selected++;
				}
				break;

			case KEY_ENTER:
				/* Build full path */
				snprintf(result.selected_path, sizeof(result.selected_path),
				         "%s/%s", directory, entries[selected].name);
				result.completed = true;
				done = true;
				break;

			case KEY_ESCAPE:
			case KEY_CTRL_C:
				/* Cancel completion */
				done = true;
				break;

			default:
				/* Ignore other keys */
				break;
		}
	}

	/* Clear menu display */
	for (size_t i = 0; i < count; i++) {
		terminal_clear_line();
		terminal_move_cursor_down(1);
	}
	terminal_move_cursor_up((int)count);

	/* Disable raw mode */
	terminal_disable_raw_mode(terminal);

	return result;
}
