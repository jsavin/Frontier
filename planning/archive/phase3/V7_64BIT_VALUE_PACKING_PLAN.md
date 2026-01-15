# v7 64-Bit Value Packing Implementation Plan

**Date**: 2026-01-08
**Owner**: System Architect
**Status**: DRAFT - Pending User Approval
**Priority**: LAUNCH-BLOCKING
**Related Analysis**: `planning/phase3/LONG_VALUE_PACKING_DATA_LOSS_ANALYSIS.md`

---

## Overview

### Problem Statement

The current implementation in `langpackvalue()` has a critical data integrity bug:
- **Packing**: Writes 4 bytes via `db_format_write_be32()`, but copies 8 bytes via `langpackdata(sizeof(val.data.longvalue), ...)`
- **Result**: 4 bytes of valid data + 4 bytes of garbage (uninitialized union memory)
- **Impact**: Non-deterministic packing, potential data loss for values > 2^32, migration integrity issues

### User Requirements (from task context)

1. **"We don't need v6 packing at all. This code will never save or create v6 databases."**
2. **"We have to be 100% 64-bit compatible. That's a foundational principle."**
3. **"We need Option 2."** (Store 64-bit in v7 format)
4. **"Think about if we ever store a date/time value in a refcon. Those will all break in 2038!"**
5. **No database version bump needed** - v7 is not in production yet

### Strategic Decision: Full 64-bit Implementation

Based on user requirements, we are implementing **Option 2** from the analysis:
- **longvaluetype**: Store 64-bit (8 bytes) in big-endian format
- **datevaluetype**: Store 64-bit (8 bytes) in big-endian format (Year 2038 fix)
- **ostypevaluetype**: Keep 32-bit (4 bytes) - OSTypes are always 4-character codes
- **enumvaluetype**: Store 64-bit (8 bytes) - same as longvaluetype
- **fixedvaluetype**: Store 64-bit (8 bytes) - same as longvaluetype

**Note**: While `fixedvaluetype` in UserTalk is traditionally 32-bit (16.16 fixed-point), storing it as 64-bit provides future extensibility and eliminates special-casing. The runtime can still interpret it as 32-bit Fixed when needed.

---

## Phase 1: In-Memory Structure Verification

### Objective
Verify that all in-memory structures already use 64-bit storage for long/date values.

### Files to Check

#### 1. `Common/headers/lang.h`

**Lines 306-365**: `tyvaluedata` union definition

```c
typedef union tyvaluedata {
    boolean flvalue;
    byte chvalue;
    int64_t intvalue;       // ✅ Already 64-bit
    int64_t longvalue;      // ✅ Already 64-bit
    int64_t datevalue;      // ✅ Already 64-bit
    tydirection dirvalue;
    OSType ostypevalue;     // ✅ Correctly 32-bit (OSType is 4-byte code)
    // ... other fields
    Fixed fixedvalue;       // ⚠️ 32-bit (but stored in longvalue union field)
    OSType enumvalue;       // ⚠️ 32-bit (but stored in longvalue union field)
    // ...
} tyvaluedata;
```

**Verification checklist**:
- [x] `longvalue` is `int64_t` (line 314)
- [x] `datevalue` is `int64_t` (line 316)
- [x] `ostypevalue` is `OSType` (32-bit) (line 320)
- [ ] Verify `fixedvalue` and `enumvalue` handling

**Key insight**: `fixedvalue` and `enumvalue` are separate fields in the union, but when packed, they share the `longvalue` field's storage. This is why the switch statement groups them together.

#### 2. Outline Refcon Storage

**File**: `Common/headers/opinternal.h`

**Check**: `hdlheadrecord.hrefcon` - should be `Handle` (can store arbitrary binary data)

```c
typedef struct tyheadrecord {
    // ...
    Handle hrefcon;         // ✅ Can store any size packed value
    // ...
} tyheadrecord;
```

**Refcon usage** (from `Common/source/oprefcon.c`):
- `opsetrefcon_ctx()`: Allocates handle of exact size, no size constraints
- `opgetrefcon()`: Returns handle as-is
- **Result**: Refcons can store 64-bit packed values without modification

### Action Items

1. ✅ **Verified**: `longvalue`, `datevalue` are 64-bit in memory
2. ✅ **Verified**: `ostypevalue` is correctly 32-bit
3. 🔲 **Clarify**: `fixedvalue` and `enumvalue` storage (both are 32-bit types but stored in union)
4. 🔲 **Document**: Union field aliasing behavior for future maintainers

---

## Phase 2: Packing Changes (Common/source/langpack.c)

### Objective
Update `langpackvalue()` to store full 64-bit values for long/date types, keep 32-bit for OSType.

