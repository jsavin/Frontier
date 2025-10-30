/* 2025-10-31 Codex: Skip portable handle stubs when FRONTIER_USE_PORTABLE_HANDLES is active. */
#include "frontier.h"
#include "portable_handles.h"
#include "osincludes_portable.h"
#include "file.h"
#include "file_portable.h"
#include "strings.h"
#include "quickdraw.h"
#include "shell.h"
#include "shellundo.h"
#include "mouse.h"
#include "op.h"
#include "opdisplay.h"
#include "opinternal.h"
#include "osacomponent.h"
#include "cursor.h"
#include "shelltypes.h"
#include "langexternal.h"
#include "db.h"
#include "resources.h"
#include "aeutils.h"
#include "timedate.h"
#include "font.h"
#include "scripts.h"
#include "tableformats.h"
#include "kb.h"
#include "cancoon.h"
#include "opxml.h"
#include "error.h"
#include "claybrowser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef FRONTIER_HEADLESS

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

WindowPtr shellwindow = NULL;
hdlwindowinfo shellwindowinfo = NULL;
RGBColor whitecolor = {65535, 65535, 65535};

// Minimal shell globals struct to satisfy references
tyshellglobals shellglobals;

#if !defined(FRONTIER_USE_PORTABLE_HANDLES)
Handle NewHandle(long userSize) {
    return frontierAlloc(userSize);
}

void DisposeHandle(Handle h) {
    frontierFree(h);
}

void HLock(Handle h) {
    (void)frontierLock(h);
}

void HUnlock(Handle h) {
    frontierUnlock(h);
}

void DebugStr(const unsigned char *s) { (void)s; }
void Debugger(void) { }

unsigned long FastMilliseconds(void) {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return (unsigned long)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
}

long GetHandleSize(Handle h) {
    return frontierSize(h);
}

OSErr SetHandleSize(Handle h, long userSize) {
    return frontierReAlloc(h, userSize) ? noErr : memFullErr;
}
#endif /* !FRONTIER_USE_PORTABLE_HANDLES */

boolean shellpushglobals (WindowPtr w) {
    (void) w;
    return false;
}

boolean shellpopglobals (void) {
    return false;
}

void shellupdatescrollbars (hdlwindowinfo hinfo) {
    (void) hinfo;
}

void shellwindowinval (hdlwindowinfo hinfo) {
    (void) hinfo;
}

boolean windowsetchanges (WindowPtr w, boolean fldirty) {
    (void) w;
    (void) fldirty;
    return false;
}

// Process stubs
void processcodedisposed (long x) { (void)x; }
boolean processdisposecode (hdltreenode n) { (void)n; return false; }
boolean processstartprofiling (boolean fl) { (void)fl; return false; }
boolean processstopprofiling (void) { return false; }
boolean processyield (void) { return false; }
boolean processyieldtoagents (void) { return false; }
unsigned long processstackspace (void) { return 1024 * 1024; }

// QuickDraw/shell visual helpers (no-ops)
boolean pushscratchport (void) { return false; }
boolean pushstyle (short a, short b, short c) { (void)a;(void)b;(void)c; return false; }
boolean pushcolors (const RGBColor *f, const RGBColor *b) { (void)f; (void)b; return false; }

// Undo stubs
boolean pushundostep (undocallback cb, Handle h) { (void)cb; (void)h; return false; }
boolean pushundoaction (short action) { (void)action; return false; }

// Converters and resources
void rgbtodiskrgb (RGBColor *r, diskrgb *out) { (void)r; if (out) memset(out, 0, sizeof(*out)); }
void recttodiskrect (Rect *r, diskrect *out) { (void)r; if (out) memset(out, 0, sizeof(*out)); }
void releaseresourcehandle (Handle h) { (void)h; }

// Shell helpers
boolean shellbackgroundtask (void) { return false; }
boolean shellnewcallbacks (ptrcallbacks *pp) { if (pp) *pp = &shellglobals; return true; }
boolean shellgetconfig (short which, tyconfigrecord *rec) { (void)which; if (rec) memset(rec,0,sizeof(*rec)); return false; }
boolean shellclosechildwindows (hdlwindowinfo hi) { (void)hi; return false; }
boolean shellclosewindow (WindowPtr w) { (void)w; return false; }
boolean shellcloseall (WindowPtr w, boolean f) { (void)w; (void)f; return false; }
boolean shellclose (WindowPtr w, boolean f) { (void)w; (void)f; return false; }
boolean shellfilterscrollkey (byte ch) { (void)ch; return false; }
boolean shellconvertscrap (tyscraptype t, Handle *hh, boolean *fltext) { (void)t; if (hh) *hh=nil; if (fltext) *fltext=false; return false; }
void shellcheckdirtyscrollbars (void) { }
boolean shellgetglobalwindowrect (hdlwindowinfo hi, Rect *r) { (void)hi; if (r) { r->top=0; r->left=0; r->bottom=600; r->right=800; } return true; }
boolean shellbringtofront (hdlwindowinfo hi) { (void)hi; return false; }
boolean shellgetexternaldata (hdlwindowinfo hi, void *p) { (void)hi; (void)p; return false; }
boolean shellclosedatawindow (Handle h) { (void)h; return false; }
void shellinvalbuttons (void) { }

