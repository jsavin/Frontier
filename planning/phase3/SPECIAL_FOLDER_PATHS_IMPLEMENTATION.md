# Special Folder Paths Cross-Platform Implementation

**Status**: Planning
**Created**: 2025-12-31
**Verbs**: `file.getSpecialFolderPath`, `file.getSystemFolderPath`

---

## Overview

The `file.getSpecialFolderPath` and `file.getSystemFolderPath` verbs are currently stubbed. The original UserTalk implementation in `usertalk_scripts/Frontier.root/system/verbs/builtins/file/getSpecialFolderPath.ut` shows these verbs were designed for Classic Mac OS/macOS and Windows, with platform-specific folder mappings.

**Goal**: Design a cross-platform implementation that works on macOS, Linux, and Windows while handling semantic differences in modern OS folder concepts.

---

## Current State Analysis

### Original UserTalk Implementation (macOS X Carbon)

```usertalk
case string.lower (specialfolder) {
    "applications"      → /Applications/
    "desktop folder"    → /Users/{username}/Desktop/
    "extensions"        → /System/Extensions/
    "favorites"         → /Users/{username}/Library/Favorites/
    "fontcollections"   → /Users/{username}/Library/FontCollections/
    "fonts"             → /Users/{username}/Library/Fonts/
    "preferences"       → /Users/{username}/Library/Preferences/
    "startup items"     → ERROR: "Mac OS X does not have a Startup Items folder"
    "system folder"     → /System/
    "temp"              → /Temporary Items/
    "trash"             → /Trash/
}
```

### Windows Implementation (from UserTalk)

Windows special folders are kernel-based, using Windows API folder constants:
- "Desktop", "DesktopDirectory", "Fonts", "NetHood"
- "Personal", "Programs", "Recent", "SendTo"
- "StartMenu", "StartUp", "Templates"
- "Windows", "System"

### file.getSystemFolderPath

Minimal UserTalk wrapper: `kernel (file.getsystemfolderpath)`

Appears to be Classic Mac OS specific - returns the System Folder path. No cross-platform equivalent documented.

---

## Semantic Mismatches in Modern OSes

### Problem: Conceptual Differences

Several Classic Mac OS folders have modern equivalents that work **very differently**:

| Classic Concept | Modern macOS | Semantic Difference |
|----------------|--------------|---------------------|
| **Startup Items** (folder of apps/scripts that run on login) | `/Library/LaunchAgents/` | LaunchAgents are **plist configuration files**, not executable items. Conceptually different mechanism. |
| **Apple Menu Items** (menu items in the Apple menu) | `~/Library/Services/` | Services are **system-wide action bundles**, not menu items. Different UI paradigm. |
| **Extensions** (system extensions) | No direct equivalent | macOS deprecated kernel extensions; modern extensions are app bundles with different mechanisms |

### Recommendation: Use Different Terms for Different Concepts

**Option A: Return Errors for Semantic Mismatches** (Conservative)
- `"startup items"` → Error: "Not available - use LaunchAgents on macOS"
- `"apple menu items"` → Error: "Not available - use Services on macOS"
- Forces UserTalk code to be aware of platform differences

**Option B: Map to Modern Equivalents with Documentation** (Pragmatic)
- `"startup items"` → `/Library/LaunchAgents/` on macOS (with big warning in docs)
- `"apple menu items"` → `~/Library/Services/` on macOS (with big warning in docs)
- Accept that returned folder won't work the same way, but at least it's the conceptual successor

**Option C: Separate Modern Verbs** (Clean Architecture)
- Keep `file.getSpecialFolderPath` for Classic folders only (return errors for deprecated)
- Add new verb: `file.getModernFolderPath` for modern equivalents
- `file.getModernFolderPath("", "launch agents", false)` → `/Library/LaunchAgents/`
- `file.getModernFolderPath("", "services", false)` → `~/Library/Services/`