### File: `Common/source/langpack.c`

#### Change 1: Long Value Types (Lines 166-174)

**Current (BUGGY)**:
```c
case longvaluetype:
case ostypevaluetype:
case enumvaluetype:
case fixedvaluetype:
    db_format_write_be32(&val.data.longvalue, (uint32_t) val.data.longvalue);  // ❌ Writes 4 bytes

    fl = langpackdata (sizeof (val.data.longvalue), &val.data.longvalue, hpackedvalue);  // Copies 8 bytes!

    break;
```

**Proposed (FIXED)**:
```c
case longvaluetype:
case enumvaluetype:
case fixedvaluetype:
    {
        // Pack full 64-bit value in big-endian format
        uint64_t val64 = (uint64_t) val.data.longvalue;
        db_format_write_be64(&val64, val64);
        fl = langpackdata(sizeof(uint64_t), &val64, hpackedvalue);  // Pack 8 bytes
    }
    break;

case ostypevaluetype:
    {
        // OSType is always 32-bit (4-character code)
        uint32_t val32 = (uint32_t) val.data.ostypevalue;
        db_format_write_be32(&val32, val32);
        fl = langpackdata(sizeof(uint32_t), &val32, hpackedvalue);  // Pack 4 bytes
    }
    break;
```

**Rationale**:
- **Separate OSType**: OSTypes must remain 32-bit (API contract)
- **64-bit for long/enum/fixed**: Future-proof, eliminates garbage bytes
- **Temporary variables**: Avoids union aliasing issues, ensures clean packing

#### Change 2: Date Value Type (Lines 184-189)

**Current (BUGGY)**:
```c
case datevaluetype:
    db_format_write_be32(&val.data.longvalue, (uint32_t) val.data.longvalue);  // ❌ Writes 4 bytes

    fl = langpackdata (sizeof (val.data.datevalue), &val.data.datevalue, hpackedvalue);  // Copies 8 bytes!

    break;
```

**Proposed (FIXED - Year 2038 compliant)**:
```c
case datevaluetype:
    {
        // Pack full 64-bit timestamp in big-endian format (Year 2038 fix)
        int64_t date64 = val.data.datevalue;
        db_format_write_be64(&date64, (uint64_t) date64);
        fl = langpackdata(sizeof(int64_t), &date64, hpackedvalue);  // Pack 8 bytes
    }
    break;
```

**Rationale**:
- **Year 2038 fix**: Dates after 2038 require >32 bits
- **Refcon safety**: Date values stored in refcons won't overflow in 2038
- **Migration**: Timestamps from v6 (32-bit) are widened during migration, will round-trip correctly

### Implementation Notes

1. **Use temporary variables**: Don't write directly to union fields to avoid aliasing issues
2. **Explicit sizing**: Use `sizeof(uint64_t)` and `sizeof(uint32_t)` for clarity
3. **Type safety**: Cast through appropriate unsigned types for BE conversion
4. **Consistent pattern**: All 64-bit packs use same structure (allocate temp, write BE, pack)

---

## Phase 3: Unpacking Changes (Common/source/langpack.c)

### Objective
Update `langunpackvalue()` to read correct number of bytes and handle endianness properly.

### File: `Common/source/langpack.c`

#### Change 1: Long Value Types (Lines 558-565)

**Current (BUGGY)**:
```c
case longvaluetype:
case ostypevaluetype:
case enumvaluetype:
case fixedvaluetype:
    fl = langunpackdata (sizeof (v.data.longvalue), &v.data.longvalue, h, &ixunpack);  // Reads 8 bytes!

    disktomemlong (v.data.longvalue);  // ⚠️ Only swaps 4 bytes (macro is 32-bit)
    break;
```

**Proposed (FIXED)**:
```c
case longvaluetype:
case enumvaluetype:
case fixedvaluetype:
    {
        // Unpack 64-bit value from big-endian format
        uint64_t val64;
        fl = langunpackdata(sizeof(uint64_t), &val64, h, &ixunpack);
        if (fl) {
            v.data.longvalue = (int64_t) db_format_read_be64((unsigned char*)&val64);
        }
    }
    break;

case ostypevaluetype:
    {
        // OSType is always 32-bit
        uint32_t val32;
        fl = langunpackdata(sizeof(uint32_t), &val32, h, &ixunpack);
        if (fl) {
            v.data.ostypevalue = (OSType) db_format_read_be32((unsigned char*)&val32);
        }
    }
    break;
```

**Rationale**:
- **64-bit unpack**: Matches 64-bit pack (full round-trip)
- **BE64 conversion**: Use `db_format_read_be64()` for correct endianness
- **OSType separation**: Maintains 32-bit API contract

