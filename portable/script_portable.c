#ifndef FRONTIER_PORTABLE
#define FRONTIER_PORTABLE 1
#endif

#include "frontier.h"
#include "standard.h"
#include <stdio.h>

#if defined(FRONTIER_HEADLESS)

#include "osincludes_portable.h"
#include "lang.h"
#include "langinternal.h"
#include "langexternal.h"
#include "opverbs.h"
#include "scripts.h"
#include "osacomponent.h"
#include "strings.h"
#include "logging.h"

/* Portable script compiler for headless builds.
 *
 * We reuse the legacy scriptbuildtree/scriptgetcode logic but drop any direct
 * dependencies on UI layers (window zooming, debugger panes, etc.). Scripts are
 * compiled on demand the first time they are executed, and the resulting code
 * tree is linked back into the external variable record so subsequent calls run
 * the already-compiled code.
 */

boolean scriptbuildtree (Handle htext, long signature, hdltreenode *hcode) {
    hdltreenode hstub;
    tyvaluerecord codeval;
    boolean fl;

    log_debug(LOG_COMP_STARTUP, "scriptbuildtree: enter, signature=0x%08lx (%c%c%c%c)",
            signature,
            (char)((signature >> 24) & 0xFF),
            (char)((signature >> 16) & 0xFF),
            (char)((signature >> 8) & 0xFF),
            (char)(signature & 0xFF));

    if (signature == typeLAND) {
        log_debug(LOG_COMP_STARTUP, "scriptbuildtree: LAND signature, calling langbuildtree");
        return langbuildtree (htext, true, hcode);
    }

    log_debug(LOG_COMP_STARTUP, "scriptbuildtree: calling osagetcode");
    fl = osagetcode (htext, signature, false, &codeval);
    disposehandle (htext);

    if (!fl) {
        log_debug(LOG_COMP_STARTUP, "scriptbuildtree: osagetcode failed");
        return false;
    }

    exemptfromtmpstack (&codeval);

    if (!newconstnode (codeval, &hstub)) {
        log_debug(LOG_COMP_STARTUP, "scriptbuildtree: newconstnode failed");
        return false;
    }

    (**hstub).nodetype = osascriptop;

    return pushbinaryoperation (moduleop, hstub, nil, hcode);
}

static boolean headless_scriptgetcode (hdlhashnode hnode, hdltreenode *hcode) {
    tyvaluerecord val = (**hnode).val;
    hdlexternalvariable hv;
    Handle htext;
    long signature;

    *hcode = nil;

    log_debug(LOG_COMP_STARTUP, "headless_scriptgetcode: enter");

    if (val.valuetype != externalvaluetype) {
        log_debug(LOG_COMP_STARTUP, "headless_scriptgetcode: not external value type: %d", val.valuetype);
        return false;
    }

    hv = (hdlexternalvariable) val.data.externalvalue;
    log_debug(LOG_COMP_STARTUP, "headless_scriptgetcode: hv=%p", (void *)hv);

    if ((**hv).id != idscriptprocessor) {
        log_debug(LOG_COMP_STARTUP, "headless_scriptgetcode: not script processor: id=%d", (**hv).id);
        return false;
    }

    log_debug(LOG_COMP_STARTUP, "headless_scriptgetcode: calling opverbgetlangtext");
    if (!opverbgetlangtext (hv, false, &htext, &signature)) {
        log_debug(LOG_COMP_STARTUP, "headless_scriptgetcode: opverbgetlangtext failed");
        return false;
    }

    log_debug(LOG_COMP_STARTUP, "headless_scriptgetcode: got text, size=%ld, calling scriptbuildtree", (htext ? GetHandleSize(htext) : 0));
    if (!scriptbuildtree (htext, signature, hcode)) {
        log_debug(LOG_COMP_STARTUP, "headless_scriptgetcode: scriptbuildtree failed");
        return false;
    }

    if (*hcode != nil)
        (***hcode).nodeval.data.longvalue = (long) hnode;

    log_debug(LOG_COMP_STARTUP, "headless_scriptgetcode: success, hcode=%p", (void *)*hcode);
    return true;
}

static boolean headless_scriptcompiler (hdlhashnode hnode, hdltreenode *hcode) {
    hdltreenode holdcode = nil;
    hdltreenode hnewcode = nil;
    hdlexternalvariable hv;

    log_debug(LOG_COMP_STARTUP, "headless_scriptcompiler: enter");

    if (!langexternalvaltocode ((**hnode).val, &holdcode)) {
        log_debug(LOG_COMP_STARTUP, "headless_scriptcompiler: langexternalvaltocode failed");
        return false;
    }

    log_debug(LOG_COMP_STARTUP, "headless_scriptcompiler: calling headless_scriptgetcode");
    if (!headless_scriptgetcode (hnode, &hnewcode)) {
        log_debug(LOG_COMP_STARTUP, "headless_scriptcompiler: headless_scriptgetcode failed");
        return false;
    }

    log_debug(LOG_COMP_STARTUP, "headless_scriptcompiler: linking code");
    hv = (hdlexternalvariable) (**hnode).val.data.externalvalue;
    opverblinkcode (hv, (Handle) hnewcode);

    if (holdcode != nil)
        langdisposetree (holdcode);

    *hcode = hnewcode;
    log_debug(LOG_COMP_STARTUP, "headless_scriptcompiler: success");
    return true;
}

