#include "frontier.h"
#include "standard.h"
#include "langexternal.h"
#include "opverbs.h"
#include "oplist.h"
#include "strings.h"

#ifdef FRONTIER_HEADLESS

boolean opverbgetsummitstring (hdlexternalvariable h, bigstring bs) {
    (void) h;
    copyctopstring("outline", bs);
    return true;
}

boolean opverbgettypestring (hdlexternalvariable h, bigstring bs) {
    (void) h;
    copyctopstring("outline", bs);
    return true;
}

boolean opverbdispose (hdlexternalvariable h, boolean fldisk) {
    (void) h;
    (void) fldisk;
    return true;
}

void opverbunload (hdlexternalvariable h, dbaddress adr) {
    (void) h;
    (void) adr;
}

boolean opverbisdirty (hdlexternalvariable h) {
    (void) h;
    return false;
}

boolean opverbsetdirty (hdlexternalvariable h, boolean fldirty) {
    (void) h;
    (void) fldirty;
    return false;
}

boolean opverbmemorypack (hdlexternalvariable h, Handle *hpacked) {
    (void) h;
    (void) hpacked;
    return false;
}

boolean opverbmemoryunpack (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    (void) hpacked;
    (void) ixload;
    (void) h;
    return false;
}

boolean opverbscriptmemoryunpack (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    (void) hpacked;
    (void) ixload;
    (void) h;
    return false;
}

boolean opverbpack (hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
    (void) h;
    if (hpacked)
        *hpacked = nil;
    if (flnewdbaddress)
        *flnewdbaddress = false;
    return false;
}

boolean opverbpacktotext (hdlexternalvariable h, Handle htext) {
    (void) h;
    (void) htext;
    return false;
}

boolean opverbgetsize (hdlexternalvariable h, long *size) {
    (void) h;
    if (size)
        *size = 0;
    return true;
}

boolean opverbgettimes (hdlexternalvariable h, long *timecreated, long *timemodified) {
    (void) h;
    if (timecreated)
        *timecreated = 0;
    if (timemodified)
        *timemodified = 0;
    return false;
}

boolean opverbsettimes (hdlexternalvariable h, long timecreated, long timemodified) {
    (void) h;
    (void) timecreated;
    (void) timemodified;
    return false;
}

boolean opverbnew (short kind, Handle data, hdlexternalvariable *h) {
    (void) kind;
    (void) data;
    (void) h;
    return false;
}

boolean opverbcopyvalue (hdlexternalvariable h, hdlexternalvariable *dest) {
    (void) h;
    (void) dest;
    return false;
}

boolean opverbgetlangtext (hdlexternalvariable h, boolean fl, Handle *htext, long *size) {
    (void) h;
    (void) fl;
    (void) htext;
    if (size)
        *size = 0;
    return false;
}

boolean opverbarrayreference (hdlexternalvariable h, long index, hdlheadrecord *head) {
    (void) h;
    (void) index;
    (void) head;
    return false;
}

boolean opwindowopen (hdlexternalvariable h, hdlwindowinfo *hinfo) {
    (void) h;
    if (hinfo)
        *hinfo = nil;
    return false;
}

boolean opedit (hdlexternalvariable h, hdlwindowinfo win, ptrfilespec fs, bigstring bs, rectparam rzoom) {
    (void) h;
    (void) win;
    (void) fs;
    (void) bs;
    (void) rzoom;
    return false;
}

boolean opverbfind (hdlexternalvariable h, boolean *flzoom) {
    (void) h;
    if (flzoom)
        *flzoom = false;
    return false;
}

boolean opverbpackunpack (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    (void) hpacked;
    (void) ixload;
    (void) h;
    return false;
}

boolean opverbunpack (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    (void) hpacked;
    (void) ixload;
    (void) h;
    return false;
}

boolean opverbscriptunpack (Handle hpacked, long *ixload, hdlexternalvariable *h) {
    (void) hpacked;
    (void) ixload;
    (void) h;
    return false;
}

boolean opverbpackvalue (hdlexternalvariable h, Handle *hp) {
    (void) h;
    (void) hp;
    return false;
}

boolean opverbgetvariable (hdlexternalvariable *hv) {
    (void) hv;
    return false;
}

boolean opstart (void) {
    return true;
}

void opvisibarcursor (void) {
}

boolean opvisitlist (hdllistrecord hlist, opvisitlistcallback visit, ptrvoid refcon) {
    (void) hlist;
    (void) visit;
    (void) refcon;
    return false;
}

#endif /* FRONTIER_HEADLESS */