#### Change 2: Date Value Type (Lines 574-578)

**Current (BUGGY)**:
```c
case datevaluetype:
    fl = langunpackdata (sizeof (v.data.datevalue), &v.data.datevalue, h, &ixunpack);  // Reads 8 bytes!

    disktomemlong (v.data.datevalue);  // ⚠️ Only swaps 4 bytes
    break;
```

**Proposed (FIXED)**:
```c
case datevaluetype:
    {
        // Unpack 64-bit timestamp from big-endian format
        int64_t date64;
        fl = langunpackdata(sizeof(int64_t), &date64, h, &ixunpack);
        if (fl) {
            v.data.datevalue = (int64_t) db_format_read_be64((unsigned char*)&date64);
        }
    }
    break;
```

**Rationale**:
- **Year 2038 compliance**: Dates after 2038 unpack correctly
- **Migration safety**: v6 dates (32-bit) were widened during migration, will unpack correctly

### Endianness Macro Analysis

**From `Common/headers/byteorder.h`**:

```c
#define disktomemlong(x)  longswap(x)      // 32-bit swap
#define disktomemlonglong(x)  longlongswap(x)  // 64-bit swap
```

**Issue**: `disktomemlong()` is 32-bit only, but we're reading 8 bytes into a 64-bit field.

**Solution**: Use `db_format_read_be64()` which correctly reads 8 bytes and converts from big-endian.

---

## Phase 4: Migration Compatibility

### Objective
Ensure v6 → v7 migration works correctly with new 64-bit packing.

### v6 Format (Legacy)

**v6 databases store**:
- Long values: 4 bytes (32-bit)
- Dates: 4 bytes (32-bit unsigned, seconds since 1904)
- OSTypes: 4 bytes (correct)

### Migration Path

**Current migration code** (in `Common/source/db_format.c` and related):
1. Database headers widened (32-bit addresses → 64-bit)
2. Table headers migrated (v4/v5 format transition)
3. **Scalar values**: Unpacked from v6, expanded to 64-bit in memory

**What needs to happen**:
1. **v6 unpacking**: Legacy adapter reads 4-byte values
2. **In-memory widening**: Values stored as 64-bit in `tyvaluerecord`
3. **v7 packing**: New code packs 64-bit values

### Key Insight: Migration Already Works

**Why**: The migration unpacking is format-aware:

```c
// In legacy reader (used during migration):
// Reads 4 bytes for v6 long values
fl = langunpackdata(sizeof(uint32_t), &val32, h, &ixunpack);
v.data.longvalue = (int64_t) val32;  // Widen to 64-bit
```

**Then**: When re-packing to v7 database:
```c
// New v7 packing code:
uint64_t val64 = (uint64_t) v.data.longvalue;  // Full 64-bit value
db_format_write_be64(&val64, val64);
fl = langpackdata(sizeof(uint64_t), &val64, hpackedvalue);
```

### Migration Code Review Points

**Files to audit**:
1. `Common/source/tableverbpack.c` - External table packing during migration
2. `Common/source/langexternal.c` - External value serialization
3. `Common/source/oppack.c` - Outline serialization (refcons)

**What to verify**:
- [ ] Refcon unpacking from v6 reads correct byte count
- [ ] Table value unpacking from v6 widens scalars correctly
- [ ] Re-packing to v7 uses new 64-bit code paths

---

## Phase 5: Testing Strategy

### Unit Tests (tests/components/)

#### Test 1: Pack/Unpack Round-Trip

**File**: `tests/components/test_langpack_64bit.c` (NEW)

