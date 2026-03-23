
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

/* 2025-11-24 Codex: BE32 sysverb type writes. */


#include "frontier.h"
#include "standard.h"

#include <land.h>
#define wsprintf sprintf

#include "ops.h"
#include "memory.h"
#include "error.h"
#include "file.h"
#include "resources.h"
#include "scrap.h"
#include "strings.h"
#include "launch.h"
#include "notify.h"
#include "shell.h"
#include "shellmenu.h"
#include "lang.h"
#include "langexternal.h"
#include "langinternal.h"
#include "langipc.h"
#include "kernelverbs.h"
#include "kernelverbdefs.h"
#include "tablestructure.h"
	#include "shellprivate.h"
#include "process.h"
#include "processinternal.h"
#include "sysshellcall.h"

#include "langsystem7.h"  //6.1b7 AR: we need coercetolist
#include "tableverbs.h"  //6.1b7 AR: we need gettablevalue
#include "tableinternal.h" //6.1b7 AR: we need tablepacktable and tableunpacktable
#include "serialnumber.h" //7.1b34 dmb: new isvalidserialnumber verb
#include "db_format.h" /* 2025-11-23 Codex: BE helper for sys verbs */
#include "byteorder.h"	/* 2006-04-08 aradke: endianness conversion macros */

#if defined(__APPLE__) || defined(__linux__)
#include <unistd.h>	/* 2026-03-21 JES: for fork, execlp, _exit in sys.openUrl */
#include <sys/wait.h>	/* 2026-03-21 JES: for waitpid in sys.openUrl */
#endif

#define systemevents (osMask | activMask)

boolean frontierversion (tyvaluerecord *v); /* 2002-10-13 AR: also used in langhtml.c */
	
boolean sysos (tyvaluerecord *v); /* 2002-10-13 AR: also used in langhtml.c */


static tyfilespec programfspec;

static bigstring bsfrontierversion;


typedef enum tysystoken { /*verbs that are processed by sys*/
	
	systemversionfunc,
	
	systemtaskfunc,
	
	browsenetworkfunc,
	
	apprunningfunc,
	
	frontappfunc,
	
	bringapptofrontfunc,
	
	countappsfunc,
	
	getnthappfunc,
	
	getapppathfunc,
	
	memavailfunc,
	
	machinefunc,

	osfunc,

	getenvironmentvariablefunc,

	setenvironmentvariablefunc,
	
	unixshellcommandfunc,

	winshellcommandfunc,

	openurlfunc,

	ctsysverbs
	} tysystoken;


typedef enum tylaunchtoken { /*verbs that are processed by launch*/
	
	applemenufunc,
	
	launchappfunc,
	
	launchappwithdocfunc,
	
	executeresourcefunc,
	
	anythingfunc,
	
	ctlaunchverbs
	} tylaunchtoken;


typedef enum tyfrontiertoken { /*verbs that are processed by frontier*/
	
	programpathfunc,
	
	filepathfunc,
	
	
	agentsenablefunc,
	
	
	requesttofrontfunc,
	
	isruntimefunc,
	
	countthreadsfunc,
	
	isnativefunc,
	
	reclaimmemoryfunc,
	
	frontierversionfunc,

	hashstatsfunc,

	gethashloopcountfunc,

	hideapplicationfunc,

	isvalidserialnumberfunc,

	showapplicationfunc,

	ctfrontierverbs
	} tyfrontiertoken;


typedef enum tyclipboardtoken { /*verbs that are processed by clipboard*/
	
	getscrapfunc,
	
	putscrapfunc,
	
	ctclipboardverbs
	} tyclipboardtoken;



typedef enum tythreadtoken {
	
	existsfunc,
	
	evaluatefunc,

	callscriptfunc,
	
	getcurrentfunc,
	
	getcountfunc,
	
	getnththreadfunc,
	
	sleepfunc,
	
	sleepforfunc,

	sleepticksfunc,
	
	issleepingfunc,
	
	wakefunc,
	
	killfunc,
	
	gettimeslicefunc,
	
	settimeslicefunc,
	
	getdefaulttimeslicefunc,
	
	setdefaulttimeslicefunc,
	/*
	begincriticalfunc,
	
	endcriticalfunc,
	*/
	statsfunc,
	
	ctthreadverbs
	
	} tythreadtoken;



static boolean getscrapverb (hdltreenode hparam1, tyvaluerecord *v) {
	
	/*
	5.0a10 dmb: must open/close clipboard. byte swap binary type
	*/

	OSType type;
	Handle hscrap;
	hdlhashtable htable;
	bigstring bs;
	boolean fl = false;
		
	setbooleanvalue (false, v); /*default return*/
	
	if (!getostypevalue (hparam1, 1, &type))
		return (false);
	
	flnextparamislast = true;
	
	if (!getvarparam (hparam1, 2, &htable, bs)) /*returned handle holder*/
		return (false);
	
	shellwritescrap (anyscraptype); /*export our private scrap, in necessary*/
	
	if (!newemptyhandle (&hscrap))
		return (false);
	
	if (openclipboard ()) {
		
		fl = getscrap (type, hscrap);
		
		closeclipboard ();
		}
	
	if (!fl) {
		
		disposehandle (hscrap);
		
		return (true); /*not a runtime error; return value is false*/
		}
	
	db_format_write_be32(&type, (uint32_t) type);
	
	if (!insertinhandle (hscrap, 0L, &type, sizeof (type))) {
		
		disposehandle (hscrap);
		
		return (false);
		}
	
	if (!langsetbinaryval (htable, bs, hscrap)) /*probably a memory error*/
		return (false);
	
	(*v).data.flvalue = true;
	
	return (true);
	} /*getscrapverb*/


