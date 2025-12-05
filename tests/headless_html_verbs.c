#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the html processor */
enum {
    htmv_html_verb0 = 0,
    htmv_html_verb1 = 1,
    htmv_html_verb2 = 2,
    htmv_html_verb3 = 3,
    htmv_html_verb4 = 4,
    htmv_html_verb5 = 5,
    htmv_html_verb6 = 6,
    htmv_html_verb7 = 7,
    htmv_html_verb8 = 8,
    htmv_html_verb9 = 9,
    htmv_html_verb10 = 10,
    htmv_html_verb11 = 11,
    htmv_html_verb12 = 12,
    htmv_html_verb13 = 13,
    htmv_html_verb14 = 14,
    htmv_html_verb15 = 15,
    htmv_html_verb16 = 16,
    htmv_html_verb17 = 17,
    htmv_html_verb18 = 18,
    htmv_html_verb19 = 19,
    htmv_html_verb20 = 20,
    htmv_html_verb21 = 21,
    htmv_html_verb22 = 22
};

static boolean html_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case htmv_html_verb0:
            /* TODO: Implement html.html_verb0 */
            return false;
        case htmv_html_verb1:
            /* TODO: Implement html.html_verb1 */
            return false;
        case htmv_html_verb2:
            /* TODO: Implement html.html_verb2 */
            return false;
        case htmv_html_verb3:
            /* TODO: Implement html.html_verb3 */
            return false;
        case htmv_html_verb4:
            /* TODO: Implement html.html_verb4 */
            return false;
        case htmv_html_verb5:
            /* TODO: Implement html.html_verb5 */
            return false;
        case htmv_html_verb6:
            /* TODO: Implement html.html_verb6 */
            return false;
        case htmv_html_verb7:
            /* TODO: Implement html.html_verb7 */
            return false;
        case htmv_html_verb8:
            /* TODO: Implement html.html_verb8 */
            return false;
        case htmv_html_verb9:
            /* TODO: Implement html.html_verb9 */
            return false;
        case htmv_html_verb10:
            /* TODO: Implement html.html_verb10 */
            return false;
        case htmv_html_verb11:
            /* TODO: Implement html.html_verb11 */
            return false;
        case htmv_html_verb12:
            /* TODO: Implement html.html_verb12 */
            return false;
        case htmv_html_verb13:
            /* TODO: Implement html.html_verb13 */
            return false;
        case htmv_html_verb14:
            /* TODO: Implement html.html_verb14 */
            return false;
        case htmv_html_verb15:
            /* TODO: Implement html.html_verb15 */
            return false;
        case htmv_html_verb16:
            /* TODO: Implement html.html_verb16 */
            return false;
        case htmv_html_verb17:
            /* TODO: Implement html.html_verb17 */
            return false;
        case htmv_html_verb18:
            /* TODO: Implement html.html_verb18 */
            return false;
        case htmv_html_verb19:
            /* TODO: Implement html.html_verb19 */
            return false;
        case htmv_html_verb20:
            /* TODO: Implement html.html_verb20 */
            return false;
        case htmv_html_verb21:
            /* TODO: Implement html.html_verb21 */
            return false;
        case htmv_html_verb22:
            /* TODO: Implement html.html_verb22 */
            return false;
        default:
            return false;
    }
}

boolean htmlinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\phtml"), bsname);

    if (!newfunctionprocessor(bsname, &html_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\phtml_verb0"), htmv_html_verb0);
    ADD_VERB(BIGSTRING("\phtml_verb1"), htmv_html_verb1);
    ADD_VERB(BIGSTRING("\phtml_verb2"), htmv_html_verb2);
    ADD_VERB(BIGSTRING("\phtml_verb3"), htmv_html_verb3);
    ADD_VERB(BIGSTRING("\phtml_verb4"), htmv_html_verb4);
    ADD_VERB(BIGSTRING("\phtml_verb5"), htmv_html_verb5);
    ADD_VERB(BIGSTRING("\phtml_verb6"), htmv_html_verb6);
    ADD_VERB(BIGSTRING("\phtml_verb7"), htmv_html_verb7);
    ADD_VERB(BIGSTRING("\phtml_verb8"), htmv_html_verb8);
    ADD_VERB(BIGSTRING("\phtml_verb9"), htmv_html_verb9);
    ADD_VERB(BIGSTRING("\phtml_verb10"), htmv_html_verb10);
    ADD_VERB(BIGSTRING("\phtml_verb11"), htmv_html_verb11);
    ADD_VERB(BIGSTRING("\phtml_verb12"), htmv_html_verb12);
    ADD_VERB(BIGSTRING("\phtml_verb13"), htmv_html_verb13);
    ADD_VERB(BIGSTRING("\phtml_verb14"), htmv_html_verb14);
    ADD_VERB(BIGSTRING("\phtml_verb15"), htmv_html_verb15);
    ADD_VERB(BIGSTRING("\phtml_verb16"), htmv_html_verb16);
    ADD_VERB(BIGSTRING("\phtml_verb17"), htmv_html_verb17);
    ADD_VERB(BIGSTRING("\phtml_verb18"), htmv_html_verb18);
    ADD_VERB(BIGSTRING("\phtml_verb19"), htmv_html_verb19);
    ADD_VERB(BIGSTRING("\phtml_verb20"), htmv_html_verb20);
    ADD_VERB(BIGSTRING("\phtml_verb21"), htmv_html_verb21);
    ADD_VERB(BIGSTRING("\phtml_verb22"), htmv_html_verb22);

    #undef ADD_VERB

    pophashtable();
    return true;
}
