#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the lang processor */
enum {
    lanv_lang_verb0 = 0,
    lanv_lang_verb1 = 1,
    lanv_lang_verb2 = 2,
    lanv_lang_verb3 = 3,
    lanv_lang_verb4 = 4,
    lanv_lang_verb5 = 5,
    lanv_lang_verb6 = 6,
    lanv_lang_verb7 = 7,
    lanv_lang_verb8 = 8,
    lanv_lang_verb9 = 9,
    lanv_lang_verb10 = 10,
    lanv_lang_verb11 = 11,
    lanv_lang_verb12 = 12,
    lanv_lang_verb13 = 13,
    lanv_lang_verb14 = 14,
    lanv_lang_verb15 = 15,
    lanv_lang_verb16 = 16,
    lanv_lang_verb17 = 17,
    lanv_lang_verb18 = 18,
    lanv_lang_verb19 = 19,
    lanv_lang_verb20 = 20,
    lanv_lang_verb21 = 21,
    lanv_lang_verb22 = 22,
    lanv_lang_verb23 = 23,
    lanv_lang_verb24 = 24,
    lanv_lang_verb25 = 25,
    lanv_lang_verb26 = 26,
    lanv_lang_verb27 = 27,
    lanv_lang_verb28 = 28,
    lanv_lang_verb29 = 29,
    lanv_lang_verb30 = 30,
    lanv_lang_verb31 = 31,
    lanv_lang_verb32 = 32,
    lanv_lang_verb33 = 33,
    lanv_lang_verb34 = 34,
    lanv_lang_verb35 = 35,
    lanv_lang_verb36 = 36,
    lanv_lang_verb37 = 37,
    lanv_lang_verb38 = 38,
    lanv_lang_verb39 = 39,
    lanv_lang_verb40 = 40,
    lanv_lang_verb41 = 41,
    lanv_lang_verb42 = 42,
    lanv_lang_verb43 = 43,
    lanv_lang_verb44 = 44,
    lanv_lang_verb45 = 45,
    lanv_lang_verb46 = 46,
    lanv_lang_verb47 = 47,
    lanv_lang_verb48 = 48,
    lanv_lang_verb49 = 49,
    lanv_lang_verb50 = 50,
    lanv_lang_verb51 = 51,
    lanv_lang_verb52 = 52,
    lanv_lang_verb53 = 53,
    lanv_lang_verb54 = 54,
    lanv_lang_verb55 = 55,
    lanv_lang_verb56 = 56,
    lanv_lang_verb57 = 57
};

