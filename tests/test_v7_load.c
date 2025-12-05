/* Test loading v7 database */
#include <stdio.h>
#include <assert.h>

#include "../Common/headers/frontier.h"
#include "../Common/headers/standard.h"
#include "../Common/headers/memory.h"
#include "../Common/headers/file.h"
#include "../Common/headers/db.h"
#include "../Common/headers/db_format.h"

int main(int argc, char **argv) {
    const char *db_path = (argc > 1) ? argv[1] : "../databases/Frontier-v6-v7.root";

    printf("Testing load of: %s\n", db_path);

    assert(initmemory());

    // Open the database file
    tyfilespec fs;
    bigstring bs;
    copyctopstring(db_path, bs);
    if (!filespecfrompath(bs, &fs)) {
        fprintf(stderr, "Failed to create filespec from path\n");
        return 1;
    }

    hdlfilenum fnum;
    if (!openfile(&fs, &fnum, false)) {
        fprintf(stderr, "Failed to open file\n");
        return 1;
    }

    // Try to open the database
    if (!dbopenfile(fnum, true)) {
        fprintf(stderr, "Failed to open database\n");
        return 1;
    }

    printf("Successfully loaded v7 database!\n");

    // Check some basic properties
    hdldatabaserecord hdb;
    dbgetcurrentdatabase(&hdb);
    printf("Database version: %d\n", (**hdb).versionnumber);
    printf("Header length: %ld\n", (**hdb).headerLength);

    dbclose();
    closefile(fnum);

    return 0;
}
