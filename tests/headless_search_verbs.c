#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the search processor */
enum {
    seav_stripmarkup = 0,
    seav_deindexpage = 1,
    seav_indexpage = 2,
    seav_cleanindex = 3,
    seav_mergeresults = 4,
    seav_mrcalendar = 5
};

static boolean search_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case seav_stripmarkup:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case seav_deindexpage:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case seav_indexpage:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case seav_cleanindex:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case seav_mergeresults:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case seav_mrcalendar:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean searchinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\psearch"), bsname);

    if (!newfunctionprocessor(bsname, &search_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pstripmarkup"), seav_stripmarkup);
    ADD_VERB(BIGSTRING("\pdeindexpage"), seav_deindexpage);
    ADD_VERB(BIGSTRING("\pindexpage"), seav_indexpage);
    ADD_VERB(BIGSTRING("\pcleanindex"), seav_cleanindex);
    ADD_VERB(BIGSTRING("\pmergeresults"), seav_mergeresults);
    ADD_VERB(BIGSTRING("\pmrcalendar"), seav_mrcalendar);

    #undef ADD_VERB

    pophashtable();
    return true;
}