```c
#include "frontier.h"
#include "standard.h"
#include "lang.h"
#include "langinternal.h"
#include <assert.h>
#include <stdio.h>

// Test: Long value round-trip with edge cases
void test_long_pack_unpack_roundtrip(void) {
    tyvaluerecord val_in, val_out;
    Handle hpacked = nil;
    boolean ok;

    // Test case 1: Small positive value
    setlongvalue(42, &val_in);
    ok = langpackvalue(val_in, &hpacked, HNoNode);
    assert(ok && "Pack failed for value 42");
    ok = langunpackvalue(hpacked, &val_out);
    assert(ok && "Unpack failed for value 42");
    assert(val_out.data.longvalue == 42 && "Round-trip failed for 42");
    disposehandle(hpacked);

    // Test case 2: Maximum 32-bit signed value
    setlongvalue(0x7FFFFFFF, &val_in);
    ok = langpackvalue(val_in, &hpacked, HNoNode);
    assert(ok);
    ok = langunpackvalue(hpacked, &val_out);
    assert(ok);
    assert(val_out.data.longvalue == 0x7FFFFFFF && "Round-trip failed for INT32_MAX");
    disposehandle(hpacked);

    // Test case 3: Negative value
    setlongvalue(-1, &val_in);
    ok = langpackvalue(val_in, &hpacked, HNoNode);
    assert(ok);
    ok = langunpackvalue(hpacked, &val_out);
    assert(ok);
    assert(val_out.data.longvalue == -1 && "Round-trip failed for -1");
    disposehandle(hpacked);

    // Test case 4: Zero
    setlongvalue(0, &val_in);
    ok = langpackvalue(val_in, &hpacked, HNoNode);
    assert(ok);
    ok = langunpackvalue(hpacked, &val_out);
    assert(ok);
    assert(val_out.data.longvalue == 0 && "Round-trip failed for 0");
    disposehandle(hpacked);

    // Test case 5: Value > 2^32 (requires 64-bit)
    setlongvalue(4294967296LL, &val_in);  // 2^32
    ok = langpackvalue(val_in, &hpacked, HNoNode);
    assert(ok);
    ok = langunpackvalue(hpacked, &val_out);
    assert(ok);
    assert(val_out.data.longvalue == 4294967296LL && "Round-trip failed for 2^32");
    disposehandle(hpacked);

    // Test case 6: Large negative value
    setlongvalue(-4294967296LL, &val_in);  // -2^32
    ok = langpackvalue(val_in, &hpacked, HNoNode);
    assert(ok);
    ok = langunpackvalue(hpacked, &val_out);
    assert(ok);
    assert(val_out.data.longvalue == -4294967296LL && "Round-trip failed for -2^32");
    disposehandle(hpacked);

    printf("✅ Long value pack/unpack round-trip: ALL TESTS PASSED\n");
}

// Test: Date value round-trip (Year 2038 compliance)
void test_date_pack_unpack_roundtrip(void) {
    tyvaluerecord val_in, val_out;
    Handle hpacked = nil;
    boolean ok;

    // Test case 1: Date in 1904 (Frontier epoch)
    setdatevalue(0, &val_in);
    ok = langpackvalue(val_in, &hpacked, HNoNode);
    assert(ok);
    ok = langunpackvalue(hpacked, &val_out);
    assert(ok);
    assert(val_out.data.datevalue == 0 && "Round-trip failed for epoch");
    disposehandle(hpacked);

    // Test case 2: Date in 2038 (32-bit overflow point)
    // Seconds from 1904-01-01 to 2038-01-19
    int64_t date_2038 = 4233513600LL;  // Approximate
    setdatevalue(date_2038, &val_in);
    ok = langpackvalue(val_in, &hpacked, HNoNode);
    assert(ok);
    ok = langunpackvalue(hpacked, &val_out);
    assert(ok);
    assert(val_out.data.datevalue == date_2038 && "Round-trip failed for 2038 date");
    disposehandle(hpacked);

    // Test case 3: Date in 2100 (requires >32 bits)
    // Seconds from 1904-01-01 to 2100-01-01
    int64_t date_2100 = 6187382400LL;  // Approximate
    setdatevalue(date_2100, &val_in);
    ok = langpackvalue(val_in, &hpacked, HNoNode);
    assert(ok);
    ok = langunpackvalue(hpacked, &val_out);
    assert(ok);
    assert(val_out.data.datevalue == date_2100 && "Round-trip failed for 2100 date");
    disposehandle(hpacked);

    printf("✅ Date value pack/unpack round-trip: ALL TESTS PASSED\n");
}

// Test: OSType remains 32-bit
void test_ostype_pack_unpack_roundtrip(void) {
    tyvaluerecord val_in, val_out;
    Handle hpacked = nil;
    boolean ok;

    // OSType: 4-character code
    setostypevalue('TEXT', &val_in);
    ok = langpackvalue(val_in, &hpacked, HNoNode);
    assert(ok);

    // Verify packed size is 4 bytes (OSType header + 4 bytes data)
    long packed_size = gethandlesize(hpacked);
    assert(packed_size == (sizeof(typackedvalue) + 4) && "OSType should pack to 4 bytes");

    ok = langunpackvalue(hpacked, &val_out);
    assert(ok);
    assert(val_out.data.ostypevalue == 'TEXT' && "Round-trip failed for OSType");
    disposehandle(hpacked);

    printf("✅ OSType pack/unpack round-trip: ALL TESTS PASSED\n");
}

// Test: Packed size verification
void test_packed_size_verification(void) {
    tyvaluerecord val;
    Handle hpacked = nil;
    long expected_size;

    // Long value: should be 8 bytes data + header
    setlongvalue(42, &val);
    langpackvalue(val, &hpacked, HNoNode);
    expected_size = sizeof(typackedvalue) + 8;
    assert(gethandlesize(hpacked) == expected_size && "Long should pack to 8 bytes");
    disposehandle(hpacked);

    // Date value: should be 8 bytes data + header
    setdatevalue(0, &val);
    langpackvalue(val, &hpacked, HNoNode);
    expected_size = sizeof(typackedvalue) + 8;
    assert(gethandlesize(hpacked) == expected_size && "Date should pack to 8 bytes");
    disposehandle(hpacked);

    // OSType: should be 4 bytes data + header
    setostypevalue('TEXT', &val);
    langpackvalue(val, &hpacked, HNoNode);
    expected_size = sizeof(typackedvalue) + 4;
    assert(gethandlesize(hpacked) == expected_size && "OSType should pack to 4 bytes");
    disposehandle(hpacked);

    printf("✅ Packed size verification: ALL TESTS PASSED\n");
}

// Main test runner
int main(void) {
    initlang();  // Initialize language runtime

    test_long_pack_unpack_roundtrip();
    test_date_pack_unpack_roundtrip();
    test_ostype_pack_unpack_roundtrip();
    test_packed_size_verification();

    printf("\n✅✅✅ ALL 64-BIT PACKING TESTS PASSED ✅✅✅\n");
    return 0;
}
```

