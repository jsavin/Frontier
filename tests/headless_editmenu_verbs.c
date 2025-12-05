#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the editmenu processor */
enum {
    ediv_stripmarkup = 0,
    ediv_deindexpage = 1,
    ediv_indexpage = 2,
    ediv_cleanindex = 3,
    ediv_mergeresults = 4,
    ediv_mrcalendar = 5,
    ediv_getaddressday = 6,
    ediv_getdayaddress = 7,
    ediv_getfirstaddress = 8,
    ediv_getfirstday = 9,
    ediv_getlastaddress = 10,
    ediv_getlastday = 11,
    ediv_getmostrecentaddress = 12,
    ediv_getmostrecentday = 13,
    ediv_getnextaddress = 14,
    ediv_getnextday = 15
};

static boolean editmenu_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case ediv_stripmarkup:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_deindexpage:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_indexpage:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_cleanindex:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_mergeresults:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_mrcalendar:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_getaddressday:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_getdayaddress:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_getfirstaddress:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_getfirstday:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_getlastaddress:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_getlastday:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_getmostrecentaddress:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_getmostrecentday:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_getnextaddress:
            /* Verb #14 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case ediv_getnextday:
            /* Verb #15 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean editmenuinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\peditmenu"), bsname);

    if (!newfunctionprocessor(bsname, &editmenu_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pstripmarkup"), ediv_stripmarkup);
    ADD_VERB(BIGSTRING("\pdeindexpage"), ediv_deindexpage);
    ADD_VERB(BIGSTRING("\pindexpage"), ediv_indexpage);
    ADD_VERB(BIGSTRING("\pcleanindex"), ediv_cleanindex);
    ADD_VERB(BIGSTRING("\pmergeresults"), ediv_mergeresults);
    ADD_VERB(BIGSTRING("\pmrcalendar"), ediv_mrcalendar);
    ADD_VERB(BIGSTRING("\pgetaddressday"), ediv_getaddressday);
    ADD_VERB(BIGSTRING("\pgetdayaddress"), ediv_getdayaddress);
    ADD_VERB(BIGSTRING("\pgetfirstaddress"), ediv_getfirstaddress);
    ADD_VERB(BIGSTRING("\pgetfirstday"), ediv_getfirstday);
    ADD_VERB(BIGSTRING("\pgetlastaddress"), ediv_getlastaddress);
    ADD_VERB(BIGSTRING("\pgetlastday"), ediv_getlastday);
    ADD_VERB(BIGSTRING("\pgetmostrecentaddress"), ediv_getmostrecentaddress);
    ADD_VERB(BIGSTRING("\pgetmostrecentday"), ediv_getmostrecentday);
    ADD_VERB(BIGSTRING("\pgetnextaddress"), ediv_getnextaddress);
    ADD_VERB(BIGSTRING("\pgetnextday"), ediv_getnextday);

    #undef ADD_VERB

    pophashtable();
    return true;
}
