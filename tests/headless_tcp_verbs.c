#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the tcp processor */
enum {
    tcpv_tcp_verb0 = 0,
    tcpv_tcp_verb1 = 1,
    tcpv_tcp_verb2 = 2,
    tcpv_tcp_verb3 = 3,
    tcpv_tcp_verb4 = 4,
    tcpv_tcp_verb5 = 5,
    tcpv_tcp_verb6 = 6,
    tcpv_tcp_verb7 = 7,
    tcpv_tcp_verb8 = 8,
    tcpv_tcp_verb9 = 9,
    tcpv_tcp_verb10 = 10,
    tcpv_tcp_verb11 = 11,
    tcpv_tcp_verb12 = 12,
    tcpv_tcp_verb13 = 13,
    tcpv_tcp_verb14 = 14,
    tcpv_tcp_verb15 = 15,
    tcpv_tcp_verb16 = 16,
    tcpv_tcp_verb17 = 17,
    tcpv_tcp_verb18 = 18,
    tcpv_tcp_verb19 = 19,
    tcpv_tcp_verb20 = 20,
    tcpv_tcp_verb21 = 21,
    tcpv_tcp_verb22 = 22
};

static boolean tcp_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case tcpv_tcp_verb0:
            /* TODO: Implement tcp.tcp_verb0 */
            return false;
        case tcpv_tcp_verb1:
            /* TODO: Implement tcp.tcp_verb1 */
            return false;
        case tcpv_tcp_verb2:
            /* TODO: Implement tcp.tcp_verb2 */
            return false;
        case tcpv_tcp_verb3:
            /* TODO: Implement tcp.tcp_verb3 */
            return false;
        case tcpv_tcp_verb4:
            /* TODO: Implement tcp.tcp_verb4 */
            return false;
        case tcpv_tcp_verb5:
            /* TODO: Implement tcp.tcp_verb5 */
            return false;
        case tcpv_tcp_verb6:
            /* TODO: Implement tcp.tcp_verb6 */
            return false;
        case tcpv_tcp_verb7:
            /* TODO: Implement tcp.tcp_verb7 */
            return false;
        case tcpv_tcp_verb8:
            /* TODO: Implement tcp.tcp_verb8 */
            return false;
        case tcpv_tcp_verb9:
            /* TODO: Implement tcp.tcp_verb9 */
            return false;
        case tcpv_tcp_verb10:
            /* TODO: Implement tcp.tcp_verb10 */
            return false;
        case tcpv_tcp_verb11:
            /* TODO: Implement tcp.tcp_verb11 */
            return false;
        case tcpv_tcp_verb12:
            /* TODO: Implement tcp.tcp_verb12 */
            return false;
        case tcpv_tcp_verb13:
            /* TODO: Implement tcp.tcp_verb13 */
            return false;
        case tcpv_tcp_verb14:
            /* TODO: Implement tcp.tcp_verb14 */
            return false;
        case tcpv_tcp_verb15:
            /* TODO: Implement tcp.tcp_verb15 */
            return false;
        case tcpv_tcp_verb16:
            /* TODO: Implement tcp.tcp_verb16 */
            return false;
        case tcpv_tcp_verb17:
            /* TODO: Implement tcp.tcp_verb17 */
            return false;
        case tcpv_tcp_verb18:
            /* TODO: Implement tcp.tcp_verb18 */
            return false;
        case tcpv_tcp_verb19:
            /* TODO: Implement tcp.tcp_verb19 */
            return false;
        case tcpv_tcp_verb20:
            /* TODO: Implement tcp.tcp_verb20 */
            return false;
        case tcpv_tcp_verb21:
            /* TODO: Implement tcp.tcp_verb21 */
            return false;
        case tcpv_tcp_verb22:
            /* TODO: Implement tcp.tcp_verb22 */
            return false;
        default:
            return false;
    }
}

boolean tcpinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\ptcp"), bsname);

    if (!newfunctionprocessor(bsname, &tcp_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\ptcp_verb0"), tcpv_tcp_verb0);
    ADD_VERB(BIGSTRING("\ptcp_verb1"), tcpv_tcp_verb1);
    ADD_VERB(BIGSTRING("\ptcp_verb2"), tcpv_tcp_verb2);
    ADD_VERB(BIGSTRING("\ptcp_verb3"), tcpv_tcp_verb3);
    ADD_VERB(BIGSTRING("\ptcp_verb4"), tcpv_tcp_verb4);
    ADD_VERB(BIGSTRING("\ptcp_verb5"), tcpv_tcp_verb5);
    ADD_VERB(BIGSTRING("\ptcp_verb6"), tcpv_tcp_verb6);
    ADD_VERB(BIGSTRING("\ptcp_verb7"), tcpv_tcp_verb7);
    ADD_VERB(BIGSTRING("\ptcp_verb8"), tcpv_tcp_verb8);
    ADD_VERB(BIGSTRING("\ptcp_verb9"), tcpv_tcp_verb9);
    ADD_VERB(BIGSTRING("\ptcp_verb10"), tcpv_tcp_verb10);
    ADD_VERB(BIGSTRING("\ptcp_verb11"), tcpv_tcp_verb11);
    ADD_VERB(BIGSTRING("\ptcp_verb12"), tcpv_tcp_verb12);
    ADD_VERB(BIGSTRING("\ptcp_verb13"), tcpv_tcp_verb13);
    ADD_VERB(BIGSTRING("\ptcp_verb14"), tcpv_tcp_verb14);
    ADD_VERB(BIGSTRING("\ptcp_verb15"), tcpv_tcp_verb15);
    ADD_VERB(BIGSTRING("\ptcp_verb16"), tcpv_tcp_verb16);
    ADD_VERB(BIGSTRING("\ptcp_verb17"), tcpv_tcp_verb17);
    ADD_VERB(BIGSTRING("\ptcp_verb18"), tcpv_tcp_verb18);
    ADD_VERB(BIGSTRING("\ptcp_verb19"), tcpv_tcp_verb19);
    ADD_VERB(BIGSTRING("\ptcp_verb20"), tcpv_tcp_verb20);
    ADD_VERB(BIGSTRING("\ptcp_verb21"), tcpv_tcp_verb21);
    ADD_VERB(BIGSTRING("\ptcp_verb22"), tcpv_tcp_verb22);

    #undef ADD_VERB

    pophashtable();
    return true;
}
