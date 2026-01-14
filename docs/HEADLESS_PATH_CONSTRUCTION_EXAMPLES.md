# Headless Path Construction - Practical Examples

## Quick Reference

### Get the tmp Directory Path (Single Line)

```usertalk
file.fullPath(file.getPath()) + "/" + "tmp"
```

**CLI Usage**:
```bash
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli \
    -e 'return file.fullPath(file.getPath()) + "/" + "tmp"'
```

**Expected Output**:
```
/Users/jake/dev/jsavin/Frontier/tmp
```

---

## Common Patterns

### Pattern 1: Get Current Working Directory

```usertalk
file.fullPath(file.getPath())
```

Returns the current working directory as a path string.

**Test it**:
```bash
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli \
    -e 'return file.fullPath(file.getPath())'
```

**Output**: `/Users/jake/dev/jsavin/Frontier`

---

### Pattern 2: Construct Path with Multiple Components

```usertalk
local (cwd = file.fullPath(file.getPath()));
return cwd + "/" + "subdir1" + "/" + "subdir2" + "/" + "file.txt"
```

**Single-line version**:
```usertalk
file.fullPath(file.getPath()) + "/" + "subdir1" + "/" + "subdir2" + "/" + "file.txt"
```

**CLI Usage**:
```bash
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli \
    -e 'return file.fullPath(file.getPath()) + "/" + "subdir1" + "/" + "subdir2" + "/" + "file.txt"'
```

**Expected Output**:
```
/Users/jake/dev/jsavin/Frontier/subdir1/subdir2/file.txt
```

---

### Pattern 3: Build Path from Variables

```usertalk
local (
    cwd = file.fullPath(file.getPath()),
    tmpDir = "tmp",
    fileName = "test_data.txt"
);
return cwd + "/" + tmpDir + "/" + fileName
```

**Use Case**: When path components come from variables or calculations.

---

### Pattern 4: Store Paths in Records for Reuse

```usertalk
local (paths = [
    "tmpDir": file.fullPath(file.getPath()) + "/" + "tmp",
    "dbDir": file.fullPath(file.getPath()) + "/" + "databases",
    "testDir": file.fullPath(file.getPath()) + "/" + "tests"
]);
return paths["tmpDir"] + "/" + "test_file.txt"
```

This allows organizing related paths in a record for multiple uses.

---

### Pattern 5: Change Working Directory and Get New Path

```usertalk
local (tmpPath = file.fullPath(file.getPath()) + "/" + "tmp");
file.setPath(file.new(tmpPath));  // Change working directory to tmp
return file.fullPath(file.getPath())  // Returns: /Users/.../Frontier/tmp
```

**Important**: `file.new()` CREATES a directory/file. If the directory already exists, this will fail.

---

### Pattern 6: Construct Relative Path (from new cwd)

```usertalk
local (
    basePath = file.fullPath(file.getPath()),
    tmpPath = basePath + "/" + "tmp"
);
file.setPath(file.new(tmpPath));  // Now tmp is the working directory

// Subsequent file operations work relative to tmp:
local (testFile = file.fullPath(file.getPath()) + "/" + "my_test.txt");
// Result: /Users/.../Frontier/tmp/my_test.txt
return testFile
```

---

## Real-World Test Scenarios

### Scenario 1: Unit Test Accessing Database Files

**Problem**: Unit test needs to access databases from a known location without hardcoding paths.

**Solution**:
```usertalk
local (
    dbDir = file.fullPath(file.getPath()) + "/" + "databases",
    v6Path = dbDir + "/" + "Frontier.root",
    v7Path = dbDir + "/" + "Frontier-v7.root"
);

return [
    "v6Database": v6Path,
    "v7Database": v7Path
]
```

**Single-line for CLI**:
```bash
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli -e \
    'local (d=file.fullPath(file.getPath())+"/databases"); return d+"/Frontier-v7.root"'
```

