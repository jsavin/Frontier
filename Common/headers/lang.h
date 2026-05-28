
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#ifndef langinclude
#define langinclude /*so other includes can tell if we've been loaded*/


#ifndef shelltypesinclude

	#ifdef FRONTIER_PORTABLE
		#include "../portable/shelltypes_portable.h"
	#else
		#include "shelltypes.h"
	#endif

#endif

#ifndef dbinclude

	#include "db.h"

#endif

#ifndef TABLE_CONTEXT_INCLUDE

	#include "table_context.h"

#endif

/* Forward declaration for types from tableinternal.h */
struct tytablevariable;


#define idconsthashresource 128
#define idbuiltinhashresource 129
#define idkeywordhashresource 130
#define idshellhashresource 131
#define idfilehashresource 132
#define idstringhashresource 133


typedef enum tytreetype { /*assign values so we can trace with debugger*/
	
	noop = 0,
	
	addop = 1,
	
	subtractop = 2,
	
	multiplyop = 3,
	
	divideop = 4,
	
	modop = 5,
	
	identifierop = 6,
	
	constop = 7,
	
	unaryop = 8,
	
	notop = 9,
	
	assignop = 10,
	
	functionop = 11,
	
	EQop = 12,
	
	NEop = 13,
	
	GTop = 14,
	
	LTop = 15,
	
	GEop = 16,
	
	LEop = 17,
	
	ororop = 18,
	
	andandop = 19,
	
	incrpreop = 20, /* ++x */
	
	incrpostop = 21, /* x++ */
	
	decrpreop = 22, /* --x */
	
	decrpostop = 23, /* x-- */
	
	loopop = 24,
	
	fileloopop = 25,
	
	forloopop = 26,
	
	breakop = 27,
	
	returnop = 28,
	
	bundleop = 29,
	
	ifop = 30,
	
	procop = 31,
	
	localop = 32,
	
	moduleop = 33,
	
	dotop = 34,
	
	arrayop = 35,
	
	addressofop = 36,
	
	dereferenceop = 37,
	
	assignlocalop = 38,
	
	bracketop = 39,
	
	caseop = 40,
	
	caseitemop = 41, 
	
	casebodyop = 42,
	
	kernelop = 43,
	
	continueop = 44,
	
	withop = 45,
	
	fordownloopop = 46,
	
	tryop = 47,
	
	beginswithop = 48,
	
	endswithop = 49,
	
	containsop = 50,
	
	rangeop = 51,
	
	listop = 52,
	
	fieldop = 53,
	
	recordop = 54,
	
	forinloopop = 55,
	
	globalop = 56,
	
	osascriptop = 57,
	
	addvalueop = 58,
	
	subtractvalueop = 59,
	
	multiplyvalueop = 60,
	
	dividevalueop = 61,

	cttreetypes
	} tytreetype;


typedef boolean (*ptrcfunction) (int);


typedef enum tyvaluetype { /*use care -- these are saved on disk inside symbol tables*/

	uninitializedvaluetype = -1,
	
	novaluetype = 0,
	
	charvaluetype = 1,
	
	intvaluetype = 2,
	
	longvaluetype = 3,
	
	oldstringvaluetype = 4,
	
	binaryvaluetype = 5,
	
	booleanvaluetype = 6,
	
	tokenvaluetype = 7,
	
	datevaluetype = 8,
	
	addressvaluetype = 9, 
	
	codevaluetype = 10,
	
	doublevaluetype = 11,
	
	stringvaluetype = 12,
	
	externalvaluetype = 13,
	
	directionvaluetype = 14,
	
	passwordvaluetype = 15,
	
	ostypevaluetype = 16,
	
	unused2valuetype = 17,
	
	pointvaluetype = 18,
	
	rectvaluetype = 19,
	
	patternvaluetype = 20,
	
	rgbvaluetype = 21,
	
	fixedvaluetype = 22,
	
	singlevaluetype = 23,
	
	olddoublevaluetype = 24,
	
	objspecvaluetype = 25,
	
	filespecvaluetype = 26,
	
	aliasvaluetype = 27,
	
	
	enumvaluetype = 28,
	
	listvaluetype = 29,
	
	recordvaluetype = 30,
	
	
	/*
	the following value types, outline - pictvaluetype, are never used directly.
	the value would actually be externalvaluetype; these are for flattening 
	external types into a typevaluetype
	*/
	
	outlinevaluetype,
	
	wordvaluetype,
	
	headvaluetype,
	
	tablevaluetype,
	
	scriptvaluetype,
	
	menuvaluetype,
	
	pictvaluetype,
	
	#ifdef xmlfeatures
		xmlvaluetype,
	#endif

	/*new types must be added at end of list, these get saved on disk*/
	
	ctvaluetypes

	} tyvaluetype;


/*
 * tableLookupMode - Reserved for Phase 2 metadata optimization (Issue #334)
 *
 * TABLE_LOOKUP_METADATA: Check existence only, never load from disk
 * TABLE_LOOKUP_VALUE: Load from disk if needed
 */
typedef enum tableLookupMode {
	TABLE_LOOKUP_METADATA = 0,
	TABLE_LOOKUP_VALUE = 1
	} tableLookupMode;

#pragma pack(push, 2)
typedef struct tydiskvalue {	/*4.0.2b1 dmb*/

	dbaddress adr;
	
	Handle hvalue;
	
	} tydiskvalue, *ptrdiskvalue, **hdldiskvalue;
#pragma pack(pop)