**Option D: Implement in UserTalk Layer, Not Kernel** (RECOMMENDED ✅)
- Remove kernel implementation entirely
- Revert to UserTalk-based implementation (like the original)
- Kernel stub returns "not implemented in kernel - use UserTalk glue script"
- Advantages:
  - Platform-specific logic is easier to read/maintain in UserTalk
  - Once GUI exists, can edit/debug live
  - Original implementation WAS in UserTalk (proven pattern)
  - Kernel stays lean and focused
  - Cross-platform differences are more transparent to users
- Implementation: Restore/update `system.verbs.builtins.file.getSpecialFolderPath` script

**Proposed Decision**: **Option D** (UserTalk implementation)
- Short-term: Kernel returns "not implemented" error
- Medium-term: Implement comprehensive UserTalk version based on this plan
- Long-term: Maintain in UserTalk, benefit from live editing in GUI

---

## Cross-Platform Folder Mapping Strategy

### Tier 1: Core Cross-Platform Folders (MUST SUPPORT)

These folders have clear equivalents across all platforms:

| Folder Name | macOS | Linux (XDG) | Windows | Notes |
|-------------|-------|-------------|---------|-------|
| `"preferences"` | `~/Library/Preferences/` | `$XDG_CONFIG_HOME/` (default: `~/.config/`) | `%APPDATA%` | User config files |
| `"desktop folder"` | `~/Desktop/` | `$XDG_DESKTOP_DIR/` (default: `~/Desktop/`) | `%USERPROFILE%\Desktop` | User's desktop |
| `"temp"` | `/tmp/` or `$TMPDIR` | `$TMPDIR` or `/tmp/` | `%TEMP%` | Temporary files |
| `"application support"` | `~/Library/Application Support/` | `$XDG_DATA_HOME/` (default: `~/.local/share/`) | `%APPDATA%` | App-specific data |
| `"cache"` | `~/Library/Caches/` | `$XDG_CACHE_HOME/` (default: `~/.cache/`) | `%LOCALAPPDATA%` | Cache data |

### Tier 2: Platform-Specific with Reasonable Fallbacks

| Folder Name | macOS | Linux | Windows | Notes |
|-------------|-------|-------|---------|-------|
| `"applications"` | `/Applications/` | `~/.local/share/applications/` | `%PROGRAMFILES%` | System apps |
| `"fonts"` | `~/Library/Fonts/` | `~/.local/share/fonts/` | `%WINDIR%\Fonts` | User fonts |
| `"trash"` | `~/.Trash/` | `~/.local/share/Trash/` | `$Recycle.Bin` | Deleted items |
| `"documents"` | `~/Documents/` | `$XDG_DOCUMENTS_DIR/` (default: `~/Documents/`) | `%USERPROFILE%\Documents` | User documents |
| `"downloads"` | `~/Downloads/` | `$XDG_DOWNLOAD_DIR/` (default: `~/Downloads/`) | `%USERPROFILE%\Downloads` | Downloaded files |
| `"music"` | `~/Music/` | `$XDG_MUSIC_DIR/` (default: `~/Music/`) | `%USERPROFILE%\Music` | Music files |
| `"pictures"` | `~/Pictures/` | `$XDG_PICTURES_DIR/` (default: `~/Pictures/`) | `%USERPROFILE%\Pictures` | Picture files |
| `"videos"` | `~/Movies/` | `$XDG_VIDEOS_DIR/` (default: `~/Videos/`) | `%USERPROFILE%\Videos` | Video files |

### Tier 3: Deprecated/Unsupported (Return Errors)

| Folder Name | Status | Error Message |
|-------------|--------|---------------|
| `"startup items"` | Deprecated | "Startup Items folder is not available on modern macOS - use LaunchAgents instead" |
| `"apple menu items"` | Deprecated | "Apple Menu Items folder is not available on modern macOS - use Services instead" |
| `"extensions"` | Deprecated | "Extensions folder is not available on modern macOS - kernel extensions are deprecated" |
| `"system folder"` | macOS-specific | "System folder concept is macOS-specific - not available on this platform" (on Linux/Windows) |

