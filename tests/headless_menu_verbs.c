#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the menu processor */
enum {
    menv_menu_verb0 = 0,
    menv_menu_verb1 = 1,
    menv_menu_verb2 = 2,
    menv_menu_verb3 = 3,
    menv_menu_verb4 = 4,
    menv_menu_verb5 = 5,
    menv_menu_verb6 = 6,
    menv_menu_verb7 = 7,
    menv_menu_verb8 = 8,
    menv_menu_verb9 = 9,
    menv_menu_verb10 = 10,
    menv_menu_verb11 = 11,
    menv_menu_verb12 = 12,
    menv_menu_verb13 = 13
};

static boolean menu_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case menv_menu_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_menu_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_menu_verb2:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_menu_verb3:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_menu_verb4:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_menu_verb5:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_menu_verb6:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_menu_verb7:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_menu_verb8:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_menu_verb9:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_menu_verb10:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_menu_verb11:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_menu_verb12:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case menv_menu_verb13:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean menuinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pmenu"), bsname);

    if (!newfunctionprocessor(bsname, &menu_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pmenu_verb0"), menv_menu_verb0);
    ADD_VERB(BIGSTRING("\pmenu_verb1"), menv_menu_verb1);
    ADD_VERB(BIGSTRING("\pmenu_verb2"), menv_menu_verb2);
    ADD_VERB(BIGSTRING("\pmenu_verb3"), menv_menu_verb3);
    ADD_VERB(BIGSTRING("\pmenu_verb4"), menv_menu_verb4);
    ADD_VERB(BIGSTRING("\pmenu_verb5"), menv_menu_verb5);
    ADD_VERB(BIGSTRING("\pmenu_verb6"), menv_menu_verb6);
    ADD_VERB(BIGSTRING("\pmenu_verb7"), menv_menu_verb7);
    ADD_VERB(BIGSTRING("\pmenu_verb8"), menv_menu_verb8);
    ADD_VERB(BIGSTRING("\pmenu_verb9"), menv_menu_verb9);
    ADD_VERB(BIGSTRING("\pmenu_verb10"), menv_menu_verb10);
    ADD_VERB(BIGSTRING("\pmenu_verb11"), menv_menu_verb11);
    ADD_VERB(BIGSTRING("\pmenu_verb12"), menv_menu_verb12);
    ADD_VERB(BIGSTRING("\pmenu_verb13"), menv_menu_verb13);

    #undef ADD_VERB

    pophashtable();
    return true;
}
