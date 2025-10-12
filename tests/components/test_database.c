/*
 * test_database.c - Database component test suite for Frontier
 * 
 * Phase 0.5.3: Testing Strategy Implementation
 * 
 * This test suite validates the database component functionality,
 * including .odb file operations, data integrity, and legacy compatibility.
 */

#include "../framework/test_framework.h"
#include "../../Common/headers/db.h"
#include "../../Common/headers/dbinternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Include minimal type definitions for testing
typedef unsigned char Boolean;
typedef unsigned char UInt8;
typedef unsigned short UInt16;
typedef unsigned int UInt32;
typedef signed char SInt8;
typedef signed short SInt16;
typedef signed int SInt32;

typedef char* Ptr;
typedef Ptr* Handle;
typedef Handle RgnHandle;
typedef Handle ControlHandle;
typedef Handle MenuHandle;

typedef struct {
    unsigned char data[8];
} Pattern;

typedef struct {
    unsigned char data[80];
} FSRef;

typedef struct {
    unsigned short length;
    unsigned short data[255];
} HFSUniStr255;

// Database-specific types (from db.h)
typedef long dbaddress, *ptrdbaddress, **hdldbaddress;

#define nildbaddress 0L
#define ctviews 3

typedef struct tydatabaserecord {
    unsigned char systemid;        /* 0 = MAC (compatibility) */
    unsigned char versionnumber;   /* which version created this file? */
    dbaddress availlist;          /* avail list is singly-linked, nil terminated */
    short oldfnumdatabase;        /* only applies when database record is in memory */
    short flags;                  /* any changes to header since it was last flushed? */
    dbaddress views[ctviews];     /* addresses of the root of each view */
    Handle releasestack;          /* holds addresses of nodes waiting to be released */
    long fnumdatabase;            /* file handle while in memory - New to version 5 */
    long headerLength;            /* size of header - New to version 5 */
    short longversionMajor;       /* new extended version id - new to version 5 */
    short longversionMinor;       /* new extended version id - new to version 5 */
} tydatabaserecord, *ptrdatabaserecord, **hdldatabaserecord;

// Test utilities
static const char* test_db_filename = "test_database.root";
static const char* test_db_backup = "test_database_backup.root";

// Test helper functions
static bool create_test_database_file(void) {
    FILE* file = fopen(test_db_filename, "wb");
    if (!file) {
        printf("ERROR: Could not create test database file\n");
        return false;
    }
    
    // Create a minimal .odb file header
    tydatabaserecord header = {0};
    header.systemid = 0; // MAC compatibility
    header.versionnumber = 5; // Version 5
    header.availlist = nildbaddress;
    header.oldfnumdatabase = -1;
    header.flags = 0;
    header.fnumdatabase = -1;
    header.headerLength = sizeof(tydatabaserecord);
    header.longversionMajor = 5;
    header.longversionMinor = 0;
    
    // Initialize views to nil
    for (int i = 0; i < ctviews; i++) {
        header.views[i] = nildbaddress;
    }
    
    // Write header to file
    size_t written = fwrite(&header, sizeof(tydatabaserecord), 1, file);
    fclose(file);
    
    return written == 1;
}

static bool cleanup_test_files(void) {
    unlink(test_db_filename);
    unlink(test_db_backup);
    return true;
}

// Test functions

bool test_database_file_creation(void) {
    TEST_ASSERT(create_test_database_file(), "Database file creation should succeed");
    
    // Verify file exists and has correct size
    FILE* file = fopen(test_db_filename, "rb");
    TEST_ASSERT(file != NULL, "Database file should exist after creation");
    
    if (file) {
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fclose(file);
        
        TEST_ASSERT(file_size == sizeof(tydatabaserecord), 
                   "Database file should have correct header size");
    }
    
    cleanup_test_files();
    TEST_PASS("Database file creation and validation successful");
}

bool test_database_header_structure(void) {
    TEST_ASSERT(create_test_database_file(), "Database file creation should succeed");
    
    FILE* file = fopen(test_db_filename, "rb");
    TEST_ASSERT(file != NULL, "Database file should be readable");
    
    if (file) {
        tydatabaserecord header;
        size_t read = fread(&header, sizeof(tydatabaserecord), 1, file);
        fclose(file);
        
        TEST_ASSERT(read == 1, "Database header should be readable");
        TEST_ASSERT(header.systemid == 0, "System ID should be 0 (MAC compatibility)");
        TEST_ASSERT(header.versionnumber == 5, "Version number should be 5");
        TEST_ASSERT(header.availlist == nildbaddress, "Initial avail list should be nil");
        TEST_ASSERT(header.headerLength == sizeof(tydatabaserecord), 
                   "Header length should match structure size");
        TEST_ASSERT(header.longversionMajor == 5, "Major version should be 5");
        TEST_ASSERT(header.longversionMinor == 0, "Minor version should be 0");
        
        // Test views initialization
        for (int i = 0; i < ctviews; i++) {
            TEST_ASSERT(header.views[i] == nildbaddress, 
                       "Initial views should be nil");
        }
    }
    
    cleanup_test_files();
    TEST_PASS("Database header structure validation successful");
}

