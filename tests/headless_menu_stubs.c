/*
 * headless_menu_stubs.c - Menu stubs for headless builds
 *
 * This file provides two categories of stubs:
 *
 * 1. menueditor.c stubs (always included):
 *    Stub implementations for GUI-only menueditor.c functions that are called
 *    by menuverbs.c and menupack.c. The real menueditor.c is GUI-only and not
 *    included in headless builds.
 *
 * 2. menuverbs.c stubs (conditional via HEADLESS_LINKS_REAL_MENUVERBS):
 *    When HEADLESS_LINKS_REAL_MENUVERBS is NOT defined (e.g., save_migration_tests),
 *    stub implementations for menuverb* functions are provided. These simplified
 *    stubs handle database migration without requiring full GUI infrastructure.
 *
 *    When HEADLESS_LINKS_REAL_MENUVERBS IS defined (e.g., frontier-cli),
 *    the real menuverbs.c provides these functions with full functionality.
 */

#include "frontier.h"
#include "standard.h"

#include "menuverbs.h"  /* includes menueditor.h which includes menubar.h */
#include "menuinternal.h" /* megetmenuiteminfo, mesetmenuiteminfo */
#include "op.h"
#include "opinternal.h"
#include "memory.h"
#include "db.h"
#include "db_format.h"
#include "logging.h"  /* For meloadscriptoutline diagnostics */

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

void meresize (void) {
    /* GUI-only operation - no-op */
}

void meinit (void) {
    /* Minimal initialization for headless mode */
}

void mesetcallbacks (hdloutlinerecord houtline) {
    /* GUI callbacks not needed in headless mode */
    (void) houtline;
}

boolean mesomethingdirty (hdlmenurecord hmenurecord) {
    /* Return actual dirty state for proper save tracking */
    if (hmenurecord == nil)
        return (false);
    return ((**hmenurecord).fldirty);
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

    log_debug(LOG_COMP_OP, "meloadoutline_internal: START adr=0x%llx ctx_db=%p",
            (unsigned long long)adr, (void*)(ctx ? ctx->database : nil));

    oppushoutline (nil);

    if (adr == nildbaddress) {
        /* New empty outline - use newoutlinerecord which doesn't require window info */
        fl = newoutlinerecord (houtline);
        log_debug(LOG_COMP_OP, "meloadoutline_internal: created new outline fl=%d", (int)fl);
    }
    else {
        /* These are debug messages, not errors - menu loading in headless mode
         * is expected to be deferred or skipped entirely. Failures here are
         * normal and handled gracefully. */
        log_debug(LOG_COMP_OP, "meloadoutline_internal: reading adr=0x%llx ctx_db=%p global_db=%p",
                (unsigned long long)adr, (void*)(ctx ? ctx->database : nil), (void*)databasedata);

        if (ctx != nil) {
            fl = dbrefhandle_context (ctx, adr, &hpackedoutline);
        } else {
            fl = dbrefhandle (adr, &hpackedoutline);
        }

        if (!fl) {
            /* Database read failed - leave *houtline as nil. This is expected
             * in headless mode where menus are intentionally deferred. */
            log_debug(LOG_COMP_OP, "meloadoutline_internal: dbrefhandle FAILED adr=0x%llx", (unsigned long long)adr);
            oppopoutline ();
            return (false);
        }

        {
            long hsize = gethandlesize(hpackedoutline);
            unsigned char *p = (unsigned char *) *hpackedoutline;
            log_debug(LOG_COMP_OP, "meloadoutline_internal: dbrefhandle OK, unpacking size=%ld first_bytes=%02x%02x%02x%02x",
                    hsize, hsize > 0 ? p[0] : 0, hsize > 1 ? p[1] : 0, hsize > 2 ? p[2] : 0, hsize > 3 ? p[3] : 0);
        }

        fl = opunpack (hpackedoutline, &ixload, houtline);

        if (!fl) {
            log_debug(LOG_COMP_OP, "meloadoutline_internal: opunpack FAILED");
        }

        disposehandle (hpackedoutline);
    }

    oppopoutline ();

    if (!fl)
        return (false);

    if (*houtline != nil)
        opvalidate (*houtline);

    log_debug(LOG_COMP_OP, "meloadoutline_internal: SUCCESS");
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
     * Load script outline attached to a menu item.
     *
     * This function is called during menu packing when fldatabasesaveas and
     * flconvertingolddatabase are true (i.e., during migration). It needs to
     * load attached scripts so they can be re-saved to the destination database.
     *
     * Returns true if successful (even if no script is attached - *houtline=nil).
     * Returns false only on actual errors (e.g., database read failure).
     */
    tymenuiteminfo item;
    dbaddress adr;
    Handle hpackedoutline;
    long ixload = 0;
    boolean fl;

    (void) hm; /* Menu record not needed - script address is in node refcon */

    *houtline = nil;
    if (fljustloaded) *fljustloaded = false;

    /* Get the script info from the node's refcon */
    if (!megetmenuiteminfo (hnode, &item)) {
        /* No refcon data - no script attached, this is OK */
        log_trace(LOG_COMP_OP, "meloadscriptoutline: no refcon data, returning OK");
        return (true);
    }

    adr = item.linkedscript.adrlink;

    if (adr == nildbaddress) {
        /* No linked script - this is OK */
        log_trace(LOG_COMP_OP, "meloadscriptoutline: no linked script (adr=nil), returning OK");
        return (true);
    }

    log_trace(LOG_COMP_OP, "meloadscriptoutline: loading script from adr=0x%llx", (unsigned long long)adr);

    /* Load the packed script outline from database */
    fl = dbrefhandle (adr, &hpackedoutline);
    if (!fl) {
        /* Database read error */
        log_error(LOG_COMP_OP, "meloadscriptoutline: dbrefhandle failed for adr=0x%llx", (unsigned long long)adr);
        return (false);
    }

    /* Unpack into outline record */
    fl = opunpack (hpackedoutline, &ixload, houtline);

    if (!fl) {
        log_error(LOG_COMP_OP, "meloadscriptoutline: opunpack failed for adr=0x%llx", (unsigned long long)adr);
    }

    disposehandle (hpackedoutline);

    if (fl && fljustloaded) {
        *fljustloaded = true;
    }

    return (fl);
}

