/*
 * headless_script_verbs.c - Script processor verb implementations
 *
 * ⚠️  CRITICAL: DO NOT REGENERATE FROM generate_processor_stubs.py ⚠️
 *
 * This file contains 6 manually-added verbs NOT in kernelverbs.rc:
 *   - run, setcode, getsource, setsource, error, removeSource
 *
 * kernelverbs.rc only defines 13 script verbs. Regenerating from the generator
 * will LOSE the 6 manual verbs above. This file must be hand-maintained.
 *
 * Originally generated, now contains hand-written production implementations.
 *
 * Verb Implementation Status:
 *
 * PRODUCTION VERBS (have glue in Frontier.root):
 * - script.compile:      COMPLETE - Compile UserTalk source to bytecode
 * - script.error:        COMPLETE - Trigger runtime error
 * - script.removeSource: COMPLETE - Platform error stub (Mac license protection, not supported)
 *
 * NON-PRODUCTION VERBS (no glue in Frontier.root, added for testing infrastructure):
 * - script.run:          COMPLETE - Execute script (test infrastructure only)
 * - script.getsource:    COMPLETE - Get source text (test infrastructure only)
 * - script.setsource:    COMPLETE - Set source text (test infrastructure only)
 * - script.getcode:      COMPLETE - Get compiled code (test infrastructure only)
 * - script.setcode:      COMPLETE - Set compiled code (test infrastructure only)
 *
 * OTHER VERBS (from kernelverbs.rc, stubbed for future implementation):
 * - uncompile, getlanguage, setlanguage, makecomment, uncomment, iscomment,
 *   getbreakpoint, setbreakpoint, clearbreakpoint, startprofile, stopprofile
 */

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "langexternal.h"
#include "tablestructure.h"
#include "op.h"
#include "opinternal.h"
#include "opverbs.h"
#include "scripts.h"
#include "error.h"
#include "logging.h"

/* Token enum for all verbs in the script processor */
enum {
    scrv_compile = 0,
    scrv_run = 1,
    scrv_getcode = 2,
    scrv_setcode = 3,
    scrv_getsource = 4,
    scrv_setsource = 5,
    scrv_error = 6,
    scrv_removesource = 7,
    scrv_uncompile = 8,
    scrv_getlanguage = 9,
    scrv_setlanguage = 10,
    scrv_makecomment = 11,
    scrv_uncomment = 12,
    scrv_iscomment = 13,
    scrv_getbreakpoint = 14,
    scrv_setbreakpoint = 15,
    scrv_clearbreakpoint = 16,
    scrv_startprofile = 17,
    scrv_stopprofile = 18
};

/*
 * Helper: Get script external variable from parameter
 *
 * Uses langexternalgetexternalparam() to handle address parameters (@variable)
 * and verify the external variable is a script processor.
 */
static boolean script_getscriptparam(hdltreenode hparam1, short pnum, hdlexternalvariable *hv) {
    short id;

    if (!langexternalgetexternalparam(hparam1, pnum, &id, hv))
        return false;

    if (id != idscriptprocessor) {
        langerrormessage(BIGSTRING("\pnot a script object"));
        return false;
    }

    return true;
}

/*
 * script.compile(@scriptObj) - Compile UserTalk source to bytecode
 *
 * Compiles the source text in a script object and stores the compiled code.
 * Returns true on success, false on compilation error.
 */
static boolean script_compile(hdltreenode hparam1, tyvaluerecord *vreturned) {
    hdlexternalvariable hv;
    hdltreenode hcode = nil;
    Handle htext = nil;
    long signature;
    boolean fl;

    flnextparamislast = true;

    /* Get script object parameter */
    if (!script_getscriptparam(hparam1, 1, &hv))
        return false;

    /* Get the source text from the script */
    if (!opverbgetlangtext(hv, false, &htext, &signature)) {
        return setbooleanvalue(false, vreturned);
    }

    /* Compile the source text */
    fl = scriptbuildtree(htext, signature, &hcode);

    if (!fl) {
        /* Compilation failed - error already reported */
        return setbooleanvalue(false, vreturned);
    }

    /* Link the compiled code to the script variable */
    opverblinkcode(hv, hcode);

    return setbooleanvalue(true, vreturned);
}

