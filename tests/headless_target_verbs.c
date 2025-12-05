#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the target processor */
enum {
    tarv_target_verb0 = 0,
    tarv_target_verb1 = 1,
    tarv_target_verb2 = 2
};

static boolean target_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case tarv_target_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case tarv_target_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case tarv_target_verb2:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean targetinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\ptarget"), bsname);

    if (!newfunctionprocessor(bsname, &target_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\ptarget_verb0"), tarv_target_verb0);
    ADD_VERB(BIGSTRING("\ptarget_verb1"), tarv_target_verb1);
    ADD_VERB(BIGSTRING("\ptarget_verb2"), tarv_target_verb2);

    #undef ADD_VERB

    pophashtable();
    return true;
}
