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
#include <stdint.h>

// Frontier headers
#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/memory.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/shell_api.h"
#include "../Common/headers/db.h"
#include "../Common/headers/file.h"
#include "../Common/headers/tableverbs.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/stringdefs.h"
#include "../Common/headers/db_format.h"
#include "../Common/headers/dbinternal.h"
#include "../Common/headers/byteorder.h"

// CLI-specific headers
#include "cli_parser.h"
#include "cli_executor.h"
#include "cli_utils.h"

extern long grabthreadglobals(void);
extern long releasethreadglobals(void);

// Version information
#define FRONTIER_CLI_VERSION "1.0.0"
#define FRONTIER_CLI_BUILD_DATE __DATE__

// Global variables
static cli_options_t g_cli_options = {0};
static boolean g_initialized = false;
static boolean g_system_root_loaded = false;
static hdlfilenum g_system_root_fnum = 0;
static hdldatabaserecord g_previous_database = nil;
static char g_system_root_path[CLI_MAX_PATH_LENGTH + 1] = {0};

// Function prototypes
static void print_usage(const char* program_name);
static void print_version(void);
static boolean initialize_frontier_runtime(void);
static void cleanup_frontier_runtime(void);
static boolean execute_script_mode(void);
static boolean execute_database_mode(void);
static boolean execute_network_mode(void);
static boolean load_system_root_database(const char* path);
static void unload_system_root_database(void);
static boolean ensure_named_subtable(hdlhashtable parent, const unsigned char *name, hdlhashtable *out, boolean mark_dont_save);
static boolean hydrate_system_root_database(const char* path);
static boolean read_root_table_address(const char *path, dbaddress *adr_out, short *version_out);

int main(int argc, char* argv[]) {
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

    if (g_cli_options.upgrade_system_root) {
        boolean migrated = false;
        char output_path[1024];
        if (!ensure_database_modern(g_cli_options.system_root, &migrated, output_path, sizeof output_path)) {
            fprintf(stderr, "Error: Failed to upgrade system root: %s\n", g_cli_options.system_root);
            return 1;
        }
        if (migrated) {
            printf("System root upgraded to v7 format (written to): %s\n", output_path);
        } else {
            printf("System root already in modern format: %s\n", g_cli_options.system_root);
        }
        return 0;
    }

    if (g_cli_options.hydrate_system_root) {
        if (g_cli_options.system_root == NULL) {
            fprintf(stderr, "Error: --hydrate-system-root requires --system-root PATH\n");
            return 1;
        }
        const char* hydrate_path = g_cli_options.system_root;
        g_cli_options.system_root = NULL;
        if (!initialize_frontier_runtime()) {
            fprintf(stderr, "Error: Failed to initialize runtime for hydration\n");
            return 1;
        }
        boolean hydrate_ok = hydrate_system_root_database(hydrate_path);
        cleanup_frontier_runtime();
        if (!hydrate_ok) {
            fprintf(stderr, "Error: Failed to hydrate system root: %s\n", hydrate_path);
            return 1;
        }
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
    }
    
    // Cleanup
    cleanup_frontier_runtime();
    
    return success ? 0 : 1;
}

static uint64_t read_big_endian(const unsigned char *data, size_t length) {
    uint64_t value = 0;
    for (size_t i = 0; i < length; ++i) {
        value = (value << 8) | (uint64_t) data[i];
    }
    return value;
}

