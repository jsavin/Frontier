# Startup-Critical Verbs Implementation Guide

## Status
- State: Implementation Ready
- Phase: Week 1 - Startup-Critical Verbs
- Created: 2025-12-03
- Priority: BLOCKING

## Purpose

This document provides detailed implementation guidance for the two verbs currently blocking startup:
1. `frontier.getFilePath` (token 1 of frontier processor)
2. `file.folderFromPath` (token 24 of file processor)

These are the minimum verbs needed to get `system.startup` executing.

## Implementation 1: frontier.getFilePath

### Current Error
```
[ERROR] Verb not implemented: frontier.getFilePath
[ERROR] Execution failed at token 1
```

### Verb Signature
```
frontier.getFilePath() → filespec
```

**Purpose**: Returns a filespec pointing to the currently open Frontier database file.

### Legacy Implementation Analysis

**Location**: `Common/source/shellsysverbs.c` line ~136

```c
// From legacy code:
case filepathfunc: { /*frontier.getFilePath*/
    return setfilespecvalue(&frontierfilefspec, v);
}
```

**Key Observations:**
- Simply returns a global `frontierfilefspec` variable
- No UI dependencies
- Pure data access

### Headless Implementation

**File**: `tests/headless_frontier_verbs.c` (NEW)

```c
#include "frontier.h"
#include "standard.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "file.h"

/* Global to track the currently open database file */
static tyfilespec headless_frontier_filespec;
static boolean headless_frontier_filespec_valid = false;

/* Called by main() after opening database */
boolean headless_set_frontier_file(tyfilespec *fs) {
    if (fs == NULL)
        return false;

    headless_frontier_filespec = *fs;
    headless_frontier_filespec_valid = true;
    return true;
}

/* Token definitions for frontier processor */
enum {
    frv_getProgramPath = 0,      // token 0
    frv_getFilePath = 1,         // token 1 - THIS ONE
    frv_enableAgents = 2,        // token 2
    frv_requestToFront = 3,      // token 3
    frv_isRuntime = 4,           // token 4
    frv_countThreads = 5,        // token 5
    frv_isNative = 6,            // token 6 (originally isPowerPC)
    frv_reclaimMemory = 7,       // token 7
    frv_version = 8,             // token 8
    frv_hashStats = 9,           // token 9
    frv_getHashLoopCount = 10,   // token 10
    frv_hideApplication = 11,    // token 11
    frv_isValidSerialNumber = 12,// token 12
    frv_showApplication = 13     // token 13
};

static boolean frontier_valueproc(short token, hdltreenode hparam1,
                                   tyvaluerecord *vreturned, bigstring bserror) {
    tyvaluerecord *v = vreturned;

    switch (token) {
        case frv_getFilePath: {
            /* Return filespec of currently open database */
            if (!headless_frontier_filespec_valid) {
                copystring(BIGSTRING("\pNo database file is open"), bserror);
                return false;
            }
            return setfilespecvalue(&headless_frontier_filespec, v);
        }

        case frv_getProgramPath: {
            /* Return filespec of frontier-cli executable */
            /* TODO: Implement by storing executable path at startup */
            langerrormessage(BIGSTRING("\pfrontier.getProgramPath not yet implemented"));
            return false;
        }

        case frv_version: {
            /* Return Frontier version string */
            bigstring bsversion;
            copystring(BIGSTRING("\p11.0.0-headless"), bsversion);
            return setstringvalue(bsversion, v);
        }

        case frv_isRuntime: {
            /* Headless is always runtime (not development environment) */
            return setbooleanvalue(true, v);
        }

        case frv_isNative: {
            /* Always true on modern systems (no emulation) */
            return setbooleanvalue(true, v);
        }

        default:
            /* All other verbs stubbed for now */
            copystring(BIGSTRING("\pfrontier verb not implemented in headless mode"), bserror);
            return false;
    }
}

boolean frontierinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pfrontier"), bsname);

    if (!newfunctionprocessor(bsname, &frontier_valueproc, true, &htable))
        return false;

    pushhashtable(htable);

    /* Register all 14 verbs as defined in kernelverbs.rc */
    #define ADD_VERB(name, tok) do { \
        bigstring bs; \
        copystring(name, bs); \
        if (!langaddkeyword(bs, tok)) { \
            pophashtable(); \
            return false; \
        } \
    } while(0)

    ADD_VERB(BIGSTRING("\pgetprogrampath"), frv_getProgramPath);
    ADD_VERB(BIGSTRING("\pgetfilepath"), frv_getFilePath);
    ADD_VERB(BIGSTRING("\penableagents"), frv_enableAgents);
    ADD_VERB(BIGSTRING("\prequesttofront"), frv_requestToFront);
    ADD_VERB(BIGSTRING("\pisruntime"), frv_isRuntime);
    ADD_VERB(BIGSTRING("\pcountthreads"), frv_countThreads);
    ADD_VERB(BIGSTRING("\pisnative"), frv_isNative);  /* was ispowerpc */
    ADD_VERB(BIGSTRING("\preclaimmemory"), frv_reclaimMemory);
    ADD_VERB(BIGSTRING("\pversion"), frv_version);
    ADD_VERB(BIGSTRING("\phashstats"), frv_hashStats);
    ADD_VERB(BIGSTRING("\pgethashloopccount"), frv_getHashLoopCount);
    ADD_VERB(BIGSTRING("\phideapplication"), frv_hideApplication);
    ADD_VERB(BIGSTRING("\pisvalidserialnumber"), frv_isValidSerialNumber);
    ADD_VERB(BIGSTRING("\pshowapplication"), frv_showApplication);

    #undef ADD_VERB

    pophashtable();
    return true;
}
```