### Windows-Specific Folders (Tier 4)

Support Windows-specific folders on Windows, return platform error on macOS/Linux:

- `"programs"`, `"program files"` → `%PROGRAMFILES%`
- `"startup"` → `%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup`
- `"start menu"` → `%APPDATA%\Microsoft\Windows\Start Menu`
- `"recent"` → `%APPDATA%\Microsoft\Windows\Recent`
- `"sendto"` → `%APPDATA%\Microsoft\Windows\SendTo`
- `"templates"` → `%APPDATA%\Microsoft\Windows\Templates`

---

## Implementation Design

### API Signature

```c
boolean fileGetSpecialFolderPath(
    const char *volume,           // Volume/drive (e.g., "C:", "/Volumes/Data", or "" for default)
    const char *specialfolder,    // Folder name (case-insensitive)
    boolean create,               // Whether to create folder if it doesn't exist
    bigstring result              // Output: absolute path
);
```

### Implementation Strategy

```c
// Pseudocode structure
boolean fileGetSpecialFolderPath(const char *vol, const char *folder, boolean create, bigstring result) {
    char normalized_name[256];
    normalize_folder_name(folder, normalized_name); // Convert to lowercase, trim

    // Platform detection
    #if defined(__APPLE__)
        return get_macos_folder_path(vol, normalized_name, create, result);
    #elif defined(_WIN32)
        return get_windows_folder_path(vol, normalized_name, create, result);
    #else  // Linux/POSIX
        return get_linux_folder_path(vol, normalized_name, create, result);
    #endif
}
```

### macOS Implementation

```c
boolean get_macos_folder_path(const char *vol, const char *folder, boolean create, bigstring result) {
    const char *home = getenv("HOME");
    if (!home) return error("Unable to get HOME directory");

    char path[PATH_MAX];

    // Tier 1: Core folders
    if (strcmp(folder, "preferences") == 0) {
        snprintf(path, sizeof(path), "%s/Library/Preferences/", home);
    }
    else if (strcmp(folder, "desktop folder") == 0) {
        snprintf(path, sizeof(path), "%s/Desktop/", home);
    }
    else if (strcmp(folder, "temp") == 0) {
        const char *tmpdir = getenv("TMPDIR");
        snprintf(path, sizeof(path), "%s", tmpdir ? tmpdir : "/tmp/");
    }
    else if (strcmp(folder, "application support") == 0) {
        snprintf(path, sizeof(path), "%s/Library/Application Support/", home);
    }
    else if (strcmp(folder, "cache") == 0) {
        snprintf(path, sizeof(path), "%s/Library/Caches/", home);
    }

    // Tier 2: Platform-specific
    else if (strcmp(folder, "applications") == 0) {
        snprintf(path, sizeof(path), "/Applications/");
    }
    else if (strcmp(folder, "fonts") == 0) {
        snprintf(path, sizeof(path), "%s/Library/Fonts/", home);
    }
    else if (strcmp(folder, "trash") == 0) {
        snprintf(path, sizeof(path), "%s/.Trash/", home);
    }
    else if (strcmp(folder, "documents") == 0) {
        snprintf(path, sizeof(path), "%s/Documents/", home);
    }

    // Tier 3: Deprecated (return errors)
    else if (strcmp(folder, "startup items") == 0) {
        return error("Startup Items folder is not available on modern macOS - use LaunchAgents instead");
    }
    else if (strcmp(folder, "apple menu items") == 0) {
        return error("Apple Menu Items folder is not available on modern macOS - use Services instead");
    }
    else if (strcmp(folder, "extensions") == 0) {
        return error("Extensions folder is not available on modern macOS - kernel extensions are deprecated");
    }

    // Unknown folder
    else {
        return error("Unknown special folder name: %s", folder);
    }

    // Apply volume prefix if specified
    if (vol && strlen(vol) > 0) {
        char vol_path[PATH_MAX];
        snprintf(vol_path, sizeof(vol_path), "%s%s", vol, path);
        strcpy(path, vol_path);
    }

    // Create if requested
    if (create) {
        // Use mkdir with proper permissions
        if (!ensure_directory_exists(path)) {
            return error("Failed to create directory: %s", path);
        }
    }

    // Return result
    copyctopstring(path, result);
    return true;
}
```

