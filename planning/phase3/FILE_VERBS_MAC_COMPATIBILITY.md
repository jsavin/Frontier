# File Verbs: Mac Compatibility and Cross-Platform Implementation

**Status:** Implementation In Progress
**Phase:** Phase 3 (Headless Bring-Up)
**Created:** 2026-01-01
**Owner:** File verb binding workstream

---

## Overview

Frontier's file processor has 86 verbs, many of which are Mac-specific. This document defines the cross-platform implementation strategy for file verbs in headless mode, including which verbs work on all platforms, which are Mac-only, and which need conditional compilation.

**Related Documents:**
- [File Processor Audit](processor_audits/file.md) - Complete verb categorization
- [Headless Interactive Mode](HEADLESS_INTERACTIVE_MODE.md) - Dialog verb strategy
- [Headless Adaptations](../../docs/HEADLESS_ADAPTATIONS.md) - General patterns

---

## Table of Contents

1. [Implementation Summary](#implementation-summary)
2. [Cross-Platform Implementations](#cross-platform-implementations)
3. [Mac-Only Implementations](#mac-only-implementations)
4. [Conditional Compilation Strategy](#conditional-compilation-strategy)
5. [Testing Strategy](#testing-strategy)
6. [P1/P2 Enhancement Issues](#p1p2-enhancement-issues)

---

## Implementation Summary

### Verb Categories by Platform Support

| Category | Count | Strategy | Implementation |
|----------|-------|----------|----------------|
| **Core Primitives** | 37 | ✅ Cross-platform | POSIX file operations (already in file_portable.c) |
| **Script Helpers** | ~8 | ✅ Cross-platform | UserTalk scripts (auto-work once primitives exist) |
| **Volume Operations** | 12 | ✅ Cross-platform | POSIX statfs/statvfs |
| **Dialog Verbs** | 4 | ⚠️ Phase 2 | Error now, stdio prompts in Phase 2 (see HEADLESS_INTERACTIVE_MODE.md) |
| **Type/Creator Getters** | 2 | ✅ Cross-platform | Extension-based (this doc) |
| **Type/Creator Setters** | 2 | ❌ Stub with error | P2: Implement by renaming extension |
| **Aliases** | 3 | 🍎 Mac-only | `#ifdef __APPLE__` (symlink verbs are separate P2) |
| **Bundles** | 2 | ✅ Partial | hasbundle: suffix check; setbundle: error |
| **Visibility** | 2 | ✅ Stub | isvisible: always true; setvisible: return false |
| **Resource Fork** | 1 | ❌ Stub with error | Obsolete (P2 issue for extended attributes) |
| **Launch** | 1 | ❌ Stub with error | P1 issue for cross-platform solution |
| **Find Application** | 1 | ❌ Stub with error | P1 issue for cross-platform solution |
| **Versions** | 4 | 🍎 Mac-only | P1 issue for Windows PE resources |
| **Comments** | 2 | 🍎 Mac-only | P2 issue for extended attributes |
| **Icons** | 2 | 🍎 Mac-only | P2 issue |
| **Labels** | 5 | 🍎 Mac-only | P2 issue for extended attributes |

**Legend:**
- ✅ Cross-platform - Works on all platforms
- 🍎 Mac-only - `#ifdef __APPLE__`, error on non-Mac
- ⚠️ Phased - Phase 1: error, Phase 2: implement
- ❌ Stub with error - Runtime error, filed issue for future

---

## Cross-Platform Implementations

### 1. Type/Creator Getters (2 verbs)

#### `file.type()` - Get File Type

**Implementation:** Extension-based, cross-platform

**Algorithm:**
```c
// 1. Extract extension from filename
//    - "document.txt" → "txt"
//    - "archive.tar.gz" → "gz" (last extension only)
//    - "README" → "" (no extension)
//
// 2. Determine return type based on extension length:
//    - If length == 0: return "????" (OSType, 4 spaces + question marks)
//    - If length <= 4: return as OSType padded with spaces
//      * "txt" → ostypevalue "txt " (0x74787420)
//      * "pdf" → ostypevalue "pdf " (0x70646620)
//      * "c" → ostypevalue "c   " (0x63202020)
//    - If length > 4: return as string
//      * "markdown" → stringvalue "markdown"
//
// 3. Runtime does automatic type coercion (ostypevalue ↔ stringvalue)
```

**Windows Behavior:** Returns extension as OSType padded with spaces (matches our implementation)

**Mac Behavior:** Could check actual file type from metadata, but extension-based is simpler and cross-platform

**Implementation Location:** `Common/source/fileverbs.c` - case `filetypefunc` (line 2669)

**Changes Needed:**
- Remove Mac-specific `getfiletype()` call
- Add extension extraction logic
- Add padding logic for OSType
- No `#ifdef` needed - works on all platforms

#### `file.creator()` - Get File Creator

**Implementation:** Platform-dependent return value

**Algorithm:**
```c
#ifdef __APPLE__
    // Mac: Return actual creator code from file metadata
    OSType creator;
    if (getfilecreator(&fs, &creator)) {
        return setostypevalue(creator, v);
    } else {
        return setostypevalue('    ', v);  // 4 spaces if no creator
    }
#else
    // Non-Mac: Always return 4 spaces
    return setostypevalue('    ', v);
#endif
```

**Rationale:** Creator codes are Mac-specific. Windows legacy returns spaces. No cross-platform equivalent.

**Implementation Location:** `Common/source/fileverbs.c` - case `filecreatorfunc` (line 2685)

**Changes Needed:**
- Wrap existing Mac implementation with `#ifdef __APPLE__`
- Add non-Mac path returning spaces

### 2. Bundle Detection (1 verb - partial)

#### `file.hasbundle()` - Check if File is a Bundle

**Implementation:** Suffix-based detection (cross-platform)

**Algorithm:**
```c
// Check if path ends with bundle suffixes
if (endswith(path, ".app") ||
    endswith(path, ".bundle") ||
    endswith(path, ".framework") ||
    endswith(path, ".plugin") ||
    endswith(path, ".kext")) {
    return true;
}
return false;
```

**Rationale:** macOS bundles are directories with specific suffixes. On other platforms, these are just directories (no special meaning), but the suffix check is harmless and correct.

**Implementation Location:** `Common/source/fileverbs.c` - case `filehasbundlefunc` (line 2804)

**Changes Needed:**
- Replace Mac `filehasbundle()` call with suffix check
- Works on all platforms (no `#ifdef` needed)

#### `file.setbundle()` - Set Bundle Bit

**Implementation:** Error on all platforms (can't set bundle bit outside Mac filesystem)

**Algorithm:**
```c
getstringlist(langerrorlist, unimplementedverberror, bserror);
return (false);
```

**Rationale:** Bundle bit is a Mac filesystem attribute. Can't be set on other platforms.

**P2 Enhancement:** Could implement by creating `.app` directory structure, but low value.

**Implementation Location:** `Common/source/fileverbs.c` - case `filesetbundlefunc` (line 2820)

### 3. Visibility Verbs (2 verbs)

#### `file.isvisible()` - Check if File is Visible

**Implementation:** Always return true (cross-platform)

**Algorithm:**
```c
// On non-Mac: all files are "visible" (no Finder invisible attribute)
// On Mac: could check kMDItemFSInvisible, but for simplicity just return true
(*v).data.flvalue = true;
return (true);
```

**Rationale:** Visibility is a Finder-specific attribute. On other platforms, hidden files use "." prefix (checked separately). Returning true is safe default.

**Implementation Location:** `Common/source/fileverbs.c` - case `fileisvisiblefunc` (line 2862)

**Changes Needed:**
- Replace Mac implementation with simple `return true`
- Works on all platforms (no `#ifdef` needed)

#### `file.setvisible()` - Set File Visibility

**Implementation:** Return false (operation not supported, but non-fatal)

**Algorithm:**
```c
// Can't set Finder visibility on non-Mac platforms
// Return false to indicate operation failed (but don't error)
(*v).data.flvalue = false;
return (true);
```

**Rationale:** Setting visibility is Mac-specific. Return false to indicate failure without halting script.

**P2 Enhancement:** Could implement by renaming file to add/remove "." prefix on Unix.

**Implementation Location:** `Common/source/fileverbs.c` - case `filesetvisiblefunc` (line 2876)

**Changes Needed:**
- Replace Mac implementation with `return false`
- Works on all platforms (no `#ifdef` needed)

---

## Mac-Only Implementations

### 1. Aliases (3 verbs) - Mac-Only

**Verbs:**
- `file.newAlias()` - Create Mac alias
- `file.isAlias()` - Check if file is Mac alias
- `file.followAlias()` - Resolve Mac alias to target

**Strategy:** `#ifdef __APPLE__` - Keep Mac implementation, error on non-Mac

**Rationale:** Mac aliases are semantic shortcuts with unique properties (survive moves/renames). Different from symlinks. Cross-platform symlink verbs are separate P2 enhancement.

**Implementation Location:** `Common/source/fileverbs.c`
- Line 2847: `fileisaliasfunc`
- Line 3379: `newaliasfunc`
- Line 3382: `filefollowaliasfunc`

**Changes Needed:**
- Wrap all 3 cases with `#ifdef __APPLE__`
- Non-Mac: return `unimplementedverberror`

### 2. Version Verbs (4 verbs) - Mac-Only (P1: Windows)

**Verbs:**
- `file.getVersion()` / `file.getShortVersion()` - Get version string
- `file.getLongVersion()` - Get long version string
- `file.setVersion()` / `file.setShortVersion()` - Set version string
- `file.setLongVersion()` - Set long version string

**Strategy:** `#ifdef __APPLE__` - Keep Mac implementation (reads 'vers' resource)

**Rationale:** Version resources exist on both Mac and Windows, but different formats:
- Mac: 'vers' resource in resource fork
- Windows: VERSION_INFO in PE executable resources
- Linux: No standard equivalent

**Implementation Location:** `Common/source/fileverbs.c`
- Line 3364: `getshortversionfunc`
- Line 3367: `getlongversionfunc`
- Line 3391: `setshortversionfunc`
- Line 3394: `setlongversionfunc`

**Changes Needed:**
- Wrap all 4 cases with `#ifdef __APPLE__`
- Non-Mac: return `unimplementedverberror`
- **TODO P1**: File issue for Windows PE resource implementation

### 3. Finder Metadata (9 verbs) - Mac-Only

**Comments (2 verbs):**
- `file.getComment()` - Get Finder comment
- `file.setComment()` - Set Finder comment

**Icons (2 verbs):**
- `file.getIconPos()` - Get icon position in Finder
- `file.setIconPos()` - Set icon position in Finder

**Labels (5 verbs):**
- `file.getLabel()` - Get Finder label color
- `file.setLabel()` - Set Finder label color
- `file.getLabelIndex()` - Get label index (0-7)
- `file.setLabelIndex()` - Set label index
- `file.getLabelNames()` - Get array of label names

**Strategy:** `#ifdef __APPLE__` - Keep Mac implementation, error on non-Mac

**Rationale:** All are Finder-specific metadata. No cross-platform equivalent.

**Implementation Location:** `Common/source/fileverbs.c`
- Line 3370: `filegetcommentfunc`
- Line 3397: `filesetcommentfunc`
- Line 3385: `filegeticonposfunc`
- Line 3388: `fileseticonposfunc`
- Line 3400: `filegetlabelfunc`
- Line 3403: `filesetlabelfunc`
- Line 3474: `getlabelindexfunc`
- Line 3477: `setlabelindexfunc`
- Line 3480: `getlabelnamesfunc`

**Changes Needed:**
- These are already grouped together (lines 3376-3404) with comment "MAC speciifc verbs"
- Wrap entire group with `#ifdef __APPLE__`
- Non-Mac: return `unimplementedverberror`
- **TODO P2**: File issue for extended attributes implementation (comments, labels)

### 4. Other Mac-Only Verbs

#### `file.copyResourceFork()` - Copy Resource Fork

**Strategy:** Error on all platforms (obsolete)

**Rationale:** Resource forks are Mac OS 9 concept. Modern macOS uses extended attributes. Obsolete functionality.

**Implementation Location:** `Common/source/fileverbs.c` - Line 2978: `filecopyresourceforkfunc`

**Changes Needed:**
- Return `unimplementedverberror`
- **TODO P2**: File issue noting obsolete status

#### `file.launch()` - Launch File

**Strategy:** Error on all platforms (P1: cross-platform implementation)

**Rationale:** Currently uses Mac Launch Services. Need cross-platform equivalent:
- Mac: LSOpenFSRef
- Linux: xdg-open
- Windows: ShellExecute

**Implementation Location:** `Common/source/fileverbs.c` - Line 3305: `filelaunchfunc`

**Changes Needed:**
- Wrap Mac implementation with `#ifdef __APPLE__`
- Non-Mac: return `unimplementedverberror`
- **TODO P1**: File issue for cross-platform launch implementation

#### `file.findApplication()` - Find Application for File Type

**Strategy:** Error on all platforms (P1: cross-platform implementation)

**Rationale:** Currently uses Mac Launch Services to find app for file type. Need cross-platform equivalent:
- Mac: LSCopyApplicationForMIMEType or file extension mapping
- Linux: xdg-mime query default
- Windows: Registry HKEY_CLASSES_ROOT

**Implementation Location:** `Common/source/fileverbs.c` - Line 3373: `filefindappfunc`

**Changes Needed:**
- Wrap Mac implementation with `#ifdef __APPLE__`
- Non-Mac: return `unimplementedverberror`
- **TODO P1**: File issue for cross-platform application finder

---

## Conditional Compilation Strategy

### Pattern 1: Simple Cross-Platform Implementation (No #ifdef)

**Use for:** Verbs that work the same on all platforms

**Example:**
```c
case filetypefunc: {
    // Extract extension, return as OSType or string
    // Works on all platforms
    return (setstringvalue(extension, v));
}
```

### Pattern 2: Platform-Specific Return Values

**Use for:** Verbs with different behavior on Mac vs non-Mac, but both return valid results

**Example:**
```c
case filecreatorfunc: {
    #ifdef __APPLE__
        OSType creator;
        if (getfilecreator(&fs, &creator))
            return setostypevalue(creator, v);
    #endif
    // Non-Mac or Mac with no creator: return spaces
    return setostypevalue('    ', v);
}
```

### Pattern 3: Mac-Only Implementation

**Use for:** Verbs that only work on Mac, error on other platforms

**Example:**
```c
case newaliasfunc:
case filefollowaliasfunc:
case fileisaliasfunc:
    #ifdef __APPLE__
        switch (token) {
            case newaliasfunc:
                return (newaliasverb (hp1, v));
            case filefollowaliasfunc:
                return (followaliasverb (hp1, v));
            case fileisaliasfunc:
                return (isaliasverb (hp1, v));
        }
    #else
        getstringlist(langerrorlist, unimplementedverberror, bserror);
        return (false);
    #endif
```

### Pattern 4: Grouped Mac-Only Verbs

**Use for:** Multiple consecutive Mac-only verbs (already grouped in source)

**Example:**
```c
/* 3/20/97 - The following are MAC speciifc verbs and are therefore grouped
   together here for ease of ifdefing */

case newaliasfunc:
case filefollowaliasfunc:
case filegeticonposfunc:
case fileseticonposfunc:
case setshortversionfunc:
case setlongversionfunc:
case filesetcommentfunc:
case filegetlabelfunc:
case filesetlabelfunc:
    #ifdef __APPLE__
        switch (token) {
            // All Mac implementations...
        }
    #else
        getstringlist(langerrorlist, unimplementedverberror, bserror);
        return (false);
    #endif
```

---

## Testing Strategy

### Unit Tests (C-Level)

**Existing Tests:** Check if `tests/runtime_tests/` has file verb tests

**New Tests Needed:**
- Type extraction and padding logic
- Bundle suffix detection
- Extension parsing edge cases

**Location:** `tests/runtime_tests/test_file_verbs.c` (if exists, or create)

### Integration Tests (YAML)

**Existing Tests:** `tests/integration/test_cases/file_verbs.yaml` (12 tests already exist)

**New Tests Needed:**

```yaml
# Type/Creator Tests
- name: "file.type - extension <= 4 chars returns OSType"
  script: |
    local(fs = file.fileFromPath("/tmp/test.txt"));
    file.new(fs);
    local(t = file.type(fs));
    file.delete(fs);
    return typeof(t) + ":" + string(t)
  expected_success: true
  expected_result: "ostypevalue:txt "

- name: "file.type - extension > 4 chars returns string"
  script: |
    local(fs = file.fileFromPath("/tmp/test.markdown"));
    file.new(fs);
    local(t = file.type(fs));
    file.delete(fs);
    return typeof(t) + ":" + t
  expected_success: true
  expected_result: "string:markdown"

- name: "file.type - no extension returns ????"
  script: |
    local(fs = file.fileFromPath("/tmp/README"));
    file.new(fs);
    local(t = file.type(fs));
    file.delete(fs);
    return string(t)
  expected_success: true
  expected_result: "????"

- name: "file.creator - returns 4 spaces on non-Mac"
  script: |
    local(fs = file.fileFromPath("/tmp/test.txt"));
    file.new(fs);
    local(c = file.creator(fs));
    file.delete(fs);
    return string(c)
  expected_success: true
  expected_result: "    "

# Bundle Tests
- name: "file.hasbundle - .app suffix returns true"
  script: |
    local(fs = file.fileFromPath("/Applications/TextEdit.app"));
    return file.hasbundle(fs)
  expected_success: true
  expected_result: "true"

- name: "file.hasbundle - regular file returns false"
  script: |
    local(fs = file.fileFromPath("/tmp/test.txt"));
    return file.hasbundle(fs)
  expected_success: true
  expected_result: "false"

- name: "file.setbundle - returns error"
  script: |
    local(fs = file.fileFromPath("/tmp/test.txt"));
    file.new(fs);
    local(result = file.setbundle(fs, true));
    file.delete(fs);
    return result
  expected_success: false
  expected_error: "not implemented"

# Visibility Tests
- name: "file.isvisible - returns true"
  script: |
    local(fs = file.fileFromPath("/tmp/test.txt"));
    file.new(fs);
    local(vis = file.isvisible(fs));
    file.delete(fs);
    return vis
  expected_success: true
  expected_result: "true"

- name: "file.setvisible - returns false (not supported)"
  script: |
    local(fs = file.fileFromPath("/tmp/test.txt"));
    file.new(fs);
    local(result = file.setvisible(fs, false));
    file.delete(fs);
    return result
  expected_success: true
  expected_result: "false"

# Mac-Only Verbs (should error on non-Mac)
- name: "file.newAlias - not implemented on non-Mac"
  script: |
    local(fs = file.fileFromPath("/tmp/test.txt"));
    local(alias = file.fileFromPath("/tmp/test_alias"));
    file.newAlias(fs, alias)
  expected_success: false
  expected_error: "not implemented"
  platform: "non-mac"

- name: "file.getVersion - not implemented on non-Mac"
  script: |
    local(fs = file.fileFromPath("/bin/ls"));
    file.getVersion(fs)
  expected_success: false
  expected_error: "not implemented"
  platform: "non-mac"
```

**Test Execution:**
```bash
# Run all tests
cd tests && make test-integration

# Run just file verb tests
cd tests && python3 integration_test_runner.py test_cases/file_verbs.yaml
```

---

## P1/P2 Enhancement Issues

### P1 Issues (High Priority - Cross-Platform Functionality)

**Issue 1: Implement file.launch() Cross-Platform**
- **Title:** Implement cross-platform file.launch() verb
- **Description:** Currently Mac-only (Launch Services). Implement for Linux (xdg-open) and Windows (ShellExecute)
- **Affected Verbs:** `file.launch()`
- **Effort:** 5-10 hours
- **Priority:** P1 (common use case)

**Issue 2: Implement file.findApplication() Cross-Platform**
- **Title:** Implement cross-platform file.findApplication() verb
- **Description:** Find default application for file type. Mac uses Launch Services, need Linux (xdg-mime) and Windows (Registry) implementations
- **Affected Verbs:** `file.findApplication()`
- **Effort:** 5-10 hours
- **Priority:** P1 (common use case)

**Issue 3: Implement Version Verbs for Windows**
- **Title:** Implement file version verbs for Windows PE resources
- **Description:** Mac reads 'vers' resource fork. Windows should read VERSION_INFO from PE executable resources
- **Affected Verbs:** `file.getVersion()`, `file.getLongVersion()`, `file.setVersion()`, `file.setLongVersion()`
- **Effort:** 10-15 hours
- **Priority:** P1 (works on Windows legacy, should work on headless)

### P2 Issues (Lower Priority - Enhanced Functionality)

**Issue 4: Implement file.settype() by Renaming Extension**
- **Title:** Implement cross-platform file.settype() by renaming file extension
- **Description:** Currently errors on non-Mac. Could implement by renaming file to change extension (e.g., "doc.txt" → "doc.pdf" when setting type to "pdf ")
- **Affected Verbs:** `file.settype()`
- **Effort:** 3-5 hours
- **Priority:** P2 (low usage)

**Issue 5: Implement Comment/Label Verbs via Extended Attributes**
- **Title:** Implement file comments and labels using extended attributes
- **Description:** Mac stores in Finder metadata. Could use extended attributes (xattr) on Linux/Mac for portable implementation
- **Affected Verbs:** `file.getComment()`, `file.setComment()`, `file.getLabel()`, `file.setLabel()`, label index verbs
- **Effort:** 15-20 hours
- **Priority:** P2 (Mac-specific feature)

**Issue 6: Mark file.copyResourceFork() as Obsolete**
- **Title:** Document file.copyResourceFork() as obsolete (Mac OS 9)
- **Description:** Resource forks are Mac OS 9 concept. Modern macOS uses extended attributes. Mark verb as deprecated/obsolete
- **Affected Verbs:** `file.copyResourceFork()`
- **Effort:** 1 hour (documentation only)
- **Priority:** P2 (informational)

**Issue 7: Implement file.setvisible() via Filename Prefix**
- **Title:** Implement file.setvisible() by adding/removing "." prefix on Unix
- **Description:** Unix hidden files use "." prefix. Could implement setvisible by renaming file
- **Affected Verbs:** `file.setvisible()`
- **Effort:** 2-3 hours
- **Priority:** P2 (nice-to-have)

---

## Implementation Checklist

### Phase 1: Cross-Platform Implementations

- [ ] **Type/Creator Getters**
  - [ ] Implement `file.type()` - extension extraction and padding
  - [ ] Implement `file.creator()` - return spaces on non-Mac
  - [ ] Unit tests for extension parsing
  - [ ] Integration tests for type/creator

- [ ] **Bundle Detection**
  - [ ] Implement `file.hasbundle()` - suffix check
  - [ ] Stub `file.setbundle()` - return error
  - [ ] Integration tests for bundle detection

- [ ] **Visibility Verbs**
  - [ ] Implement `file.isvisible()` - always return true
  - [ ] Implement `file.setvisible()` - return false
  - [ ] Integration tests for visibility

- [ ] **Type/Creator Setters**
  - [ ] Stub `file.settype()` - return error
  - [ ] Stub `file.setcreator()` - return error
  - [ ] Integration tests expecting errors

### Phase 2: Mac-Only Conditional Compilation

- [ ] **Aliases (3 verbs)**
  - [ ] Wrap with `#ifdef __APPLE__`
  - [ ] Non-Mac: return `unimplementedverberror`
  - [ ] Integration tests (platform-specific)

- [ ] **Versions (4 verbs)**
  - [ ] Wrap with `#ifdef __APPLE__`
  - [ ] Non-Mac: return `unimplementedverberror`
  - [ ] File P1 issue for Windows implementation

- [ ] **Finder Metadata (9 verbs)**
  - [ ] Wrap grouped section with `#ifdef __APPLE__`
  - [ ] Non-Mac: return `unimplementedverberror`
  - [ ] File P2 issue for extended attributes

- [ ] **Other Mac-Only (3 verbs)**
  - [ ] `file.copyResourceFork()` - error, file P2 issue (obsolete)
  - [ ] `file.launch()` - wrap Mac, error non-Mac, file P1 issue
  - [ ] `file.findApplication()` - wrap Mac, error non-Mac, file P1 issue

### Phase 3: Testing & Verification

- [ ] **Build Verification**
  - [ ] Clean build on macOS
  - [ ] Verify all 86 verbs registered
  - [ ] No compilation warnings

- [ ] **Integration Tests**
  - [ ] Add 15+ new tests to `file_verbs.yaml`
  - [ ] Run full integration test suite
  - [ ] Verify cross-platform verbs work
  - [ ] Verify Mac-only verbs error correctly on non-Mac

- [ ] **Manual Testing**
  - [ ] Test `file.type()` with various extensions
  - [ ] Test `file.hasbundle()` with .app directories
  - [ ] Test Mac-only verbs on macOS (if available)
  - [ ] Test error messages are clear and helpful

### Phase 4: Documentation & Issues

- [ ] **File P1 Issues**
  - [ ] Issue: file.launch() cross-platform
  - [ ] Issue: file.findApplication() cross-platform
  - [ ] Issue: Version verbs for Windows PE resources

- [ ] **File P2 Issues**
  - [ ] Issue: file.settype() by renaming extension
  - [ ] Issue: Comment/label verbs via extended attributes
  - [ ] Issue: Mark file.copyResourceFork() obsolete
  - [ ] Issue: file.setvisible() via filename prefix

- [ ] **Update Documentation**
  - [ ] Update `processor_audits/file.md` with implementation status
  - [ ] Update `HEADLESS_ADAPTATIONS.md` if needed
  - [ ] Add code comments referencing this planning doc

---

## Success Criteria

**Phase 1 Complete When:**
- ✅ All 86 file verbs compile without errors
- ✅ Dialog verbs return `unimplementedverberror` (Phase 1)
- ✅ Cross-platform verbs implemented (type, creator, bundle, visibility)
- ✅ Mac-only verbs wrapped with `#ifdef __APPLE__`
- ✅ Integration tests pass (15+ new tests)
- ✅ Build succeeds on macOS
- ✅ P1/P2 issues filed with clear descriptions

**Ready for PR When:**
- ✅ All checklist items completed
- ✅ No regressions in existing tests
- ✅ Code review completed (self or pull-request agent)
- ✅ Commit message references this planning doc

---

**Last Updated:** 2026-01-01
**Status:** ✅ Ready for Implementation
