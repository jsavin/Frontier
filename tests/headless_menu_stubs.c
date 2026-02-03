/*
 * headless_menu_stubs.c - Menu editor stubs for headless build
 *
 * These are stub implementations for menueditor.c functions that are called
 * by menuverbs.c and menupack.c. The real menueditor.c is a GUI-only file
 * and not included in headless builds.
 *
 * The menuverb functions (menuverbgetsize, menuverbpack, etc.) are provided
 * by the real menuverbs.c which IS included in headless builds.
 */

#include "frontier.h"
#include "standard.h"

#include "menuverbs.h"  /* includes menueditor.h which includes menubar.h */
#include "op.h"
#include "opinternal.h"
#include "memory.h"
#include "db.h"
#include "db_format.h"

#ifdef FRONTIER_HEADLESS

/*
 * menueditor.c stub implementations for headless mode.
 * These provide minimal implementations for functions that menuverbs.c calls.
 */

boolean menewmenurecord (hdlmenurecord *hmenurecord) {
    /*
     * Allocate a new menu record. In headless mode we create a minimal
     * structure sufficient for data manipulation.
     */
    register hdlmenurecord hm;
    hdloutlinerecord houtline = nil;

    if (!newclearhandle (longsizeof (tymenurecord), (Handle *) hmenurecord))
        return (false);

    hm = *hmenurecord;

    (**hm).menuactiveitem = 1; /* menuoutlineitem */
    (**hm).scriptwindowrect.top = -1;
    (**hm).menuwindowrect.top = -1;

    /* Create the outline record for the menu structure */
    if (!newoutlinerecord (&houtline)) {
        disposehandle ((Handle) hm);
        return (false);
    }

    (**hm).menuoutline = houtline;
    (**houtline).outlinerefcon = (long) hm;

    return (true);
}

void medisposemenurecord (hdlmenurecord hmenurecord, boolean fldisk) {
    /*
     * Dispose a menu record. In headless mode we clean up the outline
     * and the record handle.
     */
    register hdlmenurecord hm = hmenurecord;

    if (hm == nil)
        return;

    /* Dispose the menu's outline */
    if ((**hm).menuoutline != nil)
        opdisposeoutline ((**hm).menuoutline, fldisk);

    /* Note: menubar stack (hmenustack) is GUI-only, not created in headless */

    disposehandle ((Handle) hm);
}

boolean meclearmenubar (void) {
    /* GUI-only operation - return true (no-op success) */
    return (true);
}

boolean meinstallmenubar (hdlmenurecord hmenurecord) {
    /* GUI-only operation - return false */
    (void) hmenurecord;
    return (false);
}

boolean meremovemenubar (hdlmenurecord hmenurecord) {
    /* GUI-only operation - return false */
    (void) hmenurecord;
    return (false);
}

boolean meeditmenurecord (void) {
    /* GUI-only operation - return false */
    return (false);
}

boolean rebuildmenubarlist (void) {
    /* GUI-only operation - return true (no-op success) */
    return (true);
}

boolean mezoomscriptwindow (void) {
    /* GUI-only operation - return false */
    return (false);
}

boolean mescriptwindowclosed (void) {
    /* GUI-only operation - return true */
    return (true);
}

boolean mesetglobals (void) {
    /*
     * Set global state from menudata. In headless mode we set the outline
     * globals if menudata exists.
     */
    if (menudata == nil) {
        opsetoutline (nil);
        return (false);
    }

    opsetoutline ((**menudata).menuoutline);

    outlinewindow = menuwindow;
    outlinewindowinfo = menuwindowinfo;

    return (true);
}

#ifdef fldebug
void mecheckglobals (void) {
    /* In debug builds, verify globals are consistent (minimal check in headless) */
    /* Allow nil menudata in headless mode */
}
#endif

hdldatabaserecord megetdatabase (hdlmenurecord hm) {
    /*
     * Get the database associated with a menu record.
     */
    if (hm == nil)
        return (nil);

    return ((**(hdlexternalvariable) (**hm).menurefcon).hdatabase);
}

void meactivate (boolean flactivate) {
    /* GUI-only operation - no-op */
    (void) flactivate;
}

void meupdate (void) {
    /* GUI-only operation - no-op */
}

boolean mekeystroke (void) {
    /* GUI-only operation - return false */
    return (false);
}

boolean mecut (void) {
    /* GUI-only operation - return false */
    return (false);
}

boolean mecopy (void) {
    /* GUI-only operation - return false */
    return (false);
}

boolean mepaste (void) {
    /* GUI-only operation - return false */
    return (false);
}

boolean meclear (void) {
    /* GUI-only operation - return false */
    return (false);
}

boolean meselectall (void) {
    /* GUI-only operation - return false */
    return (false);
}

boolean meadjustcursor (Point pt) {
    /* GUI-only operation - return false */
    (void) pt;
    return (false);
}