static boolean read_root_table_address(const char *path, dbaddress *adr_out, short *version_out) {
    if (path == NULL || adr_out == NULL)
        return false;

    unsigned char header[0x40];
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return false;

    size_t bytes = fread(header, 1, sizeof header, fp);
    fclose(fp);
    if (bytes < 0x1E) /* need at least version + view slots */
        return false;

    short version = (short) header[1];
    if (version_out != NULL)
        *version_out = version;
    cli_log_debug("raw header version=%d", version);

    const size_t view_stride = 8; /* views are stored with 8-byte slots in both formats */
    const size_t view_base = 0x0E;
    dbaddress found = nildbaddress;
    int found_index = -1;

    if (version >= 7) {
        for (int i = 0; i < ctviews; ++i) {
            size_t offset = view_base + (size_t)i * view_stride;
            if (bytes < offset + view_stride)
                break;
            uint64_t raw = read_big_endian(header + offset, 8);
            if (raw != 0) {
                found = (dbaddress) raw;
                found_index = i;
                break;
            }
        }
    } else {
        for (int i = 0; i < ctviews; ++i) {
            size_t offset = view_base + (size_t)i * view_stride;
            if (bytes < offset + 6) /* legacy stores significant bytes in the first six positions */
                break;
            uint64_t raw = read_big_endian(header + offset, 6);
            if (raw != 0) {
                found = (dbaddress) raw;
                found_index = i;
                break;
            }
        }
    }

    if (found == nildbaddress)
        return false;

    cli_log_debug("Selected view[%d] pointer = 0x%08llx", found_index, (unsigned long long)found);
    *adr_out = found;
    return true;
}