/*
 * menuverbs.c stub implementations for headless builds that DON'T link real menuverbs.c.
 *
 * When HEADLESS_LINKS_REAL_MENUVERBS is defined (e.g., frontier-cli), the real
 * menuverbs.c provides these functions. When it's NOT defined (e.g., save_migration_tests),
 * these stubs provide minimal implementations sufficient for database migration.
 */
#ifndef HEADLESS_LINKS_REAL_MENUVERBS

#include "langexternal.h"
#include "strings.h"
#include "dbinternal.h"
#include "logging.h"

boolean menuverbgetdisplaystring (hdlexternalvariable h, bigstring bs) {
    (void) h; copyctopstring("menu", bs); return true;
}

boolean menuverbgettypestring (hdlexternalvariable h, bigstring bs) {
    (void) h; copyctopstring("menu", bs); return true;
}

boolean menuverbsetdirty (hdlexternalvariable h, boolean fldirty) {
    (void) h; (void) fldirty; return false;
}

boolean menuverbmemorypack (hdlexternalvariable h, Handle *hp) {
    (void) h; if (hp) *hp = nil; return false;
}

boolean menuverbmemoryunpack (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    (void) hpacked; (void) ixload; (void) h; return false;
}

boolean menuverbpack_internal (const db_context *ctx, hdlexternalvariable h, Handle *hp, boolean *flnew) {
    /*
     * Context-aware menu packing for migration.
     * During headless migration, menu externals are preserved as database references
     * without materializing their data. We just pack the v6 address as-is.
     */
    (void) ctx;  /* Context not needed for address passthrough */

    if ((h == nil) || (hp == nil))
        return false;

    dbaddress adr = (**h).oldaddress;
    if (adr == nildbaddress)
        adr = (dbaddress) (**h).variabledata;
    if (adr == nildbaddress) {
        if (flnew)
            *flnew = false;
        return false;
    }

    /* During migration, mark as new address (will be allocated in destination) */
    if (flnew)
        *flnew = true;

    (**h).oldaddress = adr;
    return pushlongondiskhandle((long) adr, *hp);
}

boolean menuverbpack (hdlexternalvariable h, Handle *hp, boolean *flnew) {
    /* Wrapper for backward compatibility - uses global mode state */
    return menuverbpack_internal(NULL, h, hp, flnew);
}

boolean menuverbunpack (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    long rawadr = 0;
    if (!loadlongfromdiskhandle(hpacked, ixload, &rawadr))
        return false;
    return langnewexternalvariable(false, rawadr, h);
}

boolean menuverbpacktotext (hdlexternalvariable h, Handle htext) {
    (void) h; (void) htext; return false;
}

boolean menuverbgetsize (hdlexternalvariable h, long *size) {
    (void) h; if (size) *size = 0; return true;
}

boolean menuverbgettimes (hdlexternalvariable h, int64_t *tc, int64_t *tm) {
    (void) h; if (tc) *tc = 0; if (tm) *tm = 0; return false;
}

boolean menuverbsettimes (hdlexternalvariable h, int64_t tc, int64_t tm) {
    (void) h; (void) tc; (void) tm; return false;
}

boolean menuverbfindusedblocks (hdlexternalvariable h, bigstring bspath) {
    (void) h; if (bspath) setemptystring(bspath); return false;
}

boolean menuverbfind (hdlexternalvariable h, boolean *flzoom) {
    (void) h; if (flzoom) *flzoom = false; return false;
}

boolean menuverbdispose (hdlexternalvariable h, boolean fldisk) {
    (void) h; (void) fldisk; return true;
}

boolean menuverbnew (Handle hdata, hdlexternalvariable *hv) {
    (void) hdata; (void) hv; return false;
}

boolean menuverbinmemory_context (const db_context *ctx, hdlexternalvariable hvariable) {
    /*
     * Menu externals are not materialized during headless migration.
     * Mark as "in memory" to skip actual loading, but keep the address for packing.
     */
    (void) ctx;
    (**hvariable).flinmemory = true;
    return true;
}

#endif /* !HEADLESS_LINKS_REAL_MENUVERBS */

#endif /* FRONTIER_HEADLESS */
