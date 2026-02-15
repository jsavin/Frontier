/*
 * file_browser.c - Two-pane ranger-style file browser for terminal
 *
 * Provides a full-screen visual file browser with:
 * - Left pane: current directory listing with arrow key navigation
 * - Right pane: preview of selected entry (dir contents or file info)
 * - Type-ahead selection (like Finder list view)
 * - Mode-specific behavior for getFile, putFile, getFolder, getDisk
 *
 * All output goes to stderr (consistent with existing dialog pattern).
 */

#include "file_browser.h"
#include "../Common/headers/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

/* SIGWINCH handling */
static volatile sig_atomic_t sigwinch_received = 0;
static struct sigaction old_sigwinch_action;

static void sigwinch_handler(int sig) {
	(void)sig;
	sigwinch_received = 1;
}

static void install_sigwinch_handler(void) {
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigwinch_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGWINCH, &sa, &old_sigwinch_action);
}

static void restore_sigwinch_handler(void) {
	sigaction(SIGWINCH, &old_sigwinch_action, NULL);
}

/* Format file size in human-readable units */
static void format_file_size(off_t size, char *buf, size_t buf_size) {
	if (size < 1024) {
		snprintf(buf, buf_size, "%lld B", (long long)size);
	} else if (size < 1024 * 1024) {
		snprintf(buf, buf_size, "%.1f KB", (double)size / 1024.0);
	} else if (size < 1024LL * 1024 * 1024) {
		snprintf(buf, buf_size, "%.1f MB", (double)size / (1024.0 * 1024.0));
	} else {
		snprintf(buf, buf_size, "%.1f GB", (double)size / (1024.0 * 1024.0 * 1024.0));
	}
}

/* Format permission bits as rwx string */
static void format_permissions(mode_t mode, char *buf, size_t buf_size) {
	snprintf(buf, buf_size, "%c%c%c%c%c%c%c%c%c",
		(mode & S_IRUSR) ? 'r' : '-',
		(mode & S_IWUSR) ? 'w' : '-',
		(mode & S_IXUSR) ? 'x' : '-',
		(mode & S_IRGRP) ? 'r' : '-',
		(mode & S_IWGRP) ? 'w' : '-',
		(mode & S_IXGRP) ? 'x' : '-',
		(mode & S_IROTH) ? 'r' : '-',
		(mode & S_IWOTH) ? 'w' : '-',
		(mode & S_IXOTH) ? 'x' : '-');
}

static void browser_load_directory(browser_state *state);
static void browser_load_preview(browser_state *state);

/* Compute the file-list height: half the terminal but no less than
 * min(15, available_rows).  Chrome rows = prompt + breadcrumb +
 * top separator + bottom separator + status bar (+1 for putFile filename bar). */
static int compute_list_height(int term_rows, browser_mode mode) {
	int chrome = (mode == BROWSER_PUT_FILE) ? 6 : 5;
	int available = term_rows - chrome;
	if (available < 1) available = 1;

	int half = term_rows / 2;
	int minimum = (15 < available) ? 15 : available;
	int height = (half > minimum) ? half : minimum;
	if (height > available)
		height = available;
	return height;
}

