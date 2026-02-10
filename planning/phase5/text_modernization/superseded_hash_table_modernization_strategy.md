> **SUPERSEDED** — This document has been absorbed into the consolidated
> [Text Modernization Roadmap](README.md). Retained for historical reference.
> Originally at `planning/phase2/0.5.16_hash_table_modernization_strategy.md`.

# Hash Table Modernization Strategy

Status
- State: Planned/Deferred
- Phase: Future (Post-Phase 3)
- Last Updated: 2025-09-29
- Notes: Scheduled after Phase 2 UI/runtime separation. **NOT ACTIVE** - See active phase 3 work in kernel_verb_porting/ and carbon_migration/.

Related Docs
- planning/adr/0001-hash-tables-phase-3.md
- planning/Frontier_Refactoring_Plan.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

## **Problem Analysis**

### **Current Hash Table Implementation**

**Hash Function (Current):**
```c
short hashfunction (const bigstring bs) {
    register unsigned short len;
    register unsigned short val;

    len = stringlength (bs);
    if (len == 0)
        return (0);

    val = getlower(getstringcharacter(bs,0));  // First character
    val += getlower(getstringcharacter(bs,len-1));  // Last character

    return (val % ctbuckets);  // 11 buckets
}
```

**Current Structure:**
```c
#define ctbuckets 11  // Fixed 11 buckets
typedef struct tyhashtable {
    hdlhashnode hashbucket[ctbuckets];  // Fixed array of 11 buckets
    hdlhashnode hfirstsort;             // Sorted list pointer
    struct tyhashtable **prevhashtable; // Previous table pointer
    struct tyhashtable **parenthashtable; // Parent table pointer
    hdlhashnode thistableshashnode;     // This table's hash node
    // ... additional fields
} tyhashtable;
```

### **Critical Problems**

**1. Poor Hash Distribution:**
- **Algorithm**: Only uses first and last characters
- **Problem**: Numeric names like "001", "002", "003" all hash to same bucket
- **Impact**: O(n) performance instead of O(1) for large tables

**2. Fixed Bucket Count:**
- **Current**: Always 11 buckets regardless of table size
- **Problem**: No scaling with table size
- **Impact**: Performance degrades linearly with table size

**3. Collision Resolution:**
- **Current**: Simple linked list in each bucket
- **Problem**: No optimization for collision chains
- **Impact**: Linear search through collision chains

## **Proposed Solution**

### **1. Modern Hash Algorithm**

**FNV-1a Hash Function:**
```c
// 64-bit FNV-1a hash function
uint64_t fnv1a_hash(const bigstring bs) {
    uint64_t hash = 0xcbf29ce484222325ULL;  // FNV offset basis
    uint64_t fnv_prime = 0x100000001b3ULL;   // FNV prime

    register unsigned short len = stringlength(bs);
    for (register unsigned short i = 0; i < len; i++) {
        hash ^= (uint8_t)getstringcharacter(bs, i);
        hash *= fnv_prime;
    }

    return hash;
}
```

**Improved Hash Function:**
```c
short hashfunction_modern(const bigstring bs) {
    uint64_t hash = fnv1a_hash(bs);
    return (short)(hash % current_bucket_count);
}
```

### **2. Dynamic Bucket Sizing**

**Dynamic Bucket Structure:**
```c
typedef struct tyhashtable_modern {
    // Dynamic bucket array
    hdlhashnode *hashbucket;           // Dynamic array
    unsigned short bucket_count;        // Current bucket count
    unsigned short max_bucket_count;    // Maximum buckets allowed
    unsigned long item_count;           // Number of items in table

    // Sorted list (unchanged)
    hdlhashnode hfirstsort;

    // Table hierarchy (unchanged)
    struct tyhashtable_modern **prevhashtable;
    struct tyhashtable_modern **parenthashtable;
    hdlhashnode thistableshashnode;

    // Additional fields...
} tyhashtable_modern;
```

**Bucket Resizing Logic:**
```c
// Resize buckets when load factor exceeds threshold
boolean resize_hash_buckets(hdlhashtable_modern htable) {
    float load_factor = (float)htable->item_count / htable->bucket_count;

    if (load_factor > 0.75) {  // Expand when 75% full
        return expand_buckets(htable);
    } else if (load_factor < 0.25 && htable->bucket_count > 11) {  // Shrink when 25% full
        return shrink_buckets(htable);
    }

    return true;
}
```

### **3. Improved Collision Resolution**

