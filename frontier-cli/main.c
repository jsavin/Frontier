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

/*
 * main.c - Entry point for frontier-cli, handles argument parsing and runtime initialization
 *
 * This file orchestrates the CLI startup sequence: parsing command-line arguments,
 * initializing the Frontier runtime, loading the system root database, and dispatching
 * to either script execution mode or the interactive REPL.
 *
 * See docs/CLI_USAGE_GUIDE.md for usage documentation.
 */

// 2025-10-27 Codex: Added diagnostics around system table hydration to surface missing subtables.
// 2025-10-27 Codex: Stop recreating system tables during headless load and follow Cancoon root pointer so persisted tables stay wired.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>  /* for _NSGetExecutablePath */
#endif

// Frontier headers
#include "../Common/headers/frontier.h"
#include "../Common/headers/logging.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/memory.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/shell_api.h"
#include "../Common/headers/db.h"
#include "../Common/headers/file.h"
#include "../Common/headers/db_format.h"
#include "../Common/headers/tableverbs.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/stringdefs.h"
#include "../Common/headers/scripts.h"
#include "../Common/headers/db_format.h"
#include "../Common/headers/dbinternal.h"
#include "../Common/headers/byteorder.h"
#include "../Common/headers/threadregistry.h"
#include "../Common/headers/threads.h"

// Portable headers
#include "../portable/file_working_dir.h"
#include "../portable/fileverbs_portable.h"

// CLI-specific headers
#include "cli_parser.h"
#include "cli_executor.h"
#include "cli_utils.h"
#include "repl.h"
#include "protocol_handler.h"
#include "ws_server.h"

extern long grabthreadglobals(void);
extern long releasethreadglobals(void);

/* hthreadglobals is defined in headless_threadglobals.c, declared in processinternal.h.
 * We use the hdlthreadglobals typedef from threadregistry.h to avoid header conflicts. */
extern hdlthreadglobals hthreadglobals;

// Version information
// FRONTIER_CLI_VERSION_STRING is defined at compile time from git tags via Makefile
#ifndef FRONTIER_CLI_VERSION_STRING
#define FRONTIER_CLI_VERSION_STRING "1.0.0-dev"
#endif
#define FRONTIER_CLI_BUILD_DATE __DATE__

// Default system root path
#define DEFAULT_SYSTEM_ROOT "databases/Frontier.root"

// System root search paths (for auto-discovery)
#define MAX_SEARCH_PATHS 5

// Global variables
static cli_options_t g_cli_options = {0};
static boolean g_initialized = false;
static boolean g_system_root_loaded = false;
static hdlfilenum g_system_root_fnum = 0;
static hdldatabaserecord g_previous_database = nil;
static char g_system_root_path[CLI_MAX_PATH_LENGTH + 1] = {0};

/*
 * Accessor functions for system root state.
 * Used by frontier.getFilePath() verb to access CLI state without exposing globals.
 *
 * Thread Safety: These accessors are NOT thread-safe. They are designed for use
 * during single-threaded startup only. The path is set before the loaded flag
 * (see hydrate_system_root_database) to avoid partial reads, but there is no
 * mutex protection. If multi-threading is added, these will need synchronization.
 */
boolean cli_is_system_root_loaded(void) {
    return g_system_root_loaded;
}

const char* cli_get_system_root_path(void) {
    return g_system_root_path;
}

boolean cli_should_skip_startup(void) {
    return g_cli_options.skip_startup;
}

// Function prototypes
static void print_usage(const char* program_name);
static void print_version(void);
static boolean initialize_frontier_runtime(void);
static void cleanup_frontier_runtime(void);
static boolean execute_script_mode(void);
static boolean load_system_root_database(const char* path);
static void unload_system_root_database(void);
static boolean ensure_named_subtable(hdlhashtable parent, const unsigned char *name, hdlhashtable *out, boolean mark_dont_save);
static boolean hydrate_system_root_database(const char* path);
static boolean read_root_table_address(const char *path, dbaddress *adr_out, short *version_out);
static void log_system_subtable_status(const char *phase,
                                       hdlhashtable system,
                                       hdlhashtable verbs,
                                       hdlhashtable builtins,
                                       hdlhashtable agents,
                                       hdlhashtable paths,
                                       hdlhashtable resources,
                                       hdlhashtable menubar,
                                       hdlhashtable objectmodel);
static int get_system_root_search_paths(char paths[][CLI_MAX_PATH_LENGTH + 1], int max_paths);

