# Timestamp Type Audit Guide

Complete guide to auditing timestamp types when integrating new source files into Frontier headless builds.

## Context

Frontier migrated to 64-bit timestamps (`frontier_time_t` = `int64_t`) to avoid the Year 2038 problem. However, legacy code may still use `uint32_t` for timestamps, defeating this migration.

**When pulling new source files into headless builds, ALWAYS audit for uint32_t timestamp usage.**

---

## Automated Checking

The test suite automatically checks for datetime type issues:

```bash
# Runs automatically as part of:
./tools/run_headless_tests.sh

# Or run manually:
./tools/check_datetime_types.sh
```

**What It Checks:**
1. `long` or `unsigned long` used with timestamp field names (timecreated, timemodified, timelastsave)
2. `int32_t`/`uint32_t` with timestamp fields (warnings for manual review)
3. Function parameters using `long` for date/time values

---

## Pre-Merge Checklist for New Files

Before adding any file to headless builds (frontier-cli/Makefile), run this audit:

```bash
# Search for potential timestamp fields
grep -n "uint32_t.*time\|uint32_t.*date\|uint32_t.*second" <new_file>.c
```

For each match, determine if it's:
1. **Disk format structure** (OK - for backward compatibility with legacy databases)
2. **In-memory state** (MUST migrate to `frontier_time_t`)
3. **API parameters** (MUST use `int64_t`/`frontier_time_t`)

---

## Example: Correct Pattern

### Legacy Disk Format (OK to keep uint32_t)

```c
/* Disk format (legacy v4) - OK to keep uint32_t */
typedef struct legacy_diskheader {
    uint32_t timecreated;    // ✅ OK - reading old database format
    uint32_t timelastsave;   // ✅ OK - with conversion to frontier_time_t
} legacy_diskheader;
```

**Why this is OK:** We need to read old v4/v6 databases that use 32-bit timestamps on disk. The key is to convert to 64-bit in memory.

### In-Memory State (MUST use frontier_time_t)

```c
/* In-memory state - MUST use frontier_time_t */
typedef struct runtime_state {
    frontier_time_t timecreated;    // ✅ Correct - 64-bit in memory
    frontier_time_t timelastsave;   // ✅ Correct - 64-bit in memory
} runtime_state;
```

**Why this is required:** All in-memory timestamp state must be 64-bit to avoid Year 2038 problem.

### Conversion When Reading Disk Format

```c
/* Conversion when reading disk format */
state.timecreated = (frontier_time_t)disk_header.timecreated;  // ✅ Widen to 64-bit
```

**Pattern:** Always cast/widen 32-bit disk timestamps to 64-bit when loading into memory.

---

## Whitelisted Files

These files contain legacy code for Mac GUI only (not in headless builds):

- `Common/headers/claybrowser.h`
- `Common/source/claybrowserexpand.c`
- `portable/shelltypes_portable.h`
- `portable/wptext_runtime.c` (legacy wp_diskheader disk format)

These files are excluded from automated checks since they're not compiled in headless builds.

---

## When You See Warnings About uint32_t

### Decision Matrix

| Context | uint32_t Usage | Status |
|---------|----------------|--------|
| Legacy v4/v6 disk format structures | timecreated, timelastsave, etc. | ✅ OK (backward compatibility) |
| Modern v7 (BE64) disk format structures | timecreated, timelastsave, etc. | ❌ BAD (use uint64_t) |
| In-memory structures | Any time field | ❌ BAD (use frontier_time_t / int64_t) |
| API parameters | Any time parameter | ❌ BAD (use int64_t / frontier_time_t) |

### Rule of Thumb

- **Legacy readers** (`Common/source/legacy/`, v4/v6 disk formats): uint32_t OK
- **Everything else**: Use int64_t or frontier_time_t

---

## Common Scenarios

### Scenario 1: Integrating Old Mac GUI Code

**Problem:** You're pulling in a file from the Mac GUI codebase that has `uint32_t` timestamp fields.

**Solution:**
1. Check if it's in-memory or disk format
2. If in-memory: Change to `frontier_time_t`
3. If disk format: Add comment explaining it's for legacy compatibility
4. Ensure conversions happen when reading from disk

### Scenario 2: Creating New Database Format

**Problem:** You're adding a new table/structure to v7 database format.

**Solution:**
- Use `int64_t` or `uint64_t` for timestamp fields (64-bit)
- **Never** use `uint32_t` in new v7 format structures
- Follow BE64 (Big Endian 64-bit) format standards

### Scenario 3: API Function Taking Time Parameter

**Problem:** You're adding a function that takes a timestamp as parameter.

**Solution:**
```c
// ✅ CORRECT
boolean mynewfunction(frontier_time_t timestamp) { ... }

// ❌ WRONG
boolean mynewfunction(uint32_t timestamp) { ... }
```

---

## Detailed Findings

For complete audit findings and historical context, see:
- `planning/phase3/datetime_handling_audit.md` - Complete audit report

---

## References

- `docs/frontier_time_t_standard.md` - 64-bit time standard specification
- PR #231 - Discovered during file verb implementation
- Issue #167 - Original time_t portability bug
- `tools/check_datetime_types.sh` - Automated checking script

---

## Quick Reference

**Command to audit a file:**
```bash
grep -n "uint32_t.*time\|uint32_t.*date\|uint32_t.*second" <file>.c
```

**Run full check:**
```bash
./tools/check_datetime_types.sh
```

**Golden rule:**
> Legacy disk formats: uint32_t OK
> Everything else: Use frontier_time_t (int64_t)