### Linux Implementation (XDG Base Directory Spec)

```c
boolean get_linux_folder_path(const char *vol, const char *folder, boolean create, bigstring result) {
    const char *home = getenv("HOME");
    if (!home) return error("Unable to get HOME directory");

    char path[PATH_MAX];

    // Helper to get XDG directory with fallback
    const char *get_xdg_dir(const char *xdg_env, const char *fallback) {
        const char *dir = getenv(xdg_env);
        if (dir && strlen(dir) > 0) return dir;

        static char buf[PATH_MAX];
        snprintf(buf, sizeof(buf), "%s/%s", home, fallback);
        return buf;
    }

    // Tier 1: Core folders
    if (strcmp(folder, "preferences") == 0) {
        strcpy(path, get_xdg_dir("XDG_CONFIG_HOME", ".config"));
        strcat(path, "/");
    }
    else if (strcmp(folder, "desktop folder") == 0) {
        strcpy(path, get_xdg_dir("XDG_DESKTOP_DIR", "Desktop"));
        strcat(path, "/");
    }
    else if (strcmp(folder, "temp") == 0) {
        const char *tmpdir = getenv("TMPDIR");
        strcpy(path, tmpdir ? tmpdir : "/tmp/");
    }
    else if (strcmp(folder, "application support") == 0) {
        strcpy(path, get_xdg_dir("XDG_DATA_HOME", ".local/share"));
        strcat(path, "/");
    }
    else if (strcmp(folder, "cache") == 0) {
        strcpy(path, get_xdg_dir("XDG_CACHE_HOME", ".cache"));
        strcat(path, "/");
    }

    // Tier 2: Best effort mappings
    else if (strcmp(folder, "applications") == 0) {
        snprintf(path, sizeof(path), "%s/.local/share/applications/", home);
    }
    else if (strcmp(folder, "fonts") == 0) {
        snprintf(path, sizeof(path), "%s/.local/share/fonts/", home);
    }
    else if (strcmp(folder, "trash") == 0) {
        snprintf(path, sizeof(path), "%s/.local/share/Trash/", home);
    }
    else if (strcmp(folder, "documents") == 0) {
        strcpy(path, get_xdg_dir("XDG_DOCUMENTS_DIR", "Documents"));
        strcat(path, "/");
    }

    // Tier 3: Not available on Linux
    else if (strcmp(folder, "system folder") == 0) {
        return error("System folder concept is macOS-specific - not available on Linux");
    }

    // Tier 4: Windows-specific
    else if (strcmp(folder, "programs") == 0 || strcmp(folder, "startup") == 0) {
        return error("'%s' is a Windows-specific folder - not available on Linux", folder);
    }

    // Unknown folder
    else {
        return error("Unknown special folder name: %s", folder);
    }

    // Volume parameter ignored on Linux (single root filesystem)
    if (vol && strlen(vol) > 0) {
        // Log warning but don't error
        log_warn(LOG_COMP_FILE, "Volume parameter ignored on Linux: %s", vol);
    }

    // Create if requested
    if (create) {
        if (!ensure_directory_exists(path)) {
            return error("Failed to create directory: %s", path);
        }
    }

    copyctopstring(path, result);
    return true;
}
```

### Windows Implementation

