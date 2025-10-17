#include "frontier.h"
#include "tableverbs.h"
#include "tableinternal.h"
#include "tablestructure.h"
#include "tableformats.h"
#include "strings.h"

#ifdef FRONTIER_HEADLESS

hdltableformats tableformatsdata = nil;

#if defined(HEADLESS_USE_REAL_TABLEPACK)
boolean tablepushformats(hdltableformats hformats) {
    (void)hformats;
    return true;
}

boolean tablepopformats(void) {
    return true;
}

boolean tablepackformats(Handle *hpackedformats) {
    if (hpackedformats)
        *hpackedformats = nil;
    return true;
}

boolean tableunpackformats(Handle hpackedformats, hdltableformats hformats) {
    (void)hpackedformats;
    if (hformats)
        (**hformats).fldirty = false;
    return true;
}

boolean newtableformats(hdltableformats *hformats) {
    if (hformats)
        *hformats = nil;
    return true;
}
#endif /* HEADLESS_USE_REAL_TABLEPACK */

void tabledirty (void) {
}

boolean tableverbgetdisplaystring (hdlexternalvariable h, bigstring bs) {
    (void) h;
    copyctopstring("table", bs);
    return true;
}

boolean tableverbgettypestring (hdlexternalvariable h, bigstring bs) {
    (void) h;
    copyctopstring("table", bs);
    return true;
}

boolean tableverbgetsize (hdlexternalvariable h, long *size) {
    (void) h;
    if (size)
        *size = 0;
    return true;
}

boolean tableverbinmemory (hdlexternalvariable h, hdlhashnode node) {
    (void) h;
    (void) node;
    return true;
}

boolean tableverbisdirty (hdlexternalvariable h) {
    (void) h;
    return false;
}

boolean tableverbsetdirty (hdlexternalvariable h, boolean fldirty) {
    (void) h;
    (void) fldirty;
    return false;
}

boolean tableverbdispose (hdlexternalvariable h, boolean fldisk) {
    (void) h;
    (void) fldisk;
    return true;
}

boolean tableverbnew (hdlexternalvariable *hvariable) {
    hdltablevariable hv;
    hdlhashtable ht;
    if (!langnewexternalvariable(true, 0L, (hdlexternalvariable *)&hv))
        return false;
    if (!newhashtable(&ht)) {
        disposehandle((Handle)hv);
        return false;
    }
    (**hv).variabledata = (long) ht;
    (**ht).hashtablerefcon = (long) hv;
    (**ht).fldirty = true;
    *hvariable = (hdlexternalvariable) hv;
    return true;
}

#if !defined(HEADLESS_USE_REAL_TABLEPACK)
boolean tableverbpack (hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
    (void) h;
    if (hpacked)
        *hpacked = nil;
    if (flnewdbaddress)
        *flnewdbaddress = false;
    return false;
}
#endif /* !HEADLESS_USE_REAL_TABLEPACK */

#if !defined(HEADLESS_USE_REAL_TABLEPACK)
boolean tableverbpacktotext (hdlexternalvariable h, Handle htext) {
    (void) h;
    (void) htext;
    return false;
}
#endif /* !HEADLESS_USE_REAL_TABLEPACK */

#if !defined(HEADLESS_USE_REAL_TABLEPACK)
boolean tableverbunpack (Handle hpacked, long *ixload, hdlexternalvariable *h, boolean fldisk) {
    (void) hpacked;
    (void) ixload;
    (void) h;
    (void) fldisk;
    return false;
}
#endif /* !HEADLESS_USE_REAL_TABLEPACK */

#if !defined(HEADLESS_USE_REAL_TABLEPACK)
boolean tableverbmemorypack (hdlexternalvariable h, Handle *hpacked, hdlhashnode hnode) {
    (void) h;
    (void) hpacked;
    (void) hnode;
    return false;
}
#endif /* !HEADLESS_USE_REAL_TABLEPACK */

#if !defined(HEADLESS_USE_REAL_TABLEPACK)
boolean tableverbmemoryunpack (Handle hpacked, long *ixload, hdlexternalvariable *h, boolean fldisk) {
    (void) hpacked;
    (void) ixload;
    (void) h;
    (void) fldisk;
    return false;
}
#endif /* !HEADLESS_USE_REAL_TABLEPACK */

boolean tableverbfind (hdlexternalvariable h, boolean *flzoom) {
    (void) h;
    if (flzoom)
        *flzoom = false;
    return false;
}

boolean tableverbcontinuesearch (hdlexternalvariable h) {
    (void) h;
    return false;
}

#if !defined(HEADLESS_USE_REAL_TABLEPACK)
boolean tableverbgettimes (hdlexternalvariable h, long *timecreated, long *timemodified, hdlhashnode hnode) {
    (void) h;
    (void) hnode;
    if (timecreated)
        *timecreated = 0;
    if (timemodified)
        *timemodified = 0;
    return false;
}
#endif /* !HEADLESS_USE_REAL_TABLEPACK */

#if !defined(HEADLESS_USE_REAL_TABLEPACK)
boolean tableverbsettimes (hdlexternalvariable h, long timecreated, long timemodified, hdlhashnode hnode) {
    (void) h;
    (void) timecreated;
    (void) timemodified;
    (void) hnode;
    return false;
}
#endif /* !HEADLESS_USE_REAL_TABLEPACK */

#if !defined(HEADLESS_USE_REAL_TABLEPACK)
boolean tableverbfindusedblocks (hdlexternalvariable h, bigstring bspath) {
    (void) h;
    if (bspath)
        setemptystring(bspath);
    return false;
}
#endif /* !HEADLESS_USE_REAL_TABLEPACK */

boolean tablewindowopen (hdlexternalvariable h, hdlwindowinfo *hinfo) {
    (void) h;
    if (hinfo)
        *hinfo = nil;
    return false;
}

boolean tablewindowclosed (hdlexternalvariable h) {
    (void) h;
    return false;
}

boolean tableclientsurface (hdlexternalvariable h) {
    (void) h;
    return false;
}

boolean tableclienttitlepopuphit (Point pt, hdlexternalvariable h) {
    (void) pt;
    (void) h;
    return false;
}

boolean tablevaltotable (tyvaluerecord val, hdlhashtable *htable, hdlhashnode hnode) {
    (void) val;
    (void) htable;
    (void) hnode;
    return false;
}

boolean tableedit (hdlexternalvariable h, hdlwindowinfo win, ptrfilespec fs, bigstring bs, rectparam rzoom) {
    (void) h;
    (void) win;
    (void) fs;
    (void) bs;
    (void) rzoom;
    return false;
}

boolean tablesymbolsresorted (hdlhashtable htable) {
    (void) htable;
    return false;
}

#endif /* FRONTIER_HEADLESS */
