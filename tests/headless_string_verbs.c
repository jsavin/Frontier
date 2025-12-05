#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the string processor */
enum {
    strv_string_verb0 = 0,
    strv_string_verb1 = 1,
    strv_string_verb2 = 2,
    strv_string_verb3 = 3,
    strv_string_verb4 = 4,
    strv_string_verb5 = 5,
    strv_string_verb6 = 6,
    strv_string_verb7 = 7,
    strv_string_verb8 = 8,
    strv_string_verb9 = 9,
    strv_string_verb10 = 10,
    strv_string_verb11 = 11,
    strv_string_verb12 = 12,
    strv_string_verb13 = 13,
    strv_string_verb14 = 14,
    strv_string_verb15 = 15,
    strv_string_verb16 = 16,
    strv_string_verb17 = 17,
    strv_string_verb18 = 18,
    strv_string_verb19 = 19,
    strv_string_verb20 = 20,
    strv_string_verb21 = 21,
    strv_string_verb22 = 22,
    strv_string_verb23 = 23,
    strv_string_verb24 = 24,
    strv_string_verb25 = 25,
    strv_string_verb26 = 26,
    strv_string_verb27 = 27,
    strv_string_verb28 = 28,
    strv_string_verb29 = 29,
    strv_string_verb30 = 30,
    strv_string_verb31 = 31,
    strv_string_verb32 = 32,
    strv_string_verb33 = 33,
    strv_string_verb34 = 34,
    strv_string_verb35 = 35,
    strv_string_verb36 = 36,
    strv_string_verb37 = 37,
    strv_string_verb38 = 38,
    strv_string_verb39 = 39,
    strv_string_verb40 = 40,
    strv_string_verb41 = 41,
    strv_string_verb42 = 42,
    strv_string_verb43 = 43,
    strv_string_verb44 = 44,
    strv_string_verb45 = 45,
    strv_string_verb46 = 46,
    strv_string_verb47 = 47,
    strv_string_verb48 = 48,
    strv_string_verb49 = 49,
    strv_string_verb50 = 50,
    strv_string_verb51 = 51,
    strv_string_verb52 = 52,
    strv_string_verb53 = 53,
    strv_string_verb54 = 54,
    strv_string_verb55 = 55,
    strv_string_verb56 = 56,
    strv_string_verb57 = 57,
    strv_string_verb58 = 58,
    strv_string_verb59 = 59
};

