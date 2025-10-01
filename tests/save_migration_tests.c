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
    const char *src = "databases/Frontier.root";
    FILE *in = fopen(src, "rb");
    if (!in) {
        src = "../databases/Frontier.root"; // when running from tests/
        in = fopen(src, "rb");
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

    // Verify it's legacy (<=6)
    int ver_before = 0;
    analyze_header(dst, &ver_before);
    assert(ver_before <= 6);

    // Perform migration to modern format
    assert(migrate_32bit_to_64bit(dst));

    // Verify header is now v7
    int ver_after = 0;
    analyze_header(dst, &ver_after);
    assert(ver_after >= 7);

    printf("save_migration_tests: migration applied (v%d -> v%d)\n", ver_before, ver_after);
    return 0;
}
