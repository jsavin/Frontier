#include "frontier.h"
#include "portable_handles.h"
#include "osincludes_portable.h"
#include "file.h"
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef FRONTIER_HEADLESS

#include <string.h>

WindowPtr shellwindow = NULL;
hdlwindowinfo shellwindowinfo = NULL;
RGBColor whitecolor = {65535, 65535, 65535};

// Minimal shell globals struct to satisfy references
tyshellglobals shellglobals;

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

long GetHandleSize(Handle h) {
    return frontierSize(h);
}

OSErr SetHandleSize(Handle h, long userSize) {
    return frontierReAlloc(h, userSize) ? noErr : memFullErr;
}

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
boolean shellgetglobalwindowrect (hdlwindowinfo hi, Rect *r) { (void)hi; if (r) memset(r,0,sizeof(*r)); return false; }
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
boolean oserror (OSErr err) { (void)err; return false; }
boolean pathtofilespec (bigstring bs, ptrfilespec fs) { (void)bs; if (fs) memset(fs,0,sizeof(*fs)); return false; }
OSStatus pathtofsref (bigstring bs, FSRef *ref) { (void)bs; if (ref) memset(ref,0,sizeof(*ref)); return paramErr; }

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
boolean opscraphook (Handle h) { (void)h; return false; }
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

long MaxBlock(void) {
    return 1024L * 1024L; /* arbitrary large block */
}

OSErr MemError(void) {
    return noErr;
}

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

Boolean getfsfile(const ptrfilespec fs, bigstring name) {
    (void)fs;
    setstringlength(name, 0);
    return false;
}

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

boolean timetotimestring (unsigned long ptime, bigstring bs) {
    (void) ptime;
    setemptystring (bs);
    return false;
}

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