// Misc small helpers
boolean pointinrect (Point pt, Rect r) { (void)pt; (void)r; return false; }
boolean popport (void) { return false; }
boolean popstyle (void) { return false; }
boolean popundoaction (void) { return false; }
void ouch (void) { }

// Error and path helpers
#if !defined(FRONTIER_PORTABLE) && !defined(HEADLESS_TEST_PORTABLE_FILE)
boolean oserror (OSErr err) { (void)err; return false; }
boolean pathtofilespec (bigstring bs, ptrfilespec fs) { (void)bs; if (fs) memset(fs,0,sizeof(*fs)); return false; }
#endif
OSStatus pathtofsref (bigstring bs, FSRef *ref) { (void)bs; if (ref) memset(ref,0,sizeof(*ref)); return paramErr; }
#if !defined(FRONTIER_PORTABLE) && !defined(HEADLESS_TEST_PORTABLE_FILE)
boolean equalfilespecs ( const ptrfilespec fs1, const ptrfilespec fs2 ) { (void)fs1; (void)fs2; return false; }
#endif
boolean equalrects (Rect r1, Rect r2) { return r1.top==r2.top && r1.left==r2.left && r1.bottom==r2.bottom && r1.right==r2.right; }
void diskinitloop (void) { }

// Shell scrap and events
EventRecord shellevent;
boolean shellsetscrap (void *p, tyscraptype t, shelldisposescrapcallback d, shellexportscrapcallback e) { (void)p;(void)t;(void)d;(void)e; return false; }
boolean shellgetscrap (Handle *hh, tyscraptype *t) { if (hh) *hh=nil; if (t) *t=0; return false; }
boolean shelleventsblocked (void) { return true; }

// Cursor
void setcursortype (tycursortype c) { (void)c; }

// Opscreenmap/opdisplay shims
boolean opinitdisplayvariables (void) { return false; }
void opgettextrect (hdlheadrecord h, const Rect *linerect, Rect *textrect) { (void)h; if (textrect) { if (linerect) *textrect = *linerect; else memset(textrect,0,sizeof(*textrect)); } }
long opgetnodelinecount (hdlheadrecord h) { (void)h; return 1; }
boolean opgetnoderect (hdlheadrecord h, Rect *r) { (void)h; if (r) memset(r,0,sizeof(*r)); return false; }
boolean opgetscreenline (hdlheadrecord h, long *ln) { (void)h; if (ln) *ln = 0; return false; }
long opgetcurrentscreenlines (boolean flscrollwise) { (void)flscrollwise; return 0; }
boolean opgetscrollbarinfo (boolean f) { (void)f; return false; }
void opinvaldirtynodes (void) { }
void opinvaldisplay (void) { }
boolean opinvalnode (hdlheadrecord h) { (void)h; return false; }
void opinvalstructure (hdlheadrecord h) { (void)h; }
boolean opnewscreenmap (hdlscreenmap *m) { if (m) *m = nil; return false; }
void opinvalscreenmap (hdlscreenmap m) { (void)m; }
void opjumpdisplayto (hdlheadrecord a, hdlheadrecord b) { (void)a; (void)b; }
boolean opneedvisiscroll (hdlheadrecord h, long *hs, long *vs, boolean f) { (void)h; (void)f; if (hs) *hs=0; if (vs) *vs=0; return false; }
boolean opsetscrollpositiontoline1 (void) { return false; }
boolean opprint (short style) { (void)style; return false; }

// Headless op display stubs
boolean opdisplayenabled (void) { return false; }
void opupdatenow (void) { }
void opupdate (void) { }
void opsmashdisplay (void) { }
void opdocursor (boolean flon) { (void)flon; }
void opdovisiscroll (long hs, long vs) { (void)hs; (void)vs; }
boolean opgetlinerect (long lnum, Rect *r) { (void)lnum; if (r) memset(r,0,sizeof(*r)); return false; }
short opgetlineheight (hdlheadrecord hnode) { (void)hnode; if (outlinedata) return (short)((**outlinedata).defaultlineheight + 2*textvertinset); return (short)(12 + 2*textvertinset); }
short opgetlinewidth (hdlheadrecord hnode) { (void)hnode; return 0; }
void opdrawicon (hdlheadrecord hnode, Rect linerect) { (void)hnode; (void)linerect; }
boolean opgeticonrect (hdlheadrecord hnode, const Rect *linerect, Rect *iconrect) { (void)hnode; if (iconrect) memset(iconrect,0,sizeof(*iconrect)); (void)linerect; return false; }
boolean openabledisplay (void) { return false; }
void operasedisplay (void) { }
boolean opendprint (void) { return false; }
boolean opisdraggingmove (Point p, unsigned long t) { (void)p; (void)t; return false; }
void opdraggingmove (Point ptstart, hdlheadrecord hsource) { (void)ptstart; (void)hsource; }
void opinvalafter (hdlheadrecord h) { (void)h; }
void opmakegap (long lnum, short lineheight) { (void)lnum; (void)lineheight; }
void opvisisubheads (hdlheadrecord h) { (void)h; }
boolean opgetoutinesize (long *w, long *h) { if (w) *w = 0; if (h) *h = 0; return true; }
boolean shellupdatenow (WindowPtr w) { (void)w; return false; }
boolean shellpushwindowglobals (hdlwindowinfo hinfo) { (void)hinfo; return true; }
void shellupdatewindow (WindowPtr w) { (void)w; }
boolean shellmovewindowhidden (hdlwindowinfo hinfo, short h, short v) { (void)hinfo; (void)h; (void)v; return true; }
boolean shellsizewindowhidden (hdlwindowinfo hinfo, short h, short v) { (void)hinfo; (void)h; (void)v; return true; }
boolean shellsetwindowtitle (hdlwindowinfo hinfo, bigstring bstitle) { (void)hinfo; (void)bstitle; return true; }
void shellgetwindowtitle (hdlwindowinfo hinfo, bigstring bstitle) { (void)hinfo; setemptystring(bstitle); }