/* Main entry point: initializes runtime, loads database, and dispatches to execution mode. */
int main(int argc, char* argv[]) {
    // Initialize logging system (reads FRONTIER_LOG_LEVEL, FRONTIER_LOG_COMPONENT, FRONTIER_LOG_FORMAT env vars)
    log_init();

    // Initialize process-level default working directory
    char cwd_buf[4096];
    const char *cwd = getcwd(cwd_buf, sizeof(cwd_buf));
    if (!cwd) {
        // Fall back to executable directory if getcwd() fails
        char resolved_path[4096];
        if (realpath(argv[0], resolved_path) != NULL) {
            char *last_slash = strrchr(resolved_path, '/');
            if (last_slash) {
                *last_slash = '\0';
                cwd = resolved_path;
            }
        }
    }

    if (cwd) {
        init_default_working_dir(cwd);
    } else {
        // Last resort: use current directory
        init_default_working_dir(".");
    }

    // Initialize file handle cleanup (must be called before any file operations)
    init_file_handle_cleanup();

    // Parse command line arguments
    if (!cli_parse_arguments(argc, argv, &g_cli_options)) {
        log_error(LOG_COMP_GENERAL, "Error: Invalid command line arguments");
        print_usage(argv[0]);
        return 1;
    }

    // Apply --log spec if provided (overrides env var settings)
    if (g_cli_options.log_spec != NULL) {
        log_parse_spec(g_cli_options.log_spec);
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

    /* Handle --migrate mode: migrate database to v7 and exit */
    if (g_cli_options.migrate_database != NULL) {
        boolean migrated = false;
        const char *input = g_cli_options.migrate_database;
        size_t input_len = strlen(input);

        /* Validate path length */
        if (input_len >= CLI_MAX_PATH_LENGTH) {
            fprintf(stderr, "Error: Input path too long\n");
            return 1;
        }

        /* Check --output target BEFORE migration to avoid side effects on failure */
        if (g_cli_options.output_path != NULL) {
            if (access(g_cli_options.output_path, F_OK) == 0 && !g_cli_options.force_overwrite) {
                fprintf(stderr, "Error: Output file already exists: %s\n", g_cli_options.output_path);
                fprintf(stderr, "Use --force (-f) to overwrite.\n");
                return 1;
            }
        }

        /* Initialize minimal runtime for migration */
        if (!db_format_prepare_runtime()) {
            fprintf(stderr, "Error: Failed to initialize runtime for migration\n");
            return 1;
        }

        if (g_cli_options.output_path != NULL) {
            /* --output mode: write v7 directly to the specified path.
             * The v6 input file is never modified. */

            /* Check if already v7 before attempting migration.
             * Use detect_database_format + db_format_mode_apply to handle
             * endianness correctly, matching the ensure_database_v7 pattern. */
            {
                FILE *fp_check = fopen(input, "rb");
                if (!fp_check) {
                    fprintf(stderr, "Error: Cannot open input file: %s\n", input);
                    return 1;
                }
                tydatabaserecord hdr;
                boolean hdr_ok = fread(&hdr, sizeof hdr, 1, fp_check) == 1;
                fclose(fp_check);
                if (!hdr_ok || !detect_database_format(&hdr)) {
                    fprintf(stderr, "Error: Cannot read database header: %s\n", input);
                    return 1;
                }
                db_format_mode detected = {hdr.versionnumber >= 7, false};
                db_format_mode_apply(&detected);
                if (db_format_mode_current().use_64bit_format) {
                    printf("Already v7 format: %s\n", input);
                    return 0;
                }
            }

            if (!migrate_32bit_to_64bit_to_output(input, g_cli_options.output_path)) {
                fprintf(stderr, "Error: Migration failed for: %s\n", input);
                return 1;
            }
            printf("Migrated: %s -> %s\n", input, g_cli_options.output_path);
        } else {
            /* In-place mode: ensure_database_v7 renames v6 to .v6.root backup
             * and writes v7 to the original .root path. */
            if (g_cli_options.force_overwrite) {
                fprintf(stderr, "Note: --force has no effect in in-place migration mode (no --output specified)\n");
            }
            if (!ensure_database_v7(input, &migrated, NULL, 0)) {
                fprintf(stderr, "Error: Migration failed for: %s\n", input);
                return 1;
            }

            if (!migrated) {
                printf("Already v7 format: %s\n", input);
                return 0;
            }

            {
                char v6_backup[CLI_MAX_PATH_LENGTH + 16];
                db_format_derive_v6_backup_path(input, v6_backup, sizeof(v6_backup));
                printf("Migrated in-place: %s\n", input);
                printf("  v6 backed up to: %s\n", v6_backup);
            }
        }
        return 0;
    }

    /* Set JSON mode early to suppress logs on stderr when JSON output is enabled.
     * Must be set BEFORE database loading to prevent log messages from appearing in JSON output. */
    cli_set_json_mode(g_cli_options.output_json);
    log_set_suppressed(g_cli_options.output_json);

    /* Set default log level to ERROR for REPL mode (unless user explicitly configured logging)
     * This reduces startup noise from database warnings and WPText conversion messages.
     * Script mode keeps default WARN level for better diagnostics.
     * Respect explicit logging config: FRONTIER_LOG, FRONTIER_LOG_LEVEL, or --log */
    if (getenv("FRONTIER_LOG_LEVEL") == NULL &&
        getenv("FRONTIER_LOG") == NULL &&
        g_cli_options.log_spec == NULL &&
        g_cli_options.script_file == NULL &&
        g_cli_options.inline_script == NULL) {
        log_set_level(LOG_LEVEL_ERROR);
    }

    /* Always initialize runtime and hydrate system root (default path). */
    if (!initialize_frontier_runtime()) {
        log_error(LOG_COMP_GENERAL, "Error: Failed to initialize Frontier runtime");
        return 1;
    }

    /* Initialize interactive mode detection after thread globals are ready.
     * Note: cli_init_interactive_mode() accesses fl_batch_mode and fl_interactive_detected
     * which are thread-local via macros defined in processinternal.h. */
    cli_init_interactive_mode(g_cli_options.batch_mode);

    /* Auto-load system root database if not explicitly specified */
    const char *system_root_to_load = g_cli_options.system_root;
    static char found_system_root_path[CLI_MAX_PATH_LENGTH + 1];  /* Static buffer for auto-discovered path */
    if (system_root_to_load == NULL) {
        /* Build search path list and try each location in order */
        char search_paths[MAX_SEARCH_PATHS][CLI_MAX_PATH_LENGTH + 1];
        int num_paths = get_system_root_search_paths(search_paths, MAX_SEARCH_PATHS);

        for (int i = 0; i < num_paths; i++) {
            if (access(search_paths[i], F_OK) == 0) {
                /* Copy to static buffer to avoid dangling pointer when search_paths goes out of scope */
                strncpy(found_system_root_path, search_paths[i], sizeof(found_system_root_path) - 1);
                found_system_root_path[sizeof(found_system_root_path) - 1] = '\0';
                system_root_to_load = found_system_root_path;

                /* Check if this is v6 format (will auto-migrate) */
                const char *ext = strrchr(found_system_root_path, '.');
                boolean is_v6 = (ext != NULL && strcmp(ext, ".root") == 0);

                if (is_v6) {
                    log_info(LOG_COMP_STARTUP, "Auto-loading system root: %s (will auto-migrate to v7)", system_root_to_load);
                } else {
                    log_info(LOG_COMP_STARTUP, "Auto-loading system root: %s", system_root_to_load);
                }
                break;
            }
        }
        /* If no path exists, continue without system root (headless mode) */
    }

    if (system_root_to_load != NULL) {
        if (!hydrate_system_root_database(system_root_to_load)) {
            log_error(LOG_COMP_GENERAL, "Error: Failed to load system root: %s", system_root_to_load);
            cleanup_frontier_runtime();
            return 1;
        }

        /* Run startup scripts AFTER full hydration.
         * This ensures:
         * 1. EFP tables are properly linked
         * 2. system.paths is populated and resolved
         * 3. Database tables are augmented with EFP implementations
         * Scripts run by default; use --skip-startup or FRONTIER_HEADLESS_RUN_STARTUP=0 to skip. */
        if (!loadsystemscripts()) {
            log_error(LOG_COMP_GENERAL, "Error: startup scripts failed for: %s", system_root_to_load);
            /* Continue despite startup script errors - they're not fatal */
        }
    }

    // Start WebSocket server if --ws-port was specified
    ws_server_t ws_server;
    ws_server_t *ws_server_ptr = NULL;

    if (g_cli_options.ws_port > 0) {
        if (ws_server_init(&ws_server, g_cli_options.ws_port) == 0) {
            ws_server_ptr = &ws_server;
        } else {
            log_error(LOG_COMP_GENERAL, "Failed to start WebSocket server on port %d", g_cli_options.ws_port);
        }
    }

    // Determine execution mode
    boolean success = false;
    int exit_code = 0;

    if (g_cli_options.protocol_mode) {
        // NDJSON protocol mode - structured JSON over stdin/stdout
        exit_code = protocol_main(&g_cli_options, ws_server_ptr);
    } else if (g_cli_options.script_file != NULL || g_cli_options.inline_script != NULL) {
        // Batch mode - execute script and exit
        success = execute_script_mode();
        exit_code = success ? 0 : 1;
    } else {
        // Interactive mode - enter REPL
        exit_code = repl_main(&g_cli_options, ws_server_ptr);
    }

    // Shutdown WebSocket server
    if (ws_server_ptr != NULL) {
        ws_server_shutdown(ws_server_ptr);
    }

    // Cleanup
    cleanup_frontier_runtime();

    return exit_code;
}

/* Builds list of system root search paths for auto-discovery (env var, standard locations, cwd). */
static int get_system_root_search_paths(char paths[][CLI_MAX_PATH_LENGTH + 1], int max_paths) {
    int count = 0;
    char expanded_path[CLI_MAX_PATH_LENGTH + 1];
    char cwd[CLI_MAX_PATH_LENGTH + 1];
    char exe_dir[CLI_MAX_PATH_LENGTH + 1];

    // Get current working directory
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        log_debug(LOG_COMP_GENERAL, "get_system_root_search_paths: getcwd failed (errno=%d)", errno);
        cwd[0] = '\0';
    }

    // Get executable directory
    exe_dir[0] = '\0';
#ifdef __APPLE__
    {
        char exe_path[CLI_MAX_PATH_LENGTH + 1];
        uint32_t size = sizeof(exe_path);
        if (_NSGetExecutablePath(exe_path, &size) == 0) {
            // Find last slash to get directory
            char *last_slash = strrchr(exe_path, '/');
            if (last_slash != NULL) {
                size_t dir_len = (size_t)(last_slash - exe_path);
                if (dir_len < sizeof(exe_dir)) {
                    memcpy(exe_dir, exe_path, dir_len);
                    exe_dir[dir_len] = '\0';
                }
            }
        }
    }
#else
    // Linux: read /proc/self/exe symlink
    {
        char exe_path[CLI_MAX_PATH_LENGTH + 1];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len != -1) {
            exe_path[len] = '\0';
            char *last_slash = strrchr(exe_path, '/');
            if (last_slash != NULL) {
                size_t dir_len = (size_t)(last_slash - exe_path);
                if (dir_len < sizeof(exe_dir)) {
                    memcpy(exe_dir, exe_path, dir_len);
                    exe_dir[dir_len] = '\0';
                }
            }
        }
    }
