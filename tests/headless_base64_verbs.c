#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the base64 processor */
enum {
    basv_encode = 0,
    basv_decode = 1
};

static boolean base64_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case basv_encode:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case basv_decode:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
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

    ADD_VERB(BIGSTRING("\pencode"), basv_encode);
    ADD_VERB(BIGSTRING("\pdecode"), basv_decode);

    #undef ADD_VERB

    pophashtable();
    return true;
}
