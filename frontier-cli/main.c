/*
 * Frontier CLI - Command Line Interface for UserTalk Script Execution
 * Phase 1: CLI-Based UserTalk Invocation Implementation
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

// Frontier headers
#include "../Common/headers/frontier.h"
#include "../Common/headers/standard.h"
#include "../Common/headers/db.h"
#include "../Common/headers/lang.h"

// CLI-specific headers
#include "cli_parser.h"
#include "cli_executor.h"
#include "cli_database.h"
#include "cli_network.h"
#include "cli_utils.h"

// Version information
#define FRONTIER_CLI_VERSION "1.0.0"
#define FRONTIER_CLI_BUILD_DATE __DATE__

// Global variables
static cli_options_t g_cli_options = {0};
static boolean g_initialized = false;

// Function prototypes
static void print_usage(const char* program_name);
static void print_version(void);
static boolean initialize_frontier_runtime(void);
static void cleanup_frontier_runtime(void);
static boolean execute_script_mode(void);
static boolean execute_database_mode(void);
static boolean execute_network_mode(void);

int main(int argc, char* argv[]) {
    int result = 0;
    
    // Parse command line arguments
    if (!cli_parse_arguments(argc, argv, &g_cli_options)) {
        fprintf(stderr, "Error: Invalid command line arguments\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Handle help and version requests
    if (g_cli_options.show_help) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (g_cli_options.show_version) {
        print_version();
        return 0;
    }
    
    // Initialize Frontier runtime
    if (!initialize_frontier_runtime()) {
        fprintf(stderr, "Error: Failed to initialize Frontier runtime\n");
        return 1;
    }
    
    // Execute based on mode
    boolean success = false;
    
    if (g_cli_options.server_mode || g_cli_options.websocket_mode) {
        success = execute_network_mode();
    } else if (g_cli_options.database_file != NULL) {
        success = execute_database_mode();
    } else if (g_cli_options.script_file != NULL || g_cli_options.inline_script != NULL) {
        success = execute_script_mode();
    } else {
        fprintf(stderr, "Error: No execution mode specified\n");
        print_usage(argv[0]);
        result = 1;
    }
    
    // Cleanup
    cleanup_frontier_runtime();
    
    return success ? 0 : 1;
}

static void print_usage(const char* program_name) {
    printf("Frontier CLI - Command Line Interface for UserTalk Script Execution\n");
    printf("Version %s (%s)\n\n", FRONTIER_CLI_VERSION, FRONTIER_CLI_BUILD_DATE);
    
    printf("Usage: %s [OPTIONS] [SCRIPT_FILE]\n\n", program_name);
    
    printf("Execution Modes:\n");
    printf("  Script Execution:\n");
    printf("    %s script.usertalk                    # Execute UserTalk script file\n", program_name);
    printf("    %s -e \"db.open('test.root')\"         # Execute inline script\n", program_name);
    printf("\n");
    printf("  Database Operations:\n");
    printf("    %s -d test.root -q \"db.getValue('path')\"  # Database query\n", program_name);
    printf("    %s -d test.root -m migrate              # Migrate database to 64-bit\n", program_name);
    printf("\n");
    printf("  Network Server:\n");
    printf("    %s --server --port 8080                # HTTP server mode\n", program_name);
    printf("    %s --websocket --port 8081             # WebSocket server mode\n", program_name);
    printf("\n");
    
    printf("Options:\n");
    printf("  -e, --execute SCRIPT     Execute inline UserTalk script\n");
    printf("  -d, --database FILE      Specify database file for operations\n");
    printf("  -q, --query QUERY        Execute database query\n");
    printf("  -m, --migrate            Migrate database to 64-bit format\n");
    printf("  --server                 Run as HTTP server\n");
    printf("  --websocket              Enable WebSocket support\n");
    printf("  -p, --port PORT          Network server port (default: 8080)\n");
    printf("  -v, --verbose            Verbose output\n");
    printf("  --debug                  Debug mode\n");
    printf("  -h, --help               Show this help message\n");
    printf("  --version                Show version information\n");
    printf("\n");
    
    printf("Examples:\n");
    printf("  # Execute a UserTalk script\n");
    printf("  %s myscript.usertalk\n", program_name);
    printf("\n");
    printf("  # Execute inline script\n");
    printf("  %s -e \"local(x = 5); x * 2\"\n", program_name);
    printf("\n");
    printf("  # Query database\n");
    printf("  %s -d mydb.root -q \"db.getValue('myTable.myValue')\"\n", program_name);
    printf("\n");
    printf("  # Start HTTP server\n");
    printf("  %s --server --port 8080\n", program_name);
    printf("\n");
}

static void print_version(void) {
    printf("Frontier CLI %s (%s)\n", FRONTIER_CLI_VERSION, FRONTIER_CLI_BUILD_DATE);
    printf("Copyright (C) 1992-2004 UserLand Software, Inc.\n");
    printf("This is free software; see the source for copying conditions.\n");
}

static boolean initialize_frontier_runtime(void) {
    if (g_initialized) {
        return true;
    }
    
    // Initialize CLI logging
    if (!cli_init_logging(g_cli_options.verbose, g_cli_options.debug)) {
        fprintf(stderr, "Error: Failed to initialize logging\n");
        return false;
    }
    
    // Initialize Frontier shell
    if (!shellinit()) {
        fprintf(stderr, "Error: Failed to initialize Frontier shell\n");
        return false;
    }
    
    // Initialize Frontier runtime
    grabthreadglobals();
    
    if (!frontierstart()) {
        fprintf(stderr, "Error: Failed to start Frontier runtime\n");
        releasethreadglobals();
        return false;
    }
    
    g_initialized = true;
    cli_log_info("Frontier runtime initialized successfully");
    
    return true;
}

static void cleanup_frontier_runtime(void) {
    if (!g_initialized) {
        return;
    }
    
    cli_log_info("Cleaning up Frontier runtime");
    
    // Cleanup Frontier runtime
    releasethreadglobals();
    
    // Cleanup CLI components
    cli_cleanup_logging();
    
    g_initialized = false;
}

static boolean execute_script_mode(void) {
    cli_log_info("Executing script mode");
    
    if (g_cli_options.script_file != NULL) {
        // Execute script file
        return cli_execute_script_file(g_cli_options.script_file);
    } else if (g_cli_options.inline_script != NULL) {
        // Execute inline script
        return cli_execute_inline_script(g_cli_options.inline_script);
    }
    
    return false;
}

static boolean execute_database_mode(void) {
    cli_log_info("Executing database mode");
    
    if (g_cli_options.database_file == NULL) {
        fprintf(stderr, "Error: Database file not specified\n");
        return false;
    }
    
    if (g_cli_options.migrate_database) {
        // Migrate database to 64-bit format
        return cli_migrate_database(g_cli_options.database_file);
    } else if (g_cli_options.query != NULL) {
        // Execute database query
        return cli_execute_database_query(g_cli_options.database_file, g_cli_options.query);
    } else {
        fprintf(stderr, "Error: No database operation specified\n");
        return false;
    }
}

static boolean execute_network_mode(void) {
    cli_log_info("Executing network mode");
    
    if (g_cli_options.server_mode) {
        // Start HTTP server
        return cli_start_http_server(g_cli_options.port);
    } else if (g_cli_options.websocket_mode) {
        // Start WebSocket server
        return cli_start_websocket_server(g_cli_options.port);
    }
    
    return false;
}
