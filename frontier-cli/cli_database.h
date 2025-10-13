/*
 * Frontier CLI - Command Line Interface for UserTalk Script Execution
 * CLI Database Header
 * 
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef CLI_DATABASE_H
#define CLI_DATABASE_H

#include "../Common/headers/frontier.h"
#include "../Common/headers/db.h"
#include "../Common/headers/file.h"

// Database context structure
typedef struct {
    char* database_path;        // Path to database file
    boolean flreadonly;         // Read-only flag
    hdldatabaserecord hdatabase; // Frontier database handle
    short fnumdatabase;         // Database file number
} cli_database_t;

// Database management functions
boolean cli_open_database(const char* database_path, cli_database_t* db_context);
boolean cli_close_database(cli_database_t* db_context);
boolean cli_create_database(const char* database_path);
boolean cli_migrate_database(const char* database_path);

// Database query functions
boolean cli_database_get_value(cli_database_t* db_context, const char* path, tyvaluerecord* value);
boolean cli_database_set_value(cli_database_t* db_context, const char* path, const tyvaluerecord* value);
boolean cli_database_delete_value(cli_database_t* db_context, const char* path);
boolean cli_database_has_value(cli_database_t* db_context, const char* path);

// Database utility functions
char* cli_database_backup_path(const char* database_path);
boolean cli_database_is_valid(const char* database_path);
long cli_database_size(const char* database_path);
char* cli_database_info(const char* database_path);

// Memory management functions
cli_database_t* cli_create_database_context(void);
void cli_free_database_context(cli_database_t* db_context);

#endif // CLI_DATABASE_H
