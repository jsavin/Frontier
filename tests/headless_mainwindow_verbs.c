#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the mainwindow processor */
enum {
    maiv_stripmarkup = 0,
    maiv_deindexpage = 1,
    maiv_indexpage = 2,
    maiv_cleanindex = 3,
    maiv_mergeresults = 4,
    maiv_mrcalendar = 5,
    maiv_getaddressday = 6
};

static boolean mainwindow_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case maiv_stripmarkup:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case maiv_deindexpage:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case maiv_indexpage:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case maiv_cleanindex:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case maiv_mergeresults:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case maiv_mrcalendar:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case maiv_getaddressday:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean mainwindowinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pmainwindow"), bsname);

    if (!newfunctionprocessor(bsname, &mainwindow_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pstripmarkup"), maiv_stripmarkup);
    ADD_VERB(BIGSTRING("\pdeindexpage"), maiv_deindexpage);
    ADD_VERB(BIGSTRING("\pindexpage"), maiv_indexpage);
    ADD_VERB(BIGSTRING("\pcleanindex"), maiv_cleanindex);
    ADD_VERB(BIGSTRING("\pmergeresults"), maiv_mergeresults);
    ADD_VERB(BIGSTRING("\pmrcalendar"), maiv_mrcalendar);
    ADD_VERB(BIGSTRING("\pgetaddressday"), maiv_getaddressday);

    #undef ADD_VERB

    pophashtable();
    return true;
}
