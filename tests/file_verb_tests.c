#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "tablestructure.h"

extern boolean fileinitverbs(void);


static void eval_expect(const char *expr, const char *expected) {
    bigstring program, result;
    bs_from_c(expr, program);
    assert(langrunstringnoerror(program, result));
    char c_result[256];
    copyptocstring(result, c_result);
    assert(strcmp(c_result, expected) == 0);
}

static void write_file_bytes(const char *path, const char *data) {
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fwrite(data, 1, strlen(data), f) == strlen(data));
    fclose(f);
}

int main(void) {
    assert(initmemory());
    initstrings();
    assert(initlang());
    assert(inittablestructure());
    assert(langinitverbs());

    /* Register headless file verbs */
    assert(fileinitverbs());

    const char *path = "verbs_test.tmp";
    const char *payload = "one\n\r\n" "two\rthree\n" "four";
    write_file_bytes(path, payload);

    char script[512];

    /* open */
    snprintf(script, sizeof script, "file.open(\"%s\")", path);
    eval_expect(script, "true");

    /* readLine sequence */
    snprintf(script, sizeof script, "file.readLine(\"%s\")", path);
    eval_expect(script, "one");
    eval_expect(script, "");
    eval_expect(script, "two");
    eval_expect(script, "three");
    eval_expect(script, "four");

    /* EOF now: endOfFile true */
    snprintf(script, sizeof script, "file.endOfFile(\"%s\")", path);
    eval_expect(script, "true");

    /* setPosition and getPosition */
    snprintf(script, sizeof script, "file.setPosition(\"%s\", 0)", path);
    eval_expect(script, "true");
    snprintf(script, sizeof script, "file.getPosition(\"%s\")", path);
    eval_expect(script, "0");

    /* append a line */
    snprintf(script, sizeof script, "file.write(\"%s\", \"\\rAPPEND\")", path);
    eval_expect(script, "true");

    /* close */
    snprintf(script, sizeof script, "file.close(\"%s\")", path);
    eval_expect(script, "true");

    remove(path);
    printf("file_verb_tests: headless file verbs passed\n");
    return 0;
}
