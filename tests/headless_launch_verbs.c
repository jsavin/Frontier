#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the launch processor */
enum {
    lauv_launch_verb0 = 0,
    lauv_launch_verb1 = 1,
    lauv_launch_verb2 = 2,
    lauv_launch_verb3 = 3,
    lauv_launch_verb4 = 4
};

static boolean launch_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case lauv_launch_verb0:
            /* TODO: Implement launch.launch_verb0 */
            return false;
        case lauv_launch_verb1:
            /* TODO: Implement launch.launch_verb1 */
            return false;
        case lauv_launch_verb2:
            /* TODO: Implement launch.launch_verb2 */
            return false;
        case lauv_launch_verb3:
            /* TODO: Implement launch.launch_verb3 */
            return false;
        case lauv_launch_verb4:
            /* TODO: Implement launch.launch_verb4 */
            return false;
        default:
            return false;
    }
}

boolean launchinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\plaunch"), bsname);

    if (!newfunctionprocessor(bsname, &launch_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\plaunch_verb0"), lauv_launch_verb0);
    ADD_VERB(BIGSTRING("\plaunch_verb1"), lauv_launch_verb1);
    ADD_VERB(BIGSTRING("\plaunch_verb2"), lauv_launch_verb2);
    ADD_VERB(BIGSTRING("\plaunch_verb3"), lauv_launch_verb3);
    ADD_VERB(BIGSTRING("\plaunch_verb4"), lauv_launch_verb4);

    #undef ADD_VERB

    pophashtable();
    return true;
}
