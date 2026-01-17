/*
 * Frontier CLI - REPL Core Loop Implementation
 * Phase 2: Editline Integration (command history and tab completion)
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
#include <histedit.h>
#include <unistd.h>
#include <pwd.h>

#include "repl.h"
#include "repl_eval.h"
#include "repl_output.h"
#include "repl_commands.h"
#include "completion.h"
#include "../Common/headers/frontier.h"
#include "../Common/headers/logging.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/strings.h"

// Maximum input buffer size for REPL
#define REPL_MAX_INPUT_LENGTH 4096
#define HISTORY_FILE ".frontier_history"
#define HISTORY_SIZE 1000

// Editline/History state
static EditLine *g_el = NULL;
static History *g_hist = NULL;
static HistEvent g_ev;

// Prompt callback for editline
static char *repl_prompt(EditLine *el) {
    (void)el;
    return "[root]> ";
}

// Tab completion callback - delegates to completion engine
// (completion.c implements all four phases of completion)

// Initialize editline and history
static boolean init_editline(void) {
    // Initialize editline
    g_el = el_init("frontier-cli", stdin, stdout, stderr);
    if (!g_el) {
        log_error(LOG_COMP_GENERAL, "Failed to initialize editline");
        return false;
    }

    // Set prompt callback
    el_set(g_el, EL_PROMPT, repl_prompt);

    // Enable editor mode (emacs-style editing)
    el_set(g_el, EL_EDITOR, "emacs");

    // Set up tab completion (uses completion engine from completion.c)
    el_set(g_el, EL_ADDFN, "ed-complete", "Complete command", completion_callback);
    el_set(g_el, EL_BIND, "\t", "ed-complete", NULL);

    // Initialize completion engine
    if (!completion_init()) {
        log_warn(LOG_COMP_GENERAL, "Tab completion initialization failed");
        // Continue without completion - not fatal
    }

    // Initialize history
    g_hist = history_init();
    if (!g_hist) {
        log_error(LOG_COMP_GENERAL, "Failed to initialize history");
        el_end(g_el);
        g_el = NULL;
        return false;
    }

    // Set history size
    history(g_hist, &g_ev, H_SETSIZE, HISTORY_SIZE);

    // Connect history to editline
    el_set(g_el, EL_HIST, history, g_hist);

    // Load history from file
    const char *home = getenv("HOME");
    if (home) {
        char history_path[1024];
        snprintf(history_path, sizeof(history_path), "%s/%s", home, HISTORY_FILE);

        // Load existing history (ignore errors if file doesn't exist)
        history(g_hist, &g_ev, H_LOAD, history_path);
    }

    return true;
}

// Cleanup editline and history
static void cleanup_editline(void) {
    // Cleanup completion engine
    completion_cleanup();

    if (g_hist) {
        // Save history to file
        const char *home = getenv("HOME");
        if (home) {
            char history_path[1024];
            snprintf(history_path, sizeof(history_path), "%s/%s", home, HISTORY_FILE);
            history(g_hist, &g_ev, H_SAVE, history_path);
        }

        history_end(g_hist);
        g_hist = NULL;
    }

    if (g_el) {
        el_end(g_el);
        g_el = NULL;
    }
}

// Trim trailing whitespace from a string
static void trim_trailing_whitespace(char* str) {
    if (str == NULL) {
        return;
    }

    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' ||
                       str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }
}

int repl_main(cli_options_t *options) {
    (void)options;  /* Unused in Phase 1 */

    boolean running = true;

    // 1. Initialize editline
    if (!init_editline()) {
        fprintf(stderr, "Error: Failed to initialize editline. Falling back to basic mode.\n");
        // Continue with basic fgets() mode as fallback
    }

    // 2. Display welcome message
    repl_output_welcome();

    // 3. Main loop
    while (running) {
        const char *line = NULL;
        int count = 0;

        if (g_el) {
            // Use editline for input (provides history and editing)
            line = el_gets(g_el, &count);

            if (line == NULL || count <= 0) {
                // EOF (Ctrl-D)
                break;
            }

            // Copy to mutable buffer and trim
            char input[REPL_MAX_INPUT_LENGTH];
            strncpy(input, line, sizeof(input) - 1);
            input[sizeof(input) - 1] = '\0';
            trim_trailing_whitespace(input);

            // Skip empty lines
            if (strlen(input) == 0) {
                continue;
            }

            // Add to history (skip duplicates and commands starting with /)
            if (input[0] != '/' && strlen(input) > 0) {
                history(g_hist, &g_ev, H_ENTER, input);
            }

            // Process command
            if (input[0] == '/') {
                repl_command_result result = repl_process_command(input);
                if (result == REPL_CMD_EXIT) {
                    running = false;
                }
                continue;
            }

            // Evaluate as UserTalk
            bigstring result;
            bigstring error_msg;
            if (repl_eval_script(input, result, error_msg)) {
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
        } else {
            // Fallback to basic fgets() mode
            repl_output_prompt(NULL);

            char input[REPL_MAX_INPUT_LENGTH];
            if (!fgets(input, sizeof(input), stdin)) {
                // EOF (Ctrl-D)
                break;
            }

            // Trim trailing newline
            trim_trailing_whitespace(input);

            // Skip empty lines
            if (strlen(input) == 0) {
                continue;
            }

            // Check if command
            if (input[0] == '/') {
                repl_command_result result = repl_process_command(input);
                if (result == REPL_CMD_EXIT) {
                    running = false;
                }
                continue;
            }

            // Evaluate as UserTalk
            bigstring result;
            bigstring error_msg;
            if (repl_eval_script(input, result, error_msg)) {
                repl_output_result(result);
            } else {
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
        }
    }

    // 4. Cleanup
    cleanup_editline();
    repl_output_goodbye();
    return 0;
}
