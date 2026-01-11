# Long Value Packing Data Loss Analysis

**Date**: 2026-01-08
**Issue**: Potential 64-bit to 32-bit truncation in langpackvalue()
**Severity**: HIGH - Data integrity issue affecting v7 database format

---

## Executive Summary

**THE USER'S HUNCH IS ABSOLUTELY CORRECT.**

The current implementation in `langpackvalue()` writes only 32 bits of data but packs 64 bits (on 64-bit systems), resulting in:
1. **4 bytes of valid data** (written by `db_format_write_be32`)
2. **4 bytes of garbage** (uninitialized memory from the union)

While this is unlikely to cause problems in typical usage (most long values fit in 32 bits), it creates:
- **Theoretical data loss**: Values > 2^32 will be truncated
- **Non-deterministic packing**: Garbage bytes vary based on memory state
- **Migration risk**: v6→v7 migration may lose high-order bits

---

## Root Cause Analysis

### The Bug (langpack.c:166-174)

```c
case longvaluetype:
case ostypevaluetype:
case enumvaluetype:
case fixedvaluetype:
    db_format_write_be32(&val.data.longvalue, (uint32_t) val.data.longvalue);  // ⚠️ Writes 4 bytes

    fl = langpackdata (sizeof (val.data.longvalue), &val.data.longvalue, hpackedvalue);  // Packs 8 bytes!

    break;
```

### What Actually Happens

**On 64-bit systems** (current build):
```c
sizeof(val.data.longvalue) = 8 bytes  // Because longvalue is int64_t
```

**Step-by-step execution**:
1. `db_format_write_be32()` writes 4 bytes in big-endian format to first 4 bytes of `val.data.longvalue`
2. `langpackdata()` copies **8 bytes** starting at `&val.data.longvalue`
3. **Result**: 4 valid bytes + 4 garbage bytes get packed

**Example with value 42**:
```
Before db_format_write_be32:
val.data.longvalue = 0x0000000000002A (64-bit representation)

After db_format_write_be32:
val.data.longvalue = 0x0000002A???????? (4 bytes written, 4 bytes unknown)
                               ^^^^^^^^ garbage from union's previous contents

What gets packed:
[0x00, 0x00, 0x00, 0x2A, ??, ??, ??, ??]
 ^^^^^^^^^^^^^^^^^^^^^^^ valid BE32
                          ^^^^^^^^^^^ garbage
```

### The Union Issue

```c
typedef union tyvaluedata {
    boolean flvalue;
    byte chvalue;
    int64_t intvalue;
    int64_t longvalue;      // ⚠️ 8 bytes on 64-bit
    int64_t datevalue;
    // ... other fields share same memory
} tyvaluedata;
```

**Key insight**: When you write 4 bytes to `longvalue`, the other 4 bytes retain whatever was previously in that memory location (from another union member or uninitialized data).

---

## Scope Assessment

### Where is langpackvalue() Used?

1. **Outline refcons** (oprefcon.c)
   - `opattributesgetpackedtablevalue()` calls `langunpackvalue(hrefcon, &linkedval)`
   - Refcons store packed values including longs
   - **AFFECTED**: Long values in refcons may have garbage

2. **Table values** (tablepack.c, via langexternal.c)
   - External table packing calls `langpackvalue()` for table entries
   - **AFFECTED**: Long values stored in ODB tables

3. **Script parameters and local variables**
   - Function calls, return values, temporary storage
   - **LESS CRITICAL**: In-memory only, not persisted

4. **Database serialization**
   - When tables with long values are saved
   - **CRITICAL**: Persistence layer affected

### Does This Affect frontier_time_t?

**NO** - timestamps are NOT affected by this bug:
- `datevaluetype` uses same buggy pack/unpack code (lines 184-189, 574-578)
- **HOWEVER**: Frontier timestamps are stored as **seconds since 1904**, which fit comfortably in 32 bits until year ~2040
- **Migration**: v6 timestamps were 32-bit `uint32_t`, expanded to 64-bit during migration
- **Result**: Timestamp migration is safe, but the packing bug still exists

---

## Migration Impact Assessment

### Current Migration Code