#endif

    // 1. Check FRONTIER_ROOT environment variable (highest priority)
    const char *frontier_root_env = getenv("FRONTIER_ROOT");
    if (frontier_root_env != NULL && frontier_root_env[0] != '\0') {
        // Expand ~ if present
        if (frontier_root_env[0] == '~') {
            const char *home = getenv("HOME");
            if (home != NULL) {
                snprintf(expanded_path, sizeof(expanded_path), "%s%s", home, frontier_root_env + 1);
                strncpy(paths[count], expanded_path, CLI_MAX_PATH_LENGTH);
                paths[count][CLI_MAX_PATH_LENGTH] = '\0';
                count++;
            }
        } else {
            strncpy(paths[count], frontier_root_env, CLI_MAX_PATH_LENGTH);
            paths[count][CLI_MAX_PATH_LENGTH] = '\0';
            count++;
        }
    }

    // 2. Current working directory
    if (count < max_paths && cwd[0] != '\0') {
        snprintf(paths[count], CLI_MAX_PATH_LENGTH + 1, "%s/Frontier.root", cwd);
        count++;
    }

    // 3. Executable directory
    if (count < max_paths && exe_dir[0] != '\0') {
        snprintf(paths[count], CLI_MAX_PATH_LENGTH + 1, "%s/Frontier.root", exe_dir);
        count++;
    }

    // 4. User Application Support directory
    const char *home = getenv("HOME");
    if (home != NULL && count < max_paths) {
        snprintf(paths[count], CLI_MAX_PATH_LENGTH + 1,
                 "%s/Library/Application Support/Frontier/Frontier.root", home);
        count++;
    }

    // 5. ~/.frontier/Frontier.root (Linux convention)
    if (home != NULL && count < max_paths) {
        snprintf(paths[count], CLI_MAX_PATH_LENGTH + 1,
                 "%s/.frontier/Frontier.root", home);
        count++;
    }

    return count;
}

/* Reads a big-endian integer of specified length from a byte buffer. */
static uint64_t read_big_endian(const unsigned char *data, size_t length) {
    uint64_t value = 0;
    for (size_t i = 0; i < length; ++i) {
        value = (value << 8) | (uint64_t) data[i];
    }
    return value;
}

