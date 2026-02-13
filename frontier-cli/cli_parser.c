/*
 * cli_parser.c - Command-Line Argument Parsing for frontier-cli
 *
 * Parses and validates command-line options using getopt_long.
 * Supports script execution (-e), database loading (--system-root),
 * output modes (--output-json), and maintenance operations (--hydrate, --upgrade).
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
#include <getopt.h>

#include "cli_parser.h"
#include "cli_utils.h"
#include "../Common/headers/logging.h"

/* Checks if a path ends with .root or .root7 (case-insensitive). */
static boolean cli_is_root_file(const char* path) {
    if (path == NULL) {
        return false;
    }

    size_t len = strlen(path);

    // Check for .root (5 chars minimum)
    if (len >= 5) {
        const char* ext = path + len - 5;
        if (strcasecmp(ext, ".root") == 0) {
            return true;
        }
    }

    // Check for .root7 (6 chars minimum)
    if (len >= 6) {
        const char* ext = path + len - 6;
        if (strcasecmp(ext, ".root7") == 0) {
            return true;
        }
    }

    return false;
}

/* Initializes all options to zero/NULL defaults. */
static void cli_init_options(cli_options_t* options) {
    memset(options, 0, sizeof(cli_options_t));
}

/* Validates parsed options for consistency and required dependencies. */
boolean cli_validate_options(const cli_options_t* options) {
    if (options->show_help || options->show_version) {
        return true;
    }

    boolean hydration_mode = options->hydrate_system_root;
    boolean migrate_mode = (options->migrate_database != NULL);

    /* --migrate mode: migrate a database to v7 and exit */
    if (migrate_mode) {
        if (!cli_file_exists(options->migrate_database)) {
            log_error(LOG_COMP_GENERAL, "Error: Database does not exist: %s", options->migrate_database);
            return false;
        }
        if (!cli_file_readable(options->migrate_database)) {
            log_error(LOG_COMP_GENERAL, "Error: Database is not readable: %s", options->migrate_database);
            return false;
        }
        /* --output without --migrate is an error */
        /* --force without --migrate is harmless but meaningless */
        if (hydration_mode) {
            log_error(LOG_COMP_GENERAL, "Error: --migrate cannot be combined with --hydrate-system-root");
            return false;
        }
        if (options->system_root != NULL || options->script_file != NULL || options->inline_script != NULL) {
            log_error(LOG_COMP_GENERAL, "Error: --migrate runs standalone; do not combine with --system-root, scripts, or REPL mode");
            return false;
        }
        return true;
    }

    /* --output requires --migrate */
    if (options->output_path != NULL) {
        log_error(LOG_COMP_GENERAL, "Error: --output requires --migrate");
        return false;
    }

    if (hydration_mode) {
        if (options->system_root == NULL) {
            log_error(LOG_COMP_GENERAL, "Error: --hydrate-system-root requires --system-root PATH");
            return false;
        }
    }

    // Note: Conflict validation for positional .root argument is handled in cli_parse_arguments()
    // when we detect a .root file and system_root is already set.

    // Note: No script_file and no inline_script means REPL mode (interactive)
    // This is now a valid execution mode, so we don't error out here

    if (options->system_root != NULL) {
        if (!cli_file_exists(options->system_root)) {
            log_error(LOG_COMP_GENERAL, "Error: System root does not exist: %s", options->system_root);
            return false;
        }
        if (!cli_file_readable(options->system_root)) {
            log_error(LOG_COMP_GENERAL, "Error: System root is not readable: %s", options->system_root);
            return false;
        }
    }

    return true;
}

