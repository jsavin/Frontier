#include "../framework/test_framework.h"
#include "../../Common/headers/db.h"
#include "../../Common/headers/dbinternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

// Test database file names
static const char* test_legacy_db = "test_migration_legacy.root";
static const char* test_migrated_db = "test_migration_migrated.root";
// Note: test_backup_db will be timestamped, so we can't predict the exact name

// Create a synthetic legacy database file
static bool create_legacy_database(void) {
    FILE *f = fopen(test_legacy_db, "wb");
    if (!f) return false;
    
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
        return false;
    }
    
    fclose(f);
    return true;
}

// Test backup creation
static bool test_backup_creation(void) {
    TEST_ASSERT(create_legacy_database(), "Should create legacy database");
    
    // Test backup creation
    TEST_ASSERT(create_root_backup(test_legacy_db), "Should create backup");
    
    // Verify backup file exists
    FILE *backup = fopen(test_backup_db, "rb");
    TEST_ASSERT(backup != NULL, "Backup file should exist");
    if (backup) fclose(backup);
    
    return true;
}

// Test header conversion
static bool test_header_conversion(void) {
    // Read legacy header
    FILE *f = fopen(test_legacy_db, "rb");
    TEST_ASSERT(f != NULL, "Should be able to open legacy database");
    
    tydatabaserecord old_header;
    TEST_ASSERT(fread(&old_header, sizeof(old_header), 1, f) == 1, "Should read legacy header");
    fclose(f);
    
    // Convert to 64-bit format
    tydatabaserecord_64 new_header;
    TEST_ASSERT(convert_32bit_header_to_64bit(&old_header, &new_header), "Should convert header");
    
    // Verify conversion
    TEST_ASSERT(new_header.systemid == old_header.systemid, "systemid should be preserved");
    TEST_ASSERT(new_header.versionnumber == 7, "version should be updated to 7");
    TEST_ASSERT(new_header.availlist == old_header.availlist, "availlist should be converted");
    TEST_ASSERT(new_header.oldfnumdatabase == old_header.oldfnumdatabase, "oldfnumdatabase should be preserved");
    TEST_ASSERT(new_header.flags == old_header.flags, "flags should be preserved");
    
    // Test views array conversion
    for (int i = 0; i < ctviews; i++) {
        TEST_ASSERT(new_header.views[i] == old_header.views[i], "views should be converted");
    }
    
    return true;
}

// Test full migration
static bool test_full_migration(void) {
    // Test migration function
    TEST_ASSERT(migrate_32bit_to_64bit(test_legacy_db), "Should migrate database");
    
    // Verify migrated file exists
    FILE *migrated = fopen(test_legacy_db, "rb");
    TEST_ASSERT(migrated != NULL, "Migrated file should exist");
    
    if (migrated) {
        // Read new header
        tydatabaserecord_64 new_header;
        TEST_ASSERT(fread(&new_header, sizeof(new_header), 1, migrated) == 1, "Should read new header");
        fclose(migrated);
        
        // Verify new format
        TEST_ASSERT(new_header.versionnumber == 7, "Should be version 7");
        TEST_ASSERT(new_header.systemid == dbsystemidMac, "Should have correct system ID");
    }
    
    return true;
}

// Test version detection
static bool test_version_detection(void) {
    // Test legacy version detection
    tydatabaserecord legacy_header;
    legacy_header.versionnumber = 6;
    
    TEST_ASSERT(detect_database_format(&legacy_header), "Should detect legacy format");
    TEST_ASSERT(!use_64bit_format, "Should set use_64bit_format to false");
    
    // Test modern version detection
    tydatabaserecord modern_header;
    modern_header.versionnumber = 7;
    
    TEST_ASSERT(detect_database_format(&modern_header), "Should detect modern format");
    TEST_ASSERT(use_64bit_format, "Should set use_64bit_format to true");
    
    return true;
}

// Test error handling
static bool test_error_handling(void) {
    // Test backup creation with non-existent file
    TEST_ASSERT(!create_root_backup("non_existent_file.root"), "Should fail for non-existent file");
    
    // Test migration with non-existent file
    TEST_ASSERT(!migrate_32bit_to_64bit("non_existent_file.root"), "Should fail for non-existent file");
    
    return true;
}

// Cleanup test files
static bool test_cleanup(void) {
    // Remove test files
    unlink(test_legacy_db);
    unlink(test_backup_db);
    unlink(test_migrated_db);
    
    return true;
}

test_case_t migration_tests[] = {
    {"Backup Creation", test_backup_creation},
    {"Header Conversion", test_header_conversion},
    {"Full Migration", test_full_migration},
    {"Version Detection", test_version_detection},
    {"Error Handling", test_error_handling},
    {"Cleanup", test_cleanup},
};

int migration_test_count = 6;
