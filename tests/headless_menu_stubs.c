#include "frontier.h"
#include "standard.h"
#include "langexternal.h"
#include "menuverbs.h"
#include "strings.h"

#ifdef FRONTIER_HEADLESS

boolean menuverbgetdisplaystring (hdlexternalvariable h, bigstring bs) {
    (void) h; copyctopstring("menu", bs); return true;
}
boolean menuverbgettypestring (hdlexternalvariable h, bigstring bs) {
    (void) h; copyctopstring("menu", bs); return true;
}
boolean menuverbsetdirty (hdlexternalvariable h, boolean fldirty) { (void)h;(void)fldirty; return false; }
boolean menuverbmemorypack (hdlexternalvariable h, Handle *hp) { (void)h; if (hp) *hp = nil; return false; }
boolean menuverbmemoryunpack (Handle hpacked, long *ixload, hdlexternalvariable *h) { (void)hpacked;(void)ixload;(void)h; return false; }
boolean menuverbpack (hdlexternalvariable h, Handle *hp, boolean *flnew) { (void)h; if (hp) *hp=nil; if (flnew) *flnew=false; return false; }
boolean menuverbunpack (Handle hpacked, long *ixload, hdlexternalvariable *h) { (void)hpacked;(void)ixload;(void)h; return false; }
boolean menuverbpacktotext (hdlexternalvariable h, Handle htext) { (void)h;(void)htext; return false; }
boolean menuverbgetsize (hdlexternalvariable h, long *size) { (void)h; if (size) *size=0; return true; }
boolean menuverbgettimes (hdlexternalvariable h, long *tc, long *tm) { (void)h; if (tc) *tc=0; if (tm) *tm=0; return false; }
boolean menuverbsettimes (hdlexternalvariable h, long tc, long tm) { (void)h;(void)tc;(void)tm; return false; }
boolean menuverbfindusedblocks (hdlexternalvariable h, bigstring bspath) { (void)h; if (bspath) setemptystring(bspath); return false; }
boolean menuverbfind (hdlexternalvariable h, boolean *flzoom) { (void)h; if (flzoom) *flzoom=false; return false; }
boolean menuverbdispose (hdlexternalvariable h, boolean fldisk) { (void)h;(void)fldisk; return true; }
boolean menuverbnew (Handle hdata, hdlexternalvariable *hv) { (void)hdata;(void)hv; return false; }

#endif /* FRONTIER_HEADLESS */

