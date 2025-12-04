/* Test program to verify tydatabaserecord_64 structure layout */

#include <stdio.h>
#include <stddef.h>

#define FRONTIER_HEADLESS 1
#include "frontier.h"
#include "standard.h"
#include "db.h"

int main(void) {
    printf("tydatabaserecord structure layout:\n");
    printf("  sizeof(tydatabaserecord) = %zu\n", sizeof(tydatabaserecord));
    printf("  offsetof(views) = %zu\n", offsetof(tydatabaserecord, views));

    printf("\ntydatabaserecord_64 structure layout:\n");
    printf("  sizeof(tydatabaserecord_64) = %zu\n", sizeof(tydatabaserecord_64));
    printf("  offsetof(systemid) = %zu\n", offsetof(tydatabaserecord_64, systemid));
    printf("  offsetof(versionnumber) = %zu\n", offsetof(tydatabaserecord_64, versionnumber));
    printf("  offsetof(availlist) = %zu\n", offsetof(tydatabaserecord_64, availlist));
    printf("  offsetof(oldfnumdatabase) = %zu\n", offsetof(tydatabaserecord_64, oldfnumdatabase));
    printf("  offsetof(flags) = %zu\n", offsetof(tydatabaserecord_64, flags));
    printf("  offsetof(views) = %zu (expected: 16)\n", offsetof(tydatabaserecord_64, views));
    printf("  offsetof(releasestack) = %zu\n", offsetof(tydatabaserecord_64, releasestack));
    printf("  offsetof(fnumdatabase) = %zu\n", offsetof(tydatabaserecord_64, fnumdatabase));
    printf("  offsetof(headerLength) = %zu\n", offsetof(tydatabaserecord_64, headerLength));
    printf("  sizeof(dbaddress) = %zu\n", sizeof(dbaddress));

    if (offsetof(tydatabaserecord_64, views) != 16) {
        printf("\nERROR: views offset is %zu, expected 16!\n", offsetof(tydatabaserecord_64, views));
        return 1;
    }

    if (offsetof(tydatabaserecord, views) != 16) {
        printf("\nERROR: tydatabaserecord views offset is %zu, expected 16!\n", offsetof(tydatabaserecord, views));
        return 1;
    }

    printf("\nStructure layouts are CORRECT for v7 format.\n");
    return 0;
}
