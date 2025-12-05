#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the frontier processor */
enum {
    frov_frontier_verb0 = 0,
    frov_frontier_verb1 = 1,
    frov_frontier_verb2 = 2,
    frov_frontier_verb3 = 3,
    frov_frontier_verb4 = 4,
    frov_frontier_verb5 = 5,
    frov_frontier_verb6 = 6,
    frov_frontier_verb7 = 7,
    frov_frontier_verb8 = 8,
    frov_frontier_verb9 = 9,
    frov_frontier_verb10 = 10,
    frov_frontier_verb11 = 11,
    frov_frontier_verb12 = 12,
    frov_frontier_verb13 = 13
};

static boolean frontier_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case frov_frontier_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_frontier_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_frontier_verb2:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_frontier_verb3:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_frontier_verb4:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_frontier_verb5:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_frontier_verb6:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_frontier_verb7:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_frontier_verb8:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_frontier_verb9:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_frontier_verb10:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_frontier_verb11:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_frontier_verb12:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_frontier_verb13:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean frontierinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pfrontier"), bsname);

    if (!newfunctionprocessor(bsname, &frontier_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pfrontier_verb0"), frov_frontier_verb0);
    ADD_VERB(BIGSTRING("\pfrontier_verb1"), frov_frontier_verb1);
    ADD_VERB(BIGSTRING("\pfrontier_verb2"), frov_frontier_verb2);
    ADD_VERB(BIGSTRING("\pfrontier_verb3"), frov_frontier_verb3);
    ADD_VERB(BIGSTRING("\pfrontier_verb4"), frov_frontier_verb4);
    ADD_VERB(BIGSTRING("\pfrontier_verb5"), frov_frontier_verb5);
    ADD_VERB(BIGSTRING("\pfrontier_verb6"), frov_frontier_verb6);
    ADD_VERB(BIGSTRING("\pfrontier_verb7"), frov_frontier_verb7);
    ADD_VERB(BIGSTRING("\pfrontier_verb8"), frov_frontier_verb8);
    ADD_VERB(BIGSTRING("\pfrontier_verb9"), frov_frontier_verb9);
    ADD_VERB(BIGSTRING("\pfrontier_verb10"), frov_frontier_verb10);
    ADD_VERB(BIGSTRING("\pfrontier_verb11"), frov_frontier_verb11);
    ADD_VERB(BIGSTRING("\pfrontier_verb12"), frov_frontier_verb12);
    ADD_VERB(BIGSTRING("\pfrontier_verb13"), frov_frontier_verb13);

    #undef ADD_VERB

    pophashtable();
    return true;
}
