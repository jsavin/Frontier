# ifdef Cleanup Strategy

**Created**: 2025-12-20
**Status**: DRAFT - Awaiting user approval
**Context**: Frontier has ~350 ifdef blocks across 116 unique patterns, creating code bloat and maintenance burden

---

## Executive Summary

The codebase uses a **"xxx" prefix convention** to mark disabled code blocks. This convention is consistent and reliable - we can safely remove all "xxx"-prefixed ifdefs as dead code.

**Quick wins**:
- Remove 15+ "xxx"-prefixed ifdef blocks (xxxWIN95VERSION, xxxPIKE, xxxfldebug, etc.)
- Remove 3 explicit dead code markers (OBSOLETE, NEVER, NeverDefine_For_Reference)
- Clean up 76 debug ifdef blocks by migrating to runtime logging

**Total reduction**: ~94 ifdef blocks can be removed or converted to runtime control (27% of total)

---

## Phase 1: Remove Explicit Dead Code (LOW RISK)

### 1A. Remove "xxx"-Prefixed Blocks

**Target**: All ifdefs with "xxx" prefix (disabled by convention)

| Pattern | Count | Files | Action |
|---------|-------|-------|--------|
| `xxxWIN95VERSION` | 11 | strings.c, shellwindow.c, langpack.c, others | **DELETE** entire blocks |
| `xxxPIKE` | 1 | shellwindowmenu.c | **DELETE** |
| `xxxfldebug` | 1 | claybrowserexpand.c | **DELETE** |
| `xxxver`, `xxxx`, `xxxoplangli` | 3 | Various | **DELETE** |

**Example removal** (strings.c:1339):
```c
// BEFORE:
#ifdef xxxWIN95VERSION
    if (stringlength (bs) > 16) {
        RECT r;
        r.top = 0;
        r.bottom = 50;
        // ... Windows text measurement code ...
    }
#endif

// AFTER:
// (delete entire block)
```

**Risk**: NONE - "xxx" prefix explicitly marks code as disabled
**Testing**: Compile after removal, verify no build errors
**Estimated removal**: ~150 lines of dead code

---

### 1B. Remove Explicit Dead Code Markers

**Target**: Ifdefs explicitly marked as dead

| Pattern | Count | Files | Action |
|---------|-------|-------|--------|
| `OBSOLETE` | 1 | whirlpool.c:618 | **DELETE** 1000+ line obsolete crypto lookup table |
| `NEVER` | 1 | langevaluate.c:897 | **DELETE** error reporting code |
| `NeverDefine_For_Reference` | 1 | WinSockNetEvents.c:50 | **CONVERT** to comment block |

**Example** (whirlpool.c):
```c
// BEFORE:
#ifdef OBSOLETE
    static const u64 C0[256] = {
        LL(0x1818281878c0d878), LL(0x23236523af0526af),
        // ... 253 more entries ...
    };
    // ... tables C1-C7 ...
#endif

// AFTER:
// (delete entire 1000+ line block)
```

**Special case** - NeverDefine_For_Reference:
```c
// BEFORE:
#ifdef NeverDefine_For_Reference
    For reference I am listing the error codes from the windows winsock.h file here
    #define WSABASEERR 10000
    ...
#endif

// AFTER:
/*
 * Reference: Windows Sockets error codes from winsock.h
 * WSABASEERR 10000
 * ...
 */
```

**Risk**: NONE - explicitly marked as dead
**Estimated removal**: ~1200 lines

---

## Phase 2: Convert Debug Ifdefs to Runtime Logging (MEDIUM RISK)

### Problem Statement

**76 debug ifdef blocks** create:
- Code bloat from conditional branches
- Rebuild required to change log level
- Untested debug code paths in release builds
- Maintenance burden

**Current pattern**:
```c
#ifdef fldebug
    fprintf(stderr, "[DEBUG] tablename='%.*s'\n", ...);
#endif
```

**Target pattern**:
```c
log_debug(LOG_TABLE, "tablename='%.*s'", ...);
```

---

### 2A. Design Logging Infrastructure

**Create**: `Common/source/logging.h` and `Common/source/logging.c`

