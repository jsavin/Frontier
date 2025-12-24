# External Object Test Coverage Plan - Comprehensive Refcon Testing

## Status
- **State**: 📋 PLANNING
- **Phase**: Phase 3 - External Object Migration & Validation
- **Created**: 2025-12-24
- **Priority**: CRITICAL - Refcons are essential data structures that MUST survive migration

## Executive Summary

This plan provides comprehensive test coverage for external object types (outline, script, menu, wptext, picture) with **special emphasis on refcon data integrity** during serialization and v6→v7 migration. Refcons are opaque application-specific handles attached to outline headlines that can contain:
- Arbitrary binary data blobs
- References to other external objects (pictures, tables, outlines, wpt ext)
- Structured application data (e.g., menu item metadata)
- Nested packed value structures

**Critical Requirement**: Refcons MUST survive the migration pathway byte-for-byte (or semantically equivalent if containing database addresses that need format conversion).

## Background: Refcon Architecture

### Refcon Data Structure (from `Common/headers/op.h`)

```c
typedef struct tyheadrecord {
    // ... navigation pointers and flags ...
    Handle hrefcon;       // Application-specific data handle
    Handle headstring;    // Text of headline
} tyheadrecord;
```

### Refcon Callback System

Each outline maintains callbacks for refcon lifecycle management:

```c
typedef struct tyoutlinerecord {
    opcopyrefconcallback copyrefconcallback;              // Copy refcon between headlines
    optextualizerefconcallback textualizerefconcallback;  // Pack refcon to text format
    opnodebooleancallback releaserefconcallback;          // Release refcon storage
    // ... other callbacks ...
} tyoutlinerecord;
```

**Key callbacks**:
1. **`opsetrefcon(hnode, pdata, len)`** - Set refcon data on headline
2. **`opgetrefcon(hnode, pdata, len)`** - Get refcon data from headline
3. **`ophasrefcon(hnode)`** - Check if headline has refcon
4. **`opcopyrefconroutine(hsource, hdest)`** - Copy refcon during headline duplication
5. **`optextualizerefconcallback(hnode, htext)`** - Serialize refcon to text (OPML export)
6. **`releaserefconcallback(hnode, fldisk)`** - Release refcon resources (memory or disk)

### Refcon Serialization Path

**Pack (Outline→Disk)**:
```
outtablevisit(hnode)
  ├─→ Extract (**hnode).hrefcon
  ├─→ Write lenrefcon (4 bytes, BE swapped)
  └─→ Write raw refcon data (lenrefcon bytes)
```

**Unpack (Disk→Outline)**:
```
intablevisit(hnode)
  ├─→ Read lenrefcon (4 bytes, BE swapped)
  ├─→ Allocate new handle (lenrefcon bytes)
  ├─→ Read refcon data into handle
  └─→ Attach (**hnode).hrefcon = hrefcon
```

**Critical insight**: Refcon serialization is OPAQUE - the packer doesn't interpret refcon contents. This means:
- ✅ Binary blobs preserve perfectly
- ⚠️ Database addresses (if stored in refcon) need special handling during migration
- ⚠️ References to other externals must remain valid after migration

### Known Refcon Use Cases

1. **Menu Items** (`menupack.c`):
   ```c
   typedef struct tymenuiteminfo {
       tylinkedscript linkedscript;  // Contains: hdloutlinerecord + dbaddress
       // ... other menu metadata ...
   } tymenuiteminfo;
   ```
   - Refcon contains outline handle + database address to script outline
   - `mereleaserefconroutine` must dispose outline and release dbaddress

2. **Table Browser** (`tableformats.c`):
   ```c
   // Refcon contains packed table value (binary)
   opattributesgetpackedtablevalue(hnode, &val)
     ├─→ langunpackvalue(hrefcon, &linkedval)  // Unpack outer binary
     └─→ langunpackvalue(linkedval.data.binaryvalue, &val)  // Unpack inner table
   ```
   - Refcon contains double-packed binary: outer binary wraps packed table
   - Used for headline attributes (type, custom fields, etc.)

3. **Opaque Binary Data**:
   - Application-defined structures
   - Icon references, state flags, custom metadata
   - Must survive pack/unpack cycle unchanged

---

## Test Plan Structure

### Phase 1: Refcon Fundamentals (P0 - BLOCKING)
Basic refcon operations without serialization.

### Phase 2: Refcon Serialization (P0 - BLOCKING)
Pack/unpack roundtrip testing for refcon data.

### Phase 3: Refcon Migration (P0 - BLOCKING)
V6→V7 migration testing for refcons.

### Phase 4: Refcon Reference Integrity (P0 - BLOCKING)
Refcons containing references to other externals.

### Phase 5: Refcon Error Handling (P1 - HIGH)
Corrupted/invalid refcon scenarios.

### Phase 6: External Object Integration (P1 - HIGH)
Test externals with refcon support (menu, wptext, picture).

### Phase 7: Performance & Stress (P2 - MEDIUM)
Large refcons, many refcons, nested structures.

---

## Phase 1: Refcon Fundamentals

**Objective**: Validate basic refcon operations (set/get/has/empty).

### Test 1.1: Simple Binary Blob Refcon

**Test Code**:
```c
void test_refcon_simple_binary_blob(void) {
    hdloutlinerecord houtline;
    hdlheadrecord hnode;

    // Create outline + headline
    assert(opnewoutline(&houtline));
    assert(opaddheadline((*houtline)->hsummit, down, "\pTest Node", &hnode));

    // Test data: 16-byte binary blob
    unsigned char test_data[16] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10
    };

    // Set refcon
    assert(opsetrefcon(hnode, test_data, 16));
    assert(ophasrefcon(hnode));

    // Get refcon and verify byte-for-byte match
    unsigned char retrieved[16];
    memset(retrieved, 0, 16);
    assert(opgetrefcon(hnode, retrieved, 16));
    assert(memcmp(test_data, retrieved, 16) == 0);

    // Verify handle size
    assert(gethandlesize((**hnode).hrefcon) == 16);

    opdisposeoutline(houtline, false);
}
```

**Success Criteria**:
- ✅ Refcon data stores exactly 16 bytes
- ✅ Retrieved data matches original byte-for-byte
- ✅ `ophasrefcon` returns true
- ✅ Handle size is exactly 16 bytes

---

### Test 1.2: Structured Data Refcon

**Test Code**:
```c
// Define test structure
typedef struct test_refcon_struct {
    int32_t magic_number;      // 0xDEADBEEF
    int16_t version;           // 42
    unsigned char flags;       // 0xFF
    unsigned char reserved;    // 0x00
    int64_t timestamp;         // Unix timestamp
} test_refcon_struct;

void test_refcon_structured_data(void) {
    hdloutlinerecord houtline;
    hdlheadrecord hnode;

    assert(opnewoutline(&houtline));
    assert(opaddheadline((*houtline)->hsummit, down, "\pTest Node", &hnode));

    // Create structured data
    test_refcon_struct original;
    original.magic_number = 0xDEADBEEF;
    original.version = 42;
    original.flags = 0xFF;
    original.reserved = 0x00;
    original.timestamp = 1234567890LL;

    // Set refcon
    assert(opsetrefcon(hnode, &original, sizeof(test_refcon_struct)));

    // Get refcon and verify structure fields
    test_refcon_struct retrieved;
    memset(&retrieved, 0, sizeof(test_refcon_struct));
    assert(opgetrefcon(hnode, &retrieved, sizeof(test_refcon_struct)));

    assert(retrieved.magic_number == 0xDEADBEEF);
    assert(retrieved.version == 42);
    assert(retrieved.flags == 0xFF);
    assert(retrieved.reserved == 0x00);
    assert(retrieved.timestamp == 1234567890LL);

    opdisposeoutline(houtline, false);
}
```

**Success Criteria**:
- ✅ Structured data fields preserve exactly
- ✅ Alignment is maintained
- ✅ No padding corruption

---

### Test 1.3: Empty and NULL Refcon Handling

**Test Code**:
```c
void test_refcon_empty_and_null(void) {
    hdloutlinerecord houtline;
    hdlheadrecord hnode;

    assert(opnewoutline(&houtline));
    assert(opaddheadline((*houtline)->hsummit, down, "\pTest Node", &hnode));

    // Initially no refcon
    assert(!ophasrefcon(hnode));
    assert((**hnode).hrefcon == NULL);

    // opgetrefcon on NULL refcon should return false but not crash
    unsigned char buffer[16];
    memset(buffer, 0xFF, 16);  // Fill with 0xFF
    boolean result = opgetrefcon(hnode, buffer, 16);
    assert(result == false);  // Per oprefcon.c:109-110
    assert(buffer[0] == 0x00);  // Should be zeroed (per oprefcon.c:107)

    // Set refcon, then empty it
    unsigned char test_data[4] = {0x01, 0x02, 0x03, 0x04};
    assert(opsetrefcon(hnode, test_data, 4));
    assert(ophasrefcon(hnode));

    opemptyrefcon(hnode);
    assert(!ophasrefcon(hnode));
    assert((**hnode).hrefcon == NULL);

    opdisposeoutline(houtline, false);
}
```