boolean mescroll (tydirection dir, boolean flpage, long amount) {
    /* GUI-only operation - return false */
    (void) dir; (void) flpage; (void) amount;
    return (false);
}

void megetscrollbarinfo (void) {
    /* GUI-only operation - no-op */
}

boolean megetundoglobals (long *globals) {
    /* GUI-only operation - return false */
    (void) globals;
    return (false);
}

boolean mesetundoglobals (long globals, boolean flundo) {
    /* GUI-only operation - return false */
    (void) globals; (void) flundo;
    return (false);
}

boolean memousedown (Point pt, tyclickflags flags) {
    /* GUI-only operation - return false */
    (void) pt; (void) flags;
    return (false);
}

boolean mecmdkeyfilter (char ch) {
    /* GUI-only operation - return false */
    (void) ch;
    return (false);
}

boolean meclose (void) {
    /* GUI-only operation - return true */
    return (true);
}

void meidle (void) {
    /* GUI-only operation - no-op */
}

boolean mesetprintinfo (void) {
    /* GUI-only operation - return false */
    return (false);
}

boolean meprint (short pagenumber) {
    /* GUI-only operation - return false */
    (void) pagenumber;
    return (false);
}

boolean mebeginprint (void) {
    /* GUI-only operation - return false */
    return (false);
}

boolean meendprint (void) {
    /* GUI-only operation - return false */
    return (false);
}

boolean megetcontentsize (long *width, long *height) {
    /* GUI-only operation - return default size */
    if (width) *width = 100;
    if (height) *height = 100;
    return (true);
}

boolean meresetwindowrects (hdlwindowinfo hw) {
    /* GUI-only operation - return false */
    (void) hw;
    return (false);
}

boolean meresize (void) {
    /* GUI-only operation - return false */
    return (false);
}

void meinit (void) {
    /* Minimal initialization for headless mode */
}

void mesetcallbacks (hdloutlinerecord houtline) {
    /* GUI callbacks not needed in headless mode */
    (void) houtline;
}

boolean mesomethingdirty (hdlmenurecord hmenurecord) {
    /* In headless mode, nothing is dirty */
    (void) hmenurecord;
    return (false);
}

boolean medisposemenubar (hdlmenubarstack hstack) {
    /* GUI-only menubar structure not created in headless */
    (void) hstack;
    return (true);
}

boolean meuserselected (hdlheadrecord hnode) {
    /* GUI-only operation - return false */
    (void) hnode;
    return (false);
}

boolean mesmashscriptwindow (void) {
    /* GUI-only operation - return false */
    return (false);
}

boolean mesearchoutline (boolean flfromtop, boolean flwrap, boolean *flzoom) {
    /* GUI-only operation - return false */
    (void) flfromtop; (void) flwrap;
    if (flzoom) *flzoom = false;
    return (false);
}

boolean menuverbsearch (void) {
    /* GUI-only operation - return false */
    return (false);
}

/*
 * Data operations that need minimal implementations for headless mode
 */

boolean meloadoutline_internal (const db_context *ctx, dbaddress adr,
                                hdloutlinerecord *houtline) {
    /*
     * Load menu outline from database. Simplified version for headless mode.
     */
    Handle hpackedoutline;
    long ixload = 0;
    boolean fl;

    *houtline = nil;

    oppushoutline (nil);

    if (adr == nildbaddress) {
        /* New empty outline */
        Rect r = {0, 0, 100, 100};
        fl = opnewrecord (r, houtline);
    }
    else {
        if (ctx != nil) {
            fl = dbrefhandle_context (ctx, adr, &hpackedoutline);
        } else {
            fl = dbrefhandle (adr, &hpackedoutline);
        }

        if (fl) {
            fl = opunpack (hpackedoutline, &ixload, houtline);
            disposehandle (hpackedoutline);
        }
    }

    oppopoutline ();

    if (!fl)
        return (false);

    if (*houtline != nil)
        opvalidate (*houtline);

    return (true);
}

boolean meloadoutline (dbaddress adr, hdloutlinerecord *houtline) {
    return meloadoutline_internal (nil, adr, houtline);
}

boolean mesaveoutline (hdloutlinerecord ho, dbaddress *adr) {
    /*
     * Save menu outline to database.
     */
    Handle hpackedoutline = nil;

    if (!oppackoutline (ho, &hpackedoutline))
        return (false);

    boolean fl = dbsavehandle (hpackedoutline, adr);

    disposehandle (hpackedoutline);

    return (fl);
}

boolean meloadscriptoutline (hdlmenurecord hm, hdlheadrecord hnode,
                             hdloutlinerecord *houtline, boolean *fljustloaded) {
    /*
     * Load script outline attached to a menu item. Simplified for headless.
     */
    (void) hm;
    (void) hnode;
    *houtline = nil;
    if (fljustloaded) *fljustloaded = false;
    return (false); /* Scripts not loaded in headless mode */
}

#endif /* FRONTIER_HEADLESS */
