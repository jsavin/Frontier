# Logging Pattern Analysis

Analysis Date: 2025-12-20

## Overview

**Total fprintf(stderr) statements**: 352  
**Busiest files**: langhash.c (58), db.c (47), db_format.c (46)

## Log Message Prefixes Observed

Pattern analysis shows structured prefixes:
- `[headless]` - Headless-mode specific diagnostic
- `[db-trace]` - Database trace information  
- `[diag]` - General diagnostics
- `[DEBUG]` - Debug output
- Direct messages without prefix - Error reporting

## Categories

### 1. Diagnostic Output (Highest Volume)
Files: db.c, db_format.c, langhash.c, tablepack.c, oppack_v7.c

Pattern: Detailed trace of internal operations
- Table packing/unpacking steps
- Address conversions  
- Migration progress
- Format adapter state

**Volume**: ~150 statements

### 2. Error Reporting  
Pattern: Format/structure validation errors
- Hash table inconsistencies
- Serialization failures
- Address translation errors

**Volume**: ~80 statements

### 3. Debug Output
Pattern: Conditional `#ifdef fldebug` blocks
- Function entry/exit tracing
- Variable state dumps
- Tree traversal logging

**Volume**: ~40 blocks

### 4. Performance/Profiling  
Pattern: Timing and measurement  
- Not yet identified in sample

## The #ifdef Problem

**Issue**: Logging is scattered with ifdef guards:
```c
#ifdef fldebug
  fprintf(stderr, "debug: ...");
#endif
```

This creates:
- **Code bloat**: Multiple conditional branches  
- **Maintenance burden**: Rebuild needed to change log level
- **Runtime inflexibility**: Can't adjust verbosity without recompile
- **Testing difficulty**: Debug code paths untested in release builds

**Current state**: ~40+ `#ifdef fldebug` blocks scattered across files

## Solution Direction

### Option A: Structured Logging (Recommended)
Replace all fprintf(stderr) with structured logging calls:
```c
log_debug(LOG_DB, "Address convert: v6=0x%llx v7=0x%llx", old, new);
log_error(LOG_HASH, "Inconsistent hash state");
```

Benefits:
- Runtime control: `FRONTIER_LOG_LEVEL=debug FRONTIER_LOG_COMPONENT=db`
- No rebuild needed to change verbosity
- Component-based filtering
- Consistent formatting

### Option B: Complete Removal
Strip out all diagnostic output entirely.

Risk: Difficult to debug production issues

### Option C: Hybrid
Keep error/warning messages, remove debug/diagnostic traces.

---

## Recommendations

1. **Create logging abstraction** in new file (e.g., `Common/source/logging.h`)
2. **Define per-component log levels**: `LOG_DB`, `LOG_HASH`, `LOG_PARSE`, etc.
3. **Implement runtime env var control**: `FRONTIER_LOG_LEVEL=debug FRONTIER_LOG_COMPONENT=*`
4. **Phase replacement**: Replace fprintf calls incrementally
5. **Keep errors**: Always log errors/warnings, respect level for debug/diag

