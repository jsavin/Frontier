#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the frontier processor */
enum {
    frov_stripmarkup = 0,
    frov_deindexpage = 1,
    frov_indexpage = 2,
    frov_cleanindex = 3,
    frov_mergeresults = 4,
    frov_mrcalendar = 5,
    frov_getaddressday = 6,
    frov_getdayaddress = 7,
    frov_getfirstaddress = 8,
    frov_getfirstday = 9,
    frov_getlastaddress = 10,
    frov_getlastday = 11,
    frov_getmostrecentaddress = 12,
    frov_getmostrecentday = 13
};

static boolean frontier_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case frov_stripmarkup:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_deindexpage:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_indexpage:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_cleanindex:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_mergeresults:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_mrcalendar:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_getaddressday:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_getdayaddress:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_getfirstaddress:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_getfirstday:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_getlastaddress:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_getlastday:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_getmostrecentaddress:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case frov_getmostrecentday:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean frontierinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pfrontier"), bsname);

    if (!newfunctionprocessor(bsname, &frontier_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pstripmarkup"), frov_stripmarkup);
    ADD_VERB(BIGSTRING("\pdeindexpage"), frov_deindexpage);
    ADD_VERB(BIGSTRING("\pindexpage"), frov_indexpage);
    ADD_VERB(BIGSTRING("\pcleanindex"), frov_cleanindex);
    ADD_VERB(BIGSTRING("\pmergeresults"), frov_mergeresults);
    ADD_VERB(BIGSTRING("\pmrcalendar"), frov_mrcalendar);
    ADD_VERB(BIGSTRING("\pgetaddressday"), frov_getaddressday);
    ADD_VERB(BIGSTRING("\pgetdayaddress"), frov_getdayaddress);
    ADD_VERB(BIGSTRING("\pgetfirstaddress"), frov_getfirstaddress);
    ADD_VERB(BIGSTRING("\pgetfirstday"), frov_getfirstday);
    ADD_VERB(BIGSTRING("\pgetlastaddress"), frov_getlastaddress);
    ADD_VERB(BIGSTRING("\pgetlastday"), frov_getlastday);
    ADD_VERB(BIGSTRING("\pgetmostrecentaddress"), frov_getmostrecentaddress);
    ADD_VERB(BIGSTRING("\pgetmostrecentday"), frov_getmostrecentday);

    #undef ADD_VERB

    pophashtable();
    return true;
}
