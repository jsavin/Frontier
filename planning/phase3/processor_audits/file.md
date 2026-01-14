# Processor Audit: `file`

**Status:** ✅ **Partial Headless Compatibility** (Phased Implementation)
**Audit Date:** 2025-12-05
**Auditor:** Claude (Sonnet 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `file` |
| **EFP ID** | 1003 (lang block) |
| **Verb Count** | 86 (largest processor!) |
| **Window Required** | NO |
| **Documentation** | [file/](../../../docs/usertalk/docserver.userland.com/file/index.html) (111 doc files) |
| **Stub Implementation** | [headless_file_verbs.c](../../../tests/headless_file_verbs.c) |

---

## Category Assessment

**Category:** ✅ **Mixed - Core + Script + Legacy**

**Rationale:**
File operations processor with ~30-35 essential kernel primitives, ~25-30 script-based helpers, ~15-20 Mac Classic legacy verbs (resource forks, type/creator, aliases), and ~5-8 GUI dialogs. Many verbs are already implemented as UserTalk scripts built on kernel primitives.

**Headless Compatibility:** ✅ **Partial** (~50-60/86 verbs compatible)

**Blocking Verbs:**
- Mac Classic legacy: resource forks, type/creator codes, bundles, labels, versions, comments, icon positions
- Finder integration: findapplication, isvisible, setvisible

---

## Verb Categorization

### Tier 1: Core Kernel Primitives (Essential - C Implementation)

**File I/O Operations (12 verbs):**
- `open` - Open file for reading/writing
- `close` - Close open file
- `read` - Read bytes from file
- `write` - Write bytes to file
- `readline` - Read line from file
- `writeline` - Write line to file
- `endoffile` - Check if at EOF
- `setendoffile` - Truncate/extend file
- `getendoffile` - Get file size
- `setposition` - Seek to position
- `getposition` - Get current position
- `compare` - Compare two files

**File Operations (10 verbs):**
- `exists` - Check if file/folder exists
- `delete` - Delete file or empty folder
- `rename` - Rename file/folder
- `copy` - Copy file
- `move` - Move file/folder
- `new` - Create new file
- `newfolder` - Create new folder
- `size` - Get file size
- `lock` - Lock file
- `unlock` - Unlock file

**File Metadata (6 verbs):**
- `created` - Get creation date (Frontier epoch)
- `modified` - Get modification date (Frontier epoch)
- `setcreated` - Set creation date
- `setmodified` - Set modification date
- `isfolder` - Check if path is folder
- `islocked` - Check if file is locked

**Path Operations (7 verbs):**
- `fullpath` - Get absolute path
- `getpath` - Get working directory
- `setpath` - Set working directory
- `filefrompath` - Extract filename from path
- `folderfrompath` - Extract folder from path
- `getpathchar` - Get path separator character
- `getPosixPath` - Convert to POSIX path (Unix-style)

**Special Folders (2 verbs):**
- `getsystemfolderpath` - Get system folder path
- `getspecialfolderpath` - Get special folder path (e.g., temp, home)

**Total Tier 1: 37 verbs** (kernel primitives in C)

### Tier 2: Script-Based Helpers (Built on Primitives)

These are likely implemented as UserTalk scripts:
- `readwholefile` - Read entire file as string
- `writewholefile` - Write string to file
- `findinfile` - Search for text in file
- `countlines` - Count lines in file
- `copydatafork` - Copy file data (same as copy on modern systems)

**Estimated Tier 2: 5-10 verbs** (will work once Tier 1 implemented)

### Tier 3: Volume Operations (Headless-Compatible)

**Volume Info (8 verbs):**
- `isvolume` - Check if path is volume
- `freespaceonvolume` - Get free space (32-bit in legacy, 64-bit in modern headless)
- `freespaceonvolumedouble` - Get free space (64-bit)
- `volumesize` - Get volume size (32-bit in legacy, 64-bit in modern headless)
- `volumesizedouble` - Get volume size (64-bit)
- `volumeblocksize` - Get block size
- `filesonvolume` - Count files on volume
- `foldersonvolume` - Count folders on volume

**Volume Management (4 verbs):**
- `eject` - Eject removable volume
- `isejectable` - Check if volume is ejectable
- `unmountvolume` - Unmount volume
- `mountservervolume` - Mount network volume (SMB/NFS)

**Total Tier 3: 12 verbs**

### Tier 4: Mac Classic Legacy (PARTIAL Implementation)

**Resource Forks (OBSOLETE - Mac OS 9):**
- `copyresourcefork` - Copy resource fork (obsolete concept) - SKIP

**Type/Creator Codes (Implement Getters Only):**
- `type` - Get 4-char type code (extension-based: ".txt" → "txt ", >4 chars → "????") - IMPLEMENT
- `creator` - Get 4-char creator code (spaces on non-Mac platforms) - IMPLEMENT
- `settype` - Set type code (NOT implemented on Windows in legacy) - STUB (error on non-Mac)
- `setcreator` - Set creator code (NOT implemented on Windows in legacy) - STUB (error on non-Mac)

**Aliases (Mac-Specific - KEEP):**
- `newalias` - Create Mac alias (macOS semantic alias, not symlink)
- `isalias` - Check if file is Mac alias
- `followalias` - Resolve Mac alias to target

**Links (Cross-Platform - NEW):**
- `newlink` - Create symlink/junction (POSIX symlinks on Unix/Mac, NTFS junctions on Windows)
- `islink` - Check if file is symlink/junction
- `followlink` - Resolve symlink/junction to target

**File Attributes (Mac Classic):**
- `hasbundle` - Check bundle bit
- `setbundle` - Set bundle bit
- `getlabel` - Get Finder label
- `setlabel` - Set Finder label
- `getLabelIndex` - Get label index
- `setLabelIndex` - Set label index
- `getLabelNames` - Get label names
- `getversion` - Get version resource
- `setversion` - Set version resource
- `getfullversion` - Get full version
- `setfullversion` - Set full version
- `getcomment` - Get Finder comment
- `setcomment` - Set Finder comment
- `geticonpos` - Get icon position
- `seticonpos` - Set icon position

**Finder Integration:**
- `findapplication` - Find app for file type
- `isbusy` - Check if file is in use

**Total Tier 4: ~23 verbs** (skip for headless)

### Tier 5: Dialog Verbs (Headless-Compatible via stdio)

**Dialog Verbs (Implemented as stdio-based interactive prompts):**
- `getfiledialog` - Interactive file selection prompt (with path completion)
- `putfiledialog` - Interactive file save prompt (with path completion)
- `getfolderdialog` - Interactive folder selection prompt
- `getdiskdialog` - Interactive disk/volume selection prompt

**Rationale:** Instead of GUI dialogs, implement as stdio prompts with readline-style input. Enables interactive applications over terminal/SSH while maintaining portability.

**See:** [Headless Interactive Mode Planning](../HEADLESS_INTERACTIVE_MODE.md) for `--batch` flag, TTY detection, and implementation details.

**Implementation Status:**
- **Phase 1 (✅ Complete):** Infrastructure implemented (`--batch` flag, TTY detection, error fallbacks)
- **Phase 2 (Planned):** Stdio-based interactive prompts for dialog verbs

**Visibility (Mac Classic - NOT Implemented):**
- `isvisible` - Check if file is visible in Finder (skip - Finder-specific)
- `setvisible` - Set file visibility flag (skip - Finder-specific)

**Total Tier 5: 4 verbs implemented (as prompts), 2 verbs skipped (Finder-specific)**

### Tier 6: Specialized

**Media Metadata:**
- `getmp3info` - Extract MP3 metadata (ID3 tags)

**Total Tier 6: 1 verb**

---

## Summary by Compatibility

| Category | Count | Headless? | Implementation |
|----------|-------|-----------|----------------|
| **Tier 1: Core Primitives** | 37 | ✅ YES | C kernel verbs (essential) |
| **Tier 2: Script Helpers** | ~8 | ✅ YES | UserTalk scripts (auto-work) |
| **Tier 3: Volume Ops** | 12 | ✅ YES | C kernel verbs |
| **Tier 4: Type/Creator** | 4 | ✅ YES | Getters implement (type, creator); Setters stub (error on non-Mac) |
| **Tier 4: Aliases** | 3 | ✅ YES | Keep Mac-specific aliases (newalias, isalias, followalias) |
| **Tier 4: Links** | 3 | ✅ YES | New cross-platform verbs (newlink, islink, followlink) |
| **Tier 4: Other Legacy** | ~16 | ❌ NO | Skip (resource forks, bundles, labels, versions, comments, icons) |
| **Tier 5: Dialog Verbs** | 4 | ✅ YES | stdio-based interactive prompts |
| **Tier 5: Visibility** | 2 | ❌ NO | Skip (Finder-specific) |
| **Tier 6: Specialized** | 1 | ⚠️ MAYBE | Optional (MP3 metadata) |
| **TOTAL** | **86** | **~67 YES** | **~49 kernel + 8 script + 4 type/creator + 6 aliases/links + 4 dialogs** |

---

## Implementation Analysis

### Complexity: **HIGH** (but phased approach reduces risk)

### Dependencies

- **Other Processors:** None (foundational)
- **External Services:** None (except mountservervolume needs SMB/NFS)
- **OS-Specific Functionality:** YES (all file operations are OS-specific)
- **GUI/Window Context:** Only 6 dialog verbs

### Key Implementation Notes

**File Handles:**
Frontier maintains a table of open files. Each file.open() returns a reference (likely via setting a global or using the path as the handle).

**Path Handling:**
- Legacy Frontier used Mac-style paths with ":" separators
- Modern should support both ":" (legacy) and "/" (POSIX)
- `getpathchar()` returns path separator for platform
- `getPosixPath()` converts to Unix-style paths

**Timestamps:**
- Uses Frontier 1904 epoch (same as database, date processor)
- `created` and `modified` return 64-bit Frontier timestamps
- Must convert to/from Unix timestamps at OS boundary

**Core File I/O Pattern:**
```c
// Open file
file.open("/path/to/file.txt")

// Read/write
while (!file.endOfFile("/path/to/file.txt"))
    local(line = file.readLine("/path/to/file.txt"))
    // process line

// Always close
file.close("/path/to/file.txt")
```

**Platform Abstraction Needed:**
```c
// File operations - standard POSIX/C
fopen(), fclose(), fread(), fwrite(), fseek(), ftell()
stat(), mkdir(), rename(), unlink(), rmdir()

// Platform-specific timestamps
// macOS/Linux: stat.st_birthtime (creation), stat.st_mtime (modification)
// Windows: GetFileTime()

// Volume operations
// macOS: statfs()
// Linux: statfs() or statvfs()
// Windows: GetDiskFreeSpaceEx()
```

**Script-Based Verbs:**
Many helper verbs are implemented in UserTalk:
```usertalk
// file.readWholeFile() - likely implemented as:
on readWholeFile(path)
    local(result = "")
    file.open(path)
    while !file.endOfFile(path)
        result = result + file.readLine(path) + "\r\n"
    file.close(path)
    return result

// file.writeWholeFile() - similar pattern
// file.countLines() - loop with counter
// file.findInFile() - search each line
```

**Resource Forks (IGNORE):**
- Obsolete Mac OS 9 concept
- Modern macOS uses extended attributes (not resource forks)
- Skip `copyresourcefork` entirely

**Type/Creator Codes (IGNORE):**
- 4-character codes from Mac Classic (e.g., "TEXT", "JPEG")
- Replaced by file extensions and MIME types
- Skip type/creator/settype/setcreator verbs

**Aliases (Mac-Specific) & Links (Cross-Platform):**
- **Mac aliases:** Semantic shortcuts that survive refactoring, specific to macOS; keep `newalias`, `isalias`, `followalias`
- **Symlinks/Junctions:** Path-based links, portable across platforms; implement new `newlink`, `islink`, `followlink`
  - **Unix/macOS:** Standard POSIX symlinks via `symlink()`
  - **Windows:** NTFS junctions via `CreateSymbolicLink()` (requires appropriate permissions)
- **Strategy:** Both coexist; users choose `file.newAlias()` for macOS semantics or `file.newLink()` for portability

**Volume Operations:**
Essential for server operations:
- Disk space monitoring (`freespaceonvolume`)
- Network volume mounting (`mountservervolume`)
- Volume stats for storage management

---

## UserTalk Documentation Notes

**Core File I/O:**

**file.open(path)**
- Opens file for reading/writing
- Returns true if successful
- Must call file.close() when done
- Error if file already open by another process

**file.close(path)**
- Closes open file
- Should always be called (even after errors)

**file.readLine(path)**
- Reads one line from file
- Advances file position
- Returns string without newline

**file.writeLine(path, text)**
- Writes line to file with newline
- Advances file position

**file.read(path, count)**
- Reads count bytes from file
- Returns binary data

**file.write(path, data)**
- Writes data to file
- Handles binary data

**file.readWholeFile(path)**
- **Script-based** convenience verb
- Opens, reads entire file, closes
- Returns string with all content
- Note: "This verb is implemented as a script"

**file.writeWholeFile(path, content)**
- **Script-based** convenience verb
- Opens, writes content, closes
- Handles all file operations

**File Operations:**

**file.exists(path)**
- Returns true if file/folder exists
- Good practice to check before operations

**file.delete(path)**
- Deletes file or empty folder
- No confirmation dialog
- Error if folder not empty, file locked, or doesn't exist

**file.rename(oldpath, newpath)**
- Renames file or folder
- Can also move file (if paths in same volume)

**file.copy(source, dest)**
- Copies file
- Preserves metadata

**file.size(path)**
- Returns file size in bytes
- For folders, returns 0 or undefined

**Path Operations:**

**file.fullPath(relativePath)**
- Converts relative to absolute path
- Resolves based on current working directory

**file.fileFromPath(path)**
- Extracts filename from full path
- Example: "/usr/bin/ls" → "ls"

**file.folderFromPath(path)**
- Extracts folder from full path
- Example: "/usr/bin/ls" → "/usr/bin"

**file.getPathChar()**
- Returns platform path separator
- "/" on Unix/macOS, "\" on Windows
- Legacy Frontier used ":"

**file.getPosixPath(path)**
- Converts path to POSIX format (Unix-style with /)
- Useful for cross-platform scripts

**Volume Operations:**

**file.freeSpaceOnVolumeDouble(path)**
- Returns free space in bytes (64-bit)
- Use "Double" version for >2GB volumes
- Essential for storage monitoring

**file.volumeSizeDouble(path)**
- Returns total volume size (64-bit)

**file.mountServerVolume(protocol, server, volume, user, password)**
- Mounts network volume (SMB/NFS/AFP)
- Returns mount point path
- Useful for server-to-server operations

---

## Testing Requirements

**Minimum Test Cases Per Tier:**

**Tier 1 Core (37 verbs):**
- File I/O: Create, write, read, close sequence
- Edge cases: Empty files, large files, binary data
- Error handling: Non-existent files, permission errors
- Position/seek: Random access, tell, seek to end
- Locks: Lock, unlock, attempt to open locked file

**Tier 2 Scripts (~8 verbs):**
- Will auto-work once Tier 1 primitives implemented
- Test readWholeFile, writeWholeFile convenience

**Tier 3 Volumes (12 verbs):**
- Volume info on different filesystem types
- Network volume mounting (if applicable)
- Ejectable media detection

**Test Scenarios:**
```usertalk
// Basic file I/O
file.new("/tmp/test.txt")
file.open("/tmp/test.txt")
file.writeLine("/tmp/test.txt", "Hello, World!")
file.close("/tmp/test.txt")
file.exists("/tmp/test.txt")                → true
file.readWholeFile("/tmp/test.txt")         → "Hello, World!\n"

// File operations
file.copy("/tmp/test.txt", "/tmp/test2.txt")
file.rename("/tmp/test2.txt", "/tmp/renamed.txt")
file.delete("/tmp/renamed.txt")

// Path operations
file.fileFromPath("/usr/local/bin/frontier") → "frontier"
file.folderFromPath("/usr/local/bin/frontier") → "/usr/local/bin"
file.getPathChar()                           → "/" (Unix) or "\" (Windows)

// Volume operations
file.freeSpaceOnVolumeDouble("/")            → large number (bytes)
file.volumeSizeDouble("/")                   → larger number

// Binary data
file.open("/tmp/binary.dat")
file.write("/tmp/binary.dat", binaryData)
file.close("/tmp/binary.dat")

// Error handling
file.open("/nonexistent/path")               → false (error)
file.delete("/tmp/locked-file")              → error
```

**Platform-Specific Tests:**
- Path separators (/, \, :)
- Case sensitivity (Unix vs Windows)
- Permission handling
- Symlink behavior
- Network paths (UNC on Windows, NFS on Unix)

---

## Implementation Effort

**Estimated Time:** 50-75 hours (phased approach)

**Phase 1: Core Primitives (20-30 hours)**
- File I/O: open, close, read, write, readline, writeline (8-12 hours)
- File ops: exists, delete, rename, copy, move, new, newfolder (6-8 hours)
- Metadata: created, modified, size, isfolder, islocked, lock, unlock (4-6 hours)
- Position: setposition, getposition, endoffile, setendoffile, getendoffile (2-4 hours)

**Phase 2: Path & Volume Ops (10-15 hours)**
- Path operations: fullpath, filefrompath, folderfrompath, getpathchar, getPosixPath (4-6 hours)
- Volume info: freespaceonvolume, volumesize, etc. (4-6 hours)
- Volume mgmt: eject, mount, unmount (2-3 hours)

**Phase 3: Aliases & Links (5-10 hours)**
- Mac aliases: newalias, isalias, followalias (2-3 hours, macOS-specific)
- Cross-platform links: newlink, islink, followlink (3-7 hours, Unix/Windows variation)

**Phase 4: Dialog Verbs (5-10 hours)**
- Interactive prompts: getfiledialog, putfiledialog, getfolderdialog, getdiskdialog
- Implementation: readline-style input with path completion/history (5-10 hours)

**Phase 5: Script-Based (Auto)**
- Most script verbs will work once primitives exist
- May need to verify/update scripts

**Phase 6: Testing (10-12 hours)**
- Unit tests for all Tier 1 verbs
- Integration tests for aliases/links/dialogs
- Platform-specific testing (macOS, Linux, Windows)
- Edge case and error handling tests

**NOT Implementing:**
- Tier 4: Other Mac Legacy (~20 verbs) - resource forks, type/creator, bundles, labels, versions, comments, icons, finder attributes
- Tier 5: Finder Integration (2 verbs) - isvisible, setvisible
- Saved: ~15-20 hours

**Confidence:** MEDIUM-HIGH - Core POSIX file operations are well-understood, but scope is large

---

## Priority & Sequencing

**Priority:** 🏆 **CRITICAL** (Tier 0 - Foundational)

**Recommended Implementation Order:** 1 (highest priority after stub verbs)

**Blockers/Prerequisites:** None (foundational processor)

**Implementation Sequence:**

**Phase 1 (Critical - 20-30 hours):**
1. File I/O primitives: open, close, read, write, readline, writeline
2. Basic file ops: exists, delete, rename, copy, new, newfolder
3. Essential metadata: size, created, modified, isfolder
4. Position/seek: setposition, getposition, endoffile

**Phase 2 (High Priority - 10-15 hours):**
5. Path operations: fullpath, filefrompath, folderfrompath, getpathchar
6. Volume operations: freespaceonvolume, volumesize
7. File locking: lock, unlock, islocked
8. Advanced: compare, getPosixPath

**Phase 3 (Medium Priority - 5-10 hours):**
9. Volume management: mount, unmount, eject
10. File Aliases: newalias, isalias, followalias (macOS-specific)

**Phase 4 (High Priority - 5-10 hours):**
11. Cross-platform Links: newlink, islink, followlink (new verbs for portability)

**Phase 5 (High Priority - 5-10 hours):**
12. Dialog Prompts: getfiledialog, putfiledialog, getfolderdialog, getdiskdialog (stdio-based interactive)

**Phase 6 (Optional):**
13. Specialized: getmp3info (optional)

**Skip (Not Implementing):**
- Tier 4 (Other Mac Legacy) - ~20 verbs (resource forks, type/creator, bundles, labels, versions, comments, icons)
- Tier 5 (Finder Integration) - 2 verbs (isvisible, setvisible)

---

## Headless Compatibility Analysis

**Fully Compatible: ~67/86 verbs**

**Tier 1 - Core Primitives (37 verbs):** ✅ ALL headless-compatible
**Tier 2 - Script Helpers (~8 verbs):** ✅ ALL headless-compatible (built on Tier 1)
**Tier 3 - Volume Ops (12 verbs):** ✅ ALL headless-compatible
**Tier 4 - Type/Creator Getters (2 verbs):** ✅ Headless-compatible (extension-based type codes)
**Tier 4 - Type/Creator Setters (2 verbs):** ⚠️ Stubbed (error on non-Mac, following legacy)
**Tier 4 - Aliases (3 verbs):** ✅ Mac-specific aliases (headless-compatible on macOS)
**Tier 4 - Links (3 verbs - NEW):** ✅ Cross-platform symlinks/junctions (headless-compatible)
**Tier 5 - Dialog Verbs (4 verbs):** ✅ stdio-based interactive prompts (headless-compatible)

**Not Compatible: ~18/86 verbs**

**Tier 4 - Other Mac Legacy (~16 verbs):** ❌ Skip (Finder-specific, no cross-platform equivalents)
- Resource forks, bundles, labels, versions, comments, icons

**Tier 5 - Finder Integration (2 verbs):** ❌ Skip (Finder-specific)
- isvisible, setvisible - Finder metadata only

**Recommendation:** Implement Tiers 1-3 + Type/Creator + Aliases + Links + Dialog Prompts (67 verbs), skip Other Mac Legacy + Finder Integration (18 verbs)

**P2 Future Enhancement:** Extend settype/setcreator to work on Windows/Linux (rename extension or extended attributes)

---

## Related Processors

- **string** - Text operations often combined with file reading
- **sys** - System operations (shell commands complement file ops)
- **launch** - Process launching (uses file paths)
- **tcp/http** - Network file transfer
- **sqlite/mysql** - Database files

---

## Special Considerations

**File Handles & Open Files:**
Frontier tracks open files globally. Implementation needs:
- File handle table (path → file descriptor mapping)
- Automatic cleanup on script termination
- Error if file already open

**Path Compatibility:**
Support both legacy and modern path formats:
- Legacy: "System:Users:username:file.txt" (Mac Classic)
- POSIX: "/Users/username/file.txt" (modern Unix)
- Windows: "C:\Users\username\file.txt"

Provide `getPosixPath()` for conversion.

**Timestamps & Epochs:**
- Frontier uses 1904 epoch (same as database, date processor)
- OS uses 1970 epoch (Unix time)
- Must convert at boundaries:
  ```c
  frontier_time = unix_time + 2082844800LL;  // 1904 epoch offset
  ```

**Binary vs Text Mode:**
- `read`/`write` handle binary data
- `readline`/`writeline` handle text with line endings
- Cross-platform line ending handling (\n vs \r\n vs \r)

**Large File Support:**
- Use 64-bit file operations (fseeko, ftello, stat64)
- `freeSpaceOnVolumeDouble` and `volumeSizeDouble` return 64-bit values
- Support files >2GB

**Security Considerations:**
- **Path Traversal:** Validate paths to prevent ".." attacks
- **Symlink Attacks:** Be careful with symlink following
- **Permission Checks:** Respect file system permissions
- **Temp File Security:** Use secure temp file creation
- **Disk Quotas:** Handle "disk full" errors gracefully

**Network Volumes:**
`mountServerVolume()` is complex:
- macOS: Use NetFS.framework or `mount_smbfs`
- Linux: Use `mount` command with CIFS/NFS
- Windows: Use WNetAddConnection2
- Requires credentials (security sensitive!)
- May need privileged access

**Script-Based Verbs:**
Many high-level verbs are UserTalk scripts:
- `readWholeFile` / `writeWholeFile`
- `countLines`
- `findInFile`
- Various folder enumeration helpers

These will automatically work once kernel primitives are implemented. May need to verify script compatibility with headless mode.

**Mac Classic Legacy (IGNORE):**

**Resource Forks:**
- Obsolete storage mechanism (Mac OS 9)
- Modern macOS uses extended attributes
- Skip `copyresourcefork` entirely

**Type/Creator Codes (Implement Getters Only):**
- **file.type()** - Get extension as 4-char code (".txt" → "txt ", ">4 chars → "????")
- **file.creator()** - Get creator code (spaces on non-Mac)
- **file.settype() / file.setcreator()** - NOT implemented on Windows in legacy; follow legacy pattern (error on non-Mac)
- **P2 TODO:** Extend settype/setcreator to work on all platforms (rename extension on Windows/Linux)

**Aliases & Links:**
- **Aliases (macOS):** Implement `newalias`, `isalias`, `followalias` for semantic Mac aliases (survive refactoring)
- **Links (Cross-platform):** Implement new `newlink`, `islink`, `followlink` for portable symlinks/junctions
  - Unix/Mac: Standard POSIX `symlink()`
  - Windows: NTFS junctions via `CreateSymbolicLink()`
- **Strategy:** Both coexist; users choose based on portability needs

**Dialog Verbs (stdio-based interactive prompts):**
- **`getfiledialog()`** - Interactive file selection prompt with path completion and history
- **`putfiledialog()`** - Interactive file save prompt with path suggestions
- **`getfolderdialog()`** - Interactive folder selection prompt
- **`getdiskdialog()`** - Interactive disk/volume selection prompt
- **Implementation:** Use readline-style prompts to enable interactive scripts in terminal/SSH sessions
- **Benefits:** Portable (works anywhere), no GUI dependencies, server-side friendly
- **Fallback:** Non-interactive environments can error or accept stdin input

**Finder Attributes:**
- Labels (color tags), comments, version strings, icon positions
- All Finder-specific metadata
- Skip all ~15 Finder attribute verbs

**Error Handling:**
Frontier's error model:
- Verbs return `false` on error
- Error message set in `lasterror` global (or similar)
- Some operations throw errors that halt script execution

Need consistent error handling across all file verbs.

---

## Implementation Strategy

### Phased Approach (Recommended)

**Phase 1: Core File I/O (Week 1-2)**
- Implement 12 file I/O primitives
- Get basic read/write working
- Test on all platforms

**Phase 2: File Operations (Week 3)**
- Implement 10 file operation verbs
- Copy, delete, rename, move, etc.
- Cross-platform testing

**Phase 3: Metadata & Paths (Week 4)**
- Implement 13 metadata/path verbs
- Timestamp handling (1904 epoch)
- Path conversion utilities

**Phase 4: Volume Operations (Week 5)**
- Implement 12 volume verbs
- Disk space, mounting, etc.
- Network volume support

**Phase 5: Testing & Polish (Week 6)**
- Comprehensive test suite
- Error handling
- Documentation
- Performance optimization

**Total:** 6 weeks for full Tier 1-3 implementation

### Quick Win Subset

For immediate functionality, implement these 15 verbs first:
1. `open`, `close`, `read`, `write`, `readline`, `writeline`
2. `exists`, `delete`, `rename`, `copy`, `new`, `newfolder`
3. `size`, `created`, `modified`

This provides essential file operations in ~15-20 hours.

---

## P2 Enhancements (Future)

**P2 TODO: Extend settype/setcreator to all platforms**
- Currently: settype/setcreator error on non-Mac (follow legacy behavior)
- Enhancement: Implement cross-platform settype (rename file extension) and setcreator (store as metadata)
- Windows: settype could update file extension or extended attributes
- Linux: settype could update xattr user.file.type, setcreator could store xattr user.file.creator
- Effort: ~5-10 hours for future implementation

---

## References

**Documentation:**
- DocServer: `docs/usertalk/docserver.userland.com/file/` (111 HTML files)

**Implementation:**
- Stub: `tests/headless_file_verbs.c`
- Canonical verb list: `Common/resources/Win32/kernelverbs.rc` (lines 584-673)

**Standards:**
- POSIX: fopen(), stat(), rename(), unlink(), mkdir(), rmdir()
- Large File Support: fseeko(), ftello(), stat64()
- Network: SMB (CIFS), NFS, AFP
- Timestamps: Frontier 1904 epoch conversion

---

## Next Steps

1. ✅ Audit complete - ready for phased implementation
2. ⏳ Prioritize Phase 1: Core File I/O primitives (12 verbs)
3. ⏳ Create platform abstraction layer for file operations
4. ⏳ Implement file handle tracking system
5. ⏳ Implement timestamp conversion (1904 ↔ 1970 epoch)
6. ⏳ Write unit tests for each verb
7. ⏳ Test on macOS, Linux, Windows
8. ⏳ Phase 2: Path & Volume operations
9. ⏳ Phase 3: Mac Aliases (newalias, isalias, followalias)
10. ⏳ Phase 4: Cross-platform Links (newlink, islink, followlink)
11. ⏳ Phase 5: Dialog Prompts (getfiledialog, putfiledialog, getfolderdialog, getdiskdialog)
12. ⏳ Update implementation status

---

**Audit Status:** ✅ Complete and Approved for Phased Implementation

**Implementation Strategy:**
- **Phase 1 (Critical):** Core file I/O + basic operations (37 verbs, 20-30 hours)
- **Phase 2 (High Priority):** Paths, volumes, metadata (25 verbs, 10-15 hours)
- **Phase 3-4 (High Priority):** Aliases + Cross-platform Links (6 verbs, 10-15 hours)
- **Phase 5 (High Priority):** Dialog Prompts (4 verbs, 5-10 hours)
- **Skip:** Other Mac Legacy (~20 verbs - resource forks, type/creator, etc.) + Finder Integration (2 verbs)
- **Auto-work:** Script-based helpers once primitives exist

**Key Success Factors:**
1. Platform abstraction layer for portability (file I/O, aliases, links)
2. Proper 1904 epoch handling
3. Secure path validation (prevent traversal attacks)
4. Comprehensive error handling
5. Interactive prompt implementation (readline-style with path completion)
6. Careful distinction between semantic aliases (Mac) and path-based links (cross-platform)
