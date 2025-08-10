# TODO: Future Improvements

## **Phase 2: Hash Table Modernization**

### **Priority: Medium**
### **Timeline: After Phase 1 64-bit migration is stable**

#### **Background**
- Current hash table uses only first/last character of string
- Fixed 11 buckets regardless of table size
- Poor distribution leads to performance issues with large tables

#### **Proposed Changes**
- **Version 8**: 64-bit format with modern hash tables
- **FNV-1a Hash**: Replace simple first/last character hash
- **Dynamic Buckets**: Variable bucket count based on table size
- **Load Factor**: Automatic resizing when load factor exceeds threshold

#### **Implementation Plan**
```c
// New modern hash table structure
typedef struct tyhashtable_modern {
    hdlhashnode *hashbucket;           // Dynamic array
    unsigned short bucket_count;        // Current bucket count
    unsigned short max_bucket_count;    // Maximum buckets allowed
    unsigned long item_count;           // Number of items in table
    // ... other fields
} tyhashtable_modern;

// FNV-1a hash function
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

#### **Migration Strategy**
- **Database Version 8**: New format with modern hash tables
- **Automatic Migration**: Convert from Version 7 to Version 8
- **Backward Compatibility**: Version 7 databases still supported
- **Performance Testing**: Validate performance improvements

#### **Files to Update**
- `Common/headers/lang.h` - Update hash table structures
- `Common/source/langhash.c` - Implement FNV-1a hash
- `Common/headers/dbinternal.h` - Add Version 8 constants
- `Common/source/db.c` - Add Version 8 detection and migration

---

## **Headless Migration Option**

### **Priority: Low**
### **Timeline: After Phase 1 migration is working**

#### **Background**
- Current plan requires user confirmation for database migration
- Some deployment scenarios need automated migration without user interaction
- Batch processing or server environments need headless operation

#### **Proposed Changes**
- **Configuration Option**: Add setting to enable automatic migration
- **Command Line Flag**: `--auto-migrate` for headless operation
- **Environment Variable**: `FRONTIER_AUTO_MIGRATE=1`
- **Preferences Setting**: User-configurable default behavior

#### **Implementation Plan**
```c
// Configuration options
typedef enum {
    MIGRATION_PROMPT_ALWAYS,     // Always ask user
    MIGRATION_PROMPT_NEVER,      // Never ask, auto-migrate
    MIGRATION_PROMPT_ONCE        // Ask once, remember choice
} migration_prompt_mode_t;

// Migration function with headless support
boolean offer_64bit_migration_dialog(const char* db_path, boolean headless_mode) {
    if (headless_mode) {
        return true;  // Auto-migrate without prompting
    }
    
    // Show dialog: "Upgrade database to 64-bit format?"
    // Options: "Upgrade", "Cancel", "Don't ask again"
    return user_confirms_migration();
}
```

#### **Configuration Sources**
1. **Command Line**: `--auto-migrate` flag
2. **Environment**: `FRONTIER_AUTO_MIGRATE=1`
3. **Preferences**: User setting in Frontier preferences
4. **Default**: Prompt user (current behavior)

#### **Files to Update**
- `Common/source/db.c` - Add headless migration support
- `Common/source/shell.c` - Add command line parsing
- `Common/headers/shell.h` - Add migration configuration types
- `Common/source/preferences.c` - Add migration preferences

---

## **Additional Future Considerations**

### **Performance Optimizations**
- **Memory Mapping**: Use `mmap()` for large database files
- **Compression**: Optional compression for database files
- **Caching**: Improved caching strategies for frequently accessed data

### **User Experience Enhancements**
- **Progress Indicators**: Show migration progress for large databases
- **Batch Migration**: Migrate multiple databases at once
- **Rollback Options**: Easy rollback to previous format if needed

### **Developer Experience**
- **Debugging Tools**: Better tools for database inspection
- **Validation Tools**: Comprehensive database integrity checking
- **Documentation**: Complete API documentation for database operations

---

## **Notes**

- **Priority Order**: Phase 1 (64-bit) → Phase 2 (hash tables) → Headless migration
- **Testing Strategy**: Each improvement should have comprehensive testing
- **Backward Compatibility**: Maintain support for all previous versions
- **Documentation**: Update user and developer documentation for each change

This TODO list ensures we don't lose track of important improvements while focusing on the current Phase 1 implementation.