/*
 * script.run(@scriptObj) - NOT IMPLEMENTED (deprecated)
 *
 * This verb is not part of production Frontier and should not be used.
 * Use direct script evaluation (@scriptname) or lang.callscript() instead.
 *
 * Historical note: This verb was never implemented in production Frontier.
 * It exists in test code only, and has been deprecated in favor of production patterns.
 *
 * Production execution patterns:
 * - Direct evaluation: @myscript()
 * - Parameterized: lang.callscript(@myscript, paramtable)
 * - Inline: lang.evaluate(sourcetext)
 */
static boolean script_run(hdltreenode hparam1, tyvaluerecord *vreturned) {
    flnextparamislast = true;

    langerrormessage(BIGSTRING(
        "\pscript.run is not supported. Use direct script evaluation "
        "(@scriptname) or lang.callscript(@scriptname, params) instead."
    ));

    return false;
}

/*
 * script.getsource(@scriptObj, @destVar) - Get source text from script object
 *
 * Retrieves the source text from a script and stores it in the destination variable.
 * Returns true on success.
 */
static boolean script_getsource(hdltreenode hparam1, tyvaluerecord *vreturned) {
    hdlexternalvariable hv;
    hdlhashtable htable;
    bigstring varname;
    Handle htext = nil;
    long signature;
    tyvaluerecord val;
    boolean fl;

    /* Get script object parameter */
    if (!script_getscriptparam(hparam1, 1, &hv))
        return false;

    flnextparamislast = true;

    /* Get destination variable name */
    if (!getvarparam(hparam1, 2, &htable, varname))
        return false;

    /* Verify it's a script */

    /* Get the source text */
    fl = opverbgetlangtext(hv, false, &htext, &signature);

    if (!fl || htext == nil) {
        return setbooleanvalue(false, vreturned);
    }

    /* Create string value and assign to destination */
    setheapvalue(htext, stringvaluetype, &val);

    if (!hashtableassign(htable, varname, val)) {
        disposehandle(htext);
        return false;
    }

    return setbooleanvalue(true, vreturned);
}

/*
 * script.setsource(@scriptObj, @sourceVar) - Set source text in script object
 *
 * Sets the source text of a script object from a string variable.
 * Clears any existing compiled code.
 * Returns true on success.
 */
static boolean script_setsource(hdltreenode hparam1, tyvaluerecord *vreturned) {
    hdlexternalvariable hv;
    bigstring bssource;
    Handle hsourcetext = nil;
    hdloutlinerecord ho = nil;
    hdlheadrecord hsummit;
    boolean fl;

    /* Get script object parameter */
    if (!script_getscriptparam(hparam1, 1, &hv))
        return false;

    flnextparamislast = true;

    /* Get source string directly (accepts direct string values) */
    if (!getstringvalue(hparam1, 2, bssource))
        return false;

    /* Convert bigstring to handle for storage in outline */
    if (!newtexthandle(bssource, &hsourcetext))
        return false;

    /* Ensure script is in memory */
    if (!opverbinmemory(NULL, hv)) {
        disposehandle(hsourcetext);
        return false;
    }

    /* Validate hv before dereferencing */
    if (hv == nil) {
        disposehandle(hsourcetext);
        langerrormessage(BIGSTRING("\pscript variable is nil"));
        return false;
    }

    ho = (hdloutlinerecord)((**hv).variabledata);

    /* Check if outline is valid */
    if (ho == nil || (**ho).hsummit == nil) {
        disposehandle(hsourcetext);
        langerrormessage(BIGSTRING("\pscript outline not properly initialized"));
        return false;
    }

    /* Clear existing compiled code */
    opverblinkcode(hv, nil);

    /* Set new source in outline structure */
    /* In Frontier, scripts are stored as outlines with text in the summit headline */
    oppushoutline(ho);

    hsummit = (**ho).hsummit;

    /* Replace summit text with new source */
    if ((**hsummit).headstring != nil)
        disposehandle((**hsummit).headstring);

    (**hsummit).headstring = hsourcetext;

    (**ho).fldirty = true;

    fl = true;

    oppopoutline();

    return setbooleanvalue(fl, vreturned);
}

