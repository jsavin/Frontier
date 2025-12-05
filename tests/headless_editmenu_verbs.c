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
            /* TODO: Implement editmenu.editmenu_verb0 */
            return false;
        case ediv_editmenu_verb1:
            /* TODO: Implement editmenu.editmenu_verb1 */
            return false;
        case ediv_editmenu_verb2:
            /* TODO: Implement editmenu.editmenu_verb2 */
            return false;
        case ediv_editmenu_verb3:
            /* TODO: Implement editmenu.editmenu_verb3 */
            return false;
        case ediv_editmenu_verb4:
            /* TODO: Implement editmenu.editmenu_verb4 */
            return false;
        case ediv_editmenu_verb5:
            /* TODO: Implement editmenu.editmenu_verb5 */
            return false;
        case ediv_editmenu_verb6:
            /* TODO: Implement editmenu.editmenu_verb6 */
            return false;
        case ediv_editmenu_verb7:
            /* TODO: Implement editmenu.editmenu_verb7 */
            return false;
        case ediv_editmenu_verb8:
            /* TODO: Implement editmenu.editmenu_verb8 */
            return false;
        case ediv_editmenu_verb9:
            /* TODO: Implement editmenu.editmenu_verb9 */
            return false;
        case ediv_editmenu_verb10:
            /* TODO: Implement editmenu.editmenu_verb10 */
            return false;
        case ediv_editmenu_verb11:
            /* TODO: Implement editmenu.editmenu_verb11 */
            return false;
        case ediv_editmenu_verb12:
            /* TODO: Implement editmenu.editmenu_verb12 */
            return false;
        case ediv_editmenu_verb13:
            /* TODO: Implement editmenu.editmenu_verb13 */
            return false;
        case ediv_editmenu_verb14:
            /* TODO: Implement editmenu.editmenu_verb14 */
            return false;
        case ediv_editmenu_verb15:
            /* TODO: Implement editmenu.editmenu_verb15 */
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
