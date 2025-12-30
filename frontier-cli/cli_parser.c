/*
 * Frontier CLI - Command Line Interface for UserTalk Script Execution
 * CLI Parser Implementation
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

// Initialize CLI options with default values
static void cli_init_options(cli_options_t* options) {
    memset(options, 0, sizeof(cli_options_t));
}

// Validate CLI options for consistency
boolean cli_validate_options(const cli_options_t* options) {
    if (options->show_help || options->show_version) {
        return true;
    }

    boolean hydration_mode = options->hydrate_system_root;
    boolean upgrade_mode = options->upgrade_system_root;

    if (upgrade_mode) {
        if (options->system_root == NULL) {
            log_error(LOG_COMP_GENERAL, "Error: --upgrade-system-root requires --system-root PATH");
            return false;
        }
        if (hydration_mode) {
            log_error(LOG_COMP_GENERAL, "Error: --upgrade-system-root cannot be combined with --hydrate-system-root");
            return false;
        }
        return true;
    }

    if (hydration_mode) {
        if (options->system_root == NULL) {
            log_error(LOG_COMP_GENERAL, "Error: --hydrate-system-root requires --system-root PATH");
            return false;
        }
    } else {
        // Check for script execution parameters
        if (options->script_file == NULL && options->inline_script == NULL) {
            log_error(LOG_COMP_GENERAL, "Error: No execution mode specified");
            return false;
        }
    }

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

boolean cli_parse_arguments(int argc, char* argv[], cli_options_t* options) {
    int opt;
    int option_index = 0;
    
    // Initialize options with defaults
    cli_init_options(options);
    
    // Define long options
    static struct option long_options[] = {
        {"execute", required_argument, 0, 'e'},
        {"system-root", required_argument, 0, 'R'},
        {"hydrate-system-root", no_argument, 0, 'H'},
        {"upgrade-system-root", no_argument, 0, 'U'},
        {"verbose", no_argument, 0, 'v'},
        {"debug", no_argument, 0, 'D'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {0, 0, 0, 0}
    };

    // Parse command line arguments
    while ((opt = getopt_long(argc, argv, "e:R:HUvDhV", long_options, &option_index)) != -1) {
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

            case 'H':
                options->hydrate_system_root = true;
                break;

            case 'U':
                options->upgrade_system_root = true;
                break;

            case 'v':
                // Verbose mode
                options->verbose = true;
                break;

            case 'D':
                // Debug mode
                options->debug = true;
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

    // Handle non-option arguments (script files)
    if (optind < argc) {
        if (options->script_file != NULL) {
            log_error(LOG_COMP_GENERAL, "Error: Multiple script files not allowed");
            return false;
        }
        if (strlen(argv[optind]) > CLI_MAX_PATH_LENGTH) {
            log_error(LOG_COMP_GENERAL, "Error: Script file path too long (max %d characters)", CLI_MAX_PATH_LENGTH);
            return false;
        }
        options->script_file = strdup(argv[optind]);

        // Check for additional arguments
        if (optind + 1 < argc) {
            log_error(LOG_COMP_GENERAL, "Error: Unexpected argument '%s'", argv[optind + 1]);
            return false;
        }
    }
    
    // Validate the parsed options
    return cli_validate_options(options);
}

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
}

void cli_print_options(const cli_options_t* options) {
    if (options == NULL) {
        printf("CLI Options: NULL\n");
        return;
    }

    printf("CLI Options:\n");
    printf("  Script File: %s\n", options->script_file ? options->script_file : "(none)");
    printf("  Inline Script: %s\n", options->inline_script ? options->inline_script : "(none)");
    printf("  System Root: %s\n", options->system_root ? options->system_root : "(none)");
    printf("  Verbose: %s\n", options->verbose ? "yes" : "no");
    printf("  Debug: %s\n", options->debug ? "yes" : "no");
    printf("  Hydrate System Root: %s\n", options->hydrate_system_root ? "yes" : "no");
    printf("  Upgrade System Root: %s\n", options->upgrade_system_root ? "yes" : "no");
    printf("  Show Help: %s\n", options->show_help ? "yes" : "no");
    printf("  Show Version: %s\n", options->show_version ? "yes" : "no");
}
