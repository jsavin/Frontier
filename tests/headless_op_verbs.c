#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the op processor */
enum {
    opv_op_verb0 = 0,
    opv_op_verb1 = 1,
    opv_op_verb2 = 2,
    opv_op_verb3 = 3,
    opv_op_verb4 = 4,
    opv_op_verb5 = 5,
    opv_op_verb6 = 6,
    opv_op_verb7 = 7,
    opv_op_verb8 = 8,
    opv_op_verb9 = 9,
    opv_op_verb10 = 10,
    opv_op_verb11 = 11,
    opv_op_verb12 = 12,
    opv_op_verb13 = 13,
    opv_op_verb14 = 14,
    opv_op_verb15 = 15,
    opv_op_verb16 = 16,
    opv_op_verb17 = 17,
    opv_op_verb18 = 18,
    opv_op_verb19 = 19,
    opv_op_verb20 = 20,
    opv_op_verb21 = 21,
    opv_op_verb22 = 22,
    opv_op_verb23 = 23,
    opv_op_verb24 = 24,
    opv_op_verb25 = 25,
    opv_op_verb26 = 26,
    opv_op_verb27 = 27,
    opv_op_verb28 = 28,
    opv_op_verb29 = 29,
    opv_op_verb30 = 30,
    opv_op_verb31 = 31,
    opv_op_verb32 = 32,
    opv_op_verb33 = 33,
    opv_op_verb34 = 34,
    opv_op_verb35 = 35,
    opv_op_verb36 = 36,
    opv_op_verb37 = 37,
    opv_op_verb38 = 38,
    opv_op_verb39 = 39,
    opv_op_verb40 = 40,
    opv_op_verb41 = 41,
    opv_op_verb42 = 42,
    opv_op_verb43 = 43,
    opv_op_verb44 = 44
};

static boolean op_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case opv_op_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb2:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb3:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb4:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb5:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb6:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb7:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb8:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb9:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb10:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb11:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb12:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb13:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb14:
            /* Verb #14 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb15:
            /* Verb #15 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb16:
            /* Verb #16 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb17:
            /* Verb #17 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb18:
            /* Verb #18 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb19:
            /* Verb #19 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb20:
            /* Verb #20 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb21:
            /* Verb #21 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb22:
            /* Verb #22 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb23:
            /* Verb #23 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb24:
            /* Verb #24 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb25:
            /* Verb #25 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb26:
            /* Verb #26 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb27:
            /* Verb #27 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb28:
            /* Verb #28 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb29:
            /* Verb #29 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb30:
            /* Verb #30 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb31:
            /* Verb #31 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb32:
            /* Verb #32 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb33:
            /* Verb #33 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb34:
            /* Verb #34 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb35:
            /* Verb #35 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb36:
            /* Verb #36 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb37:
            /* Verb #37 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb38:
            /* Verb #38 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb39:
            /* Verb #39 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb40:
            /* Verb #40 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb41:
            /* Verb #41 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb42:
            /* Verb #42 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb43:
            /* Verb #43 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_op_verb44:
            /* Verb #44 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean opinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pop"), bsname);

    if (!newfunctionprocessor(bsname, &op_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pop_verb0"), opv_op_verb0);
    ADD_VERB(BIGSTRING("\pop_verb1"), opv_op_verb1);
    ADD_VERB(BIGSTRING("\pop_verb2"), opv_op_verb2);
    ADD_VERB(BIGSTRING("\pop_verb3"), opv_op_verb3);
    ADD_VERB(BIGSTRING("\pop_verb4"), opv_op_verb4);
    ADD_VERB(BIGSTRING("\pop_verb5"), opv_op_verb5);
    ADD_VERB(BIGSTRING("\pop_verb6"), opv_op_verb6);
    ADD_VERB(BIGSTRING("\pop_verb7"), opv_op_verb7);
    ADD_VERB(BIGSTRING("\pop_verb8"), opv_op_verb8);
    ADD_VERB(BIGSTRING("\pop_verb9"), opv_op_verb9);
    ADD_VERB(BIGSTRING("\pop_verb10"), opv_op_verb10);
    ADD_VERB(BIGSTRING("\pop_verb11"), opv_op_verb11);
    ADD_VERB(BIGSTRING("\pop_verb12"), opv_op_verb12);
    ADD_VERB(BIGSTRING("\pop_verb13"), opv_op_verb13);
    ADD_VERB(BIGSTRING("\pop_verb14"), opv_op_verb14);
    ADD_VERB(BIGSTRING("\pop_verb15"), opv_op_verb15);
    ADD_VERB(BIGSTRING("\pop_verb16"), opv_op_verb16);
    ADD_VERB(BIGSTRING("\pop_verb17"), opv_op_verb17);
    ADD_VERB(BIGSTRING("\pop_verb18"), opv_op_verb18);
    ADD_VERB(BIGSTRING("\pop_verb19"), opv_op_verb19);
    ADD_VERB(BIGSTRING("\pop_verb20"), opv_op_verb20);
    ADD_VERB(BIGSTRING("\pop_verb21"), opv_op_verb21);
    ADD_VERB(BIGSTRING("\pop_verb22"), opv_op_verb22);
    ADD_VERB(BIGSTRING("\pop_verb23"), opv_op_verb23);
    ADD_VERB(BIGSTRING("\pop_verb24"), opv_op_verb24);
    ADD_VERB(BIGSTRING("\pop_verb25"), opv_op_verb25);
    ADD_VERB(BIGSTRING("\pop_verb26"), opv_op_verb26);
    ADD_VERB(BIGSTRING("\pop_verb27"), opv_op_verb27);
    ADD_VERB(BIGSTRING("\pop_verb28"), opv_op_verb28);
    ADD_VERB(BIGSTRING("\pop_verb29"), opv_op_verb29);
    ADD_VERB(BIGSTRING("\pop_verb30"), opv_op_verb30);
    ADD_VERB(BIGSTRING("\pop_verb31"), opv_op_verb31);
    ADD_VERB(BIGSTRING("\pop_verb32"), opv_op_verb32);
    ADD_VERB(BIGSTRING("\pop_verb33"), opv_op_verb33);
    ADD_VERB(BIGSTRING("\pop_verb34"), opv_op_verb34);
    ADD_VERB(BIGSTRING("\pop_verb35"), opv_op_verb35);
    ADD_VERB(BIGSTRING("\pop_verb36"), opv_op_verb36);
    ADD_VERB(BIGSTRING("\pop_verb37"), opv_op_verb37);
    ADD_VERB(BIGSTRING("\pop_verb38"), opv_op_verb38);
    ADD_VERB(BIGSTRING("\pop_verb39"), opv_op_verb39);
    ADD_VERB(BIGSTRING("\pop_verb40"), opv_op_verb40);
    ADD_VERB(BIGSTRING("\pop_verb41"), opv_op_verb41);
    ADD_VERB(BIGSTRING("\pop_verb42"), opv_op_verb42);
    ADD_VERB(BIGSTRING("\pop_verb43"), opv_op_verb43);
    ADD_VERB(BIGSTRING("\pop_verb44"), opv_op_verb44);

    #undef ADD_VERB

    pophashtable();
    return true;
}
