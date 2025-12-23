#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/*
 * Thread Safety: Currently NOT thread-safe. Global state is accessed without locking.
 * This is acceptable as Frontier headless runtime is single-threaded.
 * If threading is added, protect g_log_level and g_component_enabled[] with mutex.
 */

// ============================================================================
// Constants
// ============================================================================

#define LOG_MESSAGE_MAX 4096
#define LOG_ESCAPED_MAX (LOG_MESSAGE_MAX * 2)  // Worst case: every char escaped

// ============================================================================
// Internal State
// ============================================================================

typedef enum {
    LOG_FORMAT_TEXT = 0,
    LOG_FORMAT_JSON = 1
} log_format_t;

static log_level_t g_log_level = LOG_LEVEL_WARN;  // Default: warnings and errors
static bool g_component_enabled[LOG_COMP_COUNT];  // Per-component enable flags
static bool g_initialized = false;
static log_format_t g_log_format = LOG_FORMAT_TEXT;  // Default: plain text

// Component names (for display and parsing)
static const char *component_names[] = {
    [LOG_COMP_DB]       = "db",
    [LOG_COMP_HASH]     = "hash",
    [LOG_COMP_TABLE]    = "table",
    [LOG_COMP_PACK]     = "pack",
    [LOG_COMP_PARSE]    = "parse",
    [LOG_COMP_EVAL]     = "eval",
    [LOG_COMP_OP]       = "op",
    [LOG_COMP_LANG]     = "lang",
    [LOG_COMP_EXTERNAL] = "external",
    [LOG_COMP_STARTUP]  = "startup",
    [LOG_COMP_GENERAL]  = "general"
};

// Level names (for display and parsing)
static const char *level_names[] = {
    [LOG_LEVEL_ERROR] = "ERROR",
    [LOG_LEVEL_WARN]  = "WARN",
    [LOG_LEVEL_INFO]  = "INFO",
    [LOG_LEVEL_DEBUG] = "DEBUG",
    [LOG_LEVEL_TRACE] = "TRACE"
};

// ============================================================================
// Internal Helpers
// ============================================================================

/**
 * Parse log level from string (case-insensitive).
 * Returns LOG_LEVEL_WARN if not recognized.
 */
static log_level_t parse_log_level(const char *str) {
    if (!str) return LOG_LEVEL_WARN;

    if (strcasecmp(str, "error") == 0) return LOG_LEVEL_ERROR;
    if (strcasecmp(str, "warn") == 0)  return LOG_LEVEL_WARN;
    if (strcasecmp(str, "info") == 0)  return LOG_LEVEL_INFO;
    if (strcasecmp(str, "debug") == 0) return LOG_LEVEL_DEBUG;
    if (strcasecmp(str, "trace") == 0) return LOG_LEVEL_TRACE;

    fprintf(stderr, "[LOG] Warning: Unknown log level '%s', defaulting to 'warn'\n", str);
    return LOG_LEVEL_WARN;
}

/**
 * Parse log format from string (case-insensitive).
 * Returns LOG_FORMAT_TEXT if not recognized.
 */
static log_format_t parse_log_format(const char *str) {
    if (!str) return LOG_FORMAT_TEXT;

    if (strcasecmp(str, "json") == 0) return LOG_FORMAT_JSON;
    if (strcasecmp(str, "text") == 0) return LOG_FORMAT_TEXT;

    fprintf(stderr, "[LOG] Warning: Unknown log format '%s', defaulting to 'text'\n", str);
    return LOG_FORMAT_TEXT;
}

/**
 * Parse component name from string.
 * Returns -1 if not recognized.
 */
