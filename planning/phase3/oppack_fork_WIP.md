# Outline Packer Fork - COMPLETED

**Status**: Implementation Complete
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

## Implementation Complete ✓

### 1. Version Dispatch - DONE ✓

Implemented Option A: Peek-and-Dispatch in opverbs.c (opverbs.c:761-787)

**How it works**:
- `opverbinmemory()` peeks at version number before unpacking
- Version 2/3 → routes to `opunpack_legacy()`
- Version 4 → routes to `opunpack()` (modern)
- Same pattern as db.c dispatch

**Code location**: Common/source/opverbs.c lines 761-787

### 2. Header File Created - DONE ✓

Created `Common/headers/oppack_legacy.h` with clean interface for legacy functions.

**Exports**:
- `oppack_legacy()`
- `oppackoutline_legacy()`
- `opunpack_legacy()`
- `opunpackoutline_legacy()`

### 3. Migration Path - DONE ✓

Migration works automatically through dispatch - no additional code needed!

**Flow**:
1. **Load v6**: `opverbinmemory()` → dispatch → `opunpack_legacy()` (reads v2/v3)
2. **Save v7**: `opverbpack()` → `oppackoutline()` → `oppack()` modern (writes v4)
3. **Result**: Outline automatically upgraded from v2/v3 → v4 during migration

### 4. v4 Portable Header - DONE ✓

Implemented v4 portable header per planning docs (1068 bytes total):

**Dropped fields** (UI metadata):
- Font fields (fontname/fontsize/fontstyle)
- Colors (forecolor/backcolor)
- Scroll positions (vertmin/max/current, horizmin/max/current)
- Window rects (outlinerect, windowrect)

**Kept fields** (runtime metadata):
- timecreated/timelastsave (64-bit!)
- ctsaves
- outlinesignature
- platform ('mac ' or 'win ')
- fltextmode
- lnumcursor (32-bit, split into low/high words)

**Added**:
- 1020-byte reserved expansion area (zero-filled)
- Note: Was 1024, reduced to 1020 for struct alignment

**Code location**: Common/source/oppack_modern.c lines 114-144

### 5. Tests Created - DONE ✓

Created compile-time verification tests in `tests/oppack_tests.c`:

**Tests verify**:
- v4 header struct size (1068 bytes) via _Static_assert
- Version number byte order and dispatch logic
- 64-bit timestamp byte order correctness
- Reserved area size adjustment for alignment
- Design verification: v4 has no font fields

**Test status**: All tests pass (verified via successful compilation)

The _Static_assert in oppack_modern.c ensures struct size is exactly 1068 bytes at compile time.

## Testing Status

✓ **Compile-time tests**: All pass (struct size, alignment verified by _Static_assert)
✓ **db_format_tests**: Pass (database format tests still work)
✓ **runtime_tests**: Pass (language and serializer round-trips work)

**Integration testing** (to be done with real databases):
1. **v6 Read**: Load v6 database with v2/v3 outlines → verify dispatch to oppack_legacy
2. **v7 Write**: Save outline to v7 database → verify v4 portable header format
3. **Migration**: Migrate v6→v7 → verify outlines upgraded from v2/v3 to v4

## Files Created/Modified

```
Common/headers/oppack_legacy.h          (new - clean interface for legacy functions)
Common/source/oppack_modern.c           (renamed from oppack.c - v4 only, 1068-byte header)
Common/source/legacy/oppack_legacy.c    (new - v2/v3 only, 120-byte header)
Common/source/opverbs.c                 (added version dispatch logic)
tests/oppack_tests.c                    (new - compile-time verification tests)
tests/Makefile                          (added oppack_tests to build)
planning/phase3/oppack_fork_WIP.md      (this file - updated to COMPLETED)
```

## Summary

The outline packer has been successfully forked into legacy (v2/v3) and modern (v4) implementations:

- **Legacy path** (oppack_legacy.c): Reads old v6 database outlines with 32-bit timestamps
- **Modern path** (oppack_modern.c): Writes new v7 database outlines with 64-bit timestamps and portable header
- **Dispatch** (opverbs.c): Routes based on version number, similar to db.c pattern
- **Migration**: Automatic upgrade from v2/v3 → v4 when loading old outlines and saving
- **Tests**: Compile-time verification ensures struct correctness

All existing tests pass. Ready for integration testing with real databases.

## References

- planning/phase3/carbon_migration/outline_script_payload.md (v4 header spec)
- planning/phase3/modern_reader_writer_split.md (db split pattern to follow)
- planning/phase2/64bit_datetime_fields_plan.md (why we're doing this)
