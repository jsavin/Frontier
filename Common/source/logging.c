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
static log_level_t g_component_level[LOG_COMP_COUNT];  // Per-component level override
static bool g_component_level_set[LOG_COMP_COUNT];      // Whether per-component level was explicitly set
static bool g_initialized = false;
static log_format_t g_log_format = LOG_FORMAT_TEXT;  // Default: plain text
static bool g_log_suppressed = false;  // Suppress all output (for JSON mode)

// Component names (for display and parsing)
static const char *component_names[] = {
    [LOG_COMP_DB]           = "db",
    [LOG_COMP_HASH]         = "hash",
    [LOG_COMP_TABLE]        = "table",
    [LOG_COMP_TABLE_LOOKUP] = "table_lookup",
    [LOG_COMP_PACK]         = "pack",
    [LOG_COMP_PARSE]        = "parse",
    [LOG_COMP_EVAL]         = "eval",
    [LOG_COMP_OP]           = "op",
    [LOG_COMP_LANG]         = "lang",
    [LOG_COMP_EXTERNAL]     = "external",
    [LOG_COMP_STARTUP]      = "startup",
    [LOG_COMP_THREAD]       = "thread",
    [LOG_COMP_MIGRATION]    = "migration",
    [LOG_COMP_FILE]         = "file",
    [LOG_COMP_GENERAL]      = "general"
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
        // Default: enable all components EXCEPT migration (disabled by default)
        for (int i = 0; i < LOG_COMP_COUNT; i++) {
            g_component_enabled[i] = (i != LOG_COMP_MIGRATION);
        }
        return;
    }

    // Check for "all" or "*"
    if (strcasecmp(str, "all") == 0 || strcmp(str, "*") == 0) {
        // Enable all components EXCEPT migration (disabled by default)
        for (int i = 0; i < LOG_COMP_COUNT; i++) {
            g_component_enabled[i] = (i != LOG_COMP_MIGRATION);
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
        // Trim leading whitespace
        while (*token == ' ' || *token == '\t') token++;
        if (*token == '\0') {
            token = strtok(NULL, ",");
            continue;
        }
        // Trim trailing whitespace
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

/**
 * Check if a string looks like a bare level name (no colons, matches a level).
 * Returns true if str is a recognized level name.
 */
static bool is_bare_level(const char *str) {
    if (!str) return false;
    return (strcasecmp(str, "error") == 0 ||
            strcasecmp(str, "warn") == 0 ||
            strcasecmp(str, "info") == 0 ||
            strcasecmp(str, "debug") == 0 ||
            strcasecmp(str, "trace") == 0);
}

void log_parse_spec(const char *spec) {
    if (!spec || !spec[0]) return;
    if (!g_initialized) log_init();

    char *str_copy = strdup(spec);
    if (!str_copy) return;

    // Check if this is a bare global level (no commas, no colons)
    if (!strchr(str_copy, ',') && !strchr(str_copy, ':') && is_bare_level(str_copy)) {
        g_log_level = parse_log_level(str_copy);
        // Enable all components (except migration) and clear per-component overrides
        for (int i = 0; i < LOG_COMP_COUNT; i++) {
            g_component_enabled[i] = (i != LOG_COMP_MIGRATION);
            g_component_level_set[i] = false;
        }
        free(str_copy);
        return;
    }

    // Per-component spec: start with all disabled, then enable mentioned components
    for (int i = 0; i < LOG_COMP_COUNT; i++) {
        g_component_enabled[i] = false;
        g_component_level_set[i] = false;
    }

    char *saveptr = NULL;
    char *token = strtok_r(str_copy, ",", &saveptr);
    while (token) {
        // Trim leading whitespace
        while (*token == ' ' || *token == '\t') token++;
        if (*token == '\0') {
            token = strtok_r(NULL, ",", &saveptr);
            continue;
        }
        // Trim trailing whitespace
        char *end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }

        // Split on colon
        char *colon = strchr(token, ':');
        if (colon) {
            *colon = '\0';
            const char *comp_name = token;
            const char *level_name_str = colon + 1;

            int comp = parse_component(comp_name);
            if (comp >= 0) {
                g_component_enabled[comp] = true;
                g_component_level[comp] = parse_log_level(level_name_str);
                g_component_level_set[comp] = true;
            } else {
                fprintf(stderr, "[LOG] Warning: Unknown component '%s' in log spec, ignoring\n", comp_name);
            }
        } else {
            // Component without level — enable at global default
            int comp = parse_component(token);
            if (comp >= 0) {
                g_component_enabled[comp] = true;
            } else {
                fprintf(stderr, "[LOG] Warning: Unknown component '%s' in log spec, ignoring\n", token);
            }
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    free(str_copy);
}

void log_init(void) {
    if (g_initialized) return;

    // Initialize per-component level arrays
    for (int i = 0; i < LOG_COMP_COUNT; i++) {
        g_component_level_set[i] = false;
        g_component_level[i] = LOG_LEVEL_WARN;
    }

    // Check for unified FRONTIER_LOG first (takes precedence over legacy vars)
    const char *log_spec = getenv("FRONTIER_LOG");
    const char *comp_str = NULL;

    if (log_spec && log_spec[0]) {
        // FRONTIER_LOG is set — use it, ignore legacy vars
        // Set defaults first, then parse_spec will override
        // Migration component is disabled by default to avoid verbose migration diagnostics
        g_log_level = LOG_LEVEL_WARN;
        for (int i = 0; i < LOG_COMP_COUNT; i++) {
            g_component_enabled[i] = (i != LOG_COMP_MIGRATION);
        }
        // Set initialized before parse_spec to prevent recursion (parse_spec calls log_init)
        g_initialized = true;
        log_parse_spec(log_spec);
    } else {
        // Legacy path: FRONTIER_LOG_LEVEL + FRONTIER_LOG_COMPONENT
        const char *level_str = getenv("FRONTIER_LOG_LEVEL");
        g_log_level = parse_log_level(level_str);

        comp_str = getenv("FRONTIER_LOG_COMPONENT");
        parse_components(comp_str);

        g_initialized = true;
    }

    // Parse FRONTIER_LOG_FORMAT (always respected)
    const char *format_str = getenv("FRONTIER_LOG_FORMAT");
    g_log_format = parse_log_format(format_str);

    // Optional: log initialization message (only if INFO or above)
    if (g_log_level >= LOG_LEVEL_INFO && g_component_enabled[LOG_COMP_STARTUP]) {
        if (log_spec && log_spec[0]) {
            fprintf(stderr, "[STARTUP-INFO] Logging initialized: spec=%s format=%s\n",
                    log_spec,
                    g_log_format == LOG_FORMAT_JSON ? "json" : "text");
        } else {
            fprintf(stderr, "[STARTUP-INFO] Logging initialized: level=%s components=%s format=%s\n",
                    level_names[g_log_level],
                    comp_str ? comp_str : "all",
                    g_log_format == LOG_FORMAT_JSON ? "json" : "text");
        }
    }
}

void log_set_level(log_level_t level) {
    if (!g_initialized) log_init();
    g_log_level = level;
}

void log_set_suppressed(bool suppressed) {
    if (!g_initialized) log_init();
    g_log_suppressed = suppressed;
}

void log_set_component_enabled(log_component_t component, bool enabled) {
    if (!g_initialized) log_init();
    if (component >= 0 && component < LOG_COMP_COUNT) {
        g_component_enabled[component] = enabled;
    }
}

void log_set_component_level(log_component_t component, log_level_t level) {
    if (!g_initialized) log_init();
    if (component >= 0 && component < LOG_COMP_COUNT) {
        g_component_level[component] = level;
        g_component_level_set[component] = true;
        g_component_enabled[component] = true;
    }
}

bool log_is_enabled(log_level_t level, log_component_t component) {
    if (!g_initialized) log_init();

    // Errors are always enabled
    if (level == LOG_LEVEL_ERROR) return true;

    // Check component is enabled
    if (component >= 0 && component < LOG_COMP_COUNT) {
        if (!g_component_enabled[component]) return false;

        // Use per-component level if set, otherwise global level
        log_level_t effective = g_component_level_set[component]
            ? g_component_level[component]
            : g_log_level;

        return (level <= effective);
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
    // Suppress all output if in JSON mode
    if (g_log_suppressed) return;

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

void log_hex_dump(log_component_t component, log_level_t level,
                  const unsigned char *data, size_t length, const char *label) {
    if (!log_is_enabled(level, component) || !data || length == 0) {
        return;
    }

    // Format hex dump with label and hex values
    char buf[512];  // Enough for ~32 bytes of hex (2 chars per byte + spaces)
    size_t pos = 0;

    // Add label
    if (label) {
        pos = snprintf(buf, sizeof(buf), "%s:", label);
    }

    // Add hex bytes (limit to 32 bytes per line for readability)
    size_t limit = (length > 32) ? 32 : length;
    for (size_t i = 0; i < limit && pos < sizeof(buf) - 3; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %02x", data[i]);
    }

    // Indicate if truncated
    if (length > limit && pos < sizeof(buf) - 4) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, " ...");
    }

    // Log the formatted hex dump
    log_write(level, component, __FILE__, __LINE__, "%s", buf);
}
