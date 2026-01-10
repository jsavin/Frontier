/* 2025-10-31 Codex: Skip portable handle stubs when FRONTIER_USE_PORTABLE_HANDLES is active. */
/* 2025-11-11 Codex: Remove Paige handler stubs whenever HEADLESS_LINKS_REAL_PAIGE is defined. */
/* 2025-12-02 Codex: Skip the opgetlangtext stub when the headless runtime links the real implementation. */
/* 2025-12-15 Codex: Update TickCount() to use portable time layer. */
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
#include "time_portable.h"
#include "error.h"
#include "claybrowser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef long pg_word;
typedef unsigned char *pg_char_ptr;
typedef struct style_info *style_info_ptr;
typedef struct font_info *font_info_ptr;
typedef struct paige_rec *paige_rec_ptr;

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

/* shellpushdefaultglobals() is now in Common/source/shell_api_headless.c (ADR-006) */

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
#if !defined(FRONTIER_PORTABLE_FILE_AVAILABLE)
boolean oserror (OSErr err) { (void)err; return false; }
boolean pathtofilespec (bigstring bs, ptrfilespec fs) { (void)bs; if (fs) memset(fs,0,sizeof(*fs)); return false; }
#endif
OSStatus pathtofsref (bigstring bs, FSRef *ref) { (void)bs; if (ref) memset(ref,0,sizeof(*ref)); return paramErr; }
#if !defined(FRONTIER_PORTABLE_FILE_AVAILABLE)
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
short opgetlineheight (hdlheadrecord hnode) { (void)hnode; if (op_get_outlinedata()) return (short)((**op_get_outlinedata()).defaultlineheight + 2*textvertinset); return (short)(12 + 2*textvertinset); }
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

#if !defined(FRONTIER_PORTABLE_FILE_AVAILABLE)
// File/FS helpers (implemented in portable/file_portable.c when available)
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
#endif /* !FRONTIER_PORTABLE_FILE_AVAILABLE */

// Timing/keyboard
/* getcurrenttimezonebias now in Common/source/timedate.c with portable implementation */
short getkeyboardstartrepeattime (void) { return 0; }
long getmousedoubleclicktime (void) { return 0; }
boolean keyboardescape (void) { return false; }
void fontgetnumber (bigstring bs, short *num) { (void)bs; if (num) *num=0; }
boolean getmachinename (bigstring bsname) { setemptystring (bsname); return false; }

#if !defined(FRONTIER_PORTABLE_APPLEEVENTS)
// Component/AE stubs
boolean havecomponentmanager (void) { return false; }
boolean newdescnull (AEDesc *desc, DescType type) { (void)type; if (desc) memset(desc,0,sizeof(*desc)); return true; }
boolean newdescwithhandle (AEDesc *desc, DescType type, Handle h) { (void)type; (void)h; if (desc) memset(desc,0,sizeof(*desc)); return true; }
boolean nildatahandle (AEDesc *desc) { (void)desc; return true; }
#endif

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
// langinitbuiltins() now provided by langverbs.c (real implementation with kernel verbs)
// langsettarget() and langcleartarget() now provided by langverbs.c (real implementation)
boolean langfindtargetwindow (short id, WindowPtr *w) { (void)id; if (w) *w=NULL; return false; }
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
/* bigstringtofsname already provided earlier */
boolean datahandletostring (AEDesc* desc, bigstring bs) { (void)desc; setemptystring(bs); return false; }
boolean getscrap (tyscraptype t, Handle h) { (void)t; (void)h; return false; }
/* Removed gettablevalue stub - real implementation is in tableverbs.c */
boolean getuntitledfilename (bigstring bs) { setemptystring(bs); return false; }
boolean getwinparam (hdltreenode node, short id, hdlwindowinfo *hi) { (void)node; (void)id; if (hi) *hi=nil; return false; }
tyshellglobals globalsarray[1];
boolean handlesearch (Handle h, long *ix, long *len) { (void)h; if (ix) *ix=0; if (len) *len=0; return false; }
void initbeachball (tydirection dir) { (void)dir; }
boolean isfilewindow (WindowPtr w) { (void)w; return false; }
boolean ismouserightclick (void) { return false; }
void killundo (void) { }
void rollbeachball (void) { }
/* secondstodatetime and secondstodayofweek now in Common/source/timedate.c with portable implementations */
void setfserrorparam ( const ptrfilespec fs ) { (void)fs; }
boolean setoserrorparam (bigstring bs) { (void)bs; return false; }
void scriptsetcallbacks (hdloutlinerecord ho) { (void)ho; }
boolean shellfindcallbacks (short id, short *ix) { (void)id; if (ix) *ix=0; return false; }
boolean browsergetrefcon (hdlheadrecord hnode, tybrowserinfo *info) { (void)hnode; if (info) memset(info,0,sizeof(*info)); return false; }
boolean memoryerror (void) { return false; }
boolean notifyuser (bigstring bs) { (void)bs; /* no UI notifications in headless */ return true; }


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

