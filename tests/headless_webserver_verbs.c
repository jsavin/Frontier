#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the webserver processor */
enum {
    webv_supervisor = 0,
    webv_re = 1,
    webv_compile = 2,
    webv_match = 3,
    webv_replace = 4,
    webv_extract = 5,
    webv_split = 6
};

static boolean webserver_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case webv_supervisor:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case webv_re:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case webv_compile:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case webv_match:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case webv_replace:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case webv_extract:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case webv_split:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean webserverinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pwebserver"), bsname);

    if (!newfunctionprocessor(bsname, &webserver_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\psupervisor"), webv_supervisor);
    ADD_VERB(BIGSTRING("\pre"), webv_re);
    ADD_VERB(BIGSTRING("\pcompile"), webv_compile);
    ADD_VERB(BIGSTRING("\pmatch"), webv_match);
    ADD_VERB(BIGSTRING("\preplace"), webv_replace);
    ADD_VERB(BIGSTRING("\pextract"), webv_extract);
    ADD_VERB(BIGSTRING("\psplit"), webv_split);

    #undef ADD_VERB

    pophashtable();
    return true;
}