**Success Criteria**:
- ✅ NULL refcon doesn't crash
- ✅ `opgetrefcon` on NULL returns false and zeros buffer
- ✅ `opemptyrefcon` disposes handle and sets to NULL
- ✅ `ophasrefcon` returns false after empty

---

### Test 1.4: Refcon Size Mismatch Handling

**Test Code**:
```c
void test_refcon_size_mismatch(void) {
    hdloutlinerecord houtline;
    hdlheadrecord hnode;

    assert(opnewoutline(&houtline));
    assert(opaddheadline((*houtline)->hsummit, down, "\pTest Node", &hnode));

    // Set 16-byte refcon
    unsigned char test_data[16];
    for (int i = 0; i < 16; i++) test_data[i] = i;
    assert(opsetrefcon(hnode, test_data, 16));

    // Try to get with smaller buffer (8 bytes)
    unsigned char small_buffer[8];
    memset(small_buffer, 0xFF, 8);
    assert(opgetrefcon(hnode, small_buffer, 8));
    // Should get first 8 bytes (per oprefcon.c:112-113)
    for (int i = 0; i < 8; i++) {
        assert(small_buffer[i] == i);
    }

    // Try to get with larger buffer (32 bytes)
    unsigned char large_buffer[32];
    memset(large_buffer, 0xFF, 32);
    assert(opgetrefcon(hnode, large_buffer, 32));
    // First 16 bytes should match, rest should be zeroed (per oprefcon.c:106-107)
    for (int i = 0; i < 16; i++) {
        assert(large_buffer[i] == i);
    }
    for (int i = 16; i < 32; i++) {
        assert(large_buffer[i] == 0x00);
    }

    opdisposeoutline(houtline, false);
}
```

**Success Criteria**:
- ✅ Smaller buffer gets truncated data (no crash)
- ✅ Larger buffer gets zero-padded data
- ✅ No buffer overruns

---

## Phase 2: Refcon Serialization (Pack/Unpack)

**Objective**: Verify refcon data survives outline pack/unpack cycle.

### Test 2.1: Single Headline with Binary Blob Refcon

**Test Code**:
```c
void test_refcon_pack_unpack_simple(void) {
    hdloutlinerecord houtline_original, houtline_unpacked;
    hdlheadrecord hnode_original, hnode_unpacked;
    Handle hpacked;

    // Create outline + headline
    assert(opnewoutline(&houtline_original));
    assert(opaddheadline((*houtline_original)->hsummit, down, "\pNode with Refcon", &hnode_original));

    // Set refcon: 32-byte binary blob
    unsigned char original_refcon[32];
    for (int i = 0; i < 32; i++) original_refcon[i] = (unsigned char)(i * 3);
    assert(opsetrefcon(hnode_original, original_refcon, 32));

    // Pack outline
    assert(oppackoutline(houtline_original, &hpacked));
    assert(hpacked != NULL);
    assert(gethandlesize(hpacked) > 0);

    // Unpack outline
    assert(opunpackoutline(hpacked, &houtline_unpacked));
    assert(houtline_unpacked != NULL);

    // Get unpacked headline (first subhead of summit)
    hnode_unpacked = opnthsubhead((*houtline_unpacked)->hsummit, 1);
    assert(hnode_unpacked != NULL);

    // Verify headline text matches
    bigstring bs;
    getheadstring(hnode_unpacked, bs);
    assert(equalstrings(bs, "\pNode with Refcon"));

    // Verify refcon exists
    assert(ophasrefcon(hnode_unpacked));
    assert(gethandlesize((**hnode_unpacked).hrefcon) == 32);

    // Verify refcon data matches byte-for-byte
    unsigned char unpacked_refcon[32];
    assert(opgetrefcon(hnode_unpacked, unpacked_refcon, 32));
    assert(memcmp(original_refcon, unpacked_refcon, 32) == 0);

    // Cleanup
    disposehandle(hpacked);
    opdisposeoutline(houtline_original, false);
    opdisposeoutline(houtline_unpacked, false);
}
```

**Success Criteria**:
- ✅ Packed data is non-empty
- ✅ Unpack succeeds
- ✅ Refcon exists in unpacked outline
- ✅ Refcon size matches (32 bytes)
- ✅ Refcon data matches byte-for-byte

---

### Test 2.2: Multiple Headlines with Different Refcon Sizes

**Test Code**:
```c
void test_refcon_pack_unpack_multiple(void) {
    hdloutlinerecord houtline_original, houtline_unpacked;
    hdlheadrecord hnodes_original[5];
    hdlheadrecord hnodes_unpacked[5];
    Handle hpacked;

    assert(opnewoutline(&houtline_original));

    // Create 5 headlines with different refcon sizes
    size_t refcon_sizes[5] = {0, 8, 16, 128, 1024};

    for (int i = 0; i < 5; i++) {
        bigstring bs;
        copystring("\pNode ", bs);
        pushchar((char)('0' + i), bs);

        assert(opaddheadline((*houtline_original)->hsummit, down, bs, &hnodes_original[i]));

        if (refcon_sizes[i] > 0) {
            unsigned char *refcon_data = malloc(refcon_sizes[i]);
            for (size_t j = 0; j < refcon_sizes[i]; j++) {
                refcon_data[j] = (unsigned char)((i + j) % 256);
            }
            assert(opsetrefcon(hnodes_original[i], refcon_data, refcon_sizes[i]));
            free(refcon_data);
        }
        // Node 0 has no refcon (size = 0)
    }

    // Pack
    assert(oppackoutline(houtline_original, &hpacked));

    // Unpack
    assert(opunpackoutline(hpacked, &houtline_unpacked));

    // Verify each headline
    for (int i = 0; i < 5; i++) {
        hnodes_unpacked[i] = opnthsubhead((*houtline_unpacked)->hsummit, i + 1);
        assert(hnodes_unpacked[i] != NULL);

        if (refcon_sizes[i] == 0) {
            // Node 0 should have no refcon
            assert(!ophasrefcon(hnodes_unpacked[i]));
        } else {
            // Verify refcon size
            assert(ophasrefcon(hnodes_unpacked[i]));
            assert(gethandlesize((**hnodes_unpacked[i]).hrefcon) == (long)refcon_sizes[i]);

            // Verify refcon data
            unsigned char *unpacked_data = malloc(refcon_sizes[i]);
            assert(opgetrefcon(hnodes_unpacked[i], unpacked_data, refcon_sizes[i]));

            for (size_t j = 0; j < refcon_sizes[i]; j++) {
                unsigned char expected = (unsigned char)((i + j) % 256);
                assert(unpacked_data[j] == expected);
            }
            free(unpacked_data);
        }
    }

    // Cleanup
    disposehandle(hpacked);
    opdisposeoutline(houtline_original, false);
    opdisposeoutline(houtline_unpacked, false);
}
```

**Success Criteria**:
- ✅ All 5 headlines unpack correctly
- ✅ Node 0 has no refcon (NULL)
- ✅ Nodes 1-4 have correct refcon sizes (8, 16, 128, 1024 bytes)
- ✅ All refcon data matches byte-for-byte

---

### Test 2.3: Nested Outline Structure with Refcons