/* Initialize browser state */
static void browser_init_state(browser_state *state, browser_mode mode,
                               const char *prompt, const char *start_path,
                               const char *type_filter) {
	memset(state, 0, sizeof(*state));
	state->mode = mode;
	state->running = true;

	if (prompt != NULL && prompt[0] != '\0') {
		strncpy(state->prompt, prompt, sizeof(state->prompt) - 1);
		state->prompt[sizeof(state->prompt) - 1] = '\0';
	} else {
		strncpy(state->prompt, "Select a file:", sizeof(state->prompt) - 1);
		state->prompt[sizeof(state->prompt) - 1] = '\0';
	}

	if (type_filter != NULL && type_filter[0] != '\0') {
		strncpy(state->type_filter, type_filter, sizeof(state->type_filter) - 1);
		state->type_filter[sizeof(state->type_filter) - 1] = '\0';
	}

	if (start_path != NULL && start_path[0] != '\0') {
		strncpy(state->current_dir, start_path, sizeof(state->current_dir) - 1);
		state->current_dir[sizeof(state->current_dir) - 1] = '\0';

		/* Check whether start_path is a directory or contains a filename */
		struct stat st;
		bool is_dir = (stat(state->current_dir, &st) == 0 && S_ISDIR(st.st_mode));

		if (!is_dir) {
			/* Either a file or a non-existent path — treat the last
			 * component as a filename and use the parent as current_dir */
			char *last_slash = strrchr(state->current_dir, '/');
			if (last_slash != NULL) {
				const char *filename_part = last_slash + 1;

				/* For putFile, pre-populate the filename buffer */
				if (mode == BROWSER_PUT_FILE && filename_part[0] != '\0') {
					strncpy(state->filename_buf, filename_part,
					        sizeof(state->filename_buf) - 1);
					state->filename_buf[sizeof(state->filename_buf) - 1] = '\0';
					state->filename_len = strlen(state->filename_buf);
					state->filename_cursor_pos = state->filename_len;
				}

				if (last_slash != state->current_dir) {
					*last_slash = '\0';
				} else {
					state->current_dir[1] = '\0';
				}
			}
		}
	} else {
		getcwd(state->current_dir, sizeof(state->current_dir));
	}

	/* Remove trailing slash unless root */
	size_t len = strlen(state->current_dir);
	if (len > 1 && state->current_dir[len - 1] == '/') {
		state->current_dir[len - 1] = '\0';
	}

	terminal_get_size(&state->term_rows, &state->term_cols);

	/* Compute layout */
	if (state->term_cols < 40) {
		/* Single-pane mode for very narrow terminals */
		state->left_width = state->term_cols;
		state->right_width = 0;
	} else {
		state->left_width = state->term_cols / 2;
		state->right_width = state->term_cols - state->left_width - 1;
	}

	state->list_height = compute_list_height(state->term_rows, mode);

	log_info(LOG_COMP_GENERAL, "file_browser: initialized mode=%d dir=%s size=%dx%d",
	         mode, state->current_dir, state->term_cols, state->term_rows);
}

/* Recompute layout after terminal resize */
static void browser_recompute_layout(browser_state *state) {
	terminal_get_size(&state->term_rows, &state->term_cols);

	if (state->term_cols < 40) {
		state->left_width = state->term_cols;
		state->right_width = 0;
	} else {
		state->left_width = state->term_cols / 2;
		state->right_width = state->term_cols - state->left_width - 1;
	}

	state->list_height = compute_list_height(state->term_rows, state->mode);
}

/* Check if file entry matches the type filter */
static bool browser_matches_filter(const browser_state *state, const file_entry *entry) {
	if (state->type_filter[0] == '\0' || entry->is_directory) {
		return true;
	}

	const char *dot = strrchr(entry->name, '.');
	if (dot == NULL) {
		return false;
	}
	dot++; /* skip the dot */

	return (strcasecmp(dot, state->type_filter) == 0);
}

/* Load directory contents into the entries array */
static void browser_load_directory(browser_state *state) {
	file_entry all_entries[MAX_COMPLETION_CANDIDATES];

	size_t count = tab_completion_find_matches(
		state->current_dir, NULL, all_entries, MAX_COMPLETION_CANDIDATES, false);

	/* Apply type filter: show directories + matching files */
	if (state->type_filter[0] != '\0') {
		size_t filtered = 0;
		for (size_t i = 0; i < count; i++) {
			if (browser_matches_filter(state, &all_entries[i])) {
				state->entries[filtered++] = all_entries[i];
			}
		}
		state->entry_count = filtered;
	} else {
		memcpy(state->entries, all_entries, count * sizeof(file_entry));
		state->entry_count = count;
	}

	state->cursor = 0;
	state->scroll_offset = 0;

	log_debug(LOG_COMP_GENERAL, "file_browser: loaded %zu entries from %s",
	          state->entry_count, state->current_dir);

	browser_load_preview(state);
}

/* Load preview for the currently selected entry */
static void browser_load_preview(browser_state *state) {
	if (state->entry_count == 0) {
		state->preview_count = 0;
		return;
	}

	file_entry *selected = &state->entries[state->cursor];

	if (selected->is_directory) {
		/* Preview directory contents */
		char subdir[PATH_MAX];
		if (state->mode == BROWSER_GET_DISK) {
			/* Disk entries store full paths in name field */
			snprintf(subdir, sizeof(subdir), "%s", selected->name);
		} else if (strcmp(state->current_dir, "/") == 0) {
			snprintf(subdir, sizeof(subdir), "/%s", selected->name);
		} else {
			snprintf(subdir, sizeof(subdir), "%s/%s", state->current_dir, selected->name);
		}
		file_entry raw_preview[MAX_COMPLETION_CANDIDATES];
		size_t raw_count = tab_completion_find_matches(
			subdir, NULL, raw_preview, MAX_COMPLETION_CANDIDATES, false);

		/* Apply type filter to preview too */
		if (state->type_filter[0] != '\0') {
			size_t filtered = 0;
			for (size_t i = 0; i < raw_count && filtered < MAX_COMPLETION_CANDIDATES; i++) {
				if (browser_matches_filter(state, &raw_preview[i])) {
					state->preview_entries[filtered++] = raw_preview[i];
				}
			}
			state->preview_count = filtered;
		} else {
			memcpy(state->preview_entries, raw_preview, raw_count * sizeof(file_entry));
			state->preview_count = raw_count;
		}
	} else {
		/* File selected - preview_count = 0 signals file info mode */
		state->preview_count = 0;
	}
}

