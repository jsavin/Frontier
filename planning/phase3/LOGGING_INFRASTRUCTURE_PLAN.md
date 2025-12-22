# Logging Infrastructure Plan

**Created**: 2025-12-20
**Status**: ✅ APPROVED - Ready for implementation
**Approved**: 2025-12-20
**Context**: Replace 352 fprintf(stderr) statements and 76 debug ifdefs with structured runtime logging

---

## Executive Summary

**Problem**: Frontier has 352 fprintf(stderr) statements scattered across the codebase, plus 76 `#ifdef fldebug` blocks that require recompilation to change verbosity. This creates maintenance burden, testing difficulty, and inflexibility.

**Solution**: Implement a lightweight structured logging system with:
- Runtime log level control (no rebuild needed)
- Per-component filtering (e.g., only show database logs)
- Environment variable configuration
- Zero runtime cost when disabled
- Compatible with existing error handling patterns

**Impact**:
- **Eliminates**: 76 ifdef blocks (22% of total ifdef count)
- **Consolidates**: 352 fprintf statements into consistent API
- **Enables**: Dynamic debugging without recompilation
- **Maintains**: Existing error message format for UserTalk errors

---

## Current State Analysis

### Distribution of Logging Statements

From `reports/static-analysis/logging/2025-12-20-logging-patterns.md`:

| File | fprintf Count | Purpose |
|------|---------------|---------|
| langhash.c | 58 | Hash table debug/diagnostics |
| db.c | 47 | Database layer debug |
| db_format.c | 46 | Format adapter/migration |
| tableexternal_common.c | 25 | External table handling |
| tablepack.c | 19 | Table serialization |
| oppack_v7.c | 17 | Outline packing |
| opops.c | 16 | Outline operations |
| langvalue.c | 16 | Value handling |
| lang.c | 16 | Language runtime |
| _20+ other files_ | 5-13 each | Various subsystems |

**Total**: 352 statements across 25+ files

### Current Patterns

**Pattern 1: Prefixed diagnostics**
```c
fprintf(stderr, "[headless] decode v7 view[%d]=0x%016llx\n", idx, adr);
fprintf(stderr, "[db-trace] materialize_recursive: depth=%d\n", depth);
fprintf(stderr, "[diag] node[%ld] name='%.*s'\n", i, len, name);
```

**Pattern 2: Debug ifdefs**
```c
#ifdef fldebug
    fprintf(stderr, "DEBUG: hashtable size=%ld\n", size);
#endif

#ifdef DATABASE_DEBUG
    fprintf(stderr, "DB: unpacking table at 0x%llx\n", adr);
#endif
```

**Pattern 3: Direct error output**
```c
fprintf(stderr, "FATAL: tydatabaserecord_64.views offset=%zu expected=16\n", offset);
fprintf(stderr, "ERROR: tableverbinmemory failed for '%.*s'\n", len, name);
```

### Problems with Current Approach

1. **No runtime control** - Must recompile to change verbosity
2. **Inconsistent formatting** - Some have prefixes, some don't
3. **No filtering** - Can't isolate specific subsystems
4. **Testing difficulty** - Debug code paths untested in release builds
5. **Maintenance burden** - 76 ifdef blocks scattered across files
6. **Token consumption** - User complains about log noise in output

---

## Design Proposal Menu

### Option A: Lightweight Macro-Based System (RECOMMENDED)

**Architecture**: Compile-time component enumeration + runtime level checking

**Pros**:
- Zero runtime cost when disabled (optimized out completely)
- Simple implementation (~200 lines of code)
- Easy migration from fprintf
- No dynamic allocation
- Compatible with all C compilers

**Cons**:
- Components must be known at compile time
- Can't add components dynamically
- Basic filtering only (level + component)

**Implementation complexity**: LOW
**Performance overhead**: NONE when disabled, minimal when enabled
**Migration effort**: LOW - mechanical search/replace

---

### Option B: Full-Featured Logging Library

**Architecture**: Plugin-based system with formatters, handlers, and filters

**Pros**:
- Highly flexible (JSON output, file logging, network logging)
- Runtime configuration of formatters
- Advanced filtering (by file, function, regex)
- Structured logging (key-value pairs)