static boolean lang_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case lanv_lang_verb0:
            /* TODO: Implement lang.lang_verb0 */
            return false;
        case lanv_lang_verb1:
            /* TODO: Implement lang.lang_verb1 */
            return false;
        case lanv_lang_verb2:
            /* TODO: Implement lang.lang_verb2 */
            return false;
        case lanv_lang_verb3:
            /* TODO: Implement lang.lang_verb3 */
            return false;
        case lanv_lang_verb4:
            /* TODO: Implement lang.lang_verb4 */
            return false;
        case lanv_lang_verb5:
            /* TODO: Implement lang.lang_verb5 */
            return false;
        case lanv_lang_verb6:
            /* TODO: Implement lang.lang_verb6 */
            return false;
        case lanv_lang_verb7:
            /* TODO: Implement lang.lang_verb7 */
            return false;
        case lanv_lang_verb8:
            /* TODO: Implement lang.lang_verb8 */
            return false;
        case lanv_lang_verb9:
            /* TODO: Implement lang.lang_verb9 */
            return false;
        case lanv_lang_verb10:
            /* TODO: Implement lang.lang_verb10 */
            return false;
        case lanv_lang_verb11:
            /* TODO: Implement lang.lang_verb11 */
            return false;
        case lanv_lang_verb12:
            /* TODO: Implement lang.lang_verb12 */
            return false;
        case lanv_lang_verb13:
            /* TODO: Implement lang.lang_verb13 */
            return false;
        case lanv_lang_verb14:
            /* TODO: Implement lang.lang_verb14 */
            return false;
        case lanv_lang_verb15:
            /* TODO: Implement lang.lang_verb15 */
            return false;
        case lanv_lang_verb16:
            /* TODO: Implement lang.lang_verb16 */
            return false;
        case lanv_lang_verb17:
            /* TODO: Implement lang.lang_verb17 */
            return false;
        case lanv_lang_verb18:
            /* TODO: Implement lang.lang_verb18 */
            return false;
        case lanv_lang_verb19:
            /* TODO: Implement lang.lang_verb19 */
            return false;
        case lanv_lang_verb20:
            /* TODO: Implement lang.lang_verb20 */
            return false;
        case lanv_lang_verb21:
            /* TODO: Implement lang.lang_verb21 */
            return false;
        case lanv_lang_verb22:
            /* TODO: Implement lang.lang_verb22 */
            return false;
        case lanv_lang_verb23:
            /* TODO: Implement lang.lang_verb23 */
            return false;
        case lanv_lang_verb24:
            /* TODO: Implement lang.lang_verb24 */
            return false;
        case lanv_lang_verb25:
            /* TODO: Implement lang.lang_verb25 */
            return false;
        case lanv_lang_verb26:
            /* TODO: Implement lang.lang_verb26 */
            return false;
        case lanv_lang_verb27:
            /* TODO: Implement lang.lang_verb27 */
            return false;
        case lanv_lang_verb28:
            /* TODO: Implement lang.lang_verb28 */
            return false;
        case lanv_lang_verb29:
            /* TODO: Implement lang.lang_verb29 */
            return false;
        case lanv_lang_verb30:
            /* TODO: Implement lang.lang_verb30 */
            return false;
        case lanv_lang_verb31:
            /* TODO: Implement lang.lang_verb31 */
            return false;
        case lanv_lang_verb32:
            /* TODO: Implement lang.lang_verb32 */
            return false;
        case lanv_lang_verb33:
            /* TODO: Implement lang.lang_verb33 */
            return false;
        case lanv_lang_verb34:
            /* TODO: Implement lang.lang_verb34 */
            return false;
        case lanv_lang_verb35:
            /* TODO: Implement lang.lang_verb35 */
            return false;
        case lanv_lang_verb36:
            /* TODO: Implement lang.lang_verb36 */
            return false;
        case lanv_lang_verb37:
            /* TODO: Implement lang.lang_verb37 */
            return false;
        case lanv_lang_verb38:
            /* TODO: Implement lang.lang_verb38 */
            return false;
        case lanv_lang_verb39:
            /* TODO: Implement lang.lang_verb39 */
            return false;
        case lanv_lang_verb40:
            /* TODO: Implement lang.lang_verb40 */
            return false;
        case lanv_lang_verb41:
            /* TODO: Implement lang.lang_verb41 */
            return false;
        case lanv_lang_verb42:
            /* TODO: Implement lang.lang_verb42 */
            return false;
        case lanv_lang_verb43:
            /* TODO: Implement lang.lang_verb43 */
            return false;
        case lanv_lang_verb44:
            /* TODO: Implement lang.lang_verb44 */
            return false;
        case lanv_lang_verb45:
            /* TODO: Implement lang.lang_verb45 */
            return false;
        case lanv_lang_verb46:
            /* TODO: Implement lang.lang_verb46 */
            return false;
        case lanv_lang_verb47:
            /* TODO: Implement lang.lang_verb47 */
            return false;
        case lanv_lang_verb48:
            /* TODO: Implement lang.lang_verb48 */
            return false;
        case lanv_lang_verb49:
            /* TODO: Implement lang.lang_verb49 */
            return false;
        case lanv_lang_verb50:
            /* TODO: Implement lang.lang_verb50 */
            return false;
        case lanv_lang_verb51:
            /* TODO: Implement lang.lang_verb51 */
            return false;
        case lanv_lang_verb52:
            /* TODO: Implement lang.lang_verb52 */
            return false;
        case lanv_lang_verb53:
            /* TODO: Implement lang.lang_verb53 */
            return false;
        case lanv_lang_verb54:
            /* TODO: Implement lang.lang_verb54 */
            return false;
        case lanv_lang_verb55:
            /* TODO: Implement lang.lang_verb55 */
            return false;
        case lanv_lang_verb56:
            /* TODO: Implement lang.lang_verb56 */
            return false;
        case lanv_lang_verb57:
            /* TODO: Implement lang.lang_verb57 */
            return false;
        default:
            return false;
    }
}

