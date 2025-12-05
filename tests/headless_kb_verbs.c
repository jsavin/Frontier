#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the kb processor */
enum {
    kbv_kb_verb0 = 0,
    kbv_kb_verb1 = 1,
    kbv_kb_verb2 = 2,
    kbv_kb_verb3 = 3
};

static boolean kb_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case kbv_kb_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case kbv_kb_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case kbv_kb_verb2:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case kbv_kb_verb3:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean kbinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pkb"), bsname);

    if (!newfunctionprocessor(bsname, &kb_valueproc, false, &htable))
        return false;

    pushhashtable(htable);

    #define ADD_VERB(name, tok) do { \
        bigstring bs; \
        copystring(name, bs); \
        if (!langaddkeyword(bs, tok)) { \
            pophashtable(); \
            return false; \
        } \
    } while(0)

    ADD_VERB(BIGSTRING("\pkb_verb0"), kbv_kb_verb0);
    ADD_VERB(BIGSTRING("\pkb_verb1"), kbv_kb_verb1);
    ADD_VERB(BIGSTRING("\pkb_verb2"), kbv_kb_verb2);
    ADD_VERB(BIGSTRING("\pkb_verb3"), kbv_kb_verb3);

    #undef ADD_VERB

    pophashtable();
    return true;
}
