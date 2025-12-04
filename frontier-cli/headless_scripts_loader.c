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
    fprintf(stderr, "[headless] compile_script: compiling '%s'\n", stringbaseaddress(bsname));

    if (val.valuetype != externalvaluetype) {
        fprintf(stderr, "[headless] compile_script: '%s' not external (type=%d)\n",
                stringbaseaddress(bsname), val.valuetype);
        return false;
    }

    hv = (hdlexternalvariable) val.data.externalvalue;
    if ((**hv).id != idscriptprocessor) {
        fprintf(stderr, "[headless] compile_script: '%s' not script (id=%d)\n",
                stringbaseaddress(bsname), (**hv).id);
        return false;
    }

    fprintf(stderr, "[headless] compile_script: '%s' getting langtext\n", stringbaseaddress(bsname));
    if (!opverbgetlangtext (hv, false, &htext, &signature)) {
        fprintf(stderr, "[headless] compile_script: '%s' opverbgetlangtext failed\n",
                stringbaseaddress(bsname));
        return false;
    }

    fprintf(stderr, "[headless] compile_script: '%s' building tree (textsize=%ld)\n",
            stringbaseaddress(bsname), htext ? gethandlesize(htext) : 0);
    if (!scriptbuildtree (htext, signature, hcode)) {
        fprintf(stderr, "[headless] compile_script: '%s' scriptbuildtree failed\n",
                stringbaseaddress(bsname));
        return false;
    }

    if (*hcode != nil)
        (***hcode).nodeval.data.longvalue = (long) hnode;

    fprintf(stderr, "[headless] compile_script: '%s' compiled successfully\n", stringbaseaddress(bsname));
    return true;
}

static boolean headless_execute_script (hdlhashnode hnode) {
    hdltreenode hcode = nil;
    tyvaluerecord result;
    bigstring bsname;
    boolean ok = true;

    gethashkey(hnode, bsname);

    setnilvalue (&result);

    fprintf(stderr, "[headless] execute_script: processing '%s'\n", stringbaseaddress(bsname));

    if (!headless_compile_script (hnode, &hcode)) {
        fprintf(stderr, "[headless] execute_script: '%s' skipped (not a script)\n", stringbaseaddress(bsname));
        return true; /* skip non-script nodes */
    }

    fprintf(stderr, "[headless] execute_script: '%s' executing script with langruncode\n", stringbaseaddress(bsname));

    ok = langruncode (hcode, NULL, &result);

    fprintf(stderr, "[headless] execute_script: '%s' langruncode returned %d\n",
            stringbaseaddress(bsname), ok);

    if (ok) {
        fprintf(stderr, "[headless] execute_script: '%s' completed successfully\n", stringbaseaddress(bsname));
        disposetmpvalue (&result);
    } else {
        fprintf(stderr, "[headless] execute_script: '%s' FAILED\n", stringbaseaddress(bsname));
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

    fprintf(stderr, "[headless] run_special_scripts: looking for table '%s'\n", stringbaseaddress(bstemp));

    if (!findnamedtable (systemtable, bstemp, &htable)) {
        fprintf(stderr, "[headless] run_special_scripts: table '%s' not found (nothing to do)\n",
                stringbaseaddress(bstemp));
        return true; /* nothing to do */
    }

    hashcountitems(htable, &ctitems);
    fprintf(stderr, "[headless] run_special_scripts: table '%s' has %ld items, visiting\n",
            stringbaseaddress(bstemp), ctitems);

    boolean result = hashtablevisit (htable, &headless_run_script_visit, nil);

    fprintf(stderr, "[headless] run_special_scripts: table '%s' visit completed, result=%d\n",
            stringbaseaddress(bstemp), result);

    return result;
}

boolean loadsystemscripts (void) {
    const char *skip_startup = getenv("FRONTIER_HEADLESS_SKIP_STARTUP");

    if (systemtable == nil) {
        fprintf(stderr, "[headless] loadsystemscripts: system table is nil\n");
        return false;
    }

    if (skip_startup && *skip_startup) {
        fprintf(stderr, "[headless] loadsystemscripts: skipping startup/agents per FRONTIER_HEADLESS_SKIP_STARTUP\n");
        return true;
    }

    fprintf(stderr, "[headless] loadsystemscripts: running system.startup\n");
    if (!headless_run_special_scripts (namestartuptable)) {
        fprintf(stderr, "[headless] loadsystemscripts: failed to run system.startup\n");
        return false;
    }

    fprintf(stderr, "[headless] loadsystemscripts: skipping system.agents (not yet supported)\n");
    return true;
}