**API Design**:
```c
// Log levels
typedef enum {
    LOG_LEVEL_ERROR = 0,   // Always on
    LOG_LEVEL_WARN = 1,    // Default for production
    LOG_LEVEL_INFO = 2,    // Startup/shutdown messages
    LOG_LEVEL_DEBUG = 3,   // Detailed diagnostics
    LOG_LEVEL_TRACE = 4    // Maximum verbosity
} log_level_t;

// Log components (subsystems)
typedef enum {
    LOG_DB = 0,           // Database layer
    LOG_HASH,             // Hash tables
    LOG_TABLE,            // Table operations
    LOG_PACK,             // Serialization
    LOG_PARSE,            // Parser
    LOG_EVAL,             // Evaluator
    LOG_OP,               // Outline processor
    LOG_LANG,             // Language runtime
    LOG_ALL               // All components
} log_component_t;

// Logging functions
void log_error(log_component_t component, const char *fmt, ...);
void log_warn(log_component_t component, const char *fmt, ...);
void log_info(log_component_t component, const char *fmt, ...);
void log_debug(log_component_t component, const char *fmt, ...);
void log_trace(log_component_t component, const char *fmt, ...);

// Configuration (via environment variables)
void log_init(void);  // Reads FRONTIER_LOG_LEVEL and FRONTIER_LOG_COMPONENT
```

**Runtime control**:
```bash
# Set global log level
FRONTIER_LOG_LEVEL=debug ./frontier-cli -e "..."

# Enable specific components
FRONTIER_LOG_COMPONENT=db,hash FRONTIER_LOG_LEVEL=trace ./frontier-cli -e "..."

# Default: errors and warnings only
./frontier-cli -e "..."
```

**Implementation strategy**:
- Check component enable flag + level in each log_*() function
- No-op if disabled (zero runtime cost when off)
- Format to stderr with component prefix: `[DB-DEBUG] message`
- Keep existing fprintf for errors in places where logging isn't initialized

---

### 2B. Migration Plan

**Replace debug ifdefs incrementally**:

| Category | Count | Files | Migration |
|----------|-------|-------|-----------|
| `fldebug` | 55 | Throughout | Replace with `log_debug()` |
| `DATABASE_DEBUG` | 10 | db.c, langhash.c | Replace with `log_debug(LOG_DB, ...)` |
| `DEBUG_SERIALIZER` | 9 | db_format.c, tablepack.c | Replace with `log_debug(LOG_PACK, ...)` |
| `PARSER_TRACE` | 2 | langparser.c | Replace with `log_trace(LOG_PARSE, ...)` |

**Example migration** (langhash.c):
```c
// BEFORE:
#ifdef fldebug
    fprintf(stderr, "[DEBUG] hashtable size=%ld\n", size);
#endif

// AFTER:
log_debug(LOG_HASH, "hashtable size=%ld", size);
```

**Phased rollout**:
1. **Week 1**: Create logging.h/logging.c infrastructure
2. **Week 2**: Migrate database layer (db.c, db_format.c) - highest fprintf count
3. **Week 3**: Migrate hash tables (langhash.c) - 58 fprintf statements
4. **Week 4**: Migrate table operations (tablepack.c, tableexternal_common.c)
5. **Week 5**: Migrate remaining files incrementally

**Risk mitigation**:
- Keep old ifdef code in comments during initial migration
- Test each file's migration with `./tools/run_headless_tests.sh`
- Use git bisect if issues arise

**Estimated reduction**: 76 ifdef blocks → 0

---

## Phase 3: Consolidate Platform Ifdefs (MEDIUM RISK)

### 3A. Obsolete Platform Code

**Remove completely**:

