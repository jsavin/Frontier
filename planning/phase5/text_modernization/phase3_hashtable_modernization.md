# Phase 3: Hashtable Modernization

**Status**: Planned
**Risk**: Medium
**Breakage**: None (in-memory only)
**Depends On**: Nothing (independent workstream)
**Can Parallel With**: Phases 1 and 2

---

## Goal

Replace Frontier's hashtable algorithm with a modern implementation that distributes keys properly and scales to large tables. This phase changes only the **in-memory** hashtable behavior — the on-disk format remains unchanged until Phase 5 (Database Format v8).

---

## Problem Analysis

### Current Hash Function

```c
// langhash.c:1339-1364
short hashfunction (const bigstring bs) {
    register unsigned short len = stringlength (bs);
    if (len == 0)
        return (0);

    register unsigned short val;
    val = getlower(getstringcharacter(bs, 0));      // First character only
    val += getlower(getstringcharacter(bs, len-1));  // Last character only

    return (val % ctbuckets);  // ctbuckets = 11
}
```

**Three fundamental problems**:

1. **Only two characters examined**: All middle characters are ignored. `"a_______z"` and `"az"` hash identically.

2. **Fixed 11 buckets**: No dynamic resizing. A table with 1,000 entries has average chain length ~90. Lookups degrade from O(1) to O(n).

3. **Pathological clustering for common patterns**:

#### Padded Numbers

Keys like `0000001` through `0000009` all share first character `'0'` (ASCII 48). Distribution:

| Key | First | Last | Sum | Bucket (% 11) |
|-----|-------|------|-----|----------------|
| 0000001 | 48 | 49 | 97 | 9 |
| 0000002 | 48 | 50 | 98 | 10 |
| 0000003 | 48 | 51 | 99 | 0 |
| ... | 48 | ... | ... | sequential |
| 0000010 | 48 | 48 | 96 | 8 |
| 0000011 | 48 | 49 | 97 | 9 (collision!) |

Every 11th padded number collides. For thousands of padded entries, chains grow linearly.

#### Prefixed Identifiers

All keys starting with the same prefix and ending with the same suffix hash identically:
- `testFunction`, `testFraction` → same bucket (both start with `t`, end with `n`)
- `item001`, `item011`, `item021` → same bucket (all start with `i`, end with `1`)

---

## Proposed Solution

### 1. FNV-1a Hash Algorithm

Replace the two-character hash with FNV-1a, which examines every byte:

```c
uint64_t fnv1a_hash(const unsigned char *data, size_t len) {
    uint64_t hash = 0xcbf29ce484222325ULL;  // FNV offset basis
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)data[i];
        hash *= 0x100000001b3ULL;            // FNV prime
    }
    return hash;
}
```

**Why FNV-1a**:
- Excellent distribution for short strings (identifiers are typically <50 bytes)
- Simple implementation (no dependencies)
- Well-studied avalanche properties
- Fast: single multiply + XOR per byte

**Case-insensitive hashing**: Apply `tolower()` (or the existing `getlower()` macro) to each byte before hashing, preserving Frontier's case-insensitive identifier semantics.

### 2. Dynamic Bucket Sizing

Replace the fixed `hashbucket[11]` array with a dynamically-sized array:

```c
typedef struct tyhashtable {
    hdlhashnode *hashbucket;           // Dynamic array (was: hashbucket[ctbuckets])
    unsigned short bucket_count;        // Current bucket count
    unsigned long item_count;           // Number of items

    hdlhashnode hfirstsort;            // Sorted list (unchanged)
    // ... remaining fields unchanged
} tyhashtable;
```

**Sizing strategy**:
- **Initial size**: 11 buckets (backward compatible for small tables)
- **Growth**: Double when load factor > 0.75
- **Shrink**: Halve when load factor < 0.25 (minimum 11)
- **Bucket counts**: Always use prime or power-of-2 sizes for good modular distribution

