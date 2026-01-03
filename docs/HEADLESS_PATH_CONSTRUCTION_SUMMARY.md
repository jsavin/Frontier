# Quick Start: Headless Path Construction in UserTalk

## The Answer: One-Line Path Construction

To construct a path to the tmp directory (or any relative path) in Frontier headless mode:

```usertalk
file.fullPath(file.getPath()) + "/" + "tmp"
```

**That's it.** This works regardless of:
- Where you run frontier-cli from
- Whether it's a symlink
- Whether you're in CI/CD or local development

---

## Why It Works

### 1. `file.getPath()` Gets the Working Directory

- **Returns**: File record (type `fss`) representing thread-local working directory
- **Working Directory**: Always set to the frontier-cli binary's directory
- **Thread-safe**: Each thread has its own working directory

### 2. `file.fullPath()` Converts to String

- **Input**: File record from `file.getPath()`
- **Output**: String representation of the path
- **Why needed**: Can only concatenate strings, not file records

### 3. `"/" + "..."` Concatenates Path Components

- UserTalk's `+` operator works for string concatenation
- Works on all platforms (macOS, Linux, Windows)
- Safe: Frontier normalizes paths internally

---

## What You Get

When you run:
```bash
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
    -e 'return file.fullPath(file.getPath()) + "/" + "tmp"'
```

You get:
```
/Users/jake/dev/jsavin/Frontier/tmp
```

(The actual path depends on where frontier-cli is located)

---

## Common Patterns

### Single Level: Just the directory
```usertalk
file.fullPath(file.getPath()) + "/" + "databases"
```

### Multiple Levels: Nested paths
```usertalk
file.fullPath(file.getPath()) + "/" + "databases" + "/" + "Frontier-v7.root"
```

### From Variables: Parameterized paths
```usertalk
local (
    cwd = file.fullPath(file.getPath()),
    dir1 = "tests",
    dir2 = "fixtures",
    file = "sample.json"
);
return cwd + "/" + dir1 + "/" + dir2 + "/" + file
```

### Store Multiple Paths: Path registry
```usertalk
local (paths = [
    "tmp": file.fullPath(file.getPath()) + "/" + "tmp",
    "db": file.fullPath(file.getPath()) + "/" + "databases",
    "tests": file.fullPath(file.getPath()) + "/" + "tests"
]);
return paths["tmp"] + "/" + "test.txt"
```

---

## Critical Gotchas

### Gotcha 1: Don't Use `file.fromPath()`

❌ **WRONG** - `file.fromPath()` takes a STRING and extracts just the filename:
```usertalk
local (path = file.fromPath(file.getPath()) + "/" + "tmp")  // Type error!
```

✅ **CORRECT** - Use `file.fullPath()` to convert file record to string:
```usertalk
local (path = file.fullPath(file.getPath()) + "/" + "tmp")  // Works!
```

### Gotcha 2: Don't Use `file.new()` Unless Creating Files

❌ **WRONG** - This actually CREATES the file/directory:
```usertalk
local (tmpDir = file.new(file.fullPath(file.getPath()) + "/" + "tmp"))
```

✅ **CORRECT** - Keep paths as strings until you need to create something:
```usertalk
local (tmpPath = file.fullPath(file.getPath()) + "/" + "tmp")
// Now tmpPath is a string you can use
```

### Gotcha 3: Working Directory is NOT Shell CWD

The working directory is always where frontier-cli is located, NOT where you run the command from:

```bash
# Even if you're in a different directory:
cd /some/other/place
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
    -e 'return file.fullPath(file.getPath())'
# Still returns: /Users/jake/dev/jsavin/Frontier
# NOT: /some/other/place
```

This is a FEATURE - it makes paths reproducible across any environment.

---

## Real-World Examples

### Access Database Files
```usertalk
local (dbPath = file.fullPath(file.getPath()) + "/" + "databases" + "/" + "Frontier-v7.root");
return dbPath
```

### Access Fixture Data
```usertalk
local (fixturePath = file.fullPath(file.getPath()) + "/" + "tests" + "/" + "fixtures" + "/" + "sample.json");
return fixturePath
```

### Create Path Function for Reuse
```usertalk
on buildPath(...components) {
    local (path = file.fullPath(file.getPath()));
    for item in components {
        path = path + "/" + item;
    }
    return path;
}

return buildPath("tests", "fixtures", "data.json")
```

---

## File Record Type System

Understanding the type system prevents confusion:

| Function | Input | Output | Use For |
|----------|-------|--------|---------|
| `file.getPath()` | (none) | File record (`fss`) | Getting working directory |
| `file.fullPath(fs)` | File record | String | Converting to string for concatenation |
| `file.fromPath(s)` | String | String (just filename) | Extracting filename from path |
| `file.new(s)` | String | Boolean | **Creating** files/folders |
| `file.setPath(fs)` | File record | Boolean | Changing working directory |

**Remember**: Concatenation only works with strings, so always use `file.fullPath()` before `+`.

---

## Testing Your Path Construction

Simple test to verify it works:

```bash
# Test 1: Get cwd
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
    -e 'return file.fullPath(file.getPath())'

# Test 2: Construct single-level path
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
    -e 'return file.fullPath(file.getPath()) + "/" + "tmp"'

# Test 3: Construct multi-level path
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
    -e 'return file.fullPath(file.getPath()) + "/" + "databases" + "/" + "Frontier-v7.root"'
```

All three should return valid absolute paths (no errors).

---

## Performance

- `file.getPath()` - O(1), just thread-local lookup
- `file.fullPath()` - O(1), just type conversion
- String concatenation - O(n) for total length
- **No filesystem calls** during path construction

Safe to use in loops and time-critical code.

---

## Full Documentation

For detailed information:
- **`USERTALK_HEADLESS_PATH_CONSTRUCTION.md`** - Complete reference and architecture
- **`HEADLESS_PATH_CONSTRUCTION_EXAMPLES.md`** - 20+ working examples and patterns
- **`portable/file_working_dir.h`** - API documentation
- **`portable/fileverbs_portable.c`** - Implementation

---

## Summary

**One-line solution:**
```usertalk
file.fullPath(file.getPath()) + "/" + "tmp"
```

**Three key facts:**
1. `file.getPath()` returns the working directory (always frontier-cli's directory)
2. `file.fullPath()` converts it to a string
3. `+` concatenates strings to build any path you need

**Never:**
- Use `file.fromPath()` (extracts filename only)
- Use `file.new()` unless creating files
- Assume working directory is shell's CWD

That's all you need to dynamically construct paths in Frontier headless mode!