**Makefile integration**:
```makefile
# Add to tests/components/Makefile
test_langpack_64bit: test_langpack_64bit.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

headless_tests: ... test_langpack_64bit
	./test_langpack_64bit
```

#### Test 2: Refcon Storage

**File**: `tests/refcon_tests.c` (UPDATE EXISTING)

Add new test case:
```c
void test_refcon_large_long_storage(void) {
    hdlheadrecord hnode;
    tyvaluerecord val_in, val_out;
    Handle hpacked, hretrieved;
    boolean ok;

    // Create outline node
    ok = opnewheadrecord(&hnode);
    assert(ok);

    // Store large long value (> 2^32) in refcon
    int64_t large_value = 4294967296LL;  // 2^32
    setlongvalue(large_value, &val_in);

    // Pack value
    ok = langpackvalue(val_in, &hpacked, HNoNode);
    assert(ok);

    // Store in refcon
    ok = opsetrefcon(hnode, *hpacked, gethandlesize(hpacked));
    assert(ok);
    disposehandle(hpacked);

    // Retrieve from refcon
    hretrieved = (**hnode).hrefcon;
    assert(hretrieved != nil);

    // Unpack and verify
    ok = langunpackvalue(hretrieved, &val_out);
    assert(ok);
    assert(val_out.data.longvalue == large_value && "Refcon lost high-order bits!");

    printf("✅ Refcon large long storage: PASSED\n");
}
```

### Integration Tests (tests/integration/)

#### Test 3: Migration with Long Values

**File**: `tests/integration/migration_longvalue_test.yaml` (NEW)

```yaml
# Integration test: v6→v7 migration preserves long values
suite: migration_64bit_values
setup:
  - name: "Create v6 database with long values"
    # This would require a pre-built v6 test database
    # Or manual creation step

tests:
  - name: "Migrate v6 database with table containing long values"
    # Assumes v6 test database exists at tests/fixtures/v6_with_longs.root
    migrate:
      source: "tests/fixtures/v6_with_longs.root"
      destination: "tests/tmp/migrated_longs_v7.root"
    verify:
      - script: |
          local (ht);
          file.openDatabase("tests/tmp/migrated_longs_v7.root", @ht);
          return ht^.testTable.largeValue == 1000000
        expected: true
        description: "Large long value preserved during migration"

  - name: "Migrate v6 outline with refcon containing long"
    migrate:
      source: "tests/fixtures/v6_outline_refcon.root"
      destination: "tests/tmp/migrated_outline_v7.root"
    verify:
      - script: |
          local (ht, val);
          file.openDatabase("tests/tmp/migrated_outline_v7.root", @ht);
          op.attributes.get(@ht^.testOutline^.testNode, "refconValue", @val);
          return val == 1000000
        expected: true
        description: "Refcon long value preserved during migration"
```

#### Test 4: Post-2038 Date Storage

**File**: `tests/integration/date_2038_test.yaml` (NEW)

