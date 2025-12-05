#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the rgb processor */
enum {
    rgbv_rgb_verb0 = 0,
    rgbv_rgb_verb1 = 1
};

static boolean rgb_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case rgbv_rgb_verb0:
            /* TODO: Implement rgb.rgb_verb0 */
            return false;
        case rgbv_rgb_verb1:
            /* TODO: Implement rgb.rgb_verb1 */
            return false;
        default:
            return false;
    }
}

boolean rgbinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\prgb"), bsname);

    if (!newfunctionprocessor(bsname, &rgb_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\prgb_verb0"), rgbv_rgb_verb0);
    ADD_VERB(BIGSTRING("\prgb_verb1"), rgbv_rgb_verb1);

    #undef ADD_VERB

    pophashtable();
    return true;
}
