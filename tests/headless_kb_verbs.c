#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the kb processor */
enum {
    kbv_optionkey = 0,
    kbv_cmdkey = 1,
    kbv_shiftkey = 2,
    kbv_controlkey = 3
};

static boolean kb_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case kbv_optionkey:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case kbv_cmdkey:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case kbv_shiftkey:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case kbv_controlkey:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean kbinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pkb"), bsname);

    if (!newfunctionprocessor(bsname, &kb_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\poptionkey"), kbv_optionkey);
    ADD_VERB(BIGSTRING("\pcmdkey"), kbv_cmdkey);
    ADD_VERB(BIGSTRING("\pshiftkey"), kbv_shiftkey);
    ADD_VERB(BIGSTRING("\pcontrolkey"), kbv_controlkey);

    #undef ADD_VERB

    pophashtable();
    return true;
}