```yaml
# Integration test: Year 2038 compliance
suite: year_2038_compliance
tests:
  - name: "Store and retrieve post-2038 date"
    script: |
      local (testDate = date.set(2050, 1, 1, 0, 0, 0));
      system.temp.testDate = testDate;
      file.save();
      file.close();
      file.open();
      return system.temp.testDate == date.set(2050, 1, 1, 0, 0, 0)
    expected: true
    description: "Post-2038 date round-trips correctly"

  - name: "Store post-2038 date in refcon"
    script: |
      local (ot = @system.temp.testOutline);
      op.insert(ot, down);
      local (futureDate = date.set(2100, 12, 31, 23, 59, 59));
      op.attributes.set(ot, "timestamp", futureDate);
      file.save();
      file.close();
      file.open();
      local (retrievedDate);
      op.attributes.get(@system.temp.testOutline, "timestamp", @retrievedDate);
      return retrievedDate == futureDate
    expected: true
    description: "Post-2038 date in refcon survives save/load cycle"
```

### Manual Testing Checklist

#### Scenario 1: Create v7 Database with Large Values

```bash
# Terminal test
./frontier-cli/frontier-cli --system-root databases/Frontier.root7 -e $'
local (ht);
new(tableType, @ht);
ht.smallValue = 42;
ht.largeValue = 4294967296;  // 2^32
ht.futureDate = date.set(2100, 1, 1, 0, 0, 0);
pack(@ht, @system.temp.packedTable);
unpack(@system.temp.packedTable, @system.temp.unpackedTable);
msg("Small: " + system.temp.unpackedTable.smallValue);
msg("Large: " + system.temp.unpackedTable.largeValue);
msg("Date: " + system.temp.unpackedTable.futureDate);
return system.temp.unpackedTable.largeValue == 4294967296
'
```

**Expected output**:
```
Small: 42
Large: 4294967296
Date: <date representation for 2100-01-01>
true
```

#### Scenario 2: Migration Test

```bash
# Migrate v6 database
rm -f databases/Frontier-v7-migrated.root
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "1"
# Should create migrated v7 database

# Verify long values preserved
./frontier-cli/frontier-cli --system-root databases/Frontier-v7-migrated.root -e $'
if defined(system.stats) {
  msg("Stats table exists");
  if defined(system.stats.hitcount) {
    return system.stats.hitcount
  }
};
return "undefined"
'
```

---

## Phase 6: Risk Assessment and Mitigation

### Risk 1: Breaking Existing v7 Databases

**Risk Level**: MEDIUM (v7 not in production, but developer databases exist)

**Issue**: Existing v7 databases pack long values as 4 bytes (with 4 garbage bytes). New code expects 8 bytes.

**Impact**:
- Old v7 databases: Unpacking reads 8 bytes, but only first 4 are valid
- New code: Reads 8 bytes, interprets correctly
- **Result**: May work by accident (if garbage bytes are zero) or fail unpredictably

**Mitigation**:
1. **Option A (Recommended)**: Declare v7 format unstable, require re-migration from v6
   - Add version sub-field to v7 header (e.g., v7.0 → v7.1)
   - Detect old v7.0 databases, reject with error message
   - Error message: "This v7.0 database format is obsolete. Please re-migrate from v6 source."

2. **Option B**: Auto-detect and convert
   - During unpack, check packed size
   - If size == 4, assume old format, widen to 64-bit
   - If size == 8, assume new format
   - **Con**: Complex, fragile, hard to test all edge cases

**Recommendation**: Option A (clean break, v7 not in production)

### Risk 2: OSType Confusion

**Risk Level**: LOW

**Issue**: Developers may confuse `ostypevaluetype` with `longvaluetype` since they were previously grouped.

**Impact**:
- If developer tries to store large value in OSType, truncation will occur
- OSType API expects 4-byte codes, not arbitrary 64-bit values

**Mitigation**:
1. Add runtime assertion in `setostypevalue()`:
   ```c
   boolean setostypevalue(OSType val, tyvaluerecord *v) {
       assert((val & 0xFFFFFFFF00000000LL) == 0 && "OSType must be 32-bit");
       // ...
   }
   ```
2. Document OSType limitation in code comments
3. Add test case verifying OSType packs to 4 bytes

### Risk 3: Migration Data Corruption

**Risk Level**: HIGH (if migration code not updated)

**Issue**: Migration unpacks v6 values (4 bytes), but re-packs using new code (8 bytes). If migration unpack code reads wrong size, corruption occurs.

**Impact**:
- v6 long values may be truncated or garbage-extended during migration
- Refcons with dates may lose post-2038 capability

**Mitigation**:
1. **Audit migration code paths**:
   - Verify `tableverbpack.c` uses format-aware unpacking
   - Check `langexternal.c` external value serialization
   - Review `oppack.c` outline refcon handling

2. **Test migration thoroughly**:
   - Create v6 database with known long values
   - Migrate to v7
   - Verify exact values preserved (not just "close enough")

3. **Add migration regression tests**:
   - Commit v6 test database to git
   - Automate migration + verification in CI