**Test Code**:
```c
void test_refcon_pack_unpack_nested(void) {
    hdloutlinerecord houtline_original, houtline_unpacked;
    Handle hpacked;

    assert(opnewoutline(&houtline_original));

    // Create nested structure:
    //   Root
    //     ├─ Level1-A (refcon: 4 bytes)
    //     │    ├─ Level2-A1 (refcon: 8 bytes)
    //     │    └─ Level2-A2 (no refcon)
    //     └─ Level1-B (refcon: 16 bytes)
    //          └─ Level2-B1 (refcon: 32 bytes)

    hdlheadrecord h_level1a, h_level1b;
    hdlheadrecord h_level2a1, h_level2a2, h_level2b1;

    assert(opaddheadline((*houtline_original)->hsummit, down, "\pLevel1-A", &h_level1a));
    uint32_t refcon1a = 0xAABBCCDD;
    assert(opsetrefcon(h_level1a, &refcon1a, 4));

    assert(opaddheadline(h_level1a, down, "\pLevel2-A1", &h_level2a1));
    uint64_t refcon2a1 = 0x1122334455667788ULL;
    assert(opsetrefcon(h_level2a1, &refcon2a1, 8));

    assert(opaddheadline(h_level1a, down, "\pLevel2-A2", &h_level2a2));
    // No refcon on Level2-A2

    assert(opaddheadline((*houtline_original)->hsummit, down, "\pLevel1-B", &h_level1b));
    unsigned char refcon1b[16];
    for (int i = 0; i < 16; i++) refcon1b[i] = i * 16;
    assert(opsetrefcon(h_level1b, refcon1b, 16));

    assert(opaddheadline(h_level1b, down, "\pLevel2-B1", &h_level2b1));
    unsigned char refcon2b1[32];
    for (int i = 0; i < 32; i++) refcon2b1[i] = 255 - i;
    assert(opsetrefcon(h_level2b1, refcon2b1, 32));

    // Pack
    assert(oppackoutline(houtline_original, &hpacked));

    // Unpack
    assert(opunpackoutline(hpacked, &houtline_unpacked));

    // Navigate unpacked structure and verify refcons
    hdlheadrecord h_level1a_u = opnthsubhead((*houtline_unpacked)->hsummit, 1);
    assert(h_level1a_u != NULL);
    uint32_t refcon1a_u = 0;
    assert(opgetrefcon(h_level1a_u, &refcon1a_u, 4));
    assert(refcon1a_u == 0xAABBCCDD);

    hdlheadrecord h_level2a1_u = opnthsubhead(h_level1a_u, 1);
    assert(h_level2a1_u != NULL);
    uint64_t refcon2a1_u = 0;
    assert(opgetrefcon(h_level2a1_u, &refcon2a1_u, 8));
    assert(refcon2a1_u == 0x1122334455667788ULL);

    hdlheadrecord h_level2a2_u = opnthsubhead(h_level1a_u, 2);
    assert(h_level2a2_u != NULL);
    assert(!ophasrefcon(h_level2a2_u));

    hdlheadrecord h_level1b_u = opnthsubhead((*houtline_unpacked)->hsummit, 2);
    assert(h_level1b_u != NULL);
    unsigned char refcon1b_u[16];
    assert(opgetrefcon(h_level1b_u, refcon1b_u, 16));
    assert(memcmp(refcon1b, refcon1b_u, 16) == 0);

    hdlheadrecord h_level2b1_u = opnthsubhead(h_level1b_u, 1);
    assert(h_level2b1_u != NULL);
    unsigned char refcon2b1_u[32];
    assert(opgetrefcon(h_level2b1_u, refcon2b1_u, 32));
    assert(memcmp(refcon2b1, refcon2b1_u, 32) == 0);

    // Cleanup
    disposehandle(hpacked);
    opdisposeoutline(houtline_original, false);
    opdisposeoutline(houtline_unpacked, false);
}
```

