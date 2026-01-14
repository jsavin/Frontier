/*
 * Frontier CLI - REPL Core Loop Implementation
 * Phase 1: Basic REPL Foundation
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

#include "repl.h"
#include "repl_eval.h"
#include "repl_output.h"
#include "repl_commands.h"
#include "../Common/headers/frontier.h"
#include "../Common/headers/logging.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/strings.h"

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

    // 1. Initialize workspace
    repl_workspace workspace;
    memset(&workspace, 0, sizeof(workspace));

    if (!repl_workspace_init(&workspace)) {
        /* Dual-purpose error reporting:
         * - log_error() for diagnostic logging (debug builds, log files)
         * - fprintf(stderr) for user-facing terminal output
         * Both are intentional and serve different purposes.
         */
        log_error(LOG_COMP_GENERAL, "Failed to initialize REPL workspace");
        fprintf(stderr, "Error: Failed to initialize REPL workspace\n");
        return 1;
    }

    boolean running = true;

    // 2. Display welcome message
    repl_output_welcome();

    // 3. Main loop
    while (running) {
        // a. Display prompt
        repl_output_prompt(NULL);

        // b. Read line from stdin (use fgets for Phase 1)
        char input[4096];
        if (!fgets(input, sizeof(input), stdin)) {
            // EOF (Ctrl-D)
            break;
        }

        // c. Check for input truncation
        size_t len = strlen(input);
        if (len > 0 && len == sizeof(input) - 1 && input[len - 1] != '\n') {
            fprintf(stderr, "Warning: Input exceeded %zu bytes and was truncated\n",
                    sizeof(input) - 1);
            /* Consume rest of line to prevent buffer overflow on next read */
            int c;
            while ((c = getchar()) != '\n' && c != EOF)
                ;
        }

        // d. Trim trailing newline
        trim_trailing_whitespace(input);

        // e. Skip empty lines
        if (strlen(input) == 0) {
            continue;
        }

        // f. Check if command
        if (input[0] == '/') {
            repl_command_result result = repl_process_command(input, &workspace);
            if (result == REPL_CMD_EXIT) {
                running = false;
            }
            continue;
        }

        // g. Evaluate as UserTalk
        bigstring result;
        bigstring error_msg;
        if (repl_eval_script(&workspace, input, result, error_msg)) {
            // Success - display result
            repl_output_result(result);
        } else {
            // Error - display error
            /* Convert bigstring to C string for output */
            char error_buf[256];
            size_t error_len = stringlength(error_msg);
            if (error_len > sizeof(error_buf) - 1) {
                error_len = sizeof(error_buf) - 1;
            }
            memcpy(error_buf, stringbaseaddress(error_msg), error_len);
            error_buf[error_len] = '\0';
            repl_output_error(error_buf);
        }
    }

    // 4. Cleanup
    repl_output_goodbye();
    repl_workspace_cleanup(&workspace);
    return 0;
}