// Default drawing/metrics helpers expected by opinit/opdisplay
boolean opdefaultdrawicon (hdlheadrecord h, const Rect *r, boolean sel, boolean inv) { (void)h;(void)r;(void)sel;(void)inv; return true; }
boolean opdefaultdrawtext (hdlheadrecord h, const Rect *r, boolean sel, boolean inv) { (void)h;(void)r;(void)sel;(void)inv; return true; }
boolean opdefaultgetfullrect (hdlheadrecord h, Rect *fullrect) { (void)h; if (fullrect) memset(fullrect,0,sizeof(*fullrect)); return true; }
boolean opdefaultgeticonrect (hdlheadrecord h, const Rect *linerect, Rect *iconrect) { (void)h; (void)linerect; if (iconrect) memset(iconrect,0,sizeof(*iconrect)); return true; }
boolean opdefaultgetlineheight (hdlheadrecord h, short *lh) { (void)h; if (lh) *lh = (short)(12 + 2*textvertinset); return true; }
boolean opdefaultgetlinewidth (hdlheadrecord h, short *lw) { (void)h; if (lw) *lw = 0; return true; }

// Mouse stubs
tymouserecord mousestatus;
boolean mousedoubleclick (void) { return false; }
boolean mousestilldown (void) { return false; }
void getmousepoint (Point *pt) { if (pt) { pt->h = 0; pt->v = 0; } }
boolean mousecheckautoscroll (Point pt, Rect r, boolean flhoriz, tydirection *dir) { (void)pt;(void)r;(void)flhoriz;(void)dir; return false; }

// Quickdraw rectangle invalidation stubs
void smashrect (Rect r) { (void)r; }
void invalrect (Rect r) { (void)r; }

// DB stubs (disabled when linking the real database core)
#if !defined(HEADLESS_LINKS_REAL_DB)
boolean dbpushdatabase (hdldatabaserecord h) { (void)h; return false; }
boolean dbpopdatabase (void) { return false; }
boolean dbcopy (dbaddress a, dbaddress *b) { (void)a; if (b) *b=0; return false; }
boolean dbassignhandle (Handle h, dbaddress *adr) { (void)h; if (adr) *adr=0; return false; }
#endif
hdldatabaserecord databasedata = nil;

// Process/debug stubs
boolean debuggingcurrentprocess (void) { return false; }

// Disk/font/color conversions
void diskgetfontname (short num, diskfontstring s) { (void)num; if (s) memset(s,0,sizeof(*s)); }
void diskgetfontnum (diskfontstring s, short *num) { (void)s; if (num) *num=0; }
void diskrecttorect (diskrect *dr, Rect *r) { if (dr && r) memset(r,0,sizeof(*r)); }
void diskrgbtorgb (diskrgb *d, RGBColor *r) { if (d && r) memset(r,0,sizeof(*r)); }

// Table formats
void disposetableformats (hdltableformats hf) { (void)hf; }

// Shell/misc helpers
// Provide a minimal, static windowinfo handle for headless paths that expect one
static tywindowinfo g_headless_wininfo; /* zero-initialized */
static tywindowinfo *g_headless_wininfo_ptr = &g_headless_wininfo;

boolean shellfinddatawindow (Handle h, hdlwindowinfo *hi) {
    (void)h;
    if (hi)
        *hi = &g_headless_wininfo_ptr; /* return a stable handle */
    return true;
}
void shellforcecursoradjust (void) { }
void shellouch (void) { }
boolean getrootwindow (WindowPtr w, hdlwindowinfo *hi) { (void)w; if (hi) *hi=nil; return false; }
Handle getresourcehandle (ResType t, short id) { (void)t; (void)id; return nil; }
void loadconfigresource (short n, tyconfigrecord *cr) { (void)n; if (cr) memset(cr,0,sizeof(*cr)); }

