#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the db processor */
enum {
    dbv_db_verb0 = 0,
    dbv_db_verb1 = 1,
    dbv_db_verb2 = 2,
    dbv_db_verb3 = 3,
    dbv_db_verb4 = 4,
    dbv_db_verb5 = 5,
    dbv_db_verb6 = 6,
    dbv_db_verb7 = 7,
    dbv_db_verb8 = 8,
    dbv_db_verb9 = 9,
    dbv_db_verb10 = 10,
    dbv_db_verb11 = 11,
    dbv_db_verb12 = 12
};

static boolean db_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case dbv_db_verb0:
            /* TODO: Implement db.db_verb0 */
            return false;
        case dbv_db_verb1:
            /* TODO: Implement db.db_verb1 */
            return false;
        case dbv_db_verb2:
            /* TODO: Implement db.db_verb2 */
            return false;
        case dbv_db_verb3:
            /* TODO: Implement db.db_verb3 */
            return false;
        case dbv_db_verb4:
            /* TODO: Implement db.db_verb4 */
            return false;
        case dbv_db_verb5:
            /* TODO: Implement db.db_verb5 */
            return false;
        case dbv_db_verb6:
            /* TODO: Implement db.db_verb6 */
            return false;
        case dbv_db_verb7:
            /* TODO: Implement db.db_verb7 */
            return false;
        case dbv_db_verb8:
            /* TODO: Implement db.db_verb8 */
            return false;
        case dbv_db_verb9:
            /* TODO: Implement db.db_verb9 */
            return false;
        case dbv_db_verb10:
            /* TODO: Implement db.db_verb10 */
            return false;
        case dbv_db_verb11:
            /* TODO: Implement db.db_verb11 */
            return false;
        case dbv_db_verb12:
            /* TODO: Implement db.db_verb12 */
            return false;
        default:
            return false;
    }
}

boolean dbinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pdb"), bsname);

    if (!newfunctionprocessor(bsname, &db_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pdb_verb0"), dbv_db_verb0);
    ADD_VERB(BIGSTRING("\pdb_verb1"), dbv_db_verb1);
    ADD_VERB(BIGSTRING("\pdb_verb2"), dbv_db_verb2);
    ADD_VERB(BIGSTRING("\pdb_verb3"), dbv_db_verb3);
    ADD_VERB(BIGSTRING("\pdb_verb4"), dbv_db_verb4);
    ADD_VERB(BIGSTRING("\pdb_verb5"), dbv_db_verb5);
    ADD_VERB(BIGSTRING("\pdb_verb6"), dbv_db_verb6);
    ADD_VERB(BIGSTRING("\pdb_verb7"), dbv_db_verb7);
    ADD_VERB(BIGSTRING("\pdb_verb8"), dbv_db_verb8);
    ADD_VERB(BIGSTRING("\pdb_verb9"), dbv_db_verb9);
    ADD_VERB(BIGSTRING("\pdb_verb10"), dbv_db_verb10);
    ADD_VERB(BIGSTRING("\pdb_verb11"), dbv_db_verb11);
    ADD_VERB(BIGSTRING("\pdb_verb12"), dbv_db_verb12);

    #undef ADD_VERB

    pophashtable();
    return true;
}
