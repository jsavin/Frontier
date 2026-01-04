# File Locking Design for Portable Frontier

**Document**: File Locking Design for file.lock(), file.unlock(), file.islocked()
**Author**: System Architect
**Date**: 2026-01-03
**Status**: Design Phase - Implementation Not Started

---

## Executive Summary

This document provides a research-based design for implementing portable file locking verbs (`file.lock()`, `file.unlock()`, `file.islocked()`) in Frontier's headless/portable build. The legacy Mac OS Classic/macOS implementation used filesystem metadata flags (`kFSNodeLockedMask`) to mark files as locked, which is a **metadata-based approach** rather than process-level file locking. This design recommends continuing this semantic by using platform-specific file attribute flags where available, with clear limitations documented.

**Key Decision**: Use **metadata-based locking** (file attributes) to match legacy behavior, NOT process-level advisory locks (flock/fcntl).

---

## Table of Contents

1. [Legacy Behavior Analysis](#legacy-behavior-analysis)
2. [Portable File Locking Research](#portable-file-locking-research)
3. [Design Decision: Metadata vs. Advisory Locks](#design-decision-metadata-vs-advisory-locks)
4. [Recommended Implementation Approach](#recommended-implementation-approach)
5. [API Design](#api-design)
6. [Platform-Specific Implementation Details](#platform-specific-implementation-details)
7. [Limitations and Caveats](#limitations-and-caveats)
8. [Testing Strategy](#testing-strategy)
9. [Open Questions](#open-questions)
10. [References](#references)

---

## Legacy Behavior Analysis

### Mac OS Classic/macOS Implementation

The legacy Frontier implementation (from `Common/source/fileops.m`) used macOS filesystem metadata:

```c
boolean lockfile (const ptrfilespec fs) {
    FSRef fsref;
    FSCatalogInfo catinfo;

    clearbytes (&catinfo, sizeof (catinfo));

    setfserrorparam (fs); // in case error message takes a filename parameter

    if (oserror (macgetfsref (fs, &fsref)))
        return (false);

    if (oserror (FSGetCatalogInfo (&fsref, kFSCatInfoNodeFlags, &catinfo, NULL, NULL, NULL)))
        return (false);

    catinfo.nodeFlags |= kFSNodeLockedMask;  // SET FILESYSTEM METADATA FLAG

    if (oserror (FSSetCatalogInfo (&fsref, kFSCatInfoNodeFlags, &catinfo)))
        return (false);

    return (true);
}

boolean unlockfile (const ptrfilespec fs) {
    FSRef fsref;
    FSCatalogInfo catinfo;

    clearbytes (&catinfo, sizeof (catinfo));

    setfserrorparam (fs); // in case error message takes a filename parameter

    if (oserror (macgetfsref (fs, &fsref)))
        return (false);

    if (oserror (FSGetCatalogInfo (&fsref, kFSCatInfoNodeFlags, &catinfo, NULL, NULL, NULL)))
        return (false);

    catinfo.nodeFlags &= ~kFSNodeLockedMask;  // CLEAR FILESYSTEM METADATA FLAG

    if (oserror (FSSetCatalogInfo (&fsref, kFSCatInfoNodeFlags, &catinfo)))
        return (false);

    return (true);
}

boolean fileislocked (const ptrfilespec fs, boolean *fllocked) {
    tyfileinfo info;

    if (!filegetinfo (fs, &info))
        return (false);

    *fllocked = info.fllocked;  // READ FILESYSTEM METADATA FLAG

    return (true);
}
```

### Key Characteristics of Legacy Implementation

1. **Persistent Metadata**: Lock state is stored in filesystem metadata (catalog info), NOT process state
2. **Survives Process Termination**: Lock persists even after Frontier exits
3. **Visible in Finder**: macOS Finder displays padlock icon for locked files
4. **File Attribute, Not File Descriptor Lock**: Lock is tied to the inode/file, not an open file descriptor
5. **Advisory, Not Mandatory**: Applications can still modify the file if they ignore the flag
6. **No Inter-Process Coordination**: No deadlock detection, no process-level coordination

### Expected UserTalk API

From legacy documentation (`docs/usertalk/docserver.userland.com/file/lock.html`):

```usertalk
file.lock ("C:\\Program Files\\Frontier\\sample.txt")
file.isLocked ("C:\\Program Files\\Frontier\\sample.txt")
  >> true

file.unlock ("C:\\Program Files\\Frontier\\sample.txt")
file.isLocked ("C:\\Program Files\\Frontier\\sample.txt")
  >> false
```

**Documentation Notes**:
- "Locks the file; keeping it from being modified."
- "If the file being locked is in an open folder in the Finder, the padlock symbol that denotes a locked file appears only if you close the folder and then re-open it."
- `file.isLocked()` returns `false` for folders and volumes (only files can be locked)

---

## Portable File Locking Research

### POSIX Advisory Locking Options

There are three main POSIX-style file locking mechanisms available on Unix-like systems:

#### Option 1: `flock()` - BSD-style Advisory Locks

**Availability**:
- macOS: ✅ Yes (BSD heritage)
- Linux: ✅ Yes (since Linux 2.0)
- Windows: ❌ No (requires emulation)

**Characteristics**:
- Locks are bound to **file descriptors**, not processes
- Locks **entire files** (no byte-range locking)
- Locks are **inherited across fork() and exec()**
- **No interaction** with `fcntl()` locks on most systems
- **BSD vs. Linux divergence**: On FreeBSD, `flock()` and `fcntl()` locks interact; on Linux they don't
- **NFS behavior**: On Linux 2.6.12+, NFS emulates `flock()` as `fcntl()` byte-range locks; older systems treat it as a no-op

**API**:
```c
#include <sys/file.h>

int flock(int fd, int operation);
// operation: LOCK_SH (shared), LOCK_EX (exclusive), LOCK_UN (unlock)
//            LOCK_NB (non-blocking)
```

#### Option 2: `fcntl()` - POSIX Advisory Locks

**Availability**:
- macOS: ✅ Yes (POSIX-compliant)
- Linux: ✅ Yes (POSIX-compliant)
- Windows: ❌ No (requires different API: `LockFileEx`)

**Characteristics**:
- Locks are bound to **(pid, inode) pairs**, not file descriptors
- Supports **byte-range locking** (can lock part of a file)
- **Closing ANY fd** to an inode releases **ALL locks** on that inode for that process
- **Thread-unsafe by default**: All threads in a process share the same locks
- **Deadlock detection** on some systems
- **NFS support**: Works over NFSv3/NFSv4 (with caveats)

**API**:
```c
#include <fcntl.h>

struct flock {
    short l_type;    // F_RDLCK, F_WRLCK, F_UNLCK
    short l_whence;  // SEEK_SET, SEEK_CUR, SEEK_END
    off_t l_start;   // Offset where lock begins
    off_t l_len;     // Number of bytes to lock (0 = whole file)
    pid_t l_pid;     // PID of process holding lock
};

int fcntl(int fd, F_SETLK, struct flock *);  // Non-blocking
int fcntl(int fd, F_SETLKW, struct flock *); // Blocking
int fcntl(int fd, F_GETLK, struct flock *);  // Query lock
```

#### Option 3: `lockf()` - POSIX Wrapper Around fcntl()

**Availability**:
- macOS: ✅ Yes (implemented as `fcntl()`)
- Linux: ✅ Yes (implemented as `fcntl()`)
- Windows: ❌ No

**Characteristics**:
- Simpler API than `fcntl()`, but **same underlying mechanism**
- On macOS and Linux, `lockf()` is documented to be equivalent to `fcntl()`
- Same limitations as `fcntl()` (process-bound, thread-unsafe)

**API**:
```c
#include <unistd.h>

int lockf(int fd, int cmd, off_t len);
// cmd: F_LOCK (exclusive), F_TLOCK (non-blocking exclusive),
//      F_ULOCK (unlock), F_TEST (test for lock)
```

### Metadata-Based Approaches (File Attributes)

#### macOS: `chflags()` System Call

**Availability**: macOS only (BSD heritage)

**Relevant Flags**:
- `UF_IMMUTABLE` (uchg): User-settable immutable flag - prevents deletion, renaming, or modification
- `SF_IMMUTABLE` (schg): System immutable flag - same as above, but only root can clear it
- `UF_APPEND` (uappend): Only allow appending to the file
- `SF_APPEND` (sappend): System append-only flag

**Equivalent to Legacy Behavior**:
The `UF_IMMUTABLE` flag is the closest modern equivalent to the legacy `kFSNodeLockedMask`:
- Visible in Finder (shows padlock icon)
- Persists across reboots
- Prevents modification by applications that respect the flag
- User-level flag (doesn't require root to set/clear)

**API**:
```c
#include <sys/stat.h>
#include <unistd.h>

int chflags(const char *path, u_int flags);
int fchflags(int fd, u_int flags);

// Get current flags
struct stat st;
stat(path, &st);
u_int current_flags = st.st_flags;

// Set immutable
chflags(path, current_flags | UF_IMMUTABLE);

// Clear immutable
chflags(path, current_flags & ~UF_IMMUTABLE);
```

**Command-line Equivalent**:
```bash
# Lock file
chflags uchg /path/to/file.txt

# Unlock file
chflags nouchg /path/to/file.txt

# View flags
ls -lO /path/to/file.txt
```

#### Linux: `chattr()` / ioctl(FS_IOC_SETFLAGS)

**Availability**: Linux only (ext2/ext3/ext4/xfs/btrfs filesystems)

**Relevant Flags**:
- `FS_IMMUTABLE_FL` (i): Immutable flag - prevents modification, deletion, renaming
- `FS_APPEND_FL` (a): Append-only flag

**Equivalent to Legacy Behavior**:
The `FS_IMMUTABLE_FL` flag provides similar semantics:
- Persists across reboots
- Prevents modification by applications
- Requires CAP_LINUX_IMMUTABLE capability (typically root) to set/clear

**API**:
```c
#include <sys/ioctl.h>
#include <linux/fs.h>

int fd = open(path, O_RDONLY);
int flags;

// Get current flags
ioctl(fd, FS_IOC_GETFLAGS, &flags);

// Set immutable
flags |= FS_IMMUTABLE_FL;
ioctl(fd, FS_IOC_SETFLAGS, &flags);

// Clear immutable
flags &= ~FS_IMMUTABLE_FL;
ioctl(fd, FS_IOC_SETFLAGS, &flags);

close(fd);
```

**Command-line Equivalent**:
```bash
# Lock file (requires root)
sudo chattr +i /path/to/file.txt

# Unlock file (requires root)
sudo chattr -i /path/to/file.txt

# View flags
lsattr /path/to/file.txt
```

**CRITICAL LIMITATION**: On Linux, setting `FS_IMMUTABLE_FL` requires **root privileges** (CAP_LINUX_IMMUTABLE capability), unlike macOS where `UF_IMMUTABLE` can be set by the file owner.

---

## Design Decision: Metadata vs. Advisory Locks

### Why NOT Use flock()/fcntl() Advisory Locks

Process-level advisory locks (flock/fcntl) have **fundamentally different semantics** from the legacy Mac OS Classic behavior:

| Aspect | Legacy Mac Behavior (kFSNodeLockedMask) | flock()/fcntl() Behavior |
|--------|----------------------------------------|--------------------------|
| **Persistence** | Survives process termination | Released when process exits |
| **Visibility** | Shows padlock in Finder | Not visible in GUI |
| **Scope** | Filesystem metadata | Process/file descriptor state |
| **Inter-Process** | No coordination | Coordination between processes |
| **Intent** | Prevent accidental modification | Coordinate access between concurrent processes |
| **Thread Safety** | N/A (metadata) | Problematic (fcntl is process-bound) |

**Conclusion**: Using flock/fcntl would **break compatibility** with legacy UserTalk scripts that expect lock state to persist across Frontier restarts.

### Why Use Metadata-Based Approach

**Advantages**:
1. **Matches legacy semantics**: Lock persists across process restarts
2. **Visible to OS**: macOS Finder shows padlock icon (UF_IMMUTABLE)
3. **File-centric**: Lock is tied to the file, not a process
4. **Simple state tracking**: Query filesystem metadata, no need to track open file descriptors

**Disadvantages**:
1. **Platform-specific**: Different APIs on macOS vs. Linux
2. **Linux requires root**: Setting `FS_IMMUTABLE_FL` on Linux requires CAP_LINUX_IMMUTABLE
3. **Not truly portable**: Windows has no direct equivalent
4. **Advisory only**: Applications can ignore the flag if they have permission

---

## Recommended Implementation Approach

### Platform Support Matrix

| Platform | Implementation | API | Root Required | Notes |
|----------|---------------|-----|---------------|-------|
| **macOS** | `chflags(UF_IMMUTABLE)` | `chflags(path, flags)` | No (owner can set) | Closest to legacy behavior |
| **Linux** | `ioctl(FS_IOC_SETFLAGS, FS_IMMUTABLE_FL)` | ioctl() on ext4/xfs/btrfs | **Yes** (CAP_LINUX_IMMUTABLE) | Requires root - **major limitation** |
| **Windows** | **Not supported** | N/A | N/A | No portable equivalent |

### Implementation Strategy

**Phase 1: macOS Implementation (Highest Priority)**
- Use `chflags()` with `UF_IMMUTABLE` flag
- Matches legacy behavior exactly
- No root privileges required
- Full compatibility with existing UserTalk scripts

**Phase 2: Linux Implementation (With Limitations)**
- Use `ioctl(FS_IOC_SETFLAGS, FS_IMMUTABLE_FL)`
- Document root privilege requirement prominently
- Return error with descriptive message if not running as root
- Consider fallback: store lock state in extended attributes (user.frontier.locked) for non-root users

**Phase 3: Windows (Future Work)**
- Document as unsupported in initial release
- Possible future approach: Set FILE_ATTRIBUTE_READONLY + custom extended attribute
- Alternative: Store lock state in NTFS alternate data streams

---

## API Design

### C API Functions (in portable/fileverbs_portable.c)

```c
/* File locking verbs - metadata-based (file attribute flags) */

/* Lock a file by setting immutable flag (macOS: UF_IMMUTABLE, Linux: FS_IMMUTABLE_FL) */
boolean lockfile_portable(const ptrfilespec fs);

/* Unlock a file by clearing immutable flag */
boolean unlockfile_portable(const ptrfilespec fs);

/* Check if file has immutable flag set */
boolean fileislocked_portable(const ptrfilespec fs, boolean *fllocked);
```

### Platform-Specific Helpers (Internal)

```c
/* macOS-specific implementation */
#ifdef __APPLE__
boolean macos_set_immutable_flag(const char *path, boolean set);
boolean macos_get_immutable_flag(const char *path, boolean *is_immutable);
#endif

/* Linux-specific implementation */
#ifdef __linux__
boolean linux_set_immutable_flag(const char *path, boolean set);
boolean linux_get_immutable_flag(const char *path, boolean *is_immutable);
#endif
```

### Verb Dispatcher Integration

In `Common/source/fileverbs.c`:

```c
case filelockfunc: {
    tyfilespec fs;
    boolean fl;

    flnextparamislast = true;

    if (!getpathvalue (hp1, 1, &fs))
        break;

    if (fileisvolume (&fs))
        fl = lockvolume (&fs, true);  // Volume locking (already exists)
    else
#if defined(FRONTIER_PORTABLE) || defined(FRONTIER_HEADLESS)
        fl = lockfile_portable (&fs);  // Portable metadata-based locking
#else
        fl = lockfile (&fs);  // Legacy Mac implementation
#endif

    if (!fl)
        break;

    (*v).data.flvalue = true;

    return (true);
}

case fileunlockfunc: {
    tyfilespec fs;
    boolean fl;

    flnextparamislast = true;

    if (!getpathvalue (hp1, 1, &fs))
        break;

    if (fileisvolume (&fs))
        fl = lockvolume (&fs, false);  // Volume unlocking (already exists)
    else
#if defined(FRONTIER_PORTABLE) || defined(FRONTIER_HEADLESS)
        fl = unlockfile_portable (&fs);  // Portable metadata-based unlocking
#else
        fl = unlockfile (&fs);  // Legacy Mac implementation
#endif

    if (!fl)
        break;

    (*v).data.flvalue = true;

    return (true);
}

case fileislockedfunc: {
    boolean fl;
    tyfilespec fs;

    flnextparamislast = true;

    if (!getpathvalue (hp1, 1, &fs))
        break;

    if (fileisvolume (&fs))
        fl = isvolumelocked (&fs, &(*v).data.flvalue);
    else
#if defined(FRONTIER_PORTABLE) || defined(FRONTIER_HEADLESS)
        fl = fileislocked_portable (&fs, &(*v).data.flvalue);
#else
        fl = fileislocked (&fs, &(*v).data.flvalue);
#endif

    if (!fl)
        break;

    return (true);
}
```

---

## Platform-Specific Implementation Details

### macOS Implementation (chflags)

**File**: `portable/fileverbs_portable.c` (new file or add to existing)

```c
#ifdef __APPLE__
#include <sys/stat.h>
#include <unistd.h>

boolean macos_set_immutable_flag(const char *path, boolean set) {
    struct stat st;

    // Get current flags
    if (stat(path, &st) != 0) {
        // File doesn't exist or permission denied
        return false;
    }

    u_int new_flags;
    if (set) {
        new_flags = st.st_flags | UF_IMMUTABLE;
    } else {
        new_flags = st.st_flags & ~UF_IMMUTABLE;
    }

    // Set new flags
    if (chflags(path, new_flags) != 0) {
        // Permission denied or other error
        return false;
    }

    return true;
}

boolean macos_get_immutable_flag(const char *path, boolean *is_immutable) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return false;
    }

    *is_immutable = (st.st_flags & UF_IMMUTABLE) != 0;
    return true;
}

boolean lockfile_portable(const ptrfilespec fs) {
    char path[1024];

    // Convert tyfilespec to POSIX path
    if (!file_spec_to_posix_path(fs, path, sizeof(path))) {
        return false;
    }

    // Check if it's a folder (folders can't be locked per legacy docs)
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        // Return true but don't actually lock (legacy behavior)
        return true;
    }

    return macos_set_immutable_flag(path, true);
}

boolean unlockfile_portable(const ptrfilespec fs) {
    char path[1024];

    if (!file_spec_to_posix_path(fs, path, sizeof(path))) {
        return false;
    }

    // Check if it's a folder
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }

    return macos_set_immutable_flag(path, false);
}

boolean fileislocked_portable(const ptrfilespec fs, boolean *fllocked) {
    char path[1024];

    if (!file_spec_to_posix_path(fs, path, sizeof(path))) {
        return false;
    }

    // Folders are never locked (per legacy docs)
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        *fllocked = false;
        return true;
    }

    return macos_get_immutable_flag(path, fllocked);
}
#endif /* __APPLE__ */
```

### Linux Implementation (ioctl FS_IOC_SETFLAGS)

**File**: `portable/fileverbs_portable.c`

```c
#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>

boolean linux_set_immutable_flag(const char *path, boolean set) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    int flags;
    if (ioctl(fd, FS_IOC_GETFLAGS, &flags) < 0) {
        close(fd);
        return false;
    }

    if (set) {
        flags |= FS_IMMUTABLE_FL;
    } else {
        flags &= ~FS_IMMUTABLE_FL;
    }

    if (ioctl(fd, FS_IOC_SETFLAGS, &flags) < 0) {
        // Likely EPERM - need CAP_LINUX_IMMUTABLE
        close(fd);
        return false;
    }

    close(fd);
    return true;
}

boolean linux_get_immutable_flag(const char *path, boolean *is_immutable) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    int flags;
    if (ioctl(fd, FS_IOC_GETFLAGS, &flags) < 0) {
        close(fd);
        return false;
    }

    close(fd);
    *is_immutable = (flags & FS_IMMUTABLE_FL) != 0;
    return true;
}

boolean lockfile_portable(const ptrfilespec fs) {
    char path[1024];

    if (!file_spec_to_posix_path(fs, path, sizeof(path))) {
        return false;
    }

    // Check if it's a folder
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }

    if (!linux_set_immutable_flag(path, true)) {
        // Check if error is EPERM (permission denied)
        if (errno == EPERM) {
            // Log warning: "file.lock() requires root privileges on Linux"
            log_warn(LOG_COMP_FILE, "file.lock() failed: requires root privileges on Linux (CAP_LINUX_IMMUTABLE)");
        }
        return false;
    }

    return true;
}

boolean unlockfile_portable(const ptrfilespec fs) {
    char path[1024];

    if (!file_spec_to_posix_path(fs, path, sizeof(path))) {
        return false;
    }

    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }

    if (!linux_set_immutable_flag(path, false)) {
        if (errno == EPERM) {
            log_warn(LOG_COMP_FILE, "file.unlock() failed: requires root privileges on Linux (CAP_LINUX_IMMUTABLE)");
        }
        return false;
    }

    return true;
}

boolean fileislocked_portable(const ptrfilespec fs, boolean *fllocked) {
    char path[1024];

    if (!file_spec_to_posix_path(fs, path, sizeof(path))) {
        return false;
    }

    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        *fllocked = false;
        return true;
    }

    return linux_get_immutable_flag(path, fllocked);
}
#endif /* __linux__ */
```

---

## Limitations and Caveats

### macOS

1. **UF_IMMUTABLE** is advisory only - root can still modify the file
2. **Finder visibility**: Padlock icon appears, matching legacy behavior
3. **Permission requirements**: File owner can set/clear flag (no root required)
4. **Filesystem support**: Works on APFS, HFS+, UFS
5. **Network filesystems**: May not work on NFS/SMB mounts

### Linux

1. **Root privileges required**: Setting `FS_IMMUTABLE_FL` requires CAP_LINUX_IMMUTABLE capability (typically root)
   - **This is a MAJOR limitation** - file.lock() will fail for non-root users
2. **Filesystem support**: Only works on ext2/ext3/ext4/xfs/btrfs (not on FAT, NTFS, exFAT)
3. **Network filesystems**: Does NOT work on NFS/CIFS/SMB
4. **No GUI indication**: Unlike macOS, Linux file managers don't typically show immutable flag

**Workaround for Linux non-root users**: Consider fallback implementation using extended attributes:
- Store lock state in `user.frontier.locked` xattr (doesn't prevent modification, just tracks intent)
- Document that file.lock() on Linux is advisory only unless running as root

### Windows

1. **Not supported** in initial implementation
2. **Possible future approach**: Combine `FILE_ATTRIBUTE_READONLY` with extended attribute tracking
3. **No direct equivalent** to immutable flag

### General

1. **Advisory, not mandatory**: Applications that don't check the flag can still modify files
2. **No inter-process coordination**: Unlike flock/fcntl, no deadlock detection or lock queuing
3. **Not suitable for concurrent access control**: This is for preventing accidental modification, not coordinating concurrent writes

---

## Testing Strategy

### Unit Tests (headless_file_verbs.c)

```c
/* Test file.lock() sets immutable flag */
void test_file_lock_sets_flag(void) {
    // Create test file
    const char *test_file = "/tmp/frontier_lock_test.txt";
    write_test_file(test_file, "test content");

    // Lock file
    tyfilespec fs;
    pathtofilespec(test_file, &fs);
    boolean result = lockfile_portable(&fs);

    assert(result == true);

    // Verify immutable flag is set
    boolean is_locked;
    fileislocked_portable(&fs, &is_locked);
    assert(is_locked == true);

    // Verify file can't be deleted (on macOS)
#ifdef __APPLE__
    int unlink_result = unlink(test_file);
    assert(unlink_result == -1 && errno == EPERM);
#endif

    // Cleanup
    unlockfile_portable(&fs);
    unlink(test_file);
}

/* Test file.unlock() clears immutable flag */
void test_file_unlock_clears_flag(void) {
    const char *test_file = "/tmp/frontier_unlock_test.txt";
    write_test_file(test_file, "test content");

    tyfilespec fs;
    pathtofilespec(test_file, &fs);

    // Lock then unlock
    lockfile_portable(&fs);
    boolean result = unlockfile_portable(&fs);

    assert(result == true);

    // Verify flag is cleared
    boolean is_locked;
    fileislocked_portable(&fs, &is_locked);
    assert(is_locked == false);

    // Verify file can be deleted
    int unlink_result = unlink(test_file);
    assert(unlink_result == 0);
}

/* Test file.islocked() returns correct state */
void test_fileislocked_query(void) {
    const char *test_file = "/tmp/frontier_islocked_test.txt";
    write_test_file(test_file, "test content");

    tyfilespec fs;
    pathtofilespec(test_file, &fs);

    // Initially unlocked
    boolean is_locked;
    fileislocked_portable(&fs, &is_locked);
    assert(is_locked == false);

    // Lock and verify
    lockfile_portable(&fs);
    fileislocked_portable(&fs, &is_locked);
    assert(is_locked == true);

    // Unlock and verify
    unlockfile_portable(&fs);
    fileislocked_portable(&fs, &is_locked);
    assert(is_locked == false);

    // Cleanup
    unlink(test_file);
}

/* Test folders can't be locked (per legacy docs) */
void test_folders_cannot_be_locked(void) {
    const char *test_dir = "/tmp/frontier_dir_test";
    mkdir(test_dir, 0755);

    tyfilespec fs;
    pathtofilespec(test_dir, &fs);

    // Lock should succeed (no-op for folders)
    boolean result = lockfile_portable(&fs);
    assert(result == true);

    // But file.islocked() should return false
    boolean is_locked;
    fileislocked_portable(&fs, &is_locked);
    assert(is_locked == false);

    // Cleanup
    rmdir(test_dir);
}
```

### Integration Tests (YAML-based verb tests)

```yaml
# tests/integration/verbs/file/lock.yaml
test_file_lock_unlock:
  description: "Test file.lock() and file.unlock()"
  script: |
    local(tmpfile = file.tmpfile());
    file.writewholefile(tmpfile, "test content");

    // Lock file
    file.lock(tmpfile);
    assert(file.islocked(tmpfile), "File should be locked after file.lock()");

    // Unlock file
    file.unlock(tmpfile);
    assert(!file.islocked(tmpfile), "File should be unlocked after file.unlock()");

    // Cleanup
    file.delete(tmpfile);
    return true;
  expected_result: true

test_file_lock_persistence:
  description: "Test that lock state persists"
  script: |
    local(tmpfile = file.tmpfile());
    file.writewholefile(tmpfile, "persistent lock");

    // Lock file
    file.lock(tmpfile);

    // Query lock state (should persist)
    assert(file.islocked(tmpfile), "Lock should persist");

    // Cleanup
    file.unlock(tmpfile);
    file.delete(tmpfile);
    return true;
  expected_result: true

test_folder_locking:
  description: "Test that folders return false for file.islocked()"
  script: |
    local(tmpdir = file.gettemporaryfolder());

    // Folders should not be lockable
    file.lock(tmpdir);  // Should succeed as no-op
    assert(!file.islocked(tmpdir), "Folders should always return false for islocked()");

    return true;
  expected_result: true
  platform: ["macOS", "Linux"]  # Windows not supported
```

### Manual Testing Checklist

- [ ] macOS: Verify padlock icon appears in Finder after file.lock()
- [ ] macOS: Verify locked file cannot be deleted from Finder
- [ ] macOS: Verify `ls -lO` shows `uchg` flag
- [ ] Linux (root): Verify `lsattr` shows `i` flag after file.lock()
- [ ] Linux (non-root): Verify file.lock() fails with permission error
- [ ] Both platforms: Verify lock persists across Frontier restarts

---

## Open Questions

1. **Linux fallback strategy**: Should we provide a fallback for non-root users using extended attributes (xattrs) to store lock state? This would make the lock purely advisory with no OS enforcement.

2. **Error handling**: Should `file.lock()` fail silently (return false) on Linux when not running as root, or raise a UserTalk error with a message?

3. **Windows support priority**: Is Windows support required for MVP, or can it be deferred to a future release?

4. **Network filesystem behavior**: Should we document network filesystem limitations, or attempt to detect NFS/SMB and return specific errors?

5. **Alternative Linux approach**: Should we consider using file-based locking (creating .lock files) as a cross-platform fallback when immutable flags aren't available?

---

## References

### Web Sources

- [flock(2) - Linux manual page](https://man7.org/linux/man-pages/man2/flock.2.html)
- [Everything you never wanted to know about file locking - apenwarr](https://apenwarr.ca/log/20101213)
- [File locking in Linux](https://gavv.net/articles/file-locks/)
- [Advisory File Locking – POSIX and BSD locks](https://loonytek.com/2015/01/15/advisory-file-locking-differences-between-posix-and-bsd-locks/)
- [chflags Man Page - macOS - SS64.com](https://ss64.com/mac/chflags.html)
- [Apple OS X: Write Protect File From Command Line](https://www.cyberciti.biz/faq/apple-osx-write-protecting-file-folders-bash-command/)
- [Changing file permissions on macOS (and using flags) - Linux Audit](https://linux-audit.com/changing-file-permissions-on-macos-and-using-flags/)

### Code References

- Legacy implementation: `/Users/jake/dev/jsavin/Frontier-file-verbs-completion/Common/source/fileops.m` (lines 2398-2459)
- Verb dispatcher: `/Users/jake/dev/jsavin/Frontier-file-verbs-completion/Common/source/fileverbs.c` (lines 2795-2976)
- Documentation: `/Users/jake/dev/jsavin/Frontier-file-verbs-completion/docs/usertalk/docserver.userland.com/file/lock.html`

---

## Next Steps

1. **Review with user**: Get approval on metadata-based approach vs. advisory locks
2. **Decide on Linux fallback**: Extended attributes vs. fail-with-error for non-root users
3. **Create implementation PR**: Start with macOS implementation (highest compatibility)
4. **Add logging infrastructure**: Ensure log_warn() for Linux permission failures
5. **Write comprehensive tests**: Unit tests + integration tests + manual verification
6. **Document platform limitations**: Update CLAUDE.md and user-facing docs

---

**End of Design Document**