static boolean putscrapverb (hdltreenode hparam1, tyvaluerecord *v) {
	
	/*
	5.0a10 dmb: must open/close clipboard
	*/

	OSType type;
	Handle hbinary;
	OSType bintype;
	
	if (!getostypevalue (hparam1, 1, &type))
		return (false);
	
	flnextparamislast = true;
	
	if (!getbinaryvalue (hparam1, 2, false, &hbinary))
		return (false);
	
	pullfromhandle (hbinary, 0L, sizeof (bintype), &bintype);
	
	if (openclipboard ()) {
		
 		releasethreadglobals ();
		
		resetscrap ();

		grabthreadglobals ();
		
		(*v).data.flvalue = putscrap (type, hbinary);

		closeclipboard ();
		}
	
	return (true);
	} /*putscrapverb*/


static boolean shellsysverbwaitroutine (void) {
	
	/*
	12/24/92 dmb: added special case for when yield is disabled
	
	2.1b9 dmb: systemevents are now just osMask. waiting for all updates 
	is dangerous, 'cause they may not be servicable
	
	3.0b15 dmb: systemevents need to include activate event too. otherwise, 
	shelleventavail (EventAvail) can return false when a juggle is indeed 
	pending.
	*/
	
	boolean fl;
	
	if (flscriptrunning)
		fl = langpartialeventloop ((short) systemevents);
	else
		fl = shellpartialeventloop ((short) systemevents);
	
	return (fl);
	} /*shellsysverbwaitroutine*/


boolean frontierversion (tyvaluerecord *v) { //6.1d1 AR: needed in langhtml.c
	
	if (stringlength (bsfrontierversion) == 0)
		filegetprogramversion (bsfrontierversion);
	
	return (setstringvalue (bsfrontierversion, v));
	} /*frontierversion*/


boolean sysos (tyvaluerecord *v) { //6.1d1 AR: needed in langhtml.c

		//#if TARGET_API_MAC_CARBON == 1
		//return (setstringvalue(osCarbon, v));
		//#else
		return (setstringvalue (osMacOS, v));
	
	//#endif
	
	} /*sysos*/


