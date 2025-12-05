#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the inetd processor */
enum {
    inev_inetd_verb0 = 0
};

static boolean inetd_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case inev_inetd_verb0:
            /* TODO: Implement inetd.inetd_verb0 */
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

    ADD_VERB(BIGSTRING("\pinetd_verb0"), inev_inetd_verb0);

    #undef ADD_VERB

    pophashtable();
    return true;
}