**Rehash on resize**: When bucket count changes, iterate all nodes and redistribute. This is O(n) but happens infrequently with good load factor thresholds.

### 3. Backward-Compatible Loading

When a v7 database is opened:
1. Read the existing 11-bucket structure from disk
2. Load all hash nodes into memory
3. **Rehash** all nodes using FNV-1a into a dynamically-sized bucket array
4. From this point on, in-memory lookups use the modern algorithm

When saving back to v7 format:
1. Redistribute nodes back into 11 buckets using the legacy hash function
2. Write in the existing v7 on-disk format

This means Phase 3 is **fully backward compatible** — databases open and save in the same format, but in-memory operations are faster.

---

## Performance Impact

| Table Size | Legacy (avg chain) | Modern (avg chain) | Speedup |
|-----------|-------------------|-------------------|---------|
| 10 items | ~1 | ~1 | Negligible |
| 100 items | ~9 | ~1-2 | ~5x |
| 1,000 items | ~90 | ~1-2 | ~50x |
| 10,000 items | ~909 | ~1-2 | ~500x |

For the common case of small tables (<50 items), performance is similar. The benefit is dramatic for large tables — and Frontier databases commonly have tables with hundreds or thousands of entries (e.g., `system.verbs.*`).

---

## Implementation Plan

### Step 1: Add FNV-1a Hash Function
- Add `fnv1a_hash()` to `langhash.c`
- Add `hashfunction_modern()` wrapper that applies case folding and modular reduction
- Keep `hashfunction()` (legacy) for v7 save compatibility

### Step 2: Make Bucket Array Dynamic
- Change `tyhashtable` to use a pointer + count instead of fixed array
- Update `newhashtable()` to allocate initial 11 buckets dynamically
- Update all code that accesses `hashbucket[]` to use the dynamic array

### Step 3: Add Resize Logic
- Implement `hashtable_resize()` with rehashing
- Call on insert when load factor exceeds threshold
- Call on delete when load factor drops below threshold

### Step 4: Add Rehash-on-Load
- After loading a table from disk (11 legacy buckets), rehash into FNV-1a with appropriate bucket count
- Before saving to v7 format, redistribute back into 11 legacy buckets

### Step 5: Performance Benchmarks
- Benchmark with real-world Frontier databases (system.verbs, user tables)
- Benchmark with pathological cases (padded numbers, sequential names)
- Verify no regression for small tables

---

## Key Files

| File | Changes |
|------|---------|
| `Common/source/langhash.c` | New hash function, dynamic buckets, resize logic |
| `Common/headers/lang.h` | `tyhashtable` struct modification, new function declarations |
| `Common/source/tablepack.c` | Rehash on load, redistribute on save |
| `Common/source/langexternal.c` | May need updates for table materialization |

---

## Verification

- All existing tests pass (the legacy-compatible load/save means zero behavioral change at the API level)
- New benchmarks demonstrate improved distribution for padded numbers and prefixed identifiers
- Memory usage for small tables is comparable (dynamic 11 buckets ≈ fixed 11 buckets plus pointer overhead)
- Round-trip test: open v7 database → modify tables → save → reopen → verify all data intact

---

## Risks

**Medium**. The `tyhashtable` struct change touches a core data structure used throughout the codebase. Key risks:

1. **Struct layout change**: Any code that assumes `hashbucket` is an inline array will break. Must audit all direct struct access.
2. **Memory management**: Dynamic bucket array must be freed on table deallocation. Missing a free path causes leaks.
3. **Rehash correctness**: If rehash-on-load or redistribute-on-save has a bug, data corruption is possible. Comprehensive round-trip tests are essential.
4. **Performance regression for small tables**: The dynamic allocation overhead must be negligible. Benchmark carefully.

Mitigation: This phase can be developed behind a compile-time flag (`FRONTIER_MODERN_HASH`) until fully validated.
