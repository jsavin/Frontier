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
            /* TODO: Implement date.date_verb0 */
            return false;
        case datv_date_verb1:
            /* TODO: Implement date.date_verb1 */
            return false;
        case datv_date_verb2:
            /* TODO: Implement date.date_verb2 */
            return false;
        case datv_date_verb3:
            /* TODO: Implement date.date_verb3 */
            return false;
        case datv_date_verb4:
            /* TODO: Implement date.date_verb4 */
            return false;
        case datv_date_verb5:
            /* TODO: Implement date.date_verb5 */
            return false;
        case datv_date_verb6:
            /* TODO: Implement date.date_verb6 */
            return false;
        case datv_date_verb7:
            /* TODO: Implement date.date_verb7 */
            return false;
        case datv_date_verb8:
            /* TODO: Implement date.date_verb8 */
            return false;
        case datv_date_verb9:
            /* TODO: Implement date.date_verb9 */
            return false;
        case datv_date_verb10:
            /* TODO: Implement date.date_verb10 */
            return false;
        case datv_date_verb11:
            /* TODO: Implement date.date_verb11 */
            return false;
        case datv_date_verb12:
            /* TODO: Implement date.date_verb12 */
            return false;
        case datv_date_verb13:
            /* TODO: Implement date.date_verb13 */
            return false;
        case datv_date_verb14:
            /* TODO: Implement date.date_verb14 */
            return false;
        case datv_date_verb15:
            /* TODO: Implement date.date_verb15 */
            return false;
        case datv_date_verb16:
            /* TODO: Implement date.date_verb16 */
            return false;
        case datv_date_verb17:
            /* TODO: Implement date.date_verb17 */
            return false;
        case datv_date_verb18:
            /* TODO: Implement date.date_verb18 */
            return false;
        case datv_date_verb19:
            /* TODO: Implement date.date_verb19 */
            return false;
        case datv_date_verb20:
            /* TODO: Implement date.date_verb20 */
            return false;
        case datv_date_verb21:
            /* TODO: Implement date.date_verb21 */
            return false;
        case datv_date_verb22:
            /* TODO: Implement date.date_verb22 */
            return false;
        case datv_date_verb23:
            /* TODO: Implement date.date_verb23 */
            return false;
        case datv_date_verb24:
            /* TODO: Implement date.date_verb24 */
            return false;
        case datv_date_verb25:
            /* TODO: Implement date.date_verb25 */
            return false;
        case datv_date_verb26:
            /* TODO: Implement date.date_verb26 */
            return false;
        case datv_date_verb27:
            /* TODO: Implement date.date_verb27 */
            return false;
        case datv_date_verb28:
            /* TODO: Implement date.date_verb28 */
            return false;
        case datv_date_verb29:
            /* TODO: Implement date.date_verb29 */
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
