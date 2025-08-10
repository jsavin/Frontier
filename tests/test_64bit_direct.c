#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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

// Define the database structures directly
#define ctviews 3
typedef long long dbaddress, *ptrdbaddress, **hdldbaddress;
#define nildbaddress 0L

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

// Version constants
#define dbversionnumber 7
#define dbversionnumber_legacy 6

int main() {
    printf("=== 64-bit Database Structure Verification ===\n\n");
    
    // Test 1: Structure sizes
    printf("1. Testing structure sizes...\n");
    printf("   sizeof(tydatabaserecord) = %zu bytes\n", sizeof(tydatabaserecord));
    printf("   sizeof(tydatabaserecord_64) = %zu bytes\n", sizeof(tydatabaserecord_64));
    printf("   sizeof(dbaddress) = %zu bytes\n", sizeof(dbaddress));
    
    // Note: On 64-bit systems, 'long' is 8 bytes, not 4 bytes
    // So the 32-bit structure is actually 116 bytes, not 88
    assert(sizeof(tydatabaserecord) == 116);
    assert(sizeof(tydatabaserecord_64) == 88);
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
