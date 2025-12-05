#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the script processor */
enum {
    scrv_script_verb0 = 0,
    scrv_script_verb1 = 1,
    scrv_script_verb2 = 2,
    scrv_script_verb3 = 3,
    scrv_script_verb4 = 4,
    scrv_script_verb5 = 5,
    scrv_script_verb6 = 6,
    scrv_script_verb7 = 7,
    scrv_script_verb8 = 8,
    scrv_script_verb9 = 9,
    scrv_script_verb10 = 10,
    scrv_script_verb11 = 11,
    scrv_script_verb12 = 12
};

static boolean script_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case scrv_script_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_script_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_script_verb2:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_script_verb3:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_script_verb4:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_script_verb5:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_script_verb6:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_script_verb7:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_script_verb8:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_script_verb9:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_script_verb10:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_script_verb11:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_script_verb12:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean scriptinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pscript"), bsname);

    if (!newfunctionprocessor(bsname, &script_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pscript_verb0"), scrv_script_verb0);
    ADD_VERB(BIGSTRING("\pscript_verb1"), scrv_script_verb1);
    ADD_VERB(BIGSTRING("\pscript_verb2"), scrv_script_verb2);
    ADD_VERB(BIGSTRING("\pscript_verb3"), scrv_script_verb3);
    ADD_VERB(BIGSTRING("\pscript_verb4"), scrv_script_verb4);
    ADD_VERB(BIGSTRING("\pscript_verb5"), scrv_script_verb5);
    ADD_VERB(BIGSTRING("\pscript_verb6"), scrv_script_verb6);
    ADD_VERB(BIGSTRING("\pscript_verb7"), scrv_script_verb7);
    ADD_VERB(BIGSTRING("\pscript_verb8"), scrv_script_verb8);
    ADD_VERB(BIGSTRING("\pscript_verb9"), scrv_script_verb9);
    ADD_VERB(BIGSTRING("\pscript_verb10"), scrv_script_verb10);
    ADD_VERB(BIGSTRING("\pscript_verb11"), scrv_script_verb11);
    ADD_VERB(BIGSTRING("\pscript_verb12"), scrv_script_verb12);

    #undef ADD_VERB

    pophashtable();
    return true;
}