/* Draw a string to stderr, truncated or padded to exactly `width` characters */
static void draw_padded(const char *str, int width) {
	int len = (int)strlen(str);
	if (len >= width) {
		fwrite(str, 1, width, stderr);
	} else {
		fputs(str, stderr);
		for (int i = len; i < width; i++) {
			fputc(' ', stderr);
		}
	}
}

/* Draw the browser UI.  Uses absolute cursor positioning for every
 * row so that the browser region is repainted in-place without
 * scrolling the terminal. */
static void browser_draw(browser_state *state) {
	/* Total rows: list_height + chrome.  PUT_FILE has an extra filename bar.
	 * Chrome: prompt, breadcrumb, top sep, bot sep, status (+ filename for putFile) */
	int chrome = (state->mode == BROWSER_PUT_FILE) ? 6 : 5;
	int total_rows = state->list_height + chrome;
	int start_row = state->term_rows - total_rows + 1;
	if (start_row < 1) start_row = 1;

	int cur_row = start_row;
	int cols = state->term_cols;

	/* Row 1: Prompt with mode label */
	const char *mode_label;
	switch (state->mode) {
	case BROWSER_GET_FILE:   mode_label = "[Open]";   break;
	case BROWSER_PUT_FILE:   mode_label = "[Save]";   break;
	case BROWSER_GET_FOLDER: mode_label = "[Folder]"; break;
	case BROWSER_GET_DISK:   mode_label = "[Disk]";   break;
	default:                 mode_label = "";          break;
	}

	int label_len = (int)strlen(mode_label);
	int prompt_space = cols - label_len - 2;
	if (prompt_space < 10) prompt_space = 10;

	terminal_move_cursor(cur_row, 1);
	fprintf(stderr, "\x1b[2K");
	fprintf(stderr, " ");
	draw_padded(state->prompt, prompt_space);
	fprintf(stderr, " %s", mode_label);
	cur_row++;

	/* Row 2: Breadcrumb */
	terminal_move_cursor(cur_row, 1);
	fprintf(stderr, "\x1b[2K");
	fprintf(stderr, " ");
	int bread_width = cols - 2;
	int dir_len = (int)strlen(state->current_dir);
	if (dir_len > bread_width) {
		fprintf(stderr, "...");
		fprintf(stderr, "%s", state->current_dir + (dir_len - bread_width + 3));
	} else {
		draw_padded(state->current_dir, bread_width);
	}
	cur_row++;

	/* Row 3: Top separator */
	terminal_move_cursor(cur_row, 1);
	fprintf(stderr, "\x1b[2K");
	for (int i = 0; i < state->left_width; i++) {
		fputc('-', stderr);
	}
	if (state->right_width > 0) {
		fputc('+', stderr);
		for (int i = 0; i < state->right_width; i++) {
			fputc('-', stderr);
		}
	}
	cur_row++;

	/* Rows 4..4+list_height-1: Two-pane listing */
	for (int row = 0; row < state->list_height; row++) {
		terminal_move_cursor(cur_row, 1);
		fprintf(stderr, "\x1b[2K");

		size_t left_idx = state->scroll_offset + row;

		/* Left pane */
		if (left_idx < state->entry_count) {
			file_entry *entry = &state->entries[left_idx];
			bool is_cursor = (left_idx == state->cursor);
			char line[512];

			if (entry->is_directory) {
				snprintf(line, sizeof(line), "%s%s/",
				         is_cursor ? "> " : "  ", entry->name);
			} else {
				snprintf(line, sizeof(line), "%s%s",
				         is_cursor ? "> " : "  ", entry->name);
			}

			if (is_cursor) {
				terminal_start_inverted();
			}
			draw_padded(line, state->left_width);
			if (is_cursor) {
				terminal_end_inverted();
			}
		} else {
			/* Empty row in left pane */
			draw_padded("", state->left_width);
		}

		/* Separator column */
		if (state->right_width > 0) {
			fputc('|', stderr);

			/* Right pane */
			size_t sel_idx = state->cursor;
			if (sel_idx < state->entry_count) {
				file_entry *selected = &state->entries[sel_idx];

				if (selected->is_directory) {
					/* Preview directory contents */
					if ((size_t)row < state->preview_count) {
						char pline[512];
						file_entry *pe = &state->preview_entries[row];
						if (pe->is_directory) {
							snprintf(pline, sizeof(pline), "  %s/", pe->name);
						} else {
							snprintf(pline, sizeof(pline), "  %s", pe->name);
						}
						draw_padded(pline, state->right_width);
					} else {
						draw_padded("", state->right_width);
					}
				} else {
					/* File info in right pane */
					char info_line[512];
					info_line[0] = '\0';

					if (row == 0) {
						snprintf(info_line, sizeof(info_line), "  %s", selected->name);
					} else if (row == 1) {
						/* Blank separator line */
					} else if (row == 2) {
						char size_buf[32];
						format_file_size(selected->size, size_buf, sizeof(size_buf));
						snprintf(info_line, sizeof(info_line), "  Size: %s", size_buf);
					} else if (row == 3) {
						char time_buf[64];
						struct tm *tm_info = localtime(&selected->mtime);
						strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M", tm_info);
						snprintf(info_line, sizeof(info_line), "  Modified: %s", time_buf);
					} else if (row == 4) {
						/* Get full path for stat to get permissions */
						char full_path[PATH_MAX];
						if (strcmp(state->current_dir, "/") == 0) {
							snprintf(full_path, sizeof(full_path), "/%s", selected->name);
						} else {
							snprintf(full_path, sizeof(full_path), "%s/%s",
							         state->current_dir, selected->name);
						}
						struct stat st;
						if (stat(full_path, &st) == 0) {
							char perm_buf[16];
							format_permissions(st.st_mode, perm_buf, sizeof(perm_buf));
							snprintf(info_line, sizeof(info_line), "  Perms: %s", perm_buf);
						}
					}

					draw_padded(info_line, state->right_width);
				}
			} else {
				draw_padded("", state->right_width);
			}
		}

		cur_row++;
	}

	/* Bottom separator */
	terminal_move_cursor(cur_row, 1);
	fprintf(stderr, "\x1b[2K");
	for (int i = 0; i < cols; i++) {
		fputc('-', stderr);
	}
	cur_row++;

	/* For PUT_FILE: always show filename bar, then status bar below it */
	if (state->mode == BROWSER_PUT_FILE) {
		/* Filename bar */
		terminal_move_cursor(cur_row, 1);
		fprintf(stderr, "\x1b[2K");
		const char *label = " Save as: ";
		int label_len = (int)strlen(label);
		fprintf(stderr, "%s", label);
		if (state->filename_editing) {
			terminal_start_inverted();
		}
		int name_width = cols - label_len;
		if (name_width < 1) name_width = 1;
		draw_padded(state->filename_buf, name_width);
		if (state->filename_editing) {
			terminal_end_inverted();
		}
		cur_row++;

		/* Status/hint bar */
		terminal_move_cursor(cur_row, 1);
		fprintf(stderr, "\x1b[2K");
		char status[512];
		const char *hints;
		if (state->confirm_overwrite) {
			snprintf(status, sizeof(status),
			         " \x1b[1m\"%s\" already exists. Overwrite? (Y/n)\x1b[22m",
			         state->filename_buf);
			hints = NULL;
		} else if (state->filename_editing) {
			hints = "Enter:Confirm  Esc:Back to browser";
		} else {
			hints = "Up/Dn:Nav  Left:Parent  Right:Open  Enter:Select  Esc:Cancel";
		}
		if (hints) {
			snprintf(status, sizeof(status), " %s", hints);
		}
		draw_padded(status, cols);

		/* Position visible cursor inside the filename field when editing */
		if (state->filename_editing) {
			terminal_show_cursor();
			terminal_move_cursor(cur_row - 1, label_len + (int)state->filename_cursor_pos + 1);
		} else {
			terminal_hide_cursor();
		}
	} else {
		/* Status bar for non-putFile modes */
		terminal_move_cursor(cur_row, 1);
		fprintf(stderr, "\x1b[2K");
		char status[512];
		const char *hints;
		switch (state->mode) {
		case BROWSER_GET_FILE:
			hints = "Up/Dn:Nav  Left:Parent  Right/Enter:Open  Esc:Cancel";
			break;
		case BROWSER_GET_FOLDER:
			hints = "Up/Dn:Nav  Left:Parent  Right:Open  Enter:Select  Esc:Cancel";
			break;
		case BROWSER_GET_DISK:
			hints = "Up/Dn:Nav  Enter:Select  Esc:Cancel";
			break;
		default:
			hints = "Esc:Cancel";
			break;
		}

		/* Truncate current_dir for status if needed */
		int hints_len = (int)strlen(hints);
		int path_space = cols - hints_len - 4;
		if (path_space < 10) path_space = 10;

		int dlen = (int)strlen(state->current_dir);
		if (dlen > path_space) {
			snprintf(status, sizeof(status), " ...%s  %s",
			         state->current_dir + (dlen - path_space + 4), hints);
		} else {
			snprintf(status, sizeof(status), " %s  %s", state->current_dir, hints);
		}
		draw_padded(status, cols);
	}

	fflush(stderr);
}

