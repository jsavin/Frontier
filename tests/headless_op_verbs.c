#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the op processor */
enum {
    opv_addgroup = 0,
    opv_getall = 1,
    opv_getone = 2,
    opv_makeempty = 3,
    opv_setone = 4,
    opv_script = 5,
    opv_compile = 6,
    opv_uncompile = 7,
    opv_getcode = 8,
    opv_getlanguage = 9,
    opv_setlanguage = 10,
    opv_makecomment = 11,
    opv_uncomment = 12,
    opv_iscomment = 13,
    opv_getbreakpoint = 14,
    opv_setbreakpoint = 15,
    opv_clearbreakpoint = 16,
    opv_startprofile = 17,
    opv_stopprofile = 18,
    opv_osa = 19,
    opv_compile = 20,
    opv_getsource = 21,
    opv_table = 22,
    opv_move = 23,
    opv_copy = 24,
    opv_rename = 25,
    opv_moveandrename = 26,
    opv_assign = 27,
    opv_validate = 28,
    opv_sortby = 29,
    opv_getcursor = 30,
    opv_getselection = 31,
    opv_go = 32,
    opv_goto = 33,
    opv_gotoname = 34,
    opv_jettison = 35,
    opv_packtable = 36,
    opv_emptytable = 37,
    opv_getdisplaysettings = 38,
    opv_setdisplaysettings = 39,
    opv_getsortorder = 40,
    opv_menu = 41,
    opv_zoomscript = 42,
    opv_buildmenubar = 43,
    opv_clearmenubar = 44
};

static boolean op_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case opv_addgroup:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_getall:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_getone:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_makeempty:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_setone:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_script:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_compile:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_uncompile:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_getcode:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_getlanguage:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_setlanguage:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_makecomment:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_uncomment:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_iscomment:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_getbreakpoint:
            /* Verb #14 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_setbreakpoint:
            /* Verb #15 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_clearbreakpoint:
            /* Verb #16 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_startprofile:
            /* Verb #17 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_stopprofile:
            /* Verb #18 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_osa:
            /* Verb #19 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_compile:
            /* Verb #20 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_getsource:
            /* Verb #21 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_table:
            /* Verb #22 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_move:
            /* Verb #23 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_copy:
            /* Verb #24 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_rename:
            /* Verb #25 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_moveandrename:
            /* Verb #26 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_assign:
            /* Verb #27 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_validate:
            /* Verb #28 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_sortby:
            /* Verb #29 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_getcursor:
            /* Verb #30 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_getselection:
            /* Verb #31 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_go:
            /* Verb #32 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_goto:
            /* Verb #33 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_gotoname:
            /* Verb #34 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_jettison:
            /* Verb #35 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_packtable:
            /* Verb #36 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_emptytable:
            /* Verb #37 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_getdisplaysettings:
            /* Verb #38 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_setdisplaysettings:
            /* Verb #39 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_getsortorder:
            /* Verb #40 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_menu:
            /* Verb #41 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_zoomscript:
            /* Verb #42 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_buildmenubar:
            /* Verb #43 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case opv_clearmenubar:
            /* Verb #44 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean opinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pop"), bsname);

    if (!newfunctionprocessor(bsname, &op_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\paddgroup"), opv_addgroup);
    ADD_VERB(BIGSTRING("\pgetall"), opv_getall);
    ADD_VERB(BIGSTRING("\pgetone"), opv_getone);
    ADD_VERB(BIGSTRING("\pmakeempty"), opv_makeempty);
    ADD_VERB(BIGSTRING("\psetone"), opv_setone);
    ADD_VERB(BIGSTRING("\pscript"), opv_script);
    ADD_VERB(BIGSTRING("\pcompile"), opv_compile);
    ADD_VERB(BIGSTRING("\puncompile"), opv_uncompile);
    ADD_VERB(BIGSTRING("\pgetcode"), opv_getcode);
    ADD_VERB(BIGSTRING("\pgetlanguage"), opv_getlanguage);
    ADD_VERB(BIGSTRING("\psetlanguage"), opv_setlanguage);
    ADD_VERB(BIGSTRING("\pmakecomment"), opv_makecomment);
    ADD_VERB(BIGSTRING("\puncomment"), opv_uncomment);
    ADD_VERB(BIGSTRING("\piscomment"), opv_iscomment);
    ADD_VERB(BIGSTRING("\pgetbreakpoint"), opv_getbreakpoint);
    ADD_VERB(BIGSTRING("\psetbreakpoint"), opv_setbreakpoint);
    ADD_VERB(BIGSTRING("\pclearbreakpoint"), opv_clearbreakpoint);
    ADD_VERB(BIGSTRING("\pstartprofile"), opv_startprofile);
    ADD_VERB(BIGSTRING("\pstopprofile"), opv_stopprofile);
    ADD_VERB(BIGSTRING("\posa"), opv_osa);
    ADD_VERB(BIGSTRING("\pcompile"), opv_compile);
    ADD_VERB(BIGSTRING("\pgetsource"), opv_getsource);
    ADD_VERB(BIGSTRING("\ptable"), opv_table);
    ADD_VERB(BIGSTRING("\pmove"), opv_move);
    ADD_VERB(BIGSTRING("\pcopy"), opv_copy);
    ADD_VERB(BIGSTRING("\prename"), opv_rename);
    ADD_VERB(BIGSTRING("\pmoveandrename"), opv_moveandrename);
    ADD_VERB(BIGSTRING("\passign"), opv_assign);
    ADD_VERB(BIGSTRING("\pvalidate"), opv_validate);
    ADD_VERB(BIGSTRING("\psortby"), opv_sortby);
    ADD_VERB(BIGSTRING("\pgetcursor"), opv_getcursor);
    ADD_VERB(BIGSTRING("\pgetselection"), opv_getselection);
    ADD_VERB(BIGSTRING("\pgo"), opv_go);
    ADD_VERB(BIGSTRING("\pgoto"), opv_goto);
    ADD_VERB(BIGSTRING("\pgotoname"), opv_gotoname);
    ADD_VERB(BIGSTRING("\pjettison"), opv_jettison);
    ADD_VERB(BIGSTRING("\ppacktable"), opv_packtable);
    ADD_VERB(BIGSTRING("\pemptytable"), opv_emptytable);
    ADD_VERB(BIGSTRING("\pgetdisplaysettings"), opv_getdisplaysettings);
    ADD_VERB(BIGSTRING("\psetdisplaysettings"), opv_setdisplaysettings);
    ADD_VERB(BIGSTRING("\pgetsortorder"), opv_getsortorder);
    ADD_VERB(BIGSTRING("\pmenu"), opv_menu);
    ADD_VERB(BIGSTRING("\pzoomscript"), opv_zoomscript);
    ADD_VERB(BIGSTRING("\pbuildmenubar"), opv_buildmenubar);
    ADD_VERB(BIGSTRING("\pclearmenubar"), opv_clearmenubar);

    #undef ADD_VERB

    pophashtable();
    return true;
}
