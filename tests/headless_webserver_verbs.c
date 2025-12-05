#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the webserver processor */
enum {
    webv_webserver_verb0 = 0,
    webv_webserver_verb1 = 1,
    webv_webserver_verb2 = 2,
    webv_webserver_verb3 = 3,
    webv_webserver_verb4 = 4,
    webv_webserver_verb5 = 5,
    webv_webserver_verb6 = 6
};

static boolean webserver_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case webv_webserver_verb0:
            /* TODO: Implement webserver.webserver_verb0 */
            return false;
        case webv_webserver_verb1:
            /* TODO: Implement webserver.webserver_verb1 */
            return false;
        case webv_webserver_verb2:
            /* TODO: Implement webserver.webserver_verb2 */
            return false;
        case webv_webserver_verb3:
            /* TODO: Implement webserver.webserver_verb3 */
            return false;
        case webv_webserver_verb4:
            /* TODO: Implement webserver.webserver_verb4 */
            return false;
        case webv_webserver_verb5:
            /* TODO: Implement webserver.webserver_verb5 */
            return false;
        case webv_webserver_verb6:
            /* TODO: Implement webserver.webserver_verb6 */
            return false;
        default:
            return false;
    }
}

boolean webserverinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pwebserver"), bsname);

    if (!newfunctionprocessor(bsname, &webserver_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pwebserver_verb0"), webv_webserver_verb0);
    ADD_VERB(BIGSTRING("\pwebserver_verb1"), webv_webserver_verb1);
    ADD_VERB(BIGSTRING("\pwebserver_verb2"), webv_webserver_verb2);
    ADD_VERB(BIGSTRING("\pwebserver_verb3"), webv_webserver_verb3);
    ADD_VERB(BIGSTRING("\pwebserver_verb4"), webv_webserver_verb4);
    ADD_VERB(BIGSTRING("\pwebserver_verb5"), webv_webserver_verb5);
    ADD_VERB(BIGSTRING("\pwebserver_verb6"), webv_webserver_verb6);

    #undef ADD_VERB

    pophashtable();
    return true;
}
