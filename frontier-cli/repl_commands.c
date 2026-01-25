/*
 * repl_commands.c - Processes slash commands (/exit, /help) in the REPL
 *
 * Commands start with '/' and are handled separately from UserTalk evaluation.
 *
 * Reference: planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md
 */

#include "repl_commands.h"
#include "repl_output.h"  /* For repl_output_help(), repl_output_vars() */
#include <string.h>
#include <ctype.h>

// Maximum buffer size for command parsing
#define REPL_MAX_COMMAND_LENGTH 256

/* Returns pointer to first non-whitespace character in string. */
static const char *trim_leading_whitespace(const char *str) {
    if (str == NULL) {
        return NULL;
    }

    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    return str;
}

/* Removes trailing whitespace from string in place. */
static void trim_trailing_whitespace(char *str) {
    if (str == NULL || *str == '\0') {
        return;
    }

    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

/* Returns true if input starts with '/' (indicating a REPL command). */
boolean repl_is_command(const char *input) {
    if (input == NULL) {
        return false;
    }

    /* Trim leading whitespace */
    input = trim_leading_whitespace(input);

    /* Check for leading '/' */
    return (input[0] == '/');
}

/* Parses and executes a slash command, returning the appropriate result code. */
repl_command_result repl_process_command(const char *input) {
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
    char cmd_buf[REPL_MAX_COMMAND_LENGTH];
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

    /* Note: /vars and /clear removed - QuickScript model has no workspace
     * Users can use 'sizeOf(system.temp)' or similar to inspect database tables
     */

    /* ======================================================================
     * Unknown command
     * ====================================================================== */
    printf("Unknown command: /%s\n", cmd_buf);
    printf("Type /help for available commands\n");
    return REPL_CMD_CONTINUE;
}
