# Non-Scalar Type Audit for v7 Format Compliance
**Date**: 2025-12-05
**Status**: Audit Complete

## Summary

Audited all non-scalar types (Outlines, Tables, WPText, Pictures, Menus) for proper 64-bit timestamp handling in v7 database format.

## Results

### ✅ Outlines (tyoutlinerecord)
**Status**: COMPLIANT (implemented in PR #62)

- **In-memory**: `int64_t timecreated, timelastsave` (Common/headers/op.h:456)
- **Disk format**: v4 format with 64-bit timestamps
- **Packing**: `oppack_modern.c` uses `memtodisklonglong()` for big-endian 64-bit writes
- **Unpacking**: Auto-dispatches v2/v3 (legacy, 32-bit) vs v4 (modern, 64-bit)
- **Static assertions**: ✅ Verified 8-byte alignment
- **Testing**: ✅ Covered by oppack_tests.c

### ✅ WPText (tywprecord)
**Status**: COMPLIANT

- **In-memory**: `int64_t timecreated, timelastsave` (Common/headers/wpengine.h:113)
- **Disk format**: tywpheader with int64_t timestamps (Common/source/wpengine.c:118)
- **Padding**: `unsigned char _pad[6]` for 8-byte alignment (line 116)
- **Packing**: `wppackheader()` uses `conditionallonglongswap()` (wpengine.c:2132-2134)
- **Static assertions**: ✅ Verified in wpengine.h:176
- **Reserved space**: 52 bytes ("waste") available for future expansion

**Code Reference**:
```c
// wpengine.c:2132-2134
header.timecreated = conditionallonglongswap ((**hwp).timecreated);
header.timelastsave = conditionallonglongswap ((**hwp).timelastsave);
```

### ✅ Pictures (typictrecord)  
**Status**: COMPLIANT

- **In-memory**: `int64_t timecreated, timelastsave` (Common/headers/pict.h:42)
- **Disk format**: tydiskpictrecord with int64_t timestamps
- **Packing**: `pictpack()` uses `conditionallonglongswap()` (pict.c:164-166)
- **Static assertions**: ✅ Verified in pict.h:68
- **Reserved space**: windowrect field available

**Code Reference**:
```c
// pict.c:164-166
header.timecreated = conditionallonglongswap ((**hp).timecreated);
header.timelastsave = conditionallonglongswap ((**hp).timelastsave);
```

### ❌ Tables (tyhashtable)
**Status**: NON-COMPLIANT - **CRITICAL ISSUE**

- **In-memory**: `int64_t timecreated, timelastsave` (Common/headers/lang.h:506) ✅
- **Disk format**: tydisktablerecord with `uint32_t` timestamps ❌
- **Problem**: langhash.c:2978-2980 writes 32-bit timestamps despite 64-bit in-memory values
- **Impact**: Tables will FAIL for dates after February 6, 2040

**Broken Code**:
```c
// langhash.c:2978-2980 - WRONG!
header.timecreated = (uint32_t) host_to_disk_int32((int32_t) (**htable).timecreated);
header.timelastsave = (uint32_t) host_to_disk_int32((int32_t) (**htable).timelastsave);
```

**Solution**: Detailed planning doc exists at `planning/phase3/table_timestamp_fix.md`
- Create v0x05 disk format with 64-bit timestamps
- Fork pack/unpack into legacy (v0x04, 32-bit) and modern (v0x05, 64-bit)  
- Follow pattern from oppack_legacy.c / oppack_modern.c split

### ✅ Menus (tymenurecord)
**Status**: COMPLIANT (delegates to outlines)

- **Implementation**: Menus are stored as outlines internally
- **Delegation**: menuverbpack → opverbpack → oppack
- **Compliance**: Inherits all outline v4 64-bit timestamp handling
- **No action needed**: Already compliant via outline infrastructure

## Compliance Matrix

| Type     | In-Memory | Disk Write | Disk Read | Static Assert | Status |
|----------|-----------|------------|-----------|---------------|--------|
| Outlines | int64_t ✅ | 64-bit ✅   | 32→64 ✅   | Yes ✅         | ✅ PASS |
| WPText   | int64_t ✅ | 64-bit ✅   | 64-bit ✅  | Yes ✅         | ✅ PASS |
| Pictures | int64_t ✅ | 64-bit ✅   | 64-bit ✅  | Yes ✅         | ✅ PASS |
| Tables   | int64_t ✅ | **32-bit ❌** | 32-bit ❌  | Yes ✅         | ❌ FAIL |
| Menus    | (outline) | (outline)  | (outline) | (outline)     | ✅ PASS |

## Action Items

1. **CRITICAL**: Implement table timestamp fix per planning/phase3/table_timestamp_fix.md
   - Priority: HIGH (blocks v7 format for tables)
   - Estimated effort: Medium (follow established oppack fork pattern)
   - Dependencies: None (pattern already proven with outlines)

2. **Testing**: Add table round-trip tests for 2040+ timestamps
   - Create table_timestamp_tests.c
   - Verify v0x04 → v0x05 migration
   - Test boundary values (2^32, 2040, 2050)

## References

- PR #62: 64-bit datetime fields (outlines, wptext, pictures)
- planning/phase2/64bit_datetime_fields_plan.md
- planning/phase3/table_timestamp_fix.md
- planning/phase3/oppack_fork_WIP.md
