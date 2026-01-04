# Constructing Dynamic Paths in Frontier Headless Mode

## Overview

When running Frontier in headless mode (`FRONTIER_HEADLESS_RUN_STARTUP=1`), you can construct dynamic paths to files without hardcoding absolute paths. This guide explains how to use `file.getPath()` and `file.setPath()` along with string concatenation to build paths relative to the working directory.

## Key Concepts

### 1. Thread-Local Working Directory

Frontier maintains a thread-local working directory for each execution thread:

- **Initialized at startup** via `init_default_working_dir()` in `frontier-cli/main.c`
- **Default value** is the directory containing the frontier-cli binary (or "." as fallback)
- **Thread-safe**: Each thread has its own working directory that can be changed independently
- **Persistent within thread**: Persists across multiple file operations in the same execution

### 2. File Path Verbs

Frontier provides key path manipulation verbs for headless mode:

#### `file.getPath()` - Get Thread-Local Working Directory
- **Parameters**: None
- **Returns**: A file record (type: `fss`) representing the current working directory
- **Implementation**: Calls `filegetdefaultpath()` in `portable/file_portable.c`
- **Headless Support**: Full support via thread-local working directory system

#### `file.fullPath(fileSpec)` - Convert File Record to Path String
- **Parameters**: A file record (fileSpec)
- **Returns**: String representation of the full path
- **Implementation**: Portable file verb implementation in `portable/fileverbs_portable.c`
- **Use**: Convert `file.getPath()` result to string for concatenation and construction

#### `file.setPath(fileSpec)` - Set Thread-Local Working Directory
- **Parameters**: A file record (path specification)
- **Returns**: Boolean (true if successful)
- **Implementation**: Calls `filesetdefaultpath()` via `portable/file_portable.c`
- **Headless Support**: Full support via thread-local working directory system

### 3. Path Construction Strategy

The strategy for constructing paths in headless mode:

```
1. Get current working directory with file.getPath()
2. Convert file record to string with file.fullPath()
3. Concatenate path components using string "+" operator
4. Use string paths directly with file verbs that accept string parameters
5. Only use file.new() if you actually want to CREATE the file/folder
```

## Implementation Details

### How File Path Verbs Work

**file.getPath() Implementation Chain**:
```
UserTalk: file.getPath()
  → fileverbs.c: case filegetpathfunc
    → portable/file_portable.c: filegetdefaultpath()
      → get_thread_working_dir() [thread-local lookup]
        → pathtofilespec() [convert to file record]
```

**file.setPath() Implementation Chain**:
```
UserTalk: file.setPath(fs)
  → fileverbs.c: case filesetpathfunc
    → portable/file_portable.c: filesetdefaultpath()
      → filespectopath() [convert to path string]
        → set_thread_working_dir() [store in thread-local]
```

### Working Directory Initialization in frontier-cli

From `frontier-cli/main.c` lines 95-110:

1. **If executable is a symlink**, follow it to find real location
2. **Extract directory** containing the frontier-cli binary
3. **Call init_default_working_dir()** with that directory
4. **Fallback** to "." (current directory) if resolution fails

**Important**: The working directory is set to the frontier-cli binary's directory, NOT the current shell directory. This is critical for reproducible paths in headless scripts.

### File Record vs String Paths

Frontier distinguishes between:

- **File Records** (`fss` type): Returned by `file.getPath()`
  - Returned as result of path operations
  - Cannot be directly concatenated with strings
  - Convert to string with `file.fullPath()` for path construction
  - **Important**: `file.new()` CREATES the file/folder - only use if that's your intent

- **Path Strings**: Text representations of paths
  - Can be built via concatenation using "+" operator
  - Work with most file verbs directly (they accept string parameters)
  - Example: "/Users/jake/dev/jsavin/Frontier/tmp"
  - Preferred approach: Keep paths as strings for construction and manipulation

### Path Concatenation in UserTalk

UserTalk uses the "+" operator for both string concatenation AND path construction:

```usertalk
// String concatenation
local (path = "/Users/jake" + "/dev")

// File path concatenation with explicit separator
local (path = "/Users/jake" + "/" + "dev" + "/" + "jsavin")

// Safer approach using string variables
local (basePath = "/Users/jake/dev")
local (tmpDir = "tmp")
local (fullPath = basePath + "/" + tmpDir)
```

## Practical Example: Constructing Tmp Directory Path

### Single-Line Script (for unit testing)

The simplest approach - convert working directory to string and concatenate:

