#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the dll processor */
enum {
    dllv_dll_verb0 = 0,
    dllv_dll_verb1 = 1,
    dllv_dll_verb2 = 2,
    dllv_dll_verb3 = 3
};

static boolean dll_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case dllv_dll_verb0:
            /* TODO: Implement dll.dll_verb0 */
            return false;
        case dllv_dll_verb1:
            /* TODO: Implement dll.dll_verb1 */
            return false;
        case dllv_dll_verb2:
            /* TODO: Implement dll.dll_verb2 */
            return false;
        case dllv_dll_verb3:
            /* TODO: Implement dll.dll_verb3 */
            return false;
        default:
            return false;
    }
}

boolean dllinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pdll"), bsname);

    if (!newfunctionprocessor(bsname, &dll_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pdll_verb0"), dllv_dll_verb0);
    ADD_VERB(BIGSTRING("\pdll_verb1"), dllv_dll_verb1);
    ADD_VERB(BIGSTRING("\pdll_verb2"), dllv_dll_verb2);
    ADD_VERB(BIGSTRING("\pdll_verb3"), dllv_dll_verb3);

    #undef ADD_VERB

    pophashtable();
    return true;
}
