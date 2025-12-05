#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the thread processor */
enum {
    thrv_stripmarkup = 0,
    thrv_deindexpage = 1,
    thrv_indexpage = 2,
    thrv_cleanindex = 3,
    thrv_mergeresults = 4,
    thrv_mrcalendar = 5,
    thrv_getaddressday = 6,
    thrv_getdayaddress = 7,
    thrv_getfirstaddress = 8,
    thrv_getfirstday = 9,
    thrv_getlastaddress = 10,
    thrv_getlastday = 11,
    thrv_getmostrecentaddress = 12,
    thrv_getmostrecentday = 13,
    thrv_getnextaddress = 14,
    thrv_getnextday = 15,
    thrv_navigate = 16
};

static boolean thread_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case thrv_stripmarkup:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_deindexpage:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_indexpage:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_cleanindex:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_mergeresults:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_mrcalendar:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_getaddressday:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_getdayaddress:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_getfirstaddress:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_getfirstday:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_getlastaddress:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_getlastday:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_getmostrecentaddress:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_getmostrecentday:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_getnextaddress:
            /* Verb #14 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_getnextday:
            /* Verb #15 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_navigate:
            /* Verb #16 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean threadinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pthread"), bsname);

    if (!newfunctionprocessor(bsname, &thread_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pstripmarkup"), thrv_stripmarkup);
    ADD_VERB(BIGSTRING("\pdeindexpage"), thrv_deindexpage);
    ADD_VERB(BIGSTRING("\pindexpage"), thrv_indexpage);
    ADD_VERB(BIGSTRING("\pcleanindex"), thrv_cleanindex);
    ADD_VERB(BIGSTRING("\pmergeresults"), thrv_mergeresults);
    ADD_VERB(BIGSTRING("\pmrcalendar"), thrv_mrcalendar);
    ADD_VERB(BIGSTRING("\pgetaddressday"), thrv_getaddressday);
    ADD_VERB(BIGSTRING("\pgetdayaddress"), thrv_getdayaddress);
    ADD_VERB(BIGSTRING("\pgetfirstaddress"), thrv_getfirstaddress);
    ADD_VERB(BIGSTRING("\pgetfirstday"), thrv_getfirstday);
    ADD_VERB(BIGSTRING("\pgetlastaddress"), thrv_getlastaddress);
    ADD_VERB(BIGSTRING("\pgetlastday"), thrv_getlastday);
    ADD_VERB(BIGSTRING("\pgetmostrecentaddress"), thrv_getmostrecentaddress);
    ADD_VERB(BIGSTRING("\pgetmostrecentday"), thrv_getmostrecentday);
    ADD_VERB(BIGSTRING("\pgetnextaddress"), thrv_getnextaddress);
    ADD_VERB(BIGSTRING("\pgetnextday"), thrv_getnextday);
    ADD_VERB(BIGSTRING("\pnavigate"), thrv_navigate);

    #undef ADD_VERB

    pophashtable();
    return true;
}