// File/FS helpers (implemented in portable/file_portable.c)
boolean fileparsevolname (bigstring bs, ptrfilespec fs) { (void)bs; if (fs) memset(fs,0,sizeof(*fs)); return false; }
OSErr macgetfsref (const ptrfilespec fs, FSRef* fsref) { (void)fs; if (fsref) memset(fsref,0,sizeof(*fsref)); return paramErr; }
OSErr macmakefilespec (const FSRef *fsref, ptrfilespec fs) { (void)fsref; if (fs) memset(fs,0,sizeof(*fs)); return paramErr; }
OSErr macgetfilespecparent (const ptrfilespec fs, ptrfilespec fsparent) { (void)fs; if (fsparent) memset(fsparent,0,sizeof(*fsparent)); return paramErr; }
void fsnametobigstring (const tyfsnameptr fsname, bigstring bs) {
    if (!fsname) {
        setemptystring(bs);
        return;
    }
    unsigned int len = fsname->length;
    if (len > lenbigstring)
        len = lenbigstring;
    bs[0] = (unsigned char) len;
    for (unsigned int i = 0; i < len; ++i)
        bs[1 + i] = (unsigned char) (fsname->unicode[i] & 0xFF);
}

void bigstringtofsname (const bigstring bs, tyfsnameptr fsname) {
    if (!fsname) return;
    unsigned int len = bs[0];
    if (len > 255)
        len = 255;
    fsname->length = (UInt16) len;
    for (unsigned int i = 0; i < len; ++i)
        fsname->unicode[i] = (UInt16) (unsigned char) bs[1 + i];
}

// Timing/keyboard
long getcurrenttimezonebias (void) { return 0; }
short getkeyboardstartrepeattime (void) { return 0; }
long getmousedoubleclicktime (void) { return 0; }
boolean keyboardescape (void) { return false; }
void fontgetnumber (bigstring bs, short *num) { (void)bs; if (num) *num=0; }
boolean getmachinename (bigstring bsname) { setemptystring (bsname); return false; }

// Component/AE stubs
boolean havecomponentmanager (void) { return false; }
boolean newdescnull (AEDesc *desc, DescType type) { (void)type; if (desc) memset(desc,0,sizeof(*desc)); return true; }
boolean newdescwithhandle (AEDesc *desc, DescType type, Handle h) { (void)type; (void)h; if (desc) memset(desc,0,sizeof(*desc)); return true; }
boolean nildatahandle (AEDesc *desc) { (void)desc; return true; }

// Menu/UI verbs used by opverbs
boolean menuedit (void) { return false; }
boolean menuwindowopen (hdlexternalvariable h, hdlwindowinfo *hi) { (void)h; if (hi) *hi=nil; return false; }
boolean menuverbisdirty (hdlexternalvariable h) { (void)h; return false; }

// OP verbs helpers
boolean opbeginprint (void) { return false; }
boolean opbuttonstatus (void) { return false; }
boolean opbutton (void) { return false; }
boolean oppostfontchange (void) { return true; }
void oprestorehoists (void) { }
boolean oprestorescrollposition (void) { return false; }
void opsetdisplaydefaults (hdloutlinerecord ho) { (void)ho; }

// Script helpers
boolean scriptgetnametype (bigstring bsname, long *signature) { (void)bsname; if (signature) *signature=0; return false; }
boolean scriptgettypename (long signature, bigstring bsname) { (void)signature; setemptystring (bsname); return false; }

// Lang target helpers
boolean langinitbuiltins (void) { return true; }
boolean langfindtargetwindow (short id, WindowPtr *w) { (void)id; if (w) *w=NULL; return false; }
boolean langsettarget (hdlhashtable ht, bigstring bs, tyvaluerecord *prev) { (void)ht;(void)bs;(void)prev; return false; }
boolean langcleartarget (tyvaluerecord *prev) { (void)prev; return false; }
boolean langzoomvalwindow (hdlhashtable ht, bigstring bs, tyvaluerecord v, boolean fl) { (void)ht;(void)bs;(void)v;(void)fl; return false; }

// Misc flags/globals expected by opverbs/tablepack
boolean flconvertingolddatabase = false;
#if !defined(HEADLESS_LINKS_REAL_DB)
boolean fldatabasesaveas = false;
#endif
boolean flinhibitclosedialogs = false;