```usertalk
file.fullPath(file.getPath()) + "/" + "tmp"
```

This returns the full path as a string ready for use with file operations.

### Multi-Line Script (for readability)

```usertalk
on getTmpPath() {
    // Get current working directory as file record
    local (cwdFileRecord = file.getPath());

    // Convert file record to path string
    local (cwdString = file.fullPath(cwdFileRecord));

    // Construct tmp directory path
    local (tmpPath = cwdString + "/" + "tmp");

    return tmpPath;
}

return getTmpPath()
```

### Complete Unit Test Example

```usertalk
// Single line for CLI:
return file.fullPath(file.getPath()) + "/" + "tmp"

// This works in:
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli -e 'return file.fullPath(file.getPath()) + "/" + "tmp"'

// Result (if running from /Users/jake/dev/jsavin/Frontier):
// "/Users/jake/dev/jsavin/Frontier/tmp"
```

## Critical Implementation Notes

### 1. No Need to Hardcode Absolute Paths

The working directory initialization in `frontier-cli/main.c` resolves symlinks and extracts the directory containing the binary. This means:

- ✅ Scripts work regardless of where frontier-cli is symlinked from
- ✅ Paths are relative to the Frontier project root
- ✅ Works consistently in CI/CD and local development

### 2. File Record Conversion is Required

You CANNOT concatenate file records directly with strings. Use `file.fullPath()` to convert:

```usertalk
// ✗ WRONG - type error (can't concatenate file record with string)
local (path = file.getPath() + "/" + "tmp")

// ✓ CORRECT - convert to string first with file.fullPath()
local (path = file.fullPath(file.getPath()) + "/" + "tmp")
```

### 3. Thread-Local Safety

Each thread has its own working directory:

```usertalk
// Thread 1: Changes working directory
file.setPath(file.new("path1"))
// Does NOT affect Thread 2

// Thread 2: Still has original working directory
local (cwd = file.getPath())  // Gets original cwd
```

This is why the implementation stores working directory in `tythreadglobals` rather than a global variable.

## Source Code References

### Key Files for Understanding Path Operations

| File | Purpose |
|------|---------|
| `portable/file_working_dir.h` | API for thread-local working directory |
| `portable/file_portable.c` | filegetdefaultpath() / filesetdefaultpath() implementations |
| `Common/source/fileverbs.c` | file.getPath() / file.setPath() verb dispatch (lines 3009-3037) |
| `frontier-cli/main.c` | Working directory initialization (lines 95-110) |

### Related Verbs

- `file.getPath()` - Get current working directory
- `file.setPath(fs)` - Set current working directory
- `file.new(path)` - Create file record from path string
- `file.fromPath(fs)` - Convert file record to path string
- `file.fullPath(fs)` - Get full path of a file

## Debugging Working Directory Issues

### Verify Working Directory

```bash
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli -e 'return file.fromPath(file.getPath())'
```

Expected output: Directory containing frontier-cli binary

### Check Where Frontier-cli Is Located

```bash
which frontier-cli
# or
ls -la frontier-cli/frontier-cli
```

### Verify Path Construction

```bash
# This should output the tmp directory path
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli \
    -e 'return file.fromPath(file.getPath()) + "/" + "tmp"'
```

## Best Practices

1. **Always convert file records to strings before concatenation**
   ```usertalk
   // Good
   local (path = file.fromPath(file.getPath()) + "/" + "subdir")
   ```

2. **Use explicit "/" separators (not platform-dependent)**
   - Frontier's file verbs normalize paths on all platforms
   - "/" works correctly on macOS, Linux, Windows

3. **Don't assume current shell directory**
   - Working directory is set to frontier-cli binary location
   - Use `file.getPath()` to be certain

4. **Test path construction in both contexts**
   - Single-line CLI: `./frontier-cli/frontier-cli -e '...'`
   - Multi-line scripts: `./frontier-cli/frontier-cli -f script.ut`

## Performance Considerations

- `file.getPath()` is O(1) - just returns thread-local working directory string
- `file.fromPath()` is O(1) - just converts internal representation to string
- String concatenation with "+" is O(n) for string length
- No filesystem calls are made to construct paths

Path construction is efficient and suitable for use in tight loops.

## See Also

- `docs/TESTING_GUIDE.md` - CLI usage and testing patterns
- `portable/file_working_dir.h` - Complete API documentation
- `tests/file_verb_tests.c` - Working examples of file verb usage