/* Locates the root table address in the database by reading the Cancoon view header. */
static boolean read_root_table_address(const char *path, dbaddress *adr_out, short *version_out) {
    (void)path; /* path only used for logging; runtime state comes from databasedata */

    if (adr_out == NULL)
        return false;

    if (databasedata == nil) {
        cli_log_error("read_root_table_address called without an open database");
        return false;
    }

    short header_version = (**databasedata).versionnumber;
    if (version_out != NULL)
        *version_out = header_version;

    dbaddress view_address = nildbaddress;
    dbgetview(cancoonview, &view_address);
    if (view_address == nildbaddress) {
        cli_log_error("View %d is empty in system root", cancoonview);
        return false;
    }

    boolean flfree = false;
    long payload_size = 0;
    tyvariance variance = 0;
    if (!dbreadheader(view_address, &flfree, &payload_size, &variance)) {
        cli_log_error("Unable to read database header for view address 0x%08llx", (unsigned long long)view_address);
        return false;
    }

    long effective_size = payload_size - (long)variance;
    dbaddress root_address = view_address; /* fallback: treat view address as root */

    if (!flfree && effective_size >= 6) {
        unsigned char cancoon_header[6];
        memset(cancoon_header, 0, sizeof cancoon_header);

        if (dbreference(view_address, (long)sizeof cancoon_header, cancoon_header)) {
            short cancoon_version = (short)read_big_endian(cancoon_header, 2);
            dbaddress adr_root = (dbaddress)read_big_endian(cancoon_header + 2, 4);

            if ((cancoon_version == 2 || cancoon_version == 3) && adr_root != nildbaddress) {
                root_address = adr_root;
                cli_log_debug("Selected view[%d] address=0x%08llx via Cancoon v%d → root=0x%08llx",
                              cancoonview,
                              (unsigned long long)view_address,
                              (int)cancoon_version,
                              (unsigned long long)root_address);
            } else {
                cli_log_debug("Cancoon header at 0x%08llx (v%d) points to 0x%08llx; using %s",
                              (unsigned long long)view_address,
                              (int)cancoon_version,
                              (unsigned long long)adr_root,
                              (adr_root != nildbaddress) ? "fallback view address" : "view address (nil root)");
            }
        } else {
            cli_log_warn("Failed to read Cancoon header at 0x%08llx; falling back to view address",
                         (unsigned long long)view_address);
        }
    } else {
        cli_log_debug("View[%d] payload (%ld bytes) is not a Cancoon record; using address 0x%08llx",
                      cancoonview,
                      effective_size,
                      (unsigned long long)view_address);
    }

    *adr_out = root_address;
    return true;
}

/* Prints command-line usage information and examples. */
static void print_usage(const char* program_name) {
    printf("Frontier CLI - Command Line Interface for UserTalk Script Execution\n");
    printf("Version %s (%s)\n\n", FRONTIER_CLI_VERSION_STRING, FRONTIER_CLI_BUILD_DATE);
    
    printf("Usage: %s [OPTIONS] [SCRIPT_FILE]\n\n", program_name);
    
    printf("Execution Modes:\n");
    printf("  Script Execution:\n");
    printf("    %s script.usertalk                    # Execute UserTalk script file\n", program_name);
    printf("    %s -e \"3 + 4\"                         # Execute inline script\n", program_name);
    printf("\n");

    printf("Options:\n");
    printf("  -e, --execute SCRIPT     Execute inline UserTalk script\n");
    printf("  --system-root PATH       Load system root database before executing scripts\n");
    printf("  -b, --batch              Batch mode (disable interactive prompts)\n");
    printf("  --non-interactive        Alias for --batch\n");
    printf("  --migrate PATH           Migrate v6 database to v7 format and exit\n");
    printf("  --output PATH            Output path for migrated database (default: in-place, v6 backed up)\n");
    printf("  -f, --force              Overwrite existing output file (only applies with --output)\n");
    printf("  --skip-startup           Skip system.startup scripts (they run by default)\n");
    printf("  --output-json            Output results in JSON format\n");
    printf("  -v, --verbose            Verbose output\n");
    printf("  --debug                  Debug mode\n");
    printf("  --log SPEC               Set per-component log levels (e.g., db:trace,lang:warn)\n");
    printf("  --ws-port PORT           Start WebSocket server on PORT (localhost only, no auth).\n");
    printf("                           WARNING: Any local process or browser page (including file://)\n");
    printf("                           can access the ODB while the server is running.\n");
    printf("  -h, --help               Show this help message\n");
    printf("  --version                Show version information\n");
    printf("\n");

    printf("Environment Variables:\n");
    printf("  FRONTIER_LOG             Per-component log levels (e.g., db:trace,lang:warn)\n");
    printf("                           Overrides FRONTIER_LOG_LEVEL and FRONTIER_LOG_COMPONENT\n");
    printf("  FRONTIER_LOG_LEVEL       Set global log level (TRACE, DEBUG, INFO, WARN, ERROR)\n");
    printf("  FRONTIER_LOG_COMPONENT   Filter logs by component (db, hash, lang, etc.)\n");
    printf("  FRONTIER_HEADLESS_RUN_STARTUP  Set to 0 to skip system.startup scripts (default: run)\n");
    printf("\n");

    printf("Examples:\n");
    printf("  # Execute inline script\n");
    printf("  %s -e \"local(x = 5); x * 2\"\n", program_name);
    printf("\n");
    printf("  # Execute a UserTalk script file\n");
    printf("  %s myscript.usertalk\n", program_name);
    printf("\n");
    printf("  # Execute with system root database\n");
    printf("  %s --system-root databases/Frontier.root -e \"sizeOf(system)\"\n", program_name);
    printf("\n");
    printf("  # Migrate v6 database to v7 format (v6 backed up to .v6.root)\n");
    printf("  %s --migrate Frontier.root\n", program_name);
    printf("\n");
    printf("  # Migrate with explicit output path\n");
    printf("  %s --migrate legacy/Frontier.root --output databases/Frontier.root\n", program_name);
    printf("\n");
    printf("  # Force overwrite existing output file\n");
    printf("  %s --migrate Frontier.root --output Frontier-v7.root -f\n", program_name);
    printf("\n");
    printf("  # Execute with JSON output (for automation/testing)\n");
    printf("  %s --output-json -e \"1+1\"\n", program_name);
    printf("\n");

    printf("For detailed documentation, see: docs/CLI_USAGE_GUIDE.md\n");
    printf("\n");
}

/* Prints version and copyright information. */
static void print_version(void) {
    printf("Frontier CLI %s (%s)\n", FRONTIER_CLI_VERSION_STRING, FRONTIER_CLI_BUILD_DATE);
    printf("Copyright (C) 1992-2026 UserLand Software, Inc. and Contributors\n");
    printf("This is free software; see the source for copying conditions.\n");
}