### Risk 4: Fixed and Enum Type Semantics

**Risk Level**: LOW

**Issue**: `fixedvaluetype` is traditionally 32-bit (16.16 fixed-point). Storing as 64-bit may confuse developers.

**Impact**:
- Fixed-point math expects 32-bit values
- Storing as 64-bit may waste space or cause unexpected behavior

**Mitigation**:
1. **Document decision**: Fixed stored as 64-bit for consistency, interpreted as 32-bit at runtime
2. **Verify Fixed math**: Ensure `Fixed` operations (multiply, divide) still work
3. **Consider future**: If Fixed moves to 64-bit (32.32 fixed-point), storage already supports it

**Alternative**: Special-case Fixed to remain 32-bit (like OSType)
```c
case fixedvaluetype:
    {
        // Fixed is 32-bit (16.16 fixed-point)
        uint32_t val32 = (uint32_t) val.data.fixedvalue;
        db_format_write_be32(&val32, val32);
        fl = langpackdata(sizeof(uint32_t), &val32, hpackedvalue);
    }
    break;
```

**Decision needed**: User input required on Fixed storage size

---

## Phase 7: Implementation Checklist

### Step 1: Code Changes
- [ ] Update `langpackvalue()` in `Common/source/langpack.c`:
  - [ ] Separate `ostypevaluetype` case (keep 32-bit)
  - [ ] Update `longvaluetype` case (use `db_format_write_be64`, pack 8 bytes)
  - [ ] Update `enumvaluetype` case (use `db_format_write_be64`, pack 8 bytes)
  - [ ] Update `fixedvaluetype` case (decision needed: 32-bit or 64-bit?)
  - [ ] Update `datevaluetype` case (use `db_format_write_be64`, pack 8 bytes)

- [ ] Update `langunpackvalue()` in `Common/source/langpack.c`:
  - [ ] Separate `ostypevaluetype` case (read 4 bytes, use `db_format_read_be32`)
  - [ ] Update `longvaluetype` case (read 8 bytes, use `db_format_read_be64`)
  - [ ] Update `enumvaluetype` case (read 8 bytes, use `db_format_read_be64`)
  - [ ] Update `fixedvaluetype` case (match pack decision)
  - [ ] Update `datevaluetype` case (read 8 bytes, use `db_format_read_be64`)

### Step 2: Testing
- [ ] Write unit tests (`tests/components/test_langpack_64bit.c`)
- [ ] Update refcon tests (`tests/refcon_tests.c`)
- [ ] Create migration integration tests (`tests/integration/migration_longvalue_test.yaml`)
- [ ] Create Year 2038 compliance tests (`tests/integration/date_2038_test.yaml`)
- [ ] Run full test suite:
  - [ ] `./tools/run_headless_tests.sh` (unit tests)
  - [ ] `cd tests && make test-integration` (integration tests)

### Step 3: Migration Verification
- [ ] Audit migration code:
  - [ ] Review `Common/source/tableverbpack.c`
  - [ ] Review `Common/source/langexternal.c`
  - [ ] Review `Common/source/oppack.c`
- [ ] Test v6→v7 migration with known test data
- [ ] Verify refcon values preserved
- [ ] Verify table scalar values preserved

### Step 4: Documentation
- [ ] Update `docs/VERB_IMPLEMENTATION_GUIDE.md` with 64-bit value handling
- [ ] Add section to `docs/frontier_time_t_standard.md` about date packing
- [ ] Document OSType 32-bit limitation
- [ ] Add ADR (Architectural Decision Record) for 64-bit value packing

### Step 5: v7 Format Versioning (if needed)
- [ ] Add version sub-field to v7 header (v7.0 → v7.1)
- [ ] Add detection code for old v7.0 databases
- [ ] Add error message directing users to re-migrate from v6

---

## Open Questions

### Question 1: Fixed Storage Size

**Decision needed**: Should `fixedvaluetype` be stored as 32-bit (traditional) or 64-bit (consistent with long)?

**Option A: 32-bit (traditional)**
- **Pro**: Matches historical Fixed behavior (16.16 fixed-point)
- **Pro**: Saves 4 bytes per Fixed value
- **Con**: Inconsistent with other integer types
- **Con**: No room for future expansion (32.32 fixed-point)

**Option B: 64-bit (consistent)**
- **Pro**: Consistent with long/enum storage
- **Pro**: Future-proof if Fixed moves to 64-bit
- **Pro**: Simpler code (same path as long)
- **Con**: Wastes 4 bytes per Fixed value (if never used)

**Recommendation**: Option B (64-bit) for consistency and future-proofing, unless user has strong preference.