/*
 * script.getcode(@scriptObj, @destVar) - Get compiled bytecode
 *
 * Retrieves the compiled code from a script and stores it as binary data.
 * Returns true on success, false if script is not compiled.
 */
static boolean script_getcode(hdltreenode hparam1, tyvaluerecord *vreturned) {
    hdlexternalvariable hv;
    hdlhashtable htable;
    bigstring varname;
    hdltreenode hcode = nil;
    Handle hpackedcode = nil;
    tyvaluerecord val;
    boolean fl;

    /* Get script object parameter */
    if (!script_getscriptparam(hparam1, 1, &hv))
        return false;

    flnextparamislast = true;

    /* Get destination variable name */
    if (!getvarparam(hparam1, 2, &htable, varname))
        return false;

    /* Verify it's a script */

    /* Get linked code */
    if (!opverbgetlinkedcode(hv, &hcode) || hcode == nil) {
        /* No compiled code available */
        return setbooleanvalue(false, vreturned);
    }

    /* Pack the code tree into binary format */
    fl = langpacktree(hcode, &hpackedcode);

    if (!fl || hpackedcode == nil) {
        return setbooleanvalue(false, vreturned);
    }

    /* Create binary value and assign to destination */
    setheapvalue(hpackedcode, binaryvaluetype, &val);

    if (!hashtableassign(htable, varname, val)) {
        disposehandle(hpackedcode);
        return false;
    }

    return setbooleanvalue(true, vreturned);
}

/*
 * script.setcode(@scriptObj, @codeVar) - Set compiled bytecode
 *
 * Sets the compiled code of a script from packed binary data.
 * The code must have been obtained from script.getcode.
 * Returns true on success.
 */
static boolean script_setcode(hdltreenode hparam1, tyvaluerecord *vreturned) {
    hdlexternalvariable hv;
    hdlhashtable htable;
    bigstring varname;
    tyvaluerecord codeval;
    Handle hpackedcode = nil;
    hdltreenode hcode = nil;
    boolean fl;

    /* Get script object parameter */
    if (!script_getscriptparam(hparam1, 1, &hv))
        return false;

    flnextparamislast = true;

    /* Get code variable */
    if (!getvarparam(hparam1, 2, &htable, varname))
        return false;

    /* Verify it's a script */

    /* Look up code value */
    hdlhashnode hnode;
    if (!hashtablelookup(htable, varname, &codeval, &hnode)) {
        langerrormessage(BIGSTRING("\pcode variable not found"));
        return false;
    }

    /* Verify it's binary data */
    if (codeval.valuetype != binaryvaluetype) {
        langerrormessage(BIGSTRING("\pcode must be binary type"));
        return false;
    }

    /* Copy the packed code handle */
    if (!copyhandle(codeval.data.binaryvalue, &hpackedcode))
        return false;

    /* Unpack the code tree */
    fl = langunpacktree(hpackedcode, &hcode);

    disposehandle(hpackedcode);

    if (!fl) {
        langerrormessage(BIGSTRING("\pinvalid compiled code"));
        return false;
    }

    /* Link the code to the script */
    opverblinkcode(hv, hcode);

    return setbooleanvalue(true, vreturned);
}

/*
 * script.error(message_or_code) - Trigger runtime error
 *
 * Terminates script execution with an error message or OS error code.
 * This function does not return normally - it throws a script error.
 */