**Cons**:
- Complex implementation (~1500+ lines)
- Runtime overhead even when disabled
- Over-engineered for current needs
- Harder to maintain

**Implementation complexity**: HIGH
**Performance overhead**: MEDIUM
**Migration effort**: MEDIUM-HIGH

---

### Option C: Hybrid (Macros + Optional Extensions)

**Architecture**: Lightweight core (Option A) + optional advanced features

**Pros**:
- Start simple, extend later if needed
- Core has zero overhead
- Extensions opt-in (JSON output, file logging)

**Cons**:
- Two-tier complexity
- Need to maintain both simple and advanced paths

**Implementation complexity**: MEDIUM
**Performance overhead**: LOW
**Migration effort**: LOW initially, MEDIUM for extensions

---

## RECOMMENDATION: Option A (Lightweight Macro-Based)

**Rationale**:
1. **Headless focus** - Don't need complex logging infrastructure for CLI tool
2. **Performance** - Zero cost when disabled is critical for runtime
3. **Simplicity** - Easier to maintain, less code to debug
4. **Sufficient** - Covers all current use cases (debug, error, trace)
5. **User preference** - User wants to minimize token consumption and noise

**Future-proofing**: Can add Option C extensions later if needed (file logging, structured output)

---

## Detailed Design: Option A

### API Design

**Header**: `Common/headers/logging.h`

```c
#ifndef logging_h
#define logging_h

#include <stdarg.h>
#include <stdbool.h>

/*
 * Logging Infrastructure
 *
 * Provides runtime-controlled logging with per-component filtering.
 * Configure via environment variables:
 *   FRONTIER_LOG_LEVEL=error|warn|info|debug|trace (default: warn)
 *   FRONTIER_LOG_COMPONENT=db,hash,table,... (default: all)
 *
 * Example:
 *   FRONTIER_LOG_LEVEL=debug FRONTIER_LOG_COMPONENT=db ./frontier-cli -e "..."
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
    LOG_COMP_PACK,            // Serialization (oppack_v7.c)
    LOG_COMP_PARSE,           // Parser (langparser.c)
    LOG_COMP_EVAL,            // Evaluator (langevaluate.c, langops.c)
    LOG_COMP_OP,              // Outline processor (opops.c, opverbs.c)
    LOG_COMP_LANG,            // Language runtime (lang.c, langvalue.c)
    LOG_COMP_EXTERNAL,        // External objects (langexternal.c)
    LOG_COMP_STARTUP,         // Startup/initialization (langstartup.c)
    LOG_COMP_GENERAL,         // General/uncategorized
    LOG_COMP_COUNT            // Number of components (internal use)
} log_component_t;

// ============================================================================
// Public API
// ============================================================================

/**
 * Initialize logging system.
 * Reads FRONTIER_LOG_LEVEL and FRONTIER_LOG_COMPONENT environment variables.
 * Call once at program startup (before any logging calls).
 */
void log_init(void);

/**
 * Set log level programmatically (alternative to environment variable).
 * level: LOG_LEVEL_ERROR through LOG_LEVEL_TRACE
 */
void log_set_level(log_level_t level);

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

#endif /* logging_h */
```

---

### Implementation: `Common/source/logging.c`

```c
#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// ============================================================================
// Internal State
// ============================================================================

static log_level_t g_log_level = LOG_LEVEL_WARN;  // Default: warnings and errors
static bool g_component_enabled[LOG_COMP_COUNT];  // Per-component enable flags
static bool g_initialized = false;

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

    g_initialized = true;

    // Optional: log initialization message (only if INFO or above)
    if (g_log_level >= LOG_LEVEL_INFO && g_component_enabled[LOG_COMP_STARTUP]) {
        fprintf(stderr, "[STARTUP-INFO] Logging initialized: level=%s components=%s\n",
                level_names[g_log_level],
                comp_str ? comp_str : "all");
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

void log_write(log_level_t level, log_component_t component,
               const char *file, int line, const char *fmt, ...) {
    if (!log_is_enabled(level, component)) return;

    // Format: [COMPONENT-LEVEL] file:line: message
    // Example: [DB-DEBUG] db.c:1234: Opening database at path=/tmp/test.root

    const char *comp_name = (component >= 0 && component < LOG_COMP_COUNT)
                           ? component_names[component]
                           : "unknown";
    const char *level_name = (level >= 0 && level < 5)
                            ? level_names[level]
                            : "UNKNOWN";

    // Extract just the filename (not full path)
    const char *filename = strrchr(file, '/');
    filename = filename ? filename + 1 : file;

    // Print prefix
    fprintf(stderr, "[%s-%s] %s:%d: ", comp_name, level_name, filename, line);

    // Print message
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    // Ensure newline
    if (fmt[strlen(fmt) - 1] != '\n') {
        fprintf(stderr, "\n");
    }
}
```

