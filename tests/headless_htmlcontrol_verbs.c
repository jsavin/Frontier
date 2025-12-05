#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the htmlcontrol processor */
enum {
    htmv_back = 0,
    htmv_forward = 1,
    htmv_refresh = 2,
    htmv_home = 3,
    htmv_stop = 4,
    htmv_navigate = 5,
    htmv_isoffline = 6,
    htmv_setoffline = 7
};

static boolean htmlcontrol_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case htmv_back:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_forward:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_refresh:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_home:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_stop:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_navigate:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_isoffline:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_setoffline:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean htmlcontrolinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\phtmlcontrol"), bsname);

    if (!newfunctionprocessor(bsname, &htmlcontrol_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pback"), htmv_back);
    ADD_VERB(BIGSTRING("\pforward"), htmv_forward);
    ADD_VERB(BIGSTRING("\prefresh"), htmv_refresh);
    ADD_VERB(BIGSTRING("\phome"), htmv_home);
    ADD_VERB(BIGSTRING("\pstop"), htmv_stop);
    ADD_VERB(BIGSTRING("\pnavigate"), htmv_navigate);
    ADD_VERB(BIGSTRING("\pisoffline"), htmv_isoffline);
    ADD_VERB(BIGSTRING("\psetoffline"), htmv_setoffline);

    #undef ADD_VERB

    pophashtable();
    return true;
}