### Question 2: v7 Format Versioning Strategy

**Decision needed**: How to handle existing v7 databases (developer-only, not production)?

**Option A: Clean break (require re-migration)**
- **Pro**: Simple, no backward compatibility code
- **Pro**: Ensures all v7 databases use correct format
- **Con**: Developers must re-migrate (one-time cost)

**Option B: Auto-convert old v7 databases**
- **Pro**: Smoother developer experience
- **Con**: Complex detection and conversion logic
- **Con**: Ongoing maintenance burden

**Recommendation**: Option A (clean break) - v7 not in production, developer cost is low.

### Question 3: Migration Code Audit Scope

**Decision needed**: How deep should migration code audit go?

**Minimal audit**: Verify pack/unpack byte counts match
**Full audit**: Trace all value serialization paths, add assertions, test all edge cases

**Recommendation**: Full audit - migration is one-time operation, must be 100% correct.

---

## Timeline and Effort Estimate

### Phase 1: Code Changes (2-3 hours)
- Implement pack changes: 1 hour
- Implement unpack changes: 1 hour
- Code review and cleanup: 1 hour

### Phase 2: Unit Testing (3-4 hours)
- Write pack/unpack tests: 2 hours
- Write refcon tests: 1 hour
- Debug and iterate: 1 hour

### Phase 3: Integration Testing (2-3 hours)
- Create migration test fixtures: 1 hour
- Write integration tests: 1 hour
- Run and verify: 1 hour

### Phase 4: Migration Audit (2-4 hours)
- Audit migration code paths: 2 hours
- Add assertions and validation: 1 hour
- Test v6→v7 migration: 1 hour

### Phase 5: Documentation (1-2 hours)
- Update guides: 1 hour
- Write ADR: 1 hour

**Total estimated effort**: 10-16 hours (1.5-2 days)

---

## Success Criteria

### Must-Have (Launch-Blocking)
1. ✅ Long values round-trip correctly through pack/unpack
2. ✅ Date values round-trip correctly (including post-2038 dates)
3. ✅ OSType remains 32-bit (API contract maintained)
4. ✅ v6→v7 migration preserves all long/date values exactly
5. ✅ Refcon storage handles 64-bit values correctly
6. ✅ All unit tests pass
7. ✅ Integration tests pass

### Should-Have (Post-Launch OK)
1. ✅ Full migration code audit complete
2. ✅ ADR documenting 64-bit packing decision
3. ✅ v7 format versioning (if breaking change)

### Nice-to-Have (Future Work)
1. ⚪ Performance benchmarks (64-bit vs 32-bit storage)
2. ⚪ Cross-platform endianness testing (Intel, ARM, etc.)
3. ⚪ Fuzzing tests for pack/unpack robustness

---

## References

### Related Documents
- `planning/phase3/LONG_VALUE_PACKING_DATA_LOSS_ANALYSIS.md` - Root cause analysis
- `docs/frontier_time_t_standard.md` - 64-bit timestamp standard
- `docs/VERB_IMPLEMENTATION_GUIDE.md` - Verb implementation patterns
- `planning/phase3/datetime_handling_audit.md` - Timestamp migration findings

### Key Files
- `Common/source/langpack.c` - Pack/unpack implementation (lines 166-189, 558-578)
- `Common/headers/lang.h` - `tyvaluedata` union definition (lines 306-365)
- `Common/headers/db_format.h` - Big-endian helpers (lines 42-88)
- `Common/source/oprefcon.c` - Refcon storage (lines 58-97)
- `Common/source/tableverbpack.c` - External table packing (migration)
- `Common/headers/byteorder.h` - Endianness macros (legacy, prefer db_format helpers)

### Related Issues
- Issue #167: time_t portability (RESOLVED - timestamps are 64-bit)
- Issue #185: Database format corruption (RESOLVED - context guards established)
- Issue #199: Hash table lookup API null pointer (RESOLVED - use hashtablelookupnode)

---

## Conclusion

This implementation plan provides a comprehensive, step-by-step path to fixing the 64-bit value packing bug in v7. The recommended approach:

1. **Store 64-bit for long/date/enum/fixed** (user requirement: "100% 64-bit compatible")
2. **Keep OSType as 32-bit** (API contract)
3. **Fix Year 2038 problem** (user requirement: "think about refcon dates")
4. **Clean break for v7 format** (not in production, require re-migration)
5. **Comprehensive testing** (unit + integration + migration verification)

**This is a foundational architectural change that must be correct before launch.** The estimated effort is 1.5-2 days, which is acceptable for a launch-blocking data integrity fix.

**Next steps**: Review with user, get approval on open questions (Fixed storage size, v7 versioning), then begin implementation.
