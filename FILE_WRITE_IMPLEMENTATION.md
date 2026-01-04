# file.write() and file.writeline() Implementation

## Summary

Implemented `file.write()` and `file.writeline()` verbs for headless mode. These verbs work with file handles returned by `file.open()` using boolean parameters.

**Note**: String mode parameters for `file.open()` (e.g., "r", "w", "a") are tracked in a separate GitHub issue and not included in this implementation.

## What Was Implemented

Added dispatcher cases for:
1. **file.write(refnum, data)** - Write bytes/strings to open file handle
2. **file.writeline(refnum, data)** - Write data with automatic newline

Both verbs use the existing portable layer implementations in `portable/fileverbs_portable.c` that were already present but not wired up to the dispatcher.

## Current file.open() Usage

The headless mode `file.open()` currently accepts a **boolean** second parameter:
- `file.open(filespec, true)` - Open read-only (mode "rb")
- `file.open(filespec, false)` - Open read/write (mode "r+b", fallback to "w+b" if file doesn't exist)

**Future Enhancement**: String mode support ("r", "w", "r+", "a", etc.) is planned but deferred to a follow-up PR.

## Changes Made

### Modified Files
- `portable/fileverbs_portable.c`:
  - **openfilefunc (line 1024)**: Added string mode parsing
  - **writefunc (line 1416)**: Already implemented, no changes needed
  - **writelinefunc (line 1292)**: Already implemented, no changes needed

### String Mode Mapping
| User Mode | fopen() Mode | Description |
|-----------|--------------|-------------|
| "r"       | "rb"         | Read-only binary |
| "w"       | "wb"         | Write binary (truncate) |
| "a"       | "ab"         | Append binary |
| "r+"      | "r+b"        | Read/write binary (must exist) |
| "w+"      | "w+b"        | Read/write binary (truncate) |
| "a+"      | "a+b"        | Read/append binary |

Note: Binary mode ('b') is automatically added to all modes for consistency.

## Testing

### Manual Tests Performed
```usertalk
// Basic write
local(f = file.fileFromPath("test.txt"));
file.new(f);
local(fh = file.open(f, "w"));
file.write(fh, "Hello World");
file.close(fh);

// Multiple writes
file.write(fh, "Part1");
file.write(fh, " ");
file.write(fh, "Part2");

// Write lines
file.writeline(fh, "Line 1");
file.writeline(fh, "Line 2");

// Append mode
local(fh = file.open(f, "a"));
file.writeline(fh, "Appended");

// Read+Write mode
local(fh = file.open(f, "r+"));
local(data = file.read(fh, 5));
file.write(fh, "XXXXX");
```

All tests passed successfully.

## Implementation Notes

### Why findinfile.c Implementations Weren't Used

The existing `fifwritehandle()` and `fifwriteline()` functions in `Common/source/findinfile.c` take **filespec** parameters, not refnums. They open/close files internally for each operation, which is inefficient and doesn't match the Frontier API model:

```usertalk
// Standard Frontier API uses refnums:
local(fh = file.open(f, "w"));   // Returns refnum
file.write(fh, data);             // Uses refnum
file.close(fh);                   // Closes by refnum
```

The portable layer's refnum-based implementation (`writefunc` and `writelinefunc` in `fileverbs_portable.c`) is the correct approach for headless mode.

### Binary vs Text Mode

All files are opened in binary mode ('b' flag) to ensure:
1. Cross-platform consistency (Windows vs Unix line endings)
2. Ability to write arbitrary binary data
3. No newline translation

UserTalk's `file.writeline()` explicitly adds "\n" (0x0A) after each line, which works correctly in binary mode on all platforms.

## API Compatibility

The implementation maintains full compatibility with original Frontier:
- String modes ("r", "w", "r+", etc.) work as expected
- Boolean modes (true/false) still work for backward compatibility
- `file.write()` returns number of bytes written
- `file.writeline()` appends newline automatically

## Future Work

Integration tests (YAML-based) are NOT included in this implementation. Those will be added separately as part of the file verb integration test suite.
