#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the menu processor */
enum {
    menv_now = 0,
    menv_set = 1,
    menv_sleepfor = 2,
    menv_ticks = 3,
    menv_milliseconds = 4,
    menv_waitseconds = 5,
    menv_waitsixtieths = 6,
    menv_date = 7,
    menv_get = 8,
    menv_set = 9,
    menv_abbrevstring = 10,
    menv_dayofweek = 11,
    menv_daysinmonth = 12,
    menv_daystring = 13
};

static boolean menu_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case menv_now:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_set:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_sleepfor:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_ticks:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_milliseconds:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_waitseconds:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_waitsixtieths:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_date:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_get:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_set:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_abbrevstring:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_dayofweek:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_daysinmonth:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_daystring:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean menuinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pmenu"), bsname);

    if (!newfunctionprocessor(bsname, &menu_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pnow"), menv_now);
    ADD_VERB(BIGSTRING("\pset"), menv_set);
    ADD_VERB(BIGSTRING("\psleepfor"), menv_sleepfor);
    ADD_VERB(BIGSTRING("\pticks"), menv_ticks);
    ADD_VERB(BIGSTRING("\pmilliseconds"), menv_milliseconds);
    ADD_VERB(BIGSTRING("\pwaitseconds"), menv_waitseconds);
    ADD_VERB(BIGSTRING("\pwaitsixtieths"), menv_waitsixtieths);
    ADD_VERB(BIGSTRING("\pdate"), menv_date);
    ADD_VERB(BIGSTRING("\pget"), menv_get);
    ADD_VERB(BIGSTRING("\pset"), menv_set);
    ADD_VERB(BIGSTRING("\pabbrevstring"), menv_abbrevstring);
    ADD_VERB(BIGSTRING("\pdayofweek"), menv_dayofweek);
    ADD_VERB(BIGSTRING("\pdaysinmonth"), menv_daysinmonth);
    ADD_VERB(BIGSTRING("\pdaystring"), menv_daystring);

    #undef ADD_VERB

    pophashtable();
    return true;
}