---

### Output Format Examples

```bash
# Default (WARN level, all components)
$ ./frontier-cli -e "1+1"
[DB-WARN] db.c:234: Database cache 80% full, performance may degrade

# Debug level, all components
$ FRONTIER_LOG_LEVEL=debug ./frontier-cli -e "sizeOf(system)"
[STARTUP-INFO] logging.c:156: Logging initialized: level=debug components=all
[DB-DEBUG] db.c:1234: Opening database at path=databases/test.root
[DB-DEBUG] db_format.c:456: Database version=7 use_64bit_format=1
[HASH-DEBUG] langhash.c:789: Unpacking hash table at adr=0x12345678
[TABLE-DEBUG] tablepack.c:234: Table 'system' has 245 entries

# Only database component
$ FRONTIER_LOG_LEVEL=debug FRONTIER_LOG_COMPONENT=db ./frontier-cli -e "..."
[DB-DEBUG] db.c:1234: Opening database at path=databases/test.root
[DB-DEBUG] db_format.c:456: Database version=7 use_64bit_format=1
# (no HASH or TABLE messages)

# Multiple components
$ FRONTIER_LOG_COMPONENT=db,hash,table FRONTIER_LOG_LEVEL=trace ./frontier-cli -e "..."
[DB-TRACE] db.c:100: Entering dbopenfile, path=databases/test.root
[DB-DEBUG] db.c:1234: Opening database at path=databases/test.root
[HASH-TRACE] langhash.c:50: Entering hashunpacktable, adr=0x12345678
[HASH-DEBUG] langhash.c:789: Unpacking hash table at adr=0x12345678
```

---

## Migration Strategy

### Phase 1: Infrastructure (Week 1)

**Tasks**:
1. Create `Common/headers/logging.h`
2. Create `Common/source/logging.c`
3. Add to build system (Makefile, Xcode project)
4. Add `log_init()` call to main() in frontier-cli
5. Write unit tests for logging system

**Testing**:
```bash
# Test environment variable parsing
FRONTIER_LOG_LEVEL=debug ./frontier-cli -e "1+1"
FRONTIER_LOG_COMPONENT=db ./frontier-cli -e "1+1"
FRONTIER_LOG_LEVEL=trace FRONTIER_LOG_COMPONENT=db,hash ./frontier-cli -e "1+1"
```

**Deliverable**: Working logging infrastructure with zero impact on existing code

---

### Phase 2: High-Volume Files (Weeks 2-4)

Migrate files with most fprintf statements first (highest impact):

| Week | Files | fprintf Count | Strategy |
|------|-------|---------------|----------|
| 2 | langhash.c | 58 | Replace with `log_debug(LOG_COMP_HASH, ...)` |
| 3 | db.c, db_format.c | 93 total | Replace with `log_debug(LOG_COMP_DB, ...)` |
| 4 | tableexternal_common.c, tablepack.c | 44 total | Replace with `log_debug(LOG_COMP_TABLE, ...)` |

**Migration pattern**:

```c
// BEFORE:
#ifdef fldebug
    fprintf(stderr, "[DEBUG] hashtable size=%ld\n", size);
#endif

// AFTER:
log_debug(LOG_COMP_HASH, "hashtable size=%ld", size);
```