typedef union tyvaluedata {
	
	boolean flvalue;
	
	byte chvalue;
	
	int64_t intvalue;
	
	int64_t longvalue;
	
	int64_t datevalue;
	
	tydirection dirvalue;

	OSType ostypevalue;
	
	Handle stringvalue;
	
	/*
	Handle passwordvalue;
	*/
	
	hdlstring addressvalue;
	
	Handle binaryvalue;
	
	short tokenvalue; /*can be coerced to a tyfunctype, see note in langtokens.h*/
	
	struct tytreenode **codevalue; /*implemented in UserLang*/
	
	Handle externalvalue; /*structure is defined in langexternal.h*/
	
	Point pointvalue;
	
	Rect **rectvalue;
	
	Pattern **patternvalue;
	
	RGBColor **rgbvalue;
	
	Fixed fixedvalue;
	
	float singlevalue;
	
	double **doublevalue;
	
	Handle objspecvalue;
	
	hdlfilespec filespecvalue;
	
	Handle aliasvalue; /*AliasHandle*/
	
	OSType enumvalue;
	
	struct tylistrecord **listvalue;

	struct tylistrecord **recordvalue;
	
	dbaddress diskvalue;	/*4.0.2b1 dmb*/
	} tyvaluedata;


/* Minimum valid heap pointer address. The first 4096 bytes of virtual address
 * space are intentionally unmapped (PROT_NONE) on macOS/Linux. Any non-NULL
 * pointer below this threshold indicates corruption, not a valid allocation.
 * Used by corrupt-handle guards in langhash.c and langvalue.c. */
#define kMinValidPointer ((uintptr_t)0x1000)


typedef struct tyvaluerecord {
	
	tyvaluetype valuetype;
	
	/*flags are never set in the database; used at runtime only*/

	byte fltmpdata: 1; /*set when value data is handle to original, and needs duplication*/
	
	byte fltmpstack: 1; /*set when value data is in the temp stack, and is volatile*/
	
	byte flformalval: 1; /*set when value is set as default by function header*/
	
	byte fldiskval: 1; /*set when value data points to disk block of actual value*/
	
	tyvaluedata data;
	} tyvaluerecord;


typedef struct tytreenode {

	tytreetype nodetype; /*add, subtract, if, etc.*/
	
	tyvaluerecord nodeval; /*the value stored at this level of the tree*/
	
	struct tytreenode **link; /*links parameter lists, statement lists*/
	
	unsigned long lnum; /*which line number in the source was this node generated from?*/

	/*
	PR1 of REPL error context chain (2026-05-26 JES): widened from byte to
	unsigned short so columns past 255 can be reported in error.location
	for long REPL lines. The on-disk format (tydisktreenode.charnum at
	langtree.c:61) is already 16-bit short, packed/unpacked by
	memtodiskshort / disktomemshort, so this is value-preserving with no
	database migration required.
	*/
	unsigned short charnum; /*at what character offset on the line?*/
	
	byte ctparams; /*number of params actually allocated, max is 4*/
	
	/*param1 - param4 must be at the end of the record - see newtreenode*/
	
	struct tytreenode **param1, **param2, **param3, **param4;
	} tytreenode, *ptrtreenode, **hdltreenode;


typedef struct tyhashnode {
	
	struct tyhashnode **hashlink; /*next guy in the same hash bucket*/
	
	struct tyhashnode **sortedlink; /*next guy in alphabetic order*/
	
	
	tyvaluerecord val; /*the value of the identifier*/
	
	byte fldontsave: 1; /*this node doesn't get packed*/
	
	byte fllocked: 1; /*are changes allowed for this value? ***unimplemented */
	
	byte flprotected: 1; /*should the value be displayed as "(protected)"?*/
	
	byte flunresolvedaddress: 1; /*does val contain an unresolved address?*/
	
	byte fldisposewhenunlocked: 1; /*node must be disposed when lock count reaches zero*/
	
	byte flunused: 3; /*round to byte boundry*/
	
	byte ctlocks: 8;
	
	byte hashkey []; /*the identifier of this table element, lives in extension of record*/
	
	} tyhashnode, *ptrhashnode, **hdlhashnode;

	
#define HNoNode ((hdlhashnode) -1)
#define gethashkey(h,bs)	copystring ((**h).hashkey, bs);
	

#define ctbuckets 11 /*should be a prime number?*/


typedef boolean (*langvaluecallback) (short, hdltreenode, tyvaluerecord *, bigstring);


typedef struct tyhashtable {
	
	hdlhashnode hashbucket [ctbuckets];
	
	hdlhashnode hfirstsort; /*the alphabetically-first hash node*/
	
	struct tyhashtable **prevhashtable; /*allow tables to be linked*/
	
	
	struct tyhashtable **parenthashtable; /*allow tables to be linked, but not lexically*/
	

	/* The following is added 12/29/99 by RAB so that we do not have to scan all the buckets of 
	this tables parent to get the hashnode for this table and therefore it's value record and it's name. */

	hdlhashnode thistableshashnode; /*this tables hashnode referenced in it's parents hashbuckets 12/29/99 RAB*/
	
	
	boolean fldirty: 1; /*indicates whether anything has changed*/
	
	boolean fllocked: 1; /*are changes allowed to be made?*/
	
	boolean flwindowopen: 1; /*is this table being displayed in a window?*/
	
	boolean flnopurge: 1; /*can table be purged from memory?*/
	
	boolean fllocaltable: 1; /*does this table contain local variables?*/
	
	boolean flchained: 1; /*is this table in the chain of local hashtables?*/
	
	boolean fldisposewhenunchained: 1; /*table must be disposed when removed from stack chain*/
	
	boolean ctwithvalues: 3; /*how many with statement values does this (local) table contain?*/
	
	boolean flverbsrequirewindow: 1; /*for EPP's, do verbs require a window context?*/
	
	boolean flneedsort: 1; /*for table editing, set if a node is not sorted*/
	
	boolean flmayaffectdisplay: 1; /*has this table ever been displayed in any table window?*/
	
	#ifdef xmlfeatures
		boolean flxml: 1;
	#endif
	
	boolean flsubsdirty: 1; /*has this table got dirty subs -- set in tablepreflightsubsdirtyflag before saving*/

	long hashtablerefcon; /*for use by application*/
	
	long lexicalrefcon;
	
	
	struct tytableformats **hashtableformats; /*place to link in a display formats record*/
	
	
	short sortorder; /*up to the application to understand what this means*/

	unsigned char _pad[6]; /*padding for 8-byte alignment of timecreated*/

	int64_t timecreated, timelastsave; /*number of seconds since 1/1/04*/
	
	langvaluecallback valueroutine; /*for EFP's -- C routine that evaluates verbs*/

	short cttmpstack;

	/* Phase 4A: Context for version tracking and mutation metadata */
	table_context_t *context;

	tyvaluerecord tmpstack []; /*temps generated during expression evaluation*/
	} tyhashtable, *ptrhashtable, **hdlhashtable;