/* Navigate to parent directory */
static void browser_navigate_to_parent(browser_state *state) {
	if (strcmp(state->current_dir, "/") == 0) {
		return; /* Already at root */
	}

	char *last_slash = strrchr(state->current_dir, '/');
	if (last_slash == state->current_dir) {
		/* Parent is root */
		state->current_dir[1] = '\0';
	} else if (last_slash != NULL) {
		*last_slash = '\0';
	}

	browser_load_directory(state);
}

/* Descend into the selected directory */
static void browser_descend(browser_state *state) {
	if (state->entry_count == 0) {
		return;
	}

	file_entry *selected = &state->entries[state->cursor];
	if (!selected->is_directory) {
		return;
	}

	size_t cur_len = strlen(state->current_dir);
	size_t name_len = strlen(selected->name);

	if (cur_len + 1 + name_len >= PATH_MAX) {
		return; /* Path too long */
	}

	if (strcmp(state->current_dir, "/") == 0) {
		snprintf(state->current_dir + 1, sizeof(state->current_dir) - 1,
		         "%s", selected->name);
	} else {
		state->current_dir[cur_len] = '/';
		strncpy(state->current_dir + cur_len + 1, selected->name,
		        sizeof(state->current_dir) - cur_len - 2);
		state->current_dir[sizeof(state->current_dir) - 1] = '\0';
	}

	browser_load_directory(state);
}

