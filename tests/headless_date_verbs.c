#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the date processor */
enum {
    datv_date_verb0 = 0,
    datv_date_verb1 = 1,
    datv_date_verb2 = 2,
    datv_date_verb3 = 3,
    datv_date_verb4 = 4,
    datv_date_verb5 = 5,
    datv_date_verb6 = 6,
    datv_date_verb7 = 7,
    datv_date_verb8 = 8,
    datv_date_verb9 = 9,
    datv_date_verb10 = 10,
    datv_date_verb11 = 11,
    datv_date_verb12 = 12,
    datv_date_verb13 = 13,
    datv_date_verb14 = 14,
    datv_date_verb15 = 15,
    datv_date_verb16 = 16,
    datv_date_verb17 = 17,
    datv_date_verb18 = 18,
    datv_date_verb19 = 19,
    datv_date_verb20 = 20,
    datv_date_verb21 = 21,
    datv_date_verb22 = 22,
    datv_date_verb23 = 23,
    datv_date_verb24 = 24,
    datv_date_verb25 = 25,
    datv_date_verb26 = 26,
    datv_date_verb27 = 27,
    datv_date_verb28 = 28,
    datv_date_verb29 = 29
};

static boolean date_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case datv_date_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb2:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb3:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb4:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb5:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb6:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb7:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb8:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb9:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb10:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb11:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb12:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb13:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb14:
            /* Verb #14 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb15:
            /* Verb #15 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb16:
            /* Verb #16 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb17:
            /* Verb #17 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb18:
            /* Verb #18 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb19:
            /* Verb #19 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb20:
            /* Verb #20 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb21:
            /* Verb #21 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb22:
            /* Verb #22 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb23:
            /* Verb #23 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb24:
            /* Verb #24 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb25:
            /* Verb #25 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb26:
            /* Verb #26 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb27:
            /* Verb #27 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb28:
            /* Verb #28 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case datv_date_verb29:
            /* Verb #29 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean dateinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pdate"), bsname);

    if (!newfunctionprocessor(bsname, &date_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pdate_verb0"), datv_date_verb0);
    ADD_VERB(BIGSTRING("\pdate_verb1"), datv_date_verb1);
    ADD_VERB(BIGSTRING("\pdate_verb2"), datv_date_verb2);
    ADD_VERB(BIGSTRING("\pdate_verb3"), datv_date_verb3);
    ADD_VERB(BIGSTRING("\pdate_verb4"), datv_date_verb4);
    ADD_VERB(BIGSTRING("\pdate_verb5"), datv_date_verb5);
    ADD_VERB(BIGSTRING("\pdate_verb6"), datv_date_verb6);
    ADD_VERB(BIGSTRING("\pdate_verb7"), datv_date_verb7);
    ADD_VERB(BIGSTRING("\pdate_verb8"), datv_date_verb8);
    ADD_VERB(BIGSTRING("\pdate_verb9"), datv_date_verb9);
    ADD_VERB(BIGSTRING("\pdate_verb10"), datv_date_verb10);
    ADD_VERB(BIGSTRING("\pdate_verb11"), datv_date_verb11);
    ADD_VERB(BIGSTRING("\pdate_verb12"), datv_date_verb12);
    ADD_VERB(BIGSTRING("\pdate_verb13"), datv_date_verb13);
    ADD_VERB(BIGSTRING("\pdate_verb14"), datv_date_verb14);
    ADD_VERB(BIGSTRING("\pdate_verb15"), datv_date_verb15);
    ADD_VERB(BIGSTRING("\pdate_verb16"), datv_date_verb16);
    ADD_VERB(BIGSTRING("\pdate_verb17"), datv_date_verb17);
    ADD_VERB(BIGSTRING("\pdate_verb18"), datv_date_verb18);
    ADD_VERB(BIGSTRING("\pdate_verb19"), datv_date_verb19);
    ADD_VERB(BIGSTRING("\pdate_verb20"), datv_date_verb20);
    ADD_VERB(BIGSTRING("\pdate_verb21"), datv_date_verb21);
    ADD_VERB(BIGSTRING("\pdate_verb22"), datv_date_verb22);
    ADD_VERB(BIGSTRING("\pdate_verb23"), datv_date_verb23);
    ADD_VERB(BIGSTRING("\pdate_verb24"), datv_date_verb24);
    ADD_VERB(BIGSTRING("\pdate_verb25"), datv_date_verb25);
    ADD_VERB(BIGSTRING("\pdate_verb26"), datv_date_verb26);
    ADD_VERB(BIGSTRING("\pdate_verb27"), datv_date_verb27);
    ADD_VERB(BIGSTRING("\pdate_verb28"), datv_date_verb28);
    ADD_VERB(BIGSTRING("\pdate_verb29"), datv_date_verb29);

    #undef ADD_VERB

    pophashtable();
    return true;
}