/* Parses command-line arguments and populates the options structure. */
boolean cli_parse_arguments(int argc, char* argv[], cli_options_t* options) {
    int opt;
    int option_index = 0;
    
    // Initialize options with defaults
    cli_init_options(options);
    
    // Define long options
    static struct option long_options[] = {
        {"execute", required_argument, 0, 'e'},
        {"system-root", required_argument, 0, 'R'},
        {"migrate", required_argument, 0, 'm'},
        {"output", required_argument, 0, 'o'},
        {"force", no_argument, 0, 'f'},
        {"batch", no_argument, 0, 'b'},
        {"non-interactive", no_argument, 0, 'b'},  /* Alias for --batch */
        {"hydrate-system-root", no_argument, 0, 'H'},
        {"output-json", no_argument, 0, 'J'},
        {"verbose", no_argument, 0, 'v'},
        {"debug", no_argument, 0, 'D'},
        {"log", required_argument, 0, 'L'},
        {"skip-startup", no_argument, 0, 'S'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {0, 0, 0, 0}
    };

    // Parse command line arguments
    while ((opt = getopt_long(argc, argv, "e:R:m:o:fbHJvDShV", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'e':
                // Inline script execution
                if (options->inline_script != NULL) {
                    log_error(LOG_COMP_GENERAL, "Error: Multiple --execute options not allowed");
                    return false;
                }
                if (strlen(optarg) > CLI_MAX_SCRIPT_LENGTH) {
                    log_error(LOG_COMP_GENERAL, "Error: Inline script too long (max %d characters)", CLI_MAX_SCRIPT_LENGTH);
                    return false;
                }
                options->inline_script = strdup(optarg);
                break;

            case 'R':
                if (options->system_root != NULL) {
                    log_error(LOG_COMP_GENERAL, "Error: Multiple --system-root options not allowed");
                    return false;
                }
                if (strlen(optarg) > CLI_MAX_PATH_LENGTH) {
                    log_error(LOG_COMP_GENERAL, "Error: System root path too long (max %d characters)", CLI_MAX_PATH_LENGTH);
                    return false;
                }
                options->system_root = strdup(optarg);
                break;

            case 'm':
                // Migrate database to v7 format
                if (options->migrate_database != NULL) {
                    log_error(LOG_COMP_GENERAL, "Error: Multiple --migrate options not allowed");
                    return false;
                }
                if (strlen(optarg) > CLI_MAX_PATH_LENGTH) {
                    log_error(LOG_COMP_GENERAL, "Error: Migrate path too long (max %d characters)", CLI_MAX_PATH_LENGTH);
                    return false;
                }
                options->migrate_database = strdup(optarg);
                break;

            case 'o':
                // Output path for migration
                if (options->output_path != NULL) {
                    log_error(LOG_COMP_GENERAL, "Error: Multiple --output options not allowed");
                    return false;
                }
                if (strlen(optarg) > CLI_MAX_PATH_LENGTH) {
                    log_error(LOG_COMP_GENERAL, "Error: Output path too long (max %d characters)", CLI_MAX_PATH_LENGTH);
                    return false;
                }
                options->output_path = strdup(optarg);
                break;

            case 'f':
                // Force overwrite existing output file
                options->force_overwrite = true;
                break;

            case 'b':
                // Batch mode (no interactive prompts)
                options->batch_mode = true;
                break;

            case 'H':
                options->hydrate_system_root = true;
                break;

            case 'J':
                // JSON output mode
                options->output_json = true;
                break;

            case 'v':
                // Verbose mode
                options->verbose = true;
                break;

            case 'D':
                // Debug mode
                options->debug = true;
                break;

            case 'L':
                // Log spec (--log comp:level,...)
                if (options->log_spec != NULL) {
                    // Multiple --log args: concatenate with commas
                    size_t old_len = strlen(options->log_spec);
                    size_t new_len = strlen(optarg);
                    char *combined = malloc(old_len + 1 + new_len + 1);
                    if (combined == NULL) {
                        log_error(LOG_COMP_GENERAL, "Error: Memory allocation failed for --log");
                        return false;
                    }
                    memcpy(combined, options->log_spec, old_len);
                    combined[old_len] = ',';
                    memcpy(combined + old_len + 1, optarg, new_len + 1);
                    free(options->log_spec);
                    options->log_spec = combined;
                } else {
                    options->log_spec = strdup(optarg);
                }
                break;

            case 'S':
                // Skip startup scripts
                options->skip_startup = true;
                break;

            case 'h':
                // Help
                options->show_help = true;
                break;

            case 'V':
                // Version
                options->show_version = true;
                break;

            case '?':
                // Unknown option
                return false;

            default:
                log_error(LOG_COMP_GENERAL, "Error: Unknown option");
                return false;
        }
    }

    // Handle non-option arguments (script files or database roots)
    if (optind < argc) {
        const char* arg = argv[optind];

        if (strlen(arg) > CLI_MAX_PATH_LENGTH) {
            log_error(LOG_COMP_GENERAL, "Error: Path too long (max %d characters)", CLI_MAX_PATH_LENGTH);
            return false;
        }

        // Detect if this is a .root or .root7 file
        if (cli_is_root_file(arg)) {
            // Positional argument is a database root
            if (options->system_root != NULL) {
                log_error(LOG_COMP_GENERAL, "Error: System root already specified via --system-root");
                return false;
            }
            options->system_root = strdup(arg);
            if (options->system_root == NULL) {
                log_error(LOG_COMP_GENERAL, "Error: Memory allocation failed");
                return false;
            }
        } else {
            // Positional argument is a script file
            if (options->script_file != NULL) {
                log_error(LOG_COMP_GENERAL, "Error: Multiple script files not allowed");
                return false;
            }
            options->script_file = strdup(arg);
            if (options->script_file == NULL) {
                log_error(LOG_COMP_GENERAL, "Error: Memory allocation failed");
                return false;
            }
        }

        // Check for additional arguments
        if (optind + 1 < argc) {
            const char* next_arg = argv[optind + 1];
            if (cli_is_root_file(next_arg)) {
                log_error(LOG_COMP_GENERAL, "Error: Multiple .root files not allowed");
            } else {
                log_error(LOG_COMP_GENERAL, "Error: Unexpected argument '%s'", next_arg);
            }
            return false;
        }
    }
    
    // Validate the parsed options
    return cli_validate_options(options);
}

/* Frees dynamically allocated strings in the options structure. */
void cli_free_options(cli_options_t* options) {
    if (options == NULL) {
        return;
    }

    // Free allocated strings
    if (options->script_file != NULL) {
        free(options->script_file);
        options->script_file = NULL;
    }

    if (options->inline_script != NULL) {
        free(options->inline_script);
        options->inline_script = NULL;
    }

    if (options->system_root != NULL) {
        free(options->system_root);
        options->system_root = NULL;
    }

    if (options->migrate_database != NULL) {
        free(options->migrate_database);
        options->migrate_database = NULL;
    }

    if (options->output_path != NULL) {
        free(options->output_path);
        options->output_path = NULL;
    }

    if (options->log_spec != NULL) {
        free(options->log_spec);
        options->log_spec = NULL;
    }
}

/* Prints parsed options for debugging purposes. */
void cli_print_options(const cli_options_t* options) {
    if (options == NULL) {
        printf("CLI Options: NULL\n");
        return;
    }

    printf("CLI Options:\n");
    printf("  Script File: %s\n", options->script_file ? options->script_file : "(none)");
    printf("  Inline Script: %s\n", options->inline_script ? options->inline_script : "(none)");
    printf("  System Root: %s\n", options->system_root ? options->system_root : "(none)");
    printf("  Migrate Database: %s\n", options->migrate_database ? options->migrate_database : "(none)");
    printf("  Output Path: %s\n", options->output_path ? options->output_path : "(none)");
    printf("  Force Overwrite: %s\n", options->force_overwrite ? "yes" : "no");
    printf("  Skip Startup: %s\n", options->skip_startup ? "yes" : "no");
    printf("  Verbose: %s\n", options->verbose ? "yes" : "no");
    printf("  Debug: %s\n", options->debug ? "yes" : "no");
    printf("  Output JSON: %s\n", options->output_json ? "yes" : "no");
    printf("  Hydrate System Root: %s\n", options->hydrate_system_root ? "yes" : "no");
    printf("  Show Help: %s\n", options->show_help ? "yes" : "no");
    printf("  Show Version: %s\n", options->show_version ? "yes" : "no");
}
