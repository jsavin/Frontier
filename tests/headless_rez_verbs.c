#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the rez processor */
enum {
    rezv_rez_verb0 = 0,
    rezv_rez_verb1 = 1,
    rezv_rez_verb2 = 2,
    rezv_rez_verb3 = 3,
    rezv_rez_verb4 = 4,
    rezv_rez_verb5 = 5,
    rezv_rez_verb6 = 6,
    rezv_rez_verb7 = 7,
    rezv_rez_verb8 = 8,
    rezv_rez_verb9 = 9,
    rezv_rez_verb10 = 10,
    rezv_rez_verb11 = 11,
    rezv_rez_verb12 = 12,
    rezv_rez_verb13 = 13,
    rezv_rez_verb14 = 14
};

static boolean rez_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case rezv_rez_verb0:
            /* TODO: Implement rez.rez_verb0 */
            return false;
        case rezv_rez_verb1:
            /* TODO: Implement rez.rez_verb1 */
            return false;
        case rezv_rez_verb2:
            /* TODO: Implement rez.rez_verb2 */
            return false;
        case rezv_rez_verb3:
            /* TODO: Implement rez.rez_verb3 */
            return false;
        case rezv_rez_verb4:
            /* TODO: Implement rez.rez_verb4 */
            return false;
        case rezv_rez_verb5:
            /* TODO: Implement rez.rez_verb5 */
            return false;
        case rezv_rez_verb6:
            /* TODO: Implement rez.rez_verb6 */
            return false;
        case rezv_rez_verb7:
            /* TODO: Implement rez.rez_verb7 */
            return false;
        case rezv_rez_verb8:
            /* TODO: Implement rez.rez_verb8 */
            return false;
        case rezv_rez_verb9:
            /* TODO: Implement rez.rez_verb9 */
            return false;
        case rezv_rez_verb10:
            /* TODO: Implement rez.rez_verb10 */
            return false;
        case rezv_rez_verb11:
            /* TODO: Implement rez.rez_verb11 */
            return false;
        case rezv_rez_verb12:
            /* TODO: Implement rez.rez_verb12 */
            return false;
        case rezv_rez_verb13:
            /* TODO: Implement rez.rez_verb13 */
            return false;
        case rezv_rez_verb14:
            /* TODO: Implement rez.rez_verb14 */
            return false;
        default:
            return false;
    }
}

boolean rezinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\prez"), bsname);

    if (!newfunctionprocessor(bsname, &rez_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\prez_verb0"), rezv_rez_verb0);
    ADD_VERB(BIGSTRING("\prez_verb1"), rezv_rez_verb1);
    ADD_VERB(BIGSTRING("\prez_verb2"), rezv_rez_verb2);
    ADD_VERB(BIGSTRING("\prez_verb3"), rezv_rez_verb3);
    ADD_VERB(BIGSTRING("\prez_verb4"), rezv_rez_verb4);
    ADD_VERB(BIGSTRING("\prez_verb5"), rezv_rez_verb5);
    ADD_VERB(BIGSTRING("\prez_verb6"), rezv_rez_verb6);
    ADD_VERB(BIGSTRING("\prez_verb7"), rezv_rez_verb7);
    ADD_VERB(BIGSTRING("\prez_verb8"), rezv_rez_verb8);
    ADD_VERB(BIGSTRING("\prez_verb9"), rezv_rez_verb9);
    ADD_VERB(BIGSTRING("\prez_verb10"), rezv_rez_verb10);
    ADD_VERB(BIGSTRING("\prez_verb11"), rezv_rez_verb11);
    ADD_VERB(BIGSTRING("\prez_verb12"), rezv_rez_verb12);
    ADD_VERB(BIGSTRING("\prez_verb13"), rezv_rez_verb13);
    ADD_VERB(BIGSTRING("\prez_verb14"), rezv_rez_verb14);

    #undef ADD_VERB

    pophashtable();
    return true;
}
