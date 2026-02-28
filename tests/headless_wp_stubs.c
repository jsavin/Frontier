#include "frontier.h"
#include "standard.h"
#include "langexternal.h"
#include "wpengine.h"
#include "wpverbs.h"
#include "strings.h"
#include "db.h"
#include "db_format.h"
#include "processinternal.h"  /* wp_sel_start/wp_sel_end macros (thread-local via GIL) */

#ifdef FRONTIER_HEADLESS

boolean wpverbgetdisplaystring (hdlexternalvariable h, bigstring bs) {
    (void) h;
    copyctopstring("wp", bs);
    return true;
}

boolean wpverbgettypestring (hdlexternalvariable h, bigstring bs) {
    (void) h;
    copyctopstring("wp", bs);
    return true;
}

boolean wpverbdispose (hdlexternalvariable h, boolean fldisk) {
    (void) h;
    (void) fldisk;
    return true;
}

boolean wpverbisdirty (hdlexternalvariable h) {
    (void) h;
    return false;
}

boolean wpverbsetdirty (hdlexternalvariable h, boolean fldirty) {
    (void) h;
    (void) fldirty;
    return false;
}

boolean wpverbnew (Handle h, hdlexternalvariable *hv) {
    (void) h;
    (void) hv;
    return false;
}

boolean wpverbmemorypack (hdlexternalvariable h, Handle *hpacked) {
    (void) h;
    (void) hpacked;
    return false;
}

boolean wpverbmemoryunpack (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    (void) hpacked;
    (void) ixload;
    (void) h;
    return false;
}

boolean wpverbpack_internal (const db_context *ctx, hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
    if ((h == nil) || (hpacked == nil))
        return false;

    db_format_mode savedmode = db_format_mode_current();

    if (ctx != NULL)
        db_format_mode_apply(&ctx->mode);

    if (!(**h).flinmemory)  {
        db_format_mode_apply(&savedmode);
        return false;
    }

    dbaddress adr = (**h).oldaddress;

    db_format_mode_apply(&savedmode);

    if (flnewdbaddress != NULL)
        *flnewdbaddress = false;

    return pushlongondiskhandle((long) adr, *hpacked);
}

boolean wpverbpack (hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
    (void) h;
    if (hpacked)
        *hpacked = nil;
    if (flnewdbaddress)
        *flnewdbaddress = false;
    return false;
}

boolean wpverbunpack (Handle hpacked, long *ixload, hdlexternalvariable *h, hdldatabaserecord hdb) {
    (void) hpacked;
    (void) ixload;
    (void) h;
    (void) hdb;
    return false;
}

boolean wpverbinmemory (const db_context *ctx, hdlexternalvariable h) {
    (void) ctx;
    (void) h;
    return true;
}

boolean wpverbpacktotext (hdlexternalvariable h, Handle htext) {
    (void) h;
    (void) htext;
    return false;
}

boolean wpverbgetsize (hdlexternalvariable h, long *size) {
    (void) h;
    if (size)
        *size = 0;
    return true;
}

boolean wpverbgettimes (hdlexternalvariable h, int64_t *timecreated, int64_t *timemodified) {
    (void) h;
    if (timecreated)
        *timecreated = 0;
    if (timemodified)
        *timemodified = 0;
    return false;
}

boolean wpverbsettimes (hdlexternalvariable h, int64_t timecreated, int64_t timemodified) {
    (void) h;
    (void) timecreated;
    (void) timemodified;
    return false;
}

boolean wpwindowopen (hdlexternalvariable h, hdlwindowinfo *hinfo) {
    (void) h;
    if (hinfo)
        *hinfo = nil;
    return false;
}

boolean wpedit (hdlexternalvariable h, hdlwindowinfo win, ptrfilespec fs, bigstring bs, rectparam rzoom) {
    (void) h;
    (void) win;
    (void) fs;
    (void) bs;
    (void) rzoom;
    return false;
}

boolean wpverbfind (hdlexternalvariable h, boolean *flzoom) {
    (void) h;
    if (flzoom)
        *flzoom = false;
    return false;
}

boolean wpstart (void) {
    return true;
}

boolean wpgetselection (long *startsel, long *endsel) {
    if (startsel)
        *startsel = wp_sel_start;
    if (endsel)
        *endsel = wp_sel_end;
    return true;
}

boolean wpsetselection (long startsel, long endsel) {
    wp_sel_start = startsel;
    wp_sel_end = endsel;
    return true;
}

#endif /* FRONTIER_HEADLESS */