// Additional stubs to satisfy remaining links
boolean arrowkey (char ch) { (void)ch; return false; }
RGBColor blackcolor = {0,0,0};
boolean ccdisposefilerecord (void) { return false; }
boolean ccsavespecialfile (ptrfilespec fs, hdlfilenum fnum, short rnum, boolean flsaveas, boolean flrunnable) { (void)fs;(void)fnum;(void)rnum;(void)flsaveas;(void)flrunnable; return false; }
boolean ccfindrootwindow (hdlwindowinfo *hi) { if (hi) *hi=nil; return false; }
void clearfilespec (ptrfilespec fs) { if (fs) memset(fs,0,sizeof(*fs)); }
boolean cmdkeydown (void) { return false; }
tyconfigrecord config; /* default-initialized */
boolean copydatahandle (AEDesc *desc, Handle *hout) { (void)desc; if (hout) *hout=nil; return false; }
void bigstringtofsname (const bigstring bs, tyfsnameptr fsname) { (void)bs; if (fsname) memset(fsname,0,sizeof(*fsname)); }
boolean datahandletostring (AEDesc* desc, bigstring bs) { (void)desc; setemptystring(bs); return false; }
boolean getscrap (tyscraptype t, Handle h) { (void)t; (void)h; return false; }
boolean gettablevalue (hdlhashtable ht, bigstring bs, tyvaluerecord *v, hdlhashnode *node) { (void)ht;(void)bs;(void)v; if (node) *node=nil; return false; }
boolean getuntitledfilename (bigstring bs) { setemptystring(bs); return false; }
boolean getwinparam (hdltreenode node, short id, hdlwindowinfo *hi) { (void)node; (void)id; if (hi) *hi=nil; return false; }
tyshellglobals globalsarray[1];
boolean handlesearch (Handle h, long *ix, long *len) { (void)h; if (ix) *ix=0; if (len) *len=0; return false; }
void initbeachball (tydirection dir) { (void)dir; }
boolean isfilewindow (WindowPtr w) { (void)w; return false; }
boolean ismouserightclick (void) { return false; }
void killundo (void) { }
void rollbeachball (void) { }
void secondstodatetime (long secs, short *yr, short *mon, short *day, short *doy, short *hr, short *min) { (void)secs; if(yr) *yr=0; if(mon) *mon=0; if(day) *day=0; if(doy) *doy=0; if(hr) *hr=0; if(min) *min=0; }
void secondstodayofweek (long secs, short *dow) { (void)secs; if (dow) *dow=0; }
void setfserrorparam ( const ptrfilespec fs ) { (void)fs; }
boolean setoserrorparam (bigstring bs) { (void)bs; return false; }
void scriptsetcallbacks (hdloutlinerecord ho) { (void)ho; }
boolean shellfindcallbacks (short id, short *ix) { (void)id; if (ix) *ix=0; return false; }
boolean browsergetrefcon (hdlheadrecord hnode, tybrowserinfo *info) { (void)hnode; if (info) memset(info,0,sizeof(*info)); return false; }
boolean memoryerror (void) { return false; }


// opdisplay default helpers
boolean opdefaultgettextrect (hdlheadrecord h, const Rect *linerect, Rect *textrect) { (void)h; if (textrect) { if (linerect) *textrect = *linerect; else memset(textrect,0,sizeof(*textrect)); } return true; }
boolean opdefaultpredrawline (hdlheadrecord h, const Rect *r, boolean fs, boolean fi) { (void)h;(void)r;(void)fs;(void)fi; return true; }
boolean opdefaultpostdrawline (hdlheadrecord h, const Rect *r, boolean fs, boolean fi) { (void)h;(void)r;(void)fs;(void)fi; return true; }
boolean opdirtymeasurements (void) { return true; }
boolean opdisabledisplay (void) { return true; }
boolean opvisinode (hdlheadrecord hnode, boolean flhoriz) { (void)hnode; (void)flhoriz; return false; }
boolean opvalidate (hdloutlinerecord houtline) { (void)houtline; return true; }
hdlheadrecord oppointnode (Point pt) { (void)pt; return nil; }
boolean opnodevisible (hdlheadrecord h) { (void)h; return true; }
void opredrawscrollbars (void) { }
void opresetscrollbars (void) { }
boolean opscroll (tydirection dir, boolean f, long n) { (void)dir; (void)f; (void)n; return false; }
boolean opscrollto (long a, long b) { (void)a; (void)b; return false; }
boolean opsetprintinfo (void) { return false; }
boolean oprmousedown (Point pt, tyclickflags flags) { (void)pt; (void)flags; return false; }
boolean oppushhoist (hdlheadrecord h) { (void)h; return false; }
boolean oppophoist (void) { return false; }
boolean oppopallhoists (void) { return false; }
// traversal functions provided by opvisit.c in headless build
void opscrollrect (Rect r, long dh, long dv) { (void)r; (void)dh; (void)dv; }

// OSA stubs
boolean osagetcode (Handle htext, OSType idserver, boolean fljustexecutable, tyvaluerecord *vcode) {
    (void)htext; (void)idserver; (void)fljustexecutable; (void)vcode; return false;
}
boolean osagetsource (const tyvaluerecord *osaval, OSType *idserver, tyvaluerecord *vsource) {
    (void)osaval; (void)idserver; (void)vsource; return false;
}

// oplangtext stub
boolean opgetlangtext (hdloutlinerecord ho, boolean fl, Handle *htext) {
    (void)ho; (void)fl; if (htext) *htext = nil; return false;
}

#if !defined(FRONTIER_USE_PORTABLE_HANDLES)
long MaxBlock(void) {
    return 1024L * 1024L; /* arbitrary large block */
}

OSErr MemError(void) {
    return noErr;
}
#endif /* !FRONTIER_USE_PORTABLE_HANDLES */

short GetMBarHeight(void) {
    return 0;
}

GrafPtr GetQDGlobalsThePort(void) {
    return NULL;
}

void SetPort(GrafPtr port) {
    (void)port;
}

void SysBeep(short duration) {
    (void)duration;
}

OSStatus TECCountAvailableTextEncodings(ItemCount *count) {
    if (count)
        *count = 0;
    return noErr;
}

