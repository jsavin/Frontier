#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// Include our database headers
#include "../Common/headers/db.h"
#include "../Common/headers/dbinternal.h"

int main() {
    printf("=== 64-bit Database Structure Verification ===\n\n");
    
    // Test 1: Structure sizes
    printf("1. Testing structure sizes...\n");
    printf("   sizeof(tydatabaserecord) = %zu bytes\n", sizeof(tydatabaserecord));
    printf("   sizeof(tydatabaserecord_64) = %zu bytes\n", sizeof(tydatabaserecord_64));
    printf("   sizeof(dbaddress) = %zu bytes\n", sizeof(dbaddress));
    
    assert(sizeof(tydatabaserecord) == 88);
    assert(sizeof(tydatabaserecord_64) == 80);
    assert(sizeof(dbaddress) == 8);
    printf("   ✅ Structure sizes are correct\n\n");
    
    // Test 2: Version constants
    printf("2. Testing version constants...\n");
    printf("   dbversionnumber = %d\n", dbversionnumber);
    printf("   dbversionnumber_legacy = %d\n", dbversionnumber_legacy);
    
    assert(dbversionnumber == 7);
    assert(dbversionnumber_legacy == 6);
    printf("   ✅ Version constants are correct\n\n");
    
    // Test 3: Address type compatibility
    printf("3. Testing address type compatibility...\n");
    dbaddress addr_32 = 0x12345678;
    dbaddress addr_64 = (dbaddress)addr_32;
    dbaddress large_addr = 0x123456789ABCDEF0;
    
    printf("   32-bit address: 0x%llx\n", addr_32);
    printf("   64-bit address: 0x%llx\n", addr_64);
    printf("   large address: 0x%llx\n", large_addr);
    
    assert(addr_64 == 0x12345678);
    assert(large_addr > 0xFFFFFFFF);
    printf("   ✅ Address type compatibility works\n\n");
    
    // Test 4: Nil address
    printf("4. Testing nil address...\n");
    printf("   nildbaddress = %lld\n", nildbaddress);
    
    dbaddress addr = nildbaddress;
    assert(addr == 0);
    printf("   ✅ Nil address works correctly\n\n");
    
    printf("🎉 ALL TESTS PASSED! 🎉\n");
    printf("64-bit database structure changes are working correctly.\n");
    
    return 0;
}
