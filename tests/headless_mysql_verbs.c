#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the mysql processor */
enum {
    mysv_mysql_verb0 = 0,
    mysv_mysql_verb1 = 1,
    mysv_mysql_verb2 = 2,
    mysv_mysql_verb3 = 3,
    mysv_mysql_verb4 = 4,
    mysv_mysql_verb5 = 5,
    mysv_mysql_verb6 = 6,
    mysv_mysql_verb7 = 7,
    mysv_mysql_verb8 = 8,
    mysv_mysql_verb9 = 9,
    mysv_mysql_verb10 = 10,
    mysv_mysql_verb11 = 11,
    mysv_mysql_verb12 = 12,
    mysv_mysql_verb13 = 13,
    mysv_mysql_verb14 = 14,
    mysv_mysql_verb15 = 15,
    mysv_mysql_verb16 = 16,
    mysv_mysql_verb17 = 17,
    mysv_mysql_verb18 = 18,
    mysv_mysql_verb19 = 19,
    mysv_mysql_verb20 = 20,
    mysv_mysql_verb21 = 21,
    mysv_mysql_verb22 = 22,
    mysv_mysql_verb23 = 23,
    mysv_mysql_verb24 = 24,
    mysv_mysql_verb25 = 25,
    mysv_mysql_verb26 = 26
};

static boolean mysql_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case mysv_mysql_verb0:
            /* TODO: Implement mysql.mysql_verb0 */
            return false;
        case mysv_mysql_verb1:
            /* TODO: Implement mysql.mysql_verb1 */
            return false;
        case mysv_mysql_verb2:
            /* TODO: Implement mysql.mysql_verb2 */
            return false;
        case mysv_mysql_verb3:
            /* TODO: Implement mysql.mysql_verb3 */
            return false;
        case mysv_mysql_verb4:
            /* TODO: Implement mysql.mysql_verb4 */
            return false;
        case mysv_mysql_verb5:
            /* TODO: Implement mysql.mysql_verb5 */
            return false;
        case mysv_mysql_verb6:
            /* TODO: Implement mysql.mysql_verb6 */
            return false;
        case mysv_mysql_verb7:
            /* TODO: Implement mysql.mysql_verb7 */
            return false;
        case mysv_mysql_verb8:
            /* TODO: Implement mysql.mysql_verb8 */
            return false;
        case mysv_mysql_verb9:
            /* TODO: Implement mysql.mysql_verb9 */
            return false;
        case mysv_mysql_verb10:
            /* TODO: Implement mysql.mysql_verb10 */
            return false;
        case mysv_mysql_verb11:
            /* TODO: Implement mysql.mysql_verb11 */
            return false;
        case mysv_mysql_verb12:
            /* TODO: Implement mysql.mysql_verb12 */
            return false;
        case mysv_mysql_verb13:
            /* TODO: Implement mysql.mysql_verb13 */
            return false;
        case mysv_mysql_verb14:
            /* TODO: Implement mysql.mysql_verb14 */
            return false;
        case mysv_mysql_verb15:
            /* TODO: Implement mysql.mysql_verb15 */
            return false;
        case mysv_mysql_verb16:
            /* TODO: Implement mysql.mysql_verb16 */
            return false;
        case mysv_mysql_verb17:
            /* TODO: Implement mysql.mysql_verb17 */
            return false;
        case mysv_mysql_verb18:
            /* TODO: Implement mysql.mysql_verb18 */
            return false;
        case mysv_mysql_verb19:
            /* TODO: Implement mysql.mysql_verb19 */
            return false;
        case mysv_mysql_verb20:
            /* TODO: Implement mysql.mysql_verb20 */
            return false;
        case mysv_mysql_verb21:
            /* TODO: Implement mysql.mysql_verb21 */
            return false;
        case mysv_mysql_verb22:
            /* TODO: Implement mysql.mysql_verb22 */
            return false;
        case mysv_mysql_verb23:
            /* TODO: Implement mysql.mysql_verb23 */
            return false;
        case mysv_mysql_verb24:
            /* TODO: Implement mysql.mysql_verb24 */
            return false;
        case mysv_mysql_verb25:
            /* TODO: Implement mysql.mysql_verb25 */
            return false;
        case mysv_mysql_verb26:
            /* TODO: Implement mysql.mysql_verb26 */
            return false;
        default:
            return false;
    }
}

boolean mysqlinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pmysql"), bsname);

    if (!newfunctionprocessor(bsname, &mysql_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pmysql_verb0"), mysv_mysql_verb0);
    ADD_VERB(BIGSTRING("\pmysql_verb1"), mysv_mysql_verb1);
    ADD_VERB(BIGSTRING("\pmysql_verb2"), mysv_mysql_verb2);
    ADD_VERB(BIGSTRING("\pmysql_verb3"), mysv_mysql_verb3);
    ADD_VERB(BIGSTRING("\pmysql_verb4"), mysv_mysql_verb4);
    ADD_VERB(BIGSTRING("\pmysql_verb5"), mysv_mysql_verb5);
    ADD_VERB(BIGSTRING("\pmysql_verb6"), mysv_mysql_verb6);
    ADD_VERB(BIGSTRING("\pmysql_verb7"), mysv_mysql_verb7);
    ADD_VERB(BIGSTRING("\pmysql_verb8"), mysv_mysql_verb8);
    ADD_VERB(BIGSTRING("\pmysql_verb9"), mysv_mysql_verb9);
    ADD_VERB(BIGSTRING("\pmysql_verb10"), mysv_mysql_verb10);
    ADD_VERB(BIGSTRING("\pmysql_verb11"), mysv_mysql_verb11);
    ADD_VERB(BIGSTRING("\pmysql_verb12"), mysv_mysql_verb12);
    ADD_VERB(BIGSTRING("\pmysql_verb13"), mysv_mysql_verb13);
    ADD_VERB(BIGSTRING("\pmysql_verb14"), mysv_mysql_verb14);
    ADD_VERB(BIGSTRING("\pmysql_verb15"), mysv_mysql_verb15);
    ADD_VERB(BIGSTRING("\pmysql_verb16"), mysv_mysql_verb16);
    ADD_VERB(BIGSTRING("\pmysql_verb17"), mysv_mysql_verb17);
    ADD_VERB(BIGSTRING("\pmysql_verb18"), mysv_mysql_verb18);
    ADD_VERB(BIGSTRING("\pmysql_verb19"), mysv_mysql_verb19);
    ADD_VERB(BIGSTRING("\pmysql_verb20"), mysv_mysql_verb20);
    ADD_VERB(BIGSTRING("\pmysql_verb21"), mysv_mysql_verb21);
    ADD_VERB(BIGSTRING("\pmysql_verb22"), mysv_mysql_verb22);
    ADD_VERB(BIGSTRING("\pmysql_verb23"), mysv_mysql_verb23);
    ADD_VERB(BIGSTRING("\pmysql_verb24"), mysv_mysql_verb24);
    ADD_VERB(BIGSTRING("\pmysql_verb25"), mysv_mysql_verb25);
    ADD_VERB(BIGSTRING("\pmysql_verb26"), mysv_mysql_verb26);

    #undef ADD_VERB

    pophashtable();
    return true;
}
