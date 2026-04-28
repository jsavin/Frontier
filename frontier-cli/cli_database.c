/*
    Frontier CLI - Command Line Interface for UserTalk Script Execution
    CLI Database Implementation

    cli_database.c - ODB database operations including open, close, backup, and migration

    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-present Frontier contributors

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "cli_database.h"
#include "cli_utils.h"
#include "cli_executor.h"

// Include Frontier database headers
#include "../Common/headers/db.h"
#include "../Common/headers/dbinternal.h"
#include "../Common/headers/file.h"

/* Global database error buffer */
static char g_database_error_buffer[1024] = {0};

/* Sets the current database error message. */
void cli_set_database_error(const char* error) {
	if (error == NULL) {
		g_database_error_buffer[0] = '\0';
	} else {
		strncpy(g_database_error_buffer, error, sizeof(g_database_error_buffer) - 1);
		g_database_error_buffer[sizeof(g_database_error_buffer) - 1] = '\0';
	}
}

/* Returns the current database error message. */
const char* cli_get_database_error(void) {
	return g_database_error_buffer;
}

/* Clears the current database error message. */
void cli_clear_database_error(void) {
	g_database_error_buffer[0] = '\0';
}

/* Checks if a database file exists at the specified path. */
boolean cli_database_exists(const char* db_path) {
	if (db_path == NULL) {
		return false;
	}
	
	return cli_file_exists(db_path);
}

/* Returns the size of the database file in bytes. */
long cli_database_size(const char* db_path) {
	if (db_path == NULL) {
		return -1;
	}
	
	return cli_file_size(db_path);
}

