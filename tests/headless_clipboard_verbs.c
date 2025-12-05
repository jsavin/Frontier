#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the clipboard processor */
enum {
    cliv_clipboard_verb0 = 0,
    cliv_clipboard_verb1 = 1
};

static boolean clipboard_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case cliv_clipboard_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case cliv_clipboard_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean clipboardinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pclipboard"), bsname);

    if (!newfunctionprocessor(bsname, &clipboard_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pclipboard_verb0"), cliv_clipboard_verb0);
    ADD_VERB(BIGSTRING("\pclipboard_verb1"), cliv_clipboard_verb1);

    #undef ADD_VERB

    pophashtable();
    return true;
}
