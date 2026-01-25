/*
 * repl.c - Main REPL loop with linenoise integration for line editing and history
 *
 * Uses linenoise (https://github.com/antirez/linenoise) for cross-platform
 * line editing, history, and tab completion support.
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "linenoise.h"

#include "repl.h"
#include "repl_eval.h"
#include "repl_output.h"
#include "repl_commands.h"
#include "completion.h"
#include "../Common/headers/frontier.h"
#include "../Common/headers/logging.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/tablestructure.h"

// History configuration
#define HISTORY_FILE ".frontier_history"
#define HISTORY_SIZE 1000
#define MAX_SESSION_COMMANDS 1000
#define MAX_COMMAND_LEN 4096

// REPL prompt
#define REPL_PROMPT "[root]> "

// Session command tracking for merge-before-save
static char *session_commands[MAX_SESSION_COMMANDS];
static size_t session_command_count = 0;

// Forward declaration for completion callback
static void linenoise_completion_callback(const char *buf, linenoiseCompletions *lc);

/* Adds a command to the session tracking list for history merge-before-save. */
static void track_session_command(const char *cmd) {
    if (session_command_count >= MAX_SESSION_COMMANDS) {
        // Shift out oldest command
        free(session_commands[0]);
        memmove(session_commands, session_commands + 1,
                (MAX_SESSION_COMMANDS - 1) * sizeof(char *));
        session_command_count = MAX_SESSION_COMMANDS - 1;
    }
    session_commands[session_command_count] = strdup(cmd);
    if (session_commands[session_command_count]) {
        session_command_count++;
    }
}

/* Frees all tracked session commands and resets the counter. */
static void free_session_commands(void) {
    for (size_t i = 0; i < session_command_count; i++) {
        free(session_commands[i]);
        session_commands[i] = NULL;
    }
    session_command_count = 0;
}

/* Merges session commands with existing history file, deduplicating and trimming to HISTORY_SIZE. */
static void merge_and_save_history(const char *history_path) {
    // Read existing history file
    char **file_commands = NULL;
    size_t file_count = 0;
    size_t file_capacity = 0;

    FILE *f = fopen(history_path, "r");
    if (f) {
        char line[MAX_COMMAND_LEN];
        while (fgets(line, sizeof(line), f)) {
            // Remove trailing newline
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
                len--;
            }
            if (len == 0) continue;

            // Grow array if needed
            if (file_count >= file_capacity) {
                file_capacity = file_capacity ? file_capacity * 2 : 256;
                char **new_commands = realloc(file_commands, file_capacity * sizeof(char *));
                if (!new_commands) break;
                file_commands = new_commands;
            }

            file_commands[file_count] = strdup(line);
            if (file_commands[file_count]) {
                file_count++;
            }
        }
        fclose(f);
    }

    // Merge: file commands first, then session commands
    // Deduplicate by keeping last occurrence of each command
    char **merged = malloc((file_count + session_command_count) * sizeof(char *));
    size_t merged_count = 0;

    if (!merged) {
        // Malloc failed - cleanup file_commands entries to avoid memory leak
        for (size_t i = 0; i < file_count; i++) {
            free(file_commands[i]);
        }
        free(file_commands);
        return;
    }

    // Add file commands (skip if duplicate of session command)
    for (size_t i = 0; i < file_count; i++) {
        int is_dup = 0;
        for (size_t j = 0; j < session_command_count; j++) {
            if (strcmp(file_commands[i], session_commands[j]) == 0) {
                is_dup = 1;
                break;
            }
        }
        if (!is_dup) {
            merged[merged_count++] = file_commands[i];
        } else {
            free(file_commands[i]);
        }
    }

    // Add all session commands (they're the most recent)
    for (size_t i = 0; i < session_command_count; i++) {
        merged[merged_count++] = session_commands[i];
    }

    // Trim to HISTORY_SIZE (keep most recent)
    size_t start = 0;
    if (merged_count > HISTORY_SIZE) {
        start = merged_count - HISTORY_SIZE;
        for (size_t i = 0; i < start; i++) {
            free(merged[i]);
        }
    }

    // Write merged history
    f = fopen(history_path, "w");
    if (f) {
        for (size_t i = start; i < merged_count; i++) {
            fprintf(f, "%s\n", merged[i]);
        }
        fclose(f);
    }

    // Free merged entries (session_commands are now owned by merged)
    for (size_t i = start; i < merged_count; i++) {
        free(merged[i]);
    }
    free(merged);

    // Clear session tracking - NULL pointers to prevent double-free
    for (size_t i = 0; i < session_command_count; i++) {
        session_commands[i] = NULL;
    }
    session_command_count = 0;

    // Free file_commands array (entries already freed or moved to merged)
    free(file_commands);
}

/* Initializes linenoise with history, tab completion, and multi-line mode. */
static boolean init_linenoise(void) {
    // Set history size
    linenoiseHistorySetMaxLen(HISTORY_SIZE);

    // Load history from file
    const char *home = getenv("HOME");
    if (home) {
        char history_path[1024];
        int written = snprintf(history_path, sizeof(history_path), "%s/%s", home, HISTORY_FILE);
        if (written >= (int)sizeof(history_path)) {
            log_warn(LOG_COMP_GENERAL, "HOME path too long, history disabled");
        } else {
            linenoiseHistoryLoad(history_path);
        }
    }

    // Set up tab completion callback
    linenoiseSetCompletionCallback(linenoise_completion_callback);

    // Initialize completion engine
    if (!completion_init()) {
        log_warn(LOG_COMP_GENERAL, "Tab completion initialization failed");
        // Continue without completion - not fatal
    }

    // Enable multi-line mode for long scripts
    linenoiseSetMultiLine(1);

    return true;
}

