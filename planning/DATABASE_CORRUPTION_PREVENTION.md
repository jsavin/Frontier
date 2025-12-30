# Database Corruption Prevention & Recovery

**Date:** 2025-12-30
**Status:** ✅ RESOLVED - Protection implemented
**Related Commits:** 9cd4f66d, 819dee5c, 10d2f293

---

## Problem Summary

The v6 test fixture database (`databases/Frontier-v6.root`) was corrupted to v7 format multiple times during development, causing unreliable migration testing and potential test failures.

**Root Cause:** Before the migration process was fixed, the CLI was modifying the input database in-place during testing. Developers accidentally committed the v7-migrated version under the v6 filename.

---

## Timeline of Corruptions

| Commit | Date | Status | Notes |
|--------|------|--------|-------|
| `b382768e` | Dec 25 | ✅ v6 format | First commit - known-good state |
| `f662f454` | Dec 27 11:43 | ❌ v7 format | Path bug fix - **CORRUPTED** |
| `fad0ffb8` | Dec 27 14:19 | ✅ v6 format | Restored from afea32c9 |
| `ab8e32ce` | Dec 27 22:30 | ❌ v7 format | Table processor - **CORRUPTED AGAIN** |
| `9cd4f66d` | Dec 30 | ✅ v6 format | Restored from b382768e + protection added |

**Evidence:** Git history shows file size stayed constant (6099497 bytes) but version bytes changed from `0006` to `0007` during testing.

---

## Detection Commands

### Check Database Version
```bash
# First 2 bytes should be 0006 for v6, 0007 for v7
xxd -l 2 databases/Frontier-v6.root

# Expected for v6:
# 00000000: 0006                                     ..

# Corrupted (v7):
# 00000000: 0007                                     ..
```

### Check Git History for Corruption
```bash
# Show version bytes at each commit
for commit in $(git log --oneline --all -- databases/Frontier-v6.root | awk '{print $1}'); do
    echo -n "$commit: "
    git show "$commit:databases/Frontier-v6.root" | xxd -l 2 -p
done
```

---

## Protection Implemented

### 1. Pre-Migration Corruption Detection
`tools/run_headless_tests.sh` now checks if v6 database has been corrupted before migration:

```bash
version_bytes=$(xxd -l 2 -p databases/Frontier-v6.root)
if [ "$version_bytes" = "0007" ]; then
    echo "WARNING: Frontier-v6.root has been corrupted (v7 format), restoring from git..."
    chmod 644 databases/Frontier-v6.root  # Make writable
    git checkout databases/Frontier-v6.root
    chmod 444 databases/Frontier-v6.root  # Protect
fi
```

### 2. Read-Only Protection
```bash
# Ensure v6 database is read-only to prevent accidental modification
chmod 444 databases/Frontier-v6.root
```

### 3. Post-Migration Verification
After migration completes, verify v6 wasn't modified:

```bash
version_bytes=$(xxd -l 2 -p databases/Frontier-v6.root)
if [ "$version_bytes" != "0006" ]; then
    echo "ERROR: Frontier-v6.root was corrupted during migration!"
    echo "Expected v6 (0006), found: $version_bytes"
    exit 1
fi
```

---

## Manual Recovery Procedure

If corruption is detected outside of the test runner:

### Step 1: Verify Corruption
```bash
xxd -l 2 databases/Frontier-v6.root
# If shows 0007, database is corrupted
```

### Step 2: Find Last Known-Good Commit
```bash
git log --reverse --oneline -- databases/Frontier-v6.root | head -1
# Usually b382768e (first commit)
```

### Step 3: Restore from Git
```bash
chmod 644 databases/Frontier-v6.root  # Make writable if read-only
git show b382768e:databases/Frontier-v6.root > databases/Frontier-v6.root
chmod 444 databases/Frontier-v6.root  # Protect
```

### Step 4: Verify Restoration
```bash
xxd -l 2 databases/Frontier-v6.root
# Should show: 00000000: 0006

ls -lh databases/Frontier-v6.root
# Should show: -r--r--r-- (read-only)
```

---

## Why Read-Only Protection?

**Problem:** Developers running tests or migrations could accidentally modify the v6 database in-place.

**Solution:** `chmod 444` (read-only for all users) prevents:
- Accidental writes during testing
- Migration tools from modifying input file
- Tools from opening database in write mode

**Trade-off:** Git operations like `git checkout` will fail on read-only files. The test runner handles this by temporarily making the file writable (`chmod 644`) before restore, then protecting it again afterward.

---

## Migration Process (Correct)

The CLI **creates a new output file** during migration, it does NOT modify the input:

```bash
# Clean migration workflow:
rm -f databases/Frontier-v7.root

# Run CLI with v6 database - creates v7 output file automatically
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root databases/Frontier-v6.root -e "1"

# Result:
# - Input:  databases/Frontier-v6.root (unchanged, still v6)
# - Output: databases/Frontier-v7.root (new file, v7 format)
```

**Pattern:** `INPUT.root` → `INPUT-v7.root`

See `CLAUDE.md` section "Database Migration (v6→v7)" for details.

---

## Prevention Checklist

Before committing changes to `databases/Frontier-v6.root`:

- [ ] Verify version bytes: `xxd -l 2 databases/Frontier-v6.root` shows `0006`
- [ ] Check file size: Should be ~5.8M (not ~9.9M like v7)
- [ ] Verify read-only: `ls -l` shows `-r--r--r--` permissions
- [ ] If corrupted, restore from `b382768e` before committing

**Rule:** Never commit `databases/Frontier-v6.root` unless you're intentionally updating the test fixture with a new known-good v6 database.

---

## Test Infrastructure Impact

### Before Protection
- v6 database could be corrupted during testing
- Developers might commit corrupted database
- Migration tests would use corrupted input → invalid results
- Hard to detect without manual verification

### After Protection
- Automatic corruption detection before every test run
- Auto-restore from git if corruption detected
- Read-only protection prevents accidental writes
- Post-migration verification catches new corruption bugs
- Tests fail fast if migration modifies input

---

## Related Documentation

- `CLAUDE.md` - Migration process and testing guidelines
- `planning/phase3/MIGRATION_VALIDATION_REPORT.md` - Migration testing procedures
- `docs/external_table_variable_management.md` - Database format differences

---

## Lessons Learned

1. **Test fixtures must be protected** - Read-only permissions prevent accidental modification
2. **Verify assumptions** - Don't assume migration creates output file, verify it
3. **Git history is valuable** - Checking version bytes at each commit revealed corruption pattern
4. **Fail fast** - Post-migration verification catches bugs immediately, not later
5. **Documentation is critical** - Future developers need to know about this protection

---

**Last Updated:** 2025-12-30
**Next Review:** When migration process changes or new test fixtures are added
