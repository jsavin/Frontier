/*
 * repl.c - Main REPL loop with linenoise integration for line editing and history
 *
 * Uses linenoise (https://github.com/antirez/linenoise) for cross-platform
 * line editing, history, and tab completion support.
 *
 * Phase 4: Event loop architecture for concurrent REPL + webserver + agents.
 * Uses non-blocking linenoise API with poll() for multiplexing.
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
#include <signal.h>
#include <poll.h>
#include <errno.h>

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
#include "../Common/headers/tcpverbs.h"  /* tcp_process_callbacks */
#include "../Common/headers/process.h"   /* agentsenabled, agentscheduler_tick */

// History configuration
#define HISTORY_FILE ".frontier_history"
#define HISTORY_SIZE 1000
#define MAX_SESSION_COMMANDS 1000
#define MAX_COMMAND_LEN 4096

// Event loop configuration
#define POLL_TIMEOUT_MS 10  // 10ms = responsive while allowing ~100Hz callback processing

// REPL prompt
#define REPL_PROMPT "[root]> "

// Session command tracking for merge-before-save
static char *session_commands[MAX_SESSION_COMMANDS];
static size_t session_command_count = 0;

// Global interrupt flag (signal-safe)
// Declared as non-static so lang.c can check it for script interruption
volatile sig_atomic_t g_repl_interrupt_requested = 0;

// Global script running flag for interrupt handling
static volatile sig_atomic_t g_script_running = 0;

// Global linenoise state for terminal cleanup and async output
static struct linenoiseState *g_active_linenoisestate = NULL;

// Forward declaration for completion callback
static void linenoise_completion_callback(const char *buf, linenoiseCompletions *lc);

/* Signal handler for SIGINT (Ctrl-C) - must be async-signal-safe */
static void sigint_handler(int sig) {
    (void)sig;
    g_repl_interrupt_requested = 1;
}

/* Signal handler for SIGTERM - exit immediately
 * NOTE: We don't call linenoiseEditStop() here because it's not async-signal-safe.
 * Terminal mode is automatically restored by the OS when the process exits.
 * Using _exit() to avoid calling atexit handlers from signal context (unsafe). */
static void sigterm_handler(int sig) {
    (void)sig;
    _exit(0);
}

/* Terminal cleanup for atexit() */
static void cleanup_terminal(void) {
    if (g_active_linenoisestate) {
        linenoiseEditStop(g_active_linenoisestate);
        g_active_linenoisestate = NULL;
    }
}

/* Install signal handlers for Ctrl-C and SIGTERM */
static void install_signal_handlers(void) {
    struct sigaction sa;

    // SIGINT handler (Ctrl-C)
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    // SIGTERM handler (for clean shutdown)
    sa.sa_handler = sigterm_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);

    // Register terminal cleanup for atexit
    atexit(cleanup_terminal);
}

/* Handle interrupt in event loop */
static void handle_interrupt(struct linenoiseState *ls, char *buf, size_t buflen) {
    g_repl_interrupt_requested = 0;

    if (g_script_running) {
        // Script is running - set interrupt flag for interpreter to check
        // The interpreter's background task handler will see this
        printf("\n^C (interrupting script...)\n");
        fflush(stdout);
    } else {
        // At prompt - clear line and redisplay
        linenoiseEditStop(ls);
        printf("^C\n");
        fflush(stdout);
        // Restart with fresh prompt using caller's buffer (not ls->buf which
        // may be invalid after linenoiseEditStop)
        linenoiseEditStart(ls, STDIN_FILENO, STDOUT_FILENO,
                          buf, buflen, REPL_PROMPT);
    }
}

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

/* Process a single command line (extracted for use in event loop) */
static boolean process_line(const char *line, boolean *running) {
    // Skip empty lines
    size_t len = strlen(line);
    if (len == 0) {
        return true;
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
            *running = false;
        }
        return true;
    }

    // Evaluate as UserTalk
    g_script_running = 1;  // Mark script as running for interrupt handling

    bigstring result;
    bigstring error_msg;
    boolean success = repl_eval_script(line, result, error_msg);

    g_script_running = 0;  // Script finished

    if (success) {
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

    return true;
}

/* Blocking REPL loop for non-TTY input (fallback mode).
 * Used when stdin is not a terminal (e.g., piped input).
 */
