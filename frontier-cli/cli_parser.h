/*
    Frontier CLI - Command Line Interface for UserTalk Script Execution
    CLI Parser Header

    cli_parser.h - Declarations for command-line argument parsing and options structure

    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include "../Common/headers/frontier.h"

#ifndef boolean
typedef unsigned char boolean;
#endif

// Extra (unknown) CLI argument — linked list node
typedef struct cli_extra_arg {
	char *key;						// camelCase key (stripped of -- prefix)
	char *value;					// value string, or NULL for boolean flags
	struct cli_extra_arg *next;
} cli_extra_arg_t;

// CLI options structure
typedef struct {
	char* script_file;			// Script file path
	char* inline_script;		// Inline script code
	char* system_root;			// Path to system/root database (e.g., Frontier.root)
	char* migrate_database;		// Path to database to migrate (--migrate)
	char* output_path;			// Output path for migration (--output)
	char* log_spec;				// Log spec string (--log comp:level,...)
	boolean verbose;			// Verbose output
	boolean debug;				// Debug output
	boolean output_json;		// Output results as JSON
	boolean batch_mode;			// Batch mode (no interactive prompts)
	boolean hydrate_system_root;// Hydrate system root tables flag
	boolean force_overwrite;	// Force overwrite existing output file (-f/--force)
	boolean skip_startup;		// Skip startup scripts (--skip-startup)
	boolean protocol_mode;		// NDJSON protocol mode (--protocol)
	boolean lock_opened_roots;	// Treat every loaded-from-disk DB as read-only on save
								// (--lock-opened-roots / FRONTIER_LOCK_OPENED_ROOTS=1).
								// In-memory mutations still work, only disk writes are
								// suppressed. Newly created roots (file.save / file.saveAs /
								// db.compactDatabase) are unaffected. Issue #127.
	int ws_port;				// WebSocket server port (--ws-port), 0 = disabled
	char* ut_sync_dir;			// .ut sync corpus root (--ut-sync <dir>); NULL = feature off
	boolean show_help;			// Show help flag
	boolean show_version;		// Show version flag
	cli_extra_arg_t *extra_args;	// Linked list of unknown --flags
	int positional_count;			// Number of extra positional args
	char **positional_args;			// Array of extra positional args (after script/root)
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