**Output**:
```
/Users/jake/dev/jsavin/Frontier/databases/Frontier-v7.root
```

---

### Scenario 2: Integration Test with Fixture Data

**Problem**: Test needs to reference fixture data in a tests/fixtures directory.

**Solution**:
```usertalk
local (
    fixturesDir = file.fullPath(file.getPath()) + "/" + "tests" + "/" + "fixtures",
    sampleData = fixturesDir + "/" + "sample_data.json",
    testConfig = fixturesDir + "/" + "config.yaml"
);

return [
    "sampleDataPath": sampleData,
    "configPath": testConfig
]
```

**CLI test**:
```bash
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli -e \
    'return file.fullPath(file.getPath()) + "/" + "tests" + "/" + "fixtures" + "/" + "sample_data.json"'
```

---

### Scenario 3: Test Accessing Multiple Related Paths

**Problem**: Test needs to work with paths in multiple directories (source, build, output).

**Solution**:
```usertalk
on getTestPaths() {
    local (cwd = file.fullPath(file.getPath()));

    return [
        "sourceDir": cwd + "/" + "Common" + "/" + "source",
        "portableDir": cwd + "/" + "portable",
        "testDir": cwd + "/" + "tests",
        "tmpDir": cwd + "/" + "tmp",
        "dbDir": cwd + "/" + "databases"
    ];
}

return getTestPaths()
```

This centralizes path construction and makes tests more maintainable.

---

### Scenario 4: Parameterized Path Construction

**Problem**: Want to construct paths dynamically based on test parameters.

**Solution**:
```usertalk
on buildTestPath(category, testName) {
    local (
        cwd = file.fullPath(file.getPath()),
        testDir = cwd + "/" + "tests" + "/" + category,
        testFile = testDir + "/" + testName + ".txt"
    );
    return testFile;
}

// Usage examples:
local (unitTestPath = buildTestPath("unit", "test_001"));
local (integrationTestPath = buildTestPath("integration", "test_db_migration"));

return [
    "unitTest": unitTestPath,
    "integrationTest": integrationTestPath
]
```

---

## Edge Cases and Gotchas

### Gotcha 1: File Record vs String Confusion

**Wrong** - Can't concatenate file record with string:
```usertalk
local (path = file.getPath() + "/" + "tmp")  // ✗ Type error
```

**Correct** - Always convert to string with `file.fullPath()`:
```usertalk
local (path = file.fullPath(file.getPath()) + "/" + "tmp")  // ✓ Works
```

**Lesson**: `file.getPath()` returns a file record (`fss` type), not a string. Always use `file.fullPath()` to convert before string operations.

---

### Gotcha 2: file.new() Creates Files/Folders

**Misconception**: `file.new()` just creates a reference like in other languages.

**Reality**: `file.new()` **actually creates** the file or directory on disk.

**Wrong**:
```usertalk
// This creates a directory! May fail if it already exists.
local (tmpDir = file.new(pathString));
```

**Correct** - Use string paths for construction, only call `file.new()` when you need to create:
```usertalk
// Construct path as string
local (tmpPath = file.fullPath(file.getPath()) + "/" + "tmp");

// Only create if you actually want to
local (tmpDir = file.new(tmpPath));  // Creates the directory
```

**Lesson**: Keep paths as strings. Only convert to file records and call operations like `file.new()` when you're performing actual file system operations.

---

### Gotcha 3: Working Directory is Not Shell CWD

**Wrong assumption**:
```bash
cd /some/other/directory
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli -e 'return file.fullPath(file.getPath())'
# Returns: /Users/jake/dev/jsavin/Frontier (NOT /some/other/directory!)
```

**Correct understanding**: Working directory is always where frontier-cli binary is located (or its real location if symlinked).

**Why this matters**:
- Scripts are reproducible regardless of where you run them from
- Paths are always relative to the Frontier project root
- Works the same in CI/CD and local development

---

### Gotcha 4: Path Separators

