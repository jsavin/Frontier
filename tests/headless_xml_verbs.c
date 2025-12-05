#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the xml processor */
enum {
    xmlv_xml_verb0 = 0,
    xmlv_xml_verb1 = 1,
    xmlv_xml_verb2 = 2,
    xmlv_xml_verb3 = 3,
    xmlv_xml_verb4 = 4,
    xmlv_xml_verb5 = 5,
    xmlv_xml_verb6 = 6,
    xmlv_xml_verb7 = 7,
    xmlv_xml_verb8 = 8,
    xmlv_xml_verb9 = 9,
    xmlv_xml_verb10 = 10,
    xmlv_xml_verb11 = 11,
    xmlv_xml_verb12 = 12,
    xmlv_xml_verb13 = 13
};

static boolean xml_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case xmlv_xml_verb0:
            /* TODO: Implement xml.xml_verb0 */
            return false;
        case xmlv_xml_verb1:
            /* TODO: Implement xml.xml_verb1 */
            return false;
        case xmlv_xml_verb2:
            /* TODO: Implement xml.xml_verb2 */
            return false;
        case xmlv_xml_verb3:
            /* TODO: Implement xml.xml_verb3 */
            return false;
        case xmlv_xml_verb4:
            /* TODO: Implement xml.xml_verb4 */
            return false;
        case xmlv_xml_verb5:
            /* TODO: Implement xml.xml_verb5 */
            return false;
        case xmlv_xml_verb6:
            /* TODO: Implement xml.xml_verb6 */
            return false;
        case xmlv_xml_verb7:
            /* TODO: Implement xml.xml_verb7 */
            return false;
        case xmlv_xml_verb8:
            /* TODO: Implement xml.xml_verb8 */
            return false;
        case xmlv_xml_verb9:
            /* TODO: Implement xml.xml_verb9 */
            return false;
        case xmlv_xml_verb10:
            /* TODO: Implement xml.xml_verb10 */
            return false;
        case xmlv_xml_verb11:
            /* TODO: Implement xml.xml_verb11 */
            return false;
        case xmlv_xml_verb12:
            /* TODO: Implement xml.xml_verb12 */
            return false;
        case xmlv_xml_verb13:
            /* TODO: Implement xml.xml_verb13 */
            return false;
        default:
            return false;
    }
}

boolean xmlinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pxml"), bsname);

    if (!newfunctionprocessor(bsname, &xml_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pxml_verb0"), xmlv_xml_verb0);
    ADD_VERB(BIGSTRING("\pxml_verb1"), xmlv_xml_verb1);
    ADD_VERB(BIGSTRING("\pxml_verb2"), xmlv_xml_verb2);
    ADD_VERB(BIGSTRING("\pxml_verb3"), xmlv_xml_verb3);
    ADD_VERB(BIGSTRING("\pxml_verb4"), xmlv_xml_verb4);
    ADD_VERB(BIGSTRING("\pxml_verb5"), xmlv_xml_verb5);
    ADD_VERB(BIGSTRING("\pxml_verb6"), xmlv_xml_verb6);
    ADD_VERB(BIGSTRING("\pxml_verb7"), xmlv_xml_verb7);
    ADD_VERB(BIGSTRING("\pxml_verb8"), xmlv_xml_verb8);
    ADD_VERB(BIGSTRING("\pxml_verb9"), xmlv_xml_verb9);
    ADD_VERB(BIGSTRING("\pxml_verb10"), xmlv_xml_verb10);
    ADD_VERB(BIGSTRING("\pxml_verb11"), xmlv_xml_verb11);
    ADD_VERB(BIGSTRING("\pxml_verb12"), xmlv_xml_verb12);
    ADD_VERB(BIGSTRING("\pxml_verb13"), xmlv_xml_verb13);

    #undef ADD_VERB

    pophashtable();
    return true;
}
