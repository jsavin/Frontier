#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the pict processor */
enum {
    picv_now = 0,
    picv_set = 1,
    picv_sleepfor = 2,
    picv_ticks = 3
};

static boolean pict_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case picv_now:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case picv_set:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case picv_sleepfor:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case picv_ticks:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean pictinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\ppict"), bsname);

    if (!newfunctionprocessor(bsname, &pict_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pnow"), picv_now);
    ADD_VERB(BIGSTRING("\pset"), picv_set);
    ADD_VERB(BIGSTRING("\psleepfor"), picv_sleepfor);
    ADD_VERB(BIGSTRING("\pticks"), picv_ticks);

    #undef ADD_VERB

    pophashtable();
    return true;
}
