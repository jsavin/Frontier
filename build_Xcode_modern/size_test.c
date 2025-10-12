#include <stdio.h>

typedef long dbaddress;
#define ctviews 3

typedef struct tydatabaserecord {
    unsigned char systemid;
    unsigned char versionnumber;
    dbaddress availlist;
    short oldfnumdatabase;
    short flags;
    dbaddress views[ctviews];
    void* releasestack;
    long fnumdatabase;
    long headerLength;
    short longversionMajor;
    short longversionMinor;
} tydatabaserecord;

int main() {
    tydatabaserecord test_db = {0};
    printf("Total Size: %zu\n", sizeof(tydatabaserecord));
    printf("systemid: %zu\n", sizeof(test_db.systemid));
    printf("versionnumber: %zu\n", sizeof(test_db.versionnumber));
    printf("availlist: %zu\n", sizeof(test_db.availlist));
    printf("oldfnumdatabase: %zu\n", sizeof(test_db.oldfnumdatabase));
    printf("flags: %zu\n", sizeof(test_db.flags));
    printf("views: %zu\n", sizeof(test_db.views));
    printf("releasestack: %zu\n", sizeof(test_db.releasestack));
    printf("fnumdatabase: %zu\n", sizeof(test_db.fnumdatabase));
    printf("headerLength: %zu\n", sizeof(test_db.headerLength));
    printf("longversionMajor: %zu\n", sizeof(test_db.longversionMajor));
    printf("longversionMinor: %zu\n", sizeof(test_db.longversionMinor));
    return 0;
}