**Separate Chaining with Optimization:**
```c
typedef struct tyhashbucket_modern {
    hdlhashnode first_node;     // First node in bucket
    unsigned short node_count;  // Number of nodes in bucket
    boolean is_optimized;       // Whether bucket is optimized
} tyhashbucket_modern;
```

**Bucket Optimization:**
```c
// Optimize bucket when chain gets too long
boolean optimize_bucket(hdlhashnode bucket_head) {
    if (bucket_head == nil) return true;

    // Count nodes in chain
    unsigned short count = 0;
    hdlhashnode current = bucket_head;
    while (current != nil) {
        count++;
        current = (**current).hashlink;
    }

    // If chain is too long, consider it for optimization
    if (count > 8) {
        return implement_bucket_optimization(bucket_head);
    }

    return true;
}
```

## **Database Format Impact**

### **1. On-Disk Storage Changes**

**Current Table Storage:**
```c
typedef struct tydisktablerecord {
    short version;                    // 2 bytes
    short sortorder;                  // 2 bytes
    unsigned long timecreated;        // 4 bytes
    unsigned long timelastsave;       // 4 bytes
    long flags;                       // 4 bytes
    // Fixed 11 buckets stored implicitly
} tydisktablerecord;
```

**New Table Storage:**
```c
typedef struct tydisktablerecord_modern {
    short version;                    // 2 bytes
    short sortorder;                  // 2 bytes
    unsigned long timecreated;        // 4 bytes
    unsigned long timelastsave;       // 4 bytes
    long flags;                       // 4 bytes
    unsigned short bucket_count;      // 2 bytes - NEW
    unsigned short max_bucket_count;  // 2 bytes - NEW
    unsigned long item_count;         // 4 bytes - NEW
    // Dynamic bucket array stored separately
} tydisktablerecord_modern;
```

### **2. Version Compatibility**

**Database Version Strategy:**
```c
// Version 6: Legacy hash tables (11 buckets)
// Version 7: 64-bit addresses + legacy hash tables
// Version 8: 64-bit addresses + modern hash tables
```

**Version Detection:**
```c
if ((**hdb).versionnumber <= 6) {
    // Use legacy 32-bit format with 11 buckets
    use_legacy_format = true;
    use_legacy_hash = true;
} else if ((**hdb).versionnumber == 7) {
    // Use 64-bit format with legacy hash
    use_legacy_format = false;
    use_legacy_hash = true;
} else if ((**hdb).versionnumber >= 8) {
    // Use 64-bit format with modern hash
    use_legacy_format = false;
    use_legacy_hash = false;
}
```

### **3. Migration Strategy**

**Legacy to Modern Hash Migration:**
```c
boolean migrate_hash_table(hdlhashtable old_table, hdlhashtable_modern new_table) {
    // 1. Create new table with modern structure
    if (!newhashtable_modern(&new_table)) return false;

    // 2. Copy all items from old table to new table
    return hashtablevisit(old_table, migrate_item_visit, new_table);
}

static boolean migrate_item_visit(hdlhashtable htable, langtablevisitcallback visit, ptrvoid refcon) {
    hdlhashtable_modern new_table = (hdlhashtable_modern)refcon;

    // For each item in old table
    bigstring item_name;
    tyvaluerecord item_value;

    if (hashgetiteminfo(htable, current_item, item_name, &item_value)) {
        // Insert into new table with modern hash
        return hashtableassign_modern(new_table, item_name, item_value);
    }

    return true;
}
```

## **Implementation Plan**

### **Phase 1: Hash Algorithm Modernization (Week 1)**

**1.1 Implement FNV-1a Hash Function**
```c
// Add to langhash.c
uint64_t fnv1a_hash(const bigstring bs);
short hashfunction_modern(const bigstring bs);
```

**1.2 Create Modern Hash Table Structure**
```c
// Add to lang.h
typedef struct tyhashtable_modern {
    hdlhashnode *hashbucket;
    unsigned short bucket_count;
    unsigned short max_bucket_count;
    unsigned long item_count;
    // ... other fields
} tyhashtable_modern;
```

**1.3 Implement Dynamic Bucket Management**
```c
// Add to langhash.c
boolean newhashtable_modern(hdlhashtable_modern *htable);
boolean resize_hash_buckets(hdlhashtable_modern htable);
boolean expand_buckets(hdlhashtable_modern htable);
boolean shrink_buckets(hdlhashtable_modern htable);
```

### **Phase 2: Database Format Updates (Week 2)**