static boolean script_error(hdltreenode hparam1, tyvaluerecord *vreturned) {
    tyvaluerecord val;
    bigstring bserror;

    /* Get error parameter (string or number) */
    if (!getparamvalue(hparam1, 1, &val))
        return false;

    flnextparamislast = true;

    /* Coerce to string if it's a number */
    if (val.valuetype == longvaluetype || val.valuetype == intvaluetype) {
        long errorcode = val.data.longvalue;

        /* Use OS error string if available, otherwise format the number */
        if (!getsystemerrorstring((OSErr)errorcode, bserror)) {
            /* Format error code as string */
            char buf[32];
            snprintf(buf, sizeof(buf), "Error %ld", errorcode);
            copyctopstring(buf, bserror);
        }
    }
    else if (!copyvaluerecord(val, &val)) {
        return false;
    }
    else if (!coercetostring(&val)) {
        disposevaluerecord(val, false);
        return false;
    }
    else {
        /* Get string value */
        texthandletostring(val.data.stringvalue, bserror);
        disposevaluerecord(val, false);
    }

    /* Trigger the script error - this does not return */
    langerrormessage(bserror);

    /* Should not reach here, but if error handling is disabled: */
    return false;
}

/*
 * script.removeSource(@scriptObj) - Platform-specific verb (not supported)
 *
 * In classic Frontier (Mac), this verb was used to remove source code from
 * compiled scripts as a license protection mechanism. This prevented customers
 * from inspecting proprietary script logic.
 *
 * This verb is not supported in the cross-platform Frontier runtime.
 */
static boolean script_removesource(hdltreenode hparam1, tyvaluerecord *vreturned) {
    #pragma unused(hparam1, vreturned)

    langerrormessage(BIGSTRING("\pscript.removeSource is not supported on this platform"));
    return false;
}

static boolean script_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case scrv_compile:
            return script_compile(hparam1, vreturned);

        case scrv_run:
            return script_run(hparam1, vreturned);

        case scrv_getsource:
            return script_getsource(hparam1, vreturned);

        case scrv_setsource:
            return script_setsource(hparam1, vreturned);

        case scrv_getcode:
            return script_getcode(hparam1, vreturned);

        case scrv_setcode:
            return script_setcode(hparam1, vreturned);

        case scrv_error:
            return script_error(hparam1, vreturned);

        case scrv_removesource:
            return script_removesource(hparam1, vreturned);

        /* Stubbed verbs for future implementation */
        case scrv_uncompile:
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_getlanguage:
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_setlanguage:
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_makecomment:
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_uncomment:
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_iscomment:
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_getbreakpoint:
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_setbreakpoint:
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_clearbreakpoint:
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_startprofile:
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case scrv_stopprofile:
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean scriptinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pscript"), bsname);

    if (!newfunctionprocessor(bsname, &script_valueproc, false, &htable))
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

    ADD_VERB(BIGSTRING("\pcompile"), scrv_compile);
    ADD_VERB(BIGSTRING("\prun"), scrv_run);
    ADD_VERB(BIGSTRING("\pgetcode"), scrv_getcode);
    ADD_VERB(BIGSTRING("\psetcode"), scrv_setcode);
    ADD_VERB(BIGSTRING("\pgetsource"), scrv_getsource);
    ADD_VERB(BIGSTRING("\psetsource"), scrv_setsource);
    ADD_VERB(BIGSTRING("\perror"), scrv_error);
    ADD_VERB(BIGSTRING("\premoveSource"), scrv_removesource);
    ADD_VERB(BIGSTRING("\puncompile"), scrv_uncompile);
    ADD_VERB(BIGSTRING("\pgetlanguage"), scrv_getlanguage);
    ADD_VERB(BIGSTRING("\psetlanguage"), scrv_setlanguage);
    ADD_VERB(BIGSTRING("\pmakecomment"), scrv_makecomment);
    ADD_VERB(BIGSTRING("\puncomment"), scrv_uncomment);
    ADD_VERB(BIGSTRING("\piscomment"), scrv_iscomment);
    ADD_VERB(BIGSTRING("\pgetbreakpoint"), scrv_getbreakpoint);
    ADD_VERB(BIGSTRING("\psetbreakpoint"), scrv_setbreakpoint);
    ADD_VERB(BIGSTRING("\pclearbreakpoint"), scrv_clearbreakpoint);
    ADD_VERB(BIGSTRING("\pstartprofile"), scrv_startprofile);
    ADD_VERB(BIGSTRING("\pstopprofile"), scrv_stopprofile);

    #undef ADD_VERB

    pophashtable();
    return true;
}
