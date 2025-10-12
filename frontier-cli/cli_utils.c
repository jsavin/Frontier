/*
 * Frontier CLI - Command Line Interface for UserTalk Script Execution
 * CLI Utilities Implementation
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
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#include "cli_utils.h"

// Global variables
boolean g_cli_verbose = false;
boolean g_cli_debug = false;
char g_cli_error_buffer[1024] = {0};

// Logging functions
boolean cli_init_logging(boolean verbose, boolean debug) {
    g_cli_verbose = verbose;
    g_cli_debug = debug;
    
    if (debug) {
        cli_log_debug("CLI logging initialized (verbose=%s, debug=%s)", 
                     verbose ? "true" : "false", debug ? "true" : "false");
    }
    
    return true;
}

void cli_cleanup_logging(void) {
    if (g_cli_debug) {
        cli_log_debug("CLI logging cleanup");
    }
    
    g_cli_verbose = false;
    g_cli_debug = false;
}

static void cli_log_internal(cli_log_level_t level, const char* level_str, const char* format, va_list args) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[26];
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Only print if level is appropriate
    if ((level == CLI_LOG_ERROR) ||
        (level == CLI_LOG_WARN && g_cli_verbose) ||
        (level == CLI_LOG_INFO && g_cli_verbose) ||
        (level == CLI_LOG_DEBUG && g_cli_debug)) {
        
        fprintf(stderr, "[%s] [%s] ", timestamp, level_str);
        vfprintf(stderr, format, args);
        fprintf(stderr, "\n");
    }
}

void cli_log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    cli_log_internal(CLI_LOG_ERROR, "ERROR", format, args);
    va_end(args);
}

void cli_log_warn(const char* format, ...) {
    va_list args;
    va_start(args, format);
    cli_log_internal(CLI_LOG_WARN, "WARN", format, args);
    va_end(args);
}

void cli_log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    cli_log_internal(CLI_LOG_INFO, "INFO", format, args);
    va_end(args);
}

void cli_log_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    cli_log_internal(CLI_LOG_DEBUG, "DEBUG", format, args);
    va_end(args);
}

// File utility functions
boolean cli_file_exists(const char* path) {
    if (path == NULL) {
        return false;
    }
    
    struct stat st;
    return (stat(path, &st) == 0);
}

boolean cli_file_readable(const char* path) {
    if (path == NULL) {
        return false;
    }
    
    return (access(path, R_OK) == 0);
}

boolean cli_file_writable(const char* path) {
    if (path == NULL) {
        return false;
    }
    
    return (access(path, W_OK) == 0);
}

long cli_file_size(const char* path) {
    if (path == NULL) {
        return -1;
    }
    
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    
    return (long)st.st_size;
}

char* cli_read_file(const char* path, long* size) {
    if (path == NULL || size == NULL) {
        return NULL;
    }
    
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        cli_log_error("Failed to open file '%s': %s", path, strerror(errno));
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size < 0) {
        cli_log_error("Failed to get file size for '%s'", path);
        fclose(file);
        return NULL;
    }
    
    // Allocate buffer
    char* buffer = cli_malloc(file_size + 1);
    if (buffer == NULL) {
        cli_log_error("Failed to allocate memory for file '%s'", path);
        fclose(file);
        return NULL;
    }
    
    // Read file
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        cli_log_error("Failed to read file '%s': expected %ld bytes, got %zu", 
                     path, file_size, bytes_read);
        cli_free(buffer);
        return NULL;
    }
    
    buffer[file_size] = '\0';
    *size = file_size;
    
    cli_log_debug("Read file '%s': %ld bytes", path, file_size);
    return buffer;
}

boolean cli_write_file(const char* path, const char* data, long size) {
    if (path == NULL || data == NULL || size < 0) {
        return false;
    }
    
    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        cli_log_error("Failed to create file '%s': %s", path, strerror(errno));
        return false;
    }
    
    size_t bytes_written = fwrite(data, 1, size, file);
    fclose(file);
    
    if (bytes_written != (size_t)size) {
        cli_log_error("Failed to write file '%s': expected %ld bytes, wrote %zu", 
                     path, size, bytes_written);
        return false;
    }
    
    cli_log_debug("Wrote file '%s': %ld bytes", path, size);
    return true;
}

// String utility functions
char* cli_strdup(const char* str) {
    if (str == NULL) {
        return NULL;
    }
    
    size_t len = strlen(str);
    char* dup = cli_malloc(len + 1);
    if (dup == NULL) {
        return NULL;
    }
    
    strcpy(dup, str);
    return dup;
}

char* cli_strndup(const char* str, size_t n) {
    if (str == NULL) {
        return NULL;
    }
    
    size_t len = strlen(str);
    if (n < len) {
        len = n;
    }
    
    char* dup = cli_malloc(len + 1);
    if (dup == NULL) {
        return NULL;
    }
    
    strncpy(dup, str, len);
    dup[len] = '\0';
    return dup;
}

char* cli_concat_strings(const char* str1, const char* str2) {
    if (str1 == NULL && str2 == NULL) {
        return cli_strdup("");
    }
    
    if (str1 == NULL) {
        return cli_strdup(str2);
    }
    
    if (str2 == NULL) {
        return cli_strdup(str1);
    }
    
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    char* result = cli_malloc(len1 + len2 + 1);
    
    if (result == NULL) {
        return NULL;
    }
    
    strcpy(result, str1);
    strcat(result, str2);
    return result;
}

char* cli_format_string(const char* format, ...) {
    if (format == NULL) {
        return NULL;
    }
    
    va_list args;
    va_start(args, format);
    
    // First pass: determine required size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (size < 0) {
        va_end(args);
        return NULL;
    }
    
    // Allocate buffer
    char* buffer = cli_malloc(size + 1);
    if (buffer == NULL) {
        va_end(args);
        return NULL;
    }
    
    // Second pass: format the string
    vsnprintf(buffer, size + 1, format, args);
    va_end(args);
    
    return buffer;
}

// Path utility functions
char* cli_get_absolute_path(const char* path) {
    if (path == NULL) {
        return NULL;
    }
    
    char* abs_path = realpath(path, NULL);
    if (abs_path == NULL) {
        cli_log_error("Failed to get absolute path for '%s': %s", path, strerror(errno));
        return NULL;
    }
    
    char* result = cli_strdup(abs_path);
    free(abs_path);
    return result;
}

char* cli_get_directory(const char* path) {
    if (path == NULL) {
        return NULL;
    }
    
    char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        return cli_strdup(".");
    }
    
    size_t dir_len = last_slash - path;
    if (dir_len == 0) {
        return cli_strdup("/");
    }
    
    return cli_strndup(path, dir_len);
}

char* cli_get_filename(const char* path) {
    if (path == NULL) {
        return NULL;
    }
    
    char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        return cli_strdup(path);
    }
    
    return cli_strdup(last_slash + 1);
}

char* cli_get_extension(const char* path) {
    if (path == NULL) {
        return NULL;
    }
    
    char* filename = cli_get_filename(path);
    if (filename == NULL) {
        return NULL;
    }
    
    char* last_dot = strrchr(filename, '.');
    if (last_dot == NULL) {
        cli_free(filename);
        return NULL;
    }
    
    char* extension = cli_strdup(last_dot + 1);
    cli_free(filename);
    return extension;
}

boolean cli_is_absolute_path(const char* path) {
    if (path == NULL) {
        return false;
    }
    
    return (path[0] == '/');
}

// Time utility functions
char* cli_get_timestamp(void) {
    time_t now = time(NULL);
    return cli_format_time(now);
}

char* cli_format_time(time_t timestamp) {
    struct tm* tm_info = localtime(&timestamp);
    char buffer[26];
    
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return cli_strdup(buffer);
}

// Memory utility functions
void* cli_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr == NULL && size > 0) {
        cli_log_error("Memory allocation failed: %zu bytes", size);
    }
    return ptr;
}

void* cli_calloc(size_t count, size_t size) {
    void* ptr = calloc(count, size);
    if (ptr == NULL && count > 0 && size > 0) {
        cli_log_error("Memory allocation failed: %zu * %zu bytes", count, size);
    }
    return ptr;
}

void* cli_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (new_ptr == NULL && size > 0) {
        cli_log_error("Memory reallocation failed: %zu bytes", size);
    }
    return new_ptr;
}

void cli_free(void* ptr) {
    if (ptr != NULL) {
        free(ptr);
    }
}

// Error handling
void cli_set_error(const char* error) {
    if (error == NULL) {
        g_cli_error_buffer[0] = '\0';
    } else {
        strncpy(g_cli_error_buffer, error, sizeof(g_cli_error_buffer) - 1);
        g_cli_error_buffer[sizeof(g_cli_error_buffer) - 1] = '\0';
    }
}

const char* cli_get_error(void) {
    return g_cli_error_buffer;
}

void cli_clear_error(void) {
    g_cli_error_buffer[0] = '\0';
}
