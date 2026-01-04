// Headless language/runtime/link stubs to satisfy runtime_tests
#include "frontier.h"
#include "portable_handles.h"
#include "osincludes_portable.h"

#include "process.h"
#include "threads.h"
#include "fileloop.h"
#include "launch.h"
#include "error.h"
#include "resources.h"
#include "kb.h"
#include "osacomponent.h"
#include "db.h"
#include "timedate.h"
#include "process.h"
#include "strings.h"
#include "BASE64.H"
#include "time_portable.h"

#include <string.h>
#include <time.h>

// Process globals expected by language engine
hdlprocessrecord currentprocess = nil;
unsigned short fldisableyield = 0;

boolean pushprocess (hdlprocessrecord p) { (void)p; return true; }
boolean popprocess (void) { return true; }

long grabthreadglobals (void) { return 0; }
long releasethreadglobals (void) { return 0; }

// DB helpers used during value/pack operations
#if !defined(HEADLESS_LINKS_REAL_DB)
boolean dbpushreleasestack (dbaddress adr, long valtype) { (void)adr; (void)valtype; return true; }
boolean dbrefhandle (dbaddress adr, Handle *h) { (void)adr; if (h) *h = nil; return false; }
/* dbassignhandle, dbcopy, dbpushdatabase, dbpopdatabase, and fldatabasesaveas
 * are provided by headless_mac_compat.c so the symbol definitions stay centralized.
 * When the real database core is linked (HEADLESS_LINKS_REAL_DB), these stubs are
 * omitted to avoid duplicate symbols.
 */
#endif

// Byte order helpers (provide out-of-line versions)
long dolongswap (long x) {
    unsigned long ux = (unsigned long) x;
    ux = ((ux & 0x000000FFUL) << 24) |
         ((ux & 0x0000FF00UL) << 8)  |
         ((ux & 0x00FF0000UL) >> 8)  |
         ((ux & 0xFF000000UL) >> 24);
    return (long) ux;
}

short doshortswap (short x) {
    unsigned short ux = (unsigned short) x;
    ux = (unsigned short) (((ux & 0x00FFu) << 8) | ((ux & 0xFF00u) >> 8));
    return (short) ux;
}

// Apple Event / OSA integration is disabled in headless
boolean isosascriptnode (hdltreenode h, tyvaluerecord *v) { (void)h; (void)v; return false; }
boolean evaluateosascript (const tyvaluerecord *vscript, hdltreenode hnode, bigstring bs, tyvaluerecord *vreturned) {
    (void)vscript; (void)hnode; (void)bs; (void)vreturned; return false; }

// File loop utilities (no iteration in headless tests)
boolean fileinitloop (const ptrfilespec fs, tyfileloopcallback cb, Handle *hstate) {
    (void)fs; (void)cb; if (hstate) *hstate = nil; return false; }
void fileendloop (Handle hstate) { (void)hstate; }
boolean filenextloop (Handle hstate, ptrfilespec outfs, boolean *flfolder) {
    (void)hstate; if (outfs) memset(outfs, 0, sizeof(*outfs)); if (flfolder) *flfolder = false; return false; }

// Error/system messaging shims
OSErr getoserror (void) { return noErr; }
boolean getsystemerrorstring (OSErr err, bigstring bs) { (void)err; setemptystring (bs); return false; }

// String table lookup using generated YAML-based tables
#include "../generated/strings_tables.h"
#include "langinternal.h"
#include "shell.rsrc.h"

#define tablestringlist 165  // from tableinternal.h

boolean getstringlist (short listid, short index, bigstring bs) {
    const strings_table_record *table = NULL;
    const char *table_name = NULL;

    // Map legacy list IDs to table names
    switch (listid) {
        case langerrorlist:  // 257 from langinternal.h
            table_name = "langerrorlist";
            break;
        case tablestringlist:  // 165
            table_name = "tablestringlist";
            break;
        case directionlistnumber:  // 135 from shell.rsrc.h
            table_name = "directionlist";
            break;
        default:
            // Unknown list ID
            setemptystring(bs);
            return false;
    }

    // Find the table
    table = strings_find_table(table_name);
    if (!table) {
        setemptystring(bs);
        return false;
    }

    // Search for matching index
    for (size_t i = 0; i < table->count; i++) {
        if (table->entries[i].index == index) {
            // Found it - copy to Pascal string
            const char *text = table->entries[i].text;
            size_t len = strlen(text);
            if (len > 255) len = 255;  // Pascal string max length
            bs[0] = (unsigned char)len;
            memcpy(bs + 1, text, len);
            return true;
        }
    }

    // Index not found
    setemptystring(bs);
    return false;
}

// Shell event gating
boolean shellblockevents (void) { return true; }
boolean shellpopevents (void) { return true; }
void shellinternalerrormessage (short n) { (void)n; }

// Process/application queries
boolean iscurrentapplication (typrocessid pid) { (void)pid; return true; }

// Keyboard status global
tykeystrokerecord keyboardstatus = {0};

// Misc language helpers (remote/kernel)

// Icon helper
boolean myMoof (short a, long b) { (void)a; (void)b; return false; }

// Time/date helpers
// 2025-12-15 Codex: Time functions moved to Common/source/timedate.c with headless guards
// These functions are no longer needed here as stubs

// Threading helpers referenced by langxml
boolean inmainthread (void) { return true; }
hdlprocessthread getcurrentthread (void) { return nil; }
boolean processsleep (hdlprocessthread t, unsigned long timeout) { (void)t; (void)timeout; return true; }

// Minimal string/handle helpers used by langxml
// NOTE: These are now provided by stringverbs.c (compiled in headless mode)
#if 0
void handlepopleadingchars (Handle htext, byte ch) {
    if (!htext) return;
    long sz = gethandlesize(htext);
    if (sz <= 0) return;
    if ((*(unsigned char*) *htext) == (unsigned char) ch)
        pullfromhandle(htext, 0, 1, (char*) &ch);
}

void handlepoptrailingchars (Handle htext, byte ch) {
    if (!htext) return;
    long sz = gethandlesize(htext);
    if (sz <= 0) return;
    unsigned char last;
    pullfromhandle(htext, sz - 1, 1, (char*) &last);
    if (last == (unsigned char) ch) {
        // Simply shrink by 1
        sethandlesize(htext, sz - 1);
    }
}
#endif

// Minimal base64 helpers: pass-through for headless tests
boolean base64encodehandle (Handle htext, Handle h64, short linelength) {
    (void)linelength;
    if (!htext || !h64) return false;
    sethandlesize(h64, 0);
    return pushhandle(htext, h64);
}

boolean base64decodehandle (Handle h64, Handle htext) {
    if (!h64 || !htext) return false;
    sethandlesize(htext, 0);
    return pushhandle(h64, htext);
}
