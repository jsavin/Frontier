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
            /* TODO: Implement op.op_verb0 */
            return false;
        case opv_op_verb1:
            /* TODO: Implement op.op_verb1 */
            return false;
        case opv_op_verb2:
            /* TODO: Implement op.op_verb2 */
            return false;
        case opv_op_verb3:
            /* TODO: Implement op.op_verb3 */
            return false;
        case opv_op_verb4:
            /* TODO: Implement op.op_verb4 */
            return false;
        case opv_op_verb5:
            /* TODO: Implement op.op_verb5 */
            return false;
        case opv_op_verb6:
            /* TODO: Implement op.op_verb6 */
            return false;
        case opv_op_verb7:
            /* TODO: Implement op.op_verb7 */
            return false;
        case opv_op_verb8:
            /* TODO: Implement op.op_verb8 */
            return false;
        case opv_op_verb9:
            /* TODO: Implement op.op_verb9 */
            return false;
        case opv_op_verb10:
            /* TODO: Implement op.op_verb10 */
            return false;
        case opv_op_verb11:
            /* TODO: Implement op.op_verb11 */
            return false;
        case opv_op_verb12:
            /* TODO: Implement op.op_verb12 */
            return false;
        case opv_op_verb13:
            /* TODO: Implement op.op_verb13 */
            return false;
        case opv_op_verb14:
            /* TODO: Implement op.op_verb14 */
            return false;
        case opv_op_verb15:
            /* TODO: Implement op.op_verb15 */
            return false;
        case opv_op_verb16:
            /* TODO: Implement op.op_verb16 */
            return false;
        case opv_op_verb17:
            /* TODO: Implement op.op_verb17 */
            return false;
        case opv_op_verb18:
            /* TODO: Implement op.op_verb18 */
            return false;
        case opv_op_verb19:
            /* TODO: Implement op.op_verb19 */
            return false;
        case opv_op_verb20:
            /* TODO: Implement op.op_verb20 */
            return false;
        case opv_op_verb21:
            /* TODO: Implement op.op_verb21 */
            return false;
        case opv_op_verb22:
            /* TODO: Implement op.op_verb22 */
            return false;
        case opv_op_verb23:
            /* TODO: Implement op.op_verb23 */
            return false;
        case opv_op_verb24:
            /* TODO: Implement op.op_verb24 */
            return false;
        case opv_op_verb25:
            /* TODO: Implement op.op_verb25 */
            return false;
        case opv_op_verb26:
            /* TODO: Implement op.op_verb26 */
            return false;
        case opv_op_verb27:
            /* TODO: Implement op.op_verb27 */
            return false;
        case opv_op_verb28:
            /* TODO: Implement op.op_verb28 */
            return false;
        case opv_op_verb29:
            /* TODO: Implement op.op_verb29 */
            return false;
        case opv_op_verb30:
            /* TODO: Implement op.op_verb30 */
            return false;
        case opv_op_verb31:
            /* TODO: Implement op.op_verb31 */
            return false;
        case opv_op_verb32:
            /* TODO: Implement op.op_verb32 */
            return false;
        case opv_op_verb33:
            /* TODO: Implement op.op_verb33 */
            return false;
        case opv_op_verb34:
            /* TODO: Implement op.op_verb34 */
            return false;
        case opv_op_verb35:
            /* TODO: Implement op.op_verb35 */
            return false;
        case opv_op_verb36:
            /* TODO: Implement op.op_verb36 */
            return false;
        case opv_op_verb37:
            /* TODO: Implement op.op_verb37 */
            return false;
        case opv_op_verb38:
            /* TODO: Implement op.op_verb38 */
            return false;
        case opv_op_verb39:
            /* TODO: Implement op.op_verb39 */
            return false;
        case opv_op_verb40:
            /* TODO: Implement op.op_verb40 */
            return false;
        case opv_op_verb41:
            /* TODO: Implement op.op_verb41 */
            return false;
        case opv_op_verb42:
            /* TODO: Implement op.op_verb42 */
            return false;
        case opv_op_verb43:
            /* TODO: Implement op.op_verb43 */
            return false;
        case opv_op_verb44:
            /* TODO: Implement op.op_verb44 */
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