/*
2025-12-05: Verify 8-byte alignment of 64-bit timestamp fields under all packing modes.
Note: pack(2) is applied only to legacy disk structs (see tydiskvalue push/pop above);
this struct remains naturally aligned, and the padding above enforces 8-byte alignment.
If a future packing change breaks this, the static assertions will fail at compile time.
*/
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(offsetof(tyhashtable, timecreated) % 8 == 0, "tyhashtable.timecreated must be 8-byte aligned");
_Static_assert(offsetof(tyhashtable, timelastsave) % 8 == 0, "tyhashtable.timelastsave must be 8-byte aligned");
#endif
	

typedef boolean (*langerrorcallback) (long, long, short, hdlhashtable *, bigstring);


typedef struct tymodulerecord {
	
	hdltreenode parsetree; /*the code tree generated by the parser*/
	
	boolean flhandler; /*is the generated code a handler (true) or standalone (false)*/
	} tymodulerecord;


typedef struct tyaddress { // 5.0.2 dmb (finally)
	
	hdlhashtable ht;
	
	bigstring bs;
	} tyaddress, *ptraddress;


//#define cterrorcallbacks 50 /*we can remember nested error callbacks up to 50 levels deep*/

#define cterrorcallbacks 200 /*6.1d19 AR*/

typedef struct tyerrorrecord {

	langerrorcallback errorcallback;

	unsigned long errorline;

	unsigned short errorchar;

	/*
	PR1 of REPL error context chain (2026-05-26 JES): bracket of the
	token the scanner had most recently consumed when the error fired.
	Snapshotted by langseterrorcallbackline from the lasttoken* globals
	maintained by langscanner. Both are columns (zero-origin character
	offsets) on errorline. When no token bracket is available, both
	default to zero.
	*/
	unsigned short tokenstart;

	unsigned short tokenend;


	unsigned long profilebase;

	unsigned long profiletotal;


	long errorrefcon;
	} tyerrorrecord;


typedef struct tyerrorstack {
	
	short toperror;
	
	tyerrorrecord stack [cterrorcallbacks];
	} tyerrorstack, *ptrerrorstack, **hdlerrorstack;


typedef boolean (*langvoidcallback) (void);

typedef boolean (*langbooleancallback) (boolean);

typedef boolean (*langtreenodecallback) (hdltreenode);

typedef boolean (*langtreenodescallback) (hdltreenode, hdltreenode);

typedef boolean (*langaddresscallback) (hdlhashtable, const bigstring);

typedef boolean (*langsymbolinsertedcallback) (hdlhashtable, const bigstring, hdlhashnode);

typedef boolean (*langtablenodecallback) (hdlhashtable, hdlhashnode);

typedef boolean (*langsymbolchangedcallback) (hdlhashtable, const bigstring, hdlhashnode, boolean);

typedef short (*langcomparenodescallback)(hdlhashtable, hdlhashnode, hdlhashnode);

typedef boolean (*langhashnodetreenodecallback)(hdlhashnode, hdltreenode *);

typedef boolean (*langtablerefcallback)(hdlhashtable *);

typedef boolean (*langtablecallback)(hdlhashtable);

typedef boolean (*langsourcecodecallback)(hdlhashtable, hdlhashnode, bigstring);

typedef boolean (*langerrormessagecallback)(bigstring, ptrvoid);

typedef boolean (*langverbcallback) (hdltreenode, tyvaluerecord *);

typedef boolean (*langshortcallback) (UInt16);

typedef boolean (*langeventcallback) (EventRecord *);

typedef boolean (*langtablevisitcallback) (hdlhashnode, ptrvoid);

typedef boolean (*langinversesearchcallback) (bigstring, hdlhashnode, tyvaluerecord, ptrvoid);

typedef boolean (*langsortedinversesearchcallback) (bigstring, hdlhashnode, tyvaluerecord, ptrvoid);


typedef struct tylangcallbacks {
	
	langsymbolchangedcallback symbolchangedcallback;
	
	langtablenodecallback symbolunlinkingcallback;
	
	langaddresscallback symboldeletedcallback;
	
	langsymbolinsertedcallback symbolinsertedcallback;
	
	langcomparenodescallback comparenodescallback;
	
	langtreenodecallback debuggercallback;
	
	langbooleancallback scriptkilledcallback;
	
	langhashnodetreenodecallback scriptcompilecallback;
	
	langbooleancallback backgroundtaskcallback;
	
	langtablerefcallback pushtablecallback;
	
	langtablecallback poptablecallback;
	
	langsourcecodecallback pushsourcecodecallback;
	
	langvoidcallback popsourcecodecallback;
	
	langvoidcallback saveglobalscallback;
	
	langvoidcallback restoreglobalscallback;
	
	langerrormessagecallback errormessagecallback;
	
	ptrvoid errormessagerefcon;
	
	langvoidcallback clearerrorcallback;
	
	langverbcallback msgverbcallback;
	
	langtreenodescallback codereplacedcallback;
	
	langverbcallback idvaluecallback;
	
	hdlerrorstack scripterrorstack;
	
	langshortcallback partialeventloopcallback;
	
	langeventcallback processeventcallback;
	
	
	langerrormessagecallback debugerrormessagecallback;
	
	
	
	callback presystemdialogcallback;
	
	callback postsystemdialogcallback;
	
	
	} tylangcallbacks;


