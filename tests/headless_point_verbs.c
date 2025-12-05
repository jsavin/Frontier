#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the point processor */
enum {
    poiv_point_verb0 = 0,
    poiv_point_verb1 = 1
};

static boolean point_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case poiv_point_verb0:
            /* TODO: Implement point.point_verb0 */
            return false;
        case poiv_point_verb1:
            /* TODO: Implement point.point_verb1 */
            return false;
        default:
            return false;
    }
}

boolean pointinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\ppoint"), bsname);

    if (!newfunctionprocessor(bsname, &point_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\ppoint_verb0"), poiv_point_verb0);
    ADD_VERB(BIGSTRING("\ppoint_verb1"), poiv_point_verb1);

    #undef ADD_VERB

    pophashtable();
    return true;
}
