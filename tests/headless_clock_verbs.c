#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the clock processor */
enum {
    clov_clock_verb0 = 0,
    clov_clock_verb1 = 1,
    clov_clock_verb2 = 2,
    clov_clock_verb3 = 3,
    clov_clock_verb4 = 4,
    clov_clock_verb5 = 5,
    clov_clock_verb6 = 6
};

static boolean clock_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case clov_clock_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case clov_clock_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case clov_clock_verb2:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case clov_clock_verb3:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case clov_clock_verb4:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case clov_clock_verb5:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case clov_clock_verb6:
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

    ADD_VERB(BIGSTRING("\pclock_verb0"), clov_clock_verb0);
    ADD_VERB(BIGSTRING("\pclock_verb1"), clov_clock_verb1);
    ADD_VERB(BIGSTRING("\pclock_verb2"), clov_clock_verb2);
    ADD_VERB(BIGSTRING("\pclock_verb3"), clov_clock_verb3);
    ADD_VERB(BIGSTRING("\pclock_verb4"), clov_clock_verb4);
    ADD_VERB(BIGSTRING("\pclock_verb5"), clov_clock_verb5);
    ADD_VERB(BIGSTRING("\pclock_verb6"), clov_clock_verb6);

    #undef ADD_VERB

    pophashtable();
    return true;
}