```c
// BEFORE:
fprintf(stderr, "[headless] decode v7 view[%d]=0x%016llx\n", idx, adr);

// AFTER:
log_debug(LOG_COMP_DB, "decode v7 view[%d]=0x%016llx", idx, adr);
```

**Testing after each file**:
```bash
./tools/run_headless_tests.sh
```

---

### Phase 3: Remaining Files (Weeks 5-8)

Migrate remaining files incrementally:

| Priority | Files | Count | Component |
|----------|-------|-------|-----------|
| High | oppack_v7.c, opops.c, opverbs.c | 43 | LOG_COMP_OP |
| High | langvalue.c, lang.c, langexternal.c | 45 | LOG_COMP_LANG |
| Medium | langstartup.c, langevaluate.c, langops.c | 30 | LOG_COMP_EVAL |
| Low | 15+ other files | 100+ | Various |

**Schedule**: 3-5 files per week, test after each

---

### Phase 4: Cleanup (Week 9)

**Tasks**:
1. Remove all `#ifdef fldebug` blocks (should be zero by now)
2. Remove all `#ifdef DATABASE_DEBUG` blocks
3. Remove all `#ifdef DEBUG_SERIALIZER` blocks
4. Audit for any remaining fprintf(stderr) - convert or justify keeping
5. Update documentation (CONTRIBUTING.md, DEVELOPER_SETUP.md)

**Verification**:
```bash
# Should return zero
rg "#ifdef fldebug" Common/source/*.c | wc -l
rg "#ifdef DATABASE_DEBUG" Common/source/*.c | wc -l
rg "#ifdef DEBUG_SERIALIZER" Common/source/*.c | wc -l
```

---

### Phase 5: Documentation & Training (Week 10)

**Create documentation**:

1. **CONTRIBUTING.md section**:
```markdown
## Logging

Frontier uses a structured logging system with runtime control. Use logging functions instead of fprintf(stderr):

```c
log_error(LOG_COMP_DB, "Failed to open database: %s", path);
log_warn(LOG_COMP_HASH, "Hash table %d%% full", pct);
log_info(LOG_COMP_STARTUP, "Runtime initialized");
log_debug(LOG_COMP_TABLE, "Unpacking table '%.*s'", len, name);
log_trace(LOG_COMP_EVAL, "Entering function");
```

**Runtime control**:
```bash
# Set log level
FRONTIER_LOG_LEVEL=debug ./frontier-cli -e "..."

# Filter by component
FRONTIER_LOG_COMPONENT=db,hash ./frontier-cli -e "..."

# Maximum verbosity
FRONTIER_LOG_LEVEL=trace FRONTIER_LOG_COMPONENT=all ./frontier-cli -e "..."
```

**Available components**: db, hash, table, pack, parse, eval, op, lang, external, startup, general
```

2. **Comment header in logging.h** (already included above)

3. **Example usage in README** or quickstart guide

---

## Performance Considerations

### Runtime Overhead

**When logging is disabled** (default WARN level):
```c
log_debug(LOG_COMP_DB, "expensive message: %s", expensive_function());
```

**Compiled to**:
```c
if (!log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB)) return;  // Fast path: single check + return
// expensive_function() never called
```

**Overhead**: 1 function call + 2 comparisons + 1 return = ~3-5 CPU cycles (negligible)

**Optimization**: Modern compilers will inline `log_is_enabled()`, making it even faster.

---

### For Expensive Operations

**Problem**: Sometimes constructing the log message is expensive:
```c
char *dump = hashtable_dump_expensive(ht);  // Expensive!
log_debug(LOG_COMP_HASH, "Table: %s", dump);
free(dump);
```

**Solution**: Use `log_enabled()` guard:
```c
if (log_enabled(LOG_LEVEL_DEBUG, LOG_COMP_HASH)) {
    char *dump = hashtable_dump_expensive(ht);
    log_debug(LOG_COMP_HASH, "Table: %s", dump);
    free(dump);
}
```

---

### Memory Overhead

- **Global state**: ~100 bytes (log level + component flags + initialization flag)
- **Stack per call**: ~50 bytes (va_list + local variables)
- **No heap allocation**: All operations use stack only

**Total**: Negligible

---