static boolean string_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case strv_string_verb0:
            /* TODO: Implement string.string_verb0 */
            return false;
        case strv_string_verb1:
            /* TODO: Implement string.string_verb1 */
            return false;
        case strv_string_verb2:
            /* TODO: Implement string.string_verb2 */
            return false;
        case strv_string_verb3:
            /* TODO: Implement string.string_verb3 */
            return false;
        case strv_string_verb4:
            /* TODO: Implement string.string_verb4 */
            return false;
        case strv_string_verb5:
            /* TODO: Implement string.string_verb5 */
            return false;
        case strv_string_verb6:
            /* TODO: Implement string.string_verb6 */
            return false;
        case strv_string_verb7:
            /* TODO: Implement string.string_verb7 */
            return false;
        case strv_string_verb8:
            /* TODO: Implement string.string_verb8 */
            return false;
        case strv_string_verb9:
            /* TODO: Implement string.string_verb9 */
            return false;
        case strv_string_verb10:
            /* TODO: Implement string.string_verb10 */
            return false;
        case strv_string_verb11:
            /* TODO: Implement string.string_verb11 */
            return false;
        case strv_string_verb12:
            /* TODO: Implement string.string_verb12 */
            return false;
        case strv_string_verb13:
            /* TODO: Implement string.string_verb13 */
            return false;
        case strv_string_verb14:
            /* TODO: Implement string.string_verb14 */
            return false;
        case strv_string_verb15:
            /* TODO: Implement string.string_verb15 */
            return false;
        case strv_string_verb16:
            /* TODO: Implement string.string_verb16 */
            return false;
        case strv_string_verb17:
            /* TODO: Implement string.string_verb17 */
            return false;
        case strv_string_verb18:
            /* TODO: Implement string.string_verb18 */
            return false;
        case strv_string_verb19:
            /* TODO: Implement string.string_verb19 */
            return false;
        case strv_string_verb20:
            /* TODO: Implement string.string_verb20 */
            return false;
        case strv_string_verb21:
            /* TODO: Implement string.string_verb21 */
            return false;
        case strv_string_verb22:
            /* TODO: Implement string.string_verb22 */
            return false;
        case strv_string_verb23:
            /* TODO: Implement string.string_verb23 */
            return false;
        case strv_string_verb24:
            /* TODO: Implement string.string_verb24 */
            return false;
        case strv_string_verb25:
            /* TODO: Implement string.string_verb25 */
            return false;
        case strv_string_verb26:
            /* TODO: Implement string.string_verb26 */
            return false;
        case strv_string_verb27:
            /* TODO: Implement string.string_verb27 */
            return false;
        case strv_string_verb28:
            /* TODO: Implement string.string_verb28 */
            return false;
        case strv_string_verb29:
            /* TODO: Implement string.string_verb29 */
            return false;
        case strv_string_verb30:
            /* TODO: Implement string.string_verb30 */
            return false;
        case strv_string_verb31:
            /* TODO: Implement string.string_verb31 */
            return false;
        case strv_string_verb32:
            /* TODO: Implement string.string_verb32 */
            return false;
        case strv_string_verb33:
            /* TODO: Implement string.string_verb33 */
            return false;
        case strv_string_verb34:
            /* TODO: Implement string.string_verb34 */
            return false;
        case strv_string_verb35:
            /* TODO: Implement string.string_verb35 */
            return false;
        case strv_string_verb36:
            /* TODO: Implement string.string_verb36 */
            return false;
        case strv_string_verb37:
            /* TODO: Implement string.string_verb37 */
            return false;
        case strv_string_verb38:
            /* TODO: Implement string.string_verb38 */
            return false;
        case strv_string_verb39:
            /* TODO: Implement string.string_verb39 */
            return false;
        case strv_string_verb40:
            /* TODO: Implement string.string_verb40 */
            return false;
        case strv_string_verb41:
            /* TODO: Implement string.string_verb41 */
            return false;
        case strv_string_verb42:
            /* TODO: Implement string.string_verb42 */
            return false;
        case strv_string_verb43:
            /* TODO: Implement string.string_verb43 */
            return false;
        case strv_string_verb44:
            /* TODO: Implement string.string_verb44 */
            return false;
        case strv_string_verb45:
            /* TODO: Implement string.string_verb45 */
            return false;
        case strv_string_verb46:
            /* TODO: Implement string.string_verb46 */
            return false;
        case strv_string_verb47:
            /* TODO: Implement string.string_verb47 */
            return false;
        case strv_string_verb48:
            /* TODO: Implement string.string_verb48 */
            return false;
        case strv_string_verb49:
            /* TODO: Implement string.string_verb49 */
            return false;
        case strv_string_verb50:
            /* TODO: Implement string.string_verb50 */
            return false;
        case strv_string_verb51:
            /* TODO: Implement string.string_verb51 */
            return false;
        case strv_string_verb52:
            /* TODO: Implement string.string_verb52 */
            return false;
        case strv_string_verb53:
            /* TODO: Implement string.string_verb53 */
            return false;
        case strv_string_verb54:
            /* TODO: Implement string.string_verb54 */
            return false;
        case strv_string_verb55:
            /* TODO: Implement string.string_verb55 */
            return false;
        case strv_string_verb56:
            /* TODO: Implement string.string_verb56 */
            return false;
        case strv_string_verb57:
            /* TODO: Implement string.string_verb57 */
            return false;
        case strv_string_verb58:
            /* TODO: Implement string.string_verb58 */
            return false;
        case strv_string_verb59:
            /* TODO: Implement string.string_verb59 */
            return false;
        default:
            return false;
    }
}

