#include "cli_utils.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../Common/headers/logging.h"

static boolean g_cli_verbose = false;
static boolean g_cli_debug = false;

char g_cli_error_buffer[1024] = {0};

static void cli_vlog(int level, const char* label, const char* format, va_list args) {
    /* Use structured logging instead of fprintf(stderr) */
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), format, args);

    switch (level) {
        case CLI_LOG_ERROR:
            log_error(LOG_COMP_GENERAL, "%s", buffer);
            break;
        case CLI_LOG_WARN:
            log_warn(LOG_COMP_GENERAL, "%s", buffer);
            break;
        case CLI_LOG_INFO:
            if (g_cli_verbose) {
                log_info(LOG_COMP_GENERAL, "%s", buffer);
            }
            break;
        case CLI_LOG_DEBUG:
            if (g_cli_debug) {
                log_debug(LOG_COMP_GENERAL, "%s", buffer);
            }
            break;
    }
}

boolean cli_init_logging(boolean verbose, boolean debug) {
    g_cli_verbose = verbose;
    g_cli_debug = debug;
    return true;
}

void cli_cleanup_logging(void) {
    g_cli_verbose = false;
    g_cli_debug = false;
}

void cli_log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    cli_vlog(CLI_LOG_ERROR, "ERROR", format, args);
    va_end(args);
}

void cli_log_warn(const char* format, ...) {
    va_list args;
    va_start(args, format);
    cli_vlog(CLI_LOG_WARN, "WARN", format, args);
    va_end(args);
}

void cli_log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    cli_vlog(CLI_LOG_INFO, "INFO", format, args);
    va_end(args);
}

void cli_log_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    cli_vlog(CLI_LOG_DEBUG, "DEBUG", format, args);
    va_end(args);
}

boolean cli_file_exists(const char* path) {
    if (path == NULL) {
        return false;
    }
    struct stat st;
    return stat(path, &st) == 0;
}

boolean cli_file_readable(const char* path) {
    if (path == NULL) {
        return false;
    }
    return access(path, R_OK) == 0;
}

boolean cli_file_writable(const char* path) {
    if (path == NULL) {
        return false;
    }
    return access(path, W_OK) == 0;
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

    if (fseek(file, 0, SEEK_END) != 0) {
        cli_log_error("Failed to seek file '%s'", path);
        fclose(file);
        return NULL;
    }

    long length = ftell(file);
    if (length < 0) {
        cli_log_error("Failed to determine size for '%s'", path);
        fclose(file);
        return NULL;
    }
    rewind(file);

    char* buffer = cli_malloc((size_t)length + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)length, file);
    fclose(file);

    if (read != (size_t)length) {
        cli_log_error("Failed to read file '%s': expected %ld bytes, got %zu", path, length, read);
        cli_free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    *size = length;
    return buffer;
}

boolean cli_write_file(const char* path, const char* data, long size) {
    if (path == NULL || data == NULL || size < 0) {
        return false;
    }

    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        cli_log_error("Failed to open file '%s' for writing: %s", path, strerror(errno));
        return false;
    }

    size_t written = fwrite(data, 1, (size_t)size, file);
    fclose(file);
    return written == (size_t)size;
}

char* cli_strdup(const char* str) {
    if (str == NULL) {
        return NULL;
    }
    size_t len = strlen(str);
    char* copy = cli_malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, str, len + 1);
    return copy;
}

char* cli_concat_strings(const char* str1, const char* str2) {
    if (str1 == NULL || str2 == NULL) {
        return NULL;
    }
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    char* result = cli_malloc(len1 + len2 + 1);
    if (result == NULL) {
        return NULL;
    }
    memcpy(result, str1, len1);
    memcpy(result + len1, str2, len2 + 1);
    return result;
}

char* cli_format_string(const char* format, ...) {
    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if (needed < 0) {
        va_end(args);
        return NULL;
    }

    char* buffer = cli_malloc((size_t)needed + 1);
    if (buffer == NULL) {
        va_end(args);
        return NULL;
    }

    vsnprintf(buffer, (size_t)needed + 1, format, args);
    va_end(args);
    return buffer;
}

void cli_free_string(char* str) {
    cli_free(str);
}

void* cli_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr == NULL && size > 0) {
        cli_log_error("Memory allocation failed (%zu bytes)", size);
    }
    return ptr;
}

void* cli_calloc(size_t count, size_t size) {
    void* ptr = calloc(count, size);
    if (ptr == NULL && count > 0 && size > 0) {
        cli_log_error("Memory allocation failed (%zu x %zu bytes)", count, size);
    }
    return ptr;
}

void* cli_realloc(void* ptr, size_t size) {
    void* result = realloc(ptr, size);
    if (result == NULL && size > 0) {
        cli_log_error("Memory reallocation failed (%zu bytes)", size);
    }
    return result;
}

void cli_free(void* ptr) {
    free(ptr);
}

const char* cli_get_error(void) {
    return g_cli_error_buffer;
}

void cli_set_error(const char* error) {
    if (error == NULL) {
        g_cli_error_buffer[0] = '\0';
        return;
    }
    strncpy(g_cli_error_buffer, error, sizeof(g_cli_error_buffer) - 1);
    g_cli_error_buffer[sizeof(g_cli_error_buffer) - 1] = '\0';
}

void cli_clear_error(void) {
    g_cli_error_buffer[0] = '\0';
}
