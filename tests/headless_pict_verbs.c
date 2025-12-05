#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the pict processor */
enum {
    picv_pict_verb0 = 0,
    picv_pict_verb1 = 1,
    picv_pict_verb2 = 2,
    picv_pict_verb3 = 3
};

static boolean pict_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case picv_pict_verb0:
            /* TODO: Implement pict.pict_verb0 */
            return false;
        case picv_pict_verb1:
            /* TODO: Implement pict.pict_verb1 */
            return false;
        case picv_pict_verb2:
            /* TODO: Implement pict.pict_verb2 */
            return false;
        case picv_pict_verb3:
            /* TODO: Implement pict.pict_verb3 */
            return false;
        default:
            return false;
    }
}

boolean pictinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\ppict"), bsname);

    if (!newfunctionprocessor(bsname, &pict_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\ppict_verb0"), picv_pict_verb0);
    ADD_VERB(BIGSTRING("\ppict_verb1"), picv_pict_verb1);
    ADD_VERB(BIGSTRING("\ppict_verb2"), picv_pict_verb2);
    ADD_VERB(BIGSTRING("\ppict_verb3"), picv_pict_verb3);

    #undef ADD_VERB

    pophashtable();
    return true;
}
