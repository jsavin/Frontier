/*
 * headless_db_verbs.c - Db processor verbs for headless mode
 *
 * This file provides the callback dispatcher for db verbs in headless mode,
 * routing verb calls to the actual implementations in dbverbs.c.
 *
 * @IMPLEMENTED - All 13 db verbs forward to dbfunctionvalue() in dbverbs.c
 *
 * Created: 2026-01-08 - DB verb binding (forwarding to real implementation)
 */

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"
#include "logging.h"
#include "odbinternal.h"
#include "file.h"

/* Forward declarations from dbverbs.c */
extern boolean dbfunctionvalue(short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror);

/* ODB list structure and global from dbverbs.c */
typedef struct tyodblistrecord {
    struct tyodblistrecord **hnext;
    tyfilespec fs;
    hdlfilenum fref;
    boolean flreadonly;
    odbref odb;
} tyodbrecord, *ptrodbrecord, **hdlodbrecord;

extern hdlodbrecord hodblist;

/* Token enum for all verbs in the db processor
 *
 * CRITICAL: This enum MUST match tydbtoken in Common/source/dbverbs.c
 *
 * Verification:
 *   1. Token order must match exactly (0=newfunc, 1=openfunc, etc.)
 *   2. Token count must match ctdbverbs value (13 verbs)
 *
 * Note: We use different names (dbv_*) to avoid conflicts, but values must match.
 */
enum {
    dbv_new = 0,        // newfunc in dbverbs.c
    dbv_open = 1,       // openfunc
    dbv_save = 2,       // savefunc
    dbv_close = 3,      // closefunc
    dbv_defined = 4,    // definedfunc
    dbv_getvalue = 5,   // getvaluefunc
    dbv_setvalue = 6,   // setvaluefunc
    dbv_delete = 7,     // deletefunc
    dbv_newTable = 8,   // newtablefunc
    dbv_isTable = 9,    // istablefunc
    dbv_countitems = 10, // countitemsfunc
    dbv_getnthitem = 11, // getnthitemfunc
    dbv_getmoddate = 12  // getmoddatefunc
};

/*
 * Headless callback dispatcher for db verbs.
 *
 * This function receives all db.* verb calls in headless mode and forwards them
 * to the real implementation (dbfunctionvalue) in dbverbs.c.
 *
 * The forwarding is straightforward since our token enum matches the one in dbverbs.c.
 */
static boolean headless_db_verbs_callback(short token, hdltreenode hparam1,
                                          tyvaluerecord *vreturned,
                                          bigstring bserror) {
    /* Forward directly to the real implementation */
    return dbfunctionvalue(token, hparam1, vreturned, bserror);
}

/*
 * dbinitverbs - Initialize db verb processor for headless mode
 *
 * This replaces dbinitverbs() from dbverbs.c for headless mode, registering
 * the db processor with headless_db_verbs_callback instead of using
 * loadfunctionprocessor() (which is a no-op in headless builds).
 *
 * Follows the same pattern as other headless processor init functions.
 *
 * Returns: true if db processor was successfully registered, false otherwise
 */
boolean dbinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    extern boolean newfunctionprocessor(bigstring, langvaluecallback, boolean, hdlhashtable*);
    extern boolean langaddkeyword(bigstring, short);
    extern boolean pushhashtable(hdlhashtable);
    extern boolean pophashtable(void);
    extern boolean newclearhandle(long, Handle*);

    log_debug(LOG_COMP_LANG, "dbinitverbs: registering headless db processor");

    copystring(BIGSTRING("\002db"), bsname);

    /* Initialize ODB list (required by db verb implementation) */
    if (!newclearhandle(sizeof(tyodbrecord), (Handle*)&hodblist))
        return false;

    if (!newfunctionprocessor(bsname, &headless_db_verbs_callback, false, &htable))
        return false;

    pushhashtable(htable);

    /* Register all db verbs */
    #define ADD_VERB(name, tok) do { \
        bigstring bs; \
        copystring(name, bs); \
        if (!langaddkeyword(bs, tok)) { \
            pophashtable(); \
            return false; \
        } \
    } while(0)

    ADD_VERB(BIGSTRING("\003new"), dbv_new);
    ADD_VERB(BIGSTRING("\004open"), dbv_open);
    ADD_VERB(BIGSTRING("\004save"), dbv_save);
    ADD_VERB(BIGSTRING("\005close"), dbv_close);
    ADD_VERB(BIGSTRING("\007defined"), dbv_defined);
    ADD_VERB(BIGSTRING("\010getvalue"), dbv_getvalue);
    ADD_VERB(BIGSTRING("\010setvalue"), dbv_setvalue);
    ADD_VERB(BIGSTRING("\006delete"), dbv_delete);
    ADD_VERB(BIGSTRING("\010newtable"), dbv_newTable);
    ADD_VERB(BIGSTRING("\007istable"), dbv_isTable);
    ADD_VERB(BIGSTRING("\012countitems"), dbv_countitems);
    ADD_VERB(BIGSTRING("\012getnthitem"), dbv_getnthitem);
    ADD_VERB(BIGSTRING("\012getmoddate"), dbv_getmoddate);

    #undef ADD_VERB

    pophashtable();

    log_debug(LOG_COMP_LANG, "dbinitverbs: db processor registered successfully");

    return true;
}