/* Initializes the Frontier runtime: logging, thread globals, and optionally loads the system root. */
static boolean initialize_frontier_runtime(void) {
    if (g_initialized) {
        return true;
    }
    
    // Initialize CLI logging
    if (!cli_init_logging(g_cli_options.verbose, g_cli_options.debug)) {
        log_error(LOG_COMP_GENERAL, "Error: Failed to initialize logging");
        return false;
    }

    if (!db_format_prepare_runtime()) {
        log_error(LOG_COMP_GENERAL, "Error: Failed to initialize Frontier runtime core");
        cli_cleanup_logging();
        return false;
    }

    /* Initialize thread registry and register main thread with idapplicationthread (2) */
    if (!init_thread_registry()) {
        log_error(LOG_COMP_GENERAL, "Error: Failed to initialize thread registry");
        cli_cleanup_logging();
        return false;
    }

    {
        frontier_pthread_record *main_rec = register_main_thread((long)idapplicationthread);
        if (main_rec == NULL) {
            log_error(LOG_COMP_GENERAL, "Error: Failed to register main thread in registry");
            cleanup_thread_registry();
            cli_cleanup_logging();
            return false;
        }
        main_rec->hglobals = hthreadglobals;
        main_rec->pthread_id = pthread_self();
    }

    headless_threading_init(); /* Main thread acquires GIL before any scripts run */

    if (g_cli_options.system_root != NULL) {
        if (!load_system_root_database(g_cli_options.system_root)) {
            cli_log_error("Failed to load system root database: %s", g_cli_options.system_root);
            releasethreadglobals();
            cli_cleanup_logging();
            return false;
        }
    }
    
    g_initialized = true;
    cli_log_info("Frontier runtime initialized successfully");
    
    return true;
}

/* Releases runtime resources: unloads database, releases thread globals, cleans up logging. */
static void cleanup_frontier_runtime(void) {
    if (!g_initialized) {
        return;
    }
    
    cli_log_info("Cleaning up Frontier runtime");

    /* Wait for all spawned threads to finish BEFORE unloading databases.
     * Spawned threads may still be running (blocked on GIL) and need roottable
     * and other database structures to be intact. */
    headless_threading_shutdown();

    if (g_system_root_loaded) {
        unload_system_root_database();
    }

    /* Release main thread registry record before cleanup. */
    {
        frontier_pthread_record *main_rec = get_thread_by_id((long)idapplicationthread);
        if (main_rec) {
            release_thread_record(main_rec);  /* release lookup ref */
            free_thread_record(main_rec);     /* release initial ref */
        }
    }
    cleanup_thread_registry();

    // Cleanup Frontier runtime
    releasethreadglobals();

    // Cleanup CLI components
    cli_cleanup_logging();
    
    g_initialized = false;
}

/* Finds or creates a named subtable under a parent table. */
static boolean ensure_named_subtable(hdlhashtable parent, const unsigned char *name, hdlhashtable *out, boolean mark_dont_save) {
    if (parent == nil || name == NULL) {
        return false;
    }

    bigstring bsname;
    copystring(name, bsname);

    hdlhashtable table = nil;
    if (findnamedtable(parent, bsname, &table)) {
        if (out != NULL) {
            *out = table;
        }
        return true;
    }

    if (tablenewsubtable(parent, bsname, &table)) {
        if (mark_dont_save) {
            langexternaldontsave(parent, bsname);
        }
        if (out != NULL) {
            *out = table;
        }
        return true;
    }

    if (findnamedtable(parent, bsname, &table)) {
        if (out != NULL) {
            *out = table;
        }
        return true;
    }

    return false;
}

/* Logs the current state of system subtables for debugging hydration issues. */
static void log_system_subtable_status(const char *phase,
                                       hdlhashtable system,
                                       hdlhashtable verbs,
                                       hdlhashtable builtins,
                                       hdlhashtable agents,
                                       hdlhashtable paths,
                                       hdlhashtable resources,
                                       hdlhashtable menubar,
                                       hdlhashtable objectmodel) {
    if (phase == NULL) {
        phase = "unknown";
    }

    cli_log_debug("system table snapshot (%s): system=%p verbs=%p builtins=%p agents=%p paths=%p resources=%p menubar=%p objectmodel=%p",
                 phase,
                 (void *)system,
                 (void *)verbs,
                 (void *)builtins,
                 (void *)agents,
                 (void *)paths,
                 (void *)resources,
                 (void *)menubar,
                 (void *)objectmodel);
}