boolean stringinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pstring"), bsname);

    if (!newfunctionprocessor(bsname, &string_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pstring_verb0"), strv_string_verb0);
    ADD_VERB(BIGSTRING("\pstring_verb1"), strv_string_verb1);
    ADD_VERB(BIGSTRING("\pstring_verb2"), strv_string_verb2);
    ADD_VERB(BIGSTRING("\pstring_verb3"), strv_string_verb3);
    ADD_VERB(BIGSTRING("\pstring_verb4"), strv_string_verb4);
    ADD_VERB(BIGSTRING("\pstring_verb5"), strv_string_verb5);
    ADD_VERB(BIGSTRING("\pstring_verb6"), strv_string_verb6);
    ADD_VERB(BIGSTRING("\pstring_verb7"), strv_string_verb7);
    ADD_VERB(BIGSTRING("\pstring_verb8"), strv_string_verb8);
    ADD_VERB(BIGSTRING("\pstring_verb9"), strv_string_verb9);
    ADD_VERB(BIGSTRING("\pstring_verb10"), strv_string_verb10);
    ADD_VERB(BIGSTRING("\pstring_verb11"), strv_string_verb11);
    ADD_VERB(BIGSTRING("\pstring_verb12"), strv_string_verb12);
    ADD_VERB(BIGSTRING("\pstring_verb13"), strv_string_verb13);
    ADD_VERB(BIGSTRING("\pstring_verb14"), strv_string_verb14);
    ADD_VERB(BIGSTRING("\pstring_verb15"), strv_string_verb15);
    ADD_VERB(BIGSTRING("\pstring_verb16"), strv_string_verb16);
    ADD_VERB(BIGSTRING("\pstring_verb17"), strv_string_verb17);
    ADD_VERB(BIGSTRING("\pstring_verb18"), strv_string_verb18);
    ADD_VERB(BIGSTRING("\pstring_verb19"), strv_string_verb19);
    ADD_VERB(BIGSTRING("\pstring_verb20"), strv_string_verb20);
    ADD_VERB(BIGSTRING("\pstring_verb21"), strv_string_verb21);
    ADD_VERB(BIGSTRING("\pstring_verb22"), strv_string_verb22);
    ADD_VERB(BIGSTRING("\pstring_verb23"), strv_string_verb23);
    ADD_VERB(BIGSTRING("\pstring_verb24"), strv_string_verb24);
    ADD_VERB(BIGSTRING("\pstring_verb25"), strv_string_verb25);
    ADD_VERB(BIGSTRING("\pstring_verb26"), strv_string_verb26);
    ADD_VERB(BIGSTRING("\pstring_verb27"), strv_string_verb27);
    ADD_VERB(BIGSTRING("\pstring_verb28"), strv_string_verb28);
    ADD_VERB(BIGSTRING("\pstring_verb29"), strv_string_verb29);
    ADD_VERB(BIGSTRING("\pstring_verb30"), strv_string_verb30);
    ADD_VERB(BIGSTRING("\pstring_verb31"), strv_string_verb31);
    ADD_VERB(BIGSTRING("\pstring_verb32"), strv_string_verb32);
    ADD_VERB(BIGSTRING("\pstring_verb33"), strv_string_verb33);
    ADD_VERB(BIGSTRING("\pstring_verb34"), strv_string_verb34);
    ADD_VERB(BIGSTRING("\pstring_verb35"), strv_string_verb35);
    ADD_VERB(BIGSTRING("\pstring_verb36"), strv_string_verb36);
    ADD_VERB(BIGSTRING("\pstring_verb37"), strv_string_verb37);
    ADD_VERB(BIGSTRING("\pstring_verb38"), strv_string_verb38);
    ADD_VERB(BIGSTRING("\pstring_verb39"), strv_string_verb39);
    ADD_VERB(BIGSTRING("\pstring_verb40"), strv_string_verb40);
    ADD_VERB(BIGSTRING("\pstring_verb41"), strv_string_verb41);
    ADD_VERB(BIGSTRING("\pstring_verb42"), strv_string_verb42);
    ADD_VERB(BIGSTRING("\pstring_verb43"), strv_string_verb43);
    ADD_VERB(BIGSTRING("\pstring_verb44"), strv_string_verb44);
    ADD_VERB(BIGSTRING("\pstring_verb45"), strv_string_verb45);
    ADD_VERB(BIGSTRING("\pstring_verb46"), strv_string_verb46);
    ADD_VERB(BIGSTRING("\pstring_verb47"), strv_string_verb47);
    ADD_VERB(BIGSTRING("\pstring_verb48"), strv_string_verb48);
    ADD_VERB(BIGSTRING("\pstring_verb49"), strv_string_verb49);
    ADD_VERB(BIGSTRING("\pstring_verb50"), strv_string_verb50);
    ADD_VERB(BIGSTRING("\pstring_verb51"), strv_string_verb51);
    ADD_VERB(BIGSTRING("\pstring_verb52"), strv_string_verb52);
    ADD_VERB(BIGSTRING("\pstring_verb53"), strv_string_verb53);
    ADD_VERB(BIGSTRING("\pstring_verb54"), strv_string_verb54);
    ADD_VERB(BIGSTRING("\pstring_verb55"), strv_string_verb55);
    ADD_VERB(BIGSTRING("\pstring_verb56"), strv_string_verb56);
    ADD_VERB(BIGSTRING("\pstring_verb57"), strv_string_verb57);
    ADD_VERB(BIGSTRING("\pstring_verb58"), strv_string_verb58);
    ADD_VERB(BIGSTRING("\pstring_verb59"), strv_string_verb59);

    #undef ADD_VERB

    pophashtable();
    return true;
}