## Special Considerations

### UserTalk Error Messages

**Do NOT replace UserTalk error messages with logging**:

```c
// CORRECT: UserTalk error (shown to user)
langerrormessage(BIGSTRING("\pCan't open file because it doesn't exist."));

// CORRECT: Internal diagnostic (for developers)
log_debug(LOG_COMP_DB, "File not found: %s", path);

// WRONG: Using log for user-facing errors
log_error(LOG_COMP_DB, "Can't open file because it doesn't exist.");
```

**Rule**:
- UserTalk errors: Use `langerrormessage()` (follows "Can't X because Y" pattern)
- Internal diagnostics: Use logging system

---

### Error Handling During Initialization

**Problem**: What if logging fails to initialize?

**Solution**: Logging system is fail-safe:
- `log_init()` has no error conditions (worst case: defaults to WARN + all components)
- `log_write()` checks initialization and calls `log_init()` if needed (lazy init)
- fprintf(stderr) is always available as fallback

**Example**:
```c
int main(int argc, char **argv) {
    // Optional: explicit initialization
    log_init();

    // OR: lazy initialization on first log call
    log_info(LOG_COMP_STARTUP, "Starting frontier-cli");

    // ... rest of program
}
```

---

### Thread Safety

**Current status**: Frontier headless is single-threaded

**If threading is added later**:
- Add mutex around `g_log_level` and `g_component_enabled[]` access
- Use thread-local storage for va_list formatting
- Ensure fprintf(stderr) calls are atomic (they are in most implementations)

**Estimated effort**: ~50 lines of pthread code

---

## Testing Strategy

### Unit Tests

**Create**: `tests/test_logging.c`

```c
#include "logging.h"
#include <assert.h>

void test_log_init_defaults(void) {
    log_init();
    assert(log_is_enabled(LOG_LEVEL_ERROR, LOG_COMP_DB) == true);
    assert(log_is_enabled(LOG_LEVEL_WARN, LOG_COMP_DB) == true);
    assert(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB) == false);
}

void test_log_set_level(void) {
    log_set_level(LOG_LEVEL_DEBUG);
    assert(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB) == true);
    assert(log_is_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB) == false);
}

void test_component_filtering(void) {
    log_set_level(LOG_LEVEL_DEBUG);
    log_set_component_enabled(LOG_COMP_DB, true);
    log_set_component_enabled(LOG_COMP_HASH, false);

    assert(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_DB) == true);
    assert(log_is_enabled(LOG_LEVEL_DEBUG, LOG_COMP_HASH) == false);
}

int main(void) {
    test_log_init_defaults();
    test_log_set_level();
    test_component_filtering();
    printf("All logging tests passed\n");
    return 0;
}
```

---

### Integration Tests

**Test with actual codebase**:

```bash
# Test 1: Default behavior (warnings only)
./frontier-cli -e "1+1" 2>&1 | grep -E "\[.*-WARN\]"

# Test 2: Debug level shows debug messages
FRONTIER_LOG_LEVEL=debug ./frontier-cli -e "1+1" 2>&1 | grep -E "\[.*-DEBUG\]"

# Test 3: Component filtering works
FRONTIER_LOG_COMPONENT=db FRONTIER_LOG_LEVEL=debug ./frontier-cli --system-root databases/Frontier-v6-v7.root -e "1+1" 2>&1 | grep -E "\[DB-"

# Test 4: Trace level is most verbose
FRONTIER_LOG_LEVEL=trace ./frontier-cli -e "1+1" 2>&1 | wc -l

# Test 5: Error level always shows
FRONTIER_LOG_LEVEL=error ./frontier-cli -e "badfunction()" 2>&1 | grep -E "\[.*-ERROR\]"
```

---

### Regression Testing

**Before migration**: Capture baseline output
```bash
./frontier-cli -e "sizeOf(system)" 2>&1 > baseline.txt
```

**After migration**: Compare behavior
```bash
FRONTIER_LOG_LEVEL=warn ./frontier-cli -e "sizeOf(system)" 2>&1 > migrated.txt
diff baseline.txt migrated.txt
```

**Expected**: Warnings and errors should be identical