//#define cthashtables 20 /*we can remember nested hash tables pushed up to 20 levels deep*/

#define cthashtables 80 /*6.1d19 AR*/

typedef struct tablestack {
	
	short toptables;
	
	hdlhashtable stack [cthashtables];
	} tytablestack, *ptrtablestack, **hdltablestack;
/*globals*/	

extern boolean flscriptrunning; /*if true, a script is currently executing in this thread*/

extern boolean flscriptresting; /*if true, a script doesn't want to tie up the processor*/

extern hdltreenode herrornode; /*for the debugger, node where error occurred*/


extern byte bstrue [];

extern byte bsfalse [];

/* currenthashtable is now a thread-local macro defined in processinternal.h (ADR-005) */

extern hdltablestack hashtablestack;

/* ADR-005: Thread-local parameter and value protection state */
/* ADR-009: Thread-local hash table stack */
/* Macros defined in processinternal.h after tythreadglobals structure */


extern hdlhashtable hkeywordtable; /*holds the language's keywords*/

extern hdlhashtable hconsttable; /*holds the names of built-in constants, like true and up*/

extern hdlhashtable hbuiltinfunctions; /*holds the names of the built-in functions*/

extern unsigned long ctscanlines; /*number of lines that have been scanned, for error reporting*/

extern unsigned short ctscanchars; /*number of chars passed over on current line, for error reporting*/

/*
PR1 of REPL error context chain (2026-05-26 JES): per-token snapshot of
the most recently scanned token's location. Captured by langscanner;
read by langseterrorcallbackline. See Common/source/langscan.c for the
capture machinery and Common/source/lang.c for the snapshot.
*/
extern unsigned long lasttokenline;

extern unsigned short lasttokenstart;

extern unsigned short lasttokenend;

extern tylangcallbacks langcallbacks; /*routines that wire the language into environment*/


/*prototypes*/

extern boolean langpusherrorcallback (langerrorcallback, long); /*lang.c*/

extern boolean langpoperrorcallback (void);

extern boolean langseterrorcallbackline (void);

extern boolean langstartprofiling (void);

extern boolean langstopprofiling (void);

extern unsigned long langgetsourceoffset (unsigned long, unsigned short);

extern void langsetsourceoffset (unsigned long);

extern boolean langfinderrorrefcon (long, langerrorcallback *);

/*
PR1 of REPL error context chain (2026-05-26 JES): snapshot/read API for
the error stack at the moment the last error fired. langseterrorcallbackline
now copies the entire stack into a file-static buffer; these accessors
expose top-of-stack and per-frame for protocol callers building structured
error responses. langsetevalinputoffset lets the REPL wrapper subtract its
prefix lines so user-relative line numbers are reported. See lang.c for
implementation.
*/
extern boolean langgetlasterror (tyerrorrecord *out);

extern boolean langgetstackframe (short ix, tyerrorrecord *out, long *outRefcon);

extern short langgetstackdepth (void);

/*
PR3 of REPL error context chain (2026-05-27 JES): causedby snapshot
accessors. Symmetric with langgetlasterror / langgetstackframe /
langgetstackdepth but read from a SEPARATE snapshot captured when an
error fires inside a try block. The protocol layer attaches this as
error.causedBy when an else-block re-failure is being reported; the
REPL renderer emits a "Caused by (line N):" header below the primary
error's context window.

langsetintryblock is called by evaluatetry around the try body.
langclearcausedbyerror is called by evaluatetry when a try block
completes without invoking the else path. See lang.c for the full
state-machine description.
*/
extern boolean langgetcausedbyerror (tyerrorrecord *out);

extern boolean langgetcausedbystackframe (short ix, tyerrorrecord *out, long *outRefcon);

extern short langgetcausedbystackdepth (void);

extern const char *langgetcausedbymessage (void);

/*
Shared script-name-from-refcon helper. Used by op_handler.c (protocol
error responses), repl_output.c (REPL stack rendering), and langevaluate.c
(tryErrorScript assignment). Refcon sentinels: 0L -> "<eval>",
-1L -> "<eval-inner>", nil/HNoNode -> "<unknown>"; otherwise the
hashnode's hashkey is copied into `out`. See lang.c for full notes.
*/
extern void langscriptnamefromrefcon (long refcon, bigstring out);

extern void langtrysetcausedbymessage (const bigstring bs);

extern void langsetintryblock (boolean fl);

extern void langclearcausedbyerror (void);

/*
PR3 P1 (concurrency + security, 2026-05-27 JES): snapshot of the
in-try-body depth + causedby state, captured on thread context switch
(pushprocess) and restored on the way back (popprocess). The struct
contents are owned by lang.c -- process.c stores a value of this type
inside typrocessstackrecord and shuttles it through the save/restore
accessors below. Keeping the layout opaque-ish (full struct so the
storage size is known to the compiler, but callers should treat it as
opaque) means process.c does not need to track lang internals.
*/
typedef struct tycausedbysnapshot {
	int trybodydepth;
	short causedbyerrorstackdepth;
	boolean flcausedbyerrorvalid;
	tyerrorrecord causedbyerrorstack [cterrorcallbacks];
	char causedbyerrormessage [256];
	} tycausedbysnapshot;

extern void langsavecausedbysnapshot (tycausedbysnapshot *out);

extern void langrestorecausedbysnapshot (const tycausedbysnapshot *in);

extern void langsetevalinputoffset (unsigned long lineOffset);

extern void langclearevalinputoffset (void);

extern boolean langcompiletext (Handle, boolean, hdltreenode *);

extern boolean langbuildtree (Handle, boolean, hdltreenode *);

extern boolean langdefaultpushtable (hdlhashtable *);

extern boolean langdefaultpoptable (hdlhashtable);

extern boolean langnoop (void);

extern void langhookerrors (void);

extern void langunhookerrors (void);

