#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Define minimal types
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

// Define the database structures
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
#pragma options align=reset

void hex_dump(const unsigned char *data, size_t length) {
    for (size_t i = 0; i < length; i += 16) {
        printf("%04zx: ", i);
        for (size_t j = 0; j < 16 && i + j < length; j++) {
            printf("%02x ", data[i + j]);
        }
        printf(" ");
        for (size_t j = 0; j < 16 && i + j < length; j++) {
            unsigned char c = data[i + j];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }
        printf("\n");
    }
}

void analyze_database_structure(const char* db_path) {
    printf("\n=== Detailed Analysis: %s ===\n", db_path);
    
    FILE *f = fopen(db_path, "rb");
    if (!f) {
        printf("❌ Cannot open database: %s\n", db_path);
        return;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    printf("File Size: %ld bytes\n", file_size);
    printf("Expected Structure Size: %zu bytes\n", sizeof(tydatabaserecord));
    
    // Read first 128 bytes for analysis
    unsigned char buffer[128];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), f);
    
    printf("\nFirst 128 bytes (hex dump):\n");
    hex_dump(buffer, bytes_read);
    
    // Try to interpret as our structure
    fseek(f, 0, SEEK_SET);
    tydatabaserecord header;
    if (fread(&header, sizeof(tydatabaserecord), 1, f) == 1) {
        printf("\nStructure Analysis:\n");
        printf("systemid: %d (0x%02x)\n", header.systemid, header.systemid);
        printf("versionnumber: %d (0x%02x)\n", header.versionnumber, header.versionnumber);
        printf("availlist: 0x%llx\n", header.availlist);
        printf("oldfnumdatabase: %d (0x%04x)\n", header.oldfnumdatabase, header.oldfnumdatabase);
        printf("flags: %d (0x%04x)\n", header.flags, header.flags);
        printf("views[0]: 0x%llx\n", header.views[0]);
        printf("views[1]: 0x%llx\n", header.views[1]);
        printf("views[2]: 0x%llx\n", header.views[2]);
        printf("releasestack: %p\n", header.releasestack);
        printf("fnumdatabase: %ld (0x%lx)\n", header.fnumdatabase, header.fnumdatabase);
        printf("headerLength: %ld (0x%lx)\n", header.headerLength, header.headerLength);
        printf("longversionMajor: %d (0x%04x)\n", header.longversionMajor, header.longversionMajor);
        printf("longversionMinor: %d (0x%04x)\n", header.longversionMinor, header.longversionMinor);
        
        // Check if headerLength makes sense
        if (header.headerLength == 0) {
            printf("\n⚠️  WARNING: headerLength is 0, which suggests:\n");
            printf("   1. The field might be in a different location\n");
            printf("   2. The structure might be different than expected\n");
            printf("   3. The database might be using a different format\n");
        }
        
        // Check if version numbers make sense
        if (header.longversionMajor == 0 && header.longversionMinor == 0) {
            printf("\n⚠️  WARNING: Long version is 0.0, which suggests:\n");
            printf("   1. The fields might be in a different location\n");
            printf("   2. The database might be using an older format\n");
        }
        
    } else {
        printf("❌ Cannot read database header\n");
    }
    
    fclose(f);
}

int main() {
    printf("=== Database Structure Debug Analysis ===\n");
    
    const char* test_databases[] = {
        "../databases/Frontier.root",
        "../databases/Guest Databases/apps/manila.root",
        "../databases/Guest Databases/apps/mainResponder.root",
        NULL
    };
    
    for (int i = 0; test_databases[i] != NULL; i++) {
        analyze_database_structure(test_databases[i]);
    }
    
    return 0;
}
