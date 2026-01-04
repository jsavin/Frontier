# Headless Path Construction - Cheat Sheet

## One-Liner Patterns

### Get Current Directory
```usertalk
file.fullPath(file.getPath())
```

### Build Path to subdirectory
```usertalk
file.fullPath(file.getPath()) + "/" + "subdir"
```

### Build Multi-level Path
```usertalk
file.fullPath(file.getPath()) + "/" + "dir1" + "/" + "dir2" + "/" + "file.txt"
```

### Store in Variable (Cleaner)
```usertalk
local (cwd = file.fullPath(file.getPath())); return cwd + "/" + "tmp"
```

## CLI Test Commands

```bash
# Get cwd
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli -e 'return file.fullPath(file.getPath())'

# Build path
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli -e 'return file.fullPath(file.getPath()) + "/" + "tmp"'

# Multi-level
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli -e 'return file.fullPath(file.getPath()) + "/" + "databases" + "/" + "Frontier-v7.root"'
```

## Real Examples

### Database Path
```usertalk
file.fullPath(file.getPath()) + "/" + "databases" + "/" + "Frontier-v7.root"
```

### Test Fixture
```usertalk
file.fullPath(file.getPath()) + "/" + "tests" + "/" + "fixtures" + "/" + "sample.json"
```

### Tmp Directory
```usertalk
file.fullPath(file.getPath()) + "/" + "tmp"
```

### Source Code
```usertalk
file.fullPath(file.getPath()) + "/" + "Common" + "/" + "source"
```

## Type Reference

| Type | How to Use | Example |
|------|-----------|---------|
| `fss` (file record) | Don't concatenate, convert with `file.fullPath()` | `file.getPath()` |
| `string` (path string) | Concatenate with `+` operator | `"path" + "/" + "subdir"` |

## Dos and Don'ts

| Do | Don't |
|----|-------|
| ✅ Use `file.fullPath()` to convert | ❌ Use `file.fromPath()` (extracts filename) |
| ✅ Keep paths as strings until needed | ❌ Use `file.new()` unless creating |
| ✅ Use `/` as separator everywhere | ❌ Mix `\` and `/` |
| ✅ Build paths from variables | ❌ Hardcode absolute paths |
| ✅ Test with CLI `-e` option | ❌ Assume working directory is shell CWD |

## Pattern Templates

### Multi-component builder
```usertalk
local (cwd = file.fullPath(file.getPath()));
local (path = cwd + "/" + "a" + "/" + "b" + "/" + "c");
return path
```

### Registry of paths
```usertalk
local (paths = [
    "tmp": file.fullPath(file.getPath()) + "/" + "tmp",
    "db": file.fullPath(file.getPath()) + "/" + "databases",
    "tests": file.fullPath(file.getPath()) + "/" + "tests"
]);
return paths["tmp"]
```

### Parameterized builder
```usertalk
on makePath(...parts) {
    local (p = file.fullPath(file.getPath()));
    for part in parts { p = p + "/" + part; }
    return p;
}
return makePath("databases", "Frontier-v7.root")
```

## Quick Troubleshooting

| Error | Cause | Fix |
|-------|-------|-----|
| "Type error" | Trying to concatenate file record | Use `file.fullPath()` first |
| "Can't create file" | Used `file.new()` on existing path | Only use for creation |
| Wrong path | Used shell's CWD assumption | Remember cwd is frontier-cli's dir |
| `fromPath` not found | Used wrong verb | Use `fullPath` instead |

## File Verb Quick Ref

```usertalk
file.getPath()              // → fss (file record) - get cwd
file.fullPath(fs)           // → string - convert fss to string
file.fromPath(s)            // → string - extract filename from string path
file.new(path)              // → boolean - CREATE file/folder (use carefully!)
file.setPath(fs)            // → boolean - change cwd
```

## Understanding the System

```
frontier-cli starts
  ↓
init_default_working_dir(argv[0]'s directory)
  ↓
Stores in thread-local storage
  ↓
UserTalk script runs:
  file.getPath()            // → Returns from thread-local
  file.fullPath(fs)         // → Converts fss to string
  concat with +             // → Builds any path
```

## Complete Minimal Example

```usertalk
// 1. Get cwd as string
local (cwd = file.fullPath(file.getPath()));

// 2. Build path
local (tmpDir = cwd + "/" + "tmp");

// 3. Use the string path
return tmpDir  // Ready to use anywhere
```

---

**See Also:**
- `HEADLESS_PATH_CONSTRUCTION_SUMMARY.md` - Complete quick start
- `USERTALK_HEADLESS_PATH_CONSTRUCTION.md` - Full reference
- `HEADLESS_PATH_CONSTRUCTION_EXAMPLES.md` - 20+ examples
