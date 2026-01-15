
/*	$Id$    */

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

#ifndef processinternalinclude
#define processinternalinclude

	#ifndef __THREADS__
		//#include <Threads.h>
	#endif

#ifndef threadsinclude
	#include "threads.h"
#endif

#ifndef shellcoreinclude
	#include "shellcore.h"
#endif

#ifndef langinclude
	#include "lang.h"
#endif

#ifndef processinclude
	#include "process.h"
#endif

/* Forward declarations and constants for types used in tythreadglobals */
typedef struct tycancoonrecord **hdlcancoonrecord;
typedef struct tyoutlinerecord **hdloutlinerecord;

#define ctoutlinestack 10 /*we can remember outline contexts up to 10 levels deep*/
#define maxerrorhooks 5

#ifndef FRONTIER_HEADLESS
typedef struct tymaceventsettings tymaceventsettings;
#else
/* Headless stubs for Mac types */
typedef struct tymaceventsettings {
	int unused;
} tymaceventsettings;
#endif

/* Mac-specific includes - not needed for headless builds */
#ifndef FRONTIER_HEADLESS
	#ifndef landinclude
		#include <land.h>
	#endif

	#ifndef cancooninclude
		#include "cancoon.h"
	#endif

	#ifndef opinternalinclude
		#include "opinternal.h"
	#endif

	#ifndef shellprivateinclude
		#include "shellprivate.h"
	#endif

	#ifndef shellhooksinclude
		#include "shellhooks.h"
	#endif
#endif /* FRONTIER_HEADLESS */


#define ctprocesses 5 /*we can remember nested processes up to 5 levels deep*/

#pragma pack(2)
typedef struct typrocessstackrecord {
	
	hdlprocessrecord hprocess;
	
	langerrormessagecallback errormessagecallback;
	
	langerrormessagecallback debugerrormessagecallback;
	
	langvoidcallback clearerrorcallback;
	
	hdlerrorstack herrorstack;
	
	hdltablestack htablestack;
	} typrocessstackrecord;


typedef struct typrocessstack {
	
	short top;
	
	typrocessstackrecord stack [ctprocesses];
	} typrocessstack;


typedef struct tythreadglobals {
	
	struct tythreadglobals **hnextglobals;
	
	hdlthread idthread;
	
	
	hdlcancoonrecord hccglobals;
	
	
	typrocessstack processstack;
	
	hdlprocessrecord hprocess;

	hdlhashtable htable;

	/* ADR-009: Hash table stack (thread-local, accessed via macro) */
	hdltablestack htablestack;

	tylangcallbacks langcallbacks;
	
	
	short cterrorhooks;
	
	callback errorhooks [maxerrorhooks];
	
	tyglobalsstack globalsstack;
	
	short topoutlinestack;
	
	hdloutlinerecord outlinestack [ctoutlinestack];
	
	hdloutlinerecord outlinedata;

	WindowPtr shellwindow;
	
	DialogPtr pmodaldialog;
	
	
	unsigned short ctscanlines;
	
	unsigned short ctscanchars;
	
	hdltreenode herrornode;
	
	unsigned long timestarted;

	unsigned long sleepticks;

	unsigned long timebeginsleep;
	
	unsigned long timetowake;
	
	unsigned long timeswappedin;

	unsigned long timesliceticks;

	
	pascal OSErr (*eventcreatecallback) (AEEventClass, AEEventID, const AEAddressDesc *, short, long, AppleEvent *);
	
	pascal OSErr (*eventsendcallback) (const AppleEvent *, AppleEvent *, AESendMode, AESendPriority, long, AEIdleUPP, AEFilterUPP);
	
	OSType applicationid;
	
	tymaceventsettings eventsettings; /*not just for OSA; new features*/
	
	
	boolean flreturn;
	
	boolean flbreak;
	
	boolean flcontinue;
	
	boolean flscriptresting;
	
	boolean flretryagent;
	
	boolean flthreadkilled;
	
	boolean fllangerror;
	
	boolean flscriptrunning;
	
	unsigned short langerrordisable; /*6.1.1b2 AR*/
	
	Handle tryerror;

	Handle tryerrorstack;
	
	unsigned short fldisableyield;
	
	long debugthreadingcookie; /*6.2b11 AR: for debugging hung threads*/

#ifndef FRONTIER_HEADLESS
	ThreadSwitchUPP threadInCallbackUPP;

	ThreadSwitchUPP threadOutCallbackUPP;

	ThreadTerminationUPP threadTerminateUPP;

	ThreadEntryUPP threadEntryCallbackUPP;
#endif

	bigstring current_working_directory; /*thread-local working directory for file operations*/

	boolean flcominitialized;

	/* ADR-005: Parameter handling state (Phase 3 migration) */
	boolean flnextparamislast;
	boolean flparamerrorenabled;
	boolean flcoerceexternaltostring;
	boolean flinhibitnilcoercion;
	boolean fllocaldotparamsonly;
	bigstring bsfunctionname;

	/* ADR-005: Value protection flags (Phase 3 migration) */
	boolean fllanghashassignprotect;
	boolean fllangexternalvalueprotect;

	/* Headless interactive mode detection (Phase 3) */
	boolean fl_batch_mode;           /* --batch flag set */
	boolean fl_interactive_detected;  /* Cached isatty() result */

	/* Reserved for future parameter state (Phase 6+) */
	void *param_reserved[4];

	} tythreadglobals, *ptrthreadglobals, **hdlthreadglobals;