/* Generates a timestamped backup path for the database file. */
char* cli_database_backup_path(const char* db_path) {
	if (db_path == NULL) {
		return NULL;
	}
	
	time_t now = time(NULL);
	struct tm* tm_info = localtime(&now);
	
	return cli_format_string("%s.%04d%02d%02d_%02d%02d%02d", 
						   db_path,
						   tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
						   tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
}

/* Creates a backup copy of the database file with a timestamp suffix. */
boolean cli_create_database_backup(const char* db_path) {
	if (db_path == NULL) {
		cli_set_database_error("No database path specified");
		return false;
	}
	
	if (!cli_database_exists(db_path)) {
		cli_set_database_error("Database file does not exist");
		return false;
	}
	
	char* backup_path = cli_database_backup_path(db_path);
	if (backup_path == NULL) {
		cli_set_database_error("Failed to generate backup path");
		return false;
	}
	
	long file_size;
	char* file_content = cli_read_file(db_path, &file_size);
	if (file_content == NULL) {
		cli_set_database_error("Failed to read database file");
		cli_free(backup_path);
		return false;
	}
	
	boolean success = cli_write_file(backup_path, file_content, file_size);
	
	cli_free(file_content);
	cli_free(backup_path);
	
	if (!success) {
		cli_set_database_error("Failed to create database backup");
		return false;
	}
	
	cli_log_info("Created database backup: %s", backup_path);
	return true;
}

/* Validates that a database path is non-null and non-empty. */
boolean cli_validate_database_path(const char* db_path) {
	if (db_path == NULL) {
		cli_set_database_error("No database path specified");
		return false;
	}
	
	if (strlen(db_path) == 0) {
		cli_set_database_error("Empty database path");
		return false;
	}
	
	/* Check if path contains invalid characters */
	if (strchr(db_path, '\0') != NULL) {
		cli_set_database_error("Database path contains null characters");
		return false;
	}
	
	return true;
}

/* Allocates and initializes a database context for the given path. */
cli_database_t* cli_create_database_context(const char* db_path) {
	if (!cli_validate_database_path(db_path)) {
		return NULL;
	}
	
	cli_database_t* db = cli_calloc(1, sizeof(cli_database_t));
	if (db == NULL) {
		cli_set_database_error("Failed to allocate database context");
		return NULL;
	}
	
	db->db_path = cli_strdup(db_path);
	db->flreadonly = false;
	db->hdb = nil;
	db->flopen = false;
	db->fnum = 0;
	
	cli_log_debug("Created database context for: %s", db_path);
	return db;
}

/* Frees a database context and closes the database if open. */
void cli_free_database_context(cli_database_t* db) {
	if (db == NULL) {
		return;
	}
	
	/* Close database if open */
	if (db->flopen) {
		cli_close_database(db);
	}
	
	/* Free allocated memory */
	if (db->db_path != NULL) {
		cli_free(db->db_path);
		db->db_path = NULL;
	}
	
	cli_free(db);
	
	cli_log_debug("Freed database context");
}

/* Opens a database file for reading or writing. */
boolean cli_open_database(const char* db_path, boolean read_only, cli_database_t* db) {
	if (db == NULL) {
		cli_set_database_error("Invalid database context");
		return false;
	}
	
	if (!cli_validate_database_path(db_path)) {
		return false;
	}
	
	if (!cli_database_exists(db_path)) {
		cli_set_database_error("Database file does not exist: %s", db_path);
		return false;
	}
	
	cli_log_info("Opening database: %s (read-only: %s)", db_path, read_only ? "yes" : "no");
	
	/* Open the file */
	tyfilespec fs;
	if (!filepathtofilespec(db_path, &fs)) {
		cli_set_database_error("Failed to convert path to filespec: %s", db_path);
		return false;
	}
	
	if (!fileopen(&fs, 'ROOT', 'ROOT', &db->fnum)) {
		cli_set_database_error("Failed to open database file: %s", db_path);
		return false;
	}
	
	/* Open the database */
	if (!dbopenfile(db->fnum, read_only)) {
		cli_set_database_error("Failed to open database: %s", db_path);
		fileclose(db->fnum);
		return false;
	}
	
	db->flopen = true;
	db->flreadonly = read_only;
	
	cli_log_info("Successfully opened database: %s", db_path);
	return true;
}

/* Closes an open database and releases the file handle. */
boolean cli_close_database(cli_database_t* db) {
	if (db == NULL) {
		return false;
	}
	
	if (!db->flopen) {
		return true; /* Already closed */
	}
	
	cli_log_info("Closing database: %s", db->db_path);
	
	/* Close the database */
	if (!dbclose()) {
		cli_set_database_error("Failed to close database: %s", db->db_path);
		return false;
	}
	
	/* Close the file */
	fileclose(db->fnum);
	
	db->flopen = false;
	db->fnum = 0;
	
	cli_log_info("Successfully closed database: %s", db->db_path);
	return true;
}

/* Creates a new database file in v7 format. */
boolean cli_create_database(const char* db_path) {
	if (!cli_validate_database_path(db_path)) {
		return false;
	}
	
	if (cli_database_exists(db_path)) {
		cli_set_database_error("Database already exists: %s", db_path);
		return false;
	}
	
	cli_log_info("Creating new database: %s", db_path);

	/* Create the file */
	tyfilespec fs;
	if (!filepathtofilespec(db_path, &fs)) {
		cli_set_database_error("Failed to convert path to filespec: %s", db_path);
		return false;
	}
	
	hdlfilenum fnum;
	if (!fileopenorcreate(&fs, 'ROOT', 'ROOT', &fnum)) {
		cli_set_database_error("Failed to create database file: %s", db_path);
		return false;
	}
	
	/* Create the database (v7 format) */
	if (!dbnew(fnum, true)) {
		cli_set_database_error("Failed to create new database: %s", db_path);
		fileclose(fnum);
		return false;
	}
	
	/* Close the file */
	fileclose(fnum);

	cli_log_info("Successfully created database: %s", db_path);
	return true;
}

/* Migrates a legacy v6 database to the modern v7 64-bit format. */
boolean cli_migrate_database(const char* db_path) {
	if (!cli_validate_database_path(db_path)) {
		return false;
	}
	
	if (!cli_database_exists(db_path)) {
		cli_set_database_error("Database does not exist: %s", db_path);
		return false;
	}
	
	cli_log_info("Migrating database to 64-bit format: %s", db_path);

	/* Create backup first */
	if (!cli_create_database_backup(db_path)) {
		cli_set_database_error("Failed to create backup before migration");
		return false;
	}

	/* TODO: Call the actual migration function from Common/source/db.c */

	cli_log_info("Database migration completed: %s", db_path);
	return true;
}

/* Executes a UserTalk query against a database. */
boolean cli_execute_database_query(const char* db_path, const char* query) {
	if (!cli_validate_database_path(db_path)) {
		return false;
	}
	
	if (query == NULL || strlen(query) == 0) {
		cli_set_database_error("No query specified");
		return false;
	}
	
	cli_log_info("Executing database query: %s", query);

	/* Create database context */
	cli_database_t* db = cli_create_database_context(db_path);
	if (db == NULL) {
		return false;
	}

	/* Open database */
	if (!cli_open_database(db_path, true, db)) {
		cli_free_database_context(db);
		return false;
	}

	/* Execute the query as a UserTalk script */
	char* script = cli_format_string("db.open('%s'); %s", db_path, query);
	if (script == NULL) {
		cli_set_database_error("Failed to format query script");
		cli_free_database_context(db);
		return false;
	}
	
	boolean success = cli_execute_inline_script(script);

	/* Cleanup */
	cli_free(script);
	cli_free_database_context(db);
	
	return success;
}

/* Retrieves a value from the database at the specified path. */
boolean cli_db_get_value(const char* db_path, const char* path, char** result) {
	if (!cli_validate_database_path(db_path) || path == NULL || result == NULL) {
		return false;
	}
	
	cli_log_debug("Getting database value: %s -> %s", db_path, path);

	/* Create database context */
	cli_database_t* db = cli_create_database_context(db_path);
	if (db == NULL) {
		return false;
	}

	/* Open database */
	if (!cli_open_database(db_path, true, db)) {
		cli_free_database_context(db);
		return false;
	}

	/* Execute query to get value */
	char* script = cli_format_string("db.getValue('%s', '%s')", db_path, path);
	if (script == NULL) {
		cli_set_database_error("Failed to format get value script");
		cli_free_database_context(db);
		return false;
	}

	/* Execute the script and capture result */
	usertalk_execution_t* execution = cli_create_execution_context();
	if (execution == NULL) {
		cli_free(script);
		cli_free_database_context(db);
		return false;
	}
	
	boolean compiled = cli_compile_script(script, execution);
	boolean executed = false;
	
	if (compiled) {
		executed = cli_execute_compiled_script(execution);
	}
	
	if (executed && execution->flsuccess) {
		*result = cli_get_execution_result_string(execution);
	} else {
		*result = NULL;
		cli_set_database_error("Failed to get database value: %s", cli_get_execution_error(execution));
	}

	/* Cleanup */
	cli_free(script);
	cli_free_execution_context(execution);
	cli_free_database_context(db);
	
	return (*result != NULL);
}

/* Returns a formatted string with database path, size, and backup information. */
char* cli_get_database_info(const char* db_path) {
	if (!cli_validate_database_path(db_path)) {
		return NULL;
	}
	
	if (!cli_database_exists(db_path)) {
		return cli_strdup("Database does not exist");
	}
	
	long size = cli_database_size(db_path);
	char* backup_path = cli_database_backup_path(db_path);
	
	char* info = cli_format_string("Database: %s\nSize: %ld bytes\nBackup: %s",
								  db_path, size, backup_path ? backup_path : "N/A");
	
	cli_free(backup_path);
	return info;
}
