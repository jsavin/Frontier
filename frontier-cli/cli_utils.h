#ifndef CLI_UTILS_H
#define CLI_UTILS_H

#include "../Common/headers/frontier.h"
#include <stdarg.h>
#include <stddef.h>

#ifndef boolean
typedef unsigned char boolean;
#endif

#define CLI_LOG_ERROR 0
#define CLI_LOG_WARN  1
#define CLI_LOG_INFO  2
#define CLI_LOG_DEBUG 3

// Logging
boolean cli_init_logging(boolean verbose, boolean debug);
void cli_set_json_mode(boolean json_mode);
void cli_cleanup_logging(void);
void cli_log_error(const char* format, ...);
void cli_log_warn(const char* format, ...);
void cli_log_info(const char* format, ...);
void cli_log_debug(const char* format, ...);

// File helpers
boolean cli_file_exists(const char* path);
boolean cli_file_readable(const char* path);
boolean cli_file_writable(const char* path);
long cli_file_size(const char* path);
char* cli_read_file(const char* path, long* size);
boolean cli_write_file(const char* path, const char* data, long size);

// String helpers
char* cli_strdup(const char* str);
char* cli_concat_strings(const char* str1, const char* str2);
char* cli_format_string(const char* format, ...);
void cli_free_string(char* str);

// Memory helpers
void* cli_malloc(size_t size);
void* cli_calloc(size_t count, size_t size);
void* cli_realloc(void* ptr, size_t size);
void cli_free(void* ptr);

// Error buffer
extern char g_cli_error_buffer[1024];
const char* cli_get_error(void);
void cli_set_error(const char* error);
void cli_clear_error(void);

#endif /* CLI_UTILS_H */
