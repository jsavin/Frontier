/*
 * headless_wp_verbs.c - Word processor verbs for headless mode
 *
 * In headless mode, wp verbs operate on portable state rather than the
 * Paige word processor engine. Most formatting verbs are no-ops that
 * return true so scripts don't fail.
 *
 * Key implementations:
 *   - wp.getText: returns plaintext from targeted WPText object
 *   - wp.setText: sets plaintext on targeted WPText object
 *   - wp.intextmode: returns false (not in text edit mode)
 *   - wp.getselect/setselect: use thread-local selection state (GIL-safe)
 *   - All formatting verbs: no-op returning true
 */

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "langexternal.h"
#include "processinternal.h"  /* wp_sel_start/wp_sel_end macros (thread-local via GIL) */
#include "tablestructure.h"
#include "wpverbs.h"
#include "wptext_portable.h"
#include "logging.h"

/* Token enum matching GUI wp verbs (from wpverbs.c) */
enum {
    wpv_intextmode = 0,
    wpv_settextmode = 1,
    wpv_gettext = 2,
    wpv_settext = 3,
    wpv_getseltext = 4,
    wpv_getdisplay = 5,
    wpv_setdisplay = 6,
    wpv_getruler = 7,
    wpv_setruler = 8,
    wpv_getindent = 9,
    wpv_setindent = 10,
    wpv_getleftmargin = 11,
    wpv_setleftmargin = 12,
    wpv_getrightmargin = 13,
    wpv_setrightmargin = 14,
    wpv_setspacing = 15,
    wpv_setjustification = 16,
    wpv_settab = 17,
    wpv_cleartabs = 18,
    wpv_getselection = 19,
    wpv_setselection = 20,
    wpv_insert = 21,
    wpv_rulerlength = 22,
    wpv_go = 23,
    wpv_selectword = 24,
    wpv_selectline = 25,
    wpv_selectparagraph = 26
};

static void seterrorstring(const char *msg, bigstring bserror) {
    if (bserror == NULL)
        return;
    copyctopstring(msg, bserror);
}

/*
 * wp_resolve_target - Resolve the current target to a WPText external variable.
 *
 * Returns the hdlexternalvariable if the target is a WPText object, or NULL
 * with a descriptive error in bserror on failure.
 */
static hdlexternalvariable wp_resolve_target(bigstring bserror) {
    hdlhashtable htable;
    bigstring bsname;
    tyvaluerecord val;
    hdlhashnode hnode;
    hdlexternalvariable hv;

    if (!langgettarget(&htable, bsname)) {
        seterrorstring("no wp target set", bserror);
        return NULL;
    }

    if (!hashtablelookup(htable, bsname, &val, &hnode)) {
        seterrorstring("wp target variable not found", bserror);
        return NULL;
    }

    if (val.valuetype != externalvaluetype) {
        seterrorstring("target is not a wp text object", bserror);
        return NULL;
    }

    hv = (hdlexternalvariable)val.data.externalvalue;

    if (hv == NULL) {
        seterrorstring("target is not a wp text object", bserror);
        return NULL;
    }

    if ((**hv).id != idwordprocessor) {
        seterrorstring("target is not a wp text object", bserror);
        return NULL;
    }

    return hv;
}

