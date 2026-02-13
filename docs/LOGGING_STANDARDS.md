# Frontier Logging Standards

This document establishes standards for all logging in the Frontier codebase. All diagnostic and debug output must use structured logging macros - **never use `fprintf(stderr, ...)`**.

## Quick Reference

```c
// ✗ WRONG
fprintf(stderr, "[headless] error: %s\n", msg);

// ✓ CORRECT
log_error(LOG_COMP_DB, "error: %s", msg);
```

---

## Logging Levels

Five levels are available, used hierarchically (higher level includes all lower):

| Level | Macro | Default Shown | Use Case |
|-------|-------|---------------|----------|
| ERROR | `log_error()` | ✓ Always | Critical failures that prevent operation |
| WARN | `log_warn()` | ✓ Default | Unexpected conditions, but operation continues |
| INFO | `log_info()` | With `FRONTIER_LOG_LEVEL=info` | Startup messages, major milestones |
| DEBUG | `log_debug()` | With `FRONTIER_LOG_LEVEL=debug` | Diagnostic information during development |
| TRACE | `log_trace()` | With `FRONTIER_LOG_LEVEL=trace` | Maximum verbosity (function entry/exit, detailed diagnostics) |

## Log Components

Each log statement is tagged with a component. This allows filtering by subsystem.

```c
typedef enum {
    LOG_COMP_DB = 0,          // Database layer (db.c, db_format.c)
    LOG_COMP_HASH,            // Hash tables (langhash.c)
    LOG_COMP_TABLE,           // Table operations (tablepack.c, tableops.c)
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
    LOG_COMP_COUNT            // Number of components
} log_component_t;
```

**Choose the component that best matches the subsystem being debugged.** For new code, default to `LOG_COMP_GENERAL` if unsure.

### Special Components

**`LOG_COMP_MIGRATION`** is disabled by default (unlike other components). This prevents verbose v6→v7 database migration diagnostics from appearing during normal operation. Enable it explicitly when debugging migration issues:

```bash
# Enable migration diagnostics
FRONTIER_LOG_LEVEL=trace FRONTIER_LOG_COMPONENT=migration ./frontier-cli -e "..."

# Or with other components
FRONTIER_LOG_COMPONENT=db,migration ./frontier-cli --system-root old.root
```

---

## Basic Usage

### Error Logging

Use `log_error()` for critical failures:

```c
// Database write failed
log_error(LOG_COMP_DB, "dbwrite failed: seek to 0x%llx failed, bytes=%ld", adr, bytes);

// Hash table corruption detected
log_error(LOG_COMP_HASH, "hashunpackstring out-of-bounds: ix=%ld hsize=%ld", ix, hsize);
```

**Note**: ERROR level is always shown, regardless of `FRONTIER_LOG_LEVEL` setting.

### Warning Logging

Use `log_warn()` for unexpected but recoverable conditions:

```c
// Graceful degradation
log_warn(LOG_COMP_TABLE, "external table '%.*s' not found, using empty table",
         (int)name[0], name + 1);
```

### Info Logging

Use `log_info()` for startup/shutdown messages:

```c
// Startup message
log_info(LOG_COMP_STARTUP, "Frontier runtime initialized, version %s", VERSION_STR);

// Milestone reached
log_info(LOG_COMP_DB, "migration from v6 to v7 complete, database size=%ld bytes", total_size);
```

### Debug Logging

Use `log_debug()` for diagnostic information:

```c
// Tracking function behavior
log_debug(LOG_COMP_HASH, "hashunpackscalar ix=%d disklen=0x%08x type=%d",
          ix, disklen, val->valuetype);

// State inspection
log_debug(LOG_COMP_DB, "dballocate context mode use_64bit=%d adapter_repack=%d",
          ctx->use_64bit_format, ctx->adapter_repack);
```

### Trace Logging

Use `log_trace()` for maximum verbosity:

```c
// Function entry/exit
log_trace(LOG_COMP_EVAL, "langevaluate enter expr=%.*s", (int)expr[0], expr + 1);

// Detailed step tracking
log_trace(LOG_COMP_HASH, "materialize visit path=%s type=%d disk=%d",
          path, val->valuetype, val->fldiskval);
```

---

## Advanced Patterns

### Binary Data Diagnostics

Use `log_hex_dump()` for binary data inspection:

```c
#include "logging.h"

// Simple hex dump
unsigned char buffer[16];
log_hex_dump(LOG_COMP_DB, LOG_LEVEL_TRACE, buffer, sizeof(buffer), "record header");

// Output:
// [hash-TRACE] langhash.c:2844: record header: 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f
```

**Features**:
- Automatically limits to 32 bytes per line
- Shows "..." if truncated
- Conditional on log level (no cost if disabled)

**Before (streaming fprintf)**:
```c
fprintf(stderr, "[headless] raw bytes:");
for (int i = 0; i < len; i++)
    fprintf(stderr, " %02x", buf[i]);
fprintf(stderr, "\n");
```

**After (log_hex_dump)**:
```c
log_hex_dump(LOG_COMP_HASH, LOG_LEVEL_TRACE, buf, len, "raw bytes");
```

