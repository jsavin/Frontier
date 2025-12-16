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

### ✅ Tables (tyhashtable)
**Status**: COMPLIANT (Fixed in PR #63)

- **In-memory**: `int64_t timecreated, timelastsave` (Common/headers/lang.h:506) ✅
- **Disk format**: tydisktablerecord_v4 (legacy, 32-bit) and tydisktablerecord (v0x05, 64-bit) ✅
- **Packing**: langhash.c uses `db_format_write_be64()` for v0x05 format (lines 3731-3732) ✅
- **Unpacking**: Auto-dispatches v0x04 (legacy, 32-bit) vs v0x05 (modern, 64-bit) (lines 3941-3959) ✅
- **Migration**: v0x04 tables upgraded to v0x05 on save to v7 format ✅

**Implementation Details**:
- v0x05 struct has proper 8-byte alignment with `uint32_t _pad` field
- Version dispatch checks header version field before reading appropriate struct
- Legacy v0x04 timestamps are widened to 64-bit on read
- Modern v0x05 timestamps are read as native 64-bit big-endian

**Completed in**: PR #63

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
| Tables   | int64_t ✅ | 64-bit ✅   | 32→64 ✅   | Yes ✅         | ✅ PASS |
| Menus    | (outline) | (outline)  | (outline) | (outline)     | ✅ PASS |

## Action Items

✅ **COMPLETE**: All non-scalar types now fully compliant with v7 64-bit timestamp format!

**Completed Item**:
- ✅ Table timestamp fix implemented in PR #63
- ✅ v0x04 → v0x05 migration implemented and tested (save_migration_tests.c)
- ✅ Struct sizes verified with static assertions (16 bytes v0x04, 32 bytes v0x05)
- ✅ Big-endian format verified (db_format_write_be64 / db_format_read_be64)

## References

- PR #62: 64-bit datetime fields (outlines, wptext, pictures)
- planning/phase2/64bit_datetime_fields_plan.md
- planning/phase3/table_timestamp_fix.md
- planning/phase3/oppack_fork_WIP.md
