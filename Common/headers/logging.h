#ifndef logging_h
#define logging_h

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>  /* for size_t */

/*
 * Logging Infrastructure
 *
 * Provides runtime-controlled logging with per-component filtering.
 * Configure via environment variables:
 *   FRONTIER_LOG_LEVEL=error|warn|info|debug|trace (default: warn)
 *   FRONTIER_LOG_COMPONENT=db,hash,table,... (default: all)
 *   FRONTIER_LOG_FORMAT=text|json (default: text)
 *
 * Example:
 *   FRONTIER_LOG_LEVEL=debug FRONTIER_LOG_COMPONENT=db ./frontier-cli -e "..."
 *   FRONTIER_LOG_FORMAT=json FRONTIER_LOG_LEVEL=debug ./frontier-cli -e "..."
 */

// ============================================================================
// Log Levels
// ============================================================================

typedef enum {
    LOG_LEVEL_ERROR = 0,   // Errors that prevent operation (always shown)
    LOG_LEVEL_WARN  = 1,   // Warnings about unexpected conditions (default)
    LOG_LEVEL_INFO  = 2,   // Informational messages (startup, shutdown)
    LOG_LEVEL_DEBUG = 3,   // Detailed diagnostics for development
    LOG_LEVEL_TRACE = 4    // Maximum verbosity (function entry/exit)
} log_level_t;

// ============================================================================
// Log Components (Subsystems)
// ============================================================================

typedef enum {
    LOG_COMP_DB = 0,          // Database layer (db.c, db_format.c)
    LOG_COMP_HASH,            // Hash tables (langhash.c)
    LOG_COMP_TABLE,           // Table operations (tablepack.c, tableops.c, tableexternal*.c)
    LOG_COMP_TABLE_LOOKUP,    // Table value resolution in hot paths (langvalue.c:langgettableval)
    LOG_COMP_PACK,            // Serialization (oppack_v7.c)
    LOG_COMP_PARSE,           // Parser (langparser.c)
    LOG_COMP_EVAL,            // Evaluator (langevaluate.c, langops.c)
    LOG_COMP_OP,              // Outline processor (opops.c, opverbs.c)
    LOG_COMP_LANG,            // Language runtime (lang.c, langvalue.c)
    LOG_COMP_EXTERNAL,        // External objects (langexternal.c)
    LOG_COMP_STARTUP,         // Startup/initialization (langstartup.c)
    LOG_COMP_THREAD,          // Thread registry (threadregistry.c)
    LOG_COMP_MIGRATION,       // Database v6→v7 migration diagnostics (disabled by default)
    LOG_COMP_GENERAL,         // General/uncategorized
    LOG_COMP_COUNT            // Number of components (internal use)
} log_component_t;

// ============================================================================
// Public API
// ============================================================================

/**
 * Initialize logging system.
 * Reads FRONTIER_LOG_LEVEL, FRONTIER_LOG_COMPONENT, and FRONTIER_LOG_FORMAT environment variables.
 * Call once at program startup (before any logging calls).
 */
void log_init(void);

/**
 * Set log level programmatically (alternative to environment variable).
 * level: LOG_LEVEL_ERROR through LOG_LEVEL_TRACE
 */
void log_set_level(log_level_t level);

/**
 * Suppress all logging output (used in JSON mode to keep stderr clean).
 * suppressed: true to suppress all logs, false to resume normal logging
 */
void log_set_suppressed(bool suppressed);

/**
 * Enable/disable a specific component.
 * component: LOG_COMP_DB, LOG_COMP_HASH, etc.
 * enabled: true to enable, false to disable
 */
void log_set_component_enabled(log_component_t component, bool enabled);

/**
 * Check if a specific level/component is enabled.
 * Used internally by macros; can also be used for conditional expensive operations.
 */
bool log_is_enabled(log_level_t level, log_component_t component);

/**
 * Core logging function (used by macros).
 * Do not call directly; use log_error(), log_debug(), etc. instead.
 */
void log_write(log_level_t level, log_component_t component,
               const char *file, int line, const char *fmt, ...);

// ============================================================================
// Convenience Macros
// ============================================================================

/**
 * Log an error message.
 * Always shown regardless of log level (critical errors).
 *
 * Example:
 *   log_error(LOG_COMP_DB, "Failed to open database: %s", path);
 */
#define log_error(component, ...) \
    log_write(LOG_LEVEL_ERROR, component, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log a warning message.
 * Shown at WARN level and above (default).
 *
 * Example:
 *   log_warn(LOG_COMP_HASH, "Hash table 70%% full, consider resizing");
 */
#define log_warn(component, ...) \
    log_write(LOG_LEVEL_WARN, component, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log an informational message.
 * Shown at INFO level and above (startup/shutdown messages).
 *
 * Example:
 *   log_info(LOG_COMP_STARTUP, "Frontier runtime initialized");
 */
#define log_info(component, ...) \
    log_write(LOG_LEVEL_INFO, component, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log a debug message.
 * Shown at DEBUG level and above (detailed diagnostics).
 *
 * Example:
 *   log_debug(LOG_COMP_TABLE, "Unpacking table '%.*s' at 0x%llx", len, name, adr);
 */
#define log_debug(component, ...) \
    log_write(LOG_LEVEL_DEBUG, component, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log a trace message.
 * Shown at TRACE level only (maximum verbosity, function entry/exit).
 *
 * Example:
 *   log_trace(LOG_COMP_EVAL, "Entering evalidentifier, name='%.*s'", len, name);
 */
#define log_trace(component, ...) \
    log_write(LOG_LEVEL_TRACE, component, __FILE__, __LINE__, __VA_ARGS__)

// ============================================================================
// Conditional Logging (for expensive operations)
// ============================================================================

/**
 * Execute block only if logging is enabled for this level/component.
 * Useful when log message construction is expensive.
 *
 * Example:
 *   if (log_enabled(LOG_LEVEL_DEBUG, LOG_COMP_HASH)) {
 *       char *dump = hashtable_dump_expensive(ht);
 *       log_debug(LOG_COMP_HASH, "Table contents: %s", dump);
 *       free(dump);
 *   }
 */
#define log_enabled(level, component) \
    log_is_enabled(level, component)

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Log a hex dump of binary data.
 *
 * Format: label: 00 01 02 03 04 05 ... (up to 32 bytes per line)
 * Useful for debugging binary structures and serialized data.
 *
 * Example:
 *   log_hex_dump(LOG_COMP_HASH, LOG_LEVEL_TRACE, buffer, sizeof(buffer), "record bytes");
 *   Output: [hash-TRACE] file.c:42: record bytes: 00 01 02 03 04 05 ...
 */
void log_hex_dump(log_component_t component, log_level_t level,
                  const unsigned char *data, size_t length, const char *label);

// ============================================================================
// Pascal String Helpers
// ============================================================================

/**
 * Helper macro for logging Pascal strings (bigstring).
 *
 * Pascal strings (bigstring type) in Frontier store the length in the first byte
 * followed by the string data. The stringbaseaddress() function converts a Pascal
 * string to a C string pointer suitable for %s format specifiers.
 *
 * This macro improves readability when logging Pascal strings.
 *
 * Example (before):
 *   log_debug(LOG_COMP_HASH, "Table name: %s", stringbaseaddress(bs));
 *
 * Example (after):
 *   log_debug(LOG_COMP_HASH, "Table name: %s", PSTR(bs));
 *
 * Note: Files using PSTR() must #include "strings.h" (which provides stringbaseaddress()).
 */
#define PSTR(bs) stringbaseaddress(bs)

#endif /* logging_h */