### Conditional Expensive Operations

When constructing log messages is expensive, guard with `log_enabled()`:

```c
// WRONG: Always constructs expensive string, even if logging disabled
char debug_path[512];
materialize_full_path(debug_path, sizeof(debug_path), nested_table);
log_debug(LOG_COMP_TABLE, "Full path: %s", debug_path);

// CORRECT: Only constructs if DEBUG level is enabled
if (log_enabled(LOG_LEVEL_DEBUG, LOG_COMP_TABLE)) {
    char debug_path[512];
    materialize_full_path(debug_path, sizeof(debug_path), nested_table);
    log_debug(LOG_COMP_TABLE, "Full path: %s", debug_path);
}
```

This pattern is essential for expensive operations like:
- Building full paths or complex strings
- Serializing data structures for inspection
- Counting/iterating for statistics

### Pascal String Formatting

Frontier uses Pascal strings (length-prefixed). Format them consistently:

```c
// Pascal string: bs[0] is length, bs[1..length] is data
bigstring bs_name;  // bs_name[0] = length, bs_name[1..] = actual string

// Correct format: %.*s takes (length, pointer)
log_trace(LOG_COMP_HASH, "Processing name='%.*s'", (int)bs_name[0], bs_name + 1);

// NOT: (void *) cast or other tricks
log_debug(LOG_COMP_TABLE, "Inserting table='%.*s'", (int)bsname[0], (char *)&bsname[1]);
```

---

## Runtime Control

### Per-Component Log Levels (`FRONTIER_LOG`)

The unified `FRONTIER_LOG` environment variable lets you set different log levels for different components. This is the recommended way to control logging.

**Syntax**: `FRONTIER_LOG=comp:level,comp:level,...`

```bash
# Per-component levels — trace db, but only errors from lang
FRONTIER_LOG=db:trace,lang:error ./frontier-cli -e "..."

# Component without level — uses global default (warn)
FRONTIER_LOG=db,hash:trace ./frontier-cli -e "..."

# Global level shorthand (no colon) — same as FRONTIER_LOG_LEVEL
FRONTIER_LOG=debug ./frontier-cli -e "..."

# Mix per-component and global: migration at trace, everything else at warn
FRONTIER_LOG=migration:trace ./frontier-cli --system-root old.root -e "..."
```

### CLI `--log` Flag

The `--log` flag provides the same syntax as `FRONTIER_LOG`, and can be specified multiple times:

```bash
# Single --log
frontier-cli --log db:trace,lang:error -e "..."

# Multiple --log (concatenated with commas)
frontier-cli --log db:trace --log lang:error -e "..."
```

### Precedence

When multiple logging configuration sources are present, they are applied in this order (highest to lowest precedence):

1. **`--log` CLI argument** — applied after env vars, overrides everything
2. **`FRONTIER_LOG` env var** — when set, ignores `FRONTIER_LOG_LEVEL` and `FRONTIER_LOG_COMPONENT`
3. **`FRONTIER_LOG_LEVEL` + `FRONTIER_LOG_COMPONENT`** — legacy vars, used when `FRONTIER_LOG` is not set

### Legacy Environment Variables

These still work when `FRONTIER_LOG` is **not** set:

```bash
# Set log level (default: WARN)
export FRONTIER_LOG_LEVEL=trace      # trace, debug, info, warn, error
export FRONTIER_LOG_LEVEL=debug

# Filter by component(s) (default: all)
export FRONTIER_LOG_COMPONENT=hash   # Single component
export FRONTIER_LOG_COMPONENT=db,hash  # Multiple components (comma-separated)

# Output format (always respected, independent of log spec)
export FRONTIER_LOG_FORMAT=json      # json or text (default: text)
export FRONTIER_LOG_FORMAT=text

# Example: Debug only hash table operations
FRONTIER_LOG_LEVEL=debug FRONTIER_LOG_COMPONENT=hash ./frontier-cli -e "..."

# Example: Full trace with JSON output
FRONTIER_LOG_LEVEL=trace FRONTIER_LOG_FORMAT=json ./frontier-cli --system-root db.root

# Example: Trace table lookups in hot path (reduce noise from general language runtime)
FRONTIER_LOG_LEVEL=trace FRONTIER_LOG_COMPONENT=table_lookup ./frontier-cli --system-root db.root -e "sizeOf(system)"
```

### Common Recipes

```bash
# Debug database issues without noise from other components
FRONTIER_LOG=db:debug ./frontier-cli --system-root Frontier.root -e "..."

# Trace hash table operations, warn on everything else
FRONTIER_LOG=hash:trace ./frontier-cli -e "..."

# Full trace on multiple components
FRONTIER_LOG=db:trace,hash:trace,table:trace ./frontier-cli -e "..."

# Migration diagnostics (migration is disabled by default)
FRONTIER_LOG=migration:trace,db:debug ./frontier-cli --system-root old.root -e "1"

# Same via CLI flag
./frontier-cli --log migration:trace --log db:debug --system-root old.root -e "1"
```