### Integration Point

**File**: `frontier-cli/main.c`

Add after database is successfully opened:

```c
/* In main(), after dbopen() succeeds */
if (!dbopen(&fspec, false, nil)) {
    fprintf(stderr, "Failed to open database\n");
    return 1;
}

/* NEW: Register database path with frontier verbs */
extern boolean headless_set_frontier_file(tyfilespec *fs);
if (!headless_set_frontier_file(&fspec)) {
    fprintf(stderr, "Warning: Failed to set frontier file path\n");
}

/* Continue with script execution... */
```

### Testing

```bash
# Build
make -C frontier-cli

# Test
./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root \
    -e "msg(frontier.getFilePath())"

# Expected output: path to database file
```

## Implementation 2: file.folderFromPath

### Current Error
```
[ERROR] Verb not implemented: file.folderFromPath
[ERROR] Execution failed at token 24
```

### Verb Signature
```
file.folderFromPath(path: string) → string
```

**Purpose**: Extracts the folder/directory portion from a file path.

**Examples**:
- `/Users/test/file.txt` → `/Users/test`
- `/Users/test/folder/` → `/Users/test`
- `file.txt` → `` (empty string)
- `/` → `/`

### Legacy Implementation Analysis

**Location**: `Common/source/file.c` line ~800 (approximately)

Legacy implementation uses Mac-specific path handling with `:` as separator.

**Key Challenge**: Need to handle POSIX paths with `/` separator in headless mode.

### Headless Implementation

**Step 1**: Create platform abstraction header

**File**: `Common/headers/file_portable.h` (NEW)

```c
#ifndef __FILE_PORTABLE_H__
#define __FILE_PORTABLE_H__

#include "frontier.h"

/*
 * Platform-independent file path operations.
 *
 * These functions handle path parsing and manipulation
 * for different platforms (POSIX, Windows).
 */

/* Extract folder/directory from path */
boolean portable_folderfrompath(const bigstring bspath, bigstring bsfolder);

/* Extract filename from path */
boolean portable_filefrompath(const bigstring bspath, bigstring bsfile);

/* Get path separator for current platform */
char portable_getpathsep(void);

/* Split path into components */
boolean portable_splitpath(const bigstring bspath, bigstring bsfolder,
                          bigstring bsfile);

#endif /* __FILE_PORTABLE_H__ */
```

