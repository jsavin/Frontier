#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the dialog processor */
enum {
    diav_alert = 0,
    diav_run = 1,
    diav_runmodeless = 2,
    diav_runcard = 3,
    diav_runmodalcard = 4,
    diav_ismodalcard = 5,
    diav_setmodalcardtimeout = 6,
    diav_getvalue = 7,
    diav_setvalue = 8,
    diav_setitemenable = 9,
    diav_showitem = 10,
    diav_hideitem = 11,
    diav_twoway = 12,
    diav_threeway = 13,
    diav_ask = 14,
    diav_getint = 15,
    diav_notify = 16,
    diav_getuserinfo = 17,
    diav_getpassword = 18
};

static boolean dialog_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case diav_alert:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_run:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_runmodeless:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_runcard:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_runmodalcard:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_ismodalcard:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_setmodalcardtimeout:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_getvalue:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_setvalue:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_setitemenable:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_showitem:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_hideitem:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_twoway:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_threeway:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_ask:
            /* Verb #14 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_getint:
            /* Verb #15 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_notify:
            /* Verb #16 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_getuserinfo:
            /* Verb #17 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_getpassword:
            /* Verb #18 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean dialoginitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pdialog"), bsname);

    if (!newfunctionprocessor(bsname, &dialog_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\palert"), diav_alert);
    ADD_VERB(BIGSTRING("\prun"), diav_run);
    ADD_VERB(BIGSTRING("\prunmodeless"), diav_runmodeless);
    ADD_VERB(BIGSTRING("\pruncard"), diav_runcard);
    ADD_VERB(BIGSTRING("\prunmodalcard"), diav_runmodalcard);
    ADD_VERB(BIGSTRING("\pismodalcard"), diav_ismodalcard);
    ADD_VERB(BIGSTRING("\psetmodalcardtimeout"), diav_setmodalcardtimeout);
    ADD_VERB(BIGSTRING("\pgetvalue"), diav_getvalue);
    ADD_VERB(BIGSTRING("\psetvalue"), diav_setvalue);
    ADD_VERB(BIGSTRING("\psetitemenable"), diav_setitemenable);
    ADD_VERB(BIGSTRING("\pshowitem"), diav_showitem);
    ADD_VERB(BIGSTRING("\phideitem"), diav_hideitem);
    ADD_VERB(BIGSTRING("\ptwoway"), diav_twoway);
    ADD_VERB(BIGSTRING("\pthreeway"), diav_threeway);
    ADD_VERB(BIGSTRING("\pask"), diav_ask);
    ADD_VERB(BIGSTRING("\pgetint"), diav_getint);
    ADD_VERB(BIGSTRING("\pnotify"), diav_notify);
    ADD_VERB(BIGSTRING("\pgetuserinfo"), diav_getuserinfo);
    ADD_VERB(BIGSTRING("\pgetpassword"), diav_getpassword);

    #undef ADD_VERB

    pophashtable();
    return true;
}
