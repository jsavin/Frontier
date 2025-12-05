#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the dll processor */
enum {
    dllv_call = 0,
    dllv_load = 1,
    dllv_unload = 2,
    dllv_isloaded = 3
};

static boolean dll_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case dllv_call:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case dllv_load:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case dllv_unload:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case dllv_isloaded:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
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

    ADD_VERB(BIGSTRING("\pcall"), dllv_call);
    ADD_VERB(BIGSTRING("\pload"), dllv_load);
    ADD_VERB(BIGSTRING("\punload"), dllv_unload);
    ADD_VERB(BIGSTRING("\pisloaded"), dllv_isloaded);

    #undef ADD_VERB

    pophashtable();
    return true;
}
