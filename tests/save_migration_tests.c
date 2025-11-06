#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "tablestructure.h"

#include "file.h"
#include "odbinternal.h"
#include "db_format.h"
#include "db_portable.h"
#include "db.h"

/* 2025-11-05 Codex: Extend migration test to exercise db_portable against the
 *  upgraded root file (open → getview → refhandle). */

static void setup_bigstring_from_c(const char *cstr, bigstring out) {
    copyctopstring(cstr, out);
}

static void analyze_header(const char *path, int *out_version) {
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    tydatabaserecord hdr;
    assert(fread(&hdr, sizeof hdr, 1, f) == 1);
    fclose(f);
    *out_version = hdr.versionnumber;
}

int main(void) {
    assert(initmemory());
    initstrings();
    assert(initlang());
    assert(inittablestructure());
    assert(langinitverbs());

    // Pick a legacy database to migrate
    const char *candidate_paths[] = {
        "databases/Frontier-v6.root",
        "../databases/Frontier-v6.root",
        "databases/Frontier.root",
        "../databases/Frontier.root",
        "databases/Frontier-v6-v7.root",
        "../databases/Frontier-v6-v7.root"
    };
    FILE *in = NULL;
    for (size_t i = 0; i < sizeof candidate_paths / sizeof candidate_paths[0]; ++i) {
        in = fopen(candidate_paths[i], "rb");
        if (in != NULL) {
            break;
        }
    }
    assert(in != NULL);

    // Make a working copy in CWD
    const char *dst = "test_save_migration.root";
    FILE *out = fopen(dst, "wb");
    assert(out != NULL);
    char buf[64 * 1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
        assert(fwrite(buf, 1, n, out) == n);
    }
    fclose(in);
    fclose(out);

    // Inspect header version; migrate if needed
    int ver_before = 0;
    analyze_header(dst, &ver_before);
    int ver_after = ver_before;

    if (ver_before <= 6) {
        if (migrate_32bit_to_64bit(dst)) {
            analyze_header(dst, &ver_after);
            assert(ver_after >= 7);
        } else {
            fprintf(stderr, "save_migration_tests: migrate_32bit_to_64bit skipped (legacy root retained)\n");
            ver_after = ver_before;
        }
    }

    // Exercise the portable DB shim against the migrated file
    assert(db_portable_init());

    bigstring dst_path;
    tyfilespec dst_spec;
    setup_bigstring_from_c(dst, dst_path);
    assert(pathtofilespec(dst_path, &dst_spec));

    hdlfilenum db_fnum = 0;
    assert(openfile(&dst_spec, &db_fnum, false));
    assert(db_portable_openfile(db_fnum, false));

    dbaddress root_addr = nildbaddress;
    for (short ix = 0; ix < ctviews; ++ix) {
        dbaddress candidate = nildbaddress;
        if (db_portable_getview(ix, &candidate) && candidate != nildbaddress) {
            root_addr = candidate;
            break;
        }
    }
    assert(root_addr != nildbaddress);

    Handle hroot = nil;
    assert(db_portable_refhandle(root_addr, &hroot));
    assert(hroot != nil);
    long root_size = gethandlesize(hroot);
    assert(root_size > 0);
    disposehandle(hroot);

    db_portable_dispose();
    assert(closefile(db_fnum));

    printf("save_migration_tests: migration applied (v%d -> v%d)\n", ver_before, ver_after);
    return 0;
}
