#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the mrcalendar processor */
enum {
    mrcv_mrcalendar_verb0 = 0,
    mrcv_mrcalendar_verb1 = 1,
    mrcv_mrcalendar_verb2 = 2,
    mrcv_mrcalendar_verb3 = 3,
    mrcv_mrcalendar_verb4 = 4,
    mrcv_mrcalendar_verb5 = 5,
    mrcv_mrcalendar_verb6 = 6,
    mrcv_mrcalendar_verb7 = 7,
    mrcv_mrcalendar_verb8 = 8,
    mrcv_mrcalendar_verb9 = 9,
    mrcv_mrcalendar_verb10 = 10
};

static boolean mrcalendar_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case mrcv_mrcalendar_verb0:
            /* TODO: Implement mrcalendar.mrcalendar_verb0 */
            return false;
        case mrcv_mrcalendar_verb1:
            /* TODO: Implement mrcalendar.mrcalendar_verb1 */
            return false;
        case mrcv_mrcalendar_verb2:
            /* TODO: Implement mrcalendar.mrcalendar_verb2 */
            return false;
        case mrcv_mrcalendar_verb3:
            /* TODO: Implement mrcalendar.mrcalendar_verb3 */
            return false;
        case mrcv_mrcalendar_verb4:
            /* TODO: Implement mrcalendar.mrcalendar_verb4 */
            return false;
        case mrcv_mrcalendar_verb5:
            /* TODO: Implement mrcalendar.mrcalendar_verb5 */
            return false;
        case mrcv_mrcalendar_verb6:
            /* TODO: Implement mrcalendar.mrcalendar_verb6 */
            return false;
        case mrcv_mrcalendar_verb7:
            /* TODO: Implement mrcalendar.mrcalendar_verb7 */
            return false;
        case mrcv_mrcalendar_verb8:
            /* TODO: Implement mrcalendar.mrcalendar_verb8 */
            return false;
        case mrcv_mrcalendar_verb9:
            /* TODO: Implement mrcalendar.mrcalendar_verb9 */
            return false;
        case mrcv_mrcalendar_verb10:
            /* TODO: Implement mrcalendar.mrcalendar_verb10 */
            return false;
        default:
            return false;
    }
}

boolean mrcalendarinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pmrcalendar"), bsname);

    if (!newfunctionprocessor(bsname, &mrcalendar_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pmrcalendar_verb0"), mrcv_mrcalendar_verb0);
    ADD_VERB(BIGSTRING("\pmrcalendar_verb1"), mrcv_mrcalendar_verb1);
    ADD_VERB(BIGSTRING("\pmrcalendar_verb2"), mrcv_mrcalendar_verb2);
    ADD_VERB(BIGSTRING("\pmrcalendar_verb3"), mrcv_mrcalendar_verb3);
    ADD_VERB(BIGSTRING("\pmrcalendar_verb4"), mrcv_mrcalendar_verb4);
    ADD_VERB(BIGSTRING("\pmrcalendar_verb5"), mrcv_mrcalendar_verb5);
    ADD_VERB(BIGSTRING("\pmrcalendar_verb6"), mrcv_mrcalendar_verb6);
    ADD_VERB(BIGSTRING("\pmrcalendar_verb7"), mrcv_mrcalendar_verb7);
    ADD_VERB(BIGSTRING("\pmrcalendar_verb8"), mrcv_mrcalendar_verb8);
    ADD_VERB(BIGSTRING("\pmrcalendar_verb9"), mrcv_mrcalendar_verb9);
    ADD_VERB(BIGSTRING("\pmrcalendar_verb10"), mrcv_mrcalendar_verb10);

    #undef ADD_VERB

    pophashtable();
    return true;
}
