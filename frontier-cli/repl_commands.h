/*
 * repl_commands.h - REPL special command processor
 *
 * Part of Frontier REPL interactive mode (Phase 1).
 * Handles special commands: /exit, /help, /vars
 *
 * Reference: planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md
 *
 * Updated: 2026-01-14 - Removed /clear command (QuickScript model has no workspace)
 */

#ifndef REPL_COMMANDS_H
#define REPL_COMMANDS_H

#include "../Common/headers/frontier.h"

/*
 * Command execution result codes
 */
typedef enum {
    REPL_CMD_CONTINUE,      /* Command executed, continue REPL loop */
    REPL_CMD_EXIT,          /* /exit command, exit REPL loop */
    REPL_CMD_NOT_COMMAND    /* Input is not a command, eval as UserTalk */
} repl_command_result;

/*
 * Check if input is a command (starts with '/')
 *
 * Returns:
 *   true if input starts with '/' (after trimming whitespace)
 *   false otherwise
 */
boolean repl_is_command(const char *input);

/*
 * Process a command and execute it
 *
 * Commands supported (Phase 1 - QuickScript Model):
 *   /exit  - Exit REPL (returns REPL_CMD_EXIT)
 *   /help  - Show available commands (returns REPL_CMD_CONTINUE)
 *
 * Unknown commands print error and return REPL_CMD_CONTINUE
 * (so REPL doesn't crash on typos).
 *
 * Parameters:
 *   input     - User input string (should start with '/')
 *
 * Returns:
 *   REPL_CMD_EXIT        - /exit command, caller should exit REPL loop
 *   REPL_CMD_CONTINUE    - Command executed, continue REPL loop
 *   REPL_CMD_NOT_COMMAND - Input doesn't start with '/', not a command
 */
repl_command_result repl_process_command(const char *input);

#endif /* REPL_COMMANDS_H */
