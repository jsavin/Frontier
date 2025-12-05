#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the mainwindow processor */
enum {
    maiv_mainwindow_verb0 = 0,
    maiv_mainwindow_verb1 = 1,
    maiv_mainwindow_verb2 = 2,
    maiv_mainwindow_verb3 = 3,
    maiv_mainwindow_verb4 = 4,
    maiv_mainwindow_verb5 = 5,
    maiv_mainwindow_verb6 = 6
};

static boolean mainwindow_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case maiv_mainwindow_verb0:
            /* TODO: Implement mainwindow.mainwindow_verb0 */
            return false;
        case maiv_mainwindow_verb1:
            /* TODO: Implement mainwindow.mainwindow_verb1 */
            return false;
        case maiv_mainwindow_verb2:
            /* TODO: Implement mainwindow.mainwindow_verb2 */
            return false;
        case maiv_mainwindow_verb3:
            /* TODO: Implement mainwindow.mainwindow_verb3 */
            return false;
        case maiv_mainwindow_verb4:
            /* TODO: Implement mainwindow.mainwindow_verb4 */
            return false;
        case maiv_mainwindow_verb5:
            /* TODO: Implement mainwindow.mainwindow_verb5 */
            return false;
        case maiv_mainwindow_verb6:
            /* TODO: Implement mainwindow.mainwindow_verb6 */
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

    ADD_VERB(BIGSTRING("\pmainwindow_verb0"), maiv_mainwindow_verb0);
    ADD_VERB(BIGSTRING("\pmainwindow_verb1"), maiv_mainwindow_verb1);
    ADD_VERB(BIGSTRING("\pmainwindow_verb2"), maiv_mainwindow_verb2);
    ADD_VERB(BIGSTRING("\pmainwindow_verb3"), maiv_mainwindow_verb3);
    ADD_VERB(BIGSTRING("\pmainwindow_verb4"), maiv_mainwindow_verb4);
    ADD_VERB(BIGSTRING("\pmainwindow_verb5"), maiv_mainwindow_verb5);
    ADD_VERB(BIGSTRING("\pmainwindow_verb6"), maiv_mainwindow_verb6);

    #undef ADD_VERB

    pophashtable();
    return true;
}
