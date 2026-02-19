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

extern long headless_readline(hdlfilenum fnum, char *buf, long bufsz);

static void write_bytes(const char *path, const char *data, size_t n) {
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fwrite(data, 1, n, f) == n);
    fclose(f);
}

int main(void) {
    TR_INIT("file_readline_tests");
    const char *path = "readline_test.tmp";
    /* Mixed endings: LF, CRLF, CR; includes empty lines and final unterminated line */
    const char payload[] = "one\n\r\n"  /* empty line via CRLF */
                            "two\rthree\n"  /* CR then LF lines */
                            "four";          /* last line no newline */
    write_bytes(path, payload, sizeof(payload) - 1);

    bigstring bspath; tyfilespec fs; hdlfilenum fnum = 0;
    bs_from_c(path, bspath);
    assert(pathtofilespec(bspath, &fs));
    assert(openfile(&fs, &fnum, true));

    char buf[64]; long n;

    n = headless_readline(fnum, buf, sizeof buf);
    assert(n == 3 && strcmp(buf, "one") == 0);

    n = headless_readline(fnum, buf, sizeof buf);
    assert(n == 0 && strcmp(buf, "") == 0); /* empty line */

    n = headless_readline(fnum, buf, sizeof buf);
    assert(n == 3 && strcmp(buf, "two") == 0);

    n = headless_readline(fnum, buf, sizeof buf);
    assert(n == 5 && strcmp(buf, "three") == 0);

    n = headless_readline(fnum, buf, sizeof buf);
    assert(n == 4 && strcmp(buf, "four") == 0);

    n = headless_readline(fnum, buf, sizeof buf);
    assert(n == 0); /* EOF */

    assert(closefile(fnum));
    remove(path);
    printf("file_readline_tests: CR/LF/CRLF handling passed\n");
    if (tr_count < TR_MAX_TESTS) {
        tr_results[tr_count].name = "all_tests";
        tr_results[tr_count].passed = 1;
        tr_count++;
        tr_pass_count++;
    }
    TR_SUMMARY();
    return TR_EXIT_CODE();
}

