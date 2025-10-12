/*
 * Frontier CLI - Command Line Interface for UserTalk Script Execution
 * CLI Utilities Header
 * 
 * Copyright (C) 1992-2004 UserLand Software, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef CLI_UTILS_H
#define CLI_UTILS_H

#include "../Common/SystemHeaders/standard.h"
#include "../Common/headers/strings.h"

// Logging levels
#define CLI_LOG_ERROR 0
#define CLI_LOG_WARN  1
#define CLI_LOG_INFO  2
#define CLI_LOG_DEBUG 3

// Logging functions
void cli_log_error(const char* message, ...);
void cli_log_warn(const char* message, ...);
void cli_log_info(const char* message, ...);
void cli_log_debug(const char* message, ...);
void cli_log_internal(int level, const char* message, va_list args);

// Logging management
boolean cli_init_logging(boolean verbose, boolean debug);
void cli_cleanup_logging(void);
void cli_set_log_level(int level);

// File operations
boolean cli_file_exists(const char* path);
boolean cli_file_readable(const char* path);
boolean cli_file_writable(const char* path);
long cli_file_size(const char* path);
char* cli_read_file(const char* path, long* size);
boolean cli_write_file(const char* path, const char* content, long size);

// String operations
char* cli_strdup(const char* str);
char* cli_concat_strings(const char* str1, const char* str2);
char* cli_format_string(const char* format, ...);
void cli_free_string(char* str);

// Path operations
char* cli_absolute_path(const char* path);
char* cli_directory_name(const char* path);
char* cli_file_name(const char* path);
char* cli_file_extension(const char* path);

// Time operations
char* cli_format_timestamp(time_t timestamp);
char* cli_current_timestamp(void);

// Memory management
void* cli_malloc(size_t size);
void* cli_calloc(size_t count, size_t size);
void* cli_realloc(void* ptr, size_t size);
void cli_free(void* ptr);

// Error handling
extern char g_cli_error_buffer[1024];
const char* cli_get_error(void);
void cli_set_error(const char* error);
void cli_clear_error(void);

#endif // CLI_UTILS_H