```c
boolean get_windows_folder_path(const char *vol, const char *folder, boolean create, bigstring result) {
    char path[MAX_PATH];

    // Use Windows SHGetFolderPath API for standard folders
    int csidl = -1;

    if (strcmp(folder, "preferences") == 0 || strcmp(folder, "application support") == 0) {
        csidl = CSIDL_APPDATA;  // %APPDATA%
    }
    else if (strcmp(folder, "desktop folder") == 0) {
        csidl = CSIDL_DESKTOPDIRECTORY;  // %USERPROFILE%\Desktop
    }
    else if (strcmp(folder, "temp") == 0) {
        // Use GetTempPath instead
        GetTempPath(sizeof(path), path);
        copyctopstring(path, result);
        return true;
    }
    else if (strcmp(folder, "cache") == 0) {
        csidl = CSIDL_LOCAL_APPDATA;  // %LOCALAPPDATA%
    }
    else if (strcmp(folder, "programs") == 0) {
        csidl = CSIDL_PROGRAM_FILES;
    }
    else if (strcmp(folder, "startup") == 0) {
        csidl = CSIDL_STARTUP;
    }
    else if (strcmp(folder, "documents") == 0) {
        csidl = CSIDL_PERSONAL;
    }
    else if (strcmp(folder, "fonts") == 0) {
        csidl = CSIDL_FONTS;
    }
    // ... more Windows folders

    else {
        return error("Unknown special folder name: %s", folder);
    }

    // Call Windows API
    HRESULT hr = SHGetFolderPath(NULL, csidl, NULL, SHGFP_TYPE_CURRENT, path);
    if (FAILED(hr)) {
        return error("Failed to get folder path for: %s", folder);
    }

    // Create if requested
    if (create) {
        if (!ensure_directory_exists(path)) {
            return error("Failed to create directory: %s", path);
        }
    }

    copyctopstring(path, result);
    return true;
}
```

---

## file.getSystemFolderPath Implementation

**Recommendation**: Stub this verb with a platform-specific error.

**Rationale**:
- This was Classic Mac OS specific (the "System Folder")
- No clear modern equivalent on any platform
- macOS has `/System/` but it's not a "System Folder" in the Classic sense
- Linux has `/usr/` and `/etc/` but conceptually different
- Windows has `%SYSTEMROOT%` but also conceptually different

**Implementation**:
```c
boolean fileGetSystemFolderPath(bigstring result) {
    #if defined(__APPLE__)
        // macOS: Return /System/ with caveat
        copyctopstring("/System/", result);
        log_warn(LOG_COMP_FILE, "getSystemFolderPath is deprecated - returning /System/ on macOS");
        return true;
    #else
        return error("getSystemFolderPath is not available on this platform - it was specific to Classic Mac OS");
    #endif
}
```

---

## REVISED: UserTalk Implementation Strategy

**Decision (2025-12-31)**: Implement this functionality in UserTalk, not in the kernel.

### Why UserTalk?

1. **Original Design**: The original implementation was in UserTalk (see `usertalk_scripts/Frontier.root/system/verbs/builtins/file/getSpecialFolderPath.ut`)
2. **Maintainability**: Platform-specific logic is easier to read, debug, and modify in UserTalk
3. **Live Editing**: Once GUI is available, scripts can be edited/tested without rebuilding
4. **Transparency**: Cross-platform differences are visible to users
5. **Kernel Simplicity**: Keep kernel focused on low-level operations

### Implementation Plan

**Phase 1: Kernel Stub (Immediate)**
- Update `stub_config.py` to return "not implemented in kernel" error for both verbs
- Error message: "getSpecialFolderPath is not implemented in the kernel - it will be available via UserTalk glue scripts"
- Create GitHub issue (P1) to implement UserTalk version

**Phase 2: UserTalk Implementation (P1 Priority)**
- Restore/update `system.verbs.builtins.file.getSpecialFolderPath` script
- Implement cross-platform folder mappings based on this plan
- Use `sys.os()` to detect platform
- Use environment variables (`getenv()`) for XDG paths on Linux
- Test on macOS, Linux, and Windows

