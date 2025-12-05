#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the rez processor */
enum {
    rezv_stripmarkup = 0,
    rezv_deindexpage = 1,
    rezv_indexpage = 2,
    rezv_cleanindex = 3,
    rezv_mergeresults = 4,
    rezv_mrcalendar = 5,
    rezv_getaddressday = 6,
    rezv_getdayaddress = 7,
    rezv_getfirstaddress = 8,
    rezv_getfirstday = 9,
    rezv_getlastaddress = 10,
    rezv_getlastday = 11,
    rezv_getmostrecentaddress = 12,
    rezv_getmostrecentday = 13,
    rezv_getnextaddress = 14
};

static boolean rez_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case rezv_stripmarkup:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_deindexpage:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_indexpage:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_cleanindex:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_mergeresults:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_mrcalendar:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_getaddressday:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_getdayaddress:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_getfirstaddress:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_getfirstday:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_getlastaddress:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_getlastday:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_getmostrecentaddress:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_getmostrecentday:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case rezv_getnextaddress:
            /* Verb #14 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean rezinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\prez"), bsname);

    if (!newfunctionprocessor(bsname, &rez_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pstripmarkup"), rezv_stripmarkup);
    ADD_VERB(BIGSTRING("\pdeindexpage"), rezv_deindexpage);
    ADD_VERB(BIGSTRING("\pindexpage"), rezv_indexpage);
    ADD_VERB(BIGSTRING("\pcleanindex"), rezv_cleanindex);
    ADD_VERB(BIGSTRING("\pmergeresults"), rezv_mergeresults);
    ADD_VERB(BIGSTRING("\pmrcalendar"), rezv_mrcalendar);
    ADD_VERB(BIGSTRING("\pgetaddressday"), rezv_getaddressday);
    ADD_VERB(BIGSTRING("\pgetdayaddress"), rezv_getdayaddress);
    ADD_VERB(BIGSTRING("\pgetfirstaddress"), rezv_getfirstaddress);
    ADD_VERB(BIGSTRING("\pgetfirstday"), rezv_getfirstday);
    ADD_VERB(BIGSTRING("\pgetlastaddress"), rezv_getlastaddress);
    ADD_VERB(BIGSTRING("\pgetlastday"), rezv_getlastday);
    ADD_VERB(BIGSTRING("\pgetmostrecentaddress"), rezv_getmostrecentaddress);
    ADD_VERB(BIGSTRING("\pgetmostrecentday"), rezv_getmostrecentday);
    ADD_VERB(BIGSTRING("\pgetnextaddress"), rezv_getnextaddress);

    #undef ADD_VERB

    pophashtable();
    return true;
}
