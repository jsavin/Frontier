# Portable Build Patterns

**Last Updated**: 2025-12-28
**Related Issues**: #186, #189
**Related PRs**: #188 (enum redefinition fixes), #189 (build blocker fixes)

## Overview

Frontier supports multiple build configurations targeting different platforms and environments:
- **Classic Mac** (FRONTIER_CLASSIC) - Original Macintosh toolbox APIs
- **Portable** (FRONTIER_PORTABLE) - Pure C, no platform-specific APIs (e.g., unit tests, embedded)
- **Headless** (FRONTIER_HEADLESS) - Server runtime, no UI, uses real file/system implementations

This document explains the patterns used to manage code that works across these configurations.

## Portable Stub Guard Pattern

### The Pattern

Many modules provide multiple implementations: high-performance platform-specific versions and portable stub versions for minimal environments.

```c
// In portable/standard_portable.h

#if defined(FRONTIER_PORTABLE) && FRONTIER_ALLOW_PORTABLE_STUBS && !defined(FRONTIER_HEADLESS)
/* Portable file helper stubs */
static inline boolean filespectopath(const struct tyfilespec* fs, bigstring bs) {
    (void)fs;
    setemptystring(bs);
    return false;
}
// ... more stub implementations
#endif
```

### Why Three Conditions?

1. **`defined(FRONTIER_PORTABLE)`** - Only in portable builds
2. **`FRONTIER_ALLOW_PORTABLE_STUBS`** - Opt-in flag for stub fallbacks (default=1)
3. **`!defined(FRONTIER_HEADLESS)`** - Exclude headless mode

### Why Exclude Headless?

**Headless mode uses real implementations, not stubs.**

- **Portable**: Unit tests, embedded environments → stubs are acceptable (return false/empty)
- **Headless**: Server runtime → needs actual functionality (file_portable.c)

The guard prevents conflicts between:
- `static inline` stubs in `standard_portable.h`
- `extern` declarations in `file.h` pointing to real implementations

### Example: File API Separation

```c
// file.h - Declarations used by all builds
#if defined(FRONTIER_PORTABLE) || defined(FRONTIER_HEADLESS)
extern boolean filespectopath(const struct tyfilespec*, bigstring);
extern boolean equalfilespecs(const struct tyfilespec*, const struct tyfilespec*);
#endif

// standard_portable.h - Stubs for PORTABLE-only (not HEADLESS)
#if defined(FRONTIER_PORTABLE) && FRONTIER_ALLOW_PORTABLE_STUBS && !defined(FRONTIER_HEADLESS)
static inline boolean filespectopath(const struct tyfilespec* fs, bigstring bs) {
    // Minimal stub implementation
    setemptystring(bs);
    return false;
}
#endif

// portable/file_portable.c - Real implementation for HEADLESS
boolean filespectopath(const struct tyfilespec* fs, bigstring bs) {
    // Full implementation that actually works
    ...
}
```

## Include Path Conventions

### Relative vs Absolute Paths

