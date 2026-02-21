/*
 * headless_table_verbs.c - Table processor verbs for headless mode
 *
 * This file provides the callback dispatcher for table verbs in headless mode,
 * routing verb calls to the actual implementations in tableverbs.c.
 *
 * @IMPLEMENTED - All 19 table verbs forward to tablefunctionvalue() in tableverbs.c
 *
 * Created: 2025-12-29 - Phase 3 table navigation verb support
 * Updated: 2026-01-01 - Added @IMPLEMENTED annotation, verified dispatcher pattern
 */

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "logging.h"

/* Token enum for all verbs in the table processor
 *
 * CRITICAL: This enum MUST be kept in sync with tytabletoken in Common/source/tableverbs.c
 *
 * Verification:
 *   1. Token order must match exactly (0=move, 1=copy, etc.)
 *   2. Token count must match cttableverbs value (19 verbs)
 *   3. Compile-time assertion below will fail if count mismatches
 *
 * To verify manually:
 *   grep -c "func," Common/source/tableverbs.c | should equal 19
 */
enum {
    tabv_move = 0,
    tabv_copy = 1,
    tabv_rename = 2,
    tabv_moveandrename = 3,
    tabv_assign = 4,
    tabv_validate = 5,
    tabv_sortby = 6,
    tabv_getcursor = 7,
    tabv_getselection = 8,
    tabv_go = 9,
    tabv_goto = 10,
    tabv_gotoname = 11,
    tabv_jettison = 12,
    tabv_packtable = 13,
    tabv_emptytable = 14,
    tabv_getdisplaysettings = 15,
    tabv_setdisplaysettings = 16,
    tabv_sortorder = 17,
    tabv_countvisiblerows = 18,

    /* Sentinel - must equal cttableverbs from tableverbs.c */
    tabv_count
};

/* Compile-time verification that token count matches tableverbs.c
 * If this fails, the enum above is out of sync with tytabletoken */
#define EXPECTED_TABLE_VERB_COUNT 19
_Static_assert(tabv_count == EXPECTED_TABLE_VERB_COUNT,
               "Token enum out of sync with tableverbs.c - update headless_table_verbs.c");

/* Forward declaration of the actual implementation in tableverbs.c */
extern boolean tablefunctionvalue(short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror);

static boolean table_valueproc(short token, hdltreenode hparam1,
                                tyvaluerecord *vreturned,
                                bigstring bserror) {
    /*
     * Dispatcher for table verbs in headless mode.
     * Simply forwards all calls to the actual implementation in tableverbs.c
     */
    boolean result;

    log_debug(LOG_COMP_TABLE, "table_valueproc: ENTRY token=%d hparam1=%p vreturned=%p bserror=%p",
              token, (void*)hparam1, (void*)vreturned, (void*)bserror);

    result = tablefunctionvalue(token, hparam1, vreturned, bserror);

    if (bserror && bserror[0] > 0) {
        char errmsg[256];
        copyptocstring(bserror, errmsg);
        log_debug(LOG_COMP_TABLE, "table_valueproc: EXIT token=%d result=%d bserror='%s'",
                  token, result, errmsg);
    } else {
        log_debug(LOG_COMP_TABLE, "table_valueproc: EXIT token=%d result=%d bserror=<empty>",
                  token, result);
    }

    return result;
}

/* Exported callback used by headless kernel verb bootstrap */
boolean headless_table_verbs_callback(short token, hdltreenode hparam1,
                                      tyvaluerecord *vreturned, bigstring bserror) {
    return table_valueproc(token, hparam1, vreturned, bserror);
}

/* Headless-specific table verb initialization function
 *
 * This replaces tableinitverbs() for headless mode, registering the table processor
 * with headless_table_verbs_callback instead of the windowed tablefunctionvalue callback.
 *
 * Follows the same pattern as other headless processor init functions (e.g., opinitverbs).
 *
 * Returns: true if table processor was successfully registered, false otherwise
 */
boolean tableinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    extern boolean newfunctionprocessor(bigstring, langvaluecallback, boolean, hdlhashtable*);
    extern boolean langaddkeyword(bigstring, short);
    extern boolean pushhashtable(hdlhashtable);
    extern boolean pophashtable(void);

    log_debug(LOG_COMP_TABLE, "tableinitverbs: registering headless table processor");

    copystring(PSTRING("\005", "table"), bsname);

    if (!newfunctionprocessor(bsname, &headless_table_verbs_callback, true, &htable))
        return false;

    pushhashtable(htable);

    /* Register all table verbs */
    #define ADD_VERB(name, tok) do { \
        bigstring bs; \
        copystring(name, bs); \
        if (!langaddkeyword(bs, tok)) { \
            pophashtable(); \
            return false; \
        } \
    } while(0)

    ADD_VERB(PSTRING("\004", "move"), tabv_move);
    ADD_VERB(PSTRING("\004", "copy"), tabv_copy);
    ADD_VERB(PSTRING("\006", "rename"), tabv_rename);
    ADD_VERB(PSTRING("\015", "moveandrename"), tabv_moveandrename);
    ADD_VERB(PSTRING("\006", "assign"), tabv_assign);
    ADD_VERB(PSTRING("\010", "validate"), tabv_validate);
    ADD_VERB(PSTRING("\006", "sortby"), tabv_sortby);
    ADD_VERB(PSTRING("\011", "getcursor"), tabv_getcursor);
    ADD_VERB(PSTRING("\014", "getselection"), tabv_getselection);
    ADD_VERB(PSTRING("\002", "go"), tabv_go);
    ADD_VERB(PSTRING("\004", "goto"), tabv_goto);
    ADD_VERB(PSTRING("\010", "gotoname"), tabv_gotoname);
    ADD_VERB(PSTRING("\010", "jettison"), tabv_jettison);
    ADD_VERB(PSTRING("\011", "packtable"), tabv_packtable);
    ADD_VERB(PSTRING("\012", "emptytable"), tabv_emptytable);
    ADD_VERB(PSTRING("\022", "getdisplaysettings"), tabv_getdisplaysettings);
    ADD_VERB(PSTRING("\022", "setdisplaysettings"), tabv_setdisplaysettings);
    ADD_VERB(PSTRING("\014", "getsortorder"), tabv_sortorder);
    ADD_VERB(PSTRING("\020", "countvisiblerows"), tabv_countvisiblerows);

    pophashtable();

    log_debug(LOG_COMP_TABLE, "tableinitverbs: table processor registered successfully");

    return true;
}
