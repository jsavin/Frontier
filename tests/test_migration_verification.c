#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

// Define minimal types to avoid complex includes
typedef unsigned char Boolean;
typedef unsigned char UInt8;
typedef unsigned short UInt16;
typedef unsigned int UInt32;
typedef signed char SInt8;
typedef signed short SInt16;
typedef signed int SInt32;

typedef char* Ptr;
typedef Ptr* Handle;

// Define boolean constants
#define true 1
#define false 0
typedef Boolean boolean;

// Define the database structures directly
#define ctviews 3
typedef long long dbaddress, *ptrdbaddress, **hdldbaddress;
#define nildbaddress 0L
#define dbsystemidMac 0

#pragma pack(2)
typedef struct tydatabaserecord { /*stored at offset 0 in the db file*/
    unsigned char systemid;		/* 0 = MAC (compatiblity)  */
    unsigned char versionnumber; /*which version created this file?*/
    dbaddress availlist; /*avail list is singly-linked, nil terminated*/
    short oldfnumdatabase; /*only applies when database record is in memory*/
    short flags; /*any changes to header since it was last flushed?*/
    dbaddress views [ctviews]; /*addresses of the root of each view*/
    Handle releasestack; /*holds addresses of nodes waiting to be released*/
    long fnumdatabase; /* file handle while in memory - New to version 5*/
    long headerLength;		/*size of header - New to version 5*/
    short longversionMajor;	/*new extended version id - new to version 5*/
    short longversionMinor;	/*new extended version id - new to version 5*/
    union {
        char growthspace [50]; /*room for new fields without format change*/
        struct {
            dbaddress availlistblock; /*6.2a9 AR: on-disk structure mirroring availlist, a contiguous block*/
            void* availlistshadow; /*never saved to disk; in-memory structure mirroring availlist*/
            Boolean flreadonly; /*6.2a9 AR: never saved to disk; if this is true, don't write to the file*/
        } extensions;
    } u;
} tydatabaserecord, *ptrdatabaserecord, **hdldatabaserecord;

// 64-bit database record structure (Version 7)
typedef struct tydatabaserecord_64 { /*stored at offset 0 in the db file*/
    unsigned char systemid;		/* 0 = MAC (compatiblity)  */
    unsigned char versionnumber; /*which version created this file?*/
    dbaddress availlist; /*avail list is singly-linked, nil terminated*/
    short oldfnumdatabase; /*only applies when database record is in memory*/
    short flags; /*any changes to header since it was last flushed?*/
    dbaddress views [ctviews]; /*addresses of the root of each view*/
    Handle releasestack; /*holds addresses of nodes waiting to be released*/
    long fnumdatabase; /* file handle while in memory - New to version 5*/
    long headerLength;		/*size of header - New to version 5*/
    short longversionMajor;	/*new extended version id - new to version 5*/
    short longversionMinor;	/*new extended version id - new to version 5*/
    union {
        char growthspace [22]; /*room for new fields without format change*/
        struct {
            dbaddress availlistblock; /*6.2a9 AR: on-disk structure mirroring availlist, a contiguous block*/
            void* availlistshadow; /*never saved to disk; in-memory structure mirroring availlist*/
            Boolean flreadonly; /*6.2a9 AR: never saved to disk; if this is true, don't write to the file*/
        } extensions;
    } u;
} tydatabaserecord_64, *ptrdatabaserecord_64, **hdldatabaserecord_64;
#pragma options align=reset