/* Cleans up linenoise resources and saves merged history to disk. */
static void cleanup_linenoise(void) {
    // Cleanup completion engine
    completion_cleanup();

    // Merge and save history (supports concurrent sessions)
    const char *home = getenv("HOME");
    if (home) {
        char history_path[1024];
        int written = snprintf(history_path, sizeof(history_path), "%s/%s", home, HISTORY_FILE);
        if (written >= (int)sizeof(history_path)) {
            log_warn(LOG_COMP_GENERAL, "HOME path too long, history not saved");
        } else {
            merge_and_save_history(history_path);
        }
    }

    // Free any remaining session commands
    free_session_commands();
}

/* Bridges linenoise tab completion to the Frontier completion engine. */
static void linenoise_completion_callback(const char *buf, linenoiseCompletions *lc) {
    // Parse completion context
    completion_context_t ctx;
    int cursor_pos = (int)strlen(buf);  // Linenoise doesn't expose cursor position, assume end of line
    completion_parse_context(buf, cursor_pos, &ctx);

    // No completion inside strings
    if (ctx.ctx_type == COMPLETION_CTX_STRING) {
        return;
    }

    // Collect matches
    completion_matches_t matches;
    completion_matches_init(&matches);

    if (ctx.has_dot) {
        // Phase 3: Dotted path completion
        hdlhashtable target = completion_navigate_path(ctx.table_path);
        if (target != nil) {
            completion_add_table_entries(&matches, target, ctx.leaf_prefix);
        }
    } else if (ctx.ctx_type == COMPLETION_CTX_ADDRESS) {
        // Phase 4: Address completion - search from roottable
        completion_add_table_entries(&matches, roottable, ctx.token);
    } else if (ctx.ctx_type == COMPLETION_CTX_VERB_CALL) {
        // Phase 4: Verb processor context
        hdlhashtable target = completion_navigate_path(ctx.table_path);
        if (target != nil) {
            completion_add_table_entries(&matches, target, ctx.leaf_prefix);
        }
    } else {
        // Phase 1 & 2: General completion
        completion_add_keywords(&matches, ctx.token);

        if (roottable != nil) {
            completion_add_table_entries(&matches, roottable, ctx.token);
        }
        if (systemtable != nil) {
            completion_add_table_entries(&matches, systemtable, ctx.token);
        }
    }

    // Add matches to linenoise
    for (size_t i = 0; i < matches.count; i++) {
        // Build full completion string (prefix + match)
        char completion[1024];
        size_t buf_len = strlen(buf);
        size_t leaf_len = strlen(ctx.leaf_prefix);

        // Guard against integer underflow if leaf_prefix longer than buf
        if (leaf_len > buf_len) {
            continue;
        }
        size_t prefix_len = buf_len - leaf_len;

        // Copy the prefix part of the buffer
        if (prefix_len > sizeof(completion) - 1) {
            prefix_len = sizeof(completion) - 1;
        }
        memcpy(completion, buf, prefix_len);
        completion[prefix_len] = '\0';

        // Append the match
        size_t remaining = sizeof(completion) - prefix_len - 1;
        strncat(completion, matches.items[i].name, remaining);

        // Add trailing character based on type
        if (matches.items[i].is_table) {
            // Recalculate remaining space after first strncat
            size_t current_len = strlen(completion);
            size_t space_left = sizeof(completion) - current_len - 1;
            if (space_left > 0) {
                strncat(completion, ".", space_left);
            }
        }

        linenoiseAddCompletion(lc, completion);
    }
}

/* Main REPL entry point: initializes linenoise, runs the read-eval-print loop, and cleans up. */
int repl_main(cli_options_t *options) {
    (void)options;  /* Unused in Phase 1 */

    boolean running = true;

    // 1. Initialize linenoise
    if (!init_linenoise()) {
        log_error(LOG_COMP_GENERAL, "Failed to initialize linenoise");
        return 1;
    }

    // 2. Display welcome message
    repl_output_welcome();

    // 3. Main loop
    while (running) {
        // Read line with linenoise (handles editing, history, completion)
        char *line = linenoise(REPL_PROMPT);

        if (line == NULL) {
            // EOF (Ctrl-D) or error
            break;
        }

        // Skip empty lines
        size_t len = strlen(line);
        if (len == 0) {
            linenoiseFree(line);
            continue;
        }

        // Add to history (skip commands starting with /)
        if (line[0] != '/') {
            linenoiseHistoryAdd(line);
            track_session_command(line);  // Track for merge-before-save
        }

        // Process command
        if (line[0] == '/') {
            repl_command_result result = repl_process_command(line);
            if (result == REPL_CMD_EXIT) {
                running = false;
            }
            linenoiseFree(line);
            continue;
        }

        // Evaluate as UserTalk
        bigstring result;
        bigstring error_msg;
        if (repl_eval_script(line, result, error_msg)) {
            // Success - display result
            repl_output_result(result);
        } else {
            // Error - display error
            char error_buf[256];
            size_t error_len = stringlength(error_msg);
            if (error_len > sizeof(error_buf) - 1) {
                error_len = sizeof(error_buf) - 4;
                memcpy(error_buf, stringbaseaddress(error_msg), error_len);
                memcpy(error_buf + error_len, "...", 4);
            } else {
                memcpy(error_buf, stringbaseaddress(error_msg), error_len);
                error_buf[error_len] = '\0';
            }
            repl_output_error(error_buf);
        }

        linenoiseFree(line);
    }

    // 4. Cleanup
    cleanup_linenoise();
    repl_output_goodbye();
    return 0;
}
