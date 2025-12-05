#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the filemenu processor */
enum {
    filv_filemenu_verb0 = 0,
    filv_filemenu_verb1 = 1,
    filv_filemenu_verb2 = 2,
    filv_filemenu_verb3 = 3,
    filv_filemenu_verb4 = 4,
    filv_filemenu_verb5 = 5,
    filv_filemenu_verb6 = 6,
    filv_filemenu_verb7 = 7,
    filv_filemenu_verb8 = 8,
    filv_filemenu_verb9 = 9
};

static boolean filemenu_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case filv_filemenu_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_filemenu_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_filemenu_verb2:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_filemenu_verb3:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_filemenu_verb4:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_filemenu_verb5:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_filemenu_verb6:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_filemenu_verb7:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_filemenu_verb8:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case filv_filemenu_verb9:
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

    ADD_VERB(BIGSTRING("\pfilemenu_verb0"), filv_filemenu_verb0);
    ADD_VERB(BIGSTRING("\pfilemenu_verb1"), filv_filemenu_verb1);
    ADD_VERB(BIGSTRING("\pfilemenu_verb2"), filv_filemenu_verb2);
    ADD_VERB(BIGSTRING("\pfilemenu_verb3"), filv_filemenu_verb3);
    ADD_VERB(BIGSTRING("\pfilemenu_verb4"), filv_filemenu_verb4);
    ADD_VERB(BIGSTRING("\pfilemenu_verb5"), filv_filemenu_verb5);
    ADD_VERB(BIGSTRING("\pfilemenu_verb6"), filv_filemenu_verb6);
    ADD_VERB(BIGSTRING("\pfilemenu_verb7"), filv_filemenu_verb7);
    ADD_VERB(BIGSTRING("\pfilemenu_verb8"), filv_filemenu_verb8);
    ADD_VERB(BIGSTRING("\pfilemenu_verb9"), filv_filemenu_verb9);

    #undef ADD_VERB

    pophashtable();
    return true;
}
