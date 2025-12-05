#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"

/* Token enum for all verbs in the speaker processor */
enum {
    spev_speaker_verb0 = 0,
    spev_speaker_verb1 = 1,
    spev_speaker_verb2 = 2
};

static boolean speaker_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case spev_speaker_verb0:
            /* TODO: Implement speaker.speaker_verb0 */
            return false;
        case spev_speaker_verb1:
            /* TODO: Implement speaker.speaker_verb1 */
            return false;
        case spev_speaker_verb2:
            /* TODO: Implement speaker.speaker_verb2 */
            return false;
        default:
            return false;
    }
}

boolean speakerinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pspeaker"), bsname);

    if (!newfunctionprocessor(bsname, &speaker_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pspeaker_verb0"), spev_speaker_verb0);
    ADD_VERB(BIGSTRING("\pspeaker_verb1"), spev_speaker_verb1);
    ADD_VERB(BIGSTRING("\pspeaker_verb2"), spev_speaker_verb2);

    #undef ADD_VERB

    pophashtable();
    return true;
}