**2.1 Update Database Header Structure**
```c
// Modify tydatabaserecord to support version 8
typedef struct tydatabaserecord_v8 {
    // ... existing fields
    unsigned char hash_table_version;  // NEW: 6=legacy, 7=64bit, 8=modern
} tydatabaserecord_v8;
```

**2.2 Update Table Storage Format**
```c
// Modify tydisktablerecord for modern hash tables
typedef struct tydisktablerecord_modern {
    // ... existing fields
    unsigned short bucket_count;      // NEW
    unsigned short max_bucket_count;  // NEW
    unsigned long item_count;         // NEW
} tydisktablerecord_modern;
```

**2.3 Implement Version Detection**
```c
// Add to dbopenfile()
if ((**hdb).hash_table_version >= 8) {
    use_modern_hash = true;
} else {
    use_modern_hash = false;
}
```

### **Phase 3: Migration Infrastructure (Week 3)**

**3.1 Create Migration Functions**
```c
// Add to langhash.c
boolean migrate_legacy_to_modern_hash(hdlhashtable old_table, hdlhashtable_modern new_table);
boolean migrate_modern_to_legacy_hash(hdlhashtable_modern old_table, hdlhashtable new_table);
```

**3.2 Implement Automatic Migration**
```c
// Add to database save operations
if (use_legacy_hash && should_upgrade_to_modern) {
    return migrate_and_save_with_modern_hash(hdb);
}
```

**3.3 Add Migration Dialog**
```c
// User confirmation for hash table upgrade
boolean offer_hash_migration_dialog(const char* db_path) {
    // "Upgrade hash tables for better performance?"
    // "This will improve performance with large tables"
    return user_confirms_migration;
}
```

### **Phase 4: Testing and Validation (Week 4)**

**4.1 Performance Testing**
```c
// Test with large tables
- Create table with 10,000 items
- Measure lookup performance
- Compare legacy vs modern hash performance
```

**4.2 Compatibility Testing**
```c
// Test migration in both directions
- Legacy → Modern migration
- Modern → Legacy migration
- Mixed database operations
```

**4.3 UserTalk Compatibility**
```c
// Test UserTalk operations with new hash tables
- Table creation and deletion
- Item insertion and lookup
- Cross-database references
```

## **Performance Benefits**

### **1. Hash Distribution**

**Before (Legacy):**
```
"001" → hash(0 + 1) % 11 = 1
"002" → hash(0 + 2) % 11 = 2
"003" → hash(0 + 3) % 11 = 3
"101" → hash(1 + 1) % 11 = 2  // Collision!
"201" → hash(2 + 1) % 11 = 3  // Collision!
```

**After (Modern):**
```
"001" → fnv1a_hash("001") % buckets = 7
"002" → fnv1a_hash("002") % buckets = 3
"003" → fnv1a_hash("003") % buckets = 9
"101" → fnv1a_hash("101") % buckets = 2  // No collision!
"201" → fnv1a_hash("201") % buckets = 5  // No collision!
```

### **2. Performance Improvements**

**Expected Performance Gains:**
- **Small tables (< 100 items)**: Minimal impact
- **Medium tables (100-1000 items)**: 2-5x faster lookups
- **Large tables (1000+ items)**: 10-50x faster lookups
- **Very large tables (10000+ items)**: 100x+ faster lookups

### **3. Memory Usage**

**Memory Impact:**
- **Small tables**: Slightly more memory (dynamic allocation overhead)
- **Large tables**: More efficient memory usage (better distribution)
- **Overall**: Better memory locality due to improved distribution

## **Backward Compatibility**

### **1. File Format Compatibility**

**Version Strategy:**
- **Version 6**: Legacy 32-bit + 11 buckets
- **Version 7**: 64-bit + 11 buckets (transitional)
- **Version 8**: 64-bit + modern hash tables

### **2. Migration Path**

**Automatic Migration:**
1. **Open legacy database**: Detect version 6
2. **Offer upgrade**: "Upgrade to modern hash tables?"
3. **Migrate data**: Convert to version 8 format
4. **Save with backup**: Create .rbk backup

### **3. Error Handling**

**Legacy Runtime Compatibility:**
```c
// Old Frontier opening new format
if ((**hdb).versionnumber >= 8) {
    shellerrormessage("This database uses modern hash tables. Please upgrade to Frontier 10.0 or later.");
    goto error;
}
```

This strategy provides a complete modernization of the hash table system while maintaining full backward compatibility and providing significant performance improvements for large tables.
