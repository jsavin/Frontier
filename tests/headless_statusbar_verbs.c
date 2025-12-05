#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the statusbar processor */
enum {
    stav_msg = 0,
    stav_setsections = 1,
    stav_getsections = 2,
    stav_getsectionone = 3,
    stav_getmessage = 4
};

static boolean statusbar_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case stav_msg:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case stav_setsections:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case stav_getsections:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case stav_getsectionone:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case stav_getmessage:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean statusbarinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pstatusbar"), bsname);

    if (!newfunctionprocessor(bsname, &statusbar_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pmsg"), stav_msg);
    ADD_VERB(BIGSTRING("\psetsections"), stav_setsections);
    ADD_VERB(BIGSTRING("\pgetsections"), stav_getsections);
    ADD_VERB(BIGSTRING("\pgetsectionone"), stav_getsectionone);
    ADD_VERB(BIGSTRING("\pgetmessage"), stav_getmessage);

    #undef ADD_VERB

    pophashtable();
    return true;
}
