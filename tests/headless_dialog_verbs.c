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
            /* TODO: Implement dialog.dialog_verb0 */
            return false;
        case diav_dialog_verb1:
            /* TODO: Implement dialog.dialog_verb1 */
            return false;
        case diav_dialog_verb2:
            /* TODO: Implement dialog.dialog_verb2 */
            return false;
        case diav_dialog_verb3:
            /* TODO: Implement dialog.dialog_verb3 */
            return false;
        case diav_dialog_verb4:
            /* TODO: Implement dialog.dialog_verb4 */
            return false;
        case diav_dialog_verb5:
            /* TODO: Implement dialog.dialog_verb5 */
            return false;
        case diav_dialog_verb6:
            /* TODO: Implement dialog.dialog_verb6 */
            return false;
        case diav_dialog_verb7:
            /* TODO: Implement dialog.dialog_verb7 */
            return false;
        case diav_dialog_verb8:
            /* TODO: Implement dialog.dialog_verb8 */
            return false;
        case diav_dialog_verb9:
            /* TODO: Implement dialog.dialog_verb9 */
            return false;
        case diav_dialog_verb10:
            /* TODO: Implement dialog.dialog_verb10 */
            return false;
        case diav_dialog_verb11:
            /* TODO: Implement dialog.dialog_verb11 */
            return false;
        case diav_dialog_verb12:
            /* TODO: Implement dialog.dialog_verb12 */
            return false;
        case diav_dialog_verb13:
            /* TODO: Implement dialog.dialog_verb13 */
            return false;
        case diav_dialog_verb14:
            /* TODO: Implement dialog.dialog_verb14 */
            return false;
        case diav_dialog_verb15:
            /* TODO: Implement dialog.dialog_verb15 */
            return false;
        case diav_dialog_verb16:
            /* TODO: Implement dialog.dialog_verb16 */
            return false;
        case diav_dialog_verb17:
            /* TODO: Implement dialog.dialog_verb17 */
            return false;
        case diav_dialog_verb18:
            /* TODO: Implement dialog.dialog_verb18 */
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
