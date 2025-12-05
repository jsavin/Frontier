#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the crypt processor */
enum {
    cryv_crypt_verb0 = 0,
    cryv_crypt_verb1 = 1,
    cryv_crypt_verb2 = 2,
    cryv_crypt_verb3 = 3,
    cryv_crypt_verb4 = 4
};

static boolean crypt_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case cryv_crypt_verb0:
            /* TODO: Implement crypt.crypt_verb0 */
            return false;
        case cryv_crypt_verb1:
            /* TODO: Implement crypt.crypt_verb1 */
            return false;
        case cryv_crypt_verb2:
            /* TODO: Implement crypt.crypt_verb2 */
            return false;
        case cryv_crypt_verb3:
            /* TODO: Implement crypt.crypt_verb3 */
            return false;
        case cryv_crypt_verb4:
            /* TODO: Implement crypt.crypt_verb4 */
            return false;
        default:
            return false;
    }
}

boolean cryptinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pcrypt"), bsname);

    if (!newfunctionprocessor(bsname, &crypt_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pcrypt_verb0"), cryv_crypt_verb0);
    ADD_VERB(BIGSTRING("\pcrypt_verb1"), cryv_crypt_verb1);
    ADD_VERB(BIGSTRING("\pcrypt_verb2"), cryv_crypt_verb2);
    ADD_VERB(BIGSTRING("\pcrypt_verb3"), cryv_crypt_verb3);
    ADD_VERB(BIGSTRING("\pcrypt_verb4"), cryv_crypt_verb4);

    #undef ADD_VERB

    pophashtable();
    return true;
}
