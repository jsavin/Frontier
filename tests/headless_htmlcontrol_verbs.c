#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the htmlcontrol processor */
enum {
    htmv_htmlcontrol_verb0 = 0,
    htmv_htmlcontrol_verb1 = 1,
    htmv_htmlcontrol_verb2 = 2,
    htmv_htmlcontrol_verb3 = 3,
    htmv_htmlcontrol_verb4 = 4,
    htmv_htmlcontrol_verb5 = 5,
    htmv_htmlcontrol_verb6 = 6,
    htmv_htmlcontrol_verb7 = 7
};

static boolean htmlcontrol_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case htmv_htmlcontrol_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_htmlcontrol_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_htmlcontrol_verb2:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_htmlcontrol_verb3:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_htmlcontrol_verb4:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_htmlcontrol_verb5:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_htmlcontrol_verb6:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case htmv_htmlcontrol_verb7:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean htmlcontrolinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\phtmlcontrol"), bsname);

    if (!newfunctionprocessor(bsname, &htmlcontrol_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\phtmlcontrol_verb0"), htmv_htmlcontrol_verb0);
    ADD_VERB(BIGSTRING("\phtmlcontrol_verb1"), htmv_htmlcontrol_verb1);
    ADD_VERB(BIGSTRING("\phtmlcontrol_verb2"), htmv_htmlcontrol_verb2);
    ADD_VERB(BIGSTRING("\phtmlcontrol_verb3"), htmv_htmlcontrol_verb3);
    ADD_VERB(BIGSTRING("\phtmlcontrol_verb4"), htmv_htmlcontrol_verb4);
    ADD_VERB(BIGSTRING("\phtmlcontrol_verb5"), htmv_htmlcontrol_verb5);
    ADD_VERB(BIGSTRING("\phtmlcontrol_verb6"), htmv_htmlcontrol_verb6);
    ADD_VERB(BIGSTRING("\phtmlcontrol_verb7"), htmv_htmlcontrol_verb7);

    #undef ADD_VERB

    pophashtable();
    return true;
}
