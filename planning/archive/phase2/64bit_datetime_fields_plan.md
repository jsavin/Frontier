# 64-bit DateTime Fields Implementation Plan

**Branch:** `feature/64bit-datetime-fields`
**Status:** ✅ Completed
**Archived:** 2025-12-17
**Date:** 2025-12-05
**Objective:** Convert all time fields from 32-bit to 64-bit to prevent 2040 overflow

---

**ARCHIVED**: This document describes completed work. DateTime field upgrade merged to develop (PR #62, commit 91bb4138). Table timestamp fix merged separately (PR #63, commit ef62a788).

---

## Problem Statement

### Current State (32-bit)
All time metadata fields are currently 32-bit unsigned:
- `unsigned long timecreated, timelastsave` in structures
- Mac epoch (1904) with 32-bit unsigned → overflows February 6, 2040
- Affects: WPText, Outline, Script, Pict, Table, File metadata

### Target State (64-bit)
- Change to `int64_t timecreated, timelastsave`
- Mac epoch (1904) with 64-bit signed → extends to year ~584 billion
- Full compatibility with legacy databases via migration
- No user script changes needed

---

## Affected Structures

### 1. **lang.h** - Script objects
```c
// Current:
unsigned long timecreated, timelastsave; /*number of seconds since 1/1/04*/

// Target:
int64_t timecreated, timelastsave; /*number of seconds since 1/1/04*/
```

### 2. **op.h** - Outline objects
```c
// Current:
unsigned long timecreated, timelastsave; /*number of seconds since 1/1/04*/

// Target:
int64_t timecreated, timelastsave; /*number of seconds since 1/1/04*/
```

### 3. **tableformats.h** - Table objects
```c
// Current:
long timecreated, timelastsave; /*maybe we'll use these at some later date?*/

// Target:
int64_t timecreated, timelastsave;
```

### 4. **wpengine.h** - WPText objects
```c
// Current:
long timelastsave, timecreated;

// Target:
int64_t timelastsave, timecreated;
```

### 5. **pict.h** - Picture objects
```c
// Current:
long timecreated, timelastsave; /*maybe we'll use these at some later date?*/

// Target:
int64_t timecreated, timelastsave;
```

### 6. **file.h** - File metadata
```c
// Current:
unsigned long timecreated, timemodified, timeaccessed;

// Target:
int64_t timecreated, timemodified, timeaccessed;
```

---

## Implementation Steps

### Phase 1: Update Header Definitions ✅

**Files to modify:**
- `Common/headers/lang.h`
- `Common/headers/op.h`
- `Common/headers/tableformats.h`
- `Common/headers/wpengine.h`
- `Common/headers/pict.h`
- `Common/headers/file.h`

**Changes:**
- Replace `unsigned long` time fields with `int64_t`
- Replace `long` time fields with `int64_t`
- Ensure `<stdint.h>` is included

### Phase 2: Update Packing Code

**Files to modify:**
- `Common/source/oppack.c` - Outline packing/unpacking
- `Common/source/tablepack.c` - Table packing/unpacking
- `Common/source/wpengine.c` - WPText packing/unpacking (if applicable)
- `Common/source/pict.c` - Picture packing/unpacking (if applicable)
- Any other pack/unpack code for these structures

**Changes for each:**
1. Find calls that pack/unpack `timecreated`, `timelastsave`
2. Change from 4-byte (32-bit) to 8-byte (64-bit) operations
3. Ensure big-endian byte order for v7 format
4. Update size calculations for structures

### Phase 3: Update Legacy Readers

**Files to modify:**
- `Common/source/legacy/oppack_legacy.c` (if exists)
- `Common/source/legacy/tablepack_legacy.c`
- Any other legacy pack code

**Changes:**
1. Legacy readers must read 4-byte values
2. Zero-extend unsigned 32-bit to 64-bit (for v6 migration)
3. Sign-extend signed 32-bit to 64-bit where applicable

### Phase 4: Update Migration Code

**Files to modify:**
- Migration code that handles v6→v7 conversion
- Ensure 32-bit time values are properly widened

**Strategy:**
- v6 files have 32-bit unsigned time values
- Read as 32-bit, zero-extend to 64-bit signed
- Write as 64-bit in v7 format

### Phase 5: Update Tests

**Files to modify:**
- `tests/db_format_tests.c`
- Any tests that verify structure sizes or packing
- Add specific tests for datetime fields

**Test cases:**
1. Pack/unpack 64-bit time values
2. Round-trip test: write → read → verify
3. Migration test: v6 32-bit → v7 64-bit
4. Boundary tests: year 1904, 2040, 2050, 9999
5. Verify structure alignment and sizes

---

## Detailed Implementation

### Finding Pack/Unpack Code

**Search for time field packing:**
```bash
grep -rn "timecreated\|timelastsave\|timemodified" Common/source/*.c | grep -i pack
```

**Common patterns to look for:**
- `pack4byte()` / `unpack4byte()` → change to `pack8byte()` / `unpack8byte()`
- Direct memory copies of time fields
- Size calculations that include time fields

### Packing/Unpacking Helpers

**Expected helpers (may need to add):**
```c
// Write 64-bit big-endian
void pack8byte(int64_t value, void *dest);

// Read 64-bit big-endian
int64_t unpack8byte(void *src);
```

### Migration Helper

**For v6→v7 migration:**
```c
int64_t migrate_time_field(uint32_t v6_time) {
    // v6 uses unsigned 32-bit, just zero-extend
    return (int64_t)v6_time;
}
```

---

## Testing Strategy

### 1. Unit Tests

**Test file:** `tests/unit/test_datetime_fields.c` (new)

```c
void test_pack_unpack_64bit_time() {
    int64_t original = 4607452800LL; // Year 2050 in Mac epoch
    char buffer[8];

    pack8byte(original, buffer);
    int64_t unpacked = unpack8byte(buffer);

    assert(unpacked == original);
}

void test_v6_migration() {
    uint32_t v6_time = 3000000000U; // Some date in 1999
    int64_t v7_time = migrate_time_field(v6_time);

    assert(v7_time == 3000000000LL);
    assert(v7_time >= 0); // Ensure positive
}

void test_year_2050() {
    // 2050-01-01 in Mac epoch (1904)
    int64_t year_2050 = 4607452800LL;

    // Should not overflow
    assert(year_2050 > 0);
    assert(year_2050 > 4294967295LL); // Larger than 32-bit max
}
```

### 2. Integration Tests

**Verify with real objects:**
1. Create WPText with current time
2. Save to database
3. Read back from database
4. Verify `timecreated` matches

### 3. Migration Tests

**Test v6→v7 conversion:**
1. Load v6 fixture with known time values
2. Migrate to v7
3. Verify times are correctly widened
4. Verify no overflow or corruption

---

## Compatibility Considerations

### v6 Files
- Read with 32-bit unsigned
- Zero-extend to 64-bit signed
- All dates before 2040 work correctly

### v7 Files
- Always write 64-bit signed
- Big-endian byte order
- Future-proof for centuries

### In-Memory
- All structures use `int64_t`
- No truncation when reading from v7
- Proper widening when reading from v6

---

## Validation Checklist

- [ ] All header files updated to `int64_t`
- [ ] All pack functions write 8 bytes
- [ ] All unpack functions read 8 bytes
- [ ] Legacy readers handle 4-byte values
- [ ] Migration code widens 32→64 bit
- [ ] Tests pass for all datetime operations
- [ ] v6→v7 migration preserves dates
- [ ] Year 2050 works correctly
- [ ] No compiler warnings
- [ ] No test failures

---

## Files Inventory

### Headers (6 files)
- [ ] `Common/headers/lang.h`
- [ ] `Common/headers/op.h`
- [ ] `Common/headers/tableformats.h`
- [ ] `Common/headers/wpengine.h`
- [ ] `Common/headers/pict.h`
- [ ] `Common/headers/file.h`

### Source - Modern Packers (estimate 3-5 files)
- [ ] `Common/source/oppack.c`
- [ ] `Common/source/tablepack.c`
- [ ] `Common/source/wpengine.c` (or wppack.c if exists)
- [ ] Others TBD

### Source - Legacy Readers (1-2 files)
- [ ] `Common/source/legacy/tablepack_legacy.c`
- [ ] Others TBD

### Tests (1-2 files)
- [ ] `tests/unit/test_datetime_fields.c` (new)
- [ ] `tests/db_format_tests.c` (update)

---

## Next Steps

1. Find and inventory all pack/unpack code for time fields
2. Update structure definitions in headers
3. Update modern packers to use 8-byte operations
4. Update legacy readers to widen 4→8 bytes
5. Write comprehensive tests
6. Run full test suite
7. Test with real v6 databases

---

**Status:** Plan complete, ready to begin implementation