**Phase 3: Documentation & Testing**
- Document supported folder names in UserTalk comments
- Add integration tests
- Update user documentation

---

## ORIGINAL PLAN (Kept for Reference)

The following sections describe a potential kernel implementation. This is kept as reference for the UserTalk implementation, but the actual code will be written in UserTalk, not C.

### Phase 1: Core Cross-Platform Folders (Week 1)

**Goal**: Support the 5 Tier 1 folders on all platforms

**Tasks**:
1. Implement folder name normalization (lowercase, trim)
2. Implement macOS version for Tier 1 folders
3. Implement Linux version for Tier 1 folders (XDG)
4. Implement Windows version for Tier 1 folders (SHGetFolderPath)
5. Add `ensure_directory_exists()` helper for `create` parameter
6. Write unit tests for each platform

**Deliverables**:
- Working `file.getSpecialFolderPath` for 5 core folders
- Tests passing on macOS and Linux (Windows if available)
- Documentation for supported folder names

### Phase 2: Extended Platform-Specific Folders (Week 2)

**Goal**: Support Tier 2 folders with platform-appropriate mappings

**Tasks**:
1. Add Tier 2 folder mappings for macOS
2. Add Tier 2 folder mappings for Linux
3. Add Tier 2 folder mappings for Windows
4. Add error handling for deprecated folders (Tier 3)
5. Add error handling for Windows-specific folders on non-Windows (Tier 4)
6. Expand unit tests

**Deliverables**:
- 13+ total folders supported (Tier 1 + Tier 2)
- Proper error messages for deprecated/unsupported folders
- Cross-platform test suite

### Phase 3: Integration & Documentation (Week 3)

**Goal**: Polish and document

**Tasks**:
1. Implement `file.getSystemFolderPath` (stub or minimal support)
2. Update UserTalk wrapper scripts if needed
3. Write comprehensive documentation
4. Add examples to docs
5. Performance testing (ensure no OS API slowdowns)

**Deliverables**:
- Complete implementation
- Documentation in `docs/FILE_VERB_REFERENCE.md`
- Migration guide for Classic Mac OS code

---

## Testing Strategy

### Unit Tests

```c
// Test Tier 1 folders exist and are reasonable
void test_get_preferences_folder() {
    bigstring result;
    assert(fileGetSpecialFolderPath("", "preferences", false, result));

    #if defined(__APPLE__)
        assert(strstr(result, "Library/Preferences"));
    #elif defined(__linux__)
        assert(strstr(result, ".config") || strstr(result, "XDG_CONFIG_HOME"));
    #elif defined(_WIN32)
        assert(strstr(result, "AppData"));
    #endif
}

// Test case-insensitivity
void test_case_insensitive() {
    bigstring r1, r2;
    assert(fileGetSpecialFolderPath("", "preferences", false, r1));
    assert(fileGetSpecialFolderPath("", "PREFERENCES", false, r2));
    assert(strcmp(r1, r2) == 0);
}

// Test deprecated folders return errors
void test_startup_items_deprecated() {
    bigstring result;
    #if defined(__APPLE__)
        assert(!fileGetSpecialFolderPath("", "startup items", false, result));
        // Should get error about LaunchAgents
    #endif
}

// Test create parameter
void test_create_folder() {
    bigstring result;

    // Create temp test folder
    assert(fileGetSpecialFolderPath("", "temp", true, result));

    // Verify it exists
    struct stat st;
    assert(stat(result, &st) == 0);
    assert(S_ISDIR(st.st_mode));
}
```

### Integration Tests

```usertalk
// Test UserTalk integration
local (path);
path = file.getSpecialFolderPath("", "preferences", false);
assert(file.exists(path));
assert(file.isFolder(path));

// Test error handling
try {
    file.getSpecialFolderPath("", "invalid folder name", false)
}
else {
    // Should error
    assert(true)
}
```

---

## Documentation Requirements