/* Build full path for the currently selected entry */
static void browser_build_selected_path(const browser_state *state, char *path_buf, size_t buf_size) {
	if (state->entry_count == 0) {
		path_buf[0] = '\0';
		return;
	}

	const file_entry *selected = &state->entries[state->cursor];

	if (strcmp(state->current_dir, "/") == 0) {
		snprintf(path_buf, buf_size, "/%s", selected->name);
	} else {
		snprintf(path_buf, buf_size, "%s/%s", state->current_dir, selected->name);
	}
}

/* Handle key input */
static void browser_handle_key(browser_state *state, key_input key, file_dialog_result *result) {
	/* Handle overwrite confirmation prompt (y/n) */
	if (state->confirm_overwrite) {
		if (key.type == KEY_ENTER ||
		    (key.type == KEY_CHAR && (key.ch == 'y' || key.ch == 'Y'))) {
			/* Confirmed — build path and return */
			if (strcmp(state->current_dir, "/") == 0) {
				snprintf(result->path, sizeof(result->path), "/%s", state->filename_buf);
			} else {
				snprintf(result->path, sizeof(result->path), "%s/%s",
				         state->current_dir, state->filename_buf);
			}
			result->success = true;
			state->running = false;
		} else if (key.type == KEY_ESCAPE ||
		           (key.type == KEY_CHAR && (key.ch == 'n' || key.ch == 'N'))) {
			/* Declined — back to filename editing */
			state->confirm_overwrite = false;
		}
		/* Ignore all other keys while confirming */
		return;
	}

	/* Handle filename editing mode for putFile */
	if (state->filename_editing) {
		switch (key.type) {
		case KEY_ENTER:
			if (state->filename_len > 0) {
				/* Build candidate path and check if file exists */
				char candidate[PATH_MAX];
				if (strcmp(state->current_dir, "/") == 0) {
					snprintf(candidate, sizeof(candidate), "/%s", state->filename_buf);
				} else {
					snprintf(candidate, sizeof(candidate), "%s/%s",
					         state->current_dir, state->filename_buf);
				}
				struct stat st;
				if (stat(candidate, &st) == 0 && !S_ISDIR(st.st_mode)) {
					/* File exists — ask for confirmation */
					state->confirm_overwrite = true;
				} else {
					/* New file — accept immediately */
					strncpy(result->path, candidate, sizeof(result->path) - 1);
					result->path[sizeof(result->path) - 1] = '\0';
					result->success = true;
					state->running = false;
				}
			}
			return;

		case KEY_ESCAPE:
			/* Return to browsing mode, but keep filename for later */
			state->filename_editing = false;
			return;

		case KEY_BACKSPACE:
			if (state->filename_cursor_pos > 0) {
				/* Remove character before cursor */
				memmove(&state->filename_buf[state->filename_cursor_pos - 1],
				        &state->filename_buf[state->filename_cursor_pos],
				        state->filename_len - state->filename_cursor_pos + 1);
				state->filename_cursor_pos--;
				state->filename_len--;
			}
			return;

		case KEY_ARROW_LEFT:
			if (state->filename_cursor_pos > 0) {
				state->filename_cursor_pos--;
			}
			return;

		case KEY_ARROW_RIGHT:
			if (state->filename_cursor_pos < state->filename_len) {
				state->filename_cursor_pos++;
			}
			return;

		case KEY_CHAR:
			if (state->filename_len < sizeof(state->filename_buf) - 1) {
				/* Insert character at cursor position */
				memmove(&state->filename_buf[state->filename_cursor_pos + 1],
				        &state->filename_buf[state->filename_cursor_pos],
				        state->filename_len - state->filename_cursor_pos + 1);
				state->filename_buf[state->filename_cursor_pos] = key.ch;
				state->filename_cursor_pos++;
				state->filename_len++;
			}
			return;

		default:
			return;
		}
	}

	/* Normal browsing mode */
	switch (key.type) {
	case KEY_ARROW_UP:
		if (state->cursor > 0) {
			state->cursor--;
			if (state->cursor < state->scroll_offset) {
				state->scroll_offset = state->cursor;
			}
			browser_load_preview(state);
		}
		break;

	case KEY_ARROW_DOWN:
		if (state->entry_count > 0 && state->cursor < state->entry_count - 1) {
			state->cursor++;
			if (state->cursor >= state->scroll_offset + (size_t)state->list_height) {
				state->scroll_offset = state->cursor - state->list_height + 1;
			}
			browser_load_preview(state);
		}
		break;

	case KEY_ARROW_LEFT:
		if (state->mode != BROWSER_GET_DISK) {
			browser_navigate_to_parent(state);
		}
		break;

	case KEY_ARROW_RIGHT:
		if (state->entry_count > 0 && state->entries[state->cursor].is_directory) {
			browser_descend(state);
		}
		break;

	case KEY_ENTER:
		if (state->entry_count == 0) {
			break;
		}

		switch (state->mode) {
		case BROWSER_GET_FILE:
			if (state->entries[state->cursor].is_directory) {
				browser_descend(state);
			} else {
				browser_build_selected_path(state, result->path, sizeof(result->path));
				result->success = true;
				state->running = false;
			}
			break;

		case BROWSER_PUT_FILE:
			if (state->entries[state->cursor].is_directory) {
				/* Enter on folder = select this folder, start editing filename */
				browser_descend(state);
				state->filename_editing = true;
			} else {
				/* Enter on file = use that name as default (overwrite) */
				strncpy(state->filename_buf, state->entries[state->cursor].name,
				        sizeof(state->filename_buf) - 1);
				state->filename_buf[sizeof(state->filename_buf) - 1] = '\0';
				state->filename_len = strlen(state->filename_buf);
				state->filename_cursor_pos = state->filename_len;
				state->filename_editing = true;
			}
			break;

		case BROWSER_GET_FOLDER:
			if (state->entries[state->cursor].is_directory) {
				browser_build_selected_path(state, result->path, sizeof(result->path));
				result->success = true;
				state->running = false;
			}
			break;

		case BROWSER_GET_DISK:
			/* Disk entries store full mount paths in name field */
			strncpy(result->path, state->entries[state->cursor].name,
			        sizeof(result->path) - 1);
			result->path[sizeof(result->path) - 1] = '\0';
			result->success = true;
			state->running = false;
			break;
		}
		break;

	case KEY_ESCAPE:
	case KEY_CTRL_C:
		result->success = false;
		result->path[0] = '\0';
		state->running = false;
		break;

	case KEY_CHAR: {
		/* Type-ahead selection */
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		/* Reset buffer if more than 1 second since last keystroke */
		double elapsed = (now.tv_sec - state->typeahead_time.tv_sec) +
		                 (now.tv_nsec - state->typeahead_time.tv_nsec) / 1e9;
		if (elapsed > 1.0) {
			state->typeahead_len = 0;
		}

		/* Append character to typeahead buffer */
		if (state->typeahead_len < sizeof(state->typeahead_buf) - 1) {
			state->typeahead_buf[state->typeahead_len++] = key.ch;
			state->typeahead_buf[state->typeahead_len] = '\0';
		} else {
			/* Buffer full - reset and start new search from this character */
			state->typeahead_buf[0] = key.ch;
			state->typeahead_buf[1] = '\0';
			state->typeahead_len = 1;
		}
		state->typeahead_time = now;

		/* Find first matching entry (case-insensitive) */
		for (size_t i = 0; i < state->entry_count; i++) {
			if (strncasecmp(state->entries[i].name, state->typeahead_buf,
			                state->typeahead_len) == 0) {
				state->cursor = i;
				/* Adjust scroll to keep cursor visible */
				if (state->cursor < state->scroll_offset) {
					state->scroll_offset = state->cursor;
				} else if (state->cursor >= state->scroll_offset + (size_t)state->list_height) {
					state->scroll_offset = state->cursor - state->list_height + 1;
				}
				browser_load_preview(state);
				break;
			}
		}
		break;
	}

	default:
		break;
	}
}