**Works on all platforms**:
```usertalk
local (path = "/Users/jake" + "/" + "dev")  // "/" works everywhere
```

Frontier's file verbs normalize paths, so "/" works consistently across macOS, Linux, and Windows.

---

### Gotcha 5: Empty/Relative Paths

UserTalk concatenation behavior:
```usertalk
local (a = "");
local (b = a + "/" + "tmp");  // Result: "/tmp" (works)

local (relative = "relative" + "/" + "path");  // Result: "relative/path" (also works)
```

---

## Testing Your Path Construction

### Test 1: Verify Working Directory

```bash
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli \
    -e 'return file.fullPath(file.getPath())'
```

Expected: Directory containing frontier-cli binary (usually Frontier project root)

---

### Test 2: Verify Path Construction

```bash
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli \
    -e 'return file.fullPath(file.getPath()) + "/" + "tmp"'
```

Expected: A valid path ending in "/tmp"

---

### Test 3: Verify Path with Variables

```bash
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli \
    -e 'local (p = file.fullPath(file.getPath()) + "/" + "databases" + "/" + "Frontier-v7.root"); return p'
```

Expected: A valid path to the database file

---

## Integration with Unit Tests

### Using in run_headless_tests.sh

If you're running inline scripts via `./tools/run_headless_tests.sh`, you can use path construction:

```bash
./frontier-cli/frontier-cli -e \
    'local (tmpPath = file.fullPath(file.getPath()) + "/" + "tmp"); return tmpPath'
```

The path construction works consistently whether you run it:
- From the Frontier root directory
- From a subdirectory
- From a completely different directory
- In CI/CD environments

This is because the working directory is always the frontier-cli binary's directory.

---

## Performance Notes

- `file.getPath()` is very fast (O(1) lookup of thread-local working directory)
- `file.fullPath()` is very fast (O(1) conversion)
- String concatenation is O(n) for the total string length
- Overall: Suitable for use in loops and performance-critical code

No filesystem calls are made during path construction.

---

## Complete End-to-End Example

Here's a complete working example that constructs paths and returns them:

```usertalk
on testPathConstruction() {
    // Step 1: Get working directory as string
    local (cwd = file.fullPath(file.getPath()));

    // Step 2: Construct paths to different directories
    local (tmpDir = cwd + "/" + "tmp");
    local (dbDir = cwd + "/" + "databases");
    local (testDir = cwd + "/" + "tests");

    // Step 3: Construct paths to specific files
    local (testFile = tmpDir + "/" + "test_data_001.json");
    local (dbFile = dbDir + "/" + "Frontier-v7.root");

    // Step 4: Return all paths as a record
    return [
        "cwd": cwd,
        "tmpDir": tmpDir,
        "dbDir": dbDir,
        "testDir": testDir,
        "testFile": testFile,
        "dbFile": dbFile
    ];
}

// Call the function
return testPathConstruction()
```

**CLI Usage**:
```bash
FRONTIER_HEADLESS_RUN_STARTUP=1 ./frontier-cli/frontier-cli -e \
    'on testPathConstruction() { local (cwd = file.fullPath(file.getPath())); return cwd + "/" + "tmp" + "/" + "test.txt"; } return testPathConstruction()'
```

**Output**:
```
/Users/jake/dev/jsavin/Frontier/tmp/test.txt
```

---

## Summary

The core pattern for headless path construction:

```usertalk
// 1. Get cwd as string
local (cwd = file.fullPath(file.getPath()));

// 2. Build path string with concatenation
local (targetPath = cwd + "/" + "relative" + "/" + "path");

// 3. Use the string path directly with file operations
// NO need to convert back to file record unless performing actual file ops
```

This works consistently regardless of:
- Where frontier-cli is invoked from
- What the shell's current directory is
- Whether it's in a CI/CD pipeline
- Whether it's a symlink to the real binary

The path construction is simple, efficient, and reproducible across all contexts.
