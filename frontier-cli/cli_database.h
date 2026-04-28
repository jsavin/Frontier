/*
    Frontier CLI - Command Line Interface for UserTalk Script Execution
    CLI Database Header

    cli_database.h - Declarations for ODB database management functions

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

#ifndef CLI_DATABASE_H
#define CLI_DATABASE_H

#include "../Common/headers/frontier.h"
#include "../Common/headers/db.h"
#include "../Common/headers/file.h"

// Database context structure
typedef struct {
	char* database_path;		// Path to database file
	boolean flreadonly;			// Read-only flag
	hdldatabaserecord hdatabase; // Frontier database handle
	short fnumdatabase;			// Database file number
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
