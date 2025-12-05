#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the bit processor */
enum {
    bitv_bit_verb0 = 0,
    bitv_bit_verb1 = 1,
    bitv_bit_verb2 = 2,
    bitv_bit_verb3 = 3,
    bitv_bit_verb4 = 4,
    bitv_bit_verb5 = 5,
    bitv_bit_verb6 = 6,
    bitv_bit_verb7 = 7
};

static boolean bit_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case bitv_bit_verb0:
            /* TODO: Implement bit.bit_verb0 */
            return false;
        case bitv_bit_verb1:
            /* TODO: Implement bit.bit_verb1 */
            return false;
        case bitv_bit_verb2:
            /* TODO: Implement bit.bit_verb2 */
            return false;
        case bitv_bit_verb3:
            /* TODO: Implement bit.bit_verb3 */
            return false;
        case bitv_bit_verb4:
            /* TODO: Implement bit.bit_verb4 */
            return false;
        case bitv_bit_verb5:
            /* TODO: Implement bit.bit_verb5 */
            return false;
        case bitv_bit_verb6:
            /* TODO: Implement bit.bit_verb6 */
            return false;
        case bitv_bit_verb7:
            /* TODO: Implement bit.bit_verb7 */
            return false;
        default:
            return false;
    }
}

boolean bitinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pbit"), bsname);

    if (!newfunctionprocessor(bsname, &bit_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pbit_verb0"), bitv_bit_verb0);
    ADD_VERB(BIGSTRING("\pbit_verb1"), bitv_bit_verb1);
    ADD_VERB(BIGSTRING("\pbit_verb2"), bitv_bit_verb2);
    ADD_VERB(BIGSTRING("\pbit_verb3"), bitv_bit_verb3);
    ADD_VERB(BIGSTRING("\pbit_verb4"), bitv_bit_verb4);
    ADD_VERB(BIGSTRING("\pbit_verb5"), bitv_bit_verb5);
    ADD_VERB(BIGSTRING("\pbit_verb6"), bitv_bit_verb6);
    ADD_VERB(BIGSTRING("\pbit_verb7"), bitv_bit_verb7);

    #undef ADD_VERB

    pophashtable();
    return true;
}