static int parse_component(const char *str) {
    for (int i = 0; i < LOG_COMP_COUNT; i++) {
        if (strcasecmp(str, component_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Parse comma-separated component list and enable them.
 * Special case: "all" or "*" enables all components.
 */
static void parse_components(const char *str) {
    if (!str) {
        // Default: enable all components
        for (int i = 0; i < LOG_COMP_COUNT; i++) {
            g_component_enabled[i] = true;
        }
        return;
    }

    // Check for "all" or "*"
    if (strcasecmp(str, "all") == 0 || strcmp(str, "*") == 0) {
        for (int i = 0; i < LOG_COMP_COUNT; i++) {
            g_component_enabled[i] = true;
        }
        return;
    }

    // Start with all disabled
    for (int i = 0; i < LOG_COMP_COUNT; i++) {
        g_component_enabled[i] = false;
    }

    // Parse comma-separated list
    char *str_copy = strdup(str);
    if (!str_copy) {
        fprintf(stderr, "[LOG] Warning: Memory allocation failed, enabling all components\n");
        for (int i = 0; i < LOG_COMP_COUNT; i++) {
            g_component_enabled[i] = true;
        }
        return;
    }

    char *token = strtok(str_copy, ",");
    while (token) {
        // Trim whitespace
        while (*token == ' ' || *token == '\t') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }

        int comp = parse_component(token);
        if (comp >= 0) {
            g_component_enabled[comp] = true;
        } else {
            fprintf(stderr, "[LOG] Warning: Unknown component '%s', ignoring\n", token);
        }

        token = strtok(NULL, ",");
    }
    free(str_copy);
}

// ============================================================================
// Public API Implementation
// ============================================================================

void log_init(void) {
    if (g_initialized) return;

    // Parse FRONTIER_LOG_LEVEL
    const char *level_str = getenv("FRONTIER_LOG_LEVEL");
    g_log_level = parse_log_level(level_str);

    // Parse FRONTIER_LOG_COMPONENT
    const char *comp_str = getenv("FRONTIER_LOG_COMPONENT");
    parse_components(comp_str);

    // Parse FRONTIER_LOG_FORMAT
    const char *format_str = getenv("FRONTIER_LOG_FORMAT");
    g_log_format = parse_log_format(format_str);

    g_initialized = true;

    // Optional: log initialization message (only if INFO or above)
    if (g_log_level >= LOG_LEVEL_INFO && g_component_enabled[LOG_COMP_STARTUP]) {
        fprintf(stderr, "[STARTUP-INFO] Logging initialized: level=%s components=%s format=%s\n",
                level_names[g_log_level],
                comp_str ? comp_str : "all",
                g_log_format == LOG_FORMAT_JSON ? "json" : "text");
    }
}

void log_set_level(log_level_t level) {
    if (!g_initialized) log_init();
    g_log_level = level;
}

void log_set_component_enabled(log_component_t component, bool enabled) {
    if (!g_initialized) log_init();
    if (component >= 0 && component < LOG_COMP_COUNT) {
        g_component_enabled[component] = enabled;
    }
}

bool log_is_enabled(log_level_t level, log_component_t component) {
    if (!g_initialized) log_init();

    // Errors are always enabled
    if (level == LOG_LEVEL_ERROR) return true;

    // Check level
    if (level > g_log_level) return false;

    // Check component
    if (component >= 0 && component < LOG_COMP_COUNT) {
        return g_component_enabled[component];
    }

    return false;
}

/**
 * Escape string for JSON output.
 * Handles: " (quote), \ (backslash), \n (newline), \r (carriage return), \t (tab)
 */
static void json_escape_string(const char *str, char *buf, size_t bufsize) {
    size_t j = 0;
    for (size_t i = 0; str[i] && j < bufsize - 1; i++) {  // Leave room for null terminator
        if (str[i] == '"' || str[i] == '\\') {
            if (j < bufsize - 2) {  // Need room for 2 chars + null
                buf[j++] = '\\';
                buf[j++] = str[i];
            } else {
                break;  // Buffer full
            }
        } else if (str[i] == '\n') {
            if (j < bufsize - 2) {
                buf[j++] = '\\';
                buf[j++] = 'n';
            } else {
                break;
            }
        } else if (str[i] == '\r') {
            if (j < bufsize - 2) {
                buf[j++] = '\\';
                buf[j++] = 'r';
            } else {
                break;
            }
        } else if (str[i] == '\t') {
            if (j < bufsize - 2) {
                buf[j++] = '\\';
                buf[j++] = 't';
            } else {
                break;
            }
        } else {
            buf[j++] = str[i];
        }
    }
    buf[j] = '\0';
}

void log_write(log_level_t level, log_component_t component,
               const char *file, int line, const char *fmt, ...) {
    if (!log_is_enabled(level, component)) return;

    const char *comp_name = (component >= 0 && component < LOG_COMP_COUNT)
                           ? component_names[component]
                           : "unknown";
    const char *level_name = (level >= 0 && level < 5)
                            ? level_names[level]
                            : "UNKNOWN";

    // Extract just the filename (not full path)
    const char *filename = strrchr(file, '/');
    filename = filename ? filename + 1 : file;

    // Format the message
    char message[LOG_MESSAGE_MAX];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    if (g_log_format == LOG_FORMAT_JSON) {
        // JSON format → stderr (for machine parsing/automation)
        char escaped_message[LOG_ESCAPED_MAX];
        json_escape_string(message, escaped_message, sizeof(escaped_message));

        fprintf(stderr, "{\"timestamp\":%ld,\"level\":\"%s\",\"component\":\"%s\","
                        "\"file\":\"%s\",\"line\":%d,\"message\":\"%s\"}\n",
                time(NULL), level_name, comp_name, filename, line, escaped_message);
    } else {
        // Plain text format → stderr (logs should not mix with program output on stdout)
        // [COMPONENT-LEVEL] file:line: message
        fprintf(stderr, "[%s-%s] %s:%d: %s", comp_name, level_name, filename, line, message);

        // Ensure newline
        if (message[0] && message[strlen(message) - 1] != '\n') {
            fprintf(stderr, "\n");
        }
    }
}
