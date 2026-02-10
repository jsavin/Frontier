# Phase 5: Database Format v8

**Status**: Planned
**Risk**: Very High
**Breakage**: Database format change (requires migration)
**Depends On**: Phase 3 (Hashtable Modernization), Phase 4 (Core Runtime Conversion)

---

## Goal

Define and implement the v8 database format that persists both the modernized hashtable algorithm and UTF-8 string encoding. This is a single format bump that captures the combined results of the string and hashtable workstreams.

This phase extends the ADR-002 context-based format versioning pattern (`db_context`) to include encoding and hash algorithm metadata.

---

## Database Version History

| Version | Introduced | Addresses | Hash | Buckets | Strings | Key Format |
|---------|-----------|-----------|------|---------|---------|------------|
| v6 | Legacy | 32-bit LE | first+last char | 11 fixed | MacRoman | Pascal (length byte + data) |
| v7 | Current | 64-bit BE | first+last char | 11 fixed | MacRoman | Pascal (length byte + data) |
| **v8** | **This plan** | 64-bit BE | FNV-1a | Dynamic | UTF-8 | Length-prefixed UTF-8 |

---

## On-Disk Format Changes

### Database Header

Add encoding and hash metadata fields:

```c
typedef struct tydatabaserecord_v8 {
    // ... existing v7 fields ...
    unsigned char string_encoding;     // 0 = MacRoman (legacy), 1 = UTF-8
    unsigned char hash_algorithm;      // 0 = legacy (first+last), 1 = FNV-1a
    unsigned char reserved[6];         // Future use, zero-filled
} tydatabaserecord_v8;
```

### Table Record

Add dynamic bucket metadata:

```c
typedef struct tydisktablerecord_v8 {
    short version;                     // Table format version
    short sortorder;                   // Sort order
    unsigned long timecreated;         // Creation timestamp
    unsigned long timelastsave;        // Last save timestamp
    long flags;                        // Table flags
    unsigned short bucket_count;       // NEW: Number of hash buckets
    unsigned long item_count;          // NEW: Number of items in table
} tydisktablerecord_v8;
```

### Hash Node Key Format

Transition from Pascal length-prefixed to UTF-8 length-prefixed:

**v7 (current)**:
```
[1 byte: length N] [N bytes: MacRoman key data]
```
- Maximum key length: 255 bytes
- Encoding: MacRoman

**v8 (proposed)**:
```
[2 bytes: length N, big-endian] [N bytes: UTF-8 key data]
```
- Maximum key length: 65,535 bytes (though 255-byte logical limit may be retained)
- Encoding: UTF-8

**Rationale for 2-byte length**: Even though we may keep the 255-byte logical limit for identifiers, a 2-byte length field future-proofs the format and distinguishes v8 keys from v7 keys unambiguously (a v7 key's first byte is always ≤ 255, but in v8 the first two bytes form a 16-bit length).

---

## Context Extension (ADR-002)

Extend `db_context` to carry encoding and hash algorithm information:

```c
typedef struct db_format_mode {
    boolean use_64bit_format;          // Existing: v6 vs v7/v8
    boolean use_utf8_encoding;         // NEW: MacRoman vs UTF-8
    boolean use_modern_hash;           // NEW: legacy vs FNV-1a
    // ... existing fields ...
} db_format_mode;
```

All pack/unpack functions that already take `const db_context *ctx` will use these new fields to determine string encoding and hash algorithm for serialization.

---

## Migration: v7 → v8

### Automatic Migration Flow

1. **Open v7 database**: Detect `versionnumber == 7`
2. **Prompt user**: "Upgrade database to v8 format? This enables UTF-8 text and improved performance for large tables. A backup will be created."
3. **Create backup**: Copy database to `<name>.v7.rbk`
4. **Walk all tables recursively**:
   a. For each hash node key: transcode MacRoman → UTF-8, rewrite with 2-byte length prefix
   b. For each string value: transcode MacRoman → UTF-8
   c. Rehash all keys using FNV-1a
   d. Write table with dynamic bucket count based on item count
5. **Update database header**: Set `versionnumber = 8`, `string_encoding = 1`, `hash_algorithm = 1`
6. **Save and verify**: Reopen the database and spot-check key tables

### Migration Considerations

- **Binary values**: Leave untouched (opaque data, no encoding assumption)
- **Script source**: Transcode from MacRoman to UTF-8 (scripts may contain non-ASCII comments or string literals)
- **Outline text**: Transcode from MacRoman to UTF-8
- **WPText/rich text**: May contain encoding metadata — handle per-format
- **Address values**: Identifier paths are ASCII; transcode for safety

### Backward Compatibility

When an older Frontier version opens a v8 database:

```c
if ((**hdb).versionnumber >= 8) {
    shellerrormessage(BIGSTRING("\x50""This database requires Frontier 10.0 or later (UTF-8 format)."));
    return false;
}
```

### Downgrade Path

A `v8_to_v7_export` tool (Phase 7) will allow exporting a v8 database back to v7 format for users who need to revert. This involves:
- Transcoding UTF-8 → MacRoman (with fallback for characters not in MacRoman)
- Redistributing hash nodes into 11 fixed buckets with legacy hash function
- Truncating keys that exceed 255 bytes (unlikely but handle gracefully)

---

## Key Files

| File | Changes |
|------|---------|
| `Common/source/langhash.c` | v8 pack/unpack with dynamic buckets + UTF-8 keys |
| `Common/source/tablepack.c` | Table record serialization with new fields |
| `Common/source/langexternal.c` | External packing with encoding-aware context |
| `Common/source/db.c` | Database header reading/writing, version detection |
| `Common/headers/lang.h` | Struct updates for v8 format |
| `Common/headers/db.h` | Database record struct updates |

---

## Verification

- **Round-trip**: Open v7 → migrate to v8 → save → reopen → all data accessible and correct
- **Determinism**: 5 identical migrations produce identical v8 databases (md5 match)
- **Backward compat**: v8 database produces clear error when opened by older Frontier
- **Performance**: v8 database loads and saves within 10% of v7 performance for typical databases
- **Integrity**: `sizeOf()` on all top-level tables matches pre-migration values
- **String content**: Spot-check non-ASCII strings survive round-trip (accented characters, special symbols)
- **Hash distribution**: Verify FNV-1a buckets are well-distributed after migration

---

## Risks

**Very High**. Database format changes are the highest-risk operation in the project.

1. **Data corruption**: A bug in the migration walker could silently corrupt data. Backup + verification mitigate this, but the migration must be exhaustively tested.
2. **Incomplete migration**: If a table or value type is missed during the walk, the database contains mixed v7/v8 data, leading to encoding mismatches.
3. **Encoding loss**: MacRoman characters with no UTF-8 equivalent (none — MacRoman is a subset of Unicode, so all characters map) or UTF-8 characters with no MacRoman equivalent (many — handled by the downgrade tool with fallbacks).
4. **Performance**: Dynamic bucket arrays require separate allocation per table; may increase memory fragmentation. Benchmark carefully.
5. **Rollback complexity**: Once a production database is migrated to v8, rolling back requires the export tool (Phase 7). The backup `.rbk` file is the primary safety net.

**Mitigation**: Ship the migration tool as a preview/beta feature first. Require explicit user opt-in. Always create backup before migration.
