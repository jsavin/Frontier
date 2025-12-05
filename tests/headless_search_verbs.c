#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the search processor */
enum {
    seav_search_verb0 = 0,
    seav_search_verb1 = 1,
    seav_search_verb2 = 2,
    seav_search_verb3 = 3,
    seav_search_verb4 = 4,
    seav_search_verb5 = 5
};

static boolean search_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case seav_search_verb0:
            /* TODO: Implement search.search_verb0 */
            return false;
        case seav_search_verb1:
            /* TODO: Implement search.search_verb1 */
            return false;
        case seav_search_verb2:
            /* TODO: Implement search.search_verb2 */
            return false;
        case seav_search_verb3:
            /* TODO: Implement search.search_verb3 */
            return false;
        case seav_search_verb4:
            /* TODO: Implement search.search_verb4 */
            return false;
        case seav_search_verb5:
            /* TODO: Implement search.search_verb5 */
            return false;
        default:
            return false;
    }
}

boolean searchinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\psearch"), bsname);

    if (!newfunctionprocessor(bsname, &search_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\psearch_verb0"), seav_search_verb0);
    ADD_VERB(BIGSTRING("\psearch_verb1"), seav_search_verb1);
    ADD_VERB(BIGSTRING("\psearch_verb2"), seav_search_verb2);
    ADD_VERB(BIGSTRING("\psearch_verb3"), seav_search_verb3);
    ADD_VERB(BIGSTRING("\psearch_verb4"), seav_search_verb4);
    ADD_VERB(BIGSTRING("\psearch_verb5"), seav_search_verb5);

    #undef ADD_VERB

    pophashtable();
    return true;
}
