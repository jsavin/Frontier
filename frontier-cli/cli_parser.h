/*
 * Frontier CLI - Command Line Interface for UserTalk Script Execution
 * CLI Parser Header
 * 
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include "../Common/SystemHeaders/standard.h"

// CLI options structure
typedef struct {
    char* script_file;          // Script file path
    char* inline_script;        // Inline script code
    char* database_file;        // Database file path
    char* query;                // Database query
    int port;                   // Network server port
    boolean verbose;            // Verbose output
    boolean debug;              // Debug output
    boolean server_mode;        // HTTP server mode
    boolean websocket_mode;     // WebSocket server mode
    boolean migrate_database;   // Migrate database flag
    boolean show_help;          // Show help flag
    boolean show_version;       // Show version flag
} cli_options_t;

// Argument parsing functions
boolean cli_parse_arguments(int argc, char* argv[], cli_options_t* options);
void cli_free_options(cli_options_t* options);
void cli_print_options(const cli_options_t* options);

// Validation functions
boolean cli_validate_options(const cli_options_t* options);
boolean cli_validate_script_file(const char* script_file);
boolean cli_validate_database_file(const char* database_file);
boolean cli_validate_port(int port);

// Help and version functions
void cli_print_usage(const char* program_name);
void cli_print_version(void);
void cli_print_help(const char* program_name);

// Default values
#define CLI_DEFAULT_PORT 8080
#define CLI_MAX_SCRIPT_LENGTH 8192
#define CLI_MAX_PATH_LENGTH 1024

#endif // CLI_PARSER_H