**Step 2**: Implement POSIX version

**File**: `Common/source/file_portable_posix.c` (NEW)

```c
#include "frontier.h"
#include "standard.h"
#include "strings.h"
#include "file_portable.h"

#ifdef __MACH__
#define PATH_SEP '/'
#define PATH_SEP_STR "/"
#else
#define PATH_SEP '/'
#define PATH_SEP_STR "/"
#endif

char portable_getpathsep(void) {
    return PATH_SEP;
}

boolean portable_folderfrompath(const bigstring bspath, bigstring bsfolder) {
    /*
     * Extract folder portion from path by finding last path separator
     * and returning everything before it.
     *
     * Examples:
     *   /Users/test/file.txt -> /Users/test
     *   /Users/test/         -> /Users
     *   file.txt             -> (empty)
     *   /                    -> /
     */

    short pathlen = stringlength(bspath);
    short i;
    short lastsep = -1;

    /* Empty path returns empty folder */
    if (pathlen == 0) {
        setemptystring(bsfolder);
        return true;
    }

    /* Find last path separator */
    for (i = pathlen; i >= 1; i--) {
        if (bspath[i] == PATH_SEP) {
            lastsep = i;
            break;
        }
    }

    /* No separator found - no folder component */
    if (lastsep == -1) {
        setemptystring(bsfolder);
        return true;
    }

    /* Root path "/" - return "/" */
    if (lastsep == 1 && pathlen == 1) {
        copystring(BIGSTRING("\p/"), bsfolder);
        return true;
    }

    /* Path ends with separator - find previous separator */
    if (lastsep == pathlen) {
        /* Look for previous separator */
        for (i = lastsep - 1; i >= 1; i--) {
            if (bspath[i] == PATH_SEP) {
                lastsep = i;
                break;
            }
        }

        /* If we found root, return it */
        if (lastsep == 1) {
            copystring(BIGSTRING("\p/"), bsfolder);
            return true;
        }
    }

    /* Copy everything before last separator */
    if (lastsep > 0) {
        copystring(bspath, bsfolder);
        setstringlength(bsfolder, lastsep - 1);
        return true;
    }

    setemptystring(bsfolder);
    return true;
}

boolean portable_filefrompath(const bigstring bspath, bigstring bsfile) {
    /*
     * Extract filename portion from path by finding last path separator
     * and returning everything after it.
     */

    short pathlen = stringlength(bspath);
    short i;
    short lastsep = 0;

    /* Empty path returns empty filename */
    if (pathlen == 0) {
        setemptystring(bsfile);
        return true;
    }

    /* Find last path separator */
    for (i = pathlen; i >= 1; i--) {
        if (bspath[i] == PATH_SEP) {
            lastsep = i;
            break;
        }
    }

    /* Path ends with separator - no filename */
    if (lastsep == pathlen) {
        setemptystring(bsfile);
        return true;
    }

    /* Copy everything after last separator */
    if (lastsep > 0) {
        short filelen = pathlen - lastsep;
        short j;

        setstringlength(bsfile, filelen);
        for (j = 1; j <= filelen; j++) {
            bsfile[j] = bspath[lastsep + j];
        }
        return true;
    }

    /* No separator - entire path is filename */
    copystring(bspath, bsfile);
    return true;
}

boolean portable_splitpath(const bigstring bspath, bigstring bsfolder,
                          bigstring bsfile) {
    if (!portable_folderfrompath(bspath, bsfolder))
        return false;
    if (!portable_filefrompath(bspath, bsfile))
        return false;
    return true;
}
```

**Step 3**: Wire into file verbs

**File**: `tests/headless_file_verbs.c` (MODIFY)

