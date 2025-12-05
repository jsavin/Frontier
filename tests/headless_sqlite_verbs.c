#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the sqlite processor */
enum {
    sqlv_sqlite_verb0 = 0,
    sqlv_sqlite_verb1 = 1,
    sqlv_sqlite_verb2 = 2,
    sqlv_sqlite_verb3 = 3,
    sqlv_sqlite_verb4 = 4,
    sqlv_sqlite_verb5 = 5,
    sqlv_sqlite_verb6 = 6,
    sqlv_sqlite_verb7 = 7,
    sqlv_sqlite_verb8 = 8,
    sqlv_sqlite_verb9 = 9,
    sqlv_sqlite_verb10 = 10,
    sqlv_sqlite_verb11 = 11,
    sqlv_sqlite_verb12 = 12,
    sqlv_sqlite_verb13 = 13,
    sqlv_sqlite_verb14 = 14,
    sqlv_sqlite_verb15 = 15,
    sqlv_sqlite_verb16 = 16
};

static boolean sqlite_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case sqlv_sqlite_verb0:
            /* TODO: Implement sqlite.sqlite_verb0 */
            return false;
        case sqlv_sqlite_verb1:
            /* TODO: Implement sqlite.sqlite_verb1 */
            return false;
        case sqlv_sqlite_verb2:
            /* TODO: Implement sqlite.sqlite_verb2 */
            return false;
        case sqlv_sqlite_verb3:
            /* TODO: Implement sqlite.sqlite_verb3 */
            return false;
        case sqlv_sqlite_verb4:
            /* TODO: Implement sqlite.sqlite_verb4 */
            return false;
        case sqlv_sqlite_verb5:
            /* TODO: Implement sqlite.sqlite_verb5 */
            return false;
        case sqlv_sqlite_verb6:
            /* TODO: Implement sqlite.sqlite_verb6 */
            return false;
        case sqlv_sqlite_verb7:
            /* TODO: Implement sqlite.sqlite_verb7 */
            return false;
        case sqlv_sqlite_verb8:
            /* TODO: Implement sqlite.sqlite_verb8 */
            return false;
        case sqlv_sqlite_verb9:
            /* TODO: Implement sqlite.sqlite_verb9 */
            return false;
        case sqlv_sqlite_verb10:
            /* TODO: Implement sqlite.sqlite_verb10 */
            return false;
        case sqlv_sqlite_verb11:
            /* TODO: Implement sqlite.sqlite_verb11 */
            return false;
        case sqlv_sqlite_verb12:
            /* TODO: Implement sqlite.sqlite_verb12 */
            return false;
        case sqlv_sqlite_verb13:
            /* TODO: Implement sqlite.sqlite_verb13 */
            return false;
        case sqlv_sqlite_verb14:
            /* TODO: Implement sqlite.sqlite_verb14 */
            return false;
        case sqlv_sqlite_verb15:
            /* TODO: Implement sqlite.sqlite_verb15 */
            return false;
        case sqlv_sqlite_verb16:
            /* TODO: Implement sqlite.sqlite_verb16 */
            return false;
        default:
            return false;
    }
}

boolean sqliteinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\psqlite"), bsname);

    if (!newfunctionprocessor(bsname, &sqlite_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\psqlite_verb0"), sqlv_sqlite_verb0);
    ADD_VERB(BIGSTRING("\psqlite_verb1"), sqlv_sqlite_verb1);
    ADD_VERB(BIGSTRING("\psqlite_verb2"), sqlv_sqlite_verb2);
    ADD_VERB(BIGSTRING("\psqlite_verb3"), sqlv_sqlite_verb3);
    ADD_VERB(BIGSTRING("\psqlite_verb4"), sqlv_sqlite_verb4);
    ADD_VERB(BIGSTRING("\psqlite_verb5"), sqlv_sqlite_verb5);
    ADD_VERB(BIGSTRING("\psqlite_verb6"), sqlv_sqlite_verb6);
    ADD_VERB(BIGSTRING("\psqlite_verb7"), sqlv_sqlite_verb7);
    ADD_VERB(BIGSTRING("\psqlite_verb8"), sqlv_sqlite_verb8);
    ADD_VERB(BIGSTRING("\psqlite_verb9"), sqlv_sqlite_verb9);
    ADD_VERB(BIGSTRING("\psqlite_verb10"), sqlv_sqlite_verb10);
    ADD_VERB(BIGSTRING("\psqlite_verb11"), sqlv_sqlite_verb11);
    ADD_VERB(BIGSTRING("\psqlite_verb12"), sqlv_sqlite_verb12);
    ADD_VERB(BIGSTRING("\psqlite_verb13"), sqlv_sqlite_verb13);
    ADD_VERB(BIGSTRING("\psqlite_verb14"), sqlv_sqlite_verb14);
    ADD_VERB(BIGSTRING("\psqlite_verb15"), sqlv_sqlite_verb15);
    ADD_VERB(BIGSTRING("\psqlite_verb16"), sqlv_sqlite_verb16);

    #undef ADD_VERB

    pophashtable();
    return true;
}