### Programmatic Control

For headless tests, set logging levels in code:

```c
#include "logging.h"

// Initialize logging from environment
log_init();

// Or set programmatically
log_set_level(LOG_LEVEL_DEBUG);
log_set_component_enabled(LOG_COMP_HASH, true);
log_set_component_enabled(LOG_COMP_DB, false);  // Disable DB logging

// Per-component log level (overrides global for this component)
log_set_component_level(LOG_COMP_DB, LOG_LEVEL_TRACE);

// Parse a log spec string programmatically
log_parse_spec("db:trace,lang:warn");
```

---

## Format String Guidelines

### Do NOT include:

- ❌ `\n` (newline) - logging framework adds it
- ❌ `[headless]` prefix - logging framework adds component/level
- ❌ File/line info - logging framework adds `file:line`

### Do include:

- ✓ Descriptive message
- ✓ Variable values in format string
- ✓ Context (path, name, state)

**Example**:

```c
// WRONG
fprintf(stderr, "[headless] error loading table '%.*s' at 0x%llx\n", len, name, adr);

// RIGHT
log_error(LOG_COMP_TABLE, "error loading table '%.*s' at 0x%llx", len, name, adr);

// Output: [table-ERROR] tableops.c:542: error loading table 'system' at 0x10000
```

---

## Migration from fprintf

If you encounter `fprintf(stderr, ...)` in the codebase, it should be migrated:

### Simple Single-line Patterns

```c
// BEFORE
fprintf(stderr, "[headless] hashunpackstring OOB ix=%ld hsize=%ld\n", ix, hsize);

// AFTER (choose appropriate level)
log_error(LOG_COMP_HASH, "hashunpackstring OOB ix=%ld hsize=%ld", ix, hsize);
```

### Multi-line Format Strings

```c
// BEFORE
fprintf(stderr, "[headless] hashpackvisit_v7 WRITE list name='%.*s' path=%s\n",
        bsname[0], bsname + 1,
        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>");

// AFTER
log_trace(LOG_COMP_HASH, "hashpackvisit_v7 WRITE list name='%.*s' path=%s",
          bsname[0], bsname + 1,
          (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>");
```

### Hex Dumps

```c
// BEFORE
fprintf(stderr, "[headless] raw bytes:");
for (int i = 0; i < len; i++)
    fprintf(stderr, " %02x", buf[i]);
fprintf(stderr, "\n");

// AFTER
log_hex_dump(LOG_COMP_HASH, LOG_LEVEL_TRACE, buf, len, "raw bytes");
```

---

## Enforcement

The `tools/check_fprintf.sh` script enforces this standard:

```bash
# Check for any fprintf(stderr) violations
./tools/check_fprintf.sh

# Show details of violations
./tools/check_fprintf.sh --fix

# Integrated into make check
make check-fprintf
```

If this check fails in CI, all `fprintf(stderr)` statements in your changes must be converted to logging macros before merge.

---

## Performance Considerations

### Zero Cost When Disabled

When a log level is disabled, the logging calls have **zero runtime overhead**:

```c
log_trace(component, "expensive message");
// ↓ With LOG_LEVEL < TRACE, this optimizes to nothing
```

This means:
- ✓ Safe to add trace/debug logging without performance impact
- ✓ No need to guard simple log calls
- ✓ Only guard expensive _message construction_ with `log_enabled()`

### Benchmarks

Typical performance impact (headless mode):
- Disabled logging: **0% overhead** (optimized to nothing)
- Enabled but filtered out: **<1ms per 1000 messages**
- Displayed: **~5-10ms per 1000 messages** (depends on output)

---

## Troubleshooting

### My logging doesn't appear

**Cause**: Log level or component not enabled

**Solution**:
```bash
# Increase log level
FRONTIER_LOG_LEVEL=trace ./frontier-cli -e "..."

# Enable all components
FRONTIER_LOG_COMPONENT=all ./frontier-cli -e "..."

# Or check what's enabled
./tools/check_fprintf.sh --fix  # Shows settings
```

### Logging format looks wrong

**Cause**: Likely still have `\n` or `[prefix]` in format string

**Solution**: Remove `\n` and prefixes - logging framework adds them:

```c
// WRONG
log_error(LOG_COMP_DB, "ERROR: something failed\n");

// CORRECT
log_error(LOG_COMP_DB, "something failed");
```

### Too much output

**Cause**: Log level is too verbose

**Solution**: Use per-component levels to focus on what matters:
```bash
# Only errors and warnings (global)
FRONTIER_LOG_LEVEL=warn ./frontier-cli -e "..."

# Only DB component
FRONTIER_LOG_COMPONENT=db ./frontier-cli -e "..."

# Per-component: trace db, but only errors from everything else
FRONTIER_LOG=db:trace ./frontier-cli -e "..."
```

---

## References

- `Common/headers/logging.h` - Logging API header
- `Common/source/logging.c` - Logging implementation
- `planning/phase3/LOGGING_INFRASTRUCTURE_PLAN.md` - Design documentation
- `tools/check_fprintf.sh` - Enforcement script
