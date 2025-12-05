#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the table processor */
enum {
    tabv_table_verb0 = 0,
    tabv_table_verb1 = 1,
    tabv_table_verb2 = 2,
    tabv_table_verb3 = 3,
    tabv_table_verb4 = 4,
    tabv_table_verb5 = 5,
    tabv_table_verb6 = 6,
    tabv_table_verb7 = 7,
    tabv_table_verb8 = 8,
    tabv_table_verb9 = 9,
    tabv_table_verb10 = 10,
    tabv_table_verb11 = 11,
    tabv_table_verb12 = 12,
    tabv_table_verb13 = 13,
    tabv_table_verb14 = 14,
    tabv_table_verb15 = 15,
    tabv_table_verb16 = 16,
    tabv_table_verb17 = 17
};

static boolean table_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case tabv_table_verb0:
            /* TODO: Implement table.table_verb0 */
            return false;
        case tabv_table_verb1:
            /* TODO: Implement table.table_verb1 */
            return false;
        case tabv_table_verb2:
            /* TODO: Implement table.table_verb2 */
            return false;
        case tabv_table_verb3:
            /* TODO: Implement table.table_verb3 */
            return false;
        case tabv_table_verb4:
            /* TODO: Implement table.table_verb4 */
            return false;
        case tabv_table_verb5:
            /* TODO: Implement table.table_verb5 */
            return false;
        case tabv_table_verb6:
            /* TODO: Implement table.table_verb6 */
            return false;
        case tabv_table_verb7:
            /* TODO: Implement table.table_verb7 */
            return false;
        case tabv_table_verb8:
            /* TODO: Implement table.table_verb8 */
            return false;
        case tabv_table_verb9:
            /* TODO: Implement table.table_verb9 */
            return false;
        case tabv_table_verb10:
            /* TODO: Implement table.table_verb10 */
            return false;
        case tabv_table_verb11:
            /* TODO: Implement table.table_verb11 */
            return false;
        case tabv_table_verb12:
            /* TODO: Implement table.table_verb12 */
            return false;
        case tabv_table_verb13:
            /* TODO: Implement table.table_verb13 */
            return false;
        case tabv_table_verb14:
            /* TODO: Implement table.table_verb14 */
            return false;
        case tabv_table_verb15:
            /* TODO: Implement table.table_verb15 */
            return false;
        case tabv_table_verb16:
            /* TODO: Implement table.table_verb16 */
            return false;
        case tabv_table_verb17:
            /* TODO: Implement table.table_verb17 */
            return false;
        default:
            return false;
    }
}

boolean tableinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\ptable"), bsname);

    if (!newfunctionprocessor(bsname, &table_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\ptable_verb0"), tabv_table_verb0);
    ADD_VERB(BIGSTRING("\ptable_verb1"), tabv_table_verb1);
    ADD_VERB(BIGSTRING("\ptable_verb2"), tabv_table_verb2);
    ADD_VERB(BIGSTRING("\ptable_verb3"), tabv_table_verb3);
    ADD_VERB(BIGSTRING("\ptable_verb4"), tabv_table_verb4);
    ADD_VERB(BIGSTRING("\ptable_verb5"), tabv_table_verb5);
    ADD_VERB(BIGSTRING("\ptable_verb6"), tabv_table_verb6);
    ADD_VERB(BIGSTRING("\ptable_verb7"), tabv_table_verb7);
    ADD_VERB(BIGSTRING("\ptable_verb8"), tabv_table_verb8);
    ADD_VERB(BIGSTRING("\ptable_verb9"), tabv_table_verb9);
    ADD_VERB(BIGSTRING("\ptable_verb10"), tabv_table_verb10);
    ADD_VERB(BIGSTRING("\ptable_verb11"), tabv_table_verb11);
    ADD_VERB(BIGSTRING("\ptable_verb12"), tabv_table_verb12);
    ADD_VERB(BIGSTRING("\ptable_verb13"), tabv_table_verb13);
    ADD_VERB(BIGSTRING("\ptable_verb14"), tabv_table_verb14);
    ADD_VERB(BIGSTRING("\ptable_verb15"), tabv_table_verb15);
    ADD_VERB(BIGSTRING("\ptable_verb16"), tabv_table_verb16);
    ADD_VERB(BIGSTRING("\ptable_verb17"), tabv_table_verb17);

    #undef ADD_VERB

    pophashtable();
    return true;
}
