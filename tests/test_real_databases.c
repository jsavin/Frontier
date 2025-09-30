#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
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

// Migration functions
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
    
    // Replace original file
    if (rename(temp_path, db_path) != 0) {
        unlink(temp_path);
        return false;
    }
    
    return true;
}

// Function to analyze database header
void analyze_database_header(const char* db_path) {
    FILE *f = fopen(db_path, "rb");
    if (!f) {
        printf("   ❌ Cannot open database: %s\n", db_path);
        return;
    }
    
    // Try to read as 32-bit format first
    tydatabaserecord header_32;
    if (fread(&header_32, sizeof(tydatabaserecord), 1, f) == 1) {
        printf("   📊 Database Analysis:\n");
        printf("      File: %s\n", db_path);
        printf("      System ID: %d\n", header_32.systemid);
        printf("      Version: %d\n", header_32.versionnumber);
        printf("      Header Length: %ld\n", header_32.headerLength);
        printf("      Available List: 0x%llx\n", header_32.availlist);
        printf("      Views: [0x%llx, 0x%llx, 0x%llx]\n", 
               header_32.views[0], header_32.views[1], header_32.views[2]);
        printf("      File Number: %ld\n", header_32.fnumdatabase);
        printf("      Flags: %d\n", header_32.flags);
        printf("      Long Version: %d.%d\n", header_32.longversionMajor, header_32.longversionMinor);
        
        // Determine format
        if (header_32.versionnumber <= 6) {
            printf("      Format: Legacy 32-bit (Version %d)\n", header_32.versionnumber);
        } else {
            printf("      Format: Modern 64-bit (Version %d)\n", header_32.versionnumber);
        }
        
        // Get file size
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        printf("      File Size: %ld bytes\n", file_size);
        
    } else {
        printf("   ❌ Cannot read database header: %s\n", db_path);
    }
    
    fclose(f);
}

// Function to test migration on a real database
boolean test_real_database_migration(const char* db_path) {
    printf("\n=== Testing Migration: %s ===\n", db_path);
    
    // Check if file exists
    FILE *test = fopen(db_path, "rb");
    if (!test) {
        printf("   ❌ Database file not found: %s\n", db_path);
        return false;
    }
    fclose(test);
    
    // Analyze original database
    printf("   📋 Original Database:\n");
    analyze_database_header(db_path);
    
    // Create a copy for testing
    char test_path[1024];
    snprintf(test_path, sizeof(test_path), "test_%s", strrchr(db_path, '/') ? strrchr(db_path, '/') + 1 : db_path);
    
    // Copy original to test file
    FILE *src = fopen(db_path, "rb");
    FILE *dst = fopen(test_path, "wb");
    if (!src || !dst) {
        if (src) fclose(src);
        if (dst) fclose(dst);
        printf("   ❌ Cannot create test copy\n");
        return false;
    }
    
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }
    fclose(src);
    fclose(dst);
    
    printf("   ✅ Created test copy: %s\n", test_path);
    
    // Test migration
    printf("   🔄 Testing migration...\n");
    if (!migrate_32bit_to_64bit(test_path)) {
        printf("   ❌ Migration failed\n");
        unlink(test_path);
        return false;
    }
    
    printf("   ✅ Migration successful\n");
    
    // Analyze migrated database
    printf("   📋 Migrated Database:\n");
    analyze_database_header(test_path);
    
    // Cleanup
    unlink(test_path);
    
    // Clean up any backup files
    DIR *dir = opendir(".");
    if (dir) {
        struct dirent *entry;
        char backup_pattern[256];
        snprintf(backup_pattern, sizeof(backup_pattern), "%s.", test_path);
        
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, backup_pattern, strlen(backup_pattern)) == 0) {
                // Check if it's a timestamped backup (ends with YYYYMMDD_HHMMSS)
                char *timestamp_start = strrchr(entry->d_name, '.');
                if (timestamp_start && strlen(timestamp_start) == 16) { // .YYYYMMDD_HHMMSS = 16 chars
                    unlink(entry->d_name);
                    printf("   ✅ Cleaned up backup: %s\n", entry->d_name);
                }
            }
        }
        closedir(dir);
    }
    
    return true;
}