Add to token enum:
```c
enum {
    fv_open = 1,
    fv_close,
    fv_readLine,
    fv_read,
    fv_write,
    fv_setPosition,
    fv_getPosition,
    fv_setEndOfFile,
    fv_getEndOfFile,
    fv_endOfFile,
    // ... (tokens 11-23 not yet implemented)
    fv_folderFromPath = 24,    // ADD THIS
    fv_fileFromPath = 23       // ADD THIS (token 23)
};
```

Add to `fv_valueproc()` switch:
```c
#include "file_portable.h"  // Add at top of file

case fv_folderFromPath: {
    bigstring bspath, bsfolder;

    flnextparamislast = true;
    if (!getstringvalue(hparam1, 1, bspath))
        return false;

    if (!portable_folderfrompath(bspath, bsfolder))
        return false;

    return setstringvalue(bsfolder, v);
}

case fv_fileFromPath: {
    bigstring bspath, bsfile;

    flnextparamislast = true;
    if (!getstringvalue(hparam1, 1, bspath))
        return false;

    if (!portable_filefrompath(bspath, bsfile))
        return false;

    return setstringvalue(bsfile, v);
}
```

Add to verb registration in `fileinitverbs()`:
```c
ADD_VERB("fileFromPath", fv_fileFromPath);
ADD_VERB("folderFromPath", fv_folderFromPath);
```

### Testing

```bash
# Build
make -C frontier-cli

# Test folderFromPath
./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root \
    -e "msg(file.folderFromPath('/Users/test/file.txt'))"
# Expected: /Users/test

./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root \
    -e "msg(file.folderFromPath('/Users/test/'))"
# Expected: /Users

./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root \
    -e "msg(file.folderFromPath('file.txt'))"
# Expected: (empty string)

# Test fileFromPath
./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root \
    -e "msg(file.fileFromPath('/Users/test/file.txt'))"
# Expected: file.txt
```

### Unit Tests

**File**: `tests/test_file_portable.c` (NEW)

```c
#include <stdio.h>
#include <string.h>
#include "frontier.h"
#include "strings.h"
#include "file_portable.h"

static int test_count = 0;
static int test_passed = 0;

#define TEST(name, condition) do { \
    test_count++; \
    if (condition) { \
        test_passed++; \
        printf("✓ %s\n", name); \
    } else { \
        printf("✗ %s\n", name); \
    } \
} while(0)

void test_folderfrompath(void) {
    bigstring bspath, bsfolder;
    char *expected;

    printf("\nTesting portable_folderfrompath:\n");

    /* Test 1: Standard path */
    copyctopstring("/Users/test/file.txt", bspath);
    portable_folderfrompath(bspath, bsfolder);
    expected = "/Users/test";
    TEST("Standard path", strcmp(stringbaseaddress(bsfolder), expected) == 0);

    /* Test 2: Path ending with separator */
    copyctopstring("/Users/test/", bspath);
    portable_folderfrompath(bspath, bsfolder);
    expected = "/Users";
    TEST("Path with trailing slash", strcmp(stringbaseaddress(bsfolder), expected) == 0);

    /* Test 3: Filename only */
    copyctopstring("file.txt", bspath);
    portable_folderfrompath(bspath, bsfolder);
    TEST("Filename only", stringlength(bsfolder) == 0);

    /* Test 4: Root path */
    copyctopstring("/", bspath);
    portable_folderfrompath(bspath, bsfolder);
    expected = "/";
    TEST("Root path", strcmp(stringbaseaddress(bsfolder), expected) == 0);

    /* Test 5: Deep path */
    copyctopstring("/a/b/c/d/e/file.txt", bspath);
    portable_folderfrompath(bspath, bsfolder);
    expected = "/a/b/c/d/e";
    TEST("Deep path", strcmp(stringbaseaddress(bsfolder), expected) == 0);
}

void test_filefrompath(void) {
    bigstring bspath, bsfile;
    char *expected;

    printf("\nTesting portable_filefrompath:\n");

    /* Test 1: Standard path */
    copyctopstring("/Users/test/file.txt", bspath);
    portable_filefrompath(bspath, bsfile);
    expected = "file.txt";
    TEST("Standard path", strcmp(stringbaseaddress(bsfile), expected) == 0);

    /* Test 2: Path ending with separator */
    copyctopstring("/Users/test/", bspath);
    portable_filefrompath(bspath, bsfile);
    TEST("Path with trailing slash", stringlength(bsfile) == 0);

    /* Test 3: Filename only */
    copyctopstring("file.txt", bspath);
    portable_filefrompath(bspath, bsfile);
    expected = "file.txt";
    TEST("Filename only", strcmp(stringbaseaddress(bsfile), expected) == 0);

    /* Test 4: Root path */
    copyctopstring("/", bspath);
    portable_filefrompath(bspath, bsfile);
    TEST("Root path", stringlength(bsfile) == 0);
}

int main(void) {
    printf("File Portable Path Operations Tests\n");
    printf("====================================\n");

    test_folderfrompath();
    test_filefrompath();

    printf("\n====================================\n");
    printf("Results: %d/%d tests passed\n", test_passed, test_count);

    return (test_passed == test_count) ? 0 : 1;
}
```