Migration does NOT directly use `langpackvalue()` for scalar expansion. Instead:

1. **Database addresses widened**: 32-bit → 64-bit (in `tableverbpack_internal`)
2. **Table headers migrated**: v4/v5 format transition
3. **Refcon unpacking**: Uses `langunpackvalue()` which reads garbage bytes

### Refcon Migration Path

**Scenario**: Outline with refcon containing long value

**v6 database**:
- Refcon packed with legacy 32-bit format
- Long value: 4 bytes (correct for v6)

**During migration**:
1. Unpack refcon using `langunpackvalue()`
2. If value is `longvaluetype`, reads `sizeof(v.data.longvalue)` bytes
3. On 64-bit system, reads 8 bytes (4 data + 4 garbage)
4. `disktomemlong()` swaps endianness
5. Re-pack for v7 using `langpackvalue()`
6. **Result**: Original 32-bit value preserved, but garbage may be introduced

### Table Value Migration

**Tables with long values**:
- Hash table unpacking reads scalar values from packed format
- Long values unpacked using `langunpackdata(sizeof(v.data.longvalue), ...)`
- **On 64-bit**: Reads 8 bytes even if only 4 were valid
- **Result**: May interpret garbage as high-order bits

---

## Theoretical Data Loss Scenarios

### Scenario 1: Large Integer Values

**User code**:
```usertalk
local (x = 4294967296)  /* 2^32, requires >32 bits */
table.value = x
pack(table)
```

**What happens**:
1. Value stored in-memory as 64-bit: `0x0000000100000000`
2. Pack truncates to 32-bit: `0x00000000`
3. **DATA LOST**: Value becomes 0 after pack/unpack

**Likelihood**: LOW - UserTalk rarely uses values > 2^31

### Scenario 2: Refcon with Computed Values

**User code**:
```usertalk
op.attributes.set(@x, "id", clock.ticks())  /* Mac ticks since boot */
```

**What happens**:
1. `clock.ticks()` returns 64-bit value
2. Packed to refcon (4 valid bytes + 4 garbage)
3. **NON-DETERMINISTIC**: Unpacking reads garbage as high bits

**Likelihood**: LOW - Most refcon values are small integers or strings

### Scenario 3: Migration Data Corruption

**v6 database with table**:
```
system.stats.hitcount = 1000000
```

**Migration**:
1. Unpack v6 value (4 bytes): `0x000F4240`
2. On 64-bit unpack, reads 8 bytes: `0x000F4240????????`
3. Re-pack for v7: garbage preserved
4. **Result**: Value may become corrupted if garbage is non-zero

**Likelihood**: MEDIUM - Depends on memory state during migration

---

## Unpacking Symmetry

### Unpack Code (langpack.c:558-565)

```c
case longvaluetype:
case ostypevaluetype:
case enumvaluetype:
case fixedvaluetype:
    fl = langunpackdata (sizeof (v.data.longvalue), &v.data.longvalue, h, &ixunpack);  // Reads 8 bytes!

    disktomemlong (v.data.longvalue);  // ⚠️ Swaps 4 bytes, leaves 4 bytes untouched
    break;
```

**The issue**:
1. Unpacks 8 bytes even if only 4 were valid in v6 format
2. `disktomemlong()` only swaps 4 bytes (macro expands to 32-bit swap)
3. High-order 4 bytes are left as-is (garbage from pack)

**Result**: Values round-trip through pack/unpack, but garbage is preserved.

---

## Proposed Fix

### Option 1: Store 32-bit in v7 Format (Backward Compatible)

**Change pack to write 4 bytes**:
```c
case longvaluetype:
case ostypevaluetype:
case enumvaluetype:
case fixedvaluetype:
    {
        uint32_t val32 = (uint32_t) val.data.longvalue;  // Truncate to 32-bit
        db_format_write_be32((unsigned char*)&val32, val32);
        fl = langpackdata(sizeof(uint32_t), &val32, hpackedvalue);  // Pack 4 bytes
    }
    break;
```

