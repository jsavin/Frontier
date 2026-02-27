# UserTalk File and Database Operations

Guide to file path requirements, database operations, and temporary file conventions.

---

## Absolute Paths Required

**CRITICAL**: All UserTalk file/database verbs require FULL/ABSOLUTE paths. The runtime has NO cwd awareness at the UserTalk level.

### Affected Verbs

All file and database operations require absolute paths:
- `db.new()`, `db.open()`, `db.close()`
- `file.create()`, `file.write()`, `file.read()`, `file.delete()`
- All `file.*` operations

### Why This Matters

UserTalk's runtime does not maintain a current working directory (cwd) at the script level:
- No concept of "relative to script location"
- No automatic path resolution
- Relative paths will fail with "file not found" errors

### Correct Usage

```usertalk
// ✅ CORRECT - absolute path
db.new("/Users/jake/databases/test.root")
file.write("/Users/jake/output.txt", "data")

// ✅ CORRECT - build absolute path at runtime
local(fullPath = file.getcwd() + "/test.root")
db.new(fullPath)

// ✅ CORRECT - use system.paths for well-known locations
local(dbPath = system.paths.databases + "test.root")
db.new(dbPath)
```

### Wrong Usage

```usertalk
// ❌ WRONG - relative path fails
db.new("test.root")               // Error: file not found
file.write("output.txt", "data")   // Error: file not found

// ❌ WRONG - assuming script directory context
db.new("../databases/test.root")   // Error: no cwd to be relative to
```

---

## macOS Sandbox Constraints

### Temporary File Locations

Both `/tmp` and project-relative paths (`tests/tmp/`) are valid for test scratch files.

**Project test directories** (gitignored):
```bash
tests/tmp/unit/          # Unit test outputs
tests/tmp/integration/   # Integration test outputs
tests/tmp/migration/     # Migration test artifacts
tests/tmp/results/       # Test logs and CLI runtime artifacts
```

### Testing Patterns

**Integration tests** - Use template replacement:
```yaml
# YAML test file
tests:
  - name: "db.new - create database"
    script: 'db.new("{FRONTIER_TEST_TMP_DIR}/test.root")'
    expected_success: true
```

The test framework automatically replaces `{FRONTIER_TEST_TMP_DIR}` with a safe project-relative path.

**Manual CLI testing** - Use `/tmp` or helper script:
```bash
# Option 1: /tmp directly
./frontier-cli/frontier-cli -e 'db.new("/tmp/test.root")'

# Option 2: project-relative via helper
TESTDIR=$(./tools/get_test_temp_path.sh)
./frontier-cli/frontier-cli -e "db.new(\"$TESTDIR/test.root\")"
```

**C unit tests** - Use `/tmp` for scratch files:
```c
const char *scratch_path = "/tmp/my_test_scratch.db";
```

---

## Database Operations

### System Root vs Guest Databases

**System root** - The main Frontier database:
```bash
# Load system root when starting CLI
./frontier-cli/frontier-cli --system-root databases/Frontier.root7
```

Loading the system root:
- Initializes `system.*` tables
- Enables `target.*` verbs
- Provides `system.paths` for path construction

**Guest databases** - Any other databases opened:
```usertalk
// Open a guest database
db.open("/path/to/guest.root")

// Top-level items in guest databases are in global scope
// (managed via system.compiler.files)
```

### Database Path Construction

**Best practices:**

```usertalk
// ✅ Use system.paths when system root is loaded
local(dbPath = system.paths.databases + "mydb.root")
db.new(dbPath)

// ✅ Build absolute path with file.getcwd()
local(basePath = file.getcwd())
local(dbPath = basePath + "/databases/mydb.root")
db.new(dbPath)

// ✅ Hardcode for known locations (testing)
db.new("/Users/jake/dev/jsavin/Frontier/databases/test.root")
```

### Common Errors

```usertalk
// ❌ ERROR: Relative path
db.new("test.root")
// Result: "file not found" error

// ✅ OK: Using /tmp
db.new("/tmp/test.root")

// ❌ ERROR: Assuming cwd context
local(cwd = file.getcwd())
db.new(cwd + "/test.root")  // ✅ This works
db.new("test.root")          // ❌ This doesn't
```

---

## File Operations

### Reading Files

```usertalk
// ✅ CORRECT - absolute path
local(content = file.read("/Users/jake/data.txt"))

// ✅ CORRECT - build path
local(path = system.paths.data + "file.txt")
local(content = file.read(path))
```

### Writing Files

```usertalk
// ✅ CORRECT - absolute path
file.write("/Users/jake/output.txt", "data")

// ✅ CORRECT - build path for test output
local(testDir = file.getcwd() + "/tests/tmp/unit")
file.write(testDir + "/result.txt", "test output")
```

### Deleting Files

```usertalk
// ✅ CORRECT - absolute path
file.delete("/Users/jake/temp.txt")

// ⚠️ CAUTION - verify path before deleting
local(path = "/Users/jake/important.txt")
if file.exists(path) {
    // Confirm before delete in production code
    file.delete(path)
}
```

---

## Path Construction Helpers

### Using file.getcwd()

```usertalk
// Get current working directory (at shell level, not script level)
local(cwd = file.getcwd())
// Returns: "/Users/jake/dev/jsavin/Frontier"

// Build relative-to-cwd paths
local(dbPath = cwd + "/databases/test.root")
db.new(dbPath)
```

### Using system.paths

```usertalk
// Requires system root to be loaded
system.paths.databases    // Path to databases directory
system.paths.data         // Path to data directory
system.paths.tools        // Path to tools directory

// Build paths
local(dbPath = system.paths.databases + "mydb.root")
db.new(dbPath)
```

---

## Testing Best Practices

### DO:
- ✅ Use project-relative paths (`tests/tmp/*`)
- ✅ Use `{FRONTIER_TEST_TMP_DIR}` template in integration tests
- ✅ Use `./tools/get_test_temp_path.sh` for manual testing
- ✅ Clean up test files after tests complete
- ✅ Verify paths are absolute before file operations

### DON'T:
- ❌ Use relative paths without building absolute paths first
- ❌ Assume cwd awareness in UserTalk scripts
- ❌ Hardcode user-specific paths in tests
- ❌ Leave test files around after test completion

---

## Agent Delegation Template

When delegating file operations work to agents, ALWAYS include this constraint:

```markdown
For test scratch files, use /tmp or project-relative paths (tests/tmp/).
For integration tests, use {FRONTIER_TEST_TMP_DIR} template in YAML files.
```

---

## Troubleshooting

### "File not found" errors
- **Cause:** Relative path used instead of absolute
- **Fix:** Build absolute path with `file.getcwd()` or use full path

### "Permission denied" errors
- **Cause:** Trying to write to a read-only location
- **Fix:** Use `/tmp` or project-relative path in `tests/tmp/`

### "No such table" errors in guest databases
- **Cause:** System root not loaded
- **Fix:** Start CLI with `--system-root databases/Frontier.root7`

### Path construction issues
- **Cause:** Missing system.paths or wrong separator
- **Fix:** Verify system root loaded, use `+` to concatenate paths

---

## See Also

- **Syntax reference:** [SYNTAX.md](SYNTAX.md)
- **typeof() behavior:** [TYPEOF.md](TYPEOF.md)
- **Testing guide:** [docs/TESTING_GUIDE.md](../TESTING_GUIDE.md)
- **CLI usage:** [docs/CLI_USAGE_GUIDE.md](../CLI_USAGE_GUIDE.md)