OSStatus TECGetAvailableTextEncodings(TextEncoding encodings[], ItemCount maxCount, ItemCount *actualCount) {
    (void)encodings;
    (void)maxCount;
    if (actualCount)
        *actualCount = 0;
    return noErr;
}

OSStatus TECGetTextEncodingFromInternetName(TextEncoding *outEncoding, const unsigned char *name) {
    (void)name;
    if (outEncoding)
        *outEncoding = kTextEncodingMacRoman;
    return noErr;
}

OSStatus TECGetTextEncodingInfo(TextEncoding encoding, TextEncodingBase *base, TextEncodingVariant *variant, TextEncodingFormat *format) {
    (void)encoding;
    if (base)
        *base = 0;
    if (variant)
        *variant = 0;
    if (format)
        *format = 0;
    return noErr;
}

OSStatus TECGetTextEncodingInternetName(TextEncoding encoding, unsigned char *name) {
    (void)encoding;
    if (name)
        name[0] = '\0';
    return noErr;
}

OSStatus GetTextEncodingName(TextEncoding encoding, TextEncodingNameSelector selector, RegionCode region, TextEncoding referenceEncoding, ItemCount maxLen, unsigned long *actualLen, RegionCode *outRegion, TextEncoding *outEncoding, unsigned char *outName) {
    (void)encoding;
    (void)selector;
    (void)region;
    (void)referenceEncoding;
    (void)maxLen;
    if (actualLen)
        *actualLen = 0;
    if (outRegion)
        *outRegion = 0;
    if (outEncoding)
        *outEncoding = kTextEncodingMacRoman;
    if (outName)
        outName[0] = '\0';
    return noErr;
}

OSStatus TECCreateConverter(TECObjectRef *converter, TextEncoding inputEncoding, TextEncoding outputEncoding) {
    (void)inputEncoding;
    (void)outputEncoding;
    if (converter)
        *converter = (TECObjectRef)0x1;
    return noErr;
}

OSStatus TECDisposeConverter(TECObjectRef converter) {
    (void)converter;
    return noErr;
}

OSStatus TECConvertText(TECObjectRef converter, ConstTextPtr inputBuf, ByteCount inputLen, ByteCount *inputRead, TextPtr outputBuf, ByteCount outputLen, ByteCount *outputProduced) {
    (void)converter;
    if (inputRead)
        *inputRead = inputLen;
    if (outputBuf && outputLen > 0 && inputBuf) {
        ByteCount toCopy = inputLen < outputLen ? inputLen : outputLen;
        memcpy(outputBuf, inputBuf, toCopy);
        if (outputProduced)
            *outputProduced = toCopy;
        return (toCopy == inputLen) ? noErr : kTECPartialCharErr;
    }
    if (outputProduced)
        *outputProduced = 0;
    return noErr;
}

OSStatus TECFlushText(TECObjectRef converter, TextPtr outputBuf, ByteCount outputLen, ByteCount *outputProduced) {
    (void)converter;
    (void)outputBuf;
    (void)outputLen;
    if (outputProduced)
        *outputProduced = 0;
    return noErr;
}

Boolean macfilespecisvalid(const ptrfilespec fs) {
    (void)fs;
    return false;
}

Boolean filespectopath(const ptrfilespec fs, bigstring path) {
    (void)fs;
    setstringlength(path, 0);
    return false;
}

#ifndef HEADLESS_TEST_PORTABLE_FILE
Boolean getfsfile(const ptrfilespec fs, bigstring name) {
    (void)fs;
    setstringlength(name, 0);
    return false;
}
#endif

void NumToString(long value, Str255 result) {
    char buffer[256];
    int written = snprintf(buffer, sizeof buffer, "%ld", value);
    if (written < 0)
        written = 0;
    if (written > 255)
        written = 255;
    result[0] = (unsigned char) written;
    for (int i = 0; i < written; ++i)
        result[i + 1] = (unsigned char) buffer[i];
}

void StringToNum(ConstStr255Param str, long *value) {
    char buffer[256];
    short len = str ? str[0] : 0;
    if (len > 255)
        len = 255;
    for (short i = 0; i < len; ++i)
        buffer[i] = (char) str[i + 1];
    buffer[len] = '\0';
    if (value)
        *value = (long) strtol(buffer, NULL, 10);
}

Handle GetString(short resID) {
    (void)resID;
    return NULL;
}

OSStatus AEProcessAppleEvent(const EventRecord *event) {
    (void)event;
    return noErr;
}

OSStatus AECoerceDesc(const AEDesc *desc, DescType typeCode, AEDesc *result) {
    if (!result)
        return paramErr;
    result->descriptorType = typeCode;
    result->dataHandle = NULL;
    if (desc && desc->dataHandle)
        result->dataHandle = desc->dataHandle;
    return noErr;
}

OSStatus AEDisposeDesc(AEDesc *desc) {
    if (!desc)
        return paramErr;
    desc->descriptorType = typeNull;
    desc->dataHandle = NULL;
    return noErr;
}

void dtox80(const double *value, extended80 *out) {
    if (!value || !out)
        return;
    memset(out->bytes, 0, sizeof out->bytes);
    memcpy(out->bytes, value, sizeof(double) < sizeof out->bytes ? sizeof(double) : sizeof out->bytes);
}

