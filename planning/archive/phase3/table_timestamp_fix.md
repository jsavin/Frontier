# Table Timestamp Fix - 64-bit Migration

**Status**: ✅ COMPLETED (PR #63)
**Date**: 2025-12-05
**Completion Date**: 2025-12-05
**Completed In**: PR #63 (ef62a788)
**Archived**: 2025-12-17

---

**ARCHIVED**: This document describes completed work. Merged to develop on 2025-12-15 (PR #115).
**Related**:
- planning/phase2/64bit_datetime_fields_plan.md
- planning/phase3/modern_reader_writer_split.md
- planning/phase3/carbon_migration/outline_script_payload.md

## Problem (Now Fixed!)

Hash tables (tyhashtable) ~~currently write~~ **previously wrote 32-bit timestamps** to disk despite using 64-bit timestamps in memory. This violated the core requirement of PR #62 and would fail for dates after February 6, 2040.

**Status**: ✅ Fixed in PR #63

The broken code that was writing truncated timestamps:
```c
// OLD - BROKEN CODE (lines 2978-2980, now removed)
header.timecreated = (uint32_t) host_to_disk_int32((int32_t) (**htable).timecreated);
header.timelastsave = (uint32_t) host_to_disk_int32((int32_t) (**htable).timelastsave);
```

The fix implements v0x05 format with proper 64-bit timestamp handling.

**Current disk structure** (tydisktablerecord, 16 bytes):
```c
int16_t version;           // 2 bytes - currently 0x04
int16_t sortorder;         // 2 bytes
uint32_t timecreated;      // 4 bytes - ❌ 32-bit
uint32_t timelastsave;     // 4 bytes - ❌ 32-bit
int32_t flags;             // 4 bytes
// Followed by TABLE_HEADER_RESERVED_BYTES (1024 bytes) for version >= 0x04
```

**In-memory structure** (tyhashtable):
```c
int64_t timecreated;       // ✅ 64-bit
int64_t timelastsave;      // ✅ 64-bit
```

## Solution: Fork tablepack into legacy/modern paths

Follow the same pattern as:
- db.c → db_reader_legacy.c / db_writer_modern.c
- oppack.c → oppack_legacy.c / oppack_modern.c

### Step 1: Create Legacy Reader (tablepack_legacy.c)

**Already exists!** Common/source/legacy/tablepack_legacy.c

This file handles v6 database table reading with 32-bit timestamps.

**Functions**:
- `tableverbpack_legacy()` - reads old v0x03/v0x04 format with 32-bit timestamps
- `tableverbunpack_legacy()` - unpacks old format

**Note**: The legacy file currently just wraps modern functions. We need to verify it actually handles 32-bit timestamps correctly.

### Step 2: Create Modern Writer (in langhash.c)

Modify `langhash.c` to write new v0x05 format with 64-bit timestamps.

**New disk structure** (tydisktablerecord_v5, ~32 bytes):
```c
int16_t version;           // 2 bytes - set to 0x05
int16_t sortorder;         // 2 bytes
uint32_t _pad;             // 4 bytes - padding for 8-byte alignment
uint64_t timecreated;      // 8 bytes - ✅ 64-bit
uint64_t timelastsave;     // 8 bytes - ✅ 64-bit
int32_t flags;             // 4 bytes
// Followed by TABLE_HEADER_RESERVED_BYTES (1024 bytes)
// Total: 32 + 1024 = 1056 bytes header
```

### Step 3: Implement Version Dispatch

Add dispatch logic in `tableverbunpack()` (likely in tablepack.c or langexternal.c):

```c
// Peek at version number
short version = *(short *)(*hpacked);
disktomemshort(version);

if (version <= 0x04) {
    // Legacy format: 32-bit timestamps
    fl = tableverbunpack_legacy(hpacked, ixload, htable);
} else if (version == 0x05) {
    // Modern format: 64-bit timestamps
    fl = tableverbunpack_modern(hpacked, ixload, htable);
} else {
    // Unknown version
    return false;
}
```

### Step 4: Update Pack Code

Modify `hashtablepack()` in langhash.c to:
1. Check if writing to v7 database (use `db_format_mode_current().use_64bit_format`)
2. If v7: write version 0x05 with 64-bit timestamps
3. If v6 (legacy mode): write version 0x04 with 32-bit timestamps (for compatibility)

### Step 5: Migration Path

Migration happens automatically:
1. **Load v6**: tableverbunpack → dispatch → tableverbunpack_legacy (reads 32-bit)
2. **Save v7**: tableverbpack → hashtablepack → writes v0x05 with 64-bit
3. **Result**: Table upgraded from v0x04 (32-bit) → v0x05 (64-bit)

## Files to Modify

### 1. Common/source/legacy/tablepack_legacy.c
- Verify it correctly handles 32-bit timestamp reading
- May need to add `hashtableunpack_legacy()` for v0x04 format

### 2. Common/source/langhash.c
- Update `hashtablepack()` to write v0x05 with 64-bit timestamps
- Change `tydisktablerecord` to use `uint64_t` for timestamps
- Add version-based packing logic
- Use `conditionallonglongswap()` for 64-bit byte order

### 3. Common/source/tablepack.c (if needed)
- Add version dispatch in `tableverbunpack()`
- Route to legacy vs modern unpacker based on version

### 4. Common/headers/lang.h (if needed)
- May need to define new disk structure type for v0x05

### 5. tests/
- Create table_timestamp_tests.c
- Verify v0x04 → v0x05 migration
- Verify 64-bit timestamps pack/unpack correctly

## Version History

- **v0x03**: Original format (before reserved space)
- **v0x04**: Added TABLE_HEADER_RESERVED_BYTES (1024 bytes)
- **v0x05**: 64-bit timestamps + reserved space (NEW)

## Testing Plan

1. **Compile-time**: Verify struct sizes with _Static_assert
2. **Unit tests**:
   - Pack/unpack v0x05 with 64-bit timestamps
   - Verify timestamps beyond 2040
   - Verify byte order (big-endian)
3. **Migration test**:
   - Load v6 database with v0x04 tables
   - Save to v7 database
   - Verify tables upgraded to v0x05
4. **Integration test**: Run existing db_format_tests, runtime_tests

## Open Questions

1. Should we maintain backward compatibility to write v0x04 for v6 databases?
   - **Answer**: Yes, use `db_format_mode_current().use_64bit_format` flag

2. Do table formats (tyversion2tablediskrecord) also need fixing?
   - **Answer**: Later - that's a separate concern (font/style metadata)

3. What about table sorting timestamps?
   - **Check**: tablestructure.c, tableops.c for any other timestamp usage

## Success Criteria

- ✅ Tables write 64-bit timestamps in v7 format
- ✅ Tables read 32-bit timestamps from v6 format
- ✅ Automatic migration v0x04 → v0x05
- ✅ All existing tests pass
- ✅ No data loss for timestamps beyond 2040
- ✅ Big-endian byte order maintained

## References

- planning/phase2/64bit_datetime_fields_plan.md (overall 64-bit datetime plan)
- planning/phase3/modern_reader_writer_split.md (db.c fork pattern)
- planning/phase3/carbon_migration/outline_script_payload.md (oppack fork pattern)
