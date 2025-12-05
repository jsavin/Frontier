#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the searchengine processor */
enum {
    seav_searchengine_verb0 = 0,
    seav_searchengine_verb1 = 1,
    seav_searchengine_verb2 = 2,
    seav_searchengine_verb3 = 3,
    seav_searchengine_verb4 = 4
};

static boolean searchengine_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case seav_searchengine_verb0:
            /* TODO: Implement searchengine.searchengine_verb0 */
            return false;
        case seav_searchengine_verb1:
            /* TODO: Implement searchengine.searchengine_verb1 */
            return false;
        case seav_searchengine_verb2:
            /* TODO: Implement searchengine.searchengine_verb2 */
            return false;
        case seav_searchengine_verb3:
            /* TODO: Implement searchengine.searchengine_verb3 */
            return false;
        case seav_searchengine_verb4:
            /* TODO: Implement searchengine.searchengine_verb4 */
            return false;
        default:
            return false;
    }
}

boolean searchengineinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\psearchengine"), bsname);

    if (!newfunctionprocessor(bsname, &searchengine_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\psearchengine_verb0"), seav_searchengine_verb0);
    ADD_VERB(BIGSTRING("\psearchengine_verb1"), seav_searchengine_verb1);
    ADD_VERB(BIGSTRING("\psearchengine_verb2"), seav_searchengine_verb2);
    ADD_VERB(BIGSTRING("\psearchengine_verb3"), seav_searchengine_verb3);
    ADD_VERB(BIGSTRING("\psearchengine_verb4"), seav_searchengine_verb4);

    #undef ADD_VERB

    pophashtable();
    return true;
}