double x80tod(const extended80 *value) {
    double out = 0.0;
    if (!value)
        return 0.0;
    memcpy(&out, value->bytes, sizeof(double) < sizeof value->bytes ? sizeof(double) : sizeof value->bytes);
    return out;
}

short FixRound(Fixed value) {
    return (short)((value + 0x00008000L) >> 16);
}

Fixed FixRatio(long numer, long denom) {
    if (denom == 0)
        return 0;
    return (Fixed)((((int64_t)numer) << 16) / denom);
}

Fixed FixMul(Fixed a, Fixed b) {
    return (Fixed)(((int64_t)a * (int64_t)b) >> 16);
}

void DebugStr(const unsigned char *pascalString) {
    (void)pascalString;
}

void Debugger(void) {
}

OSStatus FSNewAlias(const void *fromFile, const FSRef *target, AliasHandle *result) {
    (void)fromFile;
    (void)target;
    if (result)
        *result = NULL;
    return noErr;
}

OSStatus FSNewAliasMinimal(const FSRef *target, AliasHandle *result) {
    (void)target;
    if (result)
        *result = NULL;
    return noErr;
}

OSStatus FSNewAliasMinimalUnicode(const FSRef *target, UniCharCount nameLength, const UniChar *name, AliasHandle *result, const FSRef *base) {
    (void)target;
    (void)nameLength;
    (void)name;
    (void)result;
    (void)base;
    return noErr;
}

OSStatus FSNewAliasUnicode(const void *fromFile, const FSRef *target, UniCharCount nameLength, const UniChar *name, AliasHandle *result, const FSRef *base) {
    (void)fromFile;
    (void)target;
    (void)nameLength;
    (void)name;
    (void)result;
    (void)base;
    return noErr;
}

OSStatus NewAliasMinimalFromFullPath(long fullPathLength, const void *fullPath, const void *zone, const void *hints, AliasHandle *alias) {
    (void)fullPathLength;
    (void)fullPath;
    (void)zone;
    (void)hints;
    if (alias)
        *alias = NULL;
    return noErr;
}

OSStatus FSFollowFinderAlias(const void *fromFile, AliasHandle alias, Boolean logon, FSRef *target, Boolean *changed) {
    (void)fromFile;
    (void)alias;
    (void)logon;
    if (target)
        memset(target, 0, sizeof *target);
    if (changed)
        *changed = false;
    return noErr;
}

OSStatus FSUpdateAlias(const void *fromFile, const FSRef *target, AliasHandle alias, Boolean *changed) {
    (void)fromFile;
    (void)target;
    (void)alias;
    if (changed)
        *changed = false;
    return noErr;
}

OSStatus FSResolveAliasWithMountFlags(const void *fromFile, AliasHandle alias, FSRef *target, Boolean *changed, uint32_t mountFlags) {
    (void)fromFile;
    (void)alias;
    (void)mountFlags;
    if (target)
        memset(target, 0, sizeof *target);
    if (changed)
        *changed = false;
    return noErr;
}

OSStatus FSCopyAliasInfo(AliasHandle alias, HFSUniStr255 *name, HFSUniStr255 *volumeName, void *info1, FSAliasInfoBitmap *whichInfo, void *info2) {
    (void)alias;
    (void)info1;
    (void)info2;
    if (name)
        name->length = 0;
    if (volumeName)
        volumeName->length = 0;
    if (whichInfo)
        *whichInfo = kFSAliasInfoNone;
    return noErr;
}

OSErr GetAliasInfo(AliasHandle alias, AliasInfoType index, Str255 info) {
    (void)alias;
    (void)index;
    if (info)
        setstringlength(info, 0);
    return noErr;
}

OSErr AEGetKeyPtr(const AppleEvent *event, AEKeyword keyword, DescType desiredType, AEKeyword *actualType, void *dataPtr, Size maximumSize, Size *actualSize) {
    (void)event;
    (void)keyword;
    (void)desiredType;
    (void)dataPtr;
    (void)maximumSize;
    if (actualType)
        *actualType = typeNull;
    if (actualSize)
        *actualSize = 0;
    return errAEEventNotHandled;
}

OSErr AEGetKeyDesc(const AppleEvent *event, AEKeyword keyword, DescType desiredType, AEDesc *result) {
    (void)event;
    (void)keyword;
    (void)desiredType;
    if (result) {
        result->descriptorType = typeNull;
        result->dataHandle = NULL;
    }
    return errAEEventNotHandled;
}

OSErr AEDuplicateDesc(const AEDesc *src, AEDesc *dst) {
    if (!dst)
        return paramErr;
    if (src) {
        dst->descriptorType = src->descriptorType;
        dst->dataHandle = src->dataHandle;
    } else {
        dst->descriptorType = typeNull;
        dst->dataHandle = NULL;
    }
    return noErr;
}

OSErr AECreateList(const void *factoringPtr, Size elementSize, Boolean isRecord, AEDesc *resultList) {
    (void)factoringPtr;
    (void)elementSize;
    (void)isRecord;
    if (resultList) {
        resultList->descriptorType = typeAEList;
        resultList->dataHandle = NULL;
    }
    return noErr;
}