static boolean wp_valueproc(short token, hdltreenode hparam1,
                                   tyvaluerecord *vreturned,
                                   bigstring bserror) {

    switch (token) {
        case wpv_intextmode:
            /* wp.inTextMode() - not in text edit mode in headless */
            return setbooleanvalue(false, vreturned);

        case wpv_settextmode:
            /* wp.setTextMode(fl) - no-op in headless mode */
            return setbooleanvalue(true, vreturned);

        case wpv_gettext: {
            /* wp.getText() - return plaintext from targeted WPText object */
            hdlexternalvariable hv;
            Handle htext;

            if (!langcheckparamcount(hparam1, 0))
                return false;

            hv = wp_resolve_target(bserror);
            if (hv == NULL)
                return false;

            if (!newemptyhandle(&htext))
                return false;

            if (!wpverbpacktotext(hv, htext)) {
                disposehandle(htext);
                seterrorstring("failed to extract text from wp object", bserror);
                return false;
            }

            return setheapvalue(htext, stringvaluetype, vreturned);
        }

        case wpv_settext: {
            /* wp.setText(s) - set plaintext on targeted WPText object */
            hdlexternalvariable hv;
            Handle hutf8;

            flnextparamislast = true;

            if (!getexempttextvalue(hparam1, 1, &hutf8))
                return false;

            hv = wp_resolve_target(bserror);
            if (hv == NULL) {
                disposehandle(hutf8);
                return false;
            }

            if (!wp_portable_set_plaintext(hv, hutf8)) {
                disposehandle(hutf8);
                seterrorstring("failed to set text on wp object", bserror);
                return false;
            }

            disposehandle(hutf8);

            return setbooleanvalue(true, vreturned);
        }

        case wpv_getseltext:
            /* wp.getSelText() - return empty string */
            return setstringvalue(BIGSTRING("\p"), vreturned);

        case wpv_getdisplay:
            /* wp.getDisplay() - return false (no display in headless) */
            return setbooleanvalue(false, vreturned);

        case wpv_setdisplay:
            /* wp.setDisplay(fl) - no-op */
            return setbooleanvalue(true, vreturned);

        case wpv_getruler:
            /* wp.getRuler() - return false (no ruler in headless) */
            return setbooleanvalue(false, vreturned);

        case wpv_setruler:
            /* wp.setRuler(fl) - no-op */
            return setbooleanvalue(true, vreturned);

        case wpv_getindent:
            /* wp.getIndent() - return 0 */
            return setlongvalue(0, vreturned);

        case wpv_setindent:
            /* wp.setIndent(pixels) - no-op */
            return setbooleanvalue(true, vreturned);

        case wpv_getleftmargin:
            /* wp.getLeftMargin() - return 0 */
            return setlongvalue(0, vreturned);

        case wpv_setleftmargin:
            /* wp.setLeftMargin(pixels) - no-op */
            return setbooleanvalue(true, vreturned);

        case wpv_getrightmargin:
            /* wp.getRightMargin() - return 0 */
            return setlongvalue(0, vreturned);

        case wpv_setrightmargin:
            /* wp.setRightMargin(pixels) - no-op */
            return setbooleanvalue(true, vreturned);

        case wpv_setspacing:
            /* wp.setSpacing(spacing) - no-op */
            return setbooleanvalue(true, vreturned);

        case wpv_setjustification:
            /* wp.setJustification(justification) - no-op */
            return setbooleanvalue(true, vreturned);

        case wpv_settab:
            /* wp.setTab(pos) - no-op */
            return setbooleanvalue(true, vreturned);

        case wpv_cleartabs:
            /* wp.clearTabs() - no-op */
            return setbooleanvalue(true, vreturned);

        case wpv_getselection: {
            /* wp.getSelect(startAddr, endAddr) - return persisted selection */
            if (!langsetlongvarparam(hparam1, 1, wp_sel_start))
                return false;

            flnextparamislast = true;

            if (!langsetlongvarparam(hparam1, 2, wp_sel_end))
                return false;

            return setbooleanvalue(true, vreturned);
        }

        case wpv_setselection: {
            /* wp.setSelect(start, end) - persist selection state */
            long selstart, selend;

            if (!getlongvalue(hparam1, 1, &selstart))
                return false;

            flnextparamislast = true;

            if (!getlongvalue(hparam1, 2, &selend))
                return false;

            wp_sel_start = selstart;
            wp_sel_end = selend;

            return setbooleanvalue(true, vreturned);
        }

        case wpv_insert: {
            /* wp.insert(s) - consume text param, no-op */
            tyvaluerecord val;

            flnextparamislast = true;

            if (!getparamvalue(hparam1, 1, &val))
                return false;

            return setbooleanvalue(true, vreturned);
        }

        case wpv_rulerlength:
            /* wp.rulerLength() - return 0 */
            return setlongvalue(0, vreturned);

        case wpv_go:
            /* wp.go(dir, count) - no-op */
            return setbooleanvalue(true, vreturned);

        case wpv_selectword:
            /* wp.selectWord() - no-op */
            return setbooleanvalue(true, vreturned);

        case wpv_selectline:
            /* wp.selectLine() - no-op */
            return setbooleanvalue(true, vreturned);

        case wpv_selectparagraph:
            /* wp.selectParagraph() - no-op */
            return setbooleanvalue(true, vreturned);

        default:
            return false;
    }
}

boolean wpinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pwp"), bsname);

    if (!newfunctionprocessor(bsname, &wp_valueproc, true, &htable))
        return false;

    pushhashtable(htable);

    #define ADD_VERB(name, tok) do { \
        if (!langaddkeyword(name, tok)) { \
            pophashtable(); \
            return false; \
        } \
    } while(0)

    ADD_VERB(BIGSTRING("\pintextmode"), wpv_intextmode);
    ADD_VERB(BIGSTRING("\psettextmode"), wpv_settextmode);
    ADD_VERB(BIGSTRING("\pgettext"), wpv_gettext);
    ADD_VERB(BIGSTRING("\psettext"), wpv_settext);
    ADD_VERB(BIGSTRING("\pgetseltext"), wpv_getseltext);
    ADD_VERB(BIGSTRING("\pgetdisplay"), wpv_getdisplay);
    ADD_VERB(BIGSTRING("\psetdisplay"), wpv_setdisplay);
    ADD_VERB(BIGSTRING("\pgetruler"), wpv_getruler);
    ADD_VERB(BIGSTRING("\psetruler"), wpv_setruler);
    ADD_VERB(BIGSTRING("\pgetindent"), wpv_getindent);
    ADD_VERB(BIGSTRING("\psetindent"), wpv_setindent);
    ADD_VERB(BIGSTRING("\pgetleftmargin"), wpv_getleftmargin);
    ADD_VERB(BIGSTRING("\psetleftmargin"), wpv_setleftmargin);
    ADD_VERB(BIGSTRING("\pgetrightmargin"), wpv_getrightmargin);
    ADD_VERB(BIGSTRING("\psetrightmargin"), wpv_setrightmargin);
    ADD_VERB(BIGSTRING("\psetspacing"), wpv_setspacing);
    ADD_VERB(BIGSTRING("\psetjustification"), wpv_setjustification);
    ADD_VERB(BIGSTRING("\psettab"), wpv_settab);
    ADD_VERB(BIGSTRING("\pcleartabs"), wpv_cleartabs);
    ADD_VERB(BIGSTRING("\pgetselect"), wpv_getselection);
    ADD_VERB(BIGSTRING("\psetselect"), wpv_setselection);
    ADD_VERB(BIGSTRING("\pinsert"), wpv_insert);
    ADD_VERB(BIGSTRING("\prulerlength"), wpv_rulerlength);
    ADD_VERB(BIGSTRING("\pgo"), wpv_go);
    ADD_VERB(BIGSTRING("\pselectword"), wpv_selectword);
    ADD_VERB(BIGSTRING("\pselectline"), wpv_selectline);
    ADD_VERB(BIGSTRING("\pselectparagraph"), wpv_selectparagraph);

    #undef ADD_VERB

    pophashtable();
    return true;
}