/* Loads and fully initializes the system root database, linking EFP tables and resolving paths. */
static boolean hydrate_system_root_database(const char* path) {
    /* Always start from a clean slate; useful to confirm entry. */
#if defined(FRONTIER_HEADLESS)
    log_trace(LOG_COMP_STARTUP, "hydrate_system_root_database enter path=%s", path ? path : "(nil)");
#endif
    cleartablestructureglobals();

    if (path == NULL) {
        cli_log_error("No system root path provided for hydration");
        return false;
    }

    if (!g_initialized) {
        cli_log_error("Runtime must be initialized before hydrating %s", path);
        return false;
    }

    boolean migrated = false;
    char actual_path[1024];
    if (!ensure_database_v7(path, &migrated, actual_path, sizeof actual_path)) {
        cli_log_error("Failed to verify database format before hydration: %s", path);
        return false;
    }

    /* After ensure_database_v7, we always have a v7 database (either the original if already v7,
     * or the original path now containing the migrated v7 data, with v6 backed up to .v6.root).
     *
     * v7 databases are opened read-write by default to allow startup scripts and system table
     * updates. Use FRONTIER_OPEN_READONLY=1 environment variable to force read-only mode. */
    boolean flreadonly_for_hydration = (getenv("FRONTIER_OPEN_READONLY") != NULL);

    /* Use the output path from ensure_database_v7 if it differs from input.
     * This handles both fresh migrations and cases where a v7 file already exists. */
    if (actual_path[0] != '\0' && strcmp(path, actual_path) != 0) {
        if (migrated) {
            cli_log_info("Migrated legacy system root to v7 format (written to): %s", actual_path);
        }
        path = actual_path;  /* Use the v7 file for hydration */
    }

    bigstring bspath;
    copyctopstring(path, bspath);

    tyfilespec fs;
    memset(&fs, 0, sizeof fs);
    if (!pathtofilespec(bspath, &fs)) {
        cli_log_error("Unable to convert system root path to filespec: %s", path);
        return false;
    }

    hdlfilenum fnum = 0;
    boolean ok = false;
    boolean file_open = false;
    boolean db_open = false;
    boolean dispose_rootvariable = false;

    if (!openfile(&fs, &fnum, flreadonly_for_hydration)) {
        cli_log_error("Unable to open system root for hydration: %s", path);
        return false;
    }
    file_open = true;

    hdldatabaserecord previous = databasedata;

    if (!dbopenfile(fnum, flreadonly_for_hydration)) {
        cli_log_error("dbopenfile failed for system root: %s (readonly=%s)", path, flreadonly_for_hydration ? "true" : "false");
        goto cleanup;
    }
    db_open = true;
    cli_log_debug("hydro databasedata views[0]=0x%08llx views[1]=0x%08llx views[2]=0x%08llx",
                  (unsigned long long)((**databasedata).views[0]),
                  (unsigned long long)((**databasedata).views[1]),
                  (unsigned long long)((**databasedata).views[2]));

    short header_version = 0;
    dbaddress adr = nildbaddress;
    if (!read_root_table_address(path, &adr, &header_version)) {
        cli_log_error("Unable to locate root table header in %s", path);
        goto cleanup;
    }
    cli_log_debug("Hydration header version=%d rootAdr=0x%08llx", header_version, (unsigned long long)adr);

    Handle hrootvariable = nil;
    hdlhashtable hroot = nil;
    /* Reset any cached state from previous loads. */
    cleartablestructureglobals();
    /* Load directly from disk. */
    if (!tableloadsystemtable(adr, &hrootvariable, &hroot, false)) {
        cli_log_error("Failed to load system table while hydrating %s", path);
        goto cleanup;
    }

    dispose_rootvariable = true;

    /* Materialize all disk values (including nested external tables with flinmemory=0).
     * This is critical for v7 databases that may have external tables with stale v6 addresses.
     * Without this, accessing nested tables like system.verbs.colors will fail. */
    if (!langhash_materialize_disk_values(hroot)) {
        cli_log_error("Failed to materialize disk values in system root while hydrating %s", path);
        goto cleanup;
    }

    rootvariable = hrootvariable;
    roottable = hroot;
    currenthashtable = roottable;

    if (hashtablestack == nil) {
        if (!newclearhandle(longsizeof(tytablestack), (Handle *)&hashtablestack)) {
            cli_log_error("Failed to allocate hashtablestack while hydrating %s", path);
            goto cleanup;
        }
        (**hashtablestack).toptables = 0;
    }

    ok = checktablestructure(true);
    if (!ok) {
        cli_log_warn("checktablestructure reported issues while hydrating %s", path);
        log_system_subtable_status("hydrate: post-checktablestructure", systemtable, verbstable, builtinstable, agentstable, pathstable, resourcestable, menubartable, objectmodeltable);
    }

    /* Link in-memory compiler tables (efptable with verb callbacks) into loaded system table */
    if (!linksystemtablestructure(hroot)) {
        cli_log_error("Unable to link system tables while hydrating %s", path);
        goto cleanup;
    }

    /* Populate system.paths with processor shortcuts (must be AFTER linksystemtablestructure, BEFORE resolve_system_paths) */
    if (!headless_init_system_paths(hroot)) {
        cli_log_error("Unable to populate system.paths while hydrating %s", path);
        goto cleanup;
    }

    /* Eagerly resolve system.paths addresses now that EFP tables are linked */
    if (!resolve_system_paths(hroot)) {
        cli_log_error("Unable to resolve system.paths addresses while hydrating %s", path);
        goto cleanup;
    }

    /* Augment database tables with EFP implementations for bare verb resolution */
    if (!augment_database_tables_with_efp(hroot)) {
        cli_log_error("Unable to augment database tables with EFP while hydrating %s", path);
        goto cleanup;
    }

    boolean created_optional = false;
    if (systemtable != nil) {
        if (resourcestable == nil && ensure_named_subtable(systemtable, nameresourcestable, &resourcestable, false))
            created_optional = true;

        if (pathstable == nil && ensure_named_subtable(systemtable, namepathstable, &pathstable, false))
            created_optional = true;

        hdlhashtable menustable = nil;
        bigstring bsmenus;
        copyctopstring("menus", bsmenus);
        if (ensure_named_subtable(systemtable, bsmenus, &menustable, false)) {
            if (menubartable == nil && ensure_named_subtable(menustable, namemenubartable, &menubartable, false))
                created_optional = true;
        }

        hdlhashtable macintoshtable = nil;
        bigstring bsmacintosh;
        copyctopstring("macintosh", bsmacintosh);
        if (ensure_named_subtable(systemtable, bsmacintosh, &macintoshtable, false)) {
            bigstring bsobjectmodel;
            copyctopstring("objectmodel", bsobjectmodel);
            if (objectmodeltable == nil && ensure_named_subtable(macintoshtable, bsobjectmodel, &objectmodeltable, false))
                created_optional = true;
        }
    }
    log_system_subtable_status("hydrate: after optional creation",
                               systemtable,
                               verbstable,
                               builtinstable,
                               agentstable,
                               pathstable,
                               resourcestable,
                               menubartable,
                               objectmodeltable);

    /* Only save if we made changes (created optional tables). Skip for freshly-migrated databases.
     * Also skip if no optional tables were created - v7 databases are already complete. */
    if ((!migrated && created_optional)) {
        boolean repack_scope = false;
        db_format_mode mode = {true, true};  /* 64-bit, adapter_repack */
        db_format_mode_push(&mode);
        repack_scope = true;
        if (!tablesavesystemtable(hrootvariable, &adr)) {
            if (repack_scope) {
                db_format_mode_pop();
                repack_scope = false;
            }
            cli_log_error("Failed to save system table while hydrating %s", path);
            goto cleanup;
        }
        if (repack_scope) {
            db_format_mode_pop();
            repack_scope = false;
        }
    } else {
        cli_log_debug("Skipping save for freshly-migrated database: %s", path);
    }

    dbsetview(cancoonview, adr);

    /* Log which system root was loaded and whether it was auto-migrated */
    if (migrated) {
        log_info(LOG_COMP_STARTUP, "Loaded system root: %s (migrated from v6 to v7)", path);
    } else {
        log_info(LOG_COMP_STARTUP, "Loaded system root: %s", path);
    }

    /* Set globals for frontier.getFilePath() verb access.
     * Thread safety: Set path BEFORE setting loaded flag to avoid race condition
     * where another thread sees loaded=true but path is still being written. */
    snprintf(g_system_root_path, sizeof(g_system_root_path), "%s", path);
    g_system_root_loaded = true;

    /* NOTE: Startup scripts are NOT run here during hydration.
     * They are run in main() AFTER hydration completes, which ensures:
     * 1. EFP tables are properly linked (linksystemtablestructure)
     * 2. system.paths is populated and resolved
     * 3. Database tables are augmented with EFP implementations
     * This avoids running scripts during v6->v7 migration when database state is incomplete. */

    ok = true;

cleanup:
    /* On failure, close database, dispose root variable, clear globals, and restore previous database */
    if (!ok) {
        if (db_open) {
            if (!dbclose()) {
                cli_log_warn("dbclose reported failure while hydrating %s", path);
            }
            dbdispose();
        }

        if (dispose_rootvariable && hrootvariable != nil)
            disposehandle(hrootvariable);

        databasedata = previous;
        cleartablestructureglobals();
        currenthashtable = nil;

        if (file_open && !closefile(fnum)) {
            cli_log_warn("Failed to close hydrated system root file handle: %s", path);
        }
    }
    /* On success, database remains open and globals remain set for runtime use */

    return ok;
}