extern void langtraperrors (bigstring, langerrormessagecallback *, ptrvoid *);

extern void languntraperrors (langerrormessagecallback, ptrvoid, boolean);

extern void langpushscopechain (void);

extern void langpopscopechain (void);

extern boolean langopruncallbackscripts (short);

extern boolean langruncallbackwithparams (hdlhashtable, bigstring, short, tyvaluerecord *, tyvaluerecord *);

extern boolean langzoomobject (const bigstring);

extern boolean langbuildparamlist (tyvaluerecord *, hdltreenode *); //6.1b7 AR

extern boolean langrun (Handle, tyvaluerecord *);

extern boolean langruntraperror (Handle, tyvaluerecord *, bigstring);

extern boolean langrunhandle (Handle, bigstring);

extern boolean langrunhandle_value (Handle, tyvaluerecord *);

extern boolean langrunhandletraperror (Handle, bigstring, bigstring);

extern boolean langrunstring (const bigstring, bigstring);

extern boolean langrunstringnoerror (const bigstring, bigstring);

extern boolean langreduceformula (bigstring);

extern boolean langruncode (hdltreenode, hdlhashtable, tyvaluerecord *);

extern boolean langaddkeyword (bigstring, short);

extern boolean langaddkeywordlist (hdlhashtable, byte *[], short);

extern boolean langinitverbs (void);

extern boolean initlang (void);

#ifdef FRONTIER_HEADLESS
extern boolean langinitresources_headless (void); /*langstartup.c - headless keyword/builtin/const initialization */
#endif

extern boolean langcompilescript (hdlhashnode, hdltreenode *); /*langcallbacks.c*/

extern boolean langerrorclear (void);

extern void langsymbolchanged (hdlhashtable, const bigstring, hdlhashnode, boolean);

extern void langsymbolinserted (hdlhashtable, const bigstring, hdlhashnode);

extern void langsymbolunlinking (hdlhashtable, hdlhashnode);

extern void langsymboldeleted (hdlhashtable, const bigstring);

extern boolean langbackgroundtask (boolean);


extern boolean langpartialeventloop (UInt16);



extern callback langpresystemdialog (void);

extern void langpostsystemdialog (void);



extern boolean datenetstandardstring (int64_t localdate, tyvaluerecord *vreturn); /*langdate.c*/

extern boolean datemonthtostring (long ix, tyvaluerecord *vreturn);

extern boolean datedayofweektostring (long ix, tyvaluerecord *vreturn);

extern boolean dateversionlessthan (bigstring bsv1, bigstring bsv2, tyvaluerecord *v);


extern void disablelangerror (void); /*langerror.c*/

extern void enablelangerror (void);

extern boolean langerrorenabled (void);

extern void disablelangerrorlog (void); /*suppress logging only, not callbacks*/

extern void enablelangerrorlog (void);

extern boolean langerrorlogenabled (void);

extern void langerror (short);

extern void lang3paramerror (short, const bigstring, const bigstring, const bigstring);

extern void langparamerror (short, const bigstring);

extern void lang2paramerror (short, const bigstring, const bigstring);

extern void langlongparamerror (short, long);

extern void langostypeparamerror (short, OSType);


extern boolean langerrordialog (bigstring, ptrvoid); /*langerrorwindow.c*/

extern boolean langerrorflush (void);

extern boolean clearlangerrordialog (void);



extern boolean newhashtable (hdlhashtable *); /*langhash.c*/

extern void dirtyhashtable (hdlhashtable);

extern short hashgetstackdepth (void);

extern void chainhashtable (hdlhashtable);

extern void unchainhashtable (void);

extern boolean indexhashtable (short, hdlhashtable *);

extern hdlhashtable sethashtable (hdlhashtable);

extern boolean pushhashtable (hdlhashtable);

extern boolean pophashtable (void);

extern boolean pushouterlocaltable (void);

extern boolean disposehashnode (hdlhashtable, hdlhashnode, boolean, boolean);

extern short emptyhashtable (hdlhashtable, boolean);

extern boolean disposehashtable (hdlhashtable, boolean);

extern boolean hashinsertnode (hdlhashnode, hdlhashtable);

extern boolean hashunlinknode (hdlhashtable, hdlhashnode);

extern boolean hashsetnodekey (hdlhashtable, hdlhashnode, const bigstring);

extern boolean hashinsert (const bigstring, tyvaluerecord);

//extern hashmerge (hdlhashtable, hdlhashtable);

extern boolean hashlocate (const bigstring, hdlhashnode *, hdlhashnode *);

extern boolean hashunlink (const bigstring, hdlhashnode *);

extern boolean hashdelete (const bigstring, boolean, boolean);

extern boolean hashtabledelete (hdlhashtable, bigstring);

extern boolean hashsymbolexists (const bigstring);

extern boolean hashtablesymbolexists (hdlhashtable, const bigstring);

extern void hashsetlocality (tyvaluerecord *, boolean); /*6.2b16 AR: needed to set locality in langaddlocals [langevaluate.c]*/

extern boolean hashassign (const bigstring, tyvaluerecord);

extern boolean hashtableassign (hdlhashtable, const bigstring, tyvaluerecord);

extern boolean hashlookup (const bigstring, tyvaluerecord *, hdlhashnode *);

extern boolean hashtablelookup (hdlhashtable, const bigstring, tyvaluerecord *, hdlhashnode *);

extern boolean hashlookupnode (const bigstring, hdlhashnode *);

extern boolean hashtablelookupnode (hdlhashtable, const bigstring, hdlhashnode *);

extern boolean hashtablevisit (hdlhashtable, langtablevisitcallback, ptrvoid);

extern boolean hashresort (hdlhashtable, hdlhashnode);

extern boolean hashinversesearch (hdlhashtable, langinversesearchcallback, ptrvoid, bigstring);

extern boolean hashsortedinversesearch (hdlhashtable, langsortedinversesearchcallback, ptrvoid);

