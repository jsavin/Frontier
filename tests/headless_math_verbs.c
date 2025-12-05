#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the math processor */
enum {
    matv_math_verb0 = 0,
    matv_math_verb1 = 1,
    matv_math_verb2 = 2
};

static boolean math_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case matv_math_verb0:
            /* TODO: Implement math.math_verb0 */
            return false;
        case matv_math_verb1:
            /* TODO: Implement math.math_verb1 */
            return false;
        case matv_math_verb2:
            /* TODO: Implement math.math_verb2 */
            return false;
        default:
            return false;
    }
}

boolean mathinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pmath"), bsname);

    if (!newfunctionprocessor(bsname, &math_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pmath_verb0"), matv_math_verb0);
    ADD_VERB(BIGSTRING("\pmath_verb1"), matv_math_verb1);
    ADD_VERB(BIGSTRING("\pmath_verb2"), matv_math_verb2);

    #undef ADD_VERB

    pophashtable();
    return true;
}
