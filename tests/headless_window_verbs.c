#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the window processor */
enum {
    winv_window_verb0 = 0,
    winv_window_verb1 = 1,
    winv_window_verb2 = 2,
    winv_window_verb3 = 3,
    winv_window_verb4 = 4,
    winv_window_verb5 = 5,
    winv_window_verb6 = 6,
    winv_window_verb7 = 7,
    winv_window_verb8 = 8,
    winv_window_verb9 = 9,
    winv_window_verb10 = 10,
    winv_window_verb11 = 11,
    winv_window_verb12 = 12,
    winv_window_verb13 = 13,
    winv_window_verb14 = 14,
    winv_window_verb15 = 15,
    winv_window_verb16 = 16,
    winv_window_verb17 = 17,
    winv_window_verb18 = 18,
    winv_window_verb19 = 19,
    winv_window_verb20 = 20,
    winv_window_verb21 = 21,
    winv_window_verb22 = 22,
    winv_window_verb23 = 23,
    winv_window_verb24 = 24,
    winv_window_verb25 = 25,
    winv_window_verb26 = 26,
    winv_window_verb27 = 27,
    winv_window_verb28 = 28,
    winv_window_verb29 = 29,
    winv_window_verb30 = 30
};

static boolean window_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case winv_window_verb0:
            /* TODO: Implement window.window_verb0 */
            return false;
        case winv_window_verb1:
            /* TODO: Implement window.window_verb1 */
            return false;
        case winv_window_verb2:
            /* TODO: Implement window.window_verb2 */
            return false;
        case winv_window_verb3:
            /* TODO: Implement window.window_verb3 */
            return false;
        case winv_window_verb4:
            /* TODO: Implement window.window_verb4 */
            return false;
        case winv_window_verb5:
            /* TODO: Implement window.window_verb5 */
            return false;
        case winv_window_verb6:
            /* TODO: Implement window.window_verb6 */
            return false;
        case winv_window_verb7:
            /* TODO: Implement window.window_verb7 */
            return false;
        case winv_window_verb8:
            /* TODO: Implement window.window_verb8 */
            return false;
        case winv_window_verb9:
            /* TODO: Implement window.window_verb9 */
            return false;
        case winv_window_verb10:
            /* TODO: Implement window.window_verb10 */
            return false;
        case winv_window_verb11:
            /* TODO: Implement window.window_verb11 */
            return false;
        case winv_window_verb12:
            /* TODO: Implement window.window_verb12 */
            return false;
        case winv_window_verb13:
            /* TODO: Implement window.window_verb13 */
            return false;
        case winv_window_verb14:
            /* TODO: Implement window.window_verb14 */
            return false;
        case winv_window_verb15:
            /* TODO: Implement window.window_verb15 */
            return false;
        case winv_window_verb16:
            /* TODO: Implement window.window_verb16 */
            return false;
        case winv_window_verb17:
            /* TODO: Implement window.window_verb17 */
            return false;
        case winv_window_verb18:
            /* TODO: Implement window.window_verb18 */
            return false;
        case winv_window_verb19:
            /* TODO: Implement window.window_verb19 */
            return false;
        case winv_window_verb20:
            /* TODO: Implement window.window_verb20 */
            return false;
        case winv_window_verb21:
            /* TODO: Implement window.window_verb21 */
            return false;
        case winv_window_verb22:
            /* TODO: Implement window.window_verb22 */
            return false;
        case winv_window_verb23:
            /* TODO: Implement window.window_verb23 */
            return false;
        case winv_window_verb24:
            /* TODO: Implement window.window_verb24 */
            return false;
        case winv_window_verb25:
            /* TODO: Implement window.window_verb25 */
            return false;
        case winv_window_verb26:
            /* TODO: Implement window.window_verb26 */
            return false;
        case winv_window_verb27:
            /* TODO: Implement window.window_verb27 */
            return false;
        case winv_window_verb28:
            /* TODO: Implement window.window_verb28 */
            return false;
        case winv_window_verb29:
            /* TODO: Implement window.window_verb29 */
            return false;
        case winv_window_verb30:
            /* TODO: Implement window.window_verb30 */
            return false;
        default:
            return false;
    }
}

boolean windowinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pwindow"), bsname);

    if (!newfunctionprocessor(bsname, &window_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pwindow_verb0"), winv_window_verb0);
    ADD_VERB(BIGSTRING("\pwindow_verb1"), winv_window_verb1);
    ADD_VERB(BIGSTRING("\pwindow_verb2"), winv_window_verb2);
    ADD_VERB(BIGSTRING("\pwindow_verb3"), winv_window_verb3);
    ADD_VERB(BIGSTRING("\pwindow_verb4"), winv_window_verb4);
    ADD_VERB(BIGSTRING("\pwindow_verb5"), winv_window_verb5);
    ADD_VERB(BIGSTRING("\pwindow_verb6"), winv_window_verb6);
    ADD_VERB(BIGSTRING("\pwindow_verb7"), winv_window_verb7);
    ADD_VERB(BIGSTRING("\pwindow_verb8"), winv_window_verb8);
    ADD_VERB(BIGSTRING("\pwindow_verb9"), winv_window_verb9);
    ADD_VERB(BIGSTRING("\pwindow_verb10"), winv_window_verb10);
    ADD_VERB(BIGSTRING("\pwindow_verb11"), winv_window_verb11);
    ADD_VERB(BIGSTRING("\pwindow_verb12"), winv_window_verb12);
    ADD_VERB(BIGSTRING("\pwindow_verb13"), winv_window_verb13);
    ADD_VERB(BIGSTRING("\pwindow_verb14"), winv_window_verb14);
    ADD_VERB(BIGSTRING("\pwindow_verb15"), winv_window_verb15);
    ADD_VERB(BIGSTRING("\pwindow_verb16"), winv_window_verb16);
    ADD_VERB(BIGSTRING("\pwindow_verb17"), winv_window_verb17);
    ADD_VERB(BIGSTRING("\pwindow_verb18"), winv_window_verb18);
    ADD_VERB(BIGSTRING("\pwindow_verb19"), winv_window_verb19);
    ADD_VERB(BIGSTRING("\pwindow_verb20"), winv_window_verb20);
    ADD_VERB(BIGSTRING("\pwindow_verb21"), winv_window_verb21);
    ADD_VERB(BIGSTRING("\pwindow_verb22"), winv_window_verb22);
    ADD_VERB(BIGSTRING("\pwindow_verb23"), winv_window_verb23);
    ADD_VERB(BIGSTRING("\pwindow_verb24"), winv_window_verb24);
    ADD_VERB(BIGSTRING("\pwindow_verb25"), winv_window_verb25);
    ADD_VERB(BIGSTRING("\pwindow_verb26"), winv_window_verb26);
    ADD_VERB(BIGSTRING("\pwindow_verb27"), winv_window_verb27);
    ADD_VERB(BIGSTRING("\pwindow_verb28"), winv_window_verb28);
    ADD_VERB(BIGSTRING("\pwindow_verb29"), winv_window_verb29);
    ADD_VERB(BIGSTRING("\pwindow_verb30"), winv_window_verb30);

    #undef ADD_VERB

    pophashtable();
    return true;
}