extern boolean hashnodeintable (hdlhashnode, hdlhashtable);

extern boolean hashpacktable (hdlhashtable, boolean, Handle *, boolean *);

extern boolean hashunpacktable (Handle, boolean, hdlhashtable);

/* Context-aware internal versions (used by db_format layer) */
struct db_context; /* forward declaration */
extern boolean hashpacktable_internal (const struct db_context *, hdlhashtable, boolean, Handle *, boolean *);

extern boolean hashunpacktable_internal (const struct db_context *, Handle, boolean, hdlhashtable);

extern boolean hashcountitems (hdlhashtable, long *);

extern boolean hashsortedsearch (hdlhashtable, const bigstring, long *);

extern boolean hashgetnthnode (hdlhashtable, long, hdlhashnode *);

extern boolean hashgetsortedindex (hdlhashtable, hdlhashnode, long *);

extern boolean hashgetiteminfo (hdlhashtable, long, bigstring, tyvaluerecord *);

extern boolean hashgetvaluestring (tyvaluerecord, bigstring);

extern boolean hashgettypestring (tyvaluerecord, bigstring);

extern boolean hashgetsizestring (const tyvaluerecord *, bigstring);

extern boolean hashvaltostrings (tyvaluerecord, bigstring, bigstring, bigstring);


extern boolean langgettypestring (tyvaluetype, bigstring); /*langops.c*/

extern tyvaluetype langgettype (tyvaluerecord val); /*6.2b5 AR*/

extern boolean langscalartype (tyvaluetype);

extern OSType langgettypeid (tyvaluetype);

extern tyvaluetype langgetvaluetype (OSType);

extern boolean langgoodbinarytype (tyvaluetype);

extern boolean langfindsymbol (const bigstring, hdlhashtable *, hdlhashnode *);

extern boolean langgetsymbolval (const bigstring, tyvaluerecord *, hdlhashnode *);

extern boolean langsetsymbolval (const bigstring, tyvaluerecord);

extern boolean langsetsymboltableval (hdlhashtable, const bigstring, tyvaluerecord);

extern boolean langsetstringval (const bigstring, const bigstring);

extern boolean langsetbinaryval (hdlhashtable, const bigstring, Handle);

extern boolean langassignstringvalue (hdlhashtable, const bigstring, const bigstring);

extern boolean langassignbooleanvalue (hdlhashtable, const bigstring, boolean);

extern boolean langassigncharvalue (hdlhashtable, const bigstring, unsigned char);

extern boolean langassignlongvalue (hdlhashtable, const bigstring, long);

extern boolean langassignaddressvalue (hdlhashtable, const bigstring, const tyaddress *);

extern boolean langassignnewtablevalue (hdlhashtable, const bigstring, hdlhashtable *);

extern boolean langsuretablevalue (hdlhashtable, const bigstring, hdlhashtable *);

extern boolean langcheckstackspace (void);

extern boolean langbuildnamelist (hdltreenode htree, struct tylistrecord **hlist);


extern boolean langpackvalue (tyvaluerecord, Handle *, hdlhashnode); /*langpack.c*/

extern boolean langpackverb (hdltreenode, tyvaluerecord *);

extern boolean langpackwindowverb (hdltreenode, tyvaluerecord *);

extern boolean langunpackvalue (Handle, tyvaluerecord *);

extern boolean langunpackverb (hdltreenode, tyvaluerecord *);

extern boolean langunpackwindowverb (hdltreenode, tyvaluerecord *);

extern boolean langvaluetotextscrap (tyvaluerecord, Handle);


extern boolean langcanusealiases (void);  /*langsystem7.c*/

extern boolean langcanuseappleevents (void);


extern void cleartmpstack (void); /*langtmpstack.c*/

extern boolean pushtmpstackvalue (tyvaluerecord *);

extern boolean pushtmpstack (Handle);

extern void releaseheaptmp (Handle);

extern boolean pushvalueontmpstack (tyvaluerecord *);

extern boolean disposetmpvalue (tyvaluerecord *);

extern boolean exemptfromtmpstack (tyvaluerecord *);


extern boolean langdisposetree (hdltreenode); /*langtree.c*/


extern boolean langsymbolreference (hdlhashtable, bigstring, tyvaluerecord *, hdlhashnode *); /*langvalue.c*/

extern void initvalue (tyvaluerecord *, tyvaluetype);

extern boolean setnilvalue (tyvaluerecord *);

extern boolean setbooleanvalue (boolean, tyvaluerecord *);

extern boolean setcharvalue (byte, tyvaluerecord *);

extern boolean setintvalue (int64_t, tyvaluerecord *);

extern boolean setlongvalue (int64_t, tyvaluerecord *);

extern boolean setdatevalue (int64_t, tyvaluerecord *);

extern boolean setdirectionvalue (tydirection, tyvaluerecord *);

extern boolean setostypevalue (OSType, tyvaluerecord *);

extern boolean setpointvalue (Point, tyvaluerecord *);

extern boolean setfixedvalue (Fixed, tyvaluerecord *);

extern boolean setsinglevalue (float, tyvaluerecord *);

extern boolean newheapvalue (ptrvoid, long, tyvaluetype, tyvaluerecord *);

extern boolean setstringvalue (bigstring, tyvaluerecord *);

extern boolean setaddressvalue (hdlhashtable, const bigstring, tyvaluerecord *);

extern boolean setexemptaddressvalue (hdlhashtable htable, const bigstring bs, tyvaluerecord *val);

extern boolean getaddressvalue (tyvaluerecord, hdlhashtable *, bigstring);

/*
extern boolean setpasswordvalue (bigstring, tyvaluerecord *);
*/

extern boolean setheapvalue (Handle, tyvaluetype, tyvaluerecord *);

extern boolean setbinaryvalue (Handle, OSType, tyvaluerecord *);

extern boolean setbinarytypeid (Handle, OSType);