boolean langinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\plang"), bsname);

    if (!newfunctionprocessor(bsname, &lang_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\plang_verb0"), lanv_lang_verb0);
    ADD_VERB(BIGSTRING("\plang_verb1"), lanv_lang_verb1);
    ADD_VERB(BIGSTRING("\plang_verb2"), lanv_lang_verb2);
    ADD_VERB(BIGSTRING("\plang_verb3"), lanv_lang_verb3);
    ADD_VERB(BIGSTRING("\plang_verb4"), lanv_lang_verb4);
    ADD_VERB(BIGSTRING("\plang_verb5"), lanv_lang_verb5);
    ADD_VERB(BIGSTRING("\plang_verb6"), lanv_lang_verb6);
    ADD_VERB(BIGSTRING("\plang_verb7"), lanv_lang_verb7);
    ADD_VERB(BIGSTRING("\plang_verb8"), lanv_lang_verb8);
    ADD_VERB(BIGSTRING("\plang_verb9"), lanv_lang_verb9);
    ADD_VERB(BIGSTRING("\plang_verb10"), lanv_lang_verb10);
    ADD_VERB(BIGSTRING("\plang_verb11"), lanv_lang_verb11);
    ADD_VERB(BIGSTRING("\plang_verb12"), lanv_lang_verb12);
    ADD_VERB(BIGSTRING("\plang_verb13"), lanv_lang_verb13);
    ADD_VERB(BIGSTRING("\plang_verb14"), lanv_lang_verb14);
    ADD_VERB(BIGSTRING("\plang_verb15"), lanv_lang_verb15);
    ADD_VERB(BIGSTRING("\plang_verb16"), lanv_lang_verb16);
    ADD_VERB(BIGSTRING("\plang_verb17"), lanv_lang_verb17);
    ADD_VERB(BIGSTRING("\plang_verb18"), lanv_lang_verb18);
    ADD_VERB(BIGSTRING("\plang_verb19"), lanv_lang_verb19);
    ADD_VERB(BIGSTRING("\plang_verb20"), lanv_lang_verb20);
    ADD_VERB(BIGSTRING("\plang_verb21"), lanv_lang_verb21);
    ADD_VERB(BIGSTRING("\plang_verb22"), lanv_lang_verb22);
    ADD_VERB(BIGSTRING("\plang_verb23"), lanv_lang_verb23);
    ADD_VERB(BIGSTRING("\plang_verb24"), lanv_lang_verb24);
    ADD_VERB(BIGSTRING("\plang_verb25"), lanv_lang_verb25);
    ADD_VERB(BIGSTRING("\plang_verb26"), lanv_lang_verb26);
    ADD_VERB(BIGSTRING("\plang_verb27"), lanv_lang_verb27);
    ADD_VERB(BIGSTRING("\plang_verb28"), lanv_lang_verb28);
    ADD_VERB(BIGSTRING("\plang_verb29"), lanv_lang_verb29);
    ADD_VERB(BIGSTRING("\plang_verb30"), lanv_lang_verb30);
    ADD_VERB(BIGSTRING("\plang_verb31"), lanv_lang_verb31);
    ADD_VERB(BIGSTRING("\plang_verb32"), lanv_lang_verb32);
    ADD_VERB(BIGSTRING("\plang_verb33"), lanv_lang_verb33);
    ADD_VERB(BIGSTRING("\plang_verb34"), lanv_lang_verb34);
    ADD_VERB(BIGSTRING("\plang_verb35"), lanv_lang_verb35);
    ADD_VERB(BIGSTRING("\plang_verb36"), lanv_lang_verb36);
    ADD_VERB(BIGSTRING("\plang_verb37"), lanv_lang_verb37);
    ADD_VERB(BIGSTRING("\plang_verb38"), lanv_lang_verb38);
    ADD_VERB(BIGSTRING("\plang_verb39"), lanv_lang_verb39);
    ADD_VERB(BIGSTRING("\plang_verb40"), lanv_lang_verb40);
    ADD_VERB(BIGSTRING("\plang_verb41"), lanv_lang_verb41);
    ADD_VERB(BIGSTRING("\plang_verb42"), lanv_lang_verb42);
    ADD_VERB(BIGSTRING("\plang_verb43"), lanv_lang_verb43);
    ADD_VERB(BIGSTRING("\plang_verb44"), lanv_lang_verb44);
    ADD_VERB(BIGSTRING("\plang_verb45"), lanv_lang_verb45);
    ADD_VERB(BIGSTRING("\plang_verb46"), lanv_lang_verb46);
    ADD_VERB(BIGSTRING("\plang_verb47"), lanv_lang_verb47);
    ADD_VERB(BIGSTRING("\plang_verb48"), lanv_lang_verb48);
    ADD_VERB(BIGSTRING("\plang_verb49"), lanv_lang_verb49);
    ADD_VERB(BIGSTRING("\plang_verb50"), lanv_lang_verb50);
    ADD_VERB(BIGSTRING("\plang_verb51"), lanv_lang_verb51);
    ADD_VERB(BIGSTRING("\plang_verb52"), lanv_lang_verb52);
    ADD_VERB(BIGSTRING("\plang_verb53"), lanv_lang_verb53);
    ADD_VERB(BIGSTRING("\plang_verb54"), lanv_lang_verb54);
    ADD_VERB(BIGSTRING("\plang_verb55"), lanv_lang_verb55);
    ADD_VERB(BIGSTRING("\plang_verb56"), lanv_lang_verb56);
    ADD_VERB(BIGSTRING("\plang_verb57"), lanv_lang_verb57);

    #undef ADD_VERB

    pophashtable();
    return true;
}
