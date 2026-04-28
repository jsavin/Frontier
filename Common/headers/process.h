
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

#ifndef processinclude
#define processinclude /*so other includes can tell if we've been loaded*/

#ifndef langinclude
	
	#include "lang.h"

#endif

#ifndef threadsinclude

	#include "threads.h"

#endif


typedef struct tythreadglobals **hdlprocessthread;		// it's a hdlthreadglobals really; we're avoiding dependencies
#pragma pack(2)
typedef struct typrocessrecord { 
	
	struct typrocessrecord **hnextprocess; /*nil-terminated list*/
	
	struct typrocesslist **hprocesslist; /*this list process is in, or nil*/
	
	hdltreenode hcode; /*the code that gets executed for this process*/
	
	hdltreenode holdcode; /*non-nil if code was replaced while agent was running*/
	
	hdlhashtable hcontext; /*non-nil if running from component API or other special cases*/
	
	langerrormessagecallback errormessagecallback;
	
	langerrormessagecallback debugerrormessagecallback;

	langvoidcallback clearerrorcallback;
	
	unsigned long sleepuntil; /*indicates when the process should wake up*/
	
	hdltablestack htablestack;
	
	hdlerrorstack herrorstack;
	
	hdlprocessthread hthread;
	
	boolean flsleepinbackground; /*if true, agent shouldn't be scheduled in the background*/
	
	boolean fldebugging; /*trap into debugger on every "meaty" instruction*/
	
	boolean flscheduled; /*has this processing been scheduled to run?*/
	
	boolean floneshot; /*if true process runs once and is then disposed*/
	
	boolean flrunning; /*is this processing running now?*/
	
	boolean fldisposewhenidle; /*was there an attempt to dispose of this process?*/
	
	boolean flprofiling; /*track time slice ticks*/
	
	boolean flprofilesliced; /*slice totals are not comulative*/
	
	struct tyexternalvariable **hprofiledata; /*table variable for perf data*/
	
	langvoidcallback processstartedroutine; /*execution is beginning*/
	
	langvoidcallback processkilledroutine; /*execution is terminating*/
	
	bigstring bsmsg; /*the last message posted by this agent*/
	
	bigstring bsname; // 5.0a22 dmb: why not maintain the name from the start?
	
	long processrefcon; /*for client use only*/
	} typrocessrecord, *ptrprocessrecord, **hdlprocessrecord;



typedef struct typrocesslist {
	
	hdlprocessrecord hfirstprocess; /*head of list*/
	
	/*hdlprocessrecord processlistmarker; /%for scheduler, pick up search from here*/
	
	short ctrunning; /*number of processes in this list currently running*/
	
	short ctsleeping; /*number of processes in this list currently sleeping*/
	
	boolean fldisposewhenidle; /*dispose when ctrunning returns to zero?*/
	} typrocesslist, *ptrprocesslist, **hdlprocesslist;


#pragma options align=reset


/*globals*/

extern hdlprocessrecord currentprocess; /*process that's currently running*/

extern hdlprocessrecord newlyaddedprocess; /*process that was just added to list*/

extern unsigned short fldisableyield;

extern boolean flcanusethreads;


/*prototypes*/

extern boolean setagentsenable (boolean);

extern boolean agentsenabled (void);

extern boolean agentsdisable (boolean);


extern boolean disposeprocesslist (hdlprocesslist);

extern boolean newprocesslist (hdlprocesslist *);

extern void setcurrentprocesslist (hdlprocesslist);


extern boolean pushprocess (hdlprocessrecord);

extern boolean popprocess (void);

extern void disposeprocess (hdlprocessrecord hprocess);

extern boolean newprocess (hdltreenode, boolean, langerrorcallback, long, hdlprocessrecord *);

extern boolean addprocess (hdlprocessrecord);

extern boolean addnewprocess (hdltreenode, boolean, langerrorcallback, long);

extern boolean processbackgroundtasks (void);

extern boolean processisoneshot (boolean);

extern boolean processagentsleep (long);

extern boolean processfindcode (hdltreenode, hdlprocessrecord *);

extern void processcodedisposed (long);

extern boolean processreplacecode (hdltreenode, hdltreenode);

extern boolean processdisposecode (hdltreenode);

extern boolean debuggingcurrentprocess (void);

extern boolean processstartprofiling (boolean);

extern boolean processstopprofiling (void);

extern boolean profilingcurrentprocess (void);

extern boolean processruncode (hdlprocessrecord, tyvaluerecord *);

extern boolean processruntext (Handle htext);

extern boolean processrunstring (bigstring);

extern boolean processrunstringnoerrorclear (bigstring);

extern boolean processkill (hdlprocessrecord);

// kw 2005-12-16 - These two are retired
// extern void processinvalidglobals (WindowPtr);

// extern void processinvalidoutline (struct tyoutlinerecord **);

extern boolean abort1shotprocess (void);

extern void killdependentprocesses (long);

extern boolean newprocessthread (tythreadmaincallback, tythreadmainparams, hdlprocessthread *);

extern boolean processpsuedothread (tythreadmaincallback, tythreadmainparams);

extern short processthreadcount (void);

extern boolean initprocessthread (bigstring);

extern void exitprocessthread (void);

extern hdlprocessthread getcurrentthread (void);

extern void endprocessthread (hdlprocessthread);

extern boolean infrontierthread (void);

extern boolean goodthread (hdlprocessthread);

extern boolean ingoodthread (void);

extern long getthreadid (hdlprocessthread);

extern hdlprocessthread getprocessthread (long);

extern hdlprocessthread nthprocessthread (long);

extern boolean wakeprocessthread (hdlprocessthread);

extern boolean killprocessthread (hdlprocessthread);

extern boolean processsleep (hdlprocessthread, unsigned long);

extern boolean processissleeping (hdlprocessthread hthread);

extern boolean processwake (hdlprocessthread);

extern boolean processyield (void);

extern boolean processyieldtoagents (void);

extern boolean processbusy (void);

extern boolean processnotbusy (void);

extern boolean processrunning (void);

extern boolean setprocesstimeslice (unsigned long);

extern boolean getprocesstimeslice (unsigned long *);

extern boolean setdefaulttimeslice (unsigned long);

extern boolean getdefaulttimeslice (unsigned long *);

extern boolean processtimesliceelapsed (void);

extern unsigned long processstackspace (void);

extern boolean scheduleprocess (hdlprocessrecord hprocess, hdlprocessthread *pnewthread);

extern void processscheduler (void);

extern void agentscheduler_tick (void);  /* Non-blocking tick for event loop integration */

extern void processchecktimeouts (void);

extern boolean processsymbolunlinking (hdlhashtable, hdlhashnode);

extern void processclose (void);

extern boolean processwaitforquiet (long timeoutticks);	

extern boolean initprocess (void);

extern boolean processgetstats (hdlhashtable); /*6.2b6 AR*/

#endif