extern OSType getbinarytypeid (Handle);

extern boolean stripbinarytypeid (Handle);

extern boolean setdoublevalue (double, tyvaluerecord *);

extern boolean setfilespecvalue (ptrfilespec, tyvaluerecord *);

extern boolean setexternalvalue (Handle, tyvaluerecord *);

extern boolean langhashtablelookup (hdlhashtable htable, const bigstring bs, tyvaluerecord *vreturned, hdlhashnode *); /*6.1d4 AR*/
extern boolean langhash_materialize_disk_values(hdlhashtable htable);

extern boolean langassigntextvalue (hdlhashtable ht, bigstring bs, Handle h); /*6.1d4 AR*/

extern boolean langlookupstringvalue (hdlhashtable ht, bigstring bs, bigstring bsval); /*6.1d3 AR*/

extern boolean langlookupaddressvalue (hdlhashtable ht, bigstring bs, tyaddress *addressval); /*6.1d3 AR*/

extern boolean langlookuplongvalue (hdlhashtable ht, bigstring bs, long *x); /*6.1d4 AR*/

extern boolean langlookupbooleanvalue (hdlhashtable ht, bigstring bs, boolean *fl); /*6.1d3 AR*/

extern boolean langtablecopyvalue (hdlhashtable hsource, hdlhashtable hdest, bigstring bs); /*6.1d4 AR*/


extern boolean setwinvalue (WindowPtr pwindow, tyvaluerecord *val);	/*shellwindowverbs.c*/

extern boolean copyvaluedata (tyvaluerecord *);

/* Context-aware value copying */
extern boolean copyvaluerecord_internal(const db_context *ctx, tyvaluerecord v,
                                         tyvaluerecord *vreturned);

extern boolean copyvaluerecord (tyvaluerecord, tyvaluerecord *);

#ifdef DATABASE_DEBUG
	extern void debug_disposevaluerecord (tyvaluerecord, boolean, long, char*);
	#define disposevaluerecord(val, fldisk) \
		debug_disposevaluerecord (val, fldisk, __LINE__, __FILE__)
#else
	extern void disposevaluerecord (tyvaluerecord, boolean);
#endif

extern boolean langgetspecialtable (bigstring, hdlhashtable *);

extern boolean getaddresspath (tyvaluerecord, bigstring);

//extern boolean truevalue (tyvaluerecord); //6.2b15 AR: removed

extern boolean coercetolong (tyvaluerecord *);

extern boolean coercetoint (tyvaluerecord *);

extern boolean coercetorgb (tyvaluerecord *);

extern boolean coercetoostype (tyvaluerecord *);

extern boolean coercetostring (tyvaluerecord *);

extern boolean coercetoaddress (tyvaluerecord *);

extern boolean coercetoboolean (tyvaluerecord *);

extern boolean coercetofilespec (tyvaluerecord *);

extern boolean coercetobinary (tyvaluerecord *);

extern boolean coercevalue (tyvaluerecord *, tyvaluetype);

extern boolean coercetypes (tyvaluerecord *, tyvaluerecord *);

extern void disposevalues (tyvaluerecord *, tyvaluerecord *);

extern boolean langgetidentifier (hdltreenode, bigstring);

extern boolean langtablelookup (hdlhashtable, bigstring, hdlhashtable *);

extern boolean langsearchpathlookup (bigstring, hdlhashtable *);

extern boolean langexpandtodotparams (bigstring, hdlhashtable *, bigstring);

extern boolean langgetdotparams (hdltreenode, hdlhashtable *, bigstring);

extern boolean idstringvalue (hdlhashtable, bigstring, bigstring);

extern boolean langcheckparamcount (hdltreenode, short);

extern short langgetparamcount (hdltreenode);

extern boolean getparamvalue (hdltreenode, short, tyvaluerecord *);

extern boolean getreadonlyparamvalue (hdltreenode, short, tyvaluerecord *); /* 2003-04-28 AR: needed in langregexp.c */

extern boolean getoptionalparam (hdltreenode, short *, short *, bigstring, hdltreenode *);

extern boolean getoptionalparamvalue (hdltreenode, short *, short *, bigstring, tyvaluerecord *);

extern boolean getoptionaladdressparam (hdltreenode , short *, short *, bigstring , hdlhashtable *, bigstring); /*2006-03-10 aradke*/

extern boolean getoptionaltableparam (hdltreenode , short *, short *, bigstring, hdlhashtable *); /*2006-03-10 aradke*/

extern boolean getaddressparam (hdltreenode, short, tyvaluerecord *);

extern boolean getvarparam (hdltreenode, short, hdlhashtable *, bigstring);

extern boolean getcharparam (hdltreenode, short, tyvaluerecord *);

extern boolean getlongparam (hdltreenode, short, tyvaluerecord *);

extern boolean getdateparam (hdltreenode, short, tyvaluerecord *);

extern boolean getintparam (hdltreenode, short, tyvaluerecord *);

extern boolean getbooleanparam (hdltreenode, short, tyvaluerecord *);

extern boolean getstringparam (hdltreenode, short, tyvaluerecord *);

extern boolean getdirectionparam (hdltreenode, short, tyvaluerecord *);

extern boolean getostypeparam (hdltreenode, short, tyvaluerecord *);

extern boolean getpointparam (hdltreenode, short, tyvaluerecord *);

extern boolean getrectparam (hdltreenode, short, tyvaluerecord *);

extern boolean getrgbparam (hdltreenode, short, tyvaluerecord *);
	
extern boolean getpatternparam (hdltreenode, short, tyvaluerecord *);

extern boolean getfixedparam (hdltreenode, short, tyvaluerecord *);

extern boolean getsingleparam (hdltreenode, short, tyvaluerecord *);

extern boolean getdoubleparam (hdltreenode, short, tyvaluerecord *);

extern boolean getfilespecparam (hdltreenode, short, tyvaluerecord *);