**Change unpack to read 4 bytes**:
```c
case longvaluetype:
case ostypevaluetype:
case enumvaluetype:
case fixedvaluetype:
    {
        uint32_t val32;
        fl = langunpackdata(sizeof(uint32_t), &val32, h, &ixunpack);  // Unpack 4 bytes
        v.data.longvalue = (int64_t) db_format_read_be32((unsigned char*)&val32);  // Widen to 64-bit
    }
    break;
```

**Pros**:
- Backward compatible with existing v7 databases
- Fixes garbage byte issue
- Matches historical 32-bit behavior

**Cons**:
- Still truncates values > 2^31 (but this was always true)
- Doesn't support true 64-bit long values

### Option 2: Store 64-bit in v7 Format (Future-Proof)

**Change pack to write 8 bytes**:
```c
case longvaluetype:
case ostypevaluetype:  // ⚠️ OSType must stay 32-bit!
case enumvaluetype:
case fixedvaluetype:   // ⚠️ Fixed is 32-bit Fixed type!
    {
        uint64_t val64 = (uint64_t) val.data.longvalue;
        db_format_write_be64((unsigned char*)&val64, val64);
        fl = langpackdata(sizeof(uint64_t), &val64, hpackedvalue);  // Pack 8 bytes
    }
    break;
```

**Change unpack to read 8 bytes**:
```c
case longvaluetype:
case ostypevaluetype:
case enumvaluetype:
case fixedvaluetype:
    {
        uint64_t val64;
        fl = langunpackdata(sizeof(uint64_t), &val64, h, &ixunpack);  // Unpack 8 bytes
        v.data.longvalue = (int64_t) db_format_read_be64((unsigned char*)&val64);
    }
    break;
```

**Pros**:
- Future-proof for large integers
- Eliminates garbage byte issue
- True 64-bit support

**Cons**:
- **INCOMPATIBLE** with existing v7 databases (breaks unpacking)
- **OSType and Fixed must stay 32-bit** (API contract)
- Requires database format version bump

### Option 3: Conditional Packing Based on Format Version

**Pack based on database format**:
```c
case longvaluetype:
    {
        if (db_format_use_64bit_scalars()) {  // New format flag
            uint64_t val64 = (uint64_t) val.data.longvalue;
            db_format_write_be64((unsigned char*)&val64, val64);
            fl = langpackdata(sizeof(uint64_t), &val64, hpackedvalue);
        } else {
            uint32_t val32 = (uint32_t) val.data.longvalue;
            db_format_write_be32((unsigned char*)&val32, val32);
            fl = langpackdata(sizeof(uint32_t), &val32, hpackedvalue);
        }
    }
    break;
```

**Pros**:
- Backward compatible with v7
- Enables future 64-bit support
- Clean migration path

**Cons**:
- More complex implementation
- Requires format version tracking

---

## Recommended Fix: Option 1 (32-bit storage)

### Rationale

1. **Immediate correctness**: Eliminates garbage bytes
2. **Backward compatible**: Works with existing v7 databases
3. **Low risk**: Minimal code changes
4. **Matches intent**: UserTalk rarely needs >32-bit integers

### Implementation Steps

1. **Update pack for longvaluetype**:
   - Write 4 bytes to temporary `uint32_t` variable
   - Pack 4 bytes (not 8)

2. **Update unpack for longvaluetype**:
   - Unpack 4 bytes to temporary `uint32_t` variable
   - Widen to 64-bit when storing to `v.data.longvalue`

3. **Handle other types correctly**:
   - `ostypevaluetype`: Always 32-bit (OSType is 4-byte code)
   - `enumvaluetype`: Always 32-bit (enum values are small)
   - `fixedvaluetype`: Always 32-bit (Fixed is 16.16 fixed-point)

4. **Separate datevaluetype**:
   - Keep at 32-bit for v7 (timestamps safe until 2040)
   - Plan 64-bit migration for dates separately (involves time_t standard)

5. **Test migration**:
   - Verify v6→v7 migration preserves long values
   - Test pack/unpack round-trip
   - Validate refcon handling

### Migration Safety

**Existing v7 databases**: Currently store 8 bytes (4 valid + 4 garbage)
- **Unpack with new code**: Reads 4 bytes (ignores garbage) → SAFE
- **Re-pack**: Writes 4 bytes (deterministic) → IMPROVEMENT

