# file.write() and file.writeline() Implementation

## Summary

Fixed `file.write()` and `file.writeline()` to work correctly in headless mode by implementing string mode parameter parsing in `file.open()`.

## Root Cause

The portable layer's `openfilefunc` implementation (in `portable/fileverbs_portable.c`) was only accepting a **boolean** parameter for read-only mode, but the standard Frontier API uses **string modes** like "r", "w", "r+", "a", etc.

When `file.open(f, "w")` was called, the string "w" was being coerced to boolean `true`, which caused the file to be opened in read-only mode ("rb"). Subsequently, `file.write()` would fail with errno=EBADF (Bad file descriptor) because writing to a read-only file is not permitted.

## Solution

Enhanced `openfilefunc` in `portable/fileverbs_portable.c` to support both:
1. **String modes** (e.g., "r", "w", "r+", "w+", "a", "a+") - standard Frontier API
2. **Boolean modes** (true = readonly, false = read/write) - backward compatibility

The implementation:
- Attempts to parse parameter 2 as a string mode first
- Falls back to boolean mode if string parsing fails
- Automatically appends 'b' (binary mode) if not present
- Maps string modes to appropriate fopen() modes

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