/* Internal implementation for loading the system root database with optional hydration. */
static boolean load_system_root_database_internal(const char* path, boolean allow_hydrate) {
    (void) allow_hydrate;
    if (path == NULL) {
        return true;
    }

    if (g_system_root_loaded) {
        cli_log_warn("System root already loaded; ignoring request for %s", path);
        return true;
    }

    size_t len = strlen(path);
    if (len == 0) {
        cli_log_error("System root path is empty");
        return false;
    }
    boolean migrated = false;
    char actual_path[1024];
    if (!ensure_database_v7(path, &migrated, actual_path, sizeof actual_path)) {
        cli_log_error("Failed to ensure system root is modern: %s", path);
        return false;
    }
    if (migrated) {
        cli_log_info("Migrated legacy system root to v7 format (written to): %s", actual_path);
        path = actual_path;  /* Use the v7 file */
        /* After migration, tear down any in-memory v6 root before reloading v7. */
        cleartablestructureglobals();
    }

    len = strlen(path);  /* Recalculate length after potential path change */
    if (len > lenbigstring) {
        cli_log_error("System root path exceeds %d characters (got %zu)", lenbigstring, len);
        return false;
    }

    bigstring bspath;
    copyctopstring(path, bspath);

    tyfilespec fs;
    memset(&fs, 0, sizeof fs);
    if (!pathtofilespec(bspath, &fs)) {
        cli_log_error("Unable to convert system root path to filespec: %s", path);
        return false;
    }

    hdlfilenum fnum = 0;
    if (!openfile(&fs, &fnum, true)) {
        cli_log_error("Unable to open system root for reading: %s", path);
        return false;
    }

    hdldatabaserecord previous = databasedata;

    if (!dbopenfile(fnum, true)) {
        cli_log_error("dbopenfile failed for system root: %s", path);
        closefile(fnum);
        databasedata = previous;
        return false;
    }
    cli_log_debug("databasedata views[0]=0x%08llx views[1]=0x%08llx views[2]=0x%08llx",
                  (unsigned long long)((**databasedata).views[0]),
                  (unsigned long long)((**databasedata).views[1]),
                  (unsigned long long)((**databasedata).views[2]));

    short header_version = 0;
    dbaddress adr = nildbaddress;
    if (!read_root_table_address(path, &adr, &header_version)) {
        cli_log_error("Unable to locate system root table header for %s", path);
        cleartablestructureglobals();
        dbdispose();
        closefile(fnum);
        databasedata = previous;
        return false;
    }
    cli_log_debug("System root header version=%d rootAdr=0x%08llx", header_version, (unsigned long long)adr);

    Handle hrootvariable = nil;
    hdlhashtable hroot = nil;
    /* Always clear cached globals before loading. */
    cleartablestructureglobals();
    if (!tableloadsystemtable(adr, &hrootvariable, &hroot, false)) {
        cli_log_error("Failed to load system table from %s", path);
        cleartablestructureglobals();
        dbdispose();
        closefile(fnum);
        databasedata = previous;
        return false;
    }

    boolean structure_ready = true;
    boolean partial_warning = false;
    boolean applied_patch = false;

    if (!settablestructureglobals(hrootvariable, false)) {
        cli_log_warn("System table structure is invalid in %s", path);
        cli_log_debug("systemtable=%p verbstable=%p builtinstable=%p agentstable=%p pathstable=%p resourcestable=%p menubartable=%p objectmodeltable=%p",
                      (void *)systemtable,
                      (void *)verbstable,
                      (void *)builtinstable,
                      (void *)agentstable,
                      (void *)pathstable,
                      (void *)resourcestable,
                      (void *)menubartable,
                      (void *)objectmodeltable);
        log_system_subtable_status("load: post-checktablestructure", systemtable, verbstable, builtinstable, agentstable, pathstable, resourcestable, menubartable, objectmodeltable);
        structure_ready = false;

        if (systemtable != nil) {
            if (resourcestable == nil && ensure_named_subtable(systemtable, nameresourcestable, &resourcestable, true))
                applied_patch = true;

            if (pathstable == nil && ensure_named_subtable(systemtable, namepathstable, &pathstable, true))
                applied_patch = true;

            hdlhashtable menustable = nil;
            bigstring bsmenus;
            copyctopstring("menus", bsmenus);
            if (ensure_named_subtable(systemtable, bsmenus, &menustable, true)) {
                if (menubartable == nil && ensure_named_subtable(menustable, namemenubartable, &menubartable, true))
                    applied_patch = true;
            }

            hdlhashtable macintoshtable = nil;
            bigstring bsmacintosh;
            copyctopstring("macintosh", bsmacintosh);
            if (ensure_named_subtable(systemtable, bsmacintosh, &macintoshtable, true)) {
                bigstring bsobjectmodel;
                copyctopstring("objectmodel", bsobjectmodel);
                if (objectmodeltable == nil && ensure_named_subtable(macintoshtable, bsobjectmodel, &objectmodeltable, true))
                    applied_patch = true;
            }
        }

        log_system_subtable_status("load: after ensure_named_subtable patch",
                                   systemtable,
                                   verbstable,
                                   builtinstable,
                                   agentstable,
                                   pathstable,
                                   resourcestable,
                                   menubartable,
                                   objectmodeltable);

        if (applied_patch) {
            cli_log_debug("Applied fallback table creation for %s; structure now adequate", path);
            /* The fallback tables are optional - accept the structure as-is if critical tables exist */
            if (systemtable != nil && verbstable != nil && builtinstable != nil) {
                structure_ready = true;
            } else {
                cli_log_warn("System table structure still invalid after fallback initialization: %s", path);
                cli_log_debug("systemtable=%p verbstable=%p builtinstable=%p agentstable=%p pathstable=%p resourcestable=%p menubartable=%p objectmodeltable=%p",
                              (void *)systemtable,
                              (void *)verbstable,
                              (void *)builtinstable,
                              (void *)agentstable,
                              (void *)pathstable,
                              (void *)resourcestable,
                              (void *)menubartable,
                              (void *)objectmodeltable);
                structure_ready = false;
            }
        }

        if (!structure_ready || systemtable == nil || verbstable == nil) {
            dbdispose();
            closefile(fnum);
            databasedata = previous;
            return false;
        }

        if (partial_warning) {
            cli_log_warn("Proceeding with minimally hydrated system tables (optional tables may be missing) for %s", path);
        } else if (applied_patch) {
            cli_log_info("Loaded system root database with fallback table hydration: %s", path);
        } else {
            cli_log_warn("Proceeding despite partial system table validation; optional tables may be missing for %s", path);
        }
    }

    if (!linksystemtablestructure(roottable)) {
        cli_log_error("Unable to link system tables for %s", path);
        cleartablestructureglobals();
        dbdispose();
        closefile(fnum);
        databasedata = previous;
        return false;
    }

    currenthashtable = roottable;

    /* Set globals so callers know the database is loaded.
     * Thread safety: Set path BEFORE setting loaded flag to avoid race condition
     * where another thread sees loaded=true but path is still being written. */
    g_previous_database = previous;
    g_system_root_fnum = fnum;
    snprintf(g_system_root_path, sizeof(g_system_root_path), "%s", path);
    g_system_root_loaded = true;

    /* NOTE: Startup scripts are NOT run here. They are run in main() AFTER
     * hydrate_system_root_database() completes, which ensures EFP tables are
     * properly linked and system.paths is resolved before scripts execute.
     * See loadsystemscripts() call in main() after hydration. */

    cli_log_info("Loaded system root database: %s", g_system_root_path);
    return true;
}

