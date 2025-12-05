#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the re processor */
enum {
    rev_re_verb0 = 0,
    rev_re_verb1 = 1,
    rev_re_verb2 = 2,
    rev_re_verb3 = 3,
    rev_re_verb4 = 4,
    rev_re_verb5 = 5,
    rev_re_verb6 = 6,
    rev_re_verb7 = 7,
    rev_re_verb8 = 8,
    rev_re_verb9 = 9
};

static boolean re_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case rev_re_verb0:
            /* TODO: Implement re.re_verb0 */
            return false;
        case rev_re_verb1:
            /* TODO: Implement re.re_verb1 */
            return false;
        case rev_re_verb2:
            /* TODO: Implement re.re_verb2 */
            return false;
        case rev_re_verb3:
            /* TODO: Implement re.re_verb3 */
            return false;
        case rev_re_verb4:
            /* TODO: Implement re.re_verb4 */
            return false;
        case rev_re_verb5:
            /* TODO: Implement re.re_verb5 */
            return false;
        case rev_re_verb6:
            /* TODO: Implement re.re_verb6 */
            return false;
        case rev_re_verb7:
            /* TODO: Implement re.re_verb7 */
            return false;
        case rev_re_verb8:
            /* TODO: Implement re.re_verb8 */
            return false;
        case rev_re_verb9:
            /* TODO: Implement re.re_verb9 */
            return false;
        default:
            return false;
    }
}

boolean reinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pre"), bsname);

    if (!newfunctionprocessor(bsname, &re_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pre_verb0"), rev_re_verb0);
    ADD_VERB(BIGSTRING("\pre_verb1"), rev_re_verb1);
    ADD_VERB(BIGSTRING("\pre_verb2"), rev_re_verb2);
    ADD_VERB(BIGSTRING("\pre_verb3"), rev_re_verb3);
    ADD_VERB(BIGSTRING("\pre_verb4"), rev_re_verb4);
    ADD_VERB(BIGSTRING("\pre_verb5"), rev_re_verb5);
    ADD_VERB(BIGSTRING("\pre_verb6"), rev_re_verb6);
    ADD_VERB(BIGSTRING("\pre_verb7"), rev_re_verb7);
    ADD_VERB(BIGSTRING("\pre_verb8"), rev_re_verb8);
    ADD_VERB(BIGSTRING("\pre_verb9"), rev_re_verb9);

    #undef ADD_VERB

    pophashtable();
    return true;
}
