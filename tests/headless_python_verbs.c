#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the python processor */
enum {
    pytv_python_verb0 = 0
};

static boolean python_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case pytv_python_verb0:
            /* TODO: Implement python.python_verb0 */
            return false;
        default:
            return false;
    }
}

boolean pythoninitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\ppython"), bsname);

    if (!newfunctionprocessor(bsname, &python_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\ppython_verb0"), pytv_python_verb0);

    #undef ADD_VERB

    pophashtable();
    return true;
}
