#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the sys processor */
enum {
    sysv_sys_verb0 = 0,
    sysv_sys_verb1 = 1,
    sysv_sys_verb2 = 2,
    sysv_sys_verb3 = 3,
    sysv_sys_verb4 = 4,
    sysv_sys_verb5 = 5,
    sysv_sys_verb6 = 6,
    sysv_sys_verb7 = 7,
    sysv_sys_verb8 = 8,
    sysv_sys_verb9 = 9,
    sysv_sys_verb10 = 10,
    sysv_sys_verb11 = 11,
    sysv_sys_verb12 = 12,
    sysv_sys_verb13 = 13,
    sysv_sys_verb14 = 14,
    sysv_sys_verb15 = 15
};

static boolean sys_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case sysv_sys_verb0:
            /* TODO: Implement sys.sys_verb0 */
            return false;
        case sysv_sys_verb1:
            /* TODO: Implement sys.sys_verb1 */
            return false;
        case sysv_sys_verb2:
            /* TODO: Implement sys.sys_verb2 */
            return false;
        case sysv_sys_verb3:
            /* TODO: Implement sys.sys_verb3 */
            return false;
        case sysv_sys_verb4:
            /* TODO: Implement sys.sys_verb4 */
            return false;
        case sysv_sys_verb5:
            /* TODO: Implement sys.sys_verb5 */
            return false;
        case sysv_sys_verb6:
            /* TODO: Implement sys.sys_verb6 */
            return false;
        case sysv_sys_verb7:
            /* TODO: Implement sys.sys_verb7 */
            return false;
        case sysv_sys_verb8:
            /* TODO: Implement sys.sys_verb8 */
            return false;
        case sysv_sys_verb9:
            /* TODO: Implement sys.sys_verb9 */
            return false;
        case sysv_sys_verb10:
            /* TODO: Implement sys.sys_verb10 */
            return false;
        case sysv_sys_verb11:
            /* TODO: Implement sys.sys_verb11 */
            return false;
        case sysv_sys_verb12:
            /* TODO: Implement sys.sys_verb12 */
            return false;
        case sysv_sys_verb13:
            /* TODO: Implement sys.sys_verb13 */
            return false;
        case sysv_sys_verb14:
            /* TODO: Implement sys.sys_verb14 */
            return false;
        case sysv_sys_verb15:
            /* TODO: Implement sys.sys_verb15 */
            return false;
        default:
            return false;
    }
}

boolean sysinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\psys"), bsname);

    if (!newfunctionprocessor(bsname, &sys_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\psys_verb0"), sysv_sys_verb0);
    ADD_VERB(BIGSTRING("\psys_verb1"), sysv_sys_verb1);
    ADD_VERB(BIGSTRING("\psys_verb2"), sysv_sys_verb2);
    ADD_VERB(BIGSTRING("\psys_verb3"), sysv_sys_verb3);
    ADD_VERB(BIGSTRING("\psys_verb4"), sysv_sys_verb4);
    ADD_VERB(BIGSTRING("\psys_verb5"), sysv_sys_verb5);
    ADD_VERB(BIGSTRING("\psys_verb6"), sysv_sys_verb6);
    ADD_VERB(BIGSTRING("\psys_verb7"), sysv_sys_verb7);
    ADD_VERB(BIGSTRING("\psys_verb8"), sysv_sys_verb8);
    ADD_VERB(BIGSTRING("\psys_verb9"), sysv_sys_verb9);
    ADD_VERB(BIGSTRING("\psys_verb10"), sysv_sys_verb10);
    ADD_VERB(BIGSTRING("\psys_verb11"), sysv_sys_verb11);
    ADD_VERB(BIGSTRING("\psys_verb12"), sysv_sys_verb12);
    ADD_VERB(BIGSTRING("\psys_verb13"), sysv_sys_verb13);
    ADD_VERB(BIGSTRING("\psys_verb14"), sysv_sys_verb14);
    ADD_VERB(BIGSTRING("\psys_verb15"), sysv_sys_verb15);

    #undef ADD_VERB

    pophashtable();
    return true;
}
