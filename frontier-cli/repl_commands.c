/*
 * repl_commands.c - Processes slash commands (/exit, /help) in the REPL
 *
 * Commands start with '/' and are handled separately from UserTalk evaluation.
 *
 * Reference: planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md
 */

#include "repl_commands.h"
#include "repl.h"           /* For repl_jump_path(), repl_get_current_path() */
#include "repl_output.h"    /* For repl_output_help(), repl_output_vars() */
#include "repl_variables.h" /* For repl_get_variables_table() */
#include "../Common/headers/lang.h" /* For emptyhashtable() */
#include "../third_party/linenoise/linenoise.h"  /* For linenoisePrintKeyCodes() */
#include <stdio.h>
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

    /* ======================================================================
     * /clear - Reset REPL state: clear variables and return focus to root
     * ====================================================================== */
    if (strcmp(cmd_buf, "clear") == 0) {
        hdlhashtable vars = repl_get_variables_table();
        if (vars != nil) {
            emptyhashtable(vars, true);
        }
        repl_jump_path("");
        printf("Variables cleared, focus reset to root.\n");
        return REPL_CMD_CONTINUE;
    }

    /* ======================================================================
     * /keycodes - Debug key sequences (for testing terminal keybindings)
     * ====================================================================== */
    if (strcmp(cmd_buf, "keycodes") == 0) {
        printf("Entering key code debugging mode.\n");
        printf("Press keys to see their escape sequences.\n");
        printf("Type 'quit' to exit back to REPL.\n\n");
        linenoisePrintKeyCodes();
        return REPL_CMD_CONTINUE;
    }

    /* ======================================================================
     * /list [path] - List contents of a table (current table if no path)
     * Supports [n] index syntax (1-based) and relative paths.
     * ====================================================================== */
    if (strncmp(cmd_buf, "list", 4) == 0) {
        const char *path = cmd_buf + 4;

        /* Skip whitespace after "list" */
        while (*path && isspace((unsigned char)*path)) {
            path++;
        }

        /* Empty path means list current table */
        if (*path == '\0') {
            repl_output_list(nil, NULL);
            return REPL_CMD_CONTINUE;
        }

        /* Use extended resolver for index syntax and relative paths */
        typathlookupresult result;
        char resolved_path[512];
        char error_msg[256] = "";
        boolean found = repl_resolve_path_ex(path, &result, resolved_path, sizeof(resolved_path),
                                              error_msg, sizeof(error_msg));

        if (!found) {
            if (error_msg[0] != '\0')
                printf("Error: %s\n", error_msg);
            else
                printf("Error: '%s' is not a valid table path\n", path);
            return REPL_CMD_CONTINUE;
        }

        if (result.is_table) {
            repl_output_list(result.htable, resolved_path);
        } else {
            repl_output_single_value(resolved_path, &result.val);
        }
        return REPL_CMD_CONTINUE;
    }

    /* ======================================================================
     * /jump <path> - Navigate to a table (like cd in a shell)
     * ====================================================================== */
    if (strncmp(cmd_buf, "jump", 4) == 0) {
        const char *path = cmd_buf + 4;

        /* Skip whitespace after "jump" */
        while (*path && isspace((unsigned char)*path)) {
            path++;
        }

        /* Empty path means go to root */
        if (*path == '\0') {
            if (!repl_jump_path("")) {
                printf("Error: Cannot navigate to root\n");
            }
            return REPL_CMD_CONTINUE;
        }

        /* Try to navigate to the path */
        if (!repl_jump_path(path)) {
            printf("Error: '%s' is not a valid table path\n", path);
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
