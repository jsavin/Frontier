#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the base64 processor */
enum {
    basv_base64_verb0 = 0,
    basv_base64_verb1 = 1
};

static boolean base64_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case basv_base64_verb0:
            /* TODO: Implement base64.base64_verb0 */
            return false;
        case basv_base64_verb1:
            /* TODO: Implement base64.base64_verb1 */
            return false;
        default:
            return false;
    }
}

boolean base64initverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pbase64"), bsname);

    if (!newfunctionprocessor(bsname, &base64_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pbase64_verb0"), basv_base64_verb0);
    ADD_VERB(BIGSTRING("\pbase64_verb1"), basv_base64_verb1);

    #undef ADD_VERB

    pophashtable();
    return true;
}
