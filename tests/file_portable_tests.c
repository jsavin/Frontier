#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "frontier.h"
#include "file.h"
#include "test_report.h"

static void bs_from_c(const char *c, bigstring bs) {
    size_t n = strlen(c);
    if (n > 255) n = 255;
    bs[0] = (unsigned char)n;
    memcpy(&bs[1], c, n);
}

static void write_file_bytes(const char *path, const char *data) {
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fwrite(data, 1, strlen(data), f) == strlen(data));
    fclose(f);
}

int main(void) {
    TR_INIT("file_portable_tests");
    const char *path = "portable_io_test.tmp";
    const char *payload = "line1\nline2\r\nline3\rlast";
    write_file_bytes(path, payload);

    bigstring bspath; tyfilespec fs; hdlfilenum fnum = 0;
    bs_from_c(path, bspath);
    assert(pathtofilespec(bspath, &fs));
    assert(openfile(&fs, &fnum, false));

    long eof = -1;
    assert(filegeteof(fnum, &eof));
    assert(eof == (long)strlen(payload));

    // Read exact byte counts
    char buf[64];
    memset(buf, 0, sizeof buf);
    assert(fileread(fnum, 5, buf));
    assert(strncmp(buf, "line1", 5) == 0);

    // Seek and read rest
    assert(filesetposition(fnum, 0));
    memset(buf, 0, sizeof buf);
    assert(fileread(fnum, (long)strlen(payload), buf));
    assert(strcmp(buf, payload) == 0);

    // Truncate
    assert(fileseteof(fnum, 10));
    assert(filegeteof(fnum, &eof));
    assert(eof == 10);

    assert(closefile(fnum));
    remove(path);
    printf("file_portable_tests: basic I/O passed\n");
    if (tr_count < TR_MAX_TESTS) {
        tr_results[tr_count].name = "all_tests";
        tr_results[tr_count].passed = 1;
        tr_count++;
        tr_pass_count++;
    }
    TR_SUMMARY();
    return TR_EXIT_CODE();
}

