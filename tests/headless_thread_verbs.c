#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the thread processor */
enum {
    thrv_thread_verb0 = 0,
    thrv_thread_verb1 = 1,
    thrv_thread_verb2 = 2,
    thrv_thread_verb3 = 3,
    thrv_thread_verb4 = 4,
    thrv_thread_verb5 = 5,
    thrv_thread_verb6 = 6,
    thrv_thread_verb7 = 7,
    thrv_thread_verb8 = 8,
    thrv_thread_verb9 = 9,
    thrv_thread_verb10 = 10,
    thrv_thread_verb11 = 11,
    thrv_thread_verb12 = 12,
    thrv_thread_verb13 = 13,
    thrv_thread_verb14 = 14,
    thrv_thread_verb15 = 15,
    thrv_thread_verb16 = 16
};

static boolean thread_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case thrv_thread_verb0:
            /* Verb #0 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb1:
            /* Verb #1 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb2:
            /* Verb #2 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb3:
            /* Verb #3 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb4:
            /* Verb #4 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb5:
            /* Verb #5 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb6:
            /* Verb #6 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb7:
            /* Verb #7 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb8:
            /* Verb #8 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb9:
            /* Verb #9 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb10:
            /* Verb #10 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb11:
            /* Verb #11 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb12:
            /* Verb #12 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb13:
            /* Verb #13 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb14:
            /* Verb #14 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb15:
            /* Verb #15 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_thread_verb16:
            /* Verb #16 - not yet implemented */
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean threadinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pthread"), bsname);

    if (!newfunctionprocessor(bsname, &thread_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pthread_verb0"), thrv_thread_verb0);
    ADD_VERB(BIGSTRING("\pthread_verb1"), thrv_thread_verb1);
    ADD_VERB(BIGSTRING("\pthread_verb2"), thrv_thread_verb2);
    ADD_VERB(BIGSTRING("\pthread_verb3"), thrv_thread_verb3);
    ADD_VERB(BIGSTRING("\pthread_verb4"), thrv_thread_verb4);
    ADD_VERB(BIGSTRING("\pthread_verb5"), thrv_thread_verb5);
    ADD_VERB(BIGSTRING("\pthread_verb6"), thrv_thread_verb6);
    ADD_VERB(BIGSTRING("\pthread_verb7"), thrv_thread_verb7);
    ADD_VERB(BIGSTRING("\pthread_verb8"), thrv_thread_verb8);
    ADD_VERB(BIGSTRING("\pthread_verb9"), thrv_thread_verb9);
    ADD_VERB(BIGSTRING("\pthread_verb10"), thrv_thread_verb10);
    ADD_VERB(BIGSTRING("\pthread_verb11"), thrv_thread_verb11);
    ADD_VERB(BIGSTRING("\pthread_verb12"), thrv_thread_verb12);
    ADD_VERB(BIGSTRING("\pthread_verb13"), thrv_thread_verb13);
    ADD_VERB(BIGSTRING("\pthread_verb14"), thrv_thread_verb14);
    ADD_VERB(BIGSTRING("\pthread_verb15"), thrv_thread_verb15);
    ADD_VERB(BIGSTRING("\pthread_verb16"), thrv_thread_verb16);

    #undef ADD_VERB

    pophashtable();
    return true;
}