**Current practice** (as of #189):
```c
// In Common/headers/file.h
#if defined(FRONTIER_PORTABLE) || defined(FRONTIER_HEADLESS)
#include "../../portable/standard_portable.h"
#endif
```

**Pros:**
- Works without Makefile changes
- Explicit about relationship between headers

**Cons:**
- Fragile (moves files = breaks includes)
- Less readable than absolute paths
- Harder to grep for

### Recommended Future Improvement

Add `portable/` to include paths in Makefile for all builds:

```makefile
CFLAGS += -I../portable
```

Then simplify includes:
```c
// In Common/headers/file.h
#if defined(FRONTIER_PORTABLE) || defined(FRONTIER_HEADLESS)
#include "standard_portable.h"
#endif
```

**Benefits:**
- Cleaner include statements
- Works regardless of directory structure
- Consistent with other include paths
- Easier to refactor

**Implementation note**: This is a low-priority refactoring. Current relative paths work fine and are acceptable. Make this change only when refactoring include paths for other reasons.

## Build Configuration Hierarchy

```
Classic Mac (FRONTIER_CLASSIC)
    ↓ (no portable stubs)
    → Uses OS APIs: <MacTypes.h>, <AppleEvents.h>, etc.
    → Real implementations in Common/source/

Portable (FRONTIER_PORTABLE)
    ↓ (with FRONTIER_ALLOW_PORTABLE_STUBS=1)
    → Uses stubs from portable/standard_portable.h
    → Returns false/empty for unimplemented APIs
    → Used for: unit tests, embedded environments

Headless (FRONTIER_HEADLESS)
    ↓ (FRONTIER_PORTABLE=1 + FRONTIER_HEADLESS=1)
    → **Excludes stubs** (uses real implementations)
    → Uses: portable/file_portable.c, portable/wptext_portable.c, etc.
    → Real implementations that actually work
    → Used for: server runtime, CLI
```

## Type Definition Hierarchy

**Authoritative definitions live in one location:**

1. **`osincludes_portable.h`** - AppleEvent types (struct definitions for portable/headless)
2. **`shelltypes_portable.h`** - File system types (struct tyfilespec, etc.)
3. **`standard_portable.h`** - Basic types (byte, word, dword, callbacks, etc.)

**Pattern**: Never redefine these types in multiple locations. When a header needs these types, include the authoritative source, don't create fallback typedefs.

### Example: AppleEvent Type Resolution

```c
// osincludes_portable.h (AUTHORITATIVE)
#ifndef AEDesc
typedef struct AEDesc {
    OSType descriptorType;
    void *dataHandle;
} AEDesc;
#endif

// langipc.h (WRONG - don't do this)
#ifndef __APPLEEVENTS__
#if defined(FRONTIER_PORTABLE)
    typedef void* AEDesc;  // ❌ Conflicts with struct definition above
#endif
#endif

// langipc.h (CORRECT - after #189)
#ifndef __APPLEEVENTS__
#if !defined(FRONTIER_PORTABLE) && !defined(FRONTIER_HEADLESS)
    #include <AppleEvents.h>  // Classic Mac only
#endif
/* For portable/headless: Types already defined in osincludes_portable.h */
#endif
```

## Common Pitfalls

### Pitfall 1: Guard Too Broad
❌ **Bad**: Removing functions that external code needs
```c
#if !defined(FRONTIER_USE_PORTABLE_HANDLES)
void lockhandle(Handle h) { HLock(h); }  // ❌ Now unavailable in portable builds!
#endif
```

✅ **Good**: Keep thin wrapper functions always available
```c
void lockhandle(Handle h) { HLock(h); }  // ✅ Always compiled
```

### Pitfall 2: Duplicate Type Definitions
❌ **Bad**: Define same type in multiple places
```c
// file.h
typedef void* tyfilespec;

// shelltypes_portable.h
typedef struct { ... } tyfilespec;  // ❌ Conflict!
```

✅ **Good**: Single authoritative definition with guards
```c
#ifndef FRONTIER_PORTABLE_DEFINED_TYFILESPEC
typedef struct { ... } tyfilespec;
#define FRONTIER_PORTABLE_DEFINED_TYFILESPEC 1
#endif
```

### Pitfall 3: Conflicting Stubs and Declarations
❌ **Bad**: Inline stubs conflict with extern declarations
```c
// standard_portable.h
static inline boolean filespectopath(...) { ... }

// file.h
extern boolean filespectopath(...);  // ❌ Conflicting linkage!
```

✅ **Good**: Guard stubs to exclude when extern declarations are active
```c
// standard_portable.h
#if defined(FRONTIER_PORTABLE) && !defined(FRONTIER_HEADLESS)
static inline boolean filespectopath(...) { ... }
#endif

// file.h
#if defined(FRONTIER_PORTABLE) || defined(FRONTIER_HEADLESS)
extern boolean filespectopath(...);
#endif
```

## References

- **PR #189**: Build blocker fixes explaining these patterns
- **PR #188**: Enum redefinition fixes (related pattern)
- **Issue #186**: Test suite compilation blockers
- **docs/portable_handles.md**: Handle implementation patterns
- **docs/LOGGING_STANDARDS.md**: Logging in portable/headless environments

## Future Work

1. **Include Path Refactoring**: Add `portable/` to Makefile CFLAGS and update includes
2. **Type Definition Audit**: Ensure all types follow single-definition pattern
3. **Build Configuration Documentation**: Expand this document with more examples
4. **Testing**: Add test cases verifying correct implementations are used in each build mode