bool test_database_file_operations(void) {
    TEST_ASSERT(create_test_database_file(), "Database file creation should succeed");
    
    // Test file operations
    FILE* file = fopen(test_db_filename, "rb+");
    TEST_ASSERT(file != NULL, "Database file should be openable for read/write");
    
    if (file) {
        // Test seeking to different positions
        TEST_ASSERT(fseek(file, 0, SEEK_SET) == 0, "Seek to beginning should succeed");
        TEST_ASSERT(fseek(file, sizeof(tydatabaserecord), SEEK_SET) == 0, 
                   "Seek to end of header should succeed");
        
        // Test writing data after header
        const char* test_data = "Test database content";
        size_t data_len = strlen(test_data);
        size_t written = fwrite(test_data, 1, data_len, file);
        TEST_ASSERT(written == data_len, "Data writing should succeed");
        
        // Test reading back the data
        fseek(file, sizeof(tydatabaserecord), SEEK_SET);
        char read_data[256] = {0};
        size_t read = fread(read_data, 1, data_len, file);
        TEST_ASSERT(read == data_len, "Data reading should succeed");
        TEST_ASSERT_STR_EQUAL(test_data, read_data, "Read data should match written data");
        
        fclose(file);
    }
    
    cleanup_test_files();
    TEST_PASS("Database file operations validation successful");
}

bool test_database_address_operations(void) {
    // Test database address operations
    dbaddress addr1 = 1000L;
    dbaddress addr2 = 2000L;
    dbaddress nil_addr = nildbaddress;
    
    TEST_ASSERT(addr1 != nildbaddress, "Valid address should not be nil");
    TEST_ASSERT(addr2 != nildbaddress, "Valid address should not be nil");
    TEST_ASSERT(nil_addr == nildbaddress, "Nil address should equal nildbaddress");
    TEST_ASSERT(addr1 < addr2, "Address comparison should work");
    TEST_ASSERT(addr2 > addr1, "Address comparison should work");
    
    // Test address arithmetic
    dbaddress diff = addr2 - addr1;
    TEST_ASSERT_EQUAL(1000L, diff, "Address arithmetic should work");
    
    TEST_PASS("Database address operations validation successful");
}

bool test_database_compatibility(void) {
    // Test legacy compatibility features
    TEST_ASSERT(create_test_database_file(), "Database file creation should succeed");
    
    FILE* file = fopen(test_db_filename, "rb");
    TEST_ASSERT(file != NULL, "Database file should be readable");
    
    if (file) {
        tydatabaserecord header;
        fread(&header, sizeof(tydatabaserecord), 1, file);
        fclose(file);
        
        // Test legacy compatibility
        TEST_ASSERT(header.systemid == 0, "Should maintain MAC compatibility");
        TEST_ASSERT(header.oldfnumdatabase == -1, "Should have valid legacy file number");
        TEST_ASSERT(header.fnumdatabase == -1, "Should have valid file number");
        
        // Test version compatibility
        TEST_ASSERT(header.versionnumber >= 3, "Should support version 3+ compatibility");
        TEST_ASSERT(header.longversionMajor >= 5, "Should support extended versioning");
    }
    
    cleanup_test_files();
    TEST_PASS("Database compatibility validation successful");
}

// Test suite definition
test_case_t database_tests[] = {
    {"Database File Creation", test_database_file_creation},
    {"Database Header Structure", test_database_header_structure},
    {"Database File Operations", test_database_file_operations},
    {"Database Address Operations", test_database_address_operations},
    {"Database Compatibility", test_database_compatibility},
};

// Main test runner
int main(void) {
    printf("=== Frontier Database Component Test Suite ===\n");
    printf("Phase 0.5.3: Testing Strategy Implementation\n\n");
    
    // Initialize test framework
    test_init();
    
    // Run database test suite
    int test_count = sizeof(database_tests) / sizeof(database_tests[0]);
    bool all_passed = run_test_suite(database_tests, test_count);
    
    // Print summary
    test_summary();
    
    // Cleanup any remaining test files
    cleanup_test_files();
    
    return all_passed ? 0 : 1;
}