---

## Build System Integration

### Makefile Changes

**Add logging.c to build**:
```makefile
# In Common/Makefile or equivalent
SOURCES += Common/source/logging.c
HEADERS += Common/headers/logging.h
```

---

### Xcode Project Changes

1. Add `Common/source/logging.c` to project
2. Add `Common/headers/logging.h` to project
3. Ensure both are included in all targets (frontier-cli, tests, etc.)

---

## Migration Examples

### Example 1: Simple Debug Output

**Before**:
```c
#ifdef fldebug
    fprintf(stderr, "DEBUG: hashtable size=%ld capacity=%ld\n", size, capacity);
#endif
```

**After**:
```c
log_debug(LOG_COMP_HASH, "hashtable size=%ld capacity=%ld", size, capacity);
```

---

### Example 2: Prefixed Diagnostic

**Before**:
```c
fprintf(stderr, "[headless] decode v7 view[%d]=0x%016llx\n", idx, adr);
```

**After**:
```c
log_debug(LOG_COMP_DB, "decode v7 view[%d]=0x%016llx", idx, adr);
```

---

### Example 3: Error Reporting

**Before**:
```c
fprintf(stderr, "FATAL: tydatabaserecord_64.views offset=%zu expected=16\n", offset);
```

**After**:
```c
log_error(LOG_COMP_DB, "FATAL: tydatabaserecord_64.views offset=%zu expected=16", offset);
```

**Note**: "FATAL" prefix is now redundant (level name shows ERROR), but can keep for clarity.

---

### Example 4: Multi-Level Debug

**Before**:
```c
#ifdef DATABASE_DEBUG
    fprintf(stderr, "[db-trace] unpacking table at 0x%llx\n", adr);
#endif

#ifdef fldebug
    fprintf(stderr, "DEBUG: table name='%.*s'\n", len, name);
#endif
```

**After**:
```c
log_trace(LOG_COMP_DB, "unpacking table at 0x%llx", adr);
log_debug(LOG_COMP_DB, "table name='%.*s'", len, name);
```

**Usage**:
```bash
# Show both trace and debug
FRONTIER_LOG_LEVEL=trace FRONTIER_LOG_COMPONENT=db ./frontier-cli -e "..."

# Show only debug (not trace)
FRONTIER_LOG_LEVEL=debug FRONTIER_LOG_COMPONENT=db ./frontier-cli -e "..."
```

---

### Example 5: Conditional Expensive Operation

**Before**:
```c
#ifdef fldebug
    {
        char buffer[4096];
        hashtable_dump_to_buffer(ht, buffer, sizeof(buffer));
        fprintf(stderr, "DEBUG: table contents:\n%s\n", buffer);
    }
#endif
```

**After**:
```c
if (log_enabled(LOG_LEVEL_DEBUG, LOG_COMP_HASH)) {
    char buffer[4096];
    hashtable_dump_to_buffer(ht, buffer, sizeof(buffer));
    log_debug(LOG_COMP_HASH, "table contents:\n%s", buffer);
}
```

---

## Output Formats

### Plain-Text (Default)

**Usage**: Default format, human-friendly for interactive CLI debugging

```
[DB-DEBUG] db.c:1234: Opening database at path=/tmp/test.root
[HASH-TRACE] langhash.c:789: Entering hashunpacktable, adr=0x12345678
```

**Example**:
```bash
FRONTIER_LOG_LEVEL=debug ./frontier-cli -e "..."
```

### JSON Output

**Use case**: Structured logging for machine parsing (AI agents, automation, integration)

**Implementation** (~100 lines):
```c
void log_write_json(log_level_t level, log_component_t component,
                    const char *file, int line, const char *fmt, ...) {
    if (!log_is_enabled(level, component)) return;

    fprintf(stderr, "{\"timestamp\":%ld,\"level\":\"%s\",\"component\":\"%s\","
                    "\"file\":\"%s\",\"line\":%d,\"message\":\"",
            time(NULL), level_names[level], component_names[component], file, line);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\"}\n");
}
```

**Usage**:
```bash
FRONTIER_LOG_FORMAT=json FRONTIER_LOG_LEVEL=debug ./frontier-cli -e "..."
```