OSErr AEPutDesc(AEDesc *theAERecord, long index, const AEDesc *theAEDesc) {
    (void)theAERecord;
    (void)index;
    (void)theAEDesc;
    return noErr;
}

OSErr CreateCompDescriptor(DescType operatorKeyword, const AEDesc *object1, const AEDesc *object2, Boolean disposeInputs, AEDesc *result) {
    (void)operatorKeyword;
    (void)object1;
    (void)object2;
    (void)disposeInputs;
    if (result) {
        result->descriptorType = typeCompDescriptor;
        result->dataHandle = NULL;
    }
    return noErr;
}

OSErr CreateLogicalDescriptor(const AEDescList *theList, DescType operatorKeyword, Boolean disposeInputs, AEDesc *result) {
    (void)theList;
    (void)operatorKeyword;
    (void)disposeInputs;
    if (result) {
        result->descriptorType = typeLogicalDescriptor;
        result->dataHandle = NULL;
    }
    return noErr;
}

OSErr CreateRangeDescriptor(const AEDesc *startDescriptor, const AEDesc *stopDescriptor, Boolean disposeInputs, AEDesc *result) {
    (void)startDescriptor;
    (void)stopDescriptor;
    (void)disposeInputs;
    if (result) {
        result->descriptorType = typeRangeDescriptor;
        result->dataHandle = NULL;
    }
    return noErr;
}

OSStatus FSGetResourceForkName(HFSUniStr255 *name) {
    if (name)
        name->length = 0;
    return noErr;
}

OSStatus FSOpenFork(const FSRef *ref, UniCharCount nameLength, const UniChar *name, SInt8 permissions, SInt16 *forkRef) {
    (void)ref;
    (void)nameLength;
    (void)name;
    (void)permissions;
    if (forkRef)
        *forkRef = -1;
    return fnfErr;
}

OSErr AECreateDesc(DescType typeCode, const void *dataPtr, Size dataSize, AEDesc *result) {
    (void)dataPtr;
    (void)dataSize;
    if (!result)
        return paramErr;
    result->descriptorType = typeCode;
    result->dataHandle = NULL;
    return noErr;
}

OSErr CreateObjSpecifier(DescType desiredClass, const AEDesc *container, DescType keyForm, const AEDesc *keyData, Boolean createIfNeeded, AEDesc *result) {
    (void)desiredClass;
    (void)container;
    (void)keyForm;
    (void)keyData;
    (void)createIfNeeded;
    if (!result)
        return paramErr;
    result->descriptorType = typeObjectSpecifier;
    result->dataHandle = NULL;
    return errAEEventNotHandled;
}

OSErr AECountItems(const AEDescList *list, long *count) {
    (void)list;
    if (count)
        *count = 0;
    return errAEEventNotHandled;
}

OSErr AEGetNthDesc(const AEDescList *list, long index, DescType desiredType, AEKeyword *theKeyword, AEDesc *result) {
    (void)list;
    (void)index;
    (void)desiredType;
    if (theKeyword)
        *theKeyword = typeNull;
    if (result) {
        result->descriptorType = typeNull;
        result->dataHandle = NULL;
    }
    return errAEEventNotHandled;
}

void Microseconds(UnsignedWide *result) {
    static uint64_t counter = 0;
    counter += 100;
    if (result) {
        result->hi = (uint32_t)(counter >> 32);
        result->lo = (uint32_t)(counter & 0xffffffffu);
    }
}

UInt32 TickCount(void) {
    UnsignedWide wide;
    Microseconds(&wide);
    return (UInt32)((wide.hi << 16) ^ wide.lo);
}

long FreeMem(void) {
    return 8L * 1024L * 1024L;
}

CGrafPtr GetWindowPort(WindowPtr window) {
    (void)window;
    return NULL;
}

boolean shellsetwindowchanges (hdlwindowinfo hinfo, boolean fldirty) {
    (void) hinfo;
    (void) fldirty;
    return false;
}

boolean windowgetpath (WindowPtr w, bigstring bs) {
    (void) w;
    setemptystring (bs);
    return false;
}

boolean windowgetfspec (WindowPtr w, ptrfilespec fs) {
    (void) w;
    if (fs)
        memset (fs, 0, sizeof (*fs));
    return false;
}

short stringpixels (bigstring bs) {
    return (short) (stringlength (bs));
}

boolean timetodatestring (unsigned long ptime, bigstring bs, boolean flabbreviate) {
    (void) ptime;
    (void) flabbreviate;
    setemptystring (bs);
    return false;
}

boolean timetotimestring (unsigned long ptime, bigstring bs, boolean fl) { (void)ptime; (void)fl; setemptystring(bs); return false; }

boolean unixshellcall (Handle hcommand, Handle hreturn) {
    (void) hcommand;
    (void) hreturn;
    return false;
}

boolean statsblockinuse (dbaddress adr, bigstring bsitem) {
    (void) adr;
    if (bsitem)
        setemptystring (bsitem);
    return false;
}

#endif /* FRONTIER_HEADLESS */