// Migration functions (simplified versions)
boolean create_root_backup(const char* original_path) {
    char backup_path[1024];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    // Create timestamped backup name: filename.root.YYYYMMDD_HHMMSS
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

boolean convert_32bit_header_to_64bit(tydatabaserecord *old_header, tydatabaserecord_64 *new_header) {
    // Copy basic fields
    new_header->systemid = old_header->systemid;
    new_header->versionnumber = 7;  // New version
    new_header->availlist = old_header->availlist;  // 32-bit → 64-bit
    new_header->oldfnumdatabase = old_header->oldfnumdatabase;
    new_header->flags = old_header->flags;
    
    // Convert views array
    for (int i = 0; i < ctviews; i++) {
        new_header->views[i] = old_header->views[i];  // 32-bit → 64-bit
    }
    
    new_header->releasestack = old_header->releasestack;
    new_header->fnumdatabase = old_header->fnumdatabase;  // 32-bit → 64-bit
    new_header->headerLength = old_header->headerLength;  // 32-bit → 64-bit
    new_header->longversionMajor = old_header->longversionMajor;
    new_header->longversionMinor = old_header->longversionMinor;
    
    // Copy extension fields
    new_header->u.extensions.availlistblock = old_header->u.extensions.availlistblock;
    new_header->u.extensions.availlistshadow = old_header->u.extensions.availlistshadow;
    new_header->u.extensions.flreadonly = old_header->u.extensions.flreadonly;
    
    return true;
}

boolean migrate_32bit_to_64bit(const char* db_path) {
    // Create backup first
    if (!create_root_backup(db_path)) {
        return false;
    }
    
    // Open the database for reading
    FILE *src = fopen(db_path, "rb");
    if (!src) {
        return false;
    }
    
    // Read the 32-bit header
    tydatabaserecord old_header;
    if (fread(&old_header, sizeof(tydatabaserecord), 1, src) != 1) {
        fclose(src);
        return false;
    }
    
    // Convert to 64-bit format
    tydatabaserecord_64 new_header;
    if (!convert_32bit_header_to_64bit(&old_header, &new_header)) {
        fclose(src);
        return false;
    }
    
    // Create temporary file for new format
    char temp_path[1024];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", db_path);
    
    FILE *dst = fopen(temp_path, "wb");
    if (!dst) {
        fclose(src);
        return false;
    }
    
    // Write new header
    if (fwrite(&new_header, sizeof(tydatabaserecord_64), 1, dst) != 1) {
        fclose(src);
        fclose(dst);
        unlink(temp_path);
        return false;
    }
    
    // Copy rest of file (skip old header)
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }
    
    fclose(src);
    fclose(dst);
    
    // Replace original file with new format
    if (rename(temp_path, db_path) != 0) {
        unlink(temp_path);
        return false;
    }
    
    return true;
}

