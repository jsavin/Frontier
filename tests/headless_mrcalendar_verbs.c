#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the mrcalendar processor */
enum {
    mrcv_getaddressday = 0,
    mrcv_getdayaddress = 1,
    mrcv_getfirstaddress = 2,
    mrcv_getfirstday = 3,
    mrcv_getlastaddress = 4,
    mrcv_getlastday = 5,
    mrcv_getmostrecentaddress = 6,
    mrcv_getmostrecentday = 7,
    mrcv_getnextaddress = 8,
    mrcv_getnextday = 9,
    mrcv_navigate = 10
};

static boolean mrcalendar_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case mrcv_getaddressday:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case mrcv_getdayaddress:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case mrcv_getfirstaddress:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case mrcv_getfirstday:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case mrcv_getlastaddress:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case mrcv_getlastday:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case mrcv_getmostrecentaddress:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case mrcv_getmostrecentday:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case mrcv_getnextaddress:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case mrcv_getnextday:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case mrcv_navigate:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean mrcalendarinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pmrcalendar"), bsname);

    if (!newfunctionprocessor(bsname, &mrcalendar_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pgetaddressday"), mrcv_getaddressday);
    ADD_VERB(BIGSTRING("\pgetdayaddress"), mrcv_getdayaddress);
    ADD_VERB(BIGSTRING("\pgetfirstaddress"), mrcv_getfirstaddress);
    ADD_VERB(BIGSTRING("\pgetfirstday"), mrcv_getfirstday);
    ADD_VERB(BIGSTRING("\pgetlastaddress"), mrcv_getlastaddress);
    ADD_VERB(BIGSTRING("\pgetlastday"), mrcv_getlastday);
    ADD_VERB(BIGSTRING("\pgetmostrecentaddress"), mrcv_getmostrecentaddress);
    ADD_VERB(BIGSTRING("\pgetmostrecentday"), mrcv_getmostrecentday);
    ADD_VERB(BIGSTRING("\pgetnextaddress"), mrcv_getnextaddress);
    ADD_VERB(BIGSTRING("\pgetnextday"), mrcv_getnextday);
    ADD_VERB(BIGSTRING("\pnavigate"), mrcv_navigate);

    #undef ADD_VERB

    pophashtable();
    return true;
}