static boolean sysfunctionvalue (short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror) {
	
	//
	// 2006-06-28 creedon: for Mac, FSRef-ized
	//
	// 5.0b16 dmb: undo that change. it affect performance adversely if many threads do it.
	//
	// 5.0b12 dmb: in systemtaskfunc, set flresting to false to make sure we don't slow down too much
	//
	// 1/18/93 dmb: in systemtaskfunc, don't call processyield directly; use langbackgroundtask
	//
	// 8/11/92 dmb: make apprunningfunc accept a string or an ostype
	//
	// 5/20/92 dmb: do processyield directly on systemtaskfunc
	//
	// 2/12/92 dmb: do partialeventloop on systemtask & bringapptofrontfunc
	//
	
	register tyvaluerecord *v = vreturned;
	
	setbooleanvalue (false, v); /*assume the worst*/
	
	switch (token) { /*these verbs don't need any special globals pushed*/
		
		case systemversionfunc: {
			bigstring bs;
			
			getsystemversionstring (bs, nil);
			
			if (!langcheckparamcount (hparam1, 0))
				return (false);
						
			return (setstringvalue (bs, v));
			}
		
		case systemtaskfunc:
			if (!langcheckparamcount (hparam1, 0)) /*shouldn't have any parameters*/
				return (false);
			
			shellsysverbwaitroutine ();
			
			/*
			if (!processyield ())
				return (false);
			*/
			
			if (!langbackgroundtask (true))
				return (false);
			
			(*v).data.flvalue = true;
			
			return (true);
		
		case browsenetworkfunc:
		
			
				return (langipcbrowsenetwork (hparam1, v));
				
			

		case apprunningfunc: {
			OSType appid;
			bigstring bsapp;
			tyvaluerecord val;
			
			flnextparamislast = true;
			
			/*
			if (!getostypevalue (hparam1, 1, &appid))
				return (false);
			
			(*v).data.flvalue = findrunningapplication (&appid, nil);
			*/
			
			if (!getparamvalue (hparam1, 1, &val))
				return (false);
			
			if (val.valuetype == ostypevaluetype) {
				
				setemptystring (bsapp);
				
				appid = val.data.ostypevalue;
				}
			else {
				if (!coercetostring (&val))
					return (false);
				
				pullstringvalue (&val, bsapp);
				
				if (!stringtoostype (bsapp, &appid))
					appid = 0;
				}
			
			(*v).data.flvalue = findrunningapplication (&appid, bsapp, nil);
			
			return (true);
			}
		
		case frontappfunc: {
			bigstring bs;
			
			if (!langcheckparamcount (hparam1, 0))
				return (false);
			
			if (!getfrontapplication (bs, false))
				return (false);
			
			return (setstringvalue (bs, v));
			}
		
		case bringapptofrontfunc: {
			bigstring bs;
			
			flnextparamislast = true;
			
			if (!getstringvalue (hparam1, 1, bs))
				return (false);
			
			(*v).data.flvalue = activateapplication (bs);
			
			return (true);
			}
		
		case countappsfunc:
			if (!langcheckparamcount (hparam1, 0))
				return (false);
			
			return (setlongvalue (countapplications (), v));
		
		case getnthappfunc: {
			short n;
			bigstring bs;
			
			if (!getintvalue (hparam1, 1, &n))
				return (false);
			
			if (!getnthapplication (n, bs))
				setemptystring (bs);
			
			return (setstringvalue (bs, v));
			}
		
		case getapppathfunc: {
			bigstring bs;
			tyfilespec fs;
			
			flnextparamislast = true;
			
			if ( ! getstringvalue ( hparam1, 1, bs ) )
				return ( false );
			
			if ( ! getapplicationfilespec ( bs, &fs ) ) // 2006-02-17 aradke: initializes fs even if it fails
				setemptystring (bs);
				
			return ( setfilespecvalue ( &fs, v ) );
			}
		
		case memavailfunc:
			{
			unsigned long memavail;


				memavail = TempFreeMem();
			
			if (!langcheckparamcount (hparam1, 0)) /*shouldn't have any parameters*/
				return (false);
			
			return (setlongvalue (memavail, v));
			}
		
		case machinefunc:
			
				//Code change by Timothy Paustian Friday, June 16, 2000 3:13:09 PM
				//Changed to Opaque call for Carbon
				//Carbon only runs on PPC
				return (setstringvalue (machinePPC, v));

				
			

			break;

		case osfunc:
			return (sysos (v));
			break;

		case getenvironmentvariablefunc: {
			bigstring varname;
			char *value;

			flnextparamislast = true;

			if (!getstringvalue (hparam1, 1, varname))
				return (false);

			/* Convert Pascal string to C string */
			nullterminate (varname);

			/* Get environment variable value */
			value = getenv ((char *)varname);

			if (value == NULL) {
				/* Variable not found - return empty string */
				bigstring emptystr;
				setemptystring (emptystr);
				return (setstringvalue (emptystr, v));
			}

			/* Convert C string to Pascal string and return */
			{
				bigstring result;
				size_t len = strlen (value);

				/* Check for buffer overflow - bigstring max content length is 255 */
				if (len > 255) {
					langerrormessage (BIGSTRING ("\pCan't get environment variable because value exceeds 255 characters"));
					return (false);
				}

				copyctopstring (value, result);
				return (setstringvalue (result, v));
			}
		}

		case setenvironmentvariablefunc: {
			bigstring varname, varvalue;

			if (!getstringvalue (hparam1, 1, varname))
				return (false);

			flnextparamislast = true;

			if (!getstringvalue (hparam1, 2, varvalue))
				return (false);

			/* Convert Pascal strings to C strings */
			nullterminate (varname);
			nullterminate (varvalue);

			/* Set environment variable
			 * POSIX setenv() third parameter (overwrite=1) replaces existing value if present
			 * Windows _putenv_s() always overwrites, no flag needed
			 */
			#ifdef WIN95VERSION
			if (_putenv_s ((char *)varname, (char *)varvalue) != 0) {
			#else
			if (setenv ((char *)varname, (char *)varvalue, 1) != 0) {
			#endif
				langerrormessage (BIGSTRING ("\pCan't set environment variable because system call failed"));
				return (false);
			}

			(*v).data.flvalue = true;

			return (true);
		}

		case unixshellcommandfunc: { /*7.0b51 PBS: call shell on OS X; 2025-12-27: enhanced to support optional stderr capture*/

			Handle hcommand, hstdout, hstderr;
			short paramcount;

			if (!getexempttextvalue (hparam1, 1, &hcommand))
				return (false);

			paramcount = langgetparamcount (hparam1);

			if (paramcount == 1) {
				/* Original behavior: return stdout as string (backward compatible) */

				flnextparamislast = true;

				newemptyhandle (&hstdout);

				if (!unixshellcall (hcommand, hstdout)) {
					disposehandle (hstdout);
					disposehandle (hcommand);
					return (false);
				}

				disposehandle (hcommand);
				return (setheapvalue (hstdout, stringvaluetype, v));
			}
			else if (paramcount == 2) {
				/* Two params: capture stdout to address, return boolean */

				hdlhashtable htable;
				bigstring varname;
				boolean fl;


				flnextparamislast = true;

				if (!getvarparam (hparam1, 2, &htable, varname))
					return (false);


				newemptyhandle (&hstdout);

				fl = unixshellcall (hcommand, hstdout);
				disposehandle (hcommand);

				if (!fl) {
					disposehandle (hstdout);
					return (false);
				}

				/* Use langassigntextvalue - efficient, no tmpstack overhead */
				if (!langassigntextvalue (htable, varname, hstdout)) {
					disposehandle (hstdout);
					return (false);
				}

				return (setbooleanvalue (true, v));
			}
			else if (paramcount == 3) {
				/* Three params: capture both stdout and stderr to addresses, return boolean */

				hdlhashtable htable, htable2;
				bigstring varname, varname2;
				boolean fl;

				if (!getvarparam (hparam1, 2, &htable, varname))
					return (false);


				flnextparamislast = true;

				if (!getvarparam (hparam1, 3, &htable2, varname2))
					return (false);


				newemptyhandle (&hstdout);
				newemptyhandle (&hstderr);

				fl = unixshellcall_separatestderr (hcommand, hstdout, hstderr, NULL);
				disposehandle (hcommand);

				if (!fl) {
					disposehandle (hstdout);
					disposehandle (hstderr);
					return (false);
				}

				/* Use langassigntextvalue - efficient, no tmpstack overhead */
				if (!langassigntextvalue (htable, varname, hstdout)) {
					disposehandle (hstdout);
					disposehandle (hstderr);
					return (false);
				}

				if (!langassigntextvalue (htable2, varname2, hstderr)) {
					disposehandle (hstderr);
					return (false);
				}

				return (setbooleanvalue (true, v));
			}
			else if (paramcount == 4) {
				/* Four params: capture stdout, stderr, and exit status to addresses, return boolean */

				hdlhashtable htable, htable2, htable3;
				bigstring varname, varname2, varname3;
				boolean fl;
				int exit_status;
				tyvaluerecord val_exitstatus;

				if (!getvarparam (hparam1, 2, &htable, varname))
					return (false);

				if (!getvarparam (hparam1, 3, &htable2, varname2))
					return (false);


				flnextparamislast = true;

				if (!getvarparam (hparam1, 4, &htable3, varname3))
					return (false);


				newemptyhandle (&hstdout);
				newemptyhandle (&hstderr);

				fl = unixshellcall_separatestderr (hcommand, hstdout, hstderr, &exit_status);
				disposehandle (hcommand);

				if (!fl) {
					disposehandle (hstdout);
					disposehandle (hstderr);
					return (false);
				}

				/* Use langassigntextvalue - efficient, no tmpstack overhead */
				if (!langassigntextvalue (htable, varname, hstdout)) {
					disposehandle (hstdout);
					disposehandle (hstderr);
					return (false);
				}

				if (!langassigntextvalue (htable2, varname2, hstderr)) {
					disposehandle (hstderr);
					return (false);
				}

				/* For exit status (integer), use setlongvalue + hashtableassign pattern */
				setlongvalue (exit_status, &val_exitstatus);

				if (!hashtableassign (htable3, varname3, val_exitstatus)) {
					return (false);
				}

				return (setbooleanvalue (true, v));
			}
			else {
				langerror (toomanyparamserror);
				return (false);
			}
			}

		case winshellcommandfunc: { /*Windows version of shell command verb; 2025-12-27: @PLATFORM_SPECIFIC*/

			Handle hcommand;
			short paramcount;

			if (!getexempttextvalue (hparam1, 1, &hcommand))
				return (false);

			paramcount = langgetparamcount (hparam1);

			if (paramcount > 4) {
				langerror (toomanyparamserror);
				disposehandle (hcommand);
				return (false);
			}

			flnextparamislast = true;
			disposehandle (hcommand);

#ifdef WIN32
			/* TODO (Issue #194): Implement Windows version using CreateProcess with pipes.
			   Follow the same 1/2/3/4-parameter pattern as Unix version (see unixshellcommandfunc).
			   1 param: return stdout string (backward compatible)
			   2 params: capture stdout to address, return boolean
			   3 params: capture stdout and stderr to addresses, return boolean
			   4 params: capture stdout, stderr, and exit status to addresses, return boolean */
			getstringlist (langerrorlist, unimplementedverberror, bserror);
#else
			/* Not available on non-Windows platforms */
			copystring (BIGSTRING("\psys.winshellcommand is only available on Windows"), bserror);
#endif
			return (false);
			}

		case openurlfunc: {
			/* Kernel verb name is "openUrl" (camelCase per Frontier.root glue convention).
			 * C enum uses lowercase per C convention.
			 *
			 * 3/21/26 JES: Open a URL in the default browser without shell interpolation.
			 * Uses fork/execlp to avoid command injection vulnerabilities.
			 * macOS: execlp("open", ...), Linux: execlp("xdg-open", ...).
			 */

			Handle hurl;

			flnextparamislast = true;

			if (!getexempttextvalue (hparam1, 1, &hurl))
				return (false);

			if (!enlargehandle (hurl, 1, "\0")) {
				disposehandle (hurl);
				return (false);
			}

			lockhandle (hurl);

			{
				const char *url = (const char *) *hurl;

				if (GetHandleSize(hurl) == 0 || url[0] == '\0') {
					unlockhandle (hurl);
					disposehandle (hurl);
					return (setbooleanvalue (false, v));
				}

#if defined(__APPLE__) || defined(__linux__)
				/*
				Double-fork to avoid zombie processes: first child forks again
				then exits immediately. The grandchild is reparented to init/launchd
				and reaped automatically. Parent waits only for the short-lived
				first child.
				*/
				pid_t pid = fork ();

				if (pid == 0) { /* first child */
					/* Handle hurl is intentionally not freed in the child process —
					 * _exit() tears down the address space immediately, making
					 * explicit cleanup unnecessary and potentially unsafe. */
					pid_t pid2 = fork ();

					if (pid2 == 0) { /* grandchild — runs the command */
#ifdef __APPLE__
						execlp ("open", "open", url, NULL);
#else
						execlp ("xdg-open", "xdg-open", url, NULL);
#endif
						_exit (1); /* execlp failed */
					}

					_exit (0); /* first child exits immediately */
				}

				/* These run unconditionally in the parent process (the child has
				 * already _exit'd by this point). The fork-failure check below
				 * is safe because hurl has already been cleaned up. */
				unlockhandle (hurl);
				disposehandle (hurl);

				if (pid < 0) { /* fork failed */
					return (setbooleanvalue (false, v));
				}

				waitpid (pid, NULL, 0); /* reap first child (returns instantly) */

				return (setbooleanvalue (true, v));
#elif defined(_WIN32)
				/* ShellExecuteA returns > 32 on success */
				boolean fl = ((int)(intptr_t) ShellExecuteA (NULL, "open", url, NULL, NULL, SW_SHOWNORMAL)) > 32;

				unlockhandle (hurl);
				disposehandle (hurl);

				return (setbooleanvalue (fl, v));
#else
				/* Handle cleanup before returning — no leak on unsupported platforms */
				unlockhandle (hurl);
				disposehandle (hurl);

				copystring (BIGSTRING("\psys.openUrl is not supported on this platform"), bserror);
				return (false);
#endif
			}
		}

		default:
			break;
		}

	getstringlist (langerrorlist, unimplementedverberror, bserror);

	return (false);
	} /*sysfunctionvalue*/


static boolean launchfunctionvalue (short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror) {
	
	/*
	3/10/92 dmb: added launch.appWithDocument.  check default paths
	
	5/6/93 dmb: make sure that we return false is an oserror has occurred
	*/
	
	register tyvaluerecord *v = vreturned;
	
	setbooleanvalue (false, v); /*assume the worst*/
	
	oserror (noErr); /*clear now so we can check it at end*/
	
	switch (token) { /*these verbs don't need any special globals pushed*/
		
		case applemenufunc: {
			bigstring bs;
			
			flnextparamislast = true;
			
			if (!getstringvalue (hparam1, 1, bs))
				return (false);
			
			(*v).data.flvalue = shellapplemenu (bs);
			
			break;
			}
		
		case launchappfunc: {
			tyfilespec fsapp;
			
			flnextparamislast = true;
			
			if (!getfilespecvalue (hparam1, 1, &fsapp))
				return (false);
			
			(*v).data.flvalue = launchapplication (&fsapp, nil, false);
			
			break;
			}
		
		case launchappwithdocfunc: {
			tyfilespec fsapp, fsdoc;
			
			if (!getfilespecvalue (hparam1, 1, &fsapp))
				return (false);
			
			flnextparamislast = true;
			
			if (!getfilespecvalue (hparam1, 2, &fsdoc))
				return (false);
			
			(*v).data.flvalue = launchapplication (&fsapp, &fsdoc, false);
			
			break;
			}
		
		case executeresourcefunc: {
			ResType type;
			short id;
			
			if (!getostypevalue (hparam1, 1, &type))
				return (false);
			
			flnextparamislast = true;
			
			if (!getintvalue (hparam1, 2, &id))
				return (false);
			
			(*v).data.flvalue = executeresource (type, id, nil);
			
			break;
			}
		
		case anythingfunc:
				return (filelaunchanythingverb (hparam1, v));
		
		default:
			getstringlist (langerrorlist, unimplementedverberror, bserror);

			return (false);
		}
	
	return (getoserror () == noErr);
	} /*launchfunctionvalue*/


extern long fullpathloopcount;

extern boolean hashstatsverb (tyvaluerecord *v);

static boolean frontierfunctionvalue (short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror) {
#pragma unused (bserror)

	/*
	6/5/92 dmb: added isruntime func
	
	6/1/93 dmb: when vreturned is nil, return whether or not verb token must 
	be run in the Frontier process
	*/
	
	register tyvaluerecord *v = vreturned;
	
	if (v == nil) { /*need Frontier process?*/
		
		switch (token) {
			
			case requesttofrontfunc:
				return (true);
			
			default:
				return (false);
			}
		}
	
	setbooleanvalue (false, v); /*assume the worst*/
	
	switch (token) { /*these verbs don't need any special globals pushed*/
		
		case programpathfunc: {
			
			if (!langcheckparamcount (hparam1, 0))
				return (false);
			
			return (setfilespecvalue (&programfspec, v));
			
			}
		
		case filepathfunc: {
			tyfilespec fs;
			
			if (!langcheckparamcount (hparam1, 0))
				return (false);
			
			
			shellpushfrontrootglobals ();
			
			windowgetfspec (shellwindow, &fs);
			
			shellpopglobals ();
			
			
			return (setfilespecvalue (&fs, v));
			}
		
		
		case agentsenablefunc: {
			boolean fl;
			
			flnextparamislast = true;
			
			if (!getbooleanvalue (hparam1, 1, &fl))
				return (false);
			
			(*v).data.flvalue = setagentsenable (fl);
			
			return (true);
			}
		
		
		case requesttofrontfunc: {
			bigstring bsmessage;
			
			flnextparamislast = true;
			
			if (!getstringvalue (hparam1, 1, bsmessage))
				return (false);

				(*v).data.flvalue = shellisactive () || notifyuser (bsmessage);


			return (true);
			}
		
		case isruntimefunc: {
			if (!langcheckparamcount (hparam1, 0))
				return (false);
			
			
			return (true);
			}
		
		case countthreadsfunc: {
			if (!langcheckparamcount (hparam1, 0))
				return (false);
			
			return (setlongvalue (processthreadcount (), v));
			}
		
		case isnativefunc:
			#if __powerc || __GNUC__
				(*v).data.flvalue = true;
			#else
			
				(*v).data.flvalue = false;
			#endif
			
			return (true);
		
		case reclaimmemoryfunc: {
			long ctbytes = longinfinity;

			if (!langcheckparamcount (hparam1, 0))
				return (false);
			
			hashflushcache (&ctbytes);

			return (setlongvalue (longinfinity - ctbytes, v));
			}
		
		case frontierversionfunc:
			return (frontierversion (v));
		
		case hashstatsfunc:{
			if (!langcheckparamcount (hparam1, 0))
				return (false);

			return (hashstatsverb (v));
			}

		case gethashloopcountfunc:{
			long myx;

			if (!langcheckparamcount (hparam1, 0))
				return (false);
				
			myx = fullpathloopcount;

			fullpathloopcount = 0;

			return (setlongvalue (myx, v));
			}

		case hideapplicationfunc: { /*7.1b9 PBS: minimize to system tray*/
			

			return (setbooleanvalue (true, v));
			}

		case isvalidserialnumberfunc: { /*7.1b34 dmb: expose the functionality as a verb*/
			bigstring bssn;
			
			flnextparamislast = true;
			
			if (!getstringvalue (hparam1, 1, bssn))
				return (false);

			(*v).data.flvalue = isvalidserialnumber (bssn);

			return (true);
			}
		
		case showapplicationfunc: {	/*2004-11-28 aradke: re-emerge from system tray*/


			return (setbooleanvalue (true, v));
			}

		default:
			return (false);
		}
	} /*frontierfunctionvalue*/


static boolean clipboardfunctionvalue (short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror) {
	
	register tyvaluerecord *v = vreturned;
		typrocessid processid;
	
	setbooleanvalue (false, v); /*assume the worst*/
	
		processid = getcurrentprocessid ();
		
		if (!isfrontapplication (processid)) {
			
			getstringlist (langerrorlist, cantbackgroundclipboard, bserror);
			
			return (false);
			}
	
	switch (token) { /*these verbs don't need any special globals pushed*/
		
		case getscrapfunc:
			return (getscrapverb (hparam1, v));
		
		case putscrapfunc:
			return (putscrapverb (hparam1, v));
		
		default:
			return (false);
		}
	} /*clipboardfunctionvalue*/


static boolean getthreadvalue (hdltreenode hfirst, short pnum, hdlprocessthread *hthread) {
	
	long id;
	
	if (!getlongvalue (hfirst, pnum, &id))
		return (false);
	
	*hthread = getprocessthread (id);
	
	if (*hthread == nil) {
		
		langlongparamerror (badthreadiderror, id);
		
		return (false);
		}
	
	return (true);
	} /*getthreadvalue*/


static boolean threadverbprocessstarted (void) {
	
	/*
	we don't want Frontier's menus to dim when thread.evaluate's newly-added 
	process start.
	*/
	
	processnotbusy ();
	
	return (true);
	} /*threadverbprocessstarted*/


static boolean threaddisposecontext (void) {
	/*
	6.1b7 AR: For thread.callScript, dispose the context table.
	*/

	register hdlprocessrecord hp = currentprocess;

	if (hp != nil && (**hp).hcontext != nil)
		disposehashtable ((**hp).hcontext, true);

	return (true);
	}/**/


static boolean threadcallscriptverb (bigstring bsscriptname, tyvaluerecord vparams, hdlhashtable hcontext, tyvaluerecord *v) {

	/*
	8.0.4 dmb: handle running code values
	
	9.1b3 AR: copy bsscriptname to processrecord so the thread can be more easily
	identified in the system.compiler.threads table
	*/
	
	hdlprocessrecord hp;
	hdlprocessthread hthread;

	bigstring bsverb;
	boolean fl = false;
	boolean flchained = false;
	tyvaluerecord val;
	hdltreenode hfunctioncall;
	hdltreenode hparamlist;
	hdltreenode hcode;
	hdlhashtable htable;
	tyvaluerecord vhandler;
	hdlhashnode handlernode;
	
	/*build code tree, see langrunscript*/

	pushhashtable (roottable);
	
	fl = langexpandtodotparams (bsscriptname, &htable, bsverb);

	if (fl && htable == nil)
		langsearchpathlookup (bsverb, &htable);

	pophashtable();
	
	if (!fl)
		goto exit;
	
	if (!hashtablelookupnode (htable, bsverb, &handlernode)) {
		
		langparamerror (unknownfunctionerror, bsverb);
		
		goto exit;
		}
	
	vhandler = (**handlernode).val;
	
	/*build a code tree and call the handler, with our error hook in place*/
	
	hcode = nil;
	
	if (vhandler.valuetype == codevaluetype) {

		hcode = vhandler.data.codevalue;
	}
	else if ((**htable).valueroutine == nil) { /*not a kernel table*/
		
		if (!langexternalvaltocode (vhandler, &hcode)) {

			langparamerror (notfunctionerror, bsverb);

			goto exit;
			}
		
		if (hcode == nil) { /*needs compilation*/
			
			if (!langcompilescript (handlernode, &hcode))
				goto exit;
			}
		}
	
	if (!setaddressvalue (htable, bsverb, &val))
		goto exit;
	
	if (!pushfunctionreference (val, &hfunctioncall))
		goto exit;
	
	if (hcontext != nil) {
		
		flchained = (**hcontext).flchained;
		
		if (flchained)
			pushhashtable (hcontext);
		else
			chainhashtable (hcontext); /*establishes outer local context*/
		}

	fl = langbuildparamlist (&vparams, &hparamlist);
	
	if (hcontext != nil) {
		
		if (flchained)
			pophashtable ();
		else
			unchainhashtable ();
		}
	
	if (!fl) {
		
		langdisposetree (hfunctioncall);
		
		goto exit;
		}
	
	if (!pushfunctioncall (hfunctioncall, hparamlist, &hcode)) /*consumes input parameters*/
		goto exit;

	if (!pushbinaryoperation (moduleop, hcode, nil, &hcode)) /*needs this level???*/
		goto exit;

	/*launch separate process, see processruntext*/
	
	newlyaddedprocess = nil; //process manager global

	if (!addnewprocess (hcode, true, nil, (long) 0)) {
		
		langdisposetree (hcode);
		
		goto exit;
		}
	
	/*return thread id*/

	hp = newlyaddedprocess; //process.c global; will be nil if a process wasn't just added
			
	if ((hp == nil) || !scheduleprocess (hp, &hthread))
		return (setlongvalue (0, v));
			
	(**hp).processstartedroutine = &threadverbprocessstarted; //don't dim the menu bar

	copystring (bsscriptname, (**hp).bsname);	/* 9.1b3 AR */

	if (hcontext != nil) {

		Handle hpacked;
		boolean fldummy;

		/*make a copy of the context table*/

		if (!tablepacktable (hcontext, true, &hpacked, &fldummy))
			goto exit;

		if (!tableunpacktable (hpacked, true, &hcontext))
			goto exit;		

		/*set the child thread's context to the copy of the context table*/

		(**hp).hcontext = hcontext;

		/*make sure the copy of the context table will be disposed*/

		(**hp).processkilledroutine = &threaddisposecontext;
		}

	return (setlongvalue (getthreadid (hthread), v));

exit:

	return (false);
	}/*threadcallscriptverb*/


/*
 * Public wrapper functions for thread verb kernel access from headless mode.
 * These expose the internal thread management functions for use by headless_thread_verbs.c
 */

boolean thread_callscript_kernel(bigstring bsscriptname,
                                  tyvaluerecord vparams,
                                  hdlhashtable hcontext,
                                  tyvaluerecord *vreturned) {
	/*
	 * Public wrapper for threadcallscriptverb.
	 * Allows headless verb stubs to create threads without duplicating code.
	 */
	return threadcallscriptverb(bsscriptname, vparams, hcontext, vreturned);
}


boolean thread_evaluate_kernel(bigstring bscode, tyvaluerecord *vreturned) {
	/*
	 * Public wrapper for thread.evaluate functionality.
	 * Evaluates a code string in a new thread and returns the thread ID.
	 */
	Handle htext;
	hdlprocessrecord hp;
	hdlprocessthread hthread;

	/* Convert bigstring to handle */
	if (!newtexthandle(bscode, &htext))
		return false;

	newlyaddedprocess = nil; /* process manager global */

	if (!processruntext(htext)) {
		disposehandle(htext);
		return false;
	}

	hp = newlyaddedprocess; /* process.c global; will be nil if a process wasn't just added */

	if ((hp == nil) || !scheduleprocess(hp, &hthread)) {
		disposehandle(htext);
		return setlongvalue(0, vreturned);
	}

	(**hp).processstartedroutine = &threadverbprocessstarted;

	disposehandle(htext);
	return setlongvalue(getthreadid(hthread), vreturned);
}


static boolean threadstatsverb (hdltreenode hparam1, tyvaluerecord *v) {
	
	/*
	6.2b6 AR: New verb for debugging thread-related problems
	*/

	hdlhashtable htable, hstatstable;
	bigstring bs;

	flnextparamislast = true;

	if (!getvarparam (hparam1, 1, &htable, bs))
		return (false);

	if (!langsuretablevalue (htable, bs, &hstatstable))
		return (false);
		
	if (!processgetstats (hstatstable))
		return (false);
		
	return (setbooleanvalue (true, v));
	}/*threadstatsverb*/


static boolean threadfunctionvalue (short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror) {
	
	/*
	4.1b3 dmb: new verbs
	
	4.1b5 dmb: added thread.sleep
	
	4.1b6 dmb: make thread.sleepFor take seconds, not ticks
	
	5.0d13 dmb: added v == nil check
	*/
	
	register tyvaluerecord *v = vreturned;
	typrocessid processid;
	unsigned long ticks;
	
	if (v == nil) { /*need Frontier process?*/
		
		switch (token) {
			
			case evaluatefunc:
			case callscriptfunc:
			case sleepfunc:
			case sleepforfunc:
			case sleepticksfunc:
			case issleepingfunc:
			case wakefunc:
			case killfunc:
			/*
			case begincriticalfunc:
			case endcriticalfunc:
			*/
			case statsfunc:
				return (true);
			
			case existsfunc:
			case getcurrentfunc:
			case getcountfunc:
			case getnththreadfunc:
			case gettimeslicefunc:
			case getdefaulttimeslicefunc:
			case settimeslicefunc:
			case setdefaulttimeslicefunc:
			default:
				return (false);
			}
		}
	
	setbooleanvalue (false, v); // assume the worst
	
	processid = getcurrentprocessid ();
	
	if (!iscurrentapplication (processid)) {
		
		getstringlist (langerrorlist, cantbackgroundclipboard, bserror);	// ***
		
		return (false);
		}
	
	switch (token) {
		
		case existsfunc: {
			long id;
			
			flnextparamislast = true;
			
			if (!getlongvalue (hparam1, 1, &id))
				return (false);
			
			return (setbooleanvalue (getprocessthread (id) != nil, v));
			}
		
		case evaluatefunc: {
			Handle htext;
			hdlprocessrecord hp;
			hdlprocessthread hthread;
			
			flnextparamislast = true;
			
			if (!getexempttextvalue (hparam1, 1, &htext))
				return (false);
			
			newlyaddedprocess = nil; //process manager global
			
			if (!processruntext (htext))
				return (false);
			
			hp = newlyaddedprocess; //process.c global; will be nil if a process wasn't just added
			
			if ((hp == nil) || !scheduleprocess (hp, &hthread))
				return (setlongvalue (0, v));
			
			(**hp).processstartedroutine = &threadverbprocessstarted;

			return (setlongvalue (getthreadid (hthread), v));
			}
		
		case callscriptfunc: {

			bigstring bsscriptname;
			tyvaluerecord vparams;
			hdlhashtable hcontext = nil;
			boolean fl;
	
			if (!getstringvalue (hparam1, 1, bsscriptname))
				return (false);
	
			if (!getparamvalue (hparam1, 2, &vparams))
				return (false);
	
			if (vparams.valuetype != recordvaluetype)
				if (!coercetolist (&vparams, listvaluetype))
					return (false);
	
			if (langgetparamcount (hparam1) > 2) {
		
				flnextparamislast = true;

				if (!gettablevalue (hparam1, 3, &hcontext))
					return (false);
				}
				
			(**(getcurrentthreadglobals ())).debugthreadingcookie = token;

			fl = threadcallscriptverb (bsscriptname, vparams, hcontext, v);
			
			(**(getcurrentthreadglobals ())).debugthreadingcookie = 0;

			return (fl);
			}

		case getcurrentfunc:
			if (!langcheckparamcount (hparam1, 0))
				return (false);
			
			return (setlongvalue (getthreadid (getcurrentthread ()), v));
		
		case getcountfunc:
			if (!langcheckparamcount (hparam1, 0))
				return (false);
			
			return (setlongvalue (processthreadcount (), v));
		
		case getnththreadfunc: {
			short n;
			
			flnextparamislast = true;
			
			if (!getintvalue (hparam1, 1, &n))
				return (false);
			
			return (setlongvalue (getthreadid (nthprocessthread (n)), v));
			}
		
		case sleepfunc: {
			hdlprocessthread hthread;
			
			flnextparamislast = true;
			
			if (!getthreadvalue (hparam1, 1, &hthread))
				return (false);
			
			return (setbooleanvalue (processsleep (hthread, -1), v));
			}
		
		case sleepforfunc: {
			long n;
			boolean fl;
			
			flnextparamislast = true;
			
			if (!getlongvalue (hparam1, 1, &n))
				return (false);
			
			(**(getcurrentthreadglobals ())).debugthreadingcookie = token;

			fl = processsleep (getcurrentthread (), n * 60);
			
			(**(getcurrentthreadglobals ())).debugthreadingcookie = 0;

			return (setbooleanvalue (fl, v));
			}
		
		case sleepticksfunc: {
			long n;
			boolean fl;
			
			flnextparamislast = true;
			
			if (!getlongvalue (hparam1, 1, &n))
				return (false);
			
			(**(getcurrentthreadglobals ())).debugthreadingcookie = token;

			fl = processsleep (getcurrentthread (), n);
			
			(**(getcurrentthreadglobals ())).debugthreadingcookie = 0;

			return (setbooleanvalue (fl, v));
			}

		case issleepingfunc: {
			hdlprocessthread hthread;
			
			flnextparamislast = true;
			
			if (!getthreadvalue (hparam1, 1, &hthread))
				return (false);
			
			return (setbooleanvalue (processissleeping (hthread), v));
			}
		
		case wakefunc: {
			hdlprocessthread hthread;
			
			flnextparamislast = true;
			
			if (!getthreadvalue (hparam1, 1, &hthread))
				return (false);
			
			return (setbooleanvalue (wakeprocessthread (hthread), v));
			}
		
		case killfunc: {
			hdlprocessthread hthread;
			
			flnextparamislast = true;
			
			if (!getthreadvalue (hparam1, 1, &hthread))
				return (false);
			
			return (setbooleanvalue (killprocessthread (hthread), v));
			}
		
		case gettimeslicefunc:
			if (!langcheckparamcount (hparam1, 0))
				return (false);
			
			getprocesstimeslice (&ticks);

			return (setlongvalue (ticks, v));
		
		case settimeslicefunc:
			flnextparamislast = true;

			if (!getlongvalue (hparam1, 1, (long *) (&ticks)))
				return (false);
			
			return (setbooleanvalue (setprocesstimeslice (ticks), v));
	
		case getdefaulttimeslicefunc:
			if (!langcheckparamcount (hparam1, 0))
				return (false);
			
			getdefaulttimeslice (&ticks);

			return (setlongvalue (ticks, v));
		
		case setdefaulttimeslicefunc:
			flnextparamislast = true;

			if (!getlongvalue (hparam1, 1, (long *) (&ticks)))
				return (false);
			
			return (setbooleanvalue (setdefaulttimeslice (ticks), v));
		/*
		case begincriticalfunc:
			if (!langcheckparamcount (hparam1, 0))
				return (false);
			
			++fldisableyield;
			
			return (setbooleanvalue (true, v));
		
		case endcriticalfunc:
			if (!langcheckparamcount (hparam1, 0))
				return (false);
			
			if (fldisableyield > 0) {
			
				--fldisableyield;
				
				(*v).data.flvalue = true;
				}
			
			return (true);
		*/
		case statsfunc:
			return (threadstatsverb (hparam1, v));

		default:
			return (false);
		}
	} /*threadfunctionvalue*/


boolean sysinitverbs (void) {

	getapplicationfilespec (nil, &programfspec);
	
	launchcallbacks.waitcallback = &shellsysverbwaitroutine;
	
	if (!loadfunctionprocessor (idsysverbs, &sysfunctionvalue))
		return (false);
	
	if (!loadfunctionprocessor (idlaunchverbs, &launchfunctionvalue))
		return (false);
	
	if (!loadfunctionprocessor (idclipboardverbs, &clipboardfunctionvalue))
		return (false);
	
	if (!loadfunctionprocessor (idfrontierverbs, &frontierfunctionvalue))
		return (false);
	
	if (!loadfunctionprocessor (idthreadverbs, &threadfunctionvalue))
		return (false);
	
	return (true);
	
	} // sysinitverbs