**Example output**:
```json
{"timestamp":1703100735,"level":"DEBUG","component":"db","file":"db.c","line":1234,"message":"Opening database at path=/tmp/test.root"}
{"timestamp":1703100736,"level":"TRACE","component":"hash","file":"langhash.c","line":789,"message":"Entering hashunprocesstable, adr=0x12345678"}
```

### File Persistence

Logs are written to stderr by default. For persistence, use standard shell redirection:

```bash
# Capture to file
FRONTIER_LOG_LEVEL=debug ./frontier-cli -e "..." 2> debug.log

# Both JSON to file for AI/automation, plain-text to console
FRONTIER_LOG_FORMAT=json ./frontier-cli -e "..." 2> machine.json

# For long-running service: UserTalk logger captures stderr and routes to database/files as configured
```

---

## Future Improvements

### Runtime Log Level Control (Daemon Mode)

**Context**: Once Frontier runs as a long-running service with a web server, we need to change log levels dynamically without restarting.

**Architecture**:
```
Web Server (port 8000)
  ↓
REST endpoint: POST /log/setRuntimeLogLevel
  ↓
UserTalk glue: system.verbs.builtins.log.setRuntimeLogLevel(level, components={})
  ↓
C functions: log_set_level(), log_set_component_enabled()
  ↓
Runtime log state updated (thread-safe via UserTalk caller)
```

**C-side support** (already built-in):
- `log_set_level(log_level_t level)` - Update global log level
- `log_set_component_enabled(log_component_t component, bool enabled)` - Update per-component
- `log_is_enabled()` - Checks both level and component state at runtime

**UserTalk-side implementation** (future):
```usertalk
system.verbs.builtins.log.setRuntimeLogLevel = {
    local (level = "", components = {}) {
        // Parse level string (error, warn, info, debug, trace)
        // Parse components array
        // Call C logging functions to update state
        // Return status
    }
}
```

**REST integration** (future):
```
POST /log/setRuntimeLogLevel
Content-Type: application/json
{"level":"debug","components":["db","hash","table"]}

Response:
{"status":"ok","level":"debug","components":["db","hash","table"]}
```

**Benefits**:
- No process restart needed to change verbosity
- Diagnose production issues without downtime
- AI agents can dynamically adjust logging while debugging
- Component-specific debugging while service continues

**Timeline**: Phase 4+ (after initial logging infrastructure and daemon architecture stabilization)

---

## Rollout Timeline

| Week | Task | Deliverable | Risk |
|------|------|-------------|------|
| 1 | Implement logging.h/logging.c | Working infrastructure | LOW |
| 2 | Migrate langhash.c (58 statements) | -58 fprintf, -10 ifdefs | LOW |
| 3 | Migrate db.c, db_format.c (93 statements) | -93 fprintf, -20 ifdefs | MEDIUM |
| 4 | Migrate tableexternal_common.c, tablepack.c (44) | -44 fprintf, -15 ifdefs | LOW |
| 5 | Migrate oppack_v7.c, opops.c (33) | -33 fprintf, -8 ifdefs | LOW |
| 6 | Migrate langvalue.c, lang.c (32) | -32 fprintf, -6 ifdefs | LOW |
| 7 | Migrate remaining high-priority (50) | -50 fprintf, -10 ifdefs | LOW |
| 8 | Migrate remaining low-priority (42) | -42 fprintf, -7 ifdefs | LOW |
| 9 | Cleanup, remove all debug ifdefs | -76 ifdefs total | LOW |
| 10 | Documentation, examples | Complete | LOW |

**Total**: 10 weeks, ~352 fprintf statements migrated, ~76 ifdef blocks removed

---

## Success Metrics

### Before Migration

- **fprintf(stderr) count**: 352
- **Debug ifdef blocks**: 76 (`fldebug`, `DATABASE_DEBUG`, `DEBUG_SERIALIZER`)
- **Runtime control**: None (requires recompilation)
- **Per-component filtering**: None
- **Consistent formatting**: No

### After Migration

