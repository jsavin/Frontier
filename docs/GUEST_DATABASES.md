# Guest Databases and system.compiler.files

## Overview

Guest Databases are Frontier database files (`.root` files) that exist separately from the system root (`Frontier.root`) but integrate into the global scope through a clever registration mechanism.

## What is a Guest Database?

A **Guest Database** is any root file that is not the system root itself. It has the same internal structure as the system root, but exists as a separate file on disk.

**Key Properties:**
- Independent `.root` file on disk
- Has its own root table with top-level items
- Can be opened, modified, and closed independently
- Content is NOT saved into the system root
- Integrates into global scope when opened

## The system.compiler.files Registry

When a Guest Database is created or opened, it is registered in the special table `system.compiler.files`.

### Registry Structure

Each entry in `system.compiler.files` is a table with:
- **Name**: Full absolute path to the Guest Database file
- **Content**: The root table of that Guest Database (live reference, not a copy)

Example:
```
system.compiler.files
  ├── ["/Users/jake/Projects/mydata.root"] → (root table of mydata.root)
  ├── ["/Users/jake/Projects/config.root"] → (root table of config.root)
  └── ["/Users/jake/Projects/cache.root"] → (root table of cache.root)
```

### Integration with Global Scope

The magic happens through `system.paths.path14`:

1. **Path Configuration**: `system.paths.path14 = @system.compiler.files`
2. **Global Scope**: All top-level items from Guest Databases become globally accessible
3. **Precedence**: Lowest precedence (path14 is the last path searched)
4. **Name Resolution**: When UserTalk looks up a name, it searches all paths in order, reaching Guest Database items last

**Example:**

If `mydata.root` contains:
```
mydata.root (root table)
  ├── userData
  ├── settings
  └── cache
```

After opening `mydata.root`, these become globally accessible:
```usertalk
userData      // Resolves to mydata.root.userData
settings      // Resolves to mydata.root.settings
cache         // Resolves to mydata.root.cache
```

### Separation from System Root

The `system.compiler.files` table is **flagged to not save**. This means:
- When `Frontier.root` (system root) is saved, Guest Database content is NOT saved into it
- Guest Database data remains in the separate `.root` files
- System root only knows the *paths* to Guest Databases, not their content
- This keeps data separated across multiple on-disk databases

## db Verb Responsibilities

The db verbs must properly maintain the `system.compiler.files` registry:

### db.new(path) - Create New Database

1. Create new `.root` file at `path`
2. Initialize with empty root table
3. Register in `system.compiler.files[path]` → (root table reference)

### db.open(path) - Open Existing Database

1. Open `.root` file at `path`
2. Read root table from file
3. Register in `system.compiler.files[path]` → (root table reference)
4. Top-level items now in global scope

### db.close(path) - Close Database

1. Save any pending changes to `.root` file
2. **Remove entry from `system.compiler.files[path]`**
3. Close file handle
4. Top-level items no longer in global scope

### db.save() - Save All Guest Databases

1. Iterate through `system.compiler.files`
2. For each Guest Database, save root table to disk
3. Does NOT save into system root

## ODB Engine Layer

The **ODB Engine** (`odbengine.c`) is the layer responsible for:
- Managing the `system.compiler.files` registry
- Opening/closing Guest Database files
- Maintaining live references between `system.compiler.files` entries and database root tables
- Handling context switches between system root and Guest Databases

This is why db verbs call **ODB engine functions** (`odbopenfile`, `odbsavefile`, etc.) rather than database layer functions directly:
- Database layer (`db.c`, `db_format.c`) handles file I/O and serialization
- ODB engine layer handles Guest Database registration and global scope integration

## Headless Mode Challenges

The ODB engine was designed for Mac GUI environment with assumptions about:
- Window management (each database has a window)
- Menu sharing (context switches for menu commands)
- Global state management (currenthashtable, etc.)

**Current Issue:**
- ODB engine context switches corrupt `currenthashtable` in headless mode
- Causes assertion failure in `langevaluate.c:1952`
- Documented bug since 1997, specifically mentions `db.save verb`

**Solution Requirements:**
- ODB engine calls must preserve `currenthashtable` global
- May need context guards similar to `db_context_guard` pattern
- OR: Reimplement db verbs to manage `system.compiler.files` directly without ODB engine

## Use Cases