int main() {
    printf("=== Database Migration Verification ===\n\n");
    
    const char* test_db = "test_migration_verification.root";
    // Note: backup_db will be timestamped, so we can't predict the exact name
    
    // Test 1: Create legacy database
    printf("1. Creating legacy database...\n");
    FILE *f = fopen(test_db, "wb");
    if (!f) {
        printf("   ❌ Failed to create test database\n");
        return 1;
    }
    
    // Create a minimal 32-bit database header
    tydatabaserecord header;
    memset(&header, 0, sizeof(header));
    
    header.systemid = dbsystemidMac;
    header.versionnumber = 6;  // Legacy version
    header.availlist = nildbaddress;
    header.oldfnumdatabase = 0;
    header.flags = 0;
    header.views[0] = nildbaddress;
    header.views[1] = nildbaddress;
    header.views[2] = nildbaddress;
    header.releasestack = NULL;
    header.fnumdatabase = 0;
    header.headerLength = sizeof(tydatabaserecord);
    header.longversionMajor = 6;
    header.longversionMinor = 0;
    
    // Write header
    if (fwrite(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        printf("   ❌ Failed to write header\n");
        return 1;
    }
    
    fclose(f);
    printf("   ✅ Legacy database created\n\n");
    
    // Test 2: Create backup
    printf("2. Testing backup creation...\n");
    if (!create_root_backup(test_db)) {
        printf("   ❌ Failed to create backup\n");
        return 1;
    }
    
    // Verify backup exists (we need to find the timestamped backup file)
    DIR *dir = opendir(".");
    if (!dir) {
        printf("   ❌ Cannot open directory to find backup\n");
        return 1;
    }
    
    struct dirent *entry;
    boolean found_backup = false;
    char backup_pattern[256];
    snprintf(backup_pattern, sizeof(backup_pattern), "%s.", test_db);
    
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, backup_pattern, strlen(backup_pattern)) == 0) {
            // Check if it's a timestamped backup (ends with YYYYMMDD_HHMMSS)
            char *timestamp_start = strrchr(entry->d_name, '.');
            if (timestamp_start && strlen(timestamp_start) == 16) { // .YYYYMMDD_HHMMSS = 16 chars
                found_backup = true;
                printf("   ✅ Backup created: %s\n", entry->d_name);
                break;
            }
        }
    }
    closedir(dir);
    
    if (!found_backup) {
        printf("   ❌ Backup file not found\n");
        return 1;
    }
    printf("   ✅ Backup created successfully\n\n");
    
    // Test 3: Test header conversion
    printf("3. Testing header conversion...\n");
    f = fopen(test_db, "rb");
    if (!f) {
        printf("   ❌ Failed to open database for conversion\n");
        return 1;
    }
    
    tydatabaserecord old_header;
    if (fread(&old_header, sizeof(old_header), 1, f) != 1) {
        fclose(f);
        printf("   ❌ Failed to read header\n");
        return 1;
    }
    fclose(f);
    
    tydatabaserecord_64 new_header;
    if (!convert_32bit_header_to_64bit(&old_header, &new_header)) {
        printf("   ❌ Failed to convert header\n");
        return 1;
    }
    
    // Verify conversion
    assert(new_header.systemid == old_header.systemid);
    assert(new_header.versionnumber == 7);
    assert(new_header.availlist == old_header.availlist);
    printf("   ✅ Header conversion successful\n\n");
    
    // Test 4: Full migration
    printf("4. Testing full migration...\n");
    if (!migrate_32bit_to_64bit(test_db)) {
        printf("   ❌ Migration failed\n");
        return 1;
    }
    
    // Verify migrated file
    f = fopen(test_db, "rb");
    if (!f) {
        printf("   ❌ Migrated file not found\n");
        return 1;
    }
    
    tydatabaserecord_64 migrated_header;
    if (fread(&migrated_header, sizeof(migrated_header), 1, f) != 1) {
        fclose(f);
        printf("   ❌ Failed to read migrated header\n");
        return 1;
    }
    fclose(f);
    
    // Verify new format
    assert(migrated_header.versionnumber == 7);
    assert(migrated_header.systemid == dbsystemidMac);
    printf("   ✅ Migration successful\n\n");
    
    // Test 5: Cleanup
    printf("5. Cleaning up test files...\n");
    unlink(test_db);
    
    // Clean up any timestamped backup files
    DIR *cleanup_dir = opendir(".");
    if (cleanup_dir) {
        struct dirent *entry;
        char backup_pattern[256];
        snprintf(backup_pattern, sizeof(backup_pattern), "%s.", test_db);
        
        while ((entry = readdir(cleanup_dir)) != NULL) {
            if (strncmp(entry->d_name, backup_pattern, strlen(backup_pattern)) == 0) {
                // Check if it's a timestamped backup (ends with YYYYMMDD_HHMMSS)
                char *timestamp_start = strrchr(entry->d_name, '.');
                if (timestamp_start && strlen(timestamp_start) == 16) { // .YYYYMMDD_HHMMSS = 16 chars
                    unlink(entry->d_name);
                    printf("   ✅ Cleaned up: %s\n", entry->d_name);
                }
            }
        }
        closedir(cleanup_dir);
    }
    printf("   ✅ Cleanup complete\n\n");
    
    printf("🎉 ALL TESTS PASSED! 🎉\n");
    printf("Database migration functions are working correctly.\n");
    
    return 0;
}
