# Kernel Verb Porting Technical Guide
## Comprehensive Technical Analysis for Headless Runtime Migration

**Document Version:** 1.0
**Date:** 2025-12-03
**Author:** Legacy C Application Expert Agent
**Purpose:** Technical reference for porting Frontier kernel verbs from legacy Carbon/UI runtime to headless portable runtime

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Legacy Verb Implementation Architecture](#legacy-verb-implementation-architecture)
3. [Specific Verb Analysis](#specific-verb-analysis)
4. [UI/Carbon Dependency Catalog](#uicarbon-dependency-catalog)
5. [Porting Cookbook](#porting-cookbook)
6. [Code Reuse Matrix](#code-reuse-matrix)
7. [Platform-Specific Path Handling](#platform-specific-path-handling)
8. [Testing Strategy](#testing-strategy)

---

## Executive Summary

This document provides detailed technical analysis for porting Frontier kernel verbs from the legacy Mac-centric implementation to the headless portable runtime. The analysis focuses on the two currently failing verbs (`frontier.getFilePath` and `file.folderFromPath`) while establishing patterns applicable to all kernel verb migrations.

### Critical Constraint

**ABSOLUTE REQUIREMENT:** The headless runtime MUST NOT take ANY UI or Carbon dependencies. All code must be completely clean, portable, and testable without a window system.

### Key Findings

1. **String-based path operations** (like `file.folderFromPath`) are **100% portable** and can be copied directly
2. **Window/shell-dependent verbs** (like `frontier.getFilePath`) require **adapter patterns** or **configuration-based alternatives**
3. **Most file path manipulation functions** in `fileops.m` and `strings.c` are already portable
4. **Carbon FSRef/FSSpec APIs** have clean separation points where portable alternatives can be substituted

---

## Legacy Verb Implementation Architecture

### How Kernel Verbs Are Registered

Kernel verbs in Frontier use a **function processor** pattern with token-based dispatch:

```c
// From shellsysverbs.c, lines 79-114
typedef enum tyfrontiertoken {
    programpathfunc,    // token 0
    filepathfunc,       // token 1 - frontier.getFilePath
    agentsenablefunc,   // token 2
    // ... more tokens
    ctfrontierverbs     // count
} tyfrontiertoken;

// Registration (line 1430)
boolean sysinitverbs(void) {
    if (!loadfunctionprocessor(idfrontierverbs, &frontierfunctionvalue))
        return (false);
    // ... registers other processors
    return (true);
}

// Dispatch handler (lines 684-854)
static boolean frontierfunctionvalue(
    short token,              // which verb: 0, 1, 2, etc.
    hdltreenode hparam1,      // parameter tree
    tyvaluerecord *vreturned, // result
    bigstring bserror         // error message out
) {
    switch (token) {
        case filepathfunc:
            // implementation here
            break;
        // ... other cases
    }
}
```

### File Verb Architecture

File verbs follow the same pattern in `fileverbs.c`:

```c
// fileverbs.c, lines 60-252
typedef enum tyfiletoken {
    filecreatedfunc,        // token 0
    filemodifiedfunc,       // token 1
    // ... many tokens ...
    filefrompathfunc,       // token 108
    folderfrompathfunc,     // token 110 - file.folderFromPath
    // ... more tokens
    ctfileverbs             // count
} tyfiletoken;
```

### Common Verb Implementation Patterns

**Pattern 1: Parameter Extraction**
```c
// Get required parameters
if (!getparamvalue(hparam1, 1, &v))
    return (false);

// Mark last parameter
flnextparamislast = true;
if (!getstringvalue(hparam1, 2, bs))
    return (false);
```

**Pattern 2: Return Value Setting**
```c
// Boolean result
setbooleanvalue(true, v);
return (true);

// String result
return (setstringvalue(bs, v));

// Filespec result
return (setfilespecvalue(&fs, v));

// Long result
return (setlongvalue(count, v));
```

**Pattern 3: Error Handling**
```c
// Check parameter count
if (!langcheckparamcount(hparam1, 0))
    return (false);

// OS error
if (oserror(err))
    return (false);

// Custom error message
getstringlist(langerrorlist, unimplementedverberror, bserror);
return (false);
```

---

## Specific Verb Analysis

### Case Study 1: `frontier.getFilePath` (Token 1)

**Location:** `/Users/jake/dev/jsavin/Frontier/Common/source/shellsysverbs.c`, lines 721-736

**Legacy Implementation:**
```c
case filepathfunc: {
    tyfilespec fs;

    if (!langcheckparamcount(hparam1, 0))
        return (false);

    // UI DEPENDENCY: requires shell window globals
    shellpushfrontrootglobals();

    windowgetfspec(shellwindow, &fs);  // <-- PROBLEM: needs window system

    shellpopglobals();

    return (setfilespecvalue(&fs, v));
}
```

**Dependencies Identified:**
1. `shellpushfrontrootglobals()` - pushes shell window context onto global stack
2. `shellwindow` - global variable pointing to the current shell window
3. `windowgetfspec()` - extracts file spec from window data structure
4. `shellpopglobals()` - restores previous global context

**Why This Fails in Headless:**
- No window system means `shellwindow` is NULL or undefined
- Window data structures don't exist in headless mode
- The entire concept of "current database file from window" is UI-centric

**Portable Alternative Approaches:**

**Option A: Configuration-based (RECOMMENDED)**
```c
// Headless implementation
case filepathfunc: {
    tyfilespec fs;

    if (!langcheckparamcount(hparam1, 0))
        return (false);

    #ifdef FRONTIER_HEADLESS
        // Use the database path from command-line or config
        extern tyfilespec headless_database_fspec;
        fs = headless_database_fspec;
    #else
        shellpushfrontrootglobals();
        windowgetfspec(shellwindow, &fs);
        shellpopglobals();
    #endif

    return (setfilespecvalue(&fs, v));
}
```

**Option B: Runtime State (Alternative)**
```c
// Store database path at startup in a headless global
static tyfilespec g_headless_database_path;

boolean headless_set_database_path(const char *path) {
    bigstring bs;
    copyctopstring(path, bs);
    return pathtofilespec(bs, &g_headless_database_path);
}

// Then in verb:
#ifdef FRONTIER_HEADLESS
    fs = g_headless_database_path;
#endif
```

**Implementation Location:**
- Add to: `tests/headless_shellsysverbs.c` (new file)
- Or extend: `tests/headless_shell.c`

---

### Case Study 2: `file.folderFromPath` (Token 110)

**Location:** `/Users/jake/dev/jsavin/Frontier/Common/source/fileverbs.c`, lines 488-538

**Legacy Implementation:**
```c
static boolean folderfrompathverb(hdltreenode hparam1, tyvaluerecord *vreturned) {
    tyvaluerecord v;
    bigstring bs;

    flnextparamislast = true;

    if (!getparamvalue(hparam1, 1, &v))
        return (false);

    // BRANCH 1: If not a string, work with FSRef (Mac-specific)
    if (v.valuetype != stringvaluetype) {
        tyfilespec fs, fsparent;
        OSErr err;

        if (!coercetofilespec(&v))
            return (false);

        fs = **v.data.filespecvalue;

        err = macgetfilespecparent(&fs, &fsparent);  // Mac Carbon API

        if (err != noErr) {
            oserror(err);
            return (false);
        }

        return (setfilespecvalue(&fsparent, vreturned));
    }

    // BRANCH 2: String manipulation (PORTABLE!)
    if (!coercetostring(&v))
        return (false);

    pullstringvalue(&v, bs);

    cleanendoffilename(bs);        // Remove trailing path separator

    folderfrompath(bs, bs);        // Extract folder portion

    return (setstringvalue(bs, vreturned));
}
```

**Dependencies Identified:**
1. **Branch 1 (FSRef)**: Mac-specific Carbon File Manager APIs
   - `macgetfilespecparent()` - gets parent directory from FSRef
   - Requires Carbon framework

2. **Branch 2 (String)**: 100% portable string manipulation
   - `cleanendoffilename()` - removes trailing colon/slash
   - `folderfrompath()` - extracts folder from path string

**String Helper Functions (PORTABLE):**

```c
// From fileops.m, line 153
boolean cleanendoffilename(bigstring bs) {
    if (endswithpathsep(bs)) {
        setstringlength(bs, stringlength(bs) - 1);
        return (true);
    }
    return (false);
}

// From fileops.m, line 2251
boolean folderfrompath(bigstring path, bigstring folder) {
    // Returns everything left of the last path separator
    // Example: "Work Disk #1:MORE Work:Status Center"
    //       -> "Work Disk #1:MORE Work:"

    bigstring bs;

    filefrompath(path, bs);  // Get filename portion

    copystring(path, folder);

    // Truncate to remove filename
    setstringlength(folder, stringlength(folder) - stringlength(bs));

    return (true);
}

// From fileops.m, line 2238
boolean filefrompath(bigstring path, bigstring fname) {
    // Returns everything right of the last path separator
    // Example: "Work Disk #1:MORE Work:Status Center"
    //       -> "Status Center"

    return (lastword(path, chpathseparator, fname));
}

// From strings.c, line 787
boolean lastword(bigstring bssource, byte chdelim, bigstring bsdest) {
    return (textlastword(
        stringbaseaddress(bssource),
        stringlength(bssource),
        chdelim,
        bsdest
    ));
}
```

**Path Separator Handling:**
```c
// From file.h, lines 87-93
#if defined(FRONTIER_HEADLESS)
    #define chpathseparator '/'    // Unix-style for headless
#else
    #define chpathseparator ':'    // Mac classic style
#endif
```

**Portable Implementation Strategy:**

```c
// Headless version - KEEP string branch, stub FSRef branch
static boolean folderfrompathverb(hdltreenode hparam1, tyvaluerecord *vreturned) {
    tyvaluerecord v;
    bigstring bs;

    flnextparamislast = true;

    if (!getparamvalue(hparam1, 1, &v))
        return (false);

    #ifdef FRONTIER_HEADLESS
        // Headless: force everything through string path
        if (!coercetostring(&v))
            return (false);

        pullstringvalue(&v, bs);
        cleanendoffilename(bs);
        folderfrompath(bs, bs);

        return (setstringvalue(bs, vreturned));
    #else
        // Original dual-path implementation for GUI
        if (v.valuetype != stringvaluetype) {
            // ... FSRef branch
        }
        // ... string branch
    #endif
}
```

**Key Insight:** The string manipulation code is **100% portable** and already works correctly! We just need to ensure the headless path always uses forward slashes (`/`) instead of colons (`:`).

---

## UI/Carbon Dependency Catalog

### Category 1: Window/Shell Dependencies

**Pattern:** Verbs that query the current UI state or frontmost window

| Function/Global | Purpose | Headless Alternative |
|----------------|---------|---------------------|
| `shellwindow` | Current shell window | Configuration global |
| `shellpushfrontrootglobals()` | Save/restore UI context | No-op or stub |
| `shellpopglobals()` | Restore UI context | No-op or stub |
| `windowgetfspec()` | Get filespec from window | Return config path |
| `shellisactive()` | Is app frontmost? | Always return `true` |

**Example Stub Pattern:**
```c
// In headless_shell.c
#ifdef FRONTIER_HEADLESS
void shellpushfrontrootglobals(void) {
    // No-op in headless mode
}

void shellpopglobals(void) {
    // No-op in headless mode
}
#endif
```

---

### Category 2: Dialog Box Dependencies

**Pattern:** Verbs that show UI dialogs for user input

| Verb | Token | Legacy Behavior | Headless Strategy |
|------|-------|----------------|-------------------|
| `dialog.alert()` | 280 | Show alert dialog | Print to stderr, return false |
| `dialog.ask()` | 311 | Get user string input | Return error or use stdin |
| `dialog.twoWay()` | 307 | Yes/No dialog | Return default or error |
| `dialog.notify()` | 315 | Notification dialog | Log to stderr |

**Stub Example:**
```c
// From langverbs.c
case alertdialogfunc:
    #ifdef FRONTIER_HEADLESS
        // Print message to stderr
        bigstring bs;
        if (!getstringvalue(hparam1, 1, bs))
            return (false);
        fprintf(stderr, "ALERT: %s\n", stringbaseaddress(bs));
        return (setbooleanvalue(true, v));
    #else
        // Original GUI dialog code
        return (langdialogverb(...));
    #endif
```

---

### Category 3: File System UI Dependencies

**Pattern:** Mac Carbon File Manager APIs vs. portable POSIX

| Mac Carbon API | Purpose | Portable Alternative |
|---------------|---------|---------------------|
| `FSRef` | File reference | String paths + `stat()` |
| `FSSpec` | File specification | String paths |
| `FSMakeFSSpec()` | Create file spec | `pathtofilespec()` (already abstracted) |
| `FSGetCatalogInfo()` | File metadata | `stat()`, `lstat()` |
| `macgetfilespecparent()` | Get parent dir | String manipulation: `folderfrompath()` |
| `FSRefMakePath()` | FSRef to path | Already handled in `filespectopath()` |

**Good News:** Frontier already has an abstraction layer!

The `tyfilespec` structure (in `shelltypes.h`) is designed to be cross-platform:
```c
typedef struct tyfilespec {
    FSRef ref;              // Mac: actual FSRef
    tyfsname name;          // Cross-platform: filename
    struct {
        boolean flvolume;   // Is this a volume?
        // ... flags
    } flags;
} tyfilespec;
```

Many file operations in `fileops.m` and `filepath.c` already work through this abstraction.

---

### Category 4: Event Loop Dependencies

**Pattern:** Verbs that process UI events

| Function | Purpose | Headless Strategy |
|----------|---------|-------------------|
| `shellpartialeventloop()` | Process window events | No-op |
| `langpartialeventloop()` | Process script events | Keep (needed for threads) |
| `processyield()` | Yield to OS | Keep (needed for cooperative multitasking) |
| `langbackgroundtask()` | Background processing | Keep (needed for scripts) |

**Note:** `processyield()` and threading support should be **retained** even in headless mode - they're needed for concurrent script execution.

---

### Category 5: Resource Fork Dependencies

**Pattern:** Mac-specific resource operations

| Verb | Token | Headless Strategy |
|------|-------|-------------------|
| `rez.getResource()` | 256 | Return error (resources not supported) |
| `rez.putResource()` | 257 | Return error |
| Resource-based dialogs | Various | Pre-parse or hardcode |

**Modern Reality:** Resource forks are obsolete. Most headless use cases won't need them.

---

## Porting Cookbook

### Recipe 1: Port a Pure String Manipulation Verb

**Steps:**
1. Identify the verb in the legacy source (e.g., `fileverbs.c`)
2. Check if it uses only string operations (no FSRef, no dialogs, no windows)
3. Copy the implementation directly to headless file verb handler
4. Ensure path separator is `/` not `:`
5. Test with sample inputs

**Example: `file.fileFromPath`**

```c
// Legacy code (fileverbs.c, lines 429-485) - ALREADY PORTABLE!
static boolean filefrompathverb(hdltreenode hparam1, tyvaluerecord *vreturned) {
    tyvaluerecord v;
    tyfilespec fs;
    bigstring bs;
    boolean flfolder;

    flnextparamislast = true;

    if (!getparamvalue(hparam1, 1, &v))
        return (false);

    switch (v.valuetype) {
        case stringvaluetype:
            pullstringvalue(&v, bs);
            flfolder = endswithpathsep(bs);
            if (flfolder)
                setstringlength(bs, stringlength(bs) - 1);

            filefrompath(bs, bs);  // PORTABLE!
            break;

        default:
            // FSRef branch - skip in headless
            // ... coerce to filespec, extract name
            break;
    }

    if (flfolder)
        pushchar(chpathseparator, bs);

    return (setstringvalue(bs, vreturned));
}
```

**Headless Version:**
```c
// In headless_file_verbs.c
case fv_fileFromPath: {
    bigstring bs;
    boolean flfolder;

    flnextparamislast = true;

    if (!getstringvalue(hparam1, 1, bs))
        return (false);

    flfolder = endswithpathsep(bs);
    if (flfolder)
        setstringlength(bs, stringlength(bs) - 1);

    filefrompath(bs, bs);

    if (flfolder)
        pushchar(chpathseparator, bs);

    return (setstringvalue(bs, v));
}
```

---

### Recipe 2: Stub a Dialog-Heavy Verb

**Steps:**
1. Identify what the verb returns in success case
2. Decide on headless behavior: error, default value, or parameter
3. Implement minimal logic to return that value
4. Document the limitation

**Example: `dialog.ask`**

```c
// Legacy: shows dialog, gets user string input
// Headless: return error or read from stdin (advanced)

case askdialogfunc: {
    #ifdef FRONTIER_HEADLESS
        // Headless: can't show dialog, return error
        bigstring bsprompt;

        if (!getstringvalue(hparam1, 1, bsprompt))
            return (false);

        // Log what would have been asked
        fprintf(stderr, "DIALOG: %s (headless mode, returning empty)\n",
                stringbaseaddress(bsprompt));

        // Return empty string
        return (setstringvalue(emptystring, v));
    #else
        // Original GUI implementation
        return (langdialogask(...));
    #endif
}
```

---

### Recipe 3: Adapt a Window-Dependent Verb

**Steps:**
1. Identify what state the verb queries from the window
2. Store that state in a headless global or config
3. Return the configured value instead of querying window
4. Initialize the global at startup

**Example: `frontier.getFilePath`**

```c
// Step 1: Add global in headless_shell.c
static tyfilespec g_headless_root_database;

boolean headless_init_database_path(const char *path) {
    bigstring bs;
    copyctopstring(path, bs);
    return pathtofilespec(bs, &g_headless_root_database);
}

// Step 2: Implement verb
case filepathfunc: {
    #ifdef FRONTIER_HEADLESS
        if (!langcheckparamcount(hparam1, 0))
            return (false);

        return (setfilespecvalue(&g_headless_root_database, v));
    #else
        // Original window-based code
        shellpushfrontrootglobals();
        windowgetfspec(shellwindow, &fs);
        shellpopglobals();
        return (setfilespecvalue(&fs, v));
    #endif
}

// Step 3: Initialize at startup in main.c
int main(int argc, char *argv[]) {
    // ... parse args ...
    headless_init_database_path(database_path);
    // ... continue startup ...
}
```

---

### Recipe 4: Port a File Operation Verb with FSRef

**Steps:**
1. Identify if the operation can be done with string paths
2. Use POSIX APIs (`stat`, `readdir`, etc.) instead of Carbon
3. Convert between `tyfilespec` and string paths using existing helpers
4. Test on actual filesystem

**Example: `file.getFileInfo`**

```c
// Portable approach using stat()
case filegetinfofunc: {
    tyfilespec fs;
    struct stat st;
    bigstring path;

    if (!getpathvalue(hparam1, 1, &fs))
        return (false);

    #ifdef FRONTIER_HEADLESS
        // Convert filespec to Unix path
        if (!filespectopath(&fs, path))
            return (false);

        // Use POSIX stat
        char cpath[256];
        convertpstring(path);
        if (stat((char*)path, &st) != 0) {
            oserror(errno);
            return (false);
        }

        // Extract info and return as record
        // ... populate return record with st.st_size, st.st_mtime, etc.
    #else
        // Use Mac Carbon FSGetCatalogInfo
        // ... original code
    #endif
}
```

---

## Code Reuse Matrix

This table shows which legacy code can be reused in headless runtime:

| Source File | Functions | Portability | Strategy |
|------------|-----------|-------------|----------|
| `strings.c` | `lastword()`, `firstword()`, `nthword()` | 100% portable | Copy as-is |
| `fileops.m` | `filefrompath()`, `folderfrompath()` | 100% portable | Copy as-is |
| `fileops.m` | `cleanendoffilename()`, `endswithpathsep()` | 100% portable | Copy as-is |
| `filepath.c` | `pathtofilespec()` | Has Mac branch | Use portable parts |
| `filepath.c` | `filespectopath()` | Has Mac branch | Use portable parts |
| `fileverbs.c` | String path verbs | 95% portable | Minor edits for `/` vs `:` |
| `fileverbs.c` | FSRef-based verbs | Mac-specific | Rewrite with POSIX |
| `shellsysverbs.c` | `frontier.getFilePath` | Window-dependent | Use config alternative |
| `langverbs.c` | Dialog verbs | UI-dependent | Stub or stdin/stderr |
| `langverbs.c` | Thread verbs | Portable | Copy as-is |
| `langverbs.c` | Date/time verbs | Portable | Copy as-is |

---

## Platform-Specific Path Handling

### Mac Classic vs. Unix Paths

**Mac Classic Path:**
```
Macintosh HD:Users:jake:Documents:file.txt
^           ^                      ^
volume      folders                file
```

**Unix Path:**
```
/Users/jake/Documents/file.txt
^     ^              ^
root  folders        file
```

### Path Separator Configuration

```c
// file.h, lines 87-93
#if defined(FRONTIER_HEADLESS)
    #define chpathseparator '/'
#else
    #define chpathseparator ':'
#endif
```

**This means:** All the string functions (`filefrompath`, `folderfrompath`, etc.) automatically work with Unix paths in headless mode!

### Converting Between Formats

```c
// Helper to convert Mac path to Unix path
void macpathtounixpath(bigstring bs) {
    stringswapall(':', '/', bs);

    // Handle volume name
    if (bs[1] != '/') {
        // "Macintosh HD:Users:..." -> "/Volumes/Macintosh HD/Users/..."
        // But in headless, we typically use absolute Unix paths already
    }
}

// Helper to convert Unix path to Mac path (if needed for compatibility)
void unixpathtomacpath(bigstring bs) {
    stringswapall('/', ':', bs);

    // "/Users/jake/..." -> "Macintosh HD:Users:jake:..."
    // Would need to add volume name
}
```

**In Practice:** For headless, we should standardize on **Unix absolute paths** everywhere and not try to maintain Mac-style paths.

---

## Testing Strategy

### Unit Tests for Ported Verbs

**Test File Structure:**
```
tests/
  test_headless_file_verbs.c    - File path verb tests
  test_headless_sys_verbs.c     - System verb tests
  test_headless_lang_verbs.c    - Language verb tests
```

**Sample Test:**
```c
// test_headless_file_verbs.c
void test_folderFromPath_unixPath(void) {
    bigstring input, expected, result;

    copyctopstring("/Users/jake/Documents/file.txt", input);
    copyctopstring("/Users/jake/Documents/", expected);

    folderfrompath(input, result);

    assert(equalstrings(result, expected));
}

void test_fileFromPath_unixPath(void) {
    bigstring input, expected, result;

    copyctopstring("/Users/jake/Documents/file.txt", input);
    copyctopstring("file.txt", expected);

    filefrompath(input, result);

    assert(equalstrings(result, expected));
}

void test_folderFromPath_withTrailingSlash(void) {
    bigstring input, expected, result;

    copyctopstring("/Users/jake/Documents/", input);
    copyctopstring("/Users/jake/", expected);

    // Clean trailing slash first
    cleanendoffilename(input);
    folderfrompath(input, result);

    assert(equalstrings(result, expected));
}
```

### Integration Tests

**Test Script:**
```bash
#!/bin/bash
# test_file_verbs.sh

# Test file.folderFromPath
result=$(./frontier-cli -e 'file.folderFromPath("/home/user/docs/file.txt")')
expected="/home/user/docs/"
if [ "$result" != "$expected" ]; then
    echo "FAIL: folderFromPath"
    exit 1
fi

# Test file.fileFromPath
result=$(./frontier-cli -e 'file.fileFromPath("/home/user/docs/file.txt")')
expected="file.txt"
if [ "$result" != "$expected" ]; then
    echo "FAIL: fileFromPath"
    exit 1
fi

# Test frontier.getFilePath
result=$(./frontier-cli --system-root /path/to/test.root -e 'frontier.getFilePath()')
expected="/path/to/test.root"
if [ "$result" != "$expected" ]; then
    echo "FAIL: getFilePath"
    exit 1
fi

echo "All tests passed"
```

---

## Implementation Checklist

### Phase 1: String Path Verbs (1-2 hours)

- [ ] Create `tests/headless_file_verbs_extended.c`
- [ ] Add `file.folderFromPath` implementation
- [ ] Add `file.fileFromPath` implementation
- [ ] Add token registration for these verbs
- [ ] Write unit tests
- [ ] Test with real Unix paths

### Phase 2: System Verbs (2-3 hours)

- [ ] Create `tests/headless_sys_verbs.c`
- [ ] Add global for database path
- [ ] Add `headless_init_database_path()` function
- [ ] Implement `frontier.getFilePath` using config
- [ ] Call init function from `main.c`
- [ ] Test verb returns correct path

### Phase 3: File Metadata Verbs (4-6 hours)

- [ ] Port `file.exists()` using `stat()`
- [ ] Port `file.size()` using `stat()`
- [ ] Port `file.modified()` using `stat()`
- [ ] Port `file.created()` using `stat()`
- [ ] Test all file metadata operations

### Phase 4: Dialog Stubs (1-2 hours)

- [ ] Stub `dialog.alert()` - print to stderr
- [ ] Stub `dialog.ask()` - return empty or error
- [ ] Stub `dialog.notify()` - print to stderr
- [ ] Document limitations in user guide

### Phase 5: Documentation

- [ ] Update `DEVELOPER_QUICKSTART_HEADLESS.md`
- [ ] Add "Supported Verbs" section to docs
- [ ] Add "Unsupported Verbs" section to docs
- [ ] Create migration guide for scripts

---

## Appendix A: Quick Reference

### Essential Portable Functions

```c
// String manipulation (strings.c)
boolean lastword(bigstring source, byte delim, bigstring dest);
boolean firstword(bigstring source, byte delim, bigstring dest);
boolean nthword(bigstring bs, short wordnum, byte delim, bigstring word);

// Path manipulation (fileops.m)
boolean filefrompath(bigstring path, bigstring fname);
boolean folderfrompath(bigstring path, bigstring folder);
boolean cleanendoffilename(bigstring bs);
boolean endswithpathsep(bigstring bs);

// Path conversion (filepath.c)
boolean pathtofilespec(bigstring path, ptrfilespec fs);
boolean filespectopath(const ptrfilespec fs, bigstring path);

// Value operations (lang.c)
boolean getstringvalue(hdltreenode hparam, short pnum, bigstring bs);
boolean getlongvalue(hdltreenode hparam, short pnum, long *val);
boolean getbooleanvalue(hdltreenode hparam, short pnum, boolean *val);
boolean setstringvalue(bigstring bs, tyvaluerecord *v);
boolean setlongvalue(long val, tyvaluerecord *v);
boolean setbooleanvalue(boolean val, tyvaluerecord *v);
```

### Common Error Patterns

```c
// Parameter count check
if (!langcheckparamcount(hparam1, 2))
    return (false);

// Mark last parameter (for better error messages)
flnextparamislast = true;

// File not found
filenotfounderror(bs);
return (false);

// OS error
if (oserror(err))
    return (false);

// Unimplemented
getstringlist(langerrorlist, unimplementedverberror, bserror);
return (false);
```

---

## Appendix B: File Locations

### Legacy Implementation Files

- **System verbs:** `/Users/jake/dev/jsavin/Frontier/Common/source/shellsysverbs.c`
- **File verbs:** `/Users/jake/dev/jsavin/Frontier/Common/source/fileverbs.c`
- **Language verbs:** `/Users/jake/dev/jsavin/Frontier/Common/source/langverbs.c`
- **Path functions:** `/Users/jake/dev/jsavin/Frontier/Common/source/fileops.m`
- **Path conversion:** `/Users/jake/dev/jsavin/Frontier/Common/source/filepath.c`
- **String functions:** `/Users/jake/dev/jsavin/Frontier/Common/source/strings.c`

### Headless Stubs

- **File verbs (minimal):** `/Users/jake/dev/jsavin/Frontier/tests/headless_file_verbs.c`
- **Language verbs (stub):** `/Users/jake/dev/jsavin/Frontier/tests/headless_langverbs_stub.c`
- **Shell adapter:** `/Users/jake/dev/jsavin/Frontier/tests/headless_shell.c`

### Headers

- **File API:** `/Users/jake/dev/jsavin/Frontier/Common/headers/file.h`
- **Language API:** `/Users/jake/dev/jsavin/Frontier/Common/headers/lang.h`
- **Shell API:** `/Users/jake/dev/jsavin/Frontier/Common/headers/shell.h`

---

## Conclusion

The majority of Frontier's kernel verb infrastructure is **already portable** or can be made portable with minimal effort. The key challenges are:

1. **Window/UI dependencies** - solve with configuration globals
2. **Dialog dependencies** - stub to stderr or return errors
3. **FSRef/Carbon APIs** - replace with POSIX equivalents

String-based path manipulation works **identically** in headless mode once the path separator is switched from `:` to `/`.

**Next Steps:**
1. Start with string path verbs (easiest, high value)
2. Add configuration support for window-dependent verbs
3. Progressively expand verb coverage based on actual script needs
4. Maintain strict "no UI dependencies" discipline

**Success Criteria:**
- `frontier.getFilePath()` returns correct database path
- `file.folderFromPath()` works with Unix paths
- Test suite validates all ported verbs
- No Carbon/Cocoa symbols in headless binary