### Multi-User Data Separation

```usertalk
// User-specific databases
db.open("/Users/alice/alice-data.root")  // alice.preferences, alice.history, etc.
db.open("/Users/bob/bob-data.root")      // bob.preferences, bob.history, etc.

// All user data accessible globally but in separate files
msg(alice.preferences.theme)
msg(bob.preferences.theme)
```

### Configuration Management

```usertalk
// Separate config databases
db.open("/etc/frontier/network-config.root")  // network.host, network.port, etc.
db.open("/etc/frontier/app-config.root")      // app.version, app.debug, etc.

// Global access to all config
if app.debug {
    msg("Server: " + network.host + ":" + network.port)
}
```

### Plugin/Module System

```usertalk
// Each plugin is a Guest Database
db.open("/Plugins/email-plugin.root")     // email.send(), email.receive(), etc.
db.open("/Plugins/http-plugin.root")      // http.get(), http.post(), etc.

// Plugins integrate into global scope
email.send(recipient, message)
http.get(url)
```

## Implementation Notes

### Why Full Paths Required

The db verbs require **full absolute paths** because:
- `system.compiler.files` uses full paths as keys
- Name resolution needs unambiguous database identity
- Prevents conflicts between databases with same filename in different directories

### Thread Safety Considerations

Guest Database operations involve global state:
- `system.compiler.files` table
- `currenthashtable` global
- Database file handles

**Future Work:**
- Must ensure thread-safe access to `system.compiler.files`
- Context switches must preserve `currenthashtable`
- File handle management must be thread-safe

## References

- **ODB Engine**: `Common/source/odbengine.c`
- **DB Verbs**: `Common/source/dbverbs.c`
- **Database Layer**: `Common/source/db.c`, `Common/source/db_format.c`
- **Context Issue**: `Common/source/langevaluate.c:1945-1952`
- **System Paths**: Defined in `system.paths` table (path1-path14)

## Testing Strategy

### Integration Tests Required

**Critical Constraint:** Effective unit testing of db verbs is extremely difficult because Guest Databases require:
- `system.compiler.files` table properly initialized
- `system.paths.path14` configured to point to `@system.compiler.files`
- Full global scope resolution system working
- Hash table system functional
- Runtime context properly established

**Testing Approach:**

1. **Primary: Integration Tests** (`tests/integration/db_tests.yaml`)
   - Use full CLI runtime with system root loaded
   - Test with actual system.compiler and system.paths initialized
   - Verify Guest Database registration in system.compiler.files
   - Test global scope access to Guest Database items
   - Verify precedence (path14 is lowest)

2. **Limited: Unit Tests** (if any)
   - Can only test isolated ODB engine functions
   - Cannot test full db verb workflow without runtime
   - Most functionality requires integration test coverage

3. **Manual Testing** (during development)
   - Use `./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root`
   - Verify system.compiler.files entries after db.open()
   - Check global scope access to Guest Database items
   - Confirm cleanup after db.close()

**Example Integration Test Pattern:**

```yaml
- name: "db.open - registers in system.compiler.files"
  setup:
    - 'db.new("{FRONTIER_TEST_TMP_DIR}/guest.root")'
    - 'db.close()'
  script: |
    db.open("{FRONTIER_TEST_TMP_DIR}/guest.root");
    defined(system.compiler.files.["{FRONTIER_TEST_TMP_DIR}/guest.root"])
  expected_result: "true"
  cleanup:
    - 'db.close()'
```

### Why Unit Tests Are Insufficient

Unit tests for db verbs would need to:
1. Initialize entire system.compiler table structure
2. Configure all 14 system.paths entries
3. Set up global scope resolution
4. Mock or stub ODB engine context switches
5. Simulate currenthashtable management

This essentially requires reimplementing the entire runtime bootstrap, making unit tests impractical. **Integration tests with full CLI runtime are the only reliable way to test Guest Database functionality.**

## Related Documentation

- [Database Format](DATABASE_FORMAT.md) - On-disk structure of `.root` files
- [ODB Architecture](../planning/phase3/ODB_ARCHITECTURE.md) - Overall database system design
- [Thread Safety](THREAD_LOCAL_GLOBALS_PATTERN.md) - Thread-safe global state management
- [Testing Guide](TESTING_GUIDE.md) - CLI usage and integration test patterns