static void print_usage(const char* program_name) {
    printf("Frontier CLI - Command Line Interface for UserTalk Script Execution\n");
    printf("Version %s (%s)\n\n", FRONTIER_CLI_VERSION, FRONTIER_CLI_BUILD_DATE);
    
    printf("Usage: %s [OPTIONS] [SCRIPT_FILE]\n\n", program_name);
    
    printf("Execution Modes:\n");
    printf("  Script Execution:\n");
    printf("    %s script.usertalk                    # Execute UserTalk script file\n", program_name);
    printf("    %s -e \"3 + 4\"                         # Execute inline script\n", program_name);
    printf("\n");
    printf("  (Database and server modes will return an error until Phase 3 work completes.)\n");
    printf("\n");
    
    printf("Options:\n");
    printf("  -e, --execute SCRIPT     Execute inline UserTalk script\n");
    printf("  -d, --database FILE      (disabled)\n");
    printf("  -q, --query QUERY        (disabled)\n");
    printf("  -m, --migrate            (disabled)\n");
    printf("  --server                 (disabled)\n");
    printf("  --websocket              (disabled)\n");
    printf("  -p, --port PORT          (disabled)\n");
    printf("  --system-root PATH       Load system root database before executing scripts\n");
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
    printf("  # Execute a script file\n");
    printf("  %s myscript.usertalk\n", program_name);
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
    
    // Install headless shell adapter
    shell_api_use_headless();

    // Initialize core subsystems (mirrors tests/runtime_tests bring-up)
    if (!initmemory()) {
        fprintf(stderr, "Error: initmemory failed\n");
        return false;
    }

    initstrings();

    if (!initlang()) {
        fprintf(stderr, "Error: initlang failed\n");
        return false;
    }

    if (!inittablestructure()) {
        fprintf(stderr, "Error: inittablestructure failed\n");
        return false;
    }

    if (!langinitverbs()) {
        fprintf(stderr, "Error: langinitverbs failed\n");
        return false;
    }

    grabthreadglobals();

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

static void cleanup_frontier_runtime(void) {
    if (!g_initialized) {
        return;
    }
    
    cli_log_info("Cleaning up Frontier runtime");

    if (g_system_root_loaded) {
        unload_system_root_database();
    }
    
    // Cleanup Frontier runtime
    releasethreadglobals();
    
    // Cleanup CLI components
    cli_cleanup_logging();
    
    g_initialized = false;
}

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

static boolean hydrate_system_root_database(const char* path) {
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
    if (!ensure_database_modern(path, &migrated, actual_path, sizeof actual_path)) {
        cli_log_error("Failed to verify database format before hydration: %s", path);
        return false;
    }
    if (migrated) {
        cli_log_info("Migrated legacy system root to v7 format (written to): %s", actual_path);
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

    if (!openfile(&fs, &fnum, false)) {
        cli_log_error("Unable to open system root for hydration: %s", path);
        return false;
    }
    file_open = true;

    hdldatabaserecord previous = databasedata;

    if (!dbopenfile(fnum, false)) {
        cli_log_error("dbopenfile (read-write) failed for system root: %s", path);
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
    if (!tableloadsystemtable(adr, &hrootvariable, &hroot, false)) {
        cli_log_error("Failed to load system table while hydrating %s", path);
        goto cleanup;
    }

    dispose_rootvariable = true;

    cleartablestructureglobals();
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

    if (!tablesavesystemtable(hrootvariable, &adr)) {
        cli_log_error("Failed to save system table while hydrating %s", path);
        goto cleanup;
    }

    dbsetview(cancoonview, adr);
    cli_log_info("Hydrated system root: %s%s", path,
                 created_optional ? " (created optional tables)" : "");
    ok = true;

cleanup:
    if (db_open) {
        if (!dbclose()) {
            cli_log_warn("dbclose reported failure while hydrating %s", path);
            ok = false;
        }
    }

    if (dispose_rootvariable && hrootvariable != nil)
        disposehandle(hrootvariable);

    if (db_open)
        dbdispose();

    databasedata = previous;
    cleartablestructureglobals();
    currenthashtable = nil;

    if (file_open && !closefile(fnum)) {
        cli_log_warn("Failed to close hydrated system root file handle: %s", path);
        ok = false;
    }

    return ok;
}

static boolean load_system_root_database(const char* path) {
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
    if (!ensure_database_modern(path, &migrated, actual_path, sizeof actual_path)) {
        cli_log_error("Failed to ensure system root is modern: %s", path);
        return false;
    }
    if (migrated) {
        cli_log_info("Migrated legacy system root to v7 format (written to): %s", actual_path);
        path = actual_path;  /* Use the v7 file */
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

    if (!settablestructureglobals(hrootvariable, true)) {
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
        structure_ready = false;

        hdlhashtable captured_system = systemtable;
        hdlhashtable captured_verbs = verbstable;
        hdlhashtable captured_builtins = builtinstable;
        hdlhashtable captured_agents = agentstable;
        hdlhashtable captured_paths = pathstable;
        hdlhashtable captured_resources = resourcestable;
        hdlhashtable captured_menubar = menubartable;
        hdlhashtable captured_objectmodel = objectmodeltable;

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

        if (!structure_ready) {
            cleartablestructureglobals();
            rootvariable = hrootvariable;
            roottable = hroot;
            systemtable = captured_system;
            verbstable = captured_verbs;
            builtinstable = captured_builtins;
            agentstable = captured_agents;
            pathstable = captured_paths;
            resourcestable = captured_resources;
            menubartable = captured_menubar;
            objectmodeltable = captured_objectmodel;

            if (systemtable != nil && verbstable != nil) {
                structure_ready = true;
                partial_warning = true;
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

    g_previous_database = previous;
    g_system_root_fnum = fnum;
    g_system_root_loaded = true;
    snprintf(g_system_root_path, sizeof(g_system_root_path), "%s", path);

    cli_log_info("Loaded system root database: %s", g_system_root_path);
    return true;
}

static void unload_system_root_database(void) {
    if (!g_system_root_loaded) {
        return;
    }

    const char* path = (g_system_root_path[0] != '\0') ? g_system_root_path : "(unknown)";
    cli_log_info("Unloading system root database: %s", path);

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
    (void)g_cli_options;
    fprintf(stderr, "Error: Database operations are not yet available in the headless CLI build.\n");
    return false;
}

static boolean execute_network_mode(void) {
    (void)g_cli_options;
    fprintf(stderr, "Error: Network server modes are not yet available in the headless CLI build.\n");
    return false;
}
