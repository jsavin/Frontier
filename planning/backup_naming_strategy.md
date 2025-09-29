# Backup Naming Strategy Analysis

Status
- State: Unknown
- Phase: Unknown
- Last Updated: 2025-09-29
- Notes: To be aligned with migration/CLI behavior in Phases 1–2.

Related Docs

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

## **Problem**
When creating a backup during database migration, what happens if a `.rbk` file already exists?

## **Current Implementation**
```c
boolean create_root_backup(const char* original_path) {
    char backup_path[1024];
    snprintf(backup_path, sizeof(backup_path), "%s.rbk", original_path);
    // ... copy file
}
```

**Issues:**
- ❌ **Overwrites existing backups**: Previous backups are lost
- ❌ **No versioning**: Can't distinguish between different backup attempts
- ❌ **No timestamps**: Can't tell when backup was created
- ❌ **No user control**: User can't choose to preserve or overwrite

## **Proposed Solutions**

### **Option 1: Timestamp-based Naming**
```c
// Format: filename.root.rbk.YYYYMMDD_HHMMSS
char backup_path[1024];
time_t now = time(NULL);
struct tm *tm_info = localtime(&now);
snprintf(backup_path, sizeof(backup_path), "%s.rbk.%04d%02d%02d_%02d%02d%02d", 
         original_path, 
         tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
         tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
```

**Pros:**
- ✅ **Unique names**: Each backup has unique timestamp
- ✅ **Chronological order**: Easy to sort by creation time
- ✅ **No conflicts**: Never overwrites existing backups
- ✅ **Human readable**: Easy to identify when backup was created

**Cons:**
- ❌ **Long filenames**: Can get unwieldy
- ❌ **Many files**: Could accumulate many backups over time

### **Option 2: Incremental Naming**
```c
// Format: filename.root.rbk.1, filename.root.rbk.2, etc.
int counter = 1;
char backup_path[1024];
do {
    snprintf(backup_path, sizeof(backup_path), "%s.rbk.%d", original_path, counter);
    counter++;
} while (access(backup_path, F_OK) == 0); // File exists
```

**Pros:**
- ✅ **Simple names**: Short, clean filenames
- ✅ **Sequential**: Easy to understand order
- ✅ **No conflicts**: Never overwrites existing backups

**Cons:**
- ❌ **No timestamp**: Can't tell when backup was created
- ❌ **Gaps**: If user deletes backup.2, next backup becomes .3

### **Option 3: Hybrid Approach**
```c
// Format: filename.root.rbk.YYYYMMDD_HHMMSS_N
// Where N is increment if timestamp collision
char backup_path[1024];
time_t now = time(NULL);
struct tm *tm_info = localtime(&now);
int counter = 1;
do {
    snprintf(backup_path, sizeof(backup_path), "%s.rbk.%04d%02d%02d_%02d%02d%02d_%d", 
             original_path, 
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, counter);
    counter++;
} while (access(backup_path, F_OK) == 0);
```

**Pros:**
- ✅ **Timestamp + increment**: Best of both worlds
- ✅ **Unique names**: Never conflicts
- ✅ **Chronological**: Easy to sort and understand
- ✅ **Handles collisions**: Multiple backups in same second

**Cons:**
- ❌ **Complex**: More complex implementation
- ❌ **Long names**: Can get very long with many increments

### **Option 4: User Choice**
```c
// Check if backup exists and ask user
if (access(backup_path, F_OK) == 0) {
    // Show dialog: "Backup already exists. Overwrite?"
    // Options: "Overwrite", "Create new backup", "Cancel"
}
```

**Pros:**
- ✅ **User control**: User decides what to do
- ✅ **Flexible**: Can handle any scenario
- ✅ **Safe**: No accidental overwrites

**Cons:**
- ❌ **UI dependency**: Requires user interaction
- ❌ **Headless mode**: Doesn't work in automated scenarios
- ❌ **Complex**: Need to handle user choices

## **Recommended Solution: Option 1 (Timestamp-based)**

### **Implementation**
```c
boolean create_root_backup(const char* original_path) {
    char backup_path[1024];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    // Create timestamped backup name
    snprintf(backup_path, sizeof(backup_path), "%s.%04d%02d%02d_%02d%02d%02d", 
             original_path, 
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    
    // Copy file to backup
    FILE *src = fopen(original_path, "rb");
    FILE *dst = fopen(backup_path, "wb");
    
    if (!src || !dst) {
        if (src) fclose(src);
        if (dst) fclose(dst);
        return false;
    }
    
    // Copy file contents
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }
    
    fclose(src);
    fclose(dst);
    return true;
}
```

### **Benefits**
- ✅ **Never overwrites**: Each backup is unique
- ✅ **Chronological**: Easy to sort and identify
- ✅ **Human readable**: Clear timestamp format
- ✅ **No user interaction**: Works in headless mode
- ✅ **Simple implementation**: Easy to understand and maintain
- ✅ **Clean naming**: No redundant extensions

### **Example Output**
```
database.root.20241215_143022
database.root.20241215_143045
database.root.20241215_143123
```

### **Backup Cleanup**
For long-term maintenance, we could add a cleanup function:
```c
// Optional: Clean up old backups (keep last N backups)
void cleanup_old_backups(const char* original_path, int keep_count) {
    // Find all .rbk files for this database
    // Sort by timestamp
    // Keep only the most recent N backups
    // Delete older backups
}
```

This approach provides the best balance of safety, usability, and simplicity for the migration process.