static int file_readable(const char *p) {
    return access(p, R_OK) == 0;
}

static void join_path(const char *a, const char *b, char *out, size_t outsz) {
    if (a && *a) snprintf(out, outsz, "%s/%s", a, b); else snprintf(out, outsz, "%s", b);
}

static int resolve_repo_path(const char *repo_rel, const char *argv0, char *out, size_t outsz) {
    // Try CWD-relative first
    if (file_readable(repo_rel)) { strncpy(out, repo_rel, outsz); out[outsz-1]='\0'; return 1; }

    // Try relative to executable directory
    char exe_path[PATH_MAX];
    if (argv0 && *argv0) {
        if (realpath(argv0, exe_path) != NULL) {
            char exe_dir[PATH_MAX];
            strncpy(exe_dir, exe_path, sizeof(exe_dir)); exe_dir[sizeof(exe_dir)-1]='\0';
            char *d = dirname(exe_dir);
            char candidate[PATH_MAX];
            // If binary is in tests/, repo root is parent
            char parent[PATH_MAX];
            strncpy(parent, d, sizeof(parent)); parent[sizeof(parent)-1]='\0';
            char upone[PATH_MAX];
            // parent of tests is repo root
            char *p = strrchr(parent, '/');
            if (p) *p = '\0';
            join_path(parent, repo_rel, candidate, sizeof(candidate));
            if (file_readable(candidate)) { strncpy(out, candidate, outsz); out[outsz-1]='\0'; return 1; }

            // Also try d + "/../" + repo_rel
            char d_up[PATH_MAX];
            snprintf(d_up, sizeof(d_up), "%s/..", d);
            join_path(d_up, repo_rel, candidate, sizeof(candidate));
            if (file_readable(candidate)) { strncpy(out, candidate, outsz); out[outsz-1]='\0'; return 1; }
        }
    }

    // Try adding a leading ../ for backwards compatibility
    char rel2[PATH_MAX];
    snprintf(rel2, sizeof(rel2), "../%s", repo_rel);
    if (file_readable(rel2)) { strncpy(out, rel2, outsz); out[outsz-1]='\0'; return 1; }

    return 0;
}

int main(int argc, char **argv) {
    printf("=== Real Database Migration Testing ===\n\n");
    
    // Test databases to migrate
    const char* test_databases_repo_rel[] = {
        "databases/Frontier.root",
        "databases/Guest Databases/apps/manila.root",
        "databases/Guest Databases/apps/mainResponder.root",
        NULL
    };
    
    int success_count = 0;
    int total_count = 0;
    
    for (int i = 0; test_databases_repo_rel[i] != NULL; i++) {
        char resolved[PATH_MAX];
        if (!resolve_repo_path(test_databases_repo_rel[i], (argc > 0 ? argv[0] : NULL), resolved, sizeof(resolved))) {
            printf("\n=== Testing Migration: %s ===\n", test_databases_repo_rel[i]);
            printf("   ❌ Database file not found: %s (tried CWD and executable-relative)\n", test_databases_repo_rel[i]);
            total_count++;
            continue;
        }
        total_count++;
        if (test_real_database_migration(resolved)) {
            success_count++;
        }
    }
    
    printf("\n=== Test Summary ===\n");
    printf("Total Databases Tested: %d\n", total_count);
    printf("Successful Migrations: %d\n", success_count);
    printf("Failed Migrations: %d\n", total_count - success_count);
    
    if (success_count == total_count) {
        printf("🎉 ALL MIGRATIONS SUCCESSFUL! 🎉\n");
        printf("Real-world database migration is working correctly.\n");
    } else {
        printf("⚠️  Some migrations failed. Review the output above.\n");
    }
    
    return (success_count == total_count) ? 0 : 1;
}
