#include "frontier.h"
#include "standard.h"
#include "langexternal.h"
#include "pictverbs.h"
#include "strings.h"
#include "db.h"
#include "db_format.h"

#ifdef FRONTIER_HEADLESS

boolean pictverbgetdisplaystring (hdlexternalvariable h, bigstring bs) {
    (void) h;
    copyctopstring("picture", bs);
    return true;
}

boolean pictverbgettypestring (hdlexternalvariable h, bigstring bs) {
    (void) h;
    copyctopstring("picture", bs);
    return true;
}

boolean pictverbdispose (hdlexternalvariable h, boolean fldisk) {
    (void) h;
    (void) fldisk;
    return true;
}

boolean pictverbnew (Handle h, hdlexternalvariable *hv) {
    (void) h;
    (void) hv;
    return false;
}

boolean pictverbisdirty (hdlexternalvariable h) {
    (void) h;
    return false;
}

boolean pictverbsetdirty (hdlexternalvariable h, boolean fldirty) {
    (void) h;
    (void) fldirty;
    return false;
}

boolean pictverbmemorypack (hdlexternalvariable h, Handle *hpacked) {
    (void) h;
    (void) hpacked;
    return false;
}

boolean pictverbmemoryunpack (Handle hpacked, long *ixload, hdlexternalvariable *hv) {
    (void) hpacked;
    (void) ixload;
    (void) hv;
    return false;
}

boolean pictverbpack (hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
    if ((h == nil) || (hpacked == nil))
        return false;

    dbaddress adr = (**h).oldaddress;
    if (adr == nildbaddress)
        adr = (dbaddress) (**h).variabledata;
    if (adr == nildbaddress)
        return false;

    if (fldatabasesaveas || db_format_mode_current().use_64bit_format) {
        Handle hcopy = nil;
        if (!dbrefhandle(adr, &hcopy))
            return false;
        dbaddress copy = adr;
        boolean ok = dbassignhandle(hcopy, &copy);
        disposehandle(hcopy);
        if (!ok)
            return false;
        adr = copy;
        if (flnewdbaddress)
            *flnewdbaddress = true;
    } else if (flnewdbaddress) {
        *flnewdbaddress = false;
    }

    (**h).oldaddress = adr;
    return pushlongondiskhandle((long) adr, *hpacked);
}

boolean pictverbunpack (Handle hpacked, long *ixload, hdlexternalvariable *hv) {
    long rawadr = 0;
    if (!loadlongfromdiskhandle(hpacked, ixload, &rawadr))
        return false;
    return langnewexternalvariable(false, rawadr, hv);
}

boolean pictverbpacktotext (hdlexternalvariable h, Handle htext) {
    (void) h;
    (void) htext;
    return false;
}

boolean pictverbgetsize (hdlexternalvariable h, long *size) {
    (void) h;
    if (size)
        *size = 0;
    return true;
}

boolean pictverbgettimes (hdlexternalvariable h, long *timecreated, long *timemodified) {
    (void) h;
    if (timecreated)
        *timecreated = 0;
    if (timemodified)
        *timemodified = 0;
    return false;
}

boolean pictverbsettimes (hdlexternalvariable h, long timecreated, long timemodified) {
    (void) h;
    (void) timecreated;
    (void) timemodified;
    return false;
}

boolean pictwindowopen (hdlexternalvariable h, hdlwindowinfo *hinfo) {
    (void) h;
    if (hinfo)
        *hinfo = nil;
    return false;
}

boolean pictedit (hdlexternalvariable h, hdlwindowinfo win, ptrfilespec fs, bigstring bs, rectparam rzoom) {
    (void) h;
    (void) win;
    (void) fs;
    (void) bs;
    (void) rzoom;
    return false;
}

boolean pictverbfind (hdlexternalvariable h, boolean *flzoom) {
    (void) h;
    if (flzoom)
        *flzoom = false;
    return false;
}

boolean pictstart (void) {
    return true;
}

#endif /* FRONTIER_HEADLESS */