| Pattern | Count | Status | Action |
|---------|-------|--------|--------|
| `oldMACVERSION` | 3 | Legacy Mac alias serialization | **DELETE** (v7 doesn't use aliases) |
| `WIN95VERSION` (commented) | 2 | Windows 95 trace logging | **DELETE** (commented out already) |

**Keep with cleanup**:

| Pattern | Count | Status | Action |
|---------|-------|--------|--------|
| `WIN95VERSION` (active) | 1 | Windows env var setting | **KEEP** (shellsysverbs.c:610) |

**Example** (langhash.c oldMACVERSION):
```c
// BEFORE:
#ifdef oldMACVERSION
    case filespecvaluetype: { /*need to save as a (minimal) alias*/
        register hdlfilespec x = val.data.filespecvalue;
        tyfilespec fs = **x;
        AliasHandle halias = nil;
        // ... Mac alias handling ...
    }
#endif

// AFTER:
// (delete - v7 format doesn't use Mac aliases)
```

**Risk**: LOW - v7 format migration already removed these dependencies
**Estimated removal**: ~50 lines

---

### 3B. Endianness Handling

**Current state**: Multiple patterns for byte order

| Pattern | Count | Purpose |
|---------|-------|---------|
| `__BIG_ENDIAN__` / `__LITTLE_ENDIAN__` | 2 | Compiler-defined |
| `SWAP_BYTE_ORDER` | 6 | Manual byte swapping |
| `L_ENDIAN` | 1 | Little-endian flag |

**Recommendation**: **CONSOLIDATE** to standard pattern

```c
// Use compiler-defined macros everywhere
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    // Little-endian code
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    // Big-endian code
#else
    #error "Unknown byte order"
#endif
```

**Files to update**: sha1dgst.c, db_format.c, tablepack.c
**Risk**: MEDIUM - byte order is critical; test on both Intel and Apple Silicon
**Estimated consolidation**: 9 blocks → ~3 blocks

---

## Phase 4: Feature Flag Consolidation (HIGH COMPLEXITY)

### 4A. Product Variants (PIKE)

**Current state**: 29 PIKE ifdefs for product variant

**Options**:
1. **Remove PIKE entirely** - Frontier-only codebase (RECOMMENDED for headless)
2. **Keep as compile-time flag** - Support both products via build configuration
3. **Convert to runtime flag** - Single binary supporting both modes

**Recommendation**: **Remove PIKE variant code**

**Rationale**:
- Headless Frontier is the target - Pike was a different product
- Simplifies codebase significantly (29 blocks removed)
- No current need for Pike-specific features

**Example** (progressbar.c):
```c
// BEFORE:
#ifdef PIKE
    BIGSTRING ("\x19" "http://pike.userland.com/"),
#else
    BIGSTRING ("\x1d" "http://frontier.userland.com/"),
#endif

// AFTER:
BIGSTRING ("\x1d" "http://frontier.userland.com/"),
```

**Risk**: MEDIUM - need to verify no Pike-specific features are required
**Decision needed**: User confirmation before removal

---

### 4B. Optional Features (Database Backends)

**Current state**: Multiple database backend flags

| Pattern | Count | Purpose | Status |
|---------|-------|---------|--------|
| `FRONTIER_MYSQL` | 3 | MySQL support | Optional feature |
| `FRONTIER_SQLITE` | 3 | SQLite support | Optional feature |
| `FRONTIER_PYTHON` | 2 | Python integration | Optional feature |

**Recommendation**: **Keep as optional compile-time features**

**Rationale**:
- These are genuinely optional extensions
- Compile-time flags avoid runtime overhead
- Users can build with subset of features

**Action**: Document in build system (Makefile or CMake)

```makefile
# Optional features (comment out to disable)
CFLAGS += -DFRONTIER_SQLITE
# CFLAGS += -DFRONTIER_MYSQL    # Disabled by default
# CFLAGS += -DFRONTIER_PYTHON   # Disabled by default
```

---

### 4C. UI/Display Ifdefs

**Remove entirely** (headless-only):

| Pattern | Count | Status |
|---------|-------|--------|
| `gray3Dlook` | 15 | GUI appearance |
| `appletinclude` | 5 | Applet UI |
| `claydialoginclude` | 5 | Dialog UI |

**Recommendation**: **DELETE** all GUI appearance code

**Rationale**: Headless mode has no display
**Risk**: NONE for headless builds

---

### 4D. Miscellaneous Feature Flags

**Audit and document**:

| Pattern | Count | Purpose | Recommendation |
|---------|-------|---------|----------------|
| `SMART_DB_OPENING` | 7 | DB optimization | Keep or remove based on testing |
| `xmlfeature` | 10 | XML support | Keep as optional feature |
| `landinclude` | 7 | Language includes | Investigate purpose |
| `compileall` | 11 | Build control | Document in build system |

**Action**: Requires deeper analysis of each flag's purpose

---

## Phase 5: Threading/Networking Cleanup (DEFER)

**Patterns**:
- `ACCEPT_CONN_WITHOUT_GLOBALS` (8)
- `ACCEPT_IN_SEPARATE_THREAD` (6)
- `FRONTIER_GUSI_2` (6)

**Recommendation**: **DEFER** until networking architecture is stabilized

**Rationale**:
- Networking layer needs architectural review first
- Threading model may change
- Not blocking other cleanup work

---

## Implementation Roadmap

### Month 1: Quick Wins
- **Week 1**: Remove "xxx"-prefixed blocks (15 blocks)
- **Week 2**: Remove explicit dead code (OBSOLETE, NEVER) (3 blocks)
- **Week 3**: Remove oldMACVERSION blocks (3 blocks)
- **Week 4**: Remove commented WIN95VERSION blocks (2 blocks)

**Total reduction**: 23 blocks (7% of total)
**Risk**: NONE - all dead code

---

### Month 2-3: Logging Infrastructure
- **Week 5**: Design and implement logging.h/logging.c
- **Week 6-7**: Migrate database layer debug ifdefs
- **Week 8-9**: Migrate hash table debug ifdefs
- **Week 10-11**: Migrate remaining debug ifdefs
- **Week 12**: Remove all `#ifdef fldebug` blocks

**Total reduction**: 76 blocks (22% of total)
**Risk**: MEDIUM - requires testing

---

### Month 4: Feature Flag Cleanup
- **Week 13**: Remove PIKE variant code (pending user approval)
- **Week 14**: Remove GUI appearance ifdefs (gray3Dlook, etc.)
- **Week 15**: Consolidate endianness handling
- **Week 16**: Document optional feature flags

**Total reduction**: 50+ blocks (14% of total)
**Risk**: MEDIUM-HIGH - requires architectural decisions

---

### Month 5+: Long-term Cleanup
- Audit miscellaneous flags (SMART_DB_OPENING, xmlfeature, etc.)
- Consolidate platform detection
- Review networking/threading flags
- Ongoing maintenance to prevent ifdef creep

---

## Success Metrics

**Before cleanup**:
- 116 unique ifdef patterns
- ~350 total ifdef blocks
- ~76 debug ifdef blocks requiring rebuild to change

**After Phase 1-3 (6 months)**:
- ~70 unique ifdef patterns (40% reduction)
- ~200 total ifdef blocks (43% reduction)
- 0 debug ifdef blocks (100% converted to runtime logging)

**Code quality improvements**:
- Runtime log level control (no rebuild needed)
- Reduced code bloat from dead branches
- Easier to read and maintain
- Better testing coverage (no ifdef-hidden code paths)

---

## Testing Strategy

**For each phase**:

1. **Compile test**: `make clean && make`
2. **Headless tests**: `./tools/run_headless_tests.sh`
3. **Migration test**: Verify v6→v7 database migration works
4. **Verb binding test**: `cd tools/kernelverbs_parser && python3 cli.py analyze`

**Regression detection**:
- Use git bisect if issues arise
- Keep each phase in separate commits for easy rollback
- Document any behavior changes in commit messages

---

## Risk Assessment

| Phase | Risk Level | Mitigation |
|-------|-----------|------------|
| Remove "xxx"-prefixed blocks | LOW | Convention is well-established |
| Remove explicit dead code | LOW | Marked as dead explicitly |
| Logging infrastructure | MEDIUM | Incremental migration, keep old code in comments initially |
| Platform ifdef consolidation | MEDIUM | Test on both Intel and Apple Silicon |
| PIKE removal | MEDIUM-HIGH | Requires user approval, may affect features |
| Feature flag cleanup | HIGH | Requires architectural decisions |

---

## Open Questions for User

1. **PIKE removal**: Should we remove all 29 PIKE variant ifdefs? Or keep Pike as a compile-time option?

2. **Optional features**: Which database backends should be enabled by default (MySQL, SQLite, Python)?

3. **SMART_DB_OPENING**: Is this optimization still in use? Should we keep or remove?

4. **xmlfeature**: Is XML support required for headless mode?

5. **Threading model**: Should we defer all threading/networking ifdef cleanup until architecture is reviewed?

6. **Phasing**: Is the 6-month timeline acceptable, or should we accelerate/decelerate?

---

## Next Steps

1. **User approval** on strategy and phasing
2. **Create branch** for Phase 1 work (quick wins)
3. **Begin implementation** starting with "xxx"-prefixed block removal
4. **Track progress** in reports/progress/ directory

---

## References

- Analysis reports:
  - `reports/static-analysis/2025-12-20-ifdef-inventory.md`
  - `reports/static-analysis/2025-12-20-logging-patterns.md`
  - `reports/static-analysis/dead-code/2025-12-20-dead-code-categories.md`

- Related planning docs:
  - (TBD: logging infrastructure design doc)
  - (TBD: feature flag architecture doc)