/* Run the browser main loop.
 * If skip_initial_load is true, entries are already populated (used by disk mode). */
static file_dialog_result browser_run_internal(browser_state *state, bool skip_initial_load) {
	file_dialog_result result;
	memset(&result, 0, sizeof(result));

	if (!terminal_init(&state->terminal)) {
		/* No cleanup needed - terminal_init does not allocate on failure */
		log_error(LOG_COMP_GENERAL, "file_browser: terminal_init failed, cancelling");
		result.success = false;
		return result;
	}

	if (!terminal_enable_raw_mode(&state->terminal)) {
		log_error(LOG_COMP_GENERAL, "file_browser: raw mode failed, cancelling");
		terminal_cleanup(&state->terminal);
		result.success = false;
		return result;
	}

	/* Require minimum terminal size for usable display */
	if (state->term_rows < 10 || state->term_cols < 30) {
		log_error(LOG_COMP_GENERAL,
		          "file_browser: terminal too small (%dx%d), need at least 30x10",
		          state->term_cols, state->term_rows);
		fprintf(stderr, "Terminal too small for file browser (%dx%d, need 30x10)\n",
		        state->term_cols, state->term_rows);
		terminal_cleanup(&state->terminal);
		result.success = false;
		return result;
	}

	terminal_hide_cursor();
	install_sigwinch_handler();

	/* Scroll the terminal down to make room for the browser region,
	 * then draw in-place using absolute cursor positioning. */
	int chrome = (state->mode == BROWSER_PUT_FILE) ? 6 : 5;
	int total_rows = state->list_height + chrome;
	for (int i = 0; i < total_rows; i++) {
		fputc('\n', stderr);
	}
	fflush(stderr);

	if (!skip_initial_load) {
		browser_load_directory(state);
	}

	while (state->running) {
		if (sigwinch_received) {
			sigwinch_received = 0;
			browser_recompute_layout(state);
		}

		browser_draw(state);

		key_input key = terminal_read_key();
		browser_handle_key(state, key, &result);
	}

	/* Clear the browser region before exiting */
	int start_row = state->term_rows - total_rows + 1;
	if (start_row < 1) start_row = 1;
	for (int r = start_row; r <= state->term_rows; r++) {
		terminal_move_cursor(r, 1);
		fprintf(stderr, "\x1b[2K");
	}
	terminal_move_cursor(start_row, 1);
	fflush(stderr);

	terminal_show_cursor();
	terminal_cleanup(&state->terminal);
	restore_sigwinch_handler();

	if (result.success) {
		log_info(LOG_COMP_GENERAL, "file_browser: selected path: %s", result.path);
	} else {
		log_info(LOG_COMP_GENERAL, "file_browser: cancelled");
	}

	return result;
}

