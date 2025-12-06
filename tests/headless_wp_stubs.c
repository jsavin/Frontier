#include "frontier.h"
#include "standard.h"
#include "langexternal.h"
#include "wpengine.h"
#include "wpverbs.h"
#include "strings.h"

#ifdef FRONTIER_HEADLESS

static long headless_wp_sel_start = 0;
static long headless_wp_sel_end = 0;

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

boolean wpverbpack (hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
    (void) h;
    if (hpacked)
        *hpacked = nil;
    if (flnewdbaddress)
        *flnewdbaddress = false;
    return false;
}

boolean wpverbunpack (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    (void) hpacked;
    (void) ixload;
    (void) h;
    return false;
}

boolean wpverbinmemory (hdlexternalvariable h) {
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
        *startsel = headless_wp_sel_start;
    if (endsel)
        *endsel = headless_wp_sel_end;
    return true;
}

boolean wpsetselection (long startsel, long endsel) {
    headless_wp_sel_start = startsel;
    headless_wp_sel_end = endsel;
    return true;
}

#endif /* FRONTIER_HEADLESS */
