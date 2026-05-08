#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

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

    /*
     * Issue #590: opennewfile_exclusive must create files with mode 0600
     * (owner read-write only). Database files written by db.compactDatabase
     * may contain sensitive data (e.g. user.prefs.portForwardingAdminPassword)
     * and must not be world-readable under default umask. Matches the lock
     * file pattern in db_format.c (open(... 0600)).
     */
    const char *perm_path = "portable_io_perms_test.tmp";
    /* Ensure no stale file from a prior failed run — opennewfile_exclusive
     * (O_EXCL) refuses to overwrite. */
    remove(perm_path);

    bigstring bsperm; tyfilespec fsperm; hdlfilenum permfnum = 0;
    bs_from_c(perm_path, bsperm);
    assert(pathtofilespec(bsperm, &fsperm));
    assert(opennewfile_exclusive(&fsperm, 'LAND', 'ROOT', &permfnum));

    struct stat st;
    assert(stat(perm_path, &st) == 0);
    /* mode bits must equal exactly 0600 — no group/other access */
    mode_t perm_bits = st.st_mode & 0777;
    if (perm_bits != 0600) {
        fprintf(stderr,
                "file_portable_tests: perm check FAILED — got 0%o, expected 0600\n",
                (unsigned int)perm_bits);
        assert(perm_bits == 0600);
    }

    assert(closefile(permfnum));
    remove(perm_path);
    printf("file_portable_tests: opennewfile_exclusive 0600 perms passed\n");
    if (tr_count < TR_MAX_TESTS) {
        tr_results[tr_count].name = "opennewfile_exclusive_perms_0600";
        tr_results[tr_count].passed = 1;
        tr_count++;
        tr_pass_count++;
    }

    TR_SUMMARY();
    return TR_EXIT_CODE();
}