#if !defined(HEADLESS_USE_REAL_OPLANGTEXT)
// oplangtext stub
boolean opgetlangtext (hdloutlinerecord ho, boolean fl, Handle *htext) {
    (void)ho; (void)fl; if (htext) *htext = nil; return false;
}
#endif

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

void SysBeep(short duration) {
    (void)duration;
}

#if !defined(FRONTIER_PORTABLE_STRINGS)
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
#endif /* !FRONTIER_PORTABLE_STRINGS */

Boolean macfilespecisvalid(const ptrfilespec fs) {
    (void)fs;
    return false;
}

#if !defined(FRONTIER_PORTABLE_FILE_AVAILABLE)
Boolean filespectopath(const ptrfilespec fs, bigstring path) {
    (void)fs;
    setstringlength(path, 0);
    return false;
}

Boolean getfsfile(const ptrfilespec fs, bigstring name) {
    (void)fs;
    setstringlength(name, 0);
    return false;
}
#endif /* !FRONTIER_PORTABLE_FILE_AVAILABLE */

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

#if !defined(FRONTIER_PORTABLE_APPLEEVENTS)
static void appleevent_clear_desc(AEDesc *desc) {
    if (!desc)
        return;
    desc->descriptorType = typeNull;
    desc->dataHandle = NULL;
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

#endif /* !FRONTIER_PORTABLE_APPLEEVENTS */

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

#if !defined(OS_PORTABLE_HAS_FIXMATH)
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
#endif

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

#if !defined(FRONTIER_PORTABLE_APPLEEVENTS)

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

#endif /* !FRONTIER_PORTABLE_APPLEEVENTS */

void Microseconds(UnsignedWide *result) {
    // 2025-12-15 Codex: Use portable monotonic time layer
    uint64_t us = frontier_time_monotonic_micros();
    if (result) {
        result->hi = (uint32_t)(us >> 32);
        result->lo = (uint32_t)(us & 0xffffffffu);
    }
}

UInt32 TickCount(void) {
    // 2025-12-15 Codex: Use portable monotonic time layer
    // Legacy Mac ticks are 1/60th second intervals
    uint64_t ms = frontier_time_monotonic_millis();
    // Convert milliseconds to 60ths of a second: ms * 60 / 1000 = ms * 3 / 50
    return (UInt32)((ms * 3ULL) / 50ULL);
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

/* timetodatestring and timetotimestring now in Common/source/timedate.c with portable implementations */

/* unixshellcall stub removed - use real implementation from ../Common/source/sysshellcall.c when needed */
#if 0
boolean unixshellcall (Handle hcommand, Handle hreturn) {
    (void) hcommand;
    (void) hreturn;
    return false;
}
#endif

boolean statsblockinuse (dbaddress adr, bigstring bsitem) {
    (void) adr;
    if (bsitem)
        setemptystring (bsitem);
    return false;
}

pg_word pgCharClassProc(paige_rec_ptr pg, pg_char_ptr the_char, short charsize,
        style_info_ptr style, font_info_ptr font) {
    (void)pg;
    (void)the_char;
    (void)charsize;
    (void)style;
    (void)font;
    return 0;
}

typedef struct style_walk *style_walk_ptr;
typedef struct draw_points *draw_points_ptr;
typedef void *format_ref;
typedef struct pg_globals *pg_globals_ptr;
typedef short pg_short_t;
typedef struct t_select *t_select_ptr;
typedef void *shape_ref;
typedef struct co_ordinate *co_ordinate_ptr;
typedef unsigned char pg_boolean;
typedef unsigned char pg_bits8;
typedef pg_bits8 *pg_bits8_ptr;
typedef void *text_ref;
typedef struct point_start *point_start_ptr;
typedef short pg_error;
typedef void *file_ref;
typedef short pg_file_key;
typedef void *memory_ref;
typedef struct select_pair *select_pair_ptr;
typedef void *graf_device_ptr;
typedef void *color_value_ptr;
typedef void *tab_stop_ptr;

extern pg_error pgScrapMemoryRead(void *data, short verb, size_t *position,
        size_t *data_size, file_ref filemap);
extern pg_error pgScrapMemoryWrite(void *data, short verb, size_t *position,
        size_t *data_size, file_ref filemap);
extern size_t pgUnicodeToBytes(pg_short_t *input_chars, pg_bits8_ptr output_bytes,
        font_info_ptr font, size_t input_char_size);

long pgCharInfoProc(paige_rec_ptr pg, style_walk_ptr style_walker, pg_char_ptr data,
        size_t block_offset, size_t offset_begin, size_t offset_end, size_t char_offset,
        long mask_bits) {
    (void)pg;
    (void)style_walker;
    (void)data;
    (void)block_offset;
    (void)offset_begin;
    (void)offset_end;
    (void)char_offset;
    (void)mask_bits;
    return 0;
}

void pgDeleteStyleProc(paige_rec_ptr pg, pg_globals_ptr globals,
        format_ref all_styles, style_info_ptr style) {
    (void)pg;
    (void)globals;
    (void)all_styles;
    (void)style;
}

void pgSaveStyleProc(paige_rec_ptr pg, style_info_ptr style_to_save) {
    (void)pg;
    (void)style_to_save;
}

#if !defined(HEADLESS_LINKS_REAL_PAIGE)
void pgSetGrafDevice(paige_rec_ptr pg, short verb, graf_device_ptr device,
        color_value_ptr bk_color) {
    (void)pg;
    (void)verb;
    (void)device;
    (void)bk_color;
}
#endif

void pgTabDrawProc(paige_rec_ptr pg, style_walk_ptr walker, tab_stop_ptr tab,
        draw_points_ptr draw_position) {
    (void)pg;
    (void)walker;
    (void)tab;
    (void)draw_position;
}

void pgDrawCursorProc(paige_rec_ptr pg, t_select_ptr select, short verb) {
    (void)pg;
    (void)select;
    (void)verb;
}

void pgDrawHiliteProc(paige_rec_ptr pg, shape_ref rgn) {
    (void)pg;
    (void)rgn;
}

void pgDrawProc(paige_rec_ptr pg, style_walk_ptr walker, pg_char_ptr data,
        pg_short_t offset, pg_short_t length, draw_points_ptr draw_position,
        long extra, short draw_mode) {
    (void)pg;
    (void)walker;
    (void)data;
    (void)offset;
    (void)length;
    (void)draw_position;
    (void)extra;
    (void)draw_mode;
}

void pgDupStyleProc(paige_rec_ptr src_pg, paige_rec_ptr target_pg, short reason_verb,
        format_ref all_styles, style_info_ptr style) {
    (void)src_pg;
    (void)target_pg;
    (void)reason_verb;
    (void)all_styles;
    (void)style;
}

void pgIdleProc(paige_rec_ptr pg, short verb) {
    (void)pg;
    (void)verb;
}

void pgInitFont(paige_rec_ptr pg, font_info_ptr info) {
    (void)pg;
    (void)info;
}

void pgStyleInitProc(paige_rec_ptr pg, style_info_ptr style, font_info_ptr font) {
    (void)pg;
    (void)style;
    (void)font;
}

void pgInstallFont(paige_rec_ptr pg, style_info_ptr the_style,
        font_info_ptr the_font, style_info_ptr composite_style,
        short style_overlay, pg_boolean include_offscreen) {
    (void)pg;
    (void)the_style;
    (void)the_font;
    (void)composite_style;
    (void)style_overlay;
    (void)include_offscreen;
}

void pgSpecialCharProc(paige_rec_ptr pg, style_walk_ptr walker, pg_char_ptr data,
        pg_short_t offset, pg_short_t length, draw_points_ptr draw_position,
        long extra, short draw_mode) {
    (void)pg;
    (void)walker;
    (void)data;
    (void)offset;
    (void)length;
    (void)draw_position;
    (void)extra;
    (void)draw_mode;
}

void pgMeasureProc(paige_rec_ptr pg, style_walk_ptr walker,
        pg_char_ptr data, size_t length, pg_short_t slop, long *positions,
        short *types, short measure_verb, size_t current_offset, pg_boolean scale_widths,
        short call_order) {
    (void)pg;
    (void)walker;
    (void)data;
    (void)length;
    (void)slop;
    (void)positions;
    (void)types;
    (void)measure_verb;
    (void)current_offset;
    (void)scale_widths;
    (void)call_order;
}

short pgInsertQuery(paige_rec_ptr pg, pg_char_ptr the_char, short charsize) {
    (void)pg;
    (void)the_char;
    (void)charsize;
    return 0;
}

#if !defined(HEADLESS_LINKS_REAL_PAIGE)
pg_boolean pgReadHandlerProc(paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
        long *element_info, void *aux_data, size_t *unpacked_size) {
    (void)pg;
    (void)key;
    (void)key_data;
    (void)element_info;
    (void)aux_data;
    (void)unpacked_size;
    return false;
}

pg_boolean pgWriteHandlerProc(paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
        long *element_info, void *aux_data, size_t *unpacked_size) {
    (void)pg;
    (void)key;
    (void)key_data;
    (void)element_info;
    (void)aux_data;
    (void)unpacked_size;
    return false;
}

pg_boolean pgDummyReadHandler(paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
        size_t *element_info, void *aux_data, size_t *unpacked_size) {
    (void)pg;
    (void)key;
    (void)key_data;
    (void)element_info;
    (void)aux_data;
    (void)unpacked_size;
    return false;
}

pg_boolean pgDummyWriteHandler(paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
        size_t *element_info, void *aux_data, size_t *unpacked_size) {
    (void)pg;
    (void)key;
    (void)key_data;
    (void)element_info;
    (void)aux_data;
    (void)unpacked_size;
    return false;
}

#endif /* !HEADLESS_LINKS_REAL_PAIGE */

/* Portable implementation of Mac-specific file operations */

boolean endswithpathsep(bigstring bs) {
	/*
	 * Portable implementation - checks if string ends with path separator.
	 * Mac version checks for ':', POSIX uses '/' (chpathseparator).
	 */
	char ch;

	if (stringlength(bs) == 0)
		return false;

	ch = getstringcharacter(bs, stringlength(bs) - 1);
	return (ch == chpathseparator);
}

boolean filefrompath(bigstring path, bigstring fname) {
	/*
	 * Portable implementation - extracts filename from full path.
	 * Returns everything after the last path separator.
	 *
	 * Example: "/home/user/test.txt" returns "test.txt"
	 * Example: "/usr/local/bin/" returns ""
	 */
	return lastword(path, chpathseparator, fname);
}

void macgetfilespecnameasbigstring(const ptrfilespec fs, bigstring bs) {
	/*
	 * Portable implementation - extracts just the filename from filespec.
	 * Uses filespectopath() which accesses fs->name.unicode directly.
	 *
	 * Note: Despite the name, filespectopath() actually extracts just the
	 * filename, not the full path. See portable/file_portable.c:98-108.
	 */
	if (!filespectopath(fs, bs)) {
		setemptystring(bs);
	}
}


#endif /* FRONTIER_HEADLESS */