#pragma options align=reset

/* ADR-009: Thread-local hash table stack migration (Phase 3A)
 *
 * MACRO COMMENTED OUT - Bootstrap Initialization Constraint:
 * Early initialization code (inittablestructure, main bootstrap, database loading)
 * runs BEFORE hthreadglobals is created, requiring direct access to hashtablestack.
 * Enabling this macro causes NULL pointer dereference during startup.
 *
 * Current Status:
 * - htablestack field EXISTS in tythreadglobals and is saved/restored in thread swaps
 * - Global variable still used for access (no functional change)
 * - Foundation in place for Phase 6+ when bootstrap is refactored
 *
 * Path Forward:
 * - Phase 4-5: Refactor bootstrap to initialize threads before hash tables (Issue #305)
 * - Phase 6+: Enable macro OR skip to explicit context architecture
 *
 * See ADR-009 "Phase 3A Implementation Note: Bootstrap Initialization Constraint"
 * for detailed analysis and refactoring options.
 */
/* #define hashtablestack ((**hthreadglobals).htablestack) */

/* ADR-005: Thread-local parameter and value protection state (backward-compatible macros) */
#define flnextparamislast ((**hthreadglobals).flnextparamislast)
#define flparamerrorenabled ((**hthreadglobals).flparamerrorenabled)
#define flcoerceexternaltostring ((**hthreadglobals).flcoerceexternaltostring)
#define flinhibitnilcoercion ((**hthreadglobals).flinhibitnilcoercion)
#define fllocaldotparamsonly ((**hthreadglobals).fllocaldotparamsonly)
#define bsfunctionname ((**hthreadglobals).bsfunctionname)

/* Headless interactive mode detection (backward-compatible macros) */
#define fl_batch_mode ((**hthreadglobals).fl_batch_mode)
#define fl_interactive_detected ((**hthreadglobals).fl_interactive_detected)
#define fllanghashassignprotect ((**hthreadglobals).fllanghashassignprotect)
#define fllangexternalvalueprotect ((**hthreadglobals).fllangexternalvalueprotect)

/*globals*/

extern hdlthreadglobals hthreadglobals; /* ADR-005: Current thread's globals for macro access */

extern boolean flthreadkilled;


/* ADR-006 Phase 3: Outline context - accessor functions and backward-compatible macros
 *
 * ORGANIZATION:
 * 1. Accessor functions (defined first, access raw struct members)
 * 2. Backward-compatible macros (defined after, also access raw struct members)
 *
 * This ordering prevents macro expansion inside accessor function bodies.
 * During Phase 2 migration, call sites will be refactored to use accessor functions.
 * During Phase 4, macros will be removed entirely.
 */

