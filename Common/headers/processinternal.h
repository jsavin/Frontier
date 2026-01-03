
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

	/* Reserved for future parameter state (Phase 6+) */
	void *param_reserved[4];

	} tythreadglobals, *ptrthreadglobals, **hdlthreadglobals;
#pragma options align=reset

/* ADR-005: Thread-local parameter and value protection state (backward-compatible macros) */
#define flnextparamislast ((**hthreadglobals).flnextparamislast)
#define flparamerrorenabled ((**hthreadglobals).flparamerrorenabled)
#define flcoerceexternaltostring ((**hthreadglobals).flcoerceexternaltostring)
#define flinhibitnilcoercion ((**hthreadglobals).flinhibitnilcoercion)
#define fllocaldotparamsonly ((**hthreadglobals).fllocaldotparamsonly)
#define bsfunctionname ((**hthreadglobals).bsfunctionname)
#define fllanghashassignprotect ((**hthreadglobals).fllanghashassignprotect)
#define fllangexternalvalueprotect ((**hthreadglobals).fllangexternalvalueprotect)

/*globals*/

extern hdlthreadglobals hthreadglobals; /* ADR-005: Current thread's globals for macro access */

extern boolean flthreadkilled;


/*prototypes*/

extern void disposethreadglobals (hdlthreadglobals);

extern boolean newthreadglobals (hdlthreadglobals *);

extern hdlthreadglobals getcurrentthreadglobals (void);

extern void copythreadglobals (hdlthreadglobals);

extern void swapinthreadglobals (hdlthreadglobals);

#endif /* processinternalinclude */