- **fprintf(stderr) count**: ~0 (except for fatal errors before logging init)
- **Debug ifdef blocks**: 0
- **Runtime control**: Yes (FRONTIER_LOG_LEVEL env var)
- **Per-component filtering**: Yes (FRONTIER_LOG_COMPONENT env var)
- **Consistent formatting**: Yes (`[COMPONENT-LEVEL] file:line: message`)

### Impact

- **Ifdef reduction**: 76 blocks removed (22% of total 350 ifdef blocks)
- **Code clarity**: Single consistent API vs. scattered fprintf/ifdef
- **Developer experience**: No rebuild needed to debug
- **Testing**: Debug code paths tested in all builds
- **Token consumption**: User can filter noise with FRONTIER_LOG_COMPONENT

---

## Risks & Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Migration introduces bugs | MEDIUM | HIGH | Incremental migration, test after each file |
| Performance regression | LOW | MEDIUM | Zero cost when disabled, inline optimization |
| Break existing error handling | LOW | HIGH | Don't touch langerrormessage(), only fprintf |
| Logging fails to initialize | LOW | LOW | Fail-safe defaults, lazy init |
| Too much log noise | MEDIUM | LOW | Default to WARN level, document filtering |

---

## Alternatives Considered (Not Recommended)

### Alternative 1: Keep fprintf + Remove Ifdefs

**Approach**: Remove `#ifdef fldebug` but keep fprintf statements, always compiled in.

**Pros**: Simple migration (just delete ifdefs)

**Cons**:
- No runtime control (all debug output always on)
- Performance impact (352 fprintf calls even when not needed)
- User complains about log noise

**Verdict**: ❌ Doesn't solve the core problem

---

### Alternative 2: Syslog Integration

**Approach**: Use standard syslog() API instead of custom logging

**Pros**: Standard API, works with system log aggregation

**Cons**:
- Requires syslog daemon (not always available on macOS)
- Can't control component filtering easily
- Loses stderr output (harder to debug in development)
- Additional complexity for headless CLI tool

**Verdict**: ❌ Over-engineered for current needs

---

### Alternative 3: Third-Party Logging Library

**Approach**: Integrate existing logging library (log4c, zlog, etc.)

**Pros**: Battle-tested, feature-rich

**Cons**:
- External dependency (against project philosophy)
- Learning curve for contributors
- May have features we don't need
- Harder to customize for Frontier's needs

**Verdict**: ❌ Not worth the dependency

---

## Open Questions

1. **Component granularity**: Are 11 components enough, or do we need more fine-grained control?
   - **Recommendation**: Start with 11, add more if needed later

2. **Log file rotation**: Should we implement automatic log rotation?
   - **Recommendation**: Defer - use system tools (logrotate) if needed

3. **Structured logging**: Do we need key-value pairs (JSON format)?
   - **Recommendation**: Defer - can add as Extension 2 if needed

4. **Backward compatibility**: Should we keep old fprintf for a transition period?
   - **Recommendation**: No - clean break is simpler to maintain

5. **Testing coverage**: How much unit testing is needed for logging system?
   - **Recommendation**: Basic unit tests (init, levels, components) + integration tests (actual usage)

---

## Approval Checklist

Before starting implementation, confirm:

- [ ] **Option A (Lightweight Macro-Based)** is approved
- [ ] **10-week timeline** is acceptable
- [ ] **Component list** (11 components) is sufficient
- [ ] **Migration strategy** (high-volume files first) is sound
- [ ] **Testing approach** (unit + integration + regression) is adequate
- [ ] **Risk mitigation** (incremental migration, test after each file) is acceptable

---

## Next Steps

1. **User approval** on this plan
2. **Create branch** for logging infrastructure work
3. **Week 1**: Implement logging.h/logging.c + unit tests
4. **Week 2+**: Begin incremental migration starting with langhash.c
5. **Track progress** in reports/progress/ directory

---

## References

- Analysis reports:
  - `reports/static-analysis/2025-12-20-ifdef-inventory.md`
  - `reports/static-analysis/logging/2025-12-20-logging-patterns.md`
- Related planning docs:
  - `planning/IFDEF_CLEANUP_STRATEGY.md`