/* Loads the system root database with full hydration enabled. */
static boolean load_system_root_database(const char* path) {
    return load_system_root_database_internal(path, true);
}

/* Saves the system root database to disk before unloading.
 * Called during cleanup to persist any changes made during script execution
 * (e.g., user.databases entries created by finishInstall during first-run). */
static void save_system_root_on_exit(void) {
    dbaddress root_adr;
    db_context ctx;

    if (databasedata == nil || rootvariable == nil) {
        return;
    }

    /* Note: We intentionally do NOT check dbdirtymask here.
     *
     * The dbdirtymask flag tracks disk-level allocation changes (new blocks
     * allocated/released), but in-memory table modifications (e.g., new entries
     * added via hashinsert) set fldirty on hash tables WITHOUT setting dbdirtymask.
     * This means in-memory changes can exist that dbdirtymask doesn't reflect.
     *
     * tablesavesystemtable() is efficient when nothing is dirty — it walks the
     * in-memory tree checking fldirty/flsubsdirty and no-ops for clean tables.
     * The cost of an unnecessary walk is minimal compared to losing data. */

    cli_log_info("Saving system root database before exit");

    /* Save the root table using v7 format */
    {
        db_format_mode mode = {true, false};  /* 64-bit, no adapter_repack */
        db_format_mode_push(&mode);

        if (!tablesavesystemtable(rootvariable, &root_adr)) {
            db_format_mode_pop();
            cli_log_warn("save_system_root_on_exit: tablesavesystemtable failed");
            return;
        }

        db_format_mode_pop();
    }

    /* Flush release stack */
    db_context_init(&ctx);

    if (!dbflushreleasestack_context(&ctx))
        cli_log_warn("save_system_root_on_exit: dbflushreleasestack_context failed");

    /* Update views[0] to point to the saved root table;
     * dbsetview already flushes the header to disk. */
    dbsetview(cancoonview, root_adr);

    cli_log_info("System root database saved successfully");
}

/* Unloads the system root database and clears all global table structures. */
static void unload_system_root_database(void) {
    if (!g_system_root_loaded) {
        return;
    }

    const char* path = (g_system_root_path[0] != '\0') ? g_system_root_path : "(unknown)";
    cli_log_info("Unloading system root database: %s", path);

    /* Save any changes made during this session before tearing down */
    save_system_root_on_exit();

    if (systemtable != nil) {
        if (!unlinksystemtablestructure()) {
            cli_log_warn("Failed to unlink system table structure during unload");
        }
    }

    cleartablestructureglobals();
    currenthashtable = nil;

    if (databasedata != nil) {
        dbdispose();
    }

    if (g_system_root_fnum != 0) {
        if (!closefile(g_system_root_fnum)) {
            cli_log_warn("Failed to close system root file handle");
        }
    }

    databasedata = g_previous_database;
    g_previous_database = nil;
    g_system_root_fnum = 0;
    g_system_root_loaded = false;
    g_system_root_path[0] = '\0';
}

/* Executes a script from file or inline source and returns success status. */
static boolean execute_script_mode(void) {
    cli_log_info("Executing script mode");

    if (g_cli_options.script_file != NULL) {
        // Execute script file
        return cli_execute_script_file(g_cli_options.script_file, g_cli_options.output_json);
    } else if (g_cli_options.inline_script != NULL) {
        // Execute inline script
        return cli_execute_inline_script(g_cli_options.inline_script, g_cli_options.output_json);
    }

    return false;
}