**Success Criteria**:
- ✅ All 5 headlines unpack with correct nesting
- ✅ All refcons preserve correctly
- ✅ Mixed refcon presence (some nodes have refcons, some don't)

---

### Test 2.4: Refcon with Packed Table Value (Menu-Style)

**Test Code**:
```c
void test_refcon_packed_table_value(void) {
    // This tests the pattern used by menupack.c and tableformats.c
    // where refcons contain double-packed table values

    hdloutlinerecord houtline_original, houtline_unpacked;
    hdlheadrecord hnode_original, hnode_unpacked;
    Handle hpacked;

    assert(opnewoutline(&houtline_original));
    assert(opaddheadline((*houtline_original)->hsummit, down, "\pNode with Packed Table", &hnode_original));

    // Create a simple table value
    tyvaluerecord table_val;
    clearbytes(&table_val, sizeof(table_val));
    table_val.valuetype = externalvaluetype;
    table_val.fltmpstack = false;

    hdlhashtable htable;
    assert(langnewtable(nil, &htable));

    // Add a string entry to table
    bigstring bskey, bsvalue;
    copystring("\ptest_key", bskey);
    copystring("\ptest_value", bsvalue);

    tyvaluerecord string_val;
    setstringvalue(bsvalue, &string_val);
    assert(hashassign(htable, bskey, string_val));

    // Pack the table
    Handle hpacked_table;
    assert(langpackvalue(table_val, &hpacked_table, nil));

    // Double-pack: wrap packed table in a binary value
    tyvaluerecord binary_val;
    setbinaryvalue(hpacked_table, (OSType)'TEST', &binary_val);

    Handle hpacked_binary;
    assert(langpackvalue(binary_val, &hpacked_binary, nil));

    // Set the double-packed binary as refcon
    assert(opsetrefcon(hnode_original, *hpacked_binary, gethandlesize(hpacked_binary)));

    // Pack outline
    assert(oppackoutline(houtline_original, &hpacked));

    // Unpack outline
    assert(opunpackoutline(hpacked, &houtline_unpacked));

    // Get unpacked headline
    hnode_unpacked = opnthsubhead((*houtline_unpacked)->hsummit, 1);
    assert(hnode_unpacked != NULL);
    assert(ophasrefcon(hnode_unpacked));

    // Extract refcon and verify it's the double-packed table
    Handle hrefcon_unpacked = (**hnode_unpacked).hrefcon;
    assert(hrefcon_unpacked != NULL);

    // Unpack outer layer (binary)
    tyvaluerecord binary_val_unpacked;
    assert(langunpackvalue(hrefcon_unpacked, &binary_val_unpacked));
    assert(binary_val_unpacked.valuetype == binaryvaluetype);

    // Unpack inner layer (table)
    tyvaluerecord table_val_unpacked;
    assert(langunpackvalue(binary_val_unpacked.data.binaryvalue, &table_val_unpacked));
    assert(table_val_unpacked.valuetype == externalvaluetype);

    // Extract table and verify contents
    hdlhashtable htable_unpacked;
    assert(tablevaltotable(table_val_unpacked, &htable_unpacked, nil));

    tyvaluerecord string_val_unpacked;
    hdlhashnode hnode_lookup;
    assert(hashtablelookup(htable_unpacked, bskey, &string_val_unpacked, &hnode_lookup));

    bigstring bs_retrieved;
    pullstringvalue(&string_val_unpacked, bs_retrieved);
    assert(equalstrings(bs_retrieved, bsvalue));

    // Cleanup
    disposevaluerecord(binary_val_unpacked, false);
    disposevaluerecord(table_val_unpacked, false);
    disposehandle(hpacked);
    disposehandle(hpacked_binary);
    opdisposeoutline(houtline_original, false);
    opdisposeoutline(houtline_unpacked, false);
}
```

**Success Criteria**:
- ✅ Double-packed table value survives pack/unpack
- ✅ Outer binary layer unpacks correctly
- ✅ Inner table value unpacks correctly
- ✅ Table contents (key/value) preserved

---

## Phase 3: Refcon Migration (V6→V7)

**Objective**: Verify refcon data survives v6→v7 database migration.

### Test 3.1: Simple Binary Blob Refcon Migration

**Test Setup**: Create v6 database with outline containing refcon, migrate to v7, verify refcon preservation.

**Test Code**:
```c
void test_refcon_migration_simple_blob(void) {
    // This test requires a v6 database fixture with a known outline + refcon
    // Or we create one programmatically if we have v6 writer support

    // For now, assuming we have test fixture: tests/fixtures/refcon_test_v6.root
    // Contents:
    //   workspace.test_outline
    //     └─ "Node with Refcon" (refcon: 64-byte binary blob, pattern 0x00-0x3F)

    char *v6_db_path = "/Users/jake/dev/jsavin/Frontier/tests/fixtures/refcon_test_v6.root";
    char *v7_db_path = "/Users/jake/dev/jsavin/Frontier/tests/output/refcon_test_v7.root";

    // Migrate v6→v7
    assert(migrate_database(v6_db_path, v7_db_path));

    // Open v7 database
    hdldatabaserecord hdb;
    assert(dbopenfile(v7_db_path, &hdb, 'LAND', true));
    assert(dbrefnumset(&hdb, 1));  // Set as system.misc.databaseFile

    // Navigate to workspace.test_outline
    tyvaluerecord val_outline;
    bigstring bs_path;
    copystring("\pworkspace.test_outline", bs_path);
    assert(langgetval(bs_path, &val_outline));
    assert(val_outline.valuetype == outlinevaluetype);

    hdloutlinerecord houtline = (hdloutlinerecord)val_outline.data.externalvalue;
    assert(houtline != NULL);

    // Get first headline
    hdlheadrecord hnode = opnthsubhead((*houtline)->hsummit, 1);
    assert(hnode != NULL);

    // Verify headline text
    bigstring bs_text;
    getheadstring(hnode, bs_text);
    assert(equalstrings(bs_text, "\pNode with Refcon"));

    // Verify refcon exists and has correct size
    assert(ophasrefcon(hnode));
    assert(gethandlesize((**hnode).hrefcon) == 64);

    // Verify refcon data (pattern 0x00-0x3F)
    unsigned char refcon_data[64];
    assert(opgetrefcon(hnode, refcon_data, 64));
    for (int i = 0; i < 64; i++) {
        assert(refcon_data[i] == (unsigned char)i);
    }

    // Cleanup
    dbclose(hdb);
}
```

**Test Fixture Requirements**:
- Create `tests/fixtures/refcon_test_v6.root` (v6 database)
- Contains: `workspace.test_outline` (outline value)
- Outline has 1 headline: "Node with Refcon"
- Headline refcon: 64-byte binary blob (0x00, 0x01, ..., 0x3F)

**Success Criteria**:
- ✅ Migration completes successfully
- ✅ Outline loads from v7 database
- ✅ Headline text preserved
- ✅ Refcon exists after migration
- ✅ Refcon size is 64 bytes
- ✅ Refcon data matches expected pattern

---

### Test 3.2: Multiple Refcons Migration (Mixed Sizes)

**Test Code**:
```c
void test_refcon_migration_multiple(void) {
    // Fixture: tests/fixtures/refcon_multiple_v6.root
    // Contents:
    //   workspace.multi_refcon_outline
    //     ├─ "Node 0" (no refcon)
    //     ├─ "Node 1" (refcon: 16 bytes)
    //     ├─ "Node 2" (refcon: 256 bytes)
    //     └─ "Node 3" (refcon: 2048 bytes)

    char *v6_db_path = "/Users/jake/dev/jsavin/Frontier/tests/fixtures/refcon_multiple_v6.root";
    char *v7_db_path = "/Users/jake/dev/jsavin/Frontier/tests/output/refcon_multiple_v7.root";

    assert(migrate_database(v6_db_path, v7_db_path));

    hdldatabaserecord hdb;
    assert(dbopenfile(v7_db_path, &hdb, 'LAND', true));
    assert(dbrefnumset(&hdb, 1));

    tyvaluerecord val_outline;
    assert(langgetval("\pworkspace.multi_refcon_outline", &val_outline));
    hdloutlinerecord houtline = (hdloutlinerecord)val_outline.data.externalvalue;

    size_t expected_sizes[4] = {0, 16, 256, 2048};

    for (int i = 0; i < 4; i++) {
        hdlheadrecord hnode = opnthsubhead((*houtline)->hsummit, i + 1);
        assert(hnode != NULL);

        if (expected_sizes[i] == 0) {
            assert(!ophasrefcon(hnode));
        } else {
            assert(ophasrefcon(hnode));
            assert(gethandlesize((**hnode).hrefcon) == (long)expected_sizes[i]);

            // Verify refcon data pattern (each byte = (i + offset) % 256)
            unsigned char *refcon_data = malloc(expected_sizes[i]);
            assert(opgetrefcon(hnode, refcon_data, expected_sizes[i]));

            for (size_t j = 0; j < expected_sizes[i]; j++) {
                unsigned char expected = (unsigned char)((i + j) % 256);
                assert(refcon_data[j] == expected);
            }
            free(refcon_data);
        }
    }

    dbclose(hdb);
}
```

**Success Criteria**:
- ✅ All 4 nodes migrate correctly
- ✅ Node 0 has no refcon
- ✅ Nodes 1-3 have correct refcon sizes (16, 256, 2048)
- ✅ All refcon data matches expected patterns

---

### Test 3.3: Nested Outline with Refcons Migration

**Test Code**:
```c
void test_refcon_migration_nested(void) {
    // Fixture: tests/fixtures/refcon_nested_v6.root
    // Structure (same as Test 2.3):
    //   workspace.nested_refcon_outline
    //     ├─ "Level1-A" (refcon: 4 bytes = 0xAABBCCDD)
    //     │    ├─ "Level2-A1" (refcon: 8 bytes = 0x1122334455667788)
    //     │    └─ "Level2-A2" (no refcon)
    //     └─ "Level1-B" (refcon: 16 bytes)
    //          └─ "Level2-B1" (refcon: 32 bytes)

    char *v6_db_path = "/Users/jake/dev/jsavin/Frontier/tests/fixtures/refcon_nested_v6.root";
    char *v7_db_path = "/Users/jake/dev/jsavin/Frontier/tests/output/refcon_nested_v7.root";

    assert(migrate_database(v6_db_path, v7_db_path));

    hdldatabaserecord hdb;
    assert(dbopenfile(v7_db_path, &hdb, 'LAND', true));
    assert(dbrefnumset(&hdb, 1));

    tyvaluerecord val_outline;
    assert(langgetval("\pworkspace.nested_refcon_outline", &val_outline));
    hdloutlinerecord houtline = (hdloutlinerecord)val_outline.data.externalvalue;

    // Verify Level1-A refcon
    hdlheadrecord h_level1a = opnthsubhead((*houtline)->hsummit, 1);
    uint32_t refcon1a = 0;
    assert(opgetrefcon(h_level1a, &refcon1a, 4));
    assert(refcon1a == 0xAABBCCDD);

    // Verify Level2-A1 refcon
    hdlheadrecord h_level2a1 = opnthsubhead(h_level1a, 1);
    uint64_t refcon2a1 = 0;
    assert(opgetrefcon(h_level2a1, &refcon2a1, 8));
    assert(refcon2a1 == 0x1122334455667788ULL);

    // Verify Level2-A2 no refcon
    hdlheadrecord h_level2a2 = opnthsubhead(h_level1a, 2);
    assert(!ophasrefcon(h_level2a2));

    // Verify Level1-B and Level2-B1 refcons
    hdlheadrecord h_level1b = opnthsubhead((*houtline)->hsummit, 2);
    unsigned char refcon1b[16];
    assert(opgetrefcon(h_level1b, refcon1b, 16));

    hdlheadrecord h_level2b1 = opnthsubhead(h_level1b, 1);
    unsigned char refcon2b1[32];
    assert(opgetrefcon(h_level2b1, refcon2b1, 32));

    dbclose(hdb);
}
```

**Success Criteria**:
- ✅ Nested structure preserved
- ✅ All refcons migrate correctly
- ✅ Mixed refcon presence preserved

---

### Test 3.4: Refcon Endianness Migration (Critical)

**Objective**: Verify that refcons containing multi-byte integers survive endianness conversion (v6 LE → v7 BE).

**Important Note**: This test addresses a CRITICAL migration concern. If refcons contain:
- Raw multi-byte integers (int16, int32, int64)
- dbaddress values (now 64-bit BE instead of 32-bit LE)
- Structured data with byte-order-sensitive fields

Then migration MUST either:
1. **Preserve refcon as opaque binary** (application handles endianness)
2. **Perform endianness conversion** (if refcon structure is known)

**Current Frontier behavior**: Refcons are treated as opaque binary blobs - NO endianness conversion.

**Test Code**:
```c
void test_refcon_migration_endianness(void) {
    // This test verifies that refcons are treated as OPAQUE blobs
    // and NOT endian-swapped during migration

    // Fixture: tests/fixtures/refcon_endian_v6.root
    // Contains:
    //   workspace.endian_test_outline
    //     └─ "Endian Node" (refcon: 8 bytes = int16 + int32 + int16)
    //         Original v6 (LE): 0x1234 0x56789ABC 0xDEF0
    //         Bytes: 34 12 BC 9A 78 56 F0 DE

    char *v6_db_path = "/Users/jake/dev/jsavin/Frontier/tests/fixtures/refcon_endian_v6.root";
    char *v7_db_path = "/Users/jake/dev/jsavin/Frontier/tests/output/refcon_endian_v7.root";

    assert(migrate_database(v6_db_path, v7_db_path));

    hdldatabaserecord hdb;
    assert(dbopenfile(v7_db_path, &hdb, 'LAND', true));
    assert(dbrefnumset(&hdb, 1));

    tyvaluerecord val_outline;
    assert(langgetval("\pworkspace.endian_test_outline", &val_outline));
    hdloutlinerecord houtline = (hdloutlinerecord)val_outline.data.externalvalue;

    hdlheadrecord hnode = opnthsubhead((*houtline)->hsummit, 1);
    assert(ophasrefcon(hnode));
    assert(gethandlesize((**hnode).hrefcon) == 8);

    // Get raw refcon bytes
    unsigned char refcon_bytes[8];
    assert(opgetrefcon(hnode, refcon_bytes, 8));

    // Verify bytes are UNCHANGED (opaque binary preservation)
    unsigned char expected_bytes[8] = {0x34, 0x12, 0xBC, 0x9A, 0x78, 0x56, 0xF0, 0xDE};
    assert(memcmp(refcon_bytes, expected_bytes, 8) == 0);

    // If application needs to interpret these as LE values on BE system,
    // it must perform its own endian conversion:
    int16_t val1 = (int16_t)((refcon_bytes[1] << 8) | refcon_bytes[0]);  // LE read
    int32_t val2 = (int32_t)((refcon_bytes[5] << 24) | (refcon_bytes[4] << 16) |
                             (refcon_bytes[3] << 8) | refcon_bytes[2]);
    int16_t val3 = (int16_t)((refcon_bytes[7] << 8) | refcon_bytes[6]);

    assert(val1 == 0x1234);
    assert(val2 == 0x56789ABC);
    assert(val3 == (int16_t)0xDEF0);

    dbclose(hdb);
}
```

**Success Criteria**:
- ✅ Refcon bytes are UNCHANGED (opaque preservation)
- ✅ Application can extract LE values if needed
- ✅ No automatic endian conversion by migrator

**Design Decision**: Refcons are opaque blobs - applications that store multi-byte values MUST handle their own endianness.

---

## Phase 4: Refcon Reference Integrity

**Objective**: Test refcons that contain references to other external objects.

### Test 4.1: Refcon Containing dbaddress to Picture External

**Critical Issue**: If a refcon contains a `dbaddress` to another external (e.g., picture), the address format changes during migration:
- v6: 32-bit LE dbaddress
- v7: 64-bit BE dbaddress

**Current Problem**: Refcons are opaque - addresses inside refcons are NOT converted.

**Solution Approaches**:
1. **Force externals to memory** (`flinmemory=1`) during migration - no dbaddress in refcons
2. **Application-specific refcon migration hooks** - allow callbacks to update addresses
3. **Known refcon formats registry** - migrator knows how to update specific refcon types

**Test Code** (assuming approach #1 - force to memory):
```c
void test_refcon_picture_reference_migration(void) {
    // Fixture: tests/fixtures/refcon_picture_ref_v6.root
    // Contents:
    //   workspace.picture_external = <picture external at dbaddress 0x12345678>
    //   workspace.outline_with_picture_ref
    //     └─ "Node with Picture Ref" (refcon contains dbaddress to picture)

    // NOTE: This test expects picture external to be FORCED TO MEMORY during migration
    // so refcon no longer contains dbaddress, but instead contains memory pointer or
    // is rewritten to reference the in-memory picture

    char *v6_db_path = "/Users/jake/dev/jsavin/Frontier/tests/fixtures/refcon_picture_ref_v6.root";
    char *v7_db_path = "/Users/jake/dev/jsavin/Frontier/tests/output/refcon_picture_ref_v7.root";

    assert(migrate_database(v6_db_path, v7_db_path));

    hdldatabaserecord hdb;
    assert(dbopenfile(v7_db_path, &hdb, 'LAND', true));
    assert(dbrefnumset(&hdb, 1));

    // Verify picture external exists
    tyvaluerecord val_picture;
    assert(langgetval("\pworkspace.picture_external", &val_picture));
    assert(val_picture.valuetype == pictvaluetype);
    // Verify it's in memory (flinmemory should be true after migration)
    // NOTE: Need access to internal value flags to verify this

    // Verify outline with picture reference
    tyvaluerecord val_outline;
    assert(langgetval("\pworkspace.outline_with_picture_ref", &val_outline));
    hdloutlinerecord houtline = (hdloutlinerecord)val_outline.data.externalvalue;

    hdlheadrecord hnode = opnthsubhead((*houtline)->hsummit, 1);
    assert(ophasrefcon(hnode));

    // Verify refcon is valid (no crashes accessing it)
    // The exact validation depends on refcon structure - for now, just verify it exists
    // and has expected size
    assert(gethandlesize((**hnode).hrefcon) > 0);

    dbclose(hdb);
}
```

**Test Fixture Requirements**:
- Create v6 database with picture external
- Create outline with refcon containing dbaddress to picture
- Migration should force picture to memory

**Success Criteria**:
- ✅ Picture external migrates successfully
- ✅ Picture is forced to memory (flinmemory=1)
- ✅ Outline refcon still valid (no crashes)
- ✅ Refcon reference to picture remains accessible

**CRITICAL NOTE**: This test may FAIL if refcon contains raw dbaddress that isn't updated. Requires design decision on refcon migration strategy.

---

### Test 4.2: Refcon with Outline Reference (Menu-Style)

**Test Code**:
```c
void test_refcon_outline_reference_migration(void) {
    // This tests the menupack.c pattern where refcons contain:
    //   tymenuiteminfo {
    //     tylinkedscript linkedscript;  // hdloutlinerecord + dbaddress
    //   }

    // Fixture: tests/fixtures/refcon_menu_v6.root
    // Contents:
    //   workspace.menu_outline (menu structure)
    //     └─ "Menu Item" (refcon contains linkedscript to workspace.script_outline)
    //   workspace.script_outline (the linked script)

    char *v6_db_path = "/Users/jake/dev/jsavin/Frontier/tests/fixtures/refcon_menu_v6.root";
    char *v7_db_path = "/Users/jake/dev/jsavin/Frontier/tests/output/refcon_menu_v7.root";

    assert(migrate_database(v6_db_path, v7_db_path));

    hdldatabaserecord hdb;
    assert(dbopenfile(v7_db_path, &hdb, 'LAND', true));
    assert(dbrefnumset(&hdb, 1));

    // Load menu outline
    tyvaluerecord val_menu;
    assert(langgetval("\pworkspace.menu_outline", &val_menu));
    hdloutlinerecord hmenu = (hdloutlinerecord)val_menu.data.externalvalue;

    // Get menu item headline
    hdlheadrecord hnode = opnthsubhead((*hmenu)->hsummit, 1);
    assert(ophasrefcon(hnode));

    // Extract menu item info from refcon
    tymenuiteminfo item;
    assert(opgetrefcon(hnode, &item, sizeof(tymenuiteminfo)));

    // Verify linked script outline handle is valid
    assert(item.linkedscript.houtline != NULL);

    // Verify linked script address
    // NOTE: If address was in refcon and not updated, this may be invalid
    // Migration should either:
    //   1. Force script to memory (houtline valid, adrlink = nildbaddress)
    //   2. Update adrlink to v7 format

    // For now, verify houtline is accessible
    assert((*item.linkedscript.houtline)->hsummit != NULL);

    dbclose(hdb);
}
```

**Success Criteria**:
- ✅ Menu outline migrates
- ✅ Menu item refcon valid
- ✅ Linked script outline accessible
- ✅ No crashes accessing refcon data

**CRITICAL NOTE**: This test exposes the dbaddress-in-refcon problem. Requires design decision.

---

## Phase 5: Refcon Error Handling

**Objective**: Test error scenarios with refcons.

### Test 5.1: Corrupted Refcon Data (Truncated)

**Test Code**:
```c
void test_refcon_corrupted_truncated(void) {
    // Simulate a corrupted outline pack where refcon data is truncated

    hdloutlinerecord houtline_original;
    hdlheadrecord hnode;
    Handle hpacked;

    assert(opnewoutline(&houtline_original));
    assert(opaddheadline((*houtline_original)->hsummit, down, "\pNode", &hnode));

    // Set 64-byte refcon
    unsigned char refcon_data[64];
    for (int i = 0; i < 64; i++) refcon_data[i] = i;
    assert(opsetrefcon(hnode, refcon_data, 64));

    // Pack outline
    assert(oppackoutline(houtline_original, &hpacked));

    // Corrupt: truncate packed data to simulate incomplete refcon
    long original_size = gethandlesize(hpacked);
    assert(sethandlesize(hpacked, original_size - 32));  // Remove last 32 bytes

    // Try to unpack - should fail gracefully (not crash)
    hdloutlinerecord houtline_unpacked;
    boolean result = opunpackoutline(hpacked, &houtline_unpacked);

    // Unpack may fail OR succeed with truncated refcon
    if (result) {
        // If unpack succeeded, refcon should be truncated or missing
        hdlheadrecord hnode_unpacked = opnthsubhead((*houtline_unpacked)->hsummit, 1);
        // Verify no crash accessing node
        assert(hnode_unpacked != NULL);
        opdisposeoutline(houtline_unpacked, false);
    }
    // If unpack failed, that's acceptable error handling

    disposehandle(hpacked);
    opdisposeoutline(houtline_original, false);
}
```

**Success Criteria**:
- ✅ No crash on truncated data
- ✅ Unpack either fails gracefully OR succeeds with partial data
- ✅ No memory leaks

---

### Test 5.2: Oversized Refcon (>1MB)

**Test Code**:
```c
void test_refcon_oversized(void) {
    hdloutlinerecord houtline_original, houtline_unpacked;
    hdlheadrecord hnode;
    Handle hpacked;

    assert(opnewoutline(&houtline_original));
    assert(opaddheadline((*houtline_original)->hsummit, down, "\pBig Node", &hnode));

    // Create 2MB refcon
    size_t refcon_size = 2 * 1024 * 1024;
    unsigned char *refcon_data = malloc(refcon_size);
    assert(refcon_data != NULL);

    // Fill with pattern
    for (size_t i = 0; i < refcon_size; i++) {
        refcon_data[i] = (unsigned char)(i % 256);
    }

    // Set refcon
    boolean set_result = opsetrefcon(hnode, refcon_data, refcon_size);
    if (!set_result) {
        // Acceptable: refcon too large, set fails gracefully
        free(refcon_data);
        opdisposeoutline(houtline_original, false);
        return;
    }

    // Pack outline
    boolean pack_result = oppackoutline(houtline_original, &hpacked);
    if (!pack_result) {
        // Acceptable: pack fails on oversized refcon
        free(refcon_data);
        opdisposeoutline(houtline_original, false);
        return;
    }

    // Unpack outline
    assert(opunpackoutline(hpacked, &houtline_unpacked));

    // Verify refcon
    hdlheadrecord hnode_unpacked = opnthsubhead((*houtline_unpacked)->hsummit, 1);
    assert(ophasrefcon(hnode_unpacked));
    assert(gethandlesize((**hnode_unpacked).hrefcon) == (long)refcon_size);

    // Spot-check refcon data (don't compare all 2MB)
    unsigned char sample[256];
    assert(opgetrefcon(hnode_unpacked, sample, 256));
    for (int i = 0; i < 256; i++) {
        assert(sample[i] == (unsigned char)i);
    }

    free(refcon_data);
    disposehandle(hpacked);
    opdisposeoutline(houtline_original, false);
    opdisposeoutline(houtline_unpacked, false);
}
```

**Success Criteria**:
- ✅ Either succeeds with large refcon OR fails gracefully
- ✅ No crashes
- ✅ No memory corruption

---

### Test 5.3: NULL Refcon Handle Corruption

**Test Code**:
```c
void test_refcon_null_handle_corruption(void) {
    hdloutlinerecord houtline;
    hdlheadrecord hnode;

    assert(opnewoutline(&houtline));
    assert(opaddheadline((*houtline)->hsummit, down, "\pNode", &hnode));

    // Set refcon
    uint32_t refcon_data = 0xDEADBEEF;
    assert(opsetrefcon(hnode, &refcon_data, 4));
    assert(ophasrefcon(hnode));

    // Manually corrupt: set refcon to NULL (simulates memory corruption)
    (**hnode).hrefcon = NULL;

    // Verify ophasrefcon detects NULL
    assert(!ophasrefcon(hnode));

    // Verify opgetrefcon handles NULL gracefully
    uint32_t retrieved = 0;
    boolean result = opgetrefcon(hnode, &retrieved, 4);
    assert(result == false);
    assert(retrieved == 0);  // Should be zeroed

    // Verify opemptyrefcon handles NULL gracefully (no crash)
    opemptyrefcon(hnode);  // Should be no-op per oprefcon.c:123-124

    opdisposeoutline(houtline, false);
}
```

**Success Criteria**:
- ✅ NULL refcon detected by `ophasrefcon`
- ✅ `opgetrefcon` returns false and zeros buffer
- ✅ `opemptyrefcon` doesn't crash on NULL

---

## Phase 6: External Object Integration

**Objective**: Test externals that use refcons (menu, wptext, picture).

### Test 6.1: Menu External with Refcons

**Test Code**:
```c
void test_menu_external_with_refcons(void) {
    // Test complete menu structure with script links stored in refcons

    // Create menu outline
    hdlmenurecord hmenu;
    hdloutlinerecord ho;
    assert(opnewoutline(&ho));
    assert(menewmenurecord(&hmenu, ho));

    // Add menu item with script
    hdlheadrecord hitem;
    assert(opaddheadline((*ho)->hsummit, down, "\pFile Menu Item", &hitem));

    // Create script outline
    hdloutlinerecord hscript;
    assert(opnewoutline(&hscript));
    hdlheadrecord hscript_line;
    assert(opaddheadline((*hscript)->hsummit, down, "\pbeep()", &hscript_line));

    // Attach script to menu item (sets refcon with linkedscript)
    tymenuiteminfo item;
    clearbytes(&item, sizeof(item));
    item.linkedscript.houtline = hscript;
    item.linkedscript.adrlink = nildbaddress;

    assert(opsetrefcon(hitem, &item, sizeof(tymenuiteminfo)));

    // Pack menu
    tysavedmenuinfo savedinfo;
    Handle hpackedmenu;
    assert(mepackmenustructure(&savedinfo, &hpackedmenu));

    // Unpack menu
    hdlmenurecord hmenu_unpacked;
    assert(meunpackmenustructure(hpackedmenu, &hmenu_unpacked));

    // Verify menu item refcon
    hdlheadrecord hitem_unpacked = opnthsubhead((*(*hmenu_unpacked)->menuoutline)->hsummit, 1);
    assert(ophasrefcon(hitem_unpacked));

    tymenuiteminfo item_unpacked;
    assert(opgetrefcon(hitem_unpacked, &item_unpacked, sizeof(tymenuiteminfo)));
    assert(item_unpacked.linkedscript.houtline != NULL);

    // Cleanup
    disposehandle(hpackedmenu);
    medisposemenurecord(hmenu, false);
    medisposemenurecord(hmenu_unpacked, false);
}
```

**Success Criteria**:
- ✅ Menu structure packs/unpacks
- ✅ Refcon with linkedscript preserved
- ✅ Script outline accessible after unpack

---

### Test 6.2: WPText External (No Refcons Expected)

**Test Code**:
```c
void test_wptext_external_no_refcons(void) {
    // WPText externals don't use refcons in outline headlines
    // This test verifies that wptext objects pack/unpack correctly
    // without refcon involvement

    hdloutlinerecord houtline;
    hdlheadrecord hnode;

    assert(opnewoutline(&houtline));
    assert(opaddheadline((*houtline)->hsummit, down, "\pWPText Node", &hnode));

    // Create wptext external
    Handle hwptext;
    assert(opnewwptext(&hwptext));

    // Add text to wptext
    bigstring bs;
    copystring("\pThis is rich text content.", bs);
    assert(wpinserttext(hwptext, bs));

    // Store wptext in table (not as refcon)
    tyvaluerecord val;
    setwptextvalue(hwptext, &val);

    // Pack/unpack cycle (wptext should survive)
    Handle hpacked;
    assert(langpackvalue(val, &hpacked, nil));

    tyvaluerecord val_unpacked;
    assert(langunpackvalue(hpacked, &val_unpacked));
    assert(val_unpacked.valuetype == wptextvaluetype);

    // Verify wptext content
    Handle hwptext_unpacked = (Handle)val_unpacked.data.externalvalue;
    bigstring bs_retrieved;
    assert(wpgettext(hwptext_unpacked, bs_retrieved));
    assert(equalstrings(bs_retrieved, bs));

    // Cleanup
    disposevaluerecord(val, false);
    disposevaluerecord(val_unpacked, false);
    disposehandle(hpacked);
    opdisposeoutline(houtline, false);
}
```

**Success Criteria**:
- ✅ WPText packs/unpacks correctly
- ✅ No refcons involved (wptext stored as external value)
- ✅ Text content preserved

---

### Test 6.3: Picture External (No Refcons in Picture Itself)

**Test Code**:
```c
void test_picture_external_no_refcons(void) {
    // Picture externals don't have refcons
    // But outlines might have refcons that REFERENCE pictures

    // Create picture external
    Handle hpicture;
    assert(opnewpicture(&hpicture));

    // Set picture data (simplified - actual picture format is complex)
    unsigned char picture_data[128];
    for (int i = 0; i < 128; i++) picture_data[i] = i;
    assert(oppicturesetdata(hpicture, picture_data, 128));

    // Store in value
    tyvaluerecord val;
    setpicturevalue(hpicture, &val);

    // Pack/unpack
    Handle hpacked;
    assert(langpackvalue(val, &hpacked, nil));

    tyvaluerecord val_unpacked;
    assert(langunpackvalue(hpacked, &val_unpacked));
    assert(val_unpacked.valuetype == pictvaluetype);

    // Verify picture data
    Handle hpicture_unpacked = (Handle)val_unpacked.data.externalvalue;
    unsigned char retrieved_data[128];
    assert(oppicturegetdata(hpicture_unpacked, retrieved_data, 128));
    assert(memcmp(picture_data, retrieved_data, 128) == 0);

    // Cleanup
    disposevaluerecord(val, false);
    disposevaluerecord(val_unpacked, false);
    disposehandle(hpacked);
}
```

**Success Criteria**:
- ✅ Picture packs/unpacks correctly
- ✅ No refcons involved
- ✅ Picture data preserved

---

## Phase 7: Performance & Stress Testing

### Test 7.1: 1000 Headlines with Refcons

**Test Code**:
```c
void test_refcon_performance_1000_headlines(void) {
    hdloutlinerecord houtline_original, houtline_unpacked;
    Handle hpacked;

    assert(opnewoutline(&houtline_original));

    // Create 1000 headlines, each with 64-byte refcon
    for (int i = 0; i < 1000; i++) {
        bigstring bs;
        copystring("\pNode ", bs);
        // Add number suffix
        pushlong(i, bs);

        hdlheadrecord hnode;
        assert(opaddheadline((*houtline_original)->hsummit, down, bs, &hnode));

        unsigned char refcon_data[64];
        for (int j = 0; j < 64; j++) {
            refcon_data[j] = (unsigned char)((i + j) % 256);
        }
        assert(opsetrefcon(hnode, refcon_data, 64));
    }

    // Pack
    clock_t start_pack = clock();
    assert(oppackoutline(houtline_original, &hpacked));
    clock_t end_pack = clock();
    double pack_time = (double)(end_pack - start_pack) / CLOCKS_PER_SEC;

    // Unpack
    clock_t start_unpack = clock();
    assert(opunpackoutline(hpacked, &houtline_unpacked));
    clock_t end_unpack = clock();
    double unpack_time = (double)(end_unpack - start_unpack) / CLOCKS_PER_SEC;

    // Verify sample headlines (not all 1000)
    for (int i = 0; i < 1000; i += 100) {
        hdlheadrecord hnode = opnthsubhead((*houtline_unpacked)->hsummit, i + 1);
        assert(hnode != NULL);
        assert(ophasrefcon(hnode));

        unsigned char refcon_data[64];
        assert(opgetrefcon(hnode, refcon_data, 64));

        // Spot-check first byte
        unsigned char expected = (unsigned char)(i % 256);
        assert(refcon_data[0] == expected);
    }

    printf("Pack time (1000 refcons): %.3f sec\n", pack_time);
    printf("Unpack time (1000 refcons): %.3f sec\n", unpack_time);

    // Cleanup
    disposehandle(hpacked);
    opdisposeoutline(houtline_original, false);
    opdisposeoutline(houtline_unpacked, false);
}
```

**Success Criteria**:
- ✅ Pack completes in <5 seconds
- ✅ Unpack completes in <5 seconds
- ✅ All refcons preserved correctly
- ✅ No memory leaks

---

### Test 7.2: Deep Nesting with Refcons (10 Levels)

**Test Code**:
```c
void test_refcon_deep_nesting(void) {
    hdloutlinerecord houtline_original, houtline_unpacked;
    Handle hpacked;

    assert(opnewoutline(&houtline_original));

    // Create 10-level deep nesting, each with refcon
    hdlheadrecord hcurrent = (*houtline_original)->hsummit;

    for (int level = 0; level < 10; level++) {
        bigstring bs;
        copystring("\pLevel ", bs);
        pushlong(level, bs);

        hdlheadrecord hnode;
        assert(opaddheadline(hcurrent, down, bs, &hnode));

        uint32_t refcon_data = 0x10000000 + level;
        assert(opsetrefcon(hnode, &refcon_data, 4));

        hcurrent = hnode;
    }

    // Pack
    assert(oppackoutline(houtline_original, &hpacked));

    // Unpack
    assert(opunpackoutline(hpacked, &houtline_unpacked));

    // Verify nesting
    hcurrent = (*houtline_unpacked)->hsummit;

    for (int level = 0; level < 10; level++) {
        hcurrent = opnthsubhead(hcurrent, 1);
        assert(hcurrent != NULL);
        assert(ophasrefcon(hcurrent));

        uint32_t refcon_data = 0;
        assert(opgetrefcon(hcurrent, &refcon_data, 4));
        assert(refcon_data == (uint32_t)(0x10000000 + level));
    }

    // Cleanup
    disposehandle(hpacked);
    opdisposeoutline(houtline_original, false);
    opdisposeoutline(houtline_unpacked, false);
}
```

**Success Criteria**:
- ✅ 10-level nesting packs/unpacks
- ✅ All refcons preserved at all levels
- ✅ No stack overflow

---

## Test Fixture Requirements

### Required V6 Test Fixtures

Create these v6 databases for migration testing:

1. **`refcon_test_v6.root`**
   - `workspace.test_outline` → 1 headline with 64-byte refcon (pattern 0x00-0x3F)

2. **`refcon_multiple_v6.root`**
   - `workspace.multi_refcon_outline` → 4 headlines (0, 16, 256, 2048 byte refcons)

3. **`refcon_nested_v6.root`**
   - `workspace.nested_refcon_outline` → Nested structure (see Test 2.3/3.3)

4. **`refcon_endian_v6.root`**
   - `workspace.endian_test_outline` → 1 headline with 8-byte LE integer refcon

5. **`refcon_picture_ref_v6.root`**
   - `workspace.picture_external` → Picture external
   - `workspace.outline_with_picture_ref` → Outline with refcon referencing picture

6. **`refcon_menu_v6.root`**
   - `workspace.menu_outline` → Menu with refcon containing linkedscript
   - `workspace.script_outline` → Script referenced by menu

### Fixture Creation Script

**Script**: `tests/create_refcon_fixtures.c`

```c
// Pseudo-code for fixture creation
void create_refcon_fixtures(void) {
    create_refcon_test_v6();
    create_refcon_multiple_v6();
    create_refcon_nested_v6();
    create_refcon_endian_v6();
    create_refcon_picture_ref_v6();
    create_refcon_menu_v6();
}
```

---

## Test Execution Plan

### Phase 1: Refcon Fundamentals (Week 1)
- **Day 1-2**: Implement Tests 1.1-1.4 (simple refcon operations)
- **Day 3**: Run tests, fix any failures
- **Day 4-5**: Code review + documentation

### Phase 2: Refcon Serialization (Week 2)
- **Day 1-2**: Implement Tests 2.1-2.3 (pack/unpack)
- **Day 3**: Implement Test 2.4 (packed table refcon)
- **Day 4-5**: Run tests, fix failures, validation

### Phase 3: Refcon Migration (Week 3)
- **Day 1**: Create v6 test fixtures
- **Day 2-3**: Implement Tests 3.1-3.3 (migration)
- **Day 4**: Implement Test 3.4 (endianness)
- **Day 5**: Run migration tests, analyze failures

### Phase 4: Refcon Reference Integrity (Week 4)
- **Day 1-2**: Implement Tests 4.1-4.2 (references)
- **Day 3-4**: Design decision on dbaddress-in-refcon problem
- **Day 5**: Implement solution + retest

### Phase 5: Error Handling (Week 5)
- **Day 1-2**: Implement Tests 5.1-5.3 (error scenarios)
- **Day 3-5**: Edge case testing + validation

### Phase 6: External Integration (Week 6)
- **Day 1-2**: Implement Tests 6.1-6.3 (menu/wptext/picture)
- **Day 3-5**: Integration testing + fixes

### Phase 7: Performance (Week 7)
- **Day 1-2**: Implement Tests 7.1-7.2 (stress tests)
- **Day 3-5**: Performance profiling + optimization

---

## Success Metrics

### Coverage Targets
- ✅ **Refcon API**: 100% coverage of opsetrefcon/opgetrefcon/ophasrefcon/opemptyrefcon
- ✅ **Refcon Serialization**: 100% coverage of pack/unpack paths
- ✅ **Refcon Migration**: 100% coverage of v6→v7 refcon migration
- ✅ **Error Handling**: 95% coverage of error paths

### Quality Targets
- ✅ **Data Integrity**: 100% byte-for-byte refcon preservation for opaque blobs
- ✅ **Migration Success**: 100% refcon survival rate in v6→v7 migration
- ✅ **No Crashes**: 0 crashes on corrupted/invalid refcons
- ✅ **No Memory Leaks**: 0 leaks detected by valgrind

### Performance Targets
- ✅ **Pack Time**: <5 seconds for 1000 headlines with refcons
- ✅ **Unpack Time**: <5 seconds for 1000 headlines with refcons
- ✅ **Memory Usage**: <100MB for 1000 headlines with 64-byte refcons

---

## Risk Assessment

### Critical Risks (P0 - BLOCKING)

1. **Refcon dbaddress Migration**
   - **Risk**: Refcons containing dbaddress values are NOT updated during migration
   - **Impact**: References to externals become invalid after migration
   - **Mitigation**: Force externals to memory (flinmemory=1) OR implement refcon-aware migration hooks
   - **Status**: UNRESOLVED - requires design decision

2. **Refcon Endianness**
   - **Risk**: Refcons with multi-byte integers are NOT endian-swapped
   - **Impact**: Applications storing LE integers in refcons on BE systems get corrupted data
   - **Mitigation**: Document that refcons are opaque - applications handle endianness
   - **Status**: DESIGN DECISION NEEDED

3. **Refcon Callback Invocation**
   - **Risk**: `releaserefconcallback` may not be invoked correctly during migration
   - **Impact**: Linked resources (scripts, pictures) not disposed, causing leaks
   - **Mitigation**: Verify callback invocation paths during migration
   - **Status**: TESTING REQUIRED

### High Risks (P1)

4. **Refcon Size Limits**
   - **Risk**: Very large refcons (>1MB) may cause memory issues
   - **Impact**: Out-of-memory errors during pack/unpack
   - **Mitigation**: Test with large refcons, add size limits if needed
   - **Status**: TESTING REQUIRED

5. **Nested Refcon References**
   - **Risk**: Refcons containing references to other refcons (indirect chains)
   - **Impact**: Reference chains may break during migration
   - **Mitigation**: Document limitations, test common patterns
   - **Status**: TESTING REQUIRED

### Medium Risks (P2)

6. **Refcon Performance**
   - **Risk**: Large numbers of refcons slow down pack/unpack
   - **Impact**: Slow migration for large databases
   - **Mitigation**: Performance profiling + optimization
   - **Status**: MONITORING

---

## Mitigation Strategies

### Refcon dbaddress Problem - Solution Options

**Option 1: Force Externals to Memory**
- During migration, force all externals with refcons to memory (flinmemory=1)
- No dbaddress in refcons after migration
- Pros: Simple, safe
- Cons: Increased memory usage

**Option 2: Refcon Migration Hooks**
- Add `refconmigratecallback` to outline callbacks
- Callback receives old refcon, returns updated refcon
- Allows application-specific address updating
- Pros: Flexible, correct
- Cons: Complex, requires per-application code

**Option 3: Known Refcon Formats Registry**
- Maintain registry of known refcon structures (e.g., tymenuiteminfo)
- Migrator knows how to update addresses in known formats
- Pros: Automatic, correct for known types
- Cons: Fragile, doesn't handle custom refcon formats

**Recommended**: Start with Option 1 (force to memory), add Option 2 (hooks) later if needed.

---

## Implementation Checklist

### Phase 1: Refcon Fundamentals
- [ ] Test 1.1: Simple binary blob
- [ ] Test 1.2: Structured data
- [ ] Test 1.3: Empty/NULL handling
- [ ] Test 1.4: Size mismatch handling

### Phase 2: Refcon Serialization
- [ ] Test 2.1: Single headline pack/unpack
- [ ] Test 2.2: Multiple headlines (mixed sizes)
- [ ] Test 2.3: Nested structure
- [ ] Test 2.4: Packed table value (menu-style)

### Phase 3: Refcon Migration
- [ ] Create v6 test fixtures
- [ ] Test 3.1: Simple blob migration
- [ ] Test 3.2: Multiple refcons migration
- [ ] Test 3.3: Nested refcons migration
- [ ] Test 3.4: Endianness migration

### Phase 4: Refcon Reference Integrity
- [ ] Design decision on dbaddress-in-refcon
- [ ] Test 4.1: Picture reference migration
- [ ] Test 4.2: Outline reference migration (menu)

### Phase 5: Error Handling
- [ ] Test 5.1: Corrupted/truncated refcon
- [ ] Test 5.2: Oversized refcon (>1MB)
- [ ] Test 5.3: NULL handle corruption

### Phase 6: External Integration
- [ ] Test 6.1: Menu external with refcons
- [ ] Test 6.2: WPText external (no refcons)
- [ ] Test 6.3: Picture external (no refcons)

### Phase 7: Performance
- [ ] Test 7.1: 1000 headlines with refcons
- [ ] Test 7.2: Deep nesting (10 levels)

### Documentation
- [ ] Refcon architecture documentation
- [ ] Migration strategy documentation
- [ ] Test results report
- [ ] Known limitations document

---

## Effort Estimate

### Development Time
- **Phase 1 (Fundamentals)**: 5 days
- **Phase 2 (Serialization)**: 7 days
- **Phase 3 (Migration)**: 10 days (includes fixture creation)
- **Phase 4 (References)**: 7 days (includes design decision)
- **Phase 5 (Error Handling)**: 5 days
- **Phase 6 (External Integration)**: 7 days
- **Phase 7 (Performance)**: 5 days
- **Documentation**: 4 days

**Total**: 50 days (~10 weeks)

### Priority Breakdown
- **P0 (BLOCKING)**: Phases 1-4 = 29 days (6 weeks)
- **P1 (HIGH)**: Phases 5-6 = 12 days (2.5 weeks)
- **P2 (MEDIUM)**: Phase 7 = 5 days (1 week)
- **Documentation**: 4 days (ongoing)

---

## Definition of Done

Refcon testing is considered COMPLETE when:

1. ✅ All P0 tests passing (Phases 1-4)
2. ✅ All P1 tests passing (Phases 5-6)
3. ✅ No memory leaks detected (valgrind clean)
4. ✅ No crashes on error scenarios
5. ✅ Migration design decision documented + implemented
6. ✅ Test coverage >95% for refcon code paths
7. ✅ Performance targets met (5-second pack/unpack for 1000 refcons)
8. ✅ All test fixtures created and validated
9. ✅ Comprehensive documentation completed
10. ✅ Code review completed + approved

---

## Related Documentation

- `Common/headers/op.h` - Refcon data structures
- `Common/source/oprefcon.c` - Refcon API implementation
- `Common/source/oppack_v7.c` - Refcon serialization (v7)
- `Common/source/legacy/oppack_legacy.c` - Refcon serialization (v6)
- `Common/source/menupack.c` - Menu refcon patterns
- `Common/source/tableformats.c` - Table browser refcon patterns
- `docs/external_table_variable_management.md` - Address format migration
- `planning/phase3/MIGRATION_VALIDATION_REPORT.md` - Migration testing

---

## Appendix: Refcon Code Paths

### Set Refcon
```
opsetrefcon(hnode, pdata, len)
  ├─→ Get current refcon handle
  ├─→ If NULL: allocate new handle
  ├─→ If size mismatch: resize handle
  └─→ Copy pdata to handle
```

### Get Refcon
```
opgetrefcon(hnode, pdata, len)
  ├─→ Get refcon handle
  ├─→ If NULL: zero buffer, return false
  ├─→ If size < len: partial copy + zero pad
  ├─→ If size > len: truncate copy
  └─→ Copy handle to pdata
```

### Pack Refcon
```
outtablevisit(hnode)
  ├─→ Get (**hnode).hrefcon
  ├─→ Get lenrefcon = gethandlesize(hrefcon)
  ├─→ Write tylinetableitem.flags (2 bytes)
  ├─→ Write tylinetableitem.lenrefcon (4 bytes, BE)
  └─→ Write refcon data (lenrefcon bytes, raw)
```

### Unpack Refcon
```
intablevisit(hnode)
  ├─→ Read tylinetableitem.flags (2 bytes)
  ├─→ Read tylinetableitem.lenrefcon (4 bytes, BE)
  ├─→ If lenrefcon > 0:
  │     ├─→ Allocate new handle (lenrefcon bytes)
  │     ├─→ Read refcon data (lenrefcon bytes)
  │     └─→ Set (**hnode).hrefcon = handle
  └─→ If lenrefcon == 0: hrefcon remains NULL
```

### Release Refcon
```
opdefaultreleaserefconroutine(hnode, fldisk)
  └─→ No-op (default callback)

mereleaserefconroutine(hnode, fldisk)
  ├─→ Extract tymenuiteminfo from refcon
  ├─→ opdisposeoutline(linkedscript.houtline, fldisk)
  └─→ If fldisk: dbpushreleasestack(linkedscript.adrlink)
```

---

## Change Log

- **2025-12-24**: Initial comprehensive refcon testing plan created
- Addressed critical oversight in previous external object testing
- Added 7 phases covering fundamentals → performance
- Specified 24 detailed test cases with exact code
- Identified critical migration risks (dbaddress-in-refcon problem)
- Estimated 50 days (10 weeks) for complete implementation
- Defined success metrics and completion criteria

---

**END OF PLAN**