/* ADR-006 Phase 3: Thread-safe accessor functions for outline context
 *
 * These inline accessor functions replace direct macro access to outline context,
 * preventing address-taking that creates stale pointer issues after thread switches.
 *
 * MIGRATION STRATEGY:
 * - Phase 1: Add accessors (this section) - macros and accessors coexist
 * - Phase 2: Automated refactoring - replace macro usage with accessor calls
 * - Phase 3: Manual fixes - handle address-taking sites with _unsafe accessor
 * - Phase 4: Remove macros - complete migration to accessor-only model
 *
 * THREAD-SAFETY GUARANTEE:
 * All accessors return values or write through thread-local storage. No pointers
 * to thread-local storage are exposed except through explicit _unsafe accessors
 * (used only for legacy GUI code that must be refactored later).
 */

/* Read-only accessors - return by value (safe for thread switches) */
static inline hdloutlinerecord op_get_outlinedata(void) {
	#ifdef DEBUG
		assert(hthreadglobals != nil);
	#endif
	return (**hthreadglobals).outlinedata;
}

static inline short op_get_topoutlinestack(void) {
	#ifdef DEBUG
		assert(hthreadglobals != nil);
	#endif
	return (**hthreadglobals).topoutlinestack;
}

static inline hdloutlinerecord op_get_outlinestack(short level) {
	#ifdef DEBUG
		assert(hthreadglobals != nil);
		assert(level >= 0 && level < ctoutlinestack);
	#endif
	return (**hthreadglobals).outlinestack[level];
}

/* Write accessors - modify thread-local storage in place */
static inline void op_set_outlinedata(hdloutlinerecord houtline) {
	#ifdef DEBUG
		assert(hthreadglobals != nil);
	#endif
	(**hthreadglobals).outlinedata = houtline;
}

static inline void op_set_topoutlinestack(short top) {
	#ifdef DEBUG
		assert(hthreadglobals != nil);
		assert(top >= 0 && top <= ctoutlinestack);
	#endif
	(**hthreadglobals).topoutlinestack = top;
}

static inline void op_set_outlinestack(short level, hdloutlinerecord houtline) {
	#ifdef DEBUG
		assert(hthreadglobals != nil);
		assert(level >= 0 && level < ctoutlinestack);
	#endif
	(**hthreadglobals).outlinestack[level] = houtline;
}

/* UNSAFE accessor - returns pointer to thread-local storage
 *
 * WARNING: Pointer becomes invalid after thread switch. Only use for:
 * 1. Legacy GUI code (langipc.c, opwinpad.c) that captures &outlinedata
 * 2. Code that IMMEDIATELY dereferences the pointer without storing it
 *
 * This accessor exists ONLY to support address-taking sites during migration.
 * All such call sites must be refactored when GUI becomes multi-threaded.
 *
 * TODO (Frontier 2.0): Eliminate all call sites and remove this function.
 */
static inline hdloutlinerecord* op_get_outlinedata_ptr_unsafe(void) {
	#ifdef DEBUG
		assert(hthreadglobals != nil);
	#endif
	return &(**hthreadglobals).outlinedata;
}


/* ADR-006: Thread-local outline context migration complete (Phase 4)
 *
 * All outline context access now goes through type-safe accessor functions:
 * - op_get_outlinedata() / op_set_outlinedata()
 * - op_get_topoutlinestack() / op_set_topoutlinestack()
 * - op_get_outlinestack() / op_set_outlinestack()
 *
 * The backward-compatible macros have been removed. All 677+ call sites have been
 * migrated to use accessor functions, eliminating the stale pointer footgun and
 * providing thread-safe access to outline context.
 *
 * INITIALIZATION CONTRACT: Accessor functions assume hthreadglobals is initialized.
 * In headless mode, headless_init_threadglobals() must be called during startup.
 * In GUI mode, newthreadglobals() initializes these fields when creating threads.
 */


/*prototypes*/

extern void disposethreadglobals (hdlthreadglobals);

extern boolean newthreadglobals (hdlthreadglobals *);

extern hdlthreadglobals getcurrentthreadglobals (void);

extern void copythreadglobals (hdlthreadglobals);

extern void swapinthreadglobals (hdlthreadglobals);

/* Thread visitor callback type for visitprocessthreads */
typedef pascal boolean (*threadvisitcallback) (hdlthreadglobals hthread, long refcon);

extern boolean visitprocessthreads (threadvisitcallback visit, long refcon);

#endif /* processinternalinclude */
