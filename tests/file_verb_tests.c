#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "tablestructure.h"
#include "langexternal.h"
#include "kernelverbs.h"

extern boolean fileinitverbs(void);

/*
 Headless note:
 - In classic builds, UserTalk verbs like file.open are exposed via
   system.verbs.file.open scripts which call kernelcall, dispatching to the
   EFP table under efptable. In headless, we register the EFP programmatically
   and (temporarily) link the 'file' EFP table into the current scope so that
   dotted calls like file.open resolve without system.verbs wrappers.
 - Once the broader UserTalk environment is initialized (system tables loaded),
   this workaround can be removed and calls will naturally dispatch via
   system.verbs.* -> kernelcall -> EFP.
*/


static void eval_expect(const char *expr, const char *expected) {
    bigstring program, result;
    bs_from_c(expr, program);
    if (!langrunstring(program, result)) {
        char err[256];
        copyptocstring(result, err);
        fprintf(stderr, "[file_verb_tests] eval failed: %s => %s\n", expr, err);
        assert(0);
    }
    char c_result[256];
    copyptocstring(result, c_result);
    if (strcmp(c_result, expected) != 0) {
        fprintf(stderr, "[file_verb_tests] mismatch: %s => got '%s', want '%s'\n", expr, c_result, expected);
        assert(0);
    }
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

    /* Headless workaround: make the EFP table visible as a top-level symbol so
       dotted calls like file.open resolve via efptable path without relying on
       system.verbs wrappers. */
    {
        bigstring bsname;
        copyctopstring("file", bsname);
        hdlhashtable hefp = nil;
        if (langexternalgettable(bsname, &hefp) && hefp != nil) {
            /* Find the external variable handle inside efptable for reuse */
            hdlhashnode hnode = nil;
            pushhashtable(efptable);
            if (hashtablelookupnode(efptable, bsname, &hnode)) {
                tyvaluerecord v = (**hnode).val;
                if (v.valuetype == externalvaluetype) {
                    /* Link the same external handle into current scope as 'file' */
                    langsetexternalsymbol(currenthashtable, bsname, idtableprocessor, v.data.externalvalue);
                }
            }
            pophashtable();
            /* linked */
        }
        else {
            /* unable to link */
        }
    }

    const char *path = "verbs_test.tmp";
    const char *payload = "one\n\r\n" "two\rthree\n" "four";
    write_file_bytes(path, payload);

    char script[512];

    /* open */
    snprintf(script, sizeof script, "file.open( \"%s\")", path);
    eval_expect(script, "true");

    /* readLine sequence */
    snprintf(script, sizeof script, "file.readLine( \"%s\")", path);
    eval_expect(script, "one");
    eval_expect(script, "");
    eval_expect(script, "two");
    eval_expect(script, "three");
    eval_expect(script, "four");

    /* EOF now: endOfFile true */
    snprintf(script, sizeof script, "file.endOfFile( \"%s\")", path);
    eval_expect(script, "true");

    /* setPosition and getPosition */
    snprintf(script, sizeof script, "file.setPosition( \"%s\", 0)", path);
    eval_expect(script, "true");
    snprintf(script, sizeof script, "file.getPosition( \"%s\")", path);
    eval_expect(script, "0");

    /* append a line */
    snprintf(script, sizeof script, "file.write( \"%s\", \"\\rAPPEND\")", path);
    eval_expect(script, "true");

    /* close */
    snprintf(script, sizeof script, "file.close( \"%s\")", path);
    eval_expect(script, "true");

    remove(path);
    printf("file_verb_tests: headless file verbs passed\n");
    return 0;
}
