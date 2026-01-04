/* 2025-12-02 Codex: Allow headless startup scripts to be skipped for debugging (env flag). */

#include "../Common/headers/frontier.h"

#include "../Common/headers/lang.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/langinternal.h"
#include "../Common/headers/opverbs.h"
#include "../Common/headers/scripts.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/tableverbs.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/stringdefs.h"
#include "../Common/headers/logging.h"

#include <stdio.h>

extern boolean scriptbuildtree (Handle htext, long signature, hdltreenode *hcode);
extern boolean langruncode (hdltreenode htree, hdlhashtable hcontext, tyvaluerecord *vreturned);
extern boolean pushhashtable (hdlhashtable);
extern boolean pophashtable (void);
extern hdlhashtable currenthashtable;
extern hdlhashtable roottable;

static boolean headless_compile_script (hdlhashnode hnode, hdltreenode *hcode) {
    tyvaluerecord val = (**hnode).val;
    hdlexternalvariable hv;
    Handle htext;
    long signature = 0;
    bigstring bsname;

    *hcode = nil;

    gethashkey(hnode, bsname);
    log_debug(LOG_COMP_STARTUP, "compile_script: compiling '%s'", stringbaseaddress(bsname));

    if (val.valuetype != externalvaluetype) {
        log_debug(LOG_COMP_STARTUP, "compile_script: '%s' not external (type=%d)",
                stringbaseaddress(bsname), val.valuetype);
        return false;
    }

    hv = (hdlexternalvariable) val.data.externalvalue;
    if ((**hv).id != idscriptprocessor) {
        log_debug(LOG_COMP_STARTUP, "compile_script: '%s' not script (id=%d)",
                stringbaseaddress(bsname), (**hv).id);
        return false;
    }

    log_debug(LOG_COMP_STARTUP, "compile_script: '%s' getting langtext", stringbaseaddress(bsname));
    if (!opverbgetlangtext (hv, false, &htext, &signature)) {
        log_debug(LOG_COMP_STARTUP, "compile_script: '%s' opverbgetlangtext failed",
                stringbaseaddress(bsname));
        return false;
    }

    log_debug(LOG_COMP_STARTUP, "compile_script: '%s' building tree (textsize=%ld)",
            stringbaseaddress(bsname), htext ? gethandlesize(htext) : 0);
    if (!scriptbuildtree (htext, signature, hcode)) {
        log_debug(LOG_COMP_STARTUP, "compile_script: '%s' scriptbuildtree failed",
                stringbaseaddress(bsname));
        return false;
    }

    if (*hcode != nil)
        (***hcode).nodeval.data.longvalue = (long) hnode;

    log_debug(LOG_COMP_STARTUP, "compile_script: '%s' compiled successfully", stringbaseaddress(bsname));
    return true;
}

static boolean headless_execute_script (hdlhashnode hnode) {
    hdltreenode hcode = nil;
    tyvaluerecord result;
    bigstring bsname;
    boolean ok = true;

    gethashkey(hnode, bsname);

    setnilvalue (&result);

    log_debug(LOG_COMP_STARTUP, "execute_script: processing '%s'", stringbaseaddress(bsname));

    if (!headless_compile_script (hnode, &hcode)) {
        log_debug(LOG_COMP_STARTUP, "execute_script: '%s' skipped (not a script)", stringbaseaddress(bsname));
        return true; /* skip non-script nodes */
    }

    log_debug(LOG_COMP_STARTUP, "execute_script: '%s' executing script with langruncode", stringbaseaddress(bsname));

    ok = langruncode (hcode, NULL, &result);

    log_debug(LOG_COMP_STARTUP, "execute_script: '%s' langruncode returned %d",
            stringbaseaddress(bsname), ok);

    if (ok) {
        log_debug(LOG_COMP_STARTUP, "execute_script: '%s' completed successfully", stringbaseaddress(bsname));
        disposetmpvalue (&result);
    } else {
        log_debug(LOG_COMP_STARTUP, "execute_script: '%s' FAILED", stringbaseaddress(bsname));
    }

    return ok;
}

static boolean headless_run_script_visit (hdlhashnode hnode, ptrvoid refcon) {
#pragma unused(refcon)
    return headless_execute_script (hnode);
}

static boolean headless_run_special_scripts (const unsigned char *bsspecialtable) {
    hdlhashtable htable;
    bigstring bstemp;
    long ctitems = 0;

    copystring (bsspecialtable, bstemp);

    log_debug(LOG_COMP_STARTUP, "run_special_scripts: looking for table '%s'", stringbaseaddress(bstemp));

    if (!findnamedtable (systemtable, bstemp, &htable)) {
        log_debug(LOG_COMP_STARTUP, "run_special_scripts: table '%s' not found (nothing to do)",
                stringbaseaddress(bstemp));
        return true; /* nothing to do */
    }

    hashcountitems(htable, &ctitems);
    log_debug(LOG_COMP_STARTUP, "run_special_scripts: table '%s' has %ld items, visiting",
            stringbaseaddress(bstemp), ctitems);

    boolean result = hashtablevisit (htable, &headless_run_script_visit, nil);

    log_debug(LOG_COMP_STARTUP, "run_special_scripts: table '%s' visit completed, result=%d",
            stringbaseaddress(bstemp), result);

    return result;
}

boolean loadsystemscripts (void) {
    const char *run_startup = getenv("FRONTIER_HEADLESS_RUN_STARTUP");

    if (systemtable == nil) {
        log_error(LOG_COMP_STARTUP, "loadsystemscripts: system table is nil");
        return false;
    }

    /* Default: skip startup scripts (development/testing mode)
     * Set FRONTIER_HEADLESS_RUN_STARTUP=1 to run system.startup scripts */
    if (!run_startup || !*run_startup) {
        log_debug(LOG_COMP_STARTUP, "loadsystemscripts: skipping startup/agents (default behavior)");
        return true;
    }

    log_debug(LOG_COMP_STARTUP, "loadsystemscripts: running system.startup per FRONTIER_HEADLESS_RUN_STARTUP");
    if (!headless_run_special_scripts (namestartuptable)) {
        log_error(LOG_COMP_STARTUP, "loadsystemscripts: failed to run system.startup");
        return false;
    }

    log_debug(LOG_COMP_STARTUP, "loadsystemscripts: skipping system.agents (not yet supported)");
    return true;
}
