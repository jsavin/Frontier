#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the dialog processor */
enum {
    diav_dialog_verb0 = 0,
    diav_dialog_verb1 = 1,
    diav_dialog_verb2 = 2,
    diav_dialog_verb3 = 3,
    diav_dialog_verb4 = 4,
    diav_dialog_verb5 = 5,
    diav_dialog_verb6 = 6,
    diav_dialog_verb7 = 7,
    diav_dialog_verb8 = 8,
    diav_dialog_verb9 = 9,
    diav_dialog_verb10 = 10,
    diav_dialog_verb11 = 11,
    diav_dialog_verb12 = 12,
    diav_dialog_verb13 = 13,
    diav_dialog_verb14 = 14,
    diav_dialog_verb15 = 15,
    diav_dialog_verb16 = 16,
    diav_dialog_verb17 = 17,
    diav_dialog_verb18 = 18
};

static boolean dialog_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case diav_dialog_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb2:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb3:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb4:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb5:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb6:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb7:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb8:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb9:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb10:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb11:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb12:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb13:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb14:
            /* Verb #14 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb15:
            /* Verb #15 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb16:
            /* Verb #16 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb17:
            /* Verb #17 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case diav_dialog_verb18:
            /* Verb #18 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean dialoginitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pdialog"), bsname);

    if (!newfunctionprocessor(bsname, &dialog_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pdialog_verb0"), diav_dialog_verb0);
    ADD_VERB(BIGSTRING("\pdialog_verb1"), diav_dialog_verb1);
    ADD_VERB(BIGSTRING("\pdialog_verb2"), diav_dialog_verb2);
    ADD_VERB(BIGSTRING("\pdialog_verb3"), diav_dialog_verb3);
    ADD_VERB(BIGSTRING("\pdialog_verb4"), diav_dialog_verb4);
    ADD_VERB(BIGSTRING("\pdialog_verb5"), diav_dialog_verb5);
    ADD_VERB(BIGSTRING("\pdialog_verb6"), diav_dialog_verb6);
    ADD_VERB(BIGSTRING("\pdialog_verb7"), diav_dialog_verb7);
    ADD_VERB(BIGSTRING("\pdialog_verb8"), diav_dialog_verb8);
    ADD_VERB(BIGSTRING("\pdialog_verb9"), diav_dialog_verb9);
    ADD_VERB(BIGSTRING("\pdialog_verb10"), diav_dialog_verb10);
    ADD_VERB(BIGSTRING("\pdialog_verb11"), diav_dialog_verb11);
    ADD_VERB(BIGSTRING("\pdialog_verb12"), diav_dialog_verb12);
    ADD_VERB(BIGSTRING("\pdialog_verb13"), diav_dialog_verb13);
    ADD_VERB(BIGSTRING("\pdialog_verb14"), diav_dialog_verb14);
    ADD_VERB(BIGSTRING("\pdialog_verb15"), diav_dialog_verb15);
    ADD_VERB(BIGSTRING("\pdialog_verb16"), diav_dialog_verb16);
    ADD_VERB(BIGSTRING("\pdialog_verb17"), diav_dialog_verb17);
    ADD_VERB(BIGSTRING("\pdialog_verb18"), diav_dialog_verb18);

    #undef ADD_VERB

    pophashtable();
    return true;
}