static bigstring g_headless_error = "\x00";

static void headless_log_error(const bigstring bs) {
    if (bs == NULL)
        return;
    short len = stringlength(bs);
    if (len <= 0)
        return;
    char buffer[512];
    if (len >= (short)sizeof(buffer))
        len = (short)sizeof(buffer) - 1;
    memcpy(buffer, stringbaseaddress(bs), (size_t)len);
    buffer[len] = '\0';
    log_error(LOG_COMP_STARTUP, "headless lang error: %s", buffer);
}

static boolean headless_error_callback (bigstring bs, ptrvoid refcon) {
    (void)refcon;
    if (bs != NULL)
        copystring (bs, g_headless_error);
    headless_log_error(bs);
    return true;
}

void headless_clear_last_lang_error(void) {
    setstringlength(g_headless_error, 0);
}

const unsigned char *headless_get_last_lang_error(void) {
    return g_headless_error;
}

/*
 * headless_scripterrorroutine - Error callback for script source tracking
 *
 * This is a headless-compatible version of systemscripterrorroutine from scripts.c.
 * It handles the critical case where htable != nil, which is used by langgetthisaddress()
 * to resolve the 'this' keyword.
 *
 * When called with htable != nil:
 *   - Extracts the external variable from the hash node (scripterrorrefcon)
 *   - Uses langexternalfindvariable to populate htable and bsname
 *   - Returns true on success, allowing 'this' to be resolved
 *
 * When called with htable == nil:
 *   - Would normally show error in GUI, but in headless we just return false
 *   - Error display is handled by headless_error_callback instead
 */
static boolean headless_scripterrorroutine (long scripterrorrefcon, long lnum, short charnum, hdlhashtable *htable, bigstring bsname) {
    #pragma unused (lnum, charnum)

    register hdlhashnode h = (hdlhashnode) scripterrorrefcon;
    hdlexternalvariable hv;

    if (h == nil || *h == nil) /*defensive driving*/
        return false;

    if (htable != nil) { /*caller wants table, name - this is the 'this' resolution path*/

        if ((**h).val.valuetype != externalvaluetype)
            return false;

        hv = (hdlexternalvariable) (**h).val.data.externalvalue;

        return langexternalfindvariable (hv, htable, bsname);
    }

    /* htable == nil means we should display error - but in headless mode, just return false */
    return false;
}


/*
 * headless_pushsourcecode - Push source tracking for script execution
 *
 * This enables the 'this' keyword by pushing an error callback that can
 * resolve the current script's address. Without this, 'this' returns
 * "hasn't been defined" in headless mode.
 *
 * Matches the signature of langcallbacks.pushsourcecodecallback.
 */
static boolean headless_pushsourcecode (hdlhashtable htable, hdlhashnode hnode, bigstring bsname) {
    #pragma unused (bsname)

    register hdlhashnode h = hnode;

    /* Handle local handlers - same logic as scriptpushsourcecode in scripts.c */
    if (h != nil) {

        if (((**h).val.valuetype == codevaluetype) && (**htable).fllocaltable) { /*a local handler*/

            register hdltreenode hmodule = (**h).val.data.codevalue;

            h = (hdlhashnode) (**hmodule).nodeval.data.longvalue;
        }
    }

    /* Skip debugger source record setup - not needed in headless mode */

    return langpusherrorcallback (&headless_scripterrorroutine, (long) h);
}


/*
 * headless_popsourcecode - Pop source tracking after script execution
 *
 * Matches the signature of langcallbacks.popsourcecodecallback.
 */
static boolean headless_popsourcecode (void) {

    return langpoperrorcallback ();
}


void headless_init_script_compiler(void) {
    langcallbacks.scriptcompilecallback = &headless_scriptcompiler;
    langcallbacks.errormessagecallback = &headless_error_callback;
    langcallbacks.debugerrormessagecallback = &headless_error_callback;

    /* Enable 'this' keyword support by setting up source code tracking callbacks */
    langcallbacks.pushsourcecodecallback = &headless_pushsourcecode;
    langcallbacks.popsourcecodecallback = &headless_popsourcecode;
}

#endif /* FRONTIER_HEADLESS */
