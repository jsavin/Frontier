# .root7 Extension Retirement Plan

**Date:** 2026-03-22
**Branch:** feature/setup-frontier-startup
**Status:** Implemented (PR #487)

**Note:** The implementation uses a different naming convention than originally planned.
The plan described `.v7.root` output naming; the actual implementation renames v6 to `.v6.root`
and places v7 at the original `.root` path. See the PR commits for the final design.

## Executive Summary

The `.root7` file extension was introduced as a Phase 1 coexistence mechanism: v6 source databases used `.root` while migrated v7 databases were written alongside them as `.root7`. Now that all source databases in `databases/` are natively v7 format with plain `.root` extensions, the `.root7` extension is no longer needed for new databases.

However, **migration of legacy v6 databases remains valuable** -- users may still have v6 `.root` files they need to migrate. The retirement must preserve migration functionality while eliminating the `.root7` naming convention from migration output and auto-discovery.

## Design Decisions

### Q1: Auto-discovery (main.c)

**Decision:** Remove `Frontier.root7` from all search paths. Only search for `Frontier.root`.

**Rationale:** Source databases are now v7 with `.root` extension. No distribution or build process creates `Frontier.root7` files. Anyone who previously had a `Frontier.root7` from auto-migration should rename it to `Frontier.root`.

### Q2: On-the-fly migration output (dbverbs.c, db_format.c)

**Decision:** Rename the v6 original to `.v6.root` and put the migrated v7 database at the original `.root` path.

**Rationale:** `.root` = v7 is the new standard. The migration artifact is the *old* format, not the new one. The v6 original is preserved as a backup with a `.v6.root` suffix. This matches the mental model we've already established: source databases are `.root` and they're v7.

**Examples:**
- `Frontier.root` (v6) → renamed to `Frontier.v6.root`, v7 written to `Frontier.root`
- `legacy.root` (v6) → renamed to `legacy.v6.root`, v7 written to `legacy.root`

### Q3: `--migrate` flag default output (main.c, db_format.c)

**Decision:** Default behavior: rename input to `.v6.root`, write v7 output at original `.root` path. With `--output`, write to explicit path (input unchanged).

**Examples (no --output):**
- `./frontier-cli --migrate Frontier.root` → `Frontier.v6.root` (backup) + `Frontier.root` (v7)

**Examples (with --output):**
- `./frontier-cli --migrate Frontier.root --output /tmp/migrated.root` → input unchanged, v7 at `/tmp/migrated.root`

### Q4: cli_parser.c extension recognition

**Decision:** Keep `.root7` as a recognized extension for positional argument detection.

**Rationale:** Users may still have `.root7` files from previous migrations. Refusing to recognize them as database files would be a usability regression. The cost of recognition is one `strcasecmp` call -- essentially zero. This is backward compatibility, not active promotion.

### Q5: Guest database opening (dbverbs.c)

**Decision:** Remove the `.root7` fallback from `dbopenverb` and `getodbparam`. Change `dbnewverb` to use `.root` extension for new databases.

**Rationale:** The `.root7` fallback in `dbopenverb` existed because source databases were v6 and dist databases were v7. Now source IS v7. The on-the-fly migration (v6 detection + `ensure_database_v7`) is sufficient for handling any remaining v6 files a user might open -- but its output naming changes per Q2.

---

## Changes by Category

### Category 1: Runtime Code (main.c)

#### 1A. Remove `DEFAULT_SYSTEM_ROOT_V7` constant

**File:** `frontier-cli/main.c`
**Lines:** 84-86

```c
// BEFORE:
#define DEFAULT_SYSTEM_ROOT_V7 "databases/Frontier.root7"
#define DEFAULT_SYSTEM_ROOT_V6 "databases/Frontier.root"

// AFTER:
#define DEFAULT_SYSTEM_ROOT "databases/Frontier.root"
```

**Risk:** Low. No code references `DEFAULT_SYSTEM_ROOT_V7` or `DEFAULT_SYSTEM_ROOT_V6` by name in the search paths (they're hardcoded inline). Verify with grep that these macros are not referenced elsewhere.

#### 1B. Simplify auto-discovery search paths

**File:** `frontier-cli/main.c`
**Lines:** 487-525 (function `build_search_paths` or equivalent)

Remove all `Frontier.root7` entries. Collapse the interleaved v7/v6 pairs into single `Frontier.root` entries. The search order becomes:

1. `FRONTIER_ROOT` env var (unchanged)
2. `<cwd>/Frontier.root`
3. `<exe_dir>/Frontier.root`
4. `~/Library/Application Support/Frontier/Frontier.root`

Update comments to remove v7/v6 language. Update `MAX_SEARCH_PATHS` from 8 to a smaller value if appropriate (currently 8, will need fewer slots).

**Risk:** Low. This only affects auto-discovery when `--system-root` is not specified. The v7 databases already use `.root` extension.

#### 1C. Update `--migrate` default output naming

**File:** `frontier-cli/main.c`
**Lines:** 196-218

```c
// BEFORE:
/* If no output path specified, create default: <input>.root7 or <input>7 */
if (input_len >= 5 && strcasecmp(input + input_len - 5, ".root") == 0) {
    snprintf(default_output, sizeof(default_output), "%s7", input);
} else {
    snprintf(default_output, sizeof(default_output), "%s.root7", input);
}

// AFTER:
/* If no output path specified, create default: <basename>.v7.root */
if (input_len >= 5 && strcasecmp(input + input_len - 5, ".root") == 0) {
    /* Replace .root with .v7.root */
    snprintf(default_output, sizeof(default_output), "%.*s.v7.root",
             (int)(input_len - 5), input);
} else {
    /* Append .v7.root */
    snprintf(default_output, sizeof(default_output), "%s.v7.root", input);
}
```

Also update the buffer size comment on line 198 (`Extra space for ".root7" suffix` -> appropriate new comment).

**Risk:** Medium. This changes the default output path for `--migrate`. Users who have scripts relying on the `.root7` output name will need to update them or use `--output` explicitly. Document in release notes.

#### 1D. Update help text

**File:** `frontier-cli/main.c`
**Lines:** 629, 659, 661-662

```c
// Line 629 BEFORE:
printf("  --output PATH            Output path for migrated database (default: <input>.root7)\n");
// AFTER:
printf("  --output PATH            Output path for migrated database (default: <basename>.v7.root)\n");

// Line 659 BEFORE:
printf("  %s --system-root databases/Frontier.root7 -e \"sizeOf(system)\"\n", program_name);
// AFTER:
printf("  %s --system-root databases/Frontier.root -e \"sizeOf(system)\"\n", program_name);

// Lines 661-662 BEFORE:
printf("  # Migrate v6 database to v7 format (creates .root7 alongside original)\n");
// AFTER:
printf("  # Migrate v6 database to v7 format (creates .v7.root alongside original)\n");
```

**Risk:** Low. Cosmetic only.

#### 1E. Update comment in `cli_hydrate_database`

**File:** `frontier-cli/main.c`
**Line:** 864

```c
// BEFORE:
/* After ensure_database_v7, we always have a v7 database (either the original if already v7,
 * or a newly migrated .root7 file if the source was v6). ...

// AFTER:
/* After ensure_database_v7, we always have a v7 database (either the original if already v7,
 * or a newly migrated .v7.root file if the source was v6). ...
```

**Risk:** None. Comment only.

#### 1F. Update intermediate file cleanup comment

**File:** `frontier-cli/main.c`
**Line:** 289

```c
// BEFORE:
/* Remove the intermediate .root7 file */
// AFTER:
/* Remove the intermediate .v7.root file */
```

**Risk:** None. Comment only.

---

### Category 2: Runtime Code (db_format.c)

#### 2A. Change migration output path derivation

**File:** `Common/source/db_format.c`
**Lines:** 1892-1903 (inside `migrate_internal`)

```c
// BEFORE:
/* Derive output path: replace .root with .root7 (Phase 1 naming convention)
 * Pattern: .root$ -> .root7
 * Examples: Frontier.root -> Frontier.root7, test.root -> test.root7 */
const char *ext = strrchr(db_path, '.');
if (ext && strcmp(ext, ".root") == 0) {
    size_t base_len = (size_t)(ext - db_path);
    snprintf(output_path, sizeof output_path, "%.*s.root7", (int) base_len, db_path);
} else {
    snprintf(output_path, sizeof output_path, "%s.root7", db_path);
}

// AFTER:
/* Derive output path: replace .root with .v7.root
 * Pattern: .root$ -> .v7.root
 * Examples: Frontier.root -> Frontier.v7.root, test.root -> test.v7.root */
const char *ext = strrchr(db_path, '.');
if (ext && strcmp(ext, ".root") == 0) {
    size_t base_len = (size_t)(ext - db_path);
    snprintf(output_path, sizeof output_path, "%.*s.v7.root", (int) base_len, db_path);
} else {
    snprintf(output_path, sizeof output_path, "%s.v7.root", db_path);
}
```

**Risk:** Medium. This is the core migration output path. All callers of `migrate_internal` and `ensure_database_v7` will get the new naming. Must verify all callers handle the new path correctly.

#### 2B. Update `ensure_database_v7` v7-file-exists check

**File:** `Common/source/db_format.c`
**Lines:** 2306-2330

The current code checks for `<path>7` (i.e., appending "7" to make `.root7`). This must change to check for `<basename>.v7.root`.

```c
// BEFORE:
snprintf(v7_path, sizeof v7_path, "%s7", db_path);

// AFTER:
/* Check for .v7.root version of the file */
const char *dot = strrchr(db_path, '.');
if (dot && strcmp(dot, ".root") == 0) {
    size_t base_len = (size_t)(dot - db_path);
    snprintf(v7_path, sizeof v7_path, "%.*s.v7.root", (int)base_len, db_path);
} else {
    snprintf(v7_path, sizeof v7_path, "%s.v7.root", db_path);
}
```

Update the surrounding comment accordingly.

**Risk:** Medium. This is a behavioral change -- previously migrated `.root7` files will no longer be auto-detected by `ensure_database_v7`. Users with existing `.root7` files from previous auto-migrations will need to rename them. This is intentional but should be documented.

**Mitigation:** Consider a transitional period where both `.root7` (legacy) and `.v7.root` (new) are checked, with `.v7.root` preferred. See "Transitional Compatibility" section below.

---

### Category 3: Runtime Code (dbverbs.c)

#### 3A. Remove `ROOT7_EXTENSION_LEN` constant

**File:** `Common/source/dbverbs.c`
**Line:** 71

```c
// REMOVE:
#define ROOT7_EXTENSION_LEN 6  /* ".root7" */
```

**Risk:** Low. Verify no other code references this macro.

#### 3B. Remove `odb_check_root7_exists` function

**File:** `Common/source/dbverbs.c`
**Lines:** 593-627

Remove the entire `odb_check_root7_exists` function and its forward declaration at line 433.

**Risk:** Medium. Must verify all call sites are updated first. Called from `getodbparam` (line 464) and `dbopenverb` (lines 784, 819).

#### 3C. Remove `odb_ensure_root7_extension` function

**File:** `Common/source/dbverbs.c`
**Lines:** 630-680

Remove entirely, including forward declaration at line 431.

**Risk:** Medium. Called from `dbnewverb` (line 702). Must replace with logic that ensures `.root` extension.

#### 3D. Update `dbnewverb` -- use `.root` extension

**File:** `Common/source/dbverbs.c`
**Lines:** 690-702

Replace the call to `odb_ensure_root7_extension` with a new function (or inline logic) that ensures the `.root` extension. New databases should be created with `.root` extension.

```c
// BEFORE:
/* Phase 1: Ensure .root7 extension (replaces .root if user provided it) */
odb_ensure_root7_extension(&odbrec.fs);

// AFTER:
/* Ensure .root extension for new databases */
odb_ensure_root_extension(&odbrec.fs);
```

Write a new `odb_ensure_root_extension` function that:
- If path ends with `.root`, leave it alone
- If path ends with `.root7`, replace with `.root` (backward compat)
- Otherwise, append `.root`

**Risk:** Medium. Changes the extension of newly-created guest databases. Any scripts using `db.new("foo")` that expected `foo.root7` will now get `foo.root`.

#### 3E. Simplify `dbopenverb` auto-migration logic

**File:** `Common/source/dbverbs.c`
**Lines:** 776-882

The current flow is:
1. Check if `.root7` exists -> use it
2. Else detect v6 -> migrate (creates `.root7`)
3. Double-check for TOCTOU race on `.root7`

Simplify to:
1. Detect database version
2. If already v7 -> open directly
3. If v6 -> migrate (creates `.v7.root`) -> open the migrated file

Remove:
- The `odb_check_root7_exists` call at line 784
- The TOCTOU `.root7` re-check at line 819
- The `fs_root7` variable declarations

The migration path via `ensure_database_v7` already handles everything -- it checks for an existing v7 output, detects the version, and migrates if needed. The `dbopenverb` code can be simplified to just call `ensure_database_v7` directly.

```c
// SIMPLIFIED dbopenverb migration block:
if (!odbrec.flreadonly) {
    char cpath[DB_PATH_MAX];
    char output_path[DB_PATH_MAX];
    boolean migrated = false;

    filespectopath(&odbrec.fs, bspath);
    copyptocstring(bspath, cpath);

    if (ensure_database_v7(cpath, &migrated, output_path, sizeof(output_path))) {
        if (migrated || strcmp(cpath, output_path) != 0) {
            /* Migration occurred or v7 version found -- update filespec */
            bigstring bsoutput;
            copyctopstring(output_path, bsoutput);
            pathtofilespec(bsoutput, &odbrec.fs);
        }
    }
    /* If ensure_database_v7 fails, fall through and try to open the original */
}
```

**Risk:** High. This is a significant refactor of the guest database open path. Must be tested thoroughly with:
- Opening a v7 `.root` file (common case)
- Opening a v6 `.root` file (triggers migration)
- Opening a file that already has a `.v7.root` sibling
- Opening a `.root7` file directly (backward compat via cli_parser)
- Read-only mode (should skip migration entirely)

#### 3F. Simplify `getodbparam` -- remove `.root7` fallback

**File:** `Common/source/dbverbs.c`
**Lines:** 462-471

Remove the `.root7` fallback lookup block.

```c
// REMOVE this entire block:
/* If .root path provided and not found, try .root7 fallback - skip sentinel */
tyfilespec fs_root7;
if (odb_check_root7_exists(ptrfs, &fs_root7)) {
    for (hodb = (**hodblist).hnext; hodb != nil; hodb = (**hodb).hnext) {
        if ( equalfilespecs ( &( **hodb ).fs, &fs_root7 ) ) {
            *hodbrecord = hodb;
            return (true);
            }
        }
}
```

**Risk:** Medium. This fallback allowed `db.open("foo.root"); db.x.y` to find a database opened as `foo.root7`. After retirement, databases are opened with `.root` extension, so the fallback is unnecessary. However, if any user code opens a database with one name and references it with another, this could break.

---

### Category 4: Runtime Code (odbengine.c)

#### 4A. Update comment

**File:** `Common/source/odbengine.c`
**Line:** ~434

Update the comment referencing `.root7`. This is a documentation-only change.

**Risk:** None.

---

### Category 5: Test Code

#### 5A. `test_efp_augmentation.c`

**File:** `tests/test_efp_augmentation.c`
**Line:** 49

```c
// BEFORE:
const char *db_path = "tmp/migration/test_save_migration.root7";
// AFTER:
const char *db_path = "tmp/migration/test_save_migration.v7.root";
```

**Dependency:** This test depends on a migration test that creates the file. Verify the upstream test also produces the new name. If the migration test uses `migrate_internal` or `ensure_database_v7`, it will automatically produce the new name after Category 2 changes.

**Risk:** Low, but must verify the dependency chain.

#### 5B. `test_dump_system_paths.c`

**File:** `tests/test_dump_system_paths.c`
**Line:** 153

```c
// BEFORE:
const char *dbpath = "databases/Frontier.root7";
// AFTER:
const char *dbpath = "databases/Frontier.root";
```

**Risk:** Low. The v7 database is now at `databases/Frontier.root`.

#### 5C. `test_readonly_database.c`

**File:** `tests/test_readonly_database.c`
**Line:** 154

```c
// BEFORE:
snprintf(test_v7_db, sizeof test_v7_db, "%s/test_readonly_v6.root7", unit_dir);
// AFTER:
snprintf(test_v7_db, sizeof test_v7_db, "%s/test_readonly_v6.v7.root", unit_dir);
```

**Risk:** Low. This is a test artifact path that follows the migration output naming.

#### 5D. `cli_positional_root_tests.sh`

**File:** `tests/integration/cli_positional_root_tests.sh`

**Changes:**
- Keep Test Group 2 (positional `.root7` file detection) -- this validates backward compatibility per Q4.
- Update comments from "v7 preferred" to just describe backward compat testing.
- Line 3: Update header comment.
- Lines 120-145: Update group description comments to say "backward compatibility" instead of implying `.root7` is the preferred format.
- Lines 160-162: Keep the conflict tests as-is (they test error handling for `.root7` + `--system-root` conflict, still valid).

**Risk:** Low. The tests validate that `.root7` is still *recognized*, which is the intended behavior per Q4.

---

### Category 6: Documentation

All documentation changes are low risk.

#### 6A. `tests/table_operations/README.md` (line 88)

Change `databases/Frontier.root7` to `databases/Frontier.root`.

#### 6B. `tests/table_operations/IMPLEMENTATION_SUMMARY.md` (line 109)

Change `databases/Frontier.root7` to `databases/Frontier.root`.

#### 6C. `tests/integration/REPL_TESTING_GUIDE.md` (line 241)

Change `databases/Frontier.root7` to `databases/Frontier.root`. Remove the "Or: auto-migrate" alternative since the database is already v7.

#### 6D. `tests/integration/REPL_RUNNER_CHANGES.md` (lines 86, 91, 96)

Change all `databases/Frontier.root7` references to `databases/Frontier.root`.

---

## Transitional Compatibility (Optional)

For a smoother transition, `ensure_database_v7` could check for both legacy `.root7` and new `.v7.root` siblings:

```c
/* Check for .v7.root (new convention) */
// ... build v7_new_path ...
FILE *fp_new = fopen(v7_new_path, "rb");
if (fp_new) { /* validate and use */ }

/* Fallback: check for .root7 (legacy convention) */
// ... build v7_legacy_path ...
FILE *fp_legacy = fopen(v7_legacy_path, "rb");
if (fp_legacy) { /* validate and use */ }
```

**Recommendation:** Include the transitional check in the initial implementation. Add a `log_info` message when a legacy `.root7` file is found, advising the user to rename it. Remove the legacy check in a future release.

---

## Implementation Order

The changes have dependencies. Implement in this order:

### Phase 1: Core migration output naming (must be first)

1. **2A** -- `db_format.c`: Change `migrate_internal` output path derivation
2. **2B** -- `db_format.c`: Update `ensure_database_v7` v7-file-exists check (with transitional `.root7` fallback)

These are the foundation -- all other migration-dependent code flows through here.

### Phase 2: dbverbs.c cleanup

3. **3C** -- Remove `odb_ensure_root7_extension`
4. **3D** -- Write `odb_ensure_root_extension`, update `dbnewverb`
5. **3B** -- Remove `odb_check_root7_exists`
6. **3F** -- Remove `.root7` fallback from `getodbparam`
7. **3E** -- Simplify `dbopenverb` auto-migration logic
8. **3A** -- Remove `ROOT7_EXTENSION_LEN`

### Phase 3: main.c updates

9. **1A** -- Remove `DEFAULT_SYSTEM_ROOT_V7`
10. **1B** -- Simplify auto-discovery search paths
11. **1C** -- Update `--migrate` default output naming
12. **1D** -- Update help text
13. **1E, 1F** -- Update comments

### Phase 4: Test and doc updates

14. **5B** -- `test_dump_system_paths.c`
15. **5C** -- `test_readonly_database.c`
16. **5A** -- `test_efp_augmentation.c` (depends on migration output naming)
17. **5D** -- `cli_positional_root_tests.sh` (comment updates only)
18. **4A** -- `odbengine.c` comment
19. **6A-6D** -- Documentation updates

---

## Testing Verification

After implementation, run these verification steps:

### Unit Tests

```bash
./tools/run_headless_tests.sh
```

Verify all existing tests pass. Pay special attention to:
- `test_dump_system_paths` -- must find `databases/Frontier.root`
- `test_readonly_database` -- migration output path changed
- `test_efp_augmentation` -- migration output path changed

### Integration Tests

```bash
cd tests && make test-integration
```

### Migration Tests

```bash
# Test --migrate with default output
./frontier-cli/frontier-cli --migrate tests/fixtures/v6/Frontier_v6.root
# Expected: creates tests/fixtures/v6/Frontier.v7.root (or Frontier_v6.v7.root)
ls -la tests/fixtures/v6/*.root*

# Test --migrate with explicit output
./frontier-cli/frontier-cli --migrate tests/fixtures/v6/Frontier_v6.root --output /tmp/test_migrated.root
ls -la /tmp/test_migrated.root

# Test --migrate with --force
./frontier-cli/frontier-cli --migrate tests/fixtures/v6/Frontier_v6.root --force
```

### Positional Argument Tests

```bash
cd tests/integration && bash cli_positional_root_tests.sh
```

### Auto-Discovery Tests

```bash
# Test that Frontier.root is found in databases/
cd /Users/jake/dev/jsavin/Frontier
./frontier-cli/frontier-cli -e "1+1"

# Test FRONTIER_ROOT env var
FRONTIER_ROOT=databases/Frontier.root ./frontier-cli/frontier-cli -e "1+1"
```

### Guest Database Tests

```bash
# Test db.new creates .root files
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e 'db.new("/tmp/test_new.root")'
ls -la /tmp/test_new*
# Expected: /tmp/test_new.root (NOT /tmp/test_new.root7)

# Test db.open with v7 database
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e 'db.open("/tmp/test_new.root", false); db.close("/tmp/test_new.root")'
```

### Grep Verification

After all changes, verify no unintended `.root7` references remain:

```bash
# Should return ONLY:
# - cli_parser.c (backward compat recognition)
# - cli_positional_root_tests.sh (backward compat testing)
# - This planning document
# - Transitional fallback code (if implemented)
# - Git history / changelogs
grep -rn "root7" --include="*.c" --include="*.h" --include="*.sh" --include="*.py" .
```

---

## Risk Assessment Summary

| Change | Risk | Impact if Wrong | Mitigation |
|--------|------|----------------|------------|
| Auto-discovery path removal (1B) | Low | CLI fails to find system root | Trivial to diagnose; `--system-root` always works |
| Migration output naming (2A) | Medium | Scripts expecting `.root7` output break | Document in release notes; `--output` flag still works |
| `ensure_database_v7` check (2B) | Medium | Existing `.root7` files not auto-detected | Transitional fallback + log message |
| `dbnewverb` extension (3D) | Medium | New databases get wrong extension | Test `db.new` explicitly |
| `dbopenverb` simplification (3E) | High | Guest database open breaks | Thorough testing with v6, v7, mixed scenarios |
| `getodbparam` fallback removal (3F) | Medium | Cross-reference by old name fails | Only affects databases opened under old naming |
| cli_parser recognition (kept) | None | N/A -- keeping `.root7` recognition | No change needed |

---

## Files Modified (Summary)

| File | Type | Changes |
|------|------|---------|
| `Common/source/db_format.c` | Runtime | Migration output path: `.root7` -> `.v7.root` |
| `Common/source/dbverbs.c` | Runtime | Remove 3 functions, simplify dbopenverb, update dbnewverb |
| `Common/source/odbengine.c` | Runtime | Comment update only |
| `frontier-cli/main.c` | Runtime | Search paths, migrate defaults, help text |
| `frontier-cli/cli_parser.c` | Runtime | No change (keep `.root7` recognition) |
| `tests/test_efp_augmentation.c` | Test | Update migration output path |
| `tests/test_dump_system_paths.c` | Test | Update database path |
| `tests/test_readonly_database.c` | Test | Update migration output path |
| `tests/integration/cli_positional_root_tests.sh` | Test | Comment updates only |
| `tests/table_operations/README.md` | Docs | Path update |
| `tests/table_operations/IMPLEMENTATION_SUMMARY.md` | Docs | Path update |
| `tests/integration/REPL_TESTING_GUIDE.md` | Docs | Path update |
| `tests/integration/REPL_RUNNER_CHANGES.md` | Docs | Path updates |