**v6 databases**: Store 4 bytes (correct)
- **Unpack with new code**: Reads 4 bytes, widens to 64-bit → SAFE
- **Pack to v7**: Writes 4 bytes (deterministic) → CORRECT

---

## Test Strategy

### Unit Tests

1. **Pack/unpack round-trip**:
```c
void test_long_pack_unpack(void) {
    tyvaluerecord val_in, val_out;
    Handle hpacked;

    setlongvalue(42, &val_in);
    assert(langpackvalue(val_in, &hpacked, HNoNode));
    assert(langunpackvalue(hpacked, &val_out));
    assert(val_out.data.longvalue == 42);

    // Test edge cases
    setlongvalue(0x7FFFFFFF, &val_in);  // Max 32-bit signed
    setlongvalue(-1, &val_in);          // Negative
    setlongvalue(0, &val_in);           // Zero
}
```

2. **Refcon storage**:
```c
void test_refcon_long_storage(void) {
    hdlheadrecord hnode;
    tyvaluerecord val_in, val_out;
    Handle hrefcon;

    opnewheadrecord(&hnode);
    setlongvalue(1000000, &val_in);
    langpackvalue(val_in, &hrefcon, HNoNode);
    opsetrefcon(hnode, *hrefcon, gethandlesize(hrefcon));

    // Retrieve and verify
    Handle hretrieved = (**hnode).hrefcon;
    langunpackvalue(hretrieved, &val_out);
    assert(val_out.data.longvalue == 1000000);
}
```

3. **Migration integrity**:
```usertalk
# Create v6 database with long values
table.value = 1000000
pack(table, @binary)

# Migrate to v7
# Verify value preserved
unpack(@binary, @table2)
assert(table2.value == 1000000)
```

### Integration Tests

1. **v6→v7 migration with refcons**:
   - Create v6 outline with refcon containing long value
   - Migrate to v7
   - Verify refcon value preserved

2. **Table value persistence**:
   - Create table with long values
   - Save to v7 database
   - Reload and verify values

3. **Cross-platform compatibility**:
   - Pack on 64-bit system
   - Unpack on 32-bit system (if available)
   - Verify values match

---

## Related Issues

### Issue #167: time_t Portability

**Status**: RESOLVED
- Timestamps migrated to 64-bit `frontier_time_t`
- **Not affected by this bug** (values fit in 32 bits)

### Issue #185: Database Format Corruption

**Status**: RESOLVED
- Context guard pattern established
- **Related**: Unpacking reads wrong number of bytes

### External Table Variables (docs/external_table_variable_management.md)

**Status**: DOCUMENTED
- External tables migrated with address widening
- **Not directly affected**: Addresses packed separately

---

## Action Items

### Immediate (Pre-Launch)

1. ✅ **Confirm bug exists**: Verified through code analysis
2. 🔲 **Implement Option 1 fix**: Update pack/unpack for 32-bit storage
3. 🔲 **Add unit tests**: Pack/unpack round-trip with edge cases
4. 🔲 **Test migration**: v6→v7 with long values in refcons/tables
5. 🔲 **Update documentation**: Note 32-bit limitation in v7 format

### Future (Post-Launch)

1. 🔲 **Evaluate 64-bit need**: Survey real-world UserTalk usage
2. 🔲 **Design v8 format**: If 64-bit longs needed, plan format upgrade
3. 🔲 **OSType separation**: Split `longvaluetype` from `ostypevaluetype` if needed

---

## Conclusion

**The user's hunch is 100% correct**: The current packing code writes 4 bytes but packs 8, creating data loss risk and non-determinism.

**Recommended immediate fix**: Update pack/unpack to explicitly handle 4 bytes (Option 1), eliminating garbage while maintaining backward compatibility.

**This is a launch-blocking issue** due to data integrity implications, even though real-world impact is low.

---

**References**:
- `Common/source/langpack.c:166-174` (pack bug)
- `Common/source/langpack.c:558-565` (unpack symmetry)
- `Common/headers/lang.h:306-365` (tyvaluedata union)
- `Common/source/oprefcon.c:217-264` (refcon usage)
- `planning/phase3/datetime_handling_audit.md` (timestamp migration)
