#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the script processor */
enum {
    scrv_compile = 0,
    scrv_uncompile = 1,
    scrv_getcode = 2,
    scrv_getlanguage = 3,
    scrv_setlanguage = 4,
    scrv_makecomment = 5,
    scrv_uncomment = 6,
    scrv_iscomment = 7,
    scrv_getbreakpoint = 8,
    scrv_setbreakpoint = 9,
    scrv_clearbreakpoint = 10,
    scrv_startprofile = 11,
    scrv_stopprofile = 12
};

static boolean script_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case scrv_compile:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_uncompile:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_getcode:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_getlanguage:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_setlanguage:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_makecomment:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_uncomment:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_iscomment:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_getbreakpoint:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_setbreakpoint:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_clearbreakpoint:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_startprofile:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_stopprofile:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean scriptinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pscript"), bsname);

    if (!newfunctionprocessor(bsname, &script_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pcompile"), scrv_compile);
    ADD_VERB(BIGSTRING("\puncompile"), scrv_uncompile);
    ADD_VERB(BIGSTRING("\pgetcode"), scrv_getcode);
    ADD_VERB(BIGSTRING("\pgetlanguage"), scrv_getlanguage);
    ADD_VERB(BIGSTRING("\psetlanguage"), scrv_setlanguage);
    ADD_VERB(BIGSTRING("\pmakecomment"), scrv_makecomment);
    ADD_VERB(BIGSTRING("\puncomment"), scrv_uncomment);
    ADD_VERB(BIGSTRING("\piscomment"), scrv_iscomment);
    ADD_VERB(BIGSTRING("\pgetbreakpoint"), scrv_getbreakpoint);
    ADD_VERB(BIGSTRING("\psetbreakpoint"), scrv_setbreakpoint);
    ADD_VERB(BIGSTRING("\pclearbreakpoint"), scrv_clearbreakpoint);
    ADD_VERB(BIGSTRING("\pstartprofile"), scrv_startprofile);
    ADD_VERB(BIGSTRING("\pstopprofile"), scrv_stopprofile);

    #undef ADD_VERB

    pophashtable();
    return true;
}
