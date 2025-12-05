#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the clock processor */
enum {
    clov_now = 0,
    clov_set = 1,
    clov_sleepfor = 2,
    clov_ticks = 3,
    clov_milliseconds = 4,
    clov_waitseconds = 5,
    clov_waitsixtieths = 6
};

static boolean clock_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case clov_now:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case clov_set:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case clov_sleepfor:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case clov_ticks:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case clov_milliseconds:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case clov_waitseconds:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case clov_waitsixtieths:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean clockinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pclock"), bsname);

    if (!newfunctionprocessor(bsname, &clock_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pnow"), clov_now);
    ADD_VERB(BIGSTRING("\pset"), clov_set);
    ADD_VERB(BIGSTRING("\psleepfor"), clov_sleepfor);
    ADD_VERB(BIGSTRING("\pticks"), clov_ticks);
    ADD_VERB(BIGSTRING("\pmilliseconds"), clov_milliseconds);
    ADD_VERB(BIGSTRING("\pwaitseconds"), clov_waitseconds);
    ADD_VERB(BIGSTRING("\pwaitsixtieths"), clov_waitsixtieths);

    #undef ADD_VERB

    pophashtable();
    return true;
}
