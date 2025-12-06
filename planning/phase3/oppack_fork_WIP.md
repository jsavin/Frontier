# Outline Packer Fork - Work in Progress

**Status**: Partial Implementation
**Date**: 2025-12-05
**Branch**: feature/64bit-datetime-fields
**Related**: planning/phase3/carbon_migration/outline_script_payload.md

## What's Been Done

### 1. Created Legacy Packer (oppack_legacy.c)
- Copied original oppack.c from develop branch
- Handles v2/v3 format with 32-bit timestamps (120-byte header)
- All function names suffixed with `_legacy`
- **Purpose**: Read old v6 database outline payloads

### 2. Created Modern Packer (oppack_modern.c)
- Renamed oppack.c → oppack_modern.c
- Implemented v4 portable header format (1068 bytes)
- **Dropped UI fields**: font/fontsize/fontstyle, colors, scroll positions, window rects
- **Kept runtime fields**: timecreated/timelastsave (64-bit), ctsaves, outlinesignature, platform, fltextmode
- Added 1024-byte reserved expansion area
- **Purpose**: Write v7 database outline payloads

### 3. Updated Makefile
- Added both oppack_modern.c and legacy/oppack_legacy.c to build

## What Still Needs To Be Done

### Critical: Version Dispatch Not Wired Up

The split is incomplete because **there's no dispatcher** that routes between legacy and modern unpackers based on version number.

**Problem**:
- `langexternal.c` calls `opverbunpack()` in `opverbs.c`
- `opverbs.c` doesn't actually call `opunpack()` directly - it creates a lazy-loaded variable
- The actual unpacking happens later when the outline is loaded from disk
- **Current state**: All outline unpacking goes through oppack_modern.c, which only handles v4
- **Result**: v6 databases with v2/v3 outline payloads will FAIL to unpack

**Solutions** (pick one):

####  Option A: Peek-and-Dispatch in opverbs.c
- Modify `opverbunpack()` to peek at version number in the packed handle
- If version 2/3 → call `opunpack_legacy()`
- If version 4 → call `opunpack()` (modern)
- Similar to how `db.c` does `db_read_legacy()` vs `db_read_modern()`

#### Option B: Unified opunpack() Dispatcher
- Keep single `opunpack()` function that reads version number
- Dispatch to `opunpackversion2()` (from oppack_legacy.c) for v2/v3
- Dispatch to `opunpackversion4()` (from oppack_modern.c) for v4
- Requires moving opunpackversion2/opunpackversion4 to be externally visible

#### Option C: Lazy Version Detection
- Have oppack_modern.c's `opunpack()` try to read v4 header
- On version mismatch, call into oppack_legacy.c
- Messier but might work

### Medium Priority: Forward Declarations

The legacy oppack functions need proper header declarations so opverbs.c can call them.

**Current**:
- Functions in oppack_legacy.c are not declared in any header
- opverbs_legacy.c has forward declarations but doesn't use them

**Fix**:
- Either add declarations to opverbs.h (with `_legacy` suffix)
- Or create new oppack_legacy.h header

### Low Priority: Migration Path

When v6→v7 migration happens, outline payloads should be converted from v2/v3 → v4.

**Current**:
- No migration code yet
- Old v6 outlines will stay in v2/v3 format even in migrated v7 database

**Fix**:
- Add outline payload migration to db_format.c migration flow
- Read v2/v3 → unpack to memory → repack as v4 → write back

## Testing Required

1. **v6 Read Test**: Verify oppack_legacy correctly reads old v2/v3 outlines from v6 database
2. **v7 Write Test**: Verify oppack_modern writes v4 portable header (no font fields, 64-bit timestamps)
3. **v7 Round-Trip**: Write v4 outline → read it back → verify correctness
4. **Header Size**: Verify v4 header is exactly 1068 bytes
5. **Alignment**: Verify int64_t fields are 8-byte aligned in v4 header

## Files Modified

```
Common/source/oppack_modern.c       (was oppack.c - now v4 only)
Common/source/legacy/oppack_legacy.c (new - v2/v3 only)
Common/source/legacy/opverbs_legacy.c (partial update)
tests/Makefile                       (added both oppack files)
```

## Next Steps

1. **Decide on dispatch strategy** (Option A recommended)
2. **Implement version dispatcher**
3. **Add forward declarations/headers**
4. **Test v6 database unpacking**
5. **Test v7 database round-trip**
6. **Add migration code** (optional for first PR)

## References

- planning/phase3/carbon_migration/outline_script_payload.md (v4 header spec)
- planning/phase3/modern_reader_writer_split.md (db split pattern to follow)
- planning/phase2/64bit_datetime_fields_plan.md (why we're doing this)
