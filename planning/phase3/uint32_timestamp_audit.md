# uint32_t Timestamp Audit - Phase 3 TODO

## Context

During PR #231 (File Verb Bindings Phase 2), we discovered that `timet_to_frontierseconds()` was incorrectly returning `uint32_t` instead of `frontier_time_t` (int64_t). This defeats the purpose of the 64-bit time migration documented in `docs/frontier_time_t_standard.md`.

This audit identifies all remaining `uint32_t` timestamp usage in the codebase to ensure we complete the 64-bit time migration for all files included in headless builds.

## Findings

### CRITICAL: Needs Immediate Fix (Headless Build)

#### 1. portable/wptext_runtime.c - In-Memory State Truncation

**Location**: Lines 93-99 (wp_portable_state structure)

**Problem**: In-memory state uses `long` instead of `frontier_time_t`/`int64_t` for timestamps:

```c
typedef struct wp_portable_state {
    dbaddress address;
    long timecreated;      // ❌ Should be frontier_time_t (int64_t)
    long timelastsave;     // ❌ Should be frontier_time_t (int64_t)
    long ctsaves;
    long maxpos;
    long buffersize;
    ...
```

**Impact**:
- On platforms where `long` is 32-bit (Windows, 32-bit systems), timestamps are truncated
- API functions `wpverbgettimes()`/`wpverbsettimes()` correctly use `int64_t`, but truncation happens when storing/retrieving from state
- WPText objects won't preserve full 64-bit timestamps on some platforms

**Fix Required**:
```c
typedef struct wp_portable_state {
    dbaddress address;
    frontier_time_t timecreated;    // ✅ Use frontier_time_t
    frontier_time_t timelastsave;   // ✅ Use frontier_time_t
    frontier_time_t ctsaves;
    frontier_time_t maxpos;
    frontier_time_t buffersize;
    ...
```

**Note**: Disk format structures (`wp_portable_diskheader`) can keep `uint32_t` for backward compatibility, but conversion to/from in-memory `frontier_time_t` must happen during serialization.

**Files to modify**:
- `portable/wptext_runtime.c` - Change in-memory state structure
- `portable/wptext_runtime.c` - Verify conversion logic when reading/writing disk format

**Related PR**: This file is included in headless builds (frontier-cli/Makefile:126)

---

### OK: Legacy Disk Formats (No Action Needed)

#### 2. Common/source/langhash.c - Legacy v4 Disk Structures

**Location**: Lines 406, 442-443

**Status**: ✅ Intentional for backward compatibility

These are legacy v4 disk structures for reading old databases:
```c
typedef struct tydisktablerecord_v4 {  /* Legacy v0x04 structure */
    uint32_t timecreated;    // ✅ OK - legacy disk format
    uint32_t timelastsave;   // ✅ OK - legacy disk format
    ...
```

Current v5 format correctly uses `uint64_t` (lines 455-463):
```c
typedef struct tydisktablerecord {  /* Current disk format */
    uint64_t timecreated;     // ✅ Correct - 64-bit
    uint64_t timelastsave;    // ✅ Correct - 64-bit
    ...
```

**No action needed** - this is proper handling of legacy formats.

#### 3. Common/source/timedate.c - Mac-Only Code Path

**Location**: Lines 512, 541

**Status**: ✅ Not used in headless builds

These casts to `uint32_t` are in `#else` blocks for Mac-specific CoreFoundation code:
```c
#if defined(FRONTIER_HEADLESS)
    /* Portable implementation - correctly uses int64_t */
    time_t unix_secs = (secs > FRONTIER_EPOCH_TO_UNIX_OFFSET) ? (time_t)(secs - FRONTIER_EPOCH_TO_UNIX_OFFSET) : (time_t)0;
#else
    /* Mac-only: casts to uint32_t for CoreFoundation API */
    CFAbsoluteTime timeInterval = ((uint32_t) secs) - kCFAbsoluteTimeIntervalSince1904;
#endif
```

**No action needed** - headless builds use the correct code path.

---

## Recommended Action Plan

### Phase 3 Work Item

1. **Fix wptext_runtime.c timestamp fields**:
   - Change `wp_portable_state` structure to use `frontier_time_t`
   - Verify conversion logic when reading/writing `wp_portable_diskheader` (which legitimately uses `uint32_t` for disk format)
   - Add test to verify timestamps survive round-trip on 32-bit platforms

2. **Establish ongoing audit process**:
   - Add grep pattern to pre-merge checks: Search for `uint32_t.*time|uint32_t.*date` in new files
   - Document this requirement in `docs/frontier_time_t_standard.md` under "Developer Guidelines"
   - Update `CLAUDE.md` with guidance: "When pulling new source files into headless builds, audit for uint32_t timestamp usage"

### Future Work (When Pulling New Files Into Headless)

**Checklist for any new file added to headless builds**:

1. Search file for: `uint32_t.*time|uint32_t.*date|uint32_t.*second`
2. Identify if timestamps are:
   - **Disk format** (OK to keep uint32_t with proper conversion)
   - **In-memory state** (MUST use frontier_time_t)
   - **API parameters** (MUST use int64_t/frontier_time_t)
3. If in-memory/API, migrate to frontier_time_t before merge

## Related Documentation

- **docs/frontier_time_t_standard.md** - 64-bit time migration standard
- **planning/archive/phase2/64bit_datetime_fields_plan.md** - Original migration plan
- **Issue #167** - P0 bug report on time_t portability
- **PR #193** - Implementation of frontier_time_t standard
- **PR #231** - Discovered during file verb implementation

## Summary

The 64-bit time migration is mostly complete, with one critical exception: **wptext_runtime.c in-memory state still uses `long` for timestamps**. This must be fixed before wptext objects can reliably store dates beyond 2038 on all platforms.

Legacy disk format structures correctly use uint32_t for backward compatibility, and timedate.c's uint32_t casts are Mac-only (not used in headless).

**Action Required**: Fix wptext_runtime.c in Phase 3.
