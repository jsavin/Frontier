#include "../framework/test_framework.h"
#include "../../Common/headers/db.h"
#include "../../Common/headers/dbinternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// Test structure sizes for both formats
static bool test_structure_sizes(void) {
    TEST_ASSERT(sizeof(tydatabaserecord) == 88, "32-bit database structure should be 88 bytes");
    TEST_ASSERT(sizeof(tydatabaserecord_64) == 80, "64-bit database structure should be 80 bytes");
    
    // Test address type size
    TEST_ASSERT(sizeof(dbaddress) == 8, "dbaddress should be 8 bytes (64-bit)");
    
    return true;
}

// Test version detection logic
static bool test_version_detection(void) {
    // Test version constants
    TEST_ASSERT(dbversionnumber == 7, "Current version should be 7");
    TEST_ASSERT(dbversionnumber_legacy == 6, "Legacy version should be 6");
    
    return true;
}

// Test address type compatibility
static bool test_address_compatibility(void) {
    // Test that we can assign 32-bit values to 64-bit addresses
    dbaddress addr_32 = 0x12345678;
    dbaddress addr_64 = (dbaddress)addr_32;
    
    TEST_ASSERT(addr_64 == 0x12345678, "32-bit to 64-bit conversion should preserve value");
    
    // Test large addresses
    dbaddress large_addr = 0x123456789ABCDEF0;
    TEST_ASSERT(large_addr > 0xFFFFFFFF, "64-bit addresses should support large values");
    
    return true;
}

// Test nil address constant
static bool test_nil_address(void) {
    TEST_ASSERT(nildbaddress == 0L, "nil address should be 0");
    
    dbaddress addr = nildbaddress;
    TEST_ASSERT(addr == 0, "nil address assignment should work");
    
    return true;
}

test_case_t structure_tests[] = {
    {"Structure Sizes", test_structure_sizes},
    {"Version Detection", test_version_detection},
    {"Address Compatibility", test_address_compatibility},
    {"Nil Address", test_nil_address},
};

int structure_test_count = 4;