static int repl_main_blocking(void) {
    boolean running = true;

    while (running) {
        // Read line with blocking linenoise
        char *line = linenoise(REPL_PROMPT);

        if (line == NULL) {
            // EOF (Ctrl-D) or error
            break;
        }

        // Process the line
        process_line(line, &running);
        linenoiseFree(line);

        // Process callbacks after each command
        tcp_process_callbacks();
    }

    return 0;
}

/* Main REPL entry point: runs event loop with non-blocking linenoise.
 * Falls back to blocking mode if stdin is not a TTY.
 */
int repl_main(cli_options_t *options) {
    (void)options;  /* Unused in Phase 1 */

    boolean running = true;
    struct linenoiseState ls;
    char line_buf[MAX_COMMAND_LEN];
    boolean use_event_loop;

    // 1. Initialize linenoise
    if (!init_linenoise()) {
        log_error(LOG_COMP_GENERAL, "Failed to initialize linenoise");
        return 1;
    }

    // 2. Install signal handlers for Ctrl-C and terminal cleanup
    install_signal_handlers();

    // 3. Display welcome message
    repl_output_welcome();

    // 4. Check if we can use the event loop (requires TTY)
    // The non-blocking linenoise API requires a real terminal for raw mode
    use_event_loop = isatty(STDIN_FILENO);

    if (!use_event_loop) {
        // Fall back to blocking mode for non-TTY input
        log_debug(LOG_COMP_GENERAL, "Non-TTY input detected, using blocking REPL mode");
        int result = repl_main_blocking();
        cleanup_linenoise();
        repl_output_goodbye();
        return result;
    }

    // 5. Start non-blocking line editing (only for TTY)
    if (linenoiseEditStart(&ls, STDIN_FILENO, STDOUT_FILENO,
                           line_buf, sizeof(line_buf), REPL_PROMPT) == -1) {
        log_error(LOG_COMP_GENERAL, "Failed to start linenoise editing");
        cleanup_linenoise();
        return 1;
    }

    // Store global pointer for terminal cleanup and async output
    g_active_linenoisestate = &ls;
    repl_set_active_linenoisestate(&ls);

    // 6. Event loop
    while (running) {
        // 6.1 Poll stdin with timeout
        struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
        int ready = poll(&pfd, 1, POLL_TIMEOUT_MS);

        // 6.2 Feed input to linenoise if available
        if (ready > 0 && (pfd.revents & POLLIN)) {
            char *result = linenoiseEditFeed(&ls);

            if (result == linenoiseEditMore) {
                // User is still editing - continue polling
            } else if (result != NULL) {
                // User pressed Enter - stop line editing first (prints newline)
                linenoiseEditStop(&ls);

                // Process the line
                process_line(result, &running);
                linenoiseFree(result);

                if (running) {
                    // Restart line editing for next command
                    if (linenoiseEditStart(&ls, STDIN_FILENO, STDOUT_FILENO,
                                           line_buf, sizeof(line_buf), REPL_PROMPT) == -1) {
                        log_error(LOG_COMP_GENERAL, "Failed to restart linenoise editing");
                        running = false;
                    }
                }
            } else {
                // EOF (Ctrl-D) or error
                running = false;
            }
        } else if (ready < 0) {
            // poll() error - check errno to determine if recoverable
            if (errno == EINTR) {
                // Interrupted by signal - check if it was Ctrl-C
                if (g_repl_interrupt_requested) {
                    handle_interrupt(&ls, line_buf, sizeof(line_buf));
                }
                // Otherwise continue (e.g., SIGWINCH for terminal resize)
            } else {
                // Unrecoverable poll() error (EBADF, ENOMEM, etc.)
                log_error(LOG_COMP_GENERAL, "poll() failed: %s", strerror(errno));
                running = false;
            }
        }

        // 6.3 Process TCP callbacks (webserver)
        tcp_process_callbacks();

        // 6.4 Run agent scheduler tick (if agents enabled)
        if (agentsenabled()) {
            agentscheduler_tick();
        }

        // 6.5 Check for Ctrl-C flag (in case signal arrived during poll)
        if (g_repl_interrupt_requested) {
            handle_interrupt(&ls, line_buf, sizeof(line_buf));
        }
    }

    // 7. Cleanup
    g_active_linenoisestate = NULL;
    repl_set_active_linenoisestate(NULL);
    linenoiseEditStop(&ls);
    cleanup_linenoise();
    repl_output_goodbye();
    return 0;
}
