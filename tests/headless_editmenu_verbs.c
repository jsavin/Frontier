#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the editmenu processor */
enum {
    ediv_editmenu_verb0 = 0,
    ediv_editmenu_verb1 = 1,
    ediv_editmenu_verb2 = 2,
    ediv_editmenu_verb3 = 3,
    ediv_editmenu_verb4 = 4,
    ediv_editmenu_verb5 = 5,
    ediv_editmenu_verb6 = 6,
    ediv_editmenu_verb7 = 7,
    ediv_editmenu_verb8 = 8,
    ediv_editmenu_verb9 = 9,
    ediv_editmenu_verb10 = 10,
    ediv_editmenu_verb11 = 11,
    ediv_editmenu_verb12 = 12,
    ediv_editmenu_verb13 = 13,
    ediv_editmenu_verb14 = 14,
    ediv_editmenu_verb15 = 15
};

static boolean editmenu_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case ediv_editmenu_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb2:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb3:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb4:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb5:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb6:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb7:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb8:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb9:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb10:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb11:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb12:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb13:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb14:
            /* Verb #14 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_editmenu_verb15:
            /* Verb #15 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean editmenuinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\peditmenu"), bsname);

    if (!newfunctionprocessor(bsname, &editmenu_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\peditmenu_verb0"), ediv_editmenu_verb0);
    ADD_VERB(BIGSTRING("\peditmenu_verb1"), ediv_editmenu_verb1);
    ADD_VERB(BIGSTRING("\peditmenu_verb2"), ediv_editmenu_verb2);
    ADD_VERB(BIGSTRING("\peditmenu_verb3"), ediv_editmenu_verb3);
    ADD_VERB(BIGSTRING("\peditmenu_verb4"), ediv_editmenu_verb4);
    ADD_VERB(BIGSTRING("\peditmenu_verb5"), ediv_editmenu_verb5);
    ADD_VERB(BIGSTRING("\peditmenu_verb6"), ediv_editmenu_verb6);
    ADD_VERB(BIGSTRING("\peditmenu_verb7"), ediv_editmenu_verb7);
    ADD_VERB(BIGSTRING("\peditmenu_verb8"), ediv_editmenu_verb8);
    ADD_VERB(BIGSTRING("\peditmenu_verb9"), ediv_editmenu_verb9);
    ADD_VERB(BIGSTRING("\peditmenu_verb10"), ediv_editmenu_verb10);
    ADD_VERB(BIGSTRING("\peditmenu_verb11"), ediv_editmenu_verb11);
    ADD_VERB(BIGSTRING("\peditmenu_verb12"), ediv_editmenu_verb12);
    ADD_VERB(BIGSTRING("\peditmenu_verb13"), ediv_editmenu_verb13);
    ADD_VERB(BIGSTRING("\peditmenu_verb14"), ediv_editmenu_verb14);
    ADD_VERB(BIGSTRING("\peditmenu_verb15"), ediv_editmenu_verb15);

    #undef ADD_VERB

    pophashtable();
    return true;
}
