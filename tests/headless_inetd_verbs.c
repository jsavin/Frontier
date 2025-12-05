#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the inetd processor */
enum {
    inev_supervisor = 0
};

static boolean inetd_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case inev_supervisor:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean inetdinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pinetd"), bsname);

    if (!newfunctionprocessor(bsname, &inetd_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\psupervisor"), inev_supervisor);

    #undef ADD_VERB

    pophashtable();
    return true;
}
