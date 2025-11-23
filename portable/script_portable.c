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

    if (signature == typeLAND)
        return langbuildtree (htext, true, hcode);

    fl = osagetcode (htext, signature, false, &codeval);
    disposehandle (htext);

    if (!fl)
        return false;

    exemptfromtmpstack (&codeval);

    if (!newconstnode (codeval, &hstub))
        return false;

    (**hstub).nodetype = osascriptop;

    return pushbinaryoperation (moduleop, hstub, nil, hcode);
}

static boolean headless_scriptgetcode (hdlhashnode hnode, hdltreenode *hcode) {
    tyvaluerecord val = (**hnode).val;
    hdlexternalvariable hv;
    Handle htext;
    long signature;

    *hcode = nil;

    if (val.valuetype != externalvaluetype)
        return false;

    hv = (hdlexternalvariable) val.data.externalvalue;

    if ((**hv).id != idscriptprocessor)
        return false;

    if (!opverbgetlangtext (hv, false, &htext, &signature))
        return false;

    if (!scriptbuildtree (htext, signature, hcode))
        return false;

    if (*hcode != nil)
        (***hcode).nodeval.data.longvalue = (long) hnode;

    return true;
}

static boolean headless_scriptcompiler (hdlhashnode hnode, hdltreenode *hcode) {
    hdltreenode holdcode = nil;
    hdltreenode hnewcode = nil;
    hdlexternalvariable hv;

    if (!langexternalvaltocode ((**hnode).val, &holdcode))
        return false;

    if (!headless_scriptgetcode (hnode, &hnewcode))
        return false;

    hv = (hdlexternalvariable) (**hnode).val.data.externalvalue;
    opverblinkcode (hv, (Handle) hnewcode);

    if (holdcode != nil)
        langdisposetree (holdcode);

    *hcode = hnewcode;
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
    fprintf(stderr, "[headless] lang error: %s\n", buffer);
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

void headless_init_script_compiler(void) {
    langcallbacks.scriptcompilecallback = &headless_scriptcompiler;
    langcallbacks.errormessagecallback = &headless_error_callback;
    langcallbacks.debugerrormessagecallback = &headless_error_callback;
}

#endif /* FRONTIER_HEADLESS */