extern boolean getaliasparam (hdltreenode, short, tyvaluerecord *);

extern boolean getobjspecparam (hdltreenode, short, tyvaluerecord *);

extern boolean getbinaryparam (hdltreenode, short, tyvaluerecord *);

#ifdef shellinclude
extern boolean getwinparam (hdltreenode, short, hdlwindowinfo *); // shellwindowverbs.c
#endif

extern boolean getvarvalue (hdltreenode, short, hdlhashtable *, bigstring, tyvaluerecord *, hdlhashnode *);

extern boolean getstringvalue (hdltreenode, short, bigstring);

extern boolean gettextvalue (hdltreenode, short, Handle *);

extern boolean getexempttextvalue (hdltreenode, short, Handle *);

extern boolean getreadonlytextvalue (hdltreenode, short, Handle *);

extern boolean getintvalue (hdltreenode, short, short *);

extern boolean getbooleanvalue (hdltreenode, short, boolean *);

extern boolean getcharvalue (hdltreenode, short, char *);

extern boolean getlongvalue (hdltreenode, short, long *);

extern boolean getdirectionvalue (hdltreenode, short, tydirection *);

extern boolean getdatevalue (hdltreenode, short, int64_t *);

extern boolean getostypevalue (hdltreenode, short, OSType *);

extern boolean getfilespecvalue ( hdltreenode, short, ptrfilespec );

extern boolean getpointvalue (hdltreenode, short, Point *);

extern boolean getrectvalue (hdltreenode, short, Rect *);

extern boolean getrgbvalue (hdltreenode, short, RGBColor *);

extern boolean getbinaryvalue (hdltreenode, short, boolean, Handle *);

extern boolean langsetbooleanvarparam (hdltreenode , short, boolean); /* 08/14/2000 AR */

extern boolean langsetlongvarparam (hdltreenode, short, long);

extern boolean langsetstringvarparam (hdltreenode, short, bigstring);	/* 2004-10-31 aradke */

extern boolean setintvarparam (hdltreenode, short, short);

extern boolean addvalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean subtractvalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean multiplyvalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean dividevalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean modvalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean EQvalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean NEvalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean GTvalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean LEvalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean LTvalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean GEvalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean beginswithvalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean endswithvalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean containsvalue (tyvaluerecord, tyvaluerecord, tyvaluerecord *);

extern boolean andandvalue (tyvaluerecord, hdltreenode, tyvaluerecord *);

extern boolean ororvalue (tyvaluerecord, hdltreenode, tyvaluerecord *);

extern boolean unaryminusvalue (tyvaluerecord, tyvaluerecord *);

extern boolean notvalue (tyvaluerecord, tyvaluerecord *);

extern boolean kernelfunctionvalue (hdlhashtable, bigstring, hdltreenode, tyvaluerecord *);

extern boolean langhandlercall (hdltreenode, hdltreenode, tyvaluerecord *);

extern boolean langfunctioncall (hdltreenode, hdlhashtable, hdlhashnode, bigstring, hdltreenode, hdltreenode, tyvaluerecord *);

extern boolean functionvalue (hdltreenode, hdltreenode, tyvaluerecord *);

/*langverbs.c - new() verb implementation
  Creates a new value of specified type and assigns it to a variable.
  Parameters: hparam1 - parameter tree containing type and address
              vreturned - output value record (returns flvalue=true on success)*/
extern boolean newvaluefunc (hdltreenode hparam1, tyvaluerecord *vreturned);

/* Phase 1 type conversion functions */
extern boolean langdoublefunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langsinglefunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langfixedfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langdirectionfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langstring4func (hdltreenode hparam1, tyvaluerecord *vreturned);

/* Phase 2: Memory & Utility functions */
extern boolean langabsfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langrandomfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langmemavailfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langflushmemfunc (hdltreenode hparam1, tyvaluerecord *vreturned);

/* Phase 3: Core Lang Operations functions */
extern boolean langdeletefunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langevaluatefunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langcallscriptfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langmsgfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langscripterrorfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langdisplaystringfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langgetbinarytypefunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langsetbinarytypefunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langaliasfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langevaluatethreadfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langclosefunc (hdltreenode hparam1, tyvaluerecord *vreturned);

/* Phase 4: Date/Time functions */
extern boolean langtimecreatedfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langtimemodifiedfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langsettimecreatedfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langsettimemodifiedfunc (hdltreenode hparam1, tyvaluerecord *vreturned);

/* Phase 5: Type conversion wrapper functions */
extern boolean langbooleanfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langcharfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langshortfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langlongfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langdatefunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langstringfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langaddressfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langbinaryfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langpointfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langrectfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langrgbfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langpatternfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langfilespecfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langlistfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langrecordfunc (hdltreenode hparam1, tyvaluerecord *vreturned);
extern boolean langenumfunc (hdltreenode hparam1, tyvaluerecord *vreturned);

extern boolean langzoomvalwindow (hdlhashtable, bigstring, tyvaluerecord, boolean); /*langverbs.c*/

extern boolean langfindtargetwindow (short, WindowPtr *);

extern boolean setstatusbarstring (bigstring bs, long partNumber); /*7.0b20 PBS: new statusbar verbs from Bob*/

extern boolean getstatusbarstring (bigstring bs, long partNumber);

extern boolean setstatusbarparts (long count, long * partArray);

extern long getstatusbarparts (long * array);

extern boolean langcleartarget (tyvaluerecord *prevtarget);

extern boolean langsettarget (hdlhashtable htable, bigstring bsname, tyvaluerecord *prevtarget);

extern boolean langgettarget (hdlhashtable *htable, bigstring bsname);

extern boolean langgettargetfunc (hdltreenode hparam1, tyvaluerecord *vreturned);

extern boolean langsettargetfunc (hdltreenode hparam1, tyvaluerecord *vreturned);

extern boolean langcleartargetfunc (hdltreenode hparam1, tyvaluerecord *vreturned);

#endif
