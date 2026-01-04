# File Locking Implementation - Quick Reference

**Status**: Design Complete - Ready for Implementation
**Date**: 2026-01-03

---

## TL;DR - Executive Decision

**Use metadata-based file locking (file attributes), NOT process-level advisory locks (flock/fcntl).**

**Why**: The legacy Mac OS Classic implementation used filesystem metadata flags (`kFSNodeLockedMask`), which persist across process restarts. Using process-level locks would break compatibility with UserTalk scripts that expect locks to survive Frontier restarts.

---

## Implementation Summary

### macOS (Primary Platform)

**API**: `chflags()` with `UF_IMMUTABLE` flag

```c
// Lock file
chflags(path, st.st_flags | UF_IMMUTABLE);

// Unlock file
chflags(path, st.st_flags & ~UF_IMMUTABLE);

// Check lock state
stat(path, &st);
boolean locked = (st.st_flags & UF_IMMUTABLE) != 0;
```

**Advantages**:
- ✅ Matches legacy behavior exactly
- ✅ No root privileges required (file owner can lock/unlock)
- ✅ Visible in Finder (shows padlock icon)
- ✅ Persists across reboots

### Linux (Secondary Platform)

**API**: `ioctl(FS_IOC_SETFLAGS)` with `FS_IMMUTABLE_FL` flag

```c
// Lock file
int fd = open(path, O_RDONLY);
ioctl(fd, FS_IOC_GETFLAGS, &flags);
flags |= FS_IMMUTABLE_FL;
ioctl(fd, FS_IOC_SETFLAGS, &flags);
close(fd);
```

**CRITICAL LIMITATION**:
- ❌ **Requires root privileges** (CAP_LINUX_IMMUTABLE capability)
- ❌ file.lock() will fail for non-root users on Linux
- ⚠️ Document prominently in user-facing docs

**Possible Fallback**: Store lock state in extended attributes (`user.frontier.locked`) for non-root users - purely advisory, no OS enforcement.

### Windows

**Status**: Not supported in initial implementation

**Future Approach**: Combine `FILE_ATTRIBUTE_READONLY` + extended attribute tracking

---

## Platform Support Matrix

| Platform | API | Root Required | Finder Visible | Persists | Supported |
|----------|-----|---------------|----------------|----------|-----------|
| macOS | `chflags(UF_IMMUTABLE)` | No | Yes (padlock) | Yes | ✅ Full |
| Linux | `ioctl(FS_IMMUTABLE_FL)` | **Yes** | No | Yes | ⚠️ Limited |
| Windows | N/A | N/A | N/A | N/A | ❌ Not yet |

---

## Key Behavior (Per Legacy Docs)

1. **Folders cannot be locked**: `file.lock()` succeeds as no-op, but `file.islocked()` returns false
2. **Volumes cannot be locked** (from software): Hardware locks are different
3. **Advisory only**: Applications can ignore the flag if they have permission
4. **Persistent**: Lock survives process termination and reboot
5. **No coordination**: No deadlock detection, no inter-process lock queuing

---

## API to Implement

### C Functions (portable/fileverbs_portable.c)

```c
boolean lockfile_portable(const ptrfilespec fs);
boolean unlockfile_portable(const ptrfilespec fs);
boolean fileislocked_portable(const ptrfilespec fs, boolean *fllocked);
```

### UserTalk API

```usertalk
file.lock(f)      // Lock file, returns true/false
file.unlock(f)    // Unlock file, returns true/false
file.islocked(f)  // Check if locked, returns true/false
```

---

## Testing Checklist

### Unit Tests
- [ ] file.lock() sets immutable flag
- [ ] file.unlock() clears immutable flag
- [ ] file.islocked() queries flag correctly
- [ ] Folders always return false for islocked()
- [ ] Locked file prevents deletion (macOS)

### Integration Tests (YAML)
- [ ] Lock/unlock round-trip
- [ ] Lock persistence across queries
- [ ] Folder locking behavior

### Manual Verification
- [ ] macOS: Padlock icon in Finder
- [ ] macOS: `ls -lO` shows `uchg` flag
- [ ] Linux: `lsattr` shows `i` flag (root only)
- [ ] Linux: Non-root permission error

---

## Open Questions for User

1. **Linux fallback**: Should we use extended attributes (xattrs) for non-root users? This would be purely advisory with no OS enforcement.

2. **Error handling**: Should `file.lock()` on Linux return false silently, or raise a UserTalk error when not running as root?

3. **Windows priority**: Required for MVP or defer to future release?

4. **Network filesystems**: Document limitations or detect and error?

---

## Files to Create/Modify

### New Files
- `portable/fileverbs_portable.c` - Platform-specific lock implementations
- `portable/fileverbs_portable.h` - Function prototypes
- `tests/headless_file_verbs_lock.c` - Unit tests for locking

### Modified Files
- `Common/source/fileverbs.c` - Add conditional compilation for portable builds
- `frontier-cli/Makefile` - Add fileverbs_portable.c to build
- `tests/integration/verbs/file/lock.yaml` - Integration tests

---

## Next Steps

1. Get user approval on design approach
2. Decide on Linux non-root fallback strategy
3. Implement macOS version first (highest compatibility)
4. Add unit tests + integration tests
5. Document platform limitations in CLAUDE.md
6. Create PR with comprehensive test coverage

---

**Full Design**: See `planning/file_locking_design.md` (8000+ words)
