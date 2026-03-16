/*
 * Frontier CLI - Command Line Interface for UserTalk Script Execution
 * CLI Parser Header
 *
 * cli_parser.h - Declarations for command-line argument parsing and options structure
 *
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include "../Common/headers/frontier.h"

#ifndef boolean
typedef unsigned char boolean;
#endif

// CLI options structure
typedef struct {
    char* script_file;          // Script file path
    char* inline_script;        // Inline script code
    char* system_root;          // Path to system/root database (e.g., Frontier.root)
    char* migrate_database;     // Path to database to migrate (--migrate)
    char* output_path;          // Output path for migration (--output)
    char* log_spec;             // Log spec string (--log comp:level,...)
    boolean verbose;            // Verbose output
    boolean debug;              // Debug output
    boolean output_json;        // Output results as JSON
    boolean batch_mode;         // Batch mode (no interactive prompts)
    boolean hydrate_system_root;// Hydrate system root tables flag
    boolean force_overwrite;    // Force overwrite existing output file (-f/--force)
    boolean skip_startup;       // Skip startup scripts (--skip-startup)
    boolean protocol_mode;      // NDJSON protocol mode (--protocol)
    int ws_port;                // WebSocket server port (--ws-port), 0 = disabled
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
#define CLI_DEFAULT_WS_PORT 5337  /* WebSocket ODB access (Frontier admin is on 5336) */
#define CLI_MAX_SCRIPT_LENGTH 8192
#define CLI_MAX_PATH_LENGTH 1024

#endif // CLI_PARSER_H
