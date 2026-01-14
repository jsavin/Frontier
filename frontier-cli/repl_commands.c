/*
 * repl_commands.c - REPL special command processor implementation
 *
 * Part of Frontier REPL interactive mode (Phase 1).
 * Handles special commands: /exit, /help, /vars, /clear
 *
 * Reference: planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md
 */

#include "repl_commands.h"
#include "repl_eval.h"    /* For repl_workspace_clear() */
#include "repl_output.h"  /* For repl_output_help(), repl_output_vars() */
#include <string.h>
#include <ctype.h>

/*
 * Helper: Trim leading whitespace from string
 */
static const char *trim_leading_whitespace(const char *str) {
    if (str == NULL) {
        return NULL;
    }

    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    return str;
}

/*
 * Helper: Trim trailing whitespace from string
 * Modifies buffer in place
 */
static void trim_trailing_whitespace(char *str) {
    if (str == NULL || *str == '\0') {
        return;
    }

    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

/*
 * Check if input is a command (starts with '/')
 */
boolean repl_is_command(const char *input) {
    if (input == NULL) {
        return false;
    }

    /* Trim leading whitespace */
    input = trim_leading_whitespace(input);

    /* Check for leading '/' */
    return (input[0] == '/');
}

/*
 * Process a command and execute it
 */
repl_command_result repl_process_command(
    const char *input,
    repl_workspace *workspace
) {
    if (input == NULL) {
        return REPL_CMD_NOT_COMMAND;
    }

    /* Trim leading whitespace */
    input = trim_leading_whitespace(input);

    /* Check if it's a command */
    if (input[0] != '/') {
        return REPL_CMD_NOT_COMMAND;
    }

    /* Skip leading '/' */
    const char *cmd = input + 1;

    /* Extract command name (copy to buffer so we can trim trailing whitespace) */
    char cmd_buf[256];
    size_t cmd_len = strlen(cmd);
    if (cmd_len >= sizeof(cmd_buf)) {
        cmd_len = sizeof(cmd_buf) - 1;
    }
    memcpy(cmd_buf, cmd, cmd_len);
    cmd_buf[cmd_len] = '\0';

    /* Trim trailing whitespace from command */
    trim_trailing_whitespace(cmd_buf);

    /* Handle empty command */
    if (cmd_buf[0] == '\0') {
        printf("Unknown command: /\n");
        printf("Type /help for available commands\n");
        return REPL_CMD_CONTINUE;
    }

    /* ======================================================================
     * /exit - Exit REPL
     * ====================================================================== */
    if (strcmp(cmd_buf, "exit") == 0) {
        return REPL_CMD_EXIT;
    }

    /* ======================================================================
     * /help - Show available commands
     * ====================================================================== */
    if (strcmp(cmd_buf, "help") == 0) {
        repl_output_help();
        return REPL_CMD_CONTINUE;
    }

    /* ======================================================================
     * /vars - Show workspace variables
     * ====================================================================== */
    if (strcmp(cmd_buf, "vars") == 0) {
        if (workspace == NULL) {
            printf("Error: Workspace not initialized\n");
            return REPL_CMD_CONTINUE;
        }

        repl_output_vars(workspace->workspace_table);
        return REPL_CMD_CONTINUE;
    }

    /* ======================================================================
     * /clear - Clear workspace
     * ====================================================================== */
    if (strcmp(cmd_buf, "clear") == 0) {
        if (workspace == NULL) {
            printf("Error: Workspace not initialized\n");
            return REPL_CMD_CONTINUE;
        }

        if (repl_workspace_clear(workspace)) {
            printf("Workspace cleared\n");
        } else {
            printf("Error: Failed to clear workspace\n");
        }
        return REPL_CMD_CONTINUE;
    }

    /* ======================================================================
     * Unknown command
     * ====================================================================== */
    printf("Unknown command: /%s\n", cmd_buf);
    printf("Type /help for available commands\n");
    return REPL_CMD_CONTINUE;
}