**Makefile addition**:
```makefile
test_file_portable: tests/test_file_portable.c Common/source/file_portable_posix.c
	$(CC) $(CFLAGS) -I$(COMMON_HEADERS) -o tests/test_file_portable \
		tests/test_file_portable.c Common/source/file_portable_posix.c \
		Common/source/strings.c

.PHONY: test-portable
test-portable: test_file_portable
	./tests/test_file_portable
```

## Build System Changes

### Makefile Updates

**File**: `frontier-cli/Makefile`

Add new source files:
```makefile
# Add to SOURCES
SOURCES += ../Common/source/file_portable_posix.c
SOURCES += ../tests/headless_frontier_verbs.c

# Ensure file_portable.h is in include path
INCLUDES += -I../Common/headers
```

## Verification Checklist

After implementation:

### Build Verification
- [ ] `make clean && make` succeeds
- [ ] No linker errors
- [ ] No compiler warnings

### Functionality Verification
- [ ] `frontier.getFilePath()` returns database path
- [ ] `file.folderFromPath()` handles standard paths
- [ ] `file.folderFromPath()` handles trailing slashes
- [ ] `file.folderFromPath()` handles root path
- [ ] `file.folderFromPath()` handles filename-only

### Integration Verification
- [ ] `system.startup` scripts execute without errors
- [ ] `clock.now()` test passes
- [ ] No new UI dependencies introduced

### Testing Verification
- [ ] Unit tests pass (`make test-portable`)
- [ ] Integration tests pass
- [ ] Memory leak check passes (Valgrind/ASAN)

## Next Steps After These Two Verbs

Once these are working:

1. **Run system.startup** and capture any additional missing verb errors
2. **Prioritize next batch** based on actual startup script requirements
3. **Continue with Week 1 plan**: String verbs, date verbs, lang type conversions

## Known Issues & Notes

### Issue 1: Case Sensitivity
UserTalk is case-insensitive, but verb registration might be case-sensitive.
Watch for:
- `getFilePath` vs `getfilepath`
- `folderFromPath` vs `folderfrompath`

**Solution**: Test both cases, ensure verb registration matches resource file exactly.

### Issue 2: Path Conventions
Legacy Frontier used Mac paths with `:` separator.
Scripts in the database might have hardcoded Mac paths.

**Solution**: May need path translation layer for legacy scripts. Cross that bridge when we come to it.

### Issue 3: filespec Type
The `frontier.getFilePath()` returns a filespec, which is a complex type including volume refs, directory IDs, etc.

**Solution**: Current portable filespec only stores path string. Should be sufficient for headless mode.

## Success Criteria

Implementation is successful when:

```bash
./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root \
    -e "clock.now()"
```

Executes without errors and returns a valid timestamp.

---

**Document Version**: 1.0
**Created**: 2025-12-03
**Status**: Implementation Ready
**Next Action**: Begin implementation of frontier.getFilePath