static file_dialog_result browser_run(browser_state *state) {
	return browser_run_internal(state, false);
}

/* --- Public entry points --- */

file_dialog_result file_browser_get_file(const char *prompt,
                                          const char *start_path,
                                          const char *type_filter) {
	browser_state state;
	browser_init_state(&state, BROWSER_GET_FILE, prompt, start_path, type_filter);
	return browser_run(&state);
}

file_dialog_result file_browser_put_file(const char *prompt,
                                          const char *start_path) {
	browser_state state;
	browser_init_state(&state, BROWSER_PUT_FILE, prompt, start_path, NULL);
	return browser_run(&state);
}

file_dialog_result file_browser_get_folder(const char *prompt,
                                            const char *start_path) {
	browser_state state;
	browser_init_state(&state, BROWSER_GET_FOLDER, prompt, start_path, NULL);
	return browser_run(&state);
}

file_dialog_result file_browser_get_disk(const char *prompt) {
	browser_state state;
	browser_init_state(&state, BROWSER_GET_DISK, prompt, NULL, NULL);

	/* Override default prompt */
	if (prompt == NULL || prompt[0] == '\0') {
		strncpy(state.prompt, "Select a disk:", sizeof(state.prompt) - 1);
	}

#ifdef __APPLE__
	/* macOS: enumerate mount points via getfsstat */
	int count = getfsstat(NULL, 0, MNT_NOWAIT);
	if (count > 0) {
		/* Local allocation - freed before browser_run_internal */
		struct statfs *mounts = malloc(count * sizeof(struct statfs));
		if (mounts != NULL) {
			int actual = getfsstat(mounts, count * sizeof(struct statfs), MNT_NOWAIT);
			size_t idx = 0;
			for (int i = 0; i < actual && idx < MAX_COMPLETION_CANDIDATES; i++) {
				/* Skip devfs, autofs, and other pseudo-filesystems */
				if (strcmp(mounts[i].f_fstypename, "devfs") == 0 ||
				    strcmp(mounts[i].f_fstypename, "autofs") == 0) {
					continue;
				}

				strncpy(state.entries[idx].name, mounts[i].f_mntonname,
				        sizeof(state.entries[idx].name) - 1);
				state.entries[idx].is_directory = true;
				state.entries[idx].size = 0;
				state.entries[idx].mtime = 0;
				idx++;
			}
			state.entry_count = idx;
			free(mounts);
		}
	}
#else
	/* Linux: list / plus /mnt/* and /media/* */
	size_t idx = 0;
	strncpy(state.entries[idx].name, "/", sizeof(state.entries[idx].name) - 1);
	state.entries[idx].is_directory = true;
	state.entries[idx].size = 0;
	state.entries[idx].mtime = 0;
	idx++;

	/* Add /mnt entries */
	file_entry mnt_entries[MAX_COMPLETION_CANDIDATES];
	size_t mnt_count = tab_completion_find_matches("/mnt", NULL, mnt_entries,
	                                                MAX_COMPLETION_CANDIDATES, false);
	for (size_t i = 0; i < mnt_count && idx < MAX_COMPLETION_CANDIDATES; i++) {
		if (mnt_entries[i].is_directory) {
			snprintf(state.entries[idx].name, sizeof(state.entries[idx].name),
			         "/mnt/%s", mnt_entries[i].name);
			state.entries[idx].is_directory = true;
			state.entries[idx].size = 0;
			state.entries[idx].mtime = 0;
			idx++;
		}
	}

	/* Add /media entries */
	file_entry media_entries[MAX_COMPLETION_CANDIDATES];
	size_t media_count = tab_completion_find_matches("/media", NULL, media_entries,
	                                                  MAX_COMPLETION_CANDIDATES, false);
	for (size_t i = 0; i < media_count && idx < MAX_COMPLETION_CANDIDATES; i++) {
		if (media_entries[i].is_directory) {
			snprintf(state.entries[idx].name, sizeof(state.entries[idx].name),
			         "/media/%s", media_entries[i].name);
			state.entries[idx].is_directory = true;
			state.entries[idx].size = 0;
			state.entries[idx].mtime = 0;
			idx++;
		}
	}

	state.entry_count = idx;
#endif

	/* Set current_dir to root for display purposes */
	snprintf(state.current_dir, sizeof(state.current_dir), "/");

	if (state.entry_count > 0) {
		browser_load_preview(&state);
	}

	/* Run with skip_initial_load since entries are already populated */
	return browser_run_internal(&state, true);
}