### User-Facing Documentation

Create `docs/FILE_SPECIAL_FOLDERS.md`:

```markdown
# Special Folder Paths

## Cross-Platform Folders

These folders are supported on all platforms:

- `"preferences"` - User configuration files
- `"desktop folder"` - User's desktop
- `"temp"` - Temporary files
- `"application support"` - Application data
- `"cache"` - Cache data

## Platform-Specific Folders

### macOS
- `"applications"` - /Applications/
- `"fonts"` - ~/Library/Fonts/
- `"trash"` - ~/.Trash/

### Linux
- `"applications"` - ~/.local/share/applications/
- `"fonts"` - ~/.local/share/fonts/
- `"trash"` - ~/.local/share/Trash/

### Windows
- `"programs"` - %PROGRAMFILES%
- `"startup"` - Startup folder in Start Menu

## Deprecated Folders

These Classic Mac OS folders are no longer supported:
- `"startup items"` - Use LaunchAgents on macOS
- `"apple menu items"` - Use Services on macOS
- `"extensions"` - Kernel extensions are deprecated
```

### Migration Guide for Classic Code

Document how to update UserTalk scripts that used deprecated folders:

```usertalk
// OLD (Classic Mac OS):
local (path = file.getSpecialFolderPath("", "startup items", true));
file.copy(app, path + "MyApp");  // Put app in Startup Items

// NEW (Modern macOS):
// Don't use getSpecialFolderPath - LaunchAgents work differently
// Create a plist file instead:
local (plist = file.getSpecialFolderPath("", "preferences", true) + "LaunchAgents/");
// ... create plist configuration for launch agent
```

---

## Open Questions

1. **Should `create` parameter actually create directories?**
   - PRO: Matches original behavior, convenient for UserTalk code
   - CON: Kernel verbs creating directories feels heavy-weight
   - DECISION NEEDED

2. **How to handle volume parameter on Linux/POSIX?**
   - Linux has single root filesystem, no drive letters
   - Current plan: Ignore volume parameter on Linux (log warning)
   - Alternative: Support mount points like `/mnt/data`?

3. **Should we support XDG user-dirs on Linux?**
   - XDG provides user-dirs.dirs file with custom folder locations
   - Example: `XDG_DOCUMENTS_DIR` might point to `/home/user/Dokumente` (German)
   - Current plan: Yes, read XDG environment variables
   - Requires reading `~/.config/user-dirs.dirs` if env vars not set

4. **Windows API dependency?**
   - SHGetFolderPath requires linking against Shell32.lib
   - Is this acceptable for headless Windows builds?
   - Alternative: Use environment variables only (%APPDATA%, etc.)

5. **Should "system folder" work on Linux?**
   - Could map to `/usr/` or `/etc/` or `/`
   - But these aren't conceptually equivalent to Mac OS System Folder
   - Current plan: Return error on Linux

---

## Success Criteria

- [ ] Tier 1 folders (5) work on macOS, Linux, Windows
- [ ] Tier 2 folders (8+) work with platform-appropriate mappings
- [ ] Deprecated folders return helpful error messages
- [ ] Case-insensitive folder name matching
- [ ] `create` parameter works correctly
- [ ] Unit tests pass on all platforms
- [ ] Documentation complete
- [ ] No regressions in existing file verbs
- [ ] UserTalk integration tests pass

---

## References

- **Original UserTalk**: `usertalk_scripts/Frontier.root/system/verbs/builtins/file/getSpecialFolderPath.ut`
- **XDG Base Directory Spec**: https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
- **Windows Special Folders**: https://docs.microsoft.com/en-us/windows/win32/shell/csidl
- **macOS Standard Directories**: https://developer.apple.com/library/archive/documentation/FileManagement/Conceptual/FileSystemProgrammingGuide/FileSystemOverview/FileSystemOverview.html

---

**Next Steps**: Review this plan, answer open questions, then proceed with Phase 1 implementation.
