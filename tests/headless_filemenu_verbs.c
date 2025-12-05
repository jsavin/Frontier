#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the filemenu processor */
enum {
    filv_stripmarkup = 0,
    filv_deindexpage = 1,
    filv_indexpage = 2,
    filv_cleanindex = 3,
    filv_mergeresults = 4,
    filv_mrcalendar = 5,
    filv_getaddressday = 6,
    filv_getdayaddress = 7,
    filv_getfirstaddress = 8,
    filv_getfirstday = 9
};

static boolean filemenu_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case filv_stripmarkup:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_deindexpage:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_indexpage:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_cleanindex:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_mergeresults:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_mrcalendar:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_getaddressday:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_getdayaddress:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_getfirstaddress:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_getfirstday:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean filemenuinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pfilemenu"), bsname);

    if (!newfunctionprocessor(bsname, &filemenu_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pstripmarkup"), filv_stripmarkup);
    ADD_VERB(BIGSTRING("\pdeindexpage"), filv_deindexpage);
    ADD_VERB(BIGSTRING("\pindexpage"), filv_indexpage);
    ADD_VERB(BIGSTRING("\pcleanindex"), filv_cleanindex);
    ADD_VERB(BIGSTRING("\pmergeresults"), filv_mergeresults);
    ADD_VERB(BIGSTRING("\pmrcalendar"), filv_mrcalendar);
    ADD_VERB(BIGSTRING("\pgetaddressday"), filv_getaddressday);
    ADD_VERB(BIGSTRING("\pgetdayaddress"), filv_getdayaddress);
    ADD_VERB(BIGSTRING("\pgetfirstaddress"), filv_getfirstaddress);
    ADD_VERB(BIGSTRING("\pgetfirstday"), filv_getfirstday);

    #undef ADD_VERB

    pophashtable();
    return true;
}
