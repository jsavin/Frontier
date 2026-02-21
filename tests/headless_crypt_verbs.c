/*
 * headless_crypt_verbs.c - Crypt processor verbs
 *
 * Wraps the real cryptfunctionvalue from langcrypt.c for headless mode.
 * This allows crypt verbs to work properly in the headless CLI.
 */

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "langcrypt.h"
#include "tablestructure.h"
#include "kernelverbdefs.h"

/* Token enum for all verbs in the crypt processor - from langcrypt.c */
enum {
    cryv_whirlpool = 0,
    cryv_hmacMD5 = 1,
    cryv_MD5 = 2,
    cryv_SHA1 = 3,
    cryv_hmacSHA1 = 4
};

/* Wrapper that delegates to the real cryptfunctionvalue from langcrypt.c */
static boolean crypt_valueproc(short token, hdltreenode hparam1,
                               tyvaluerecord *vreturned,
                               bigstring bserror) {
    return cryptfunctionvalue(token, hparam1, vreturned, bserror);
}

/* Initialize crypt verbs - delegates to langcrypt.c's cryptfunctionvalue */
boolean cryptinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(PSTRING("\005", "crypt"), bsname);

    if (!newfunctionprocessor(bsname, &crypt_valueproc, false, &htable))
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

    ADD_VERB(PSTRING("\011", "whirlpool"), cryv_whirlpool);
    ADD_VERB(PSTRING("\007", "hmacMD5"), cryv_hmacMD5);
    ADD_VERB(PSTRING("\003", "MD5"), cryv_MD5);
    ADD_VERB(PSTRING("\004", "SHA1"), cryv_SHA1);
    ADD_VERB(PSTRING("\010", "hmacSHA1"), cryv_hmacSHA1);

    #undef ADD_VERB

    pophashtable();
    return true;
}
