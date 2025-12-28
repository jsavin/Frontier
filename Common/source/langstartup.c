
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

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "file.h"
#include "ops.h"
#include "strings.h"
#include "shellhooks.h"
#include "langexternal.h"
#include "langinternal.h"
#include "langtokens.h"
#include "tableinternal.h"
#include "tableverbs.h"
#include "logging.h"

// 2025-10-27 Codex: Headless runtime should populate builtins table like classic app.
#include "tablestructure.h"
#include "resources.h"
#include "WinSockNetEvents.h"
#include "sysshellcall.h" /* 2006-03-09 aradke: unixshellcall moved from CallMachOFramework.h */
#include "byteorder.h"	/* 2006-04-16 aradke: swap byte-order in loadfunctionprocessor */
#include <stdlib.h>
#ifdef FRONTIER_HEADLESS
#include <stdio.h>
#endif

// 2025-12-02 Codex: Log headless EFP registration to chase missing kernel valueroutines.

#ifdef FRONTIER_HEADLESS
static int headless_should_log(void) {
    static int inited = 0;
    static int enabled = 0;
    if (!inited) {
        const char *e = getenv("FRONTIER_HEADLESS_LOG");
        enabled = (e && *e) ? 1 : 0;
        inited = 1;
    }
    return enabled;
}
#endif

/* Typed headless no-op callbacks to avoid UB from casted function pointers */
static boolean cb_noop_void(void) { return true; }
static boolean cb_true_bool(boolean a) { (void)a; return true; }
static boolean cb_false_bool(boolean a) { (void)a; return false; }
static boolean cb_noop_treenode(hdltreenode n) { (void)n; return true; }
static boolean cb_noop_treenodes(hdltreenode a, hdltreenode b) { (void)a; (void)b; return true; }
static boolean cb_noop_address(hdlhashtable ht, const bigstring bs) { (void)ht; (void)bs; return true; }
static boolean cb_noop_symbolinserted(hdlhashtable ht, const bigstring bs, hdlhashnode hn) { (void)ht; (void)bs; (void)hn; return true; }
static boolean cb_noop_tablenode(hdlhashtable ht, hdlhashnode hn) { (void)ht; (void)hn; return true; }
static boolean cb_noop_symbolchanged(hdlhashtable ht, const bigstring bs, hdlhashnode hn, boolean fl) { (void)ht; (void)bs; (void)hn; (void)fl; return true; }
static short   cb_noop_compare(hdlhashtable ht, hdlhashnode h1, hdlhashnode h2) { (void)ht; (void)h1; (void)h2; return 0; }
static boolean cb_noop_hashnode_treenode(hdlhashnode hn, hdltreenode* p) { (void)hn; (void)p; return true; }
static boolean __attribute__((unused)) cb_noop_tableref(hdlhashtable* pht) { (void)pht; return true; }
static boolean __attribute__((unused)) cb_noop_table(hdlhashtable ht) { (void)ht; return true; }
static boolean cb_noop_sourcecode(hdlhashtable ht, hdlhashnode hn, bigstring bs) { (void)ht; (void)hn; (void)bs; return true; }
static boolean cb_noop_errmsg(bigstring bs, ptrvoid refcon) { (void)bs; (void)refcon; return true; }
static boolean cb_noop_verb(hdltreenode n, tyvaluerecord* v) { (void)n; (void)v; return true; }
static boolean cb_false_short(UInt16 x) { (void)x; return false; }
static boolean cb_false_event(EventRecord* e) { (void)e; return false; }


#define str_isPike				BIGSTRING ("\x06" "isPike")
#define str_isOpmlEditor        BIGSTRING ("\x0c" "isOpmlEditor")    /* 2005-04-06 dluebbert */
#define str_isFrontier          BIGSTRING ("\x0a" "isFrontier")      /* 2005-04-06 dluebbert */
#define str_isRadio				BIGSTRING ("\x07" "isRadio")
#define str_isMac				BIGSTRING ("\x05" "isMac")
#define str_isMacOsClassic		BIGSTRING ("\x0e" "isMacOsClassic") /* 2004-11-19 creedon */
#define str_isServer			BIGSTRING ("\x08" "isServer") /* 2004-11-19 creedon */
#define str_isWindows			BIGSTRING ("\x09" "isWindows")
#define str_osFlavor			BIGSTRING ("\x08" "osFlavor")
#define str_osMajorVersion		BIGSTRING ("\x0e" "osMajorVersion")
#define str_osMinorVersion		BIGSTRING ("\x0e" "osMinorVersion")
#define str_osPointVersion		BIGSTRING ("\x0e" "osPointVersion") /* 2004-11-19 creedon */
#define str_osBuildNumber		BIGSTRING ("\x0d" "osBuildNumber")
#define str_osVersionString		BIGSTRING ("\x0f" "osVersionString")
#define str_osFullNameForDisplay	BIGSTRING ("\x14" "osFullNameForDisplay")
#define str_winServicePackNumber	BIGSTRING ("\x14" "winServicePackNumber")
#define str_isCarbon				BIGSTRING ("\x08" "isCarbon")
#define str_maxTcpConnections		BIGSTRING ("\x11" "maxTcpConnections")


void initsegment (void) {
	
	} /*initsegment*/


boolean newfunctionprocessor (bigstring bsname, langvaluecallback valuecallback, boolean flwindow, hdlhashtable *htable) {
	
	/*
	each of the external function processors register with the system by calling
	this routine.  we create a new hashtable in the EFP table, and return a
	handle to the new table.  we also link in a callback routine that processes the
	verbs for this EFP.  flwindow is also recorded -- it says whether or not
	the EFP requires a window be open in order for one of its verbs to be
	executed.
	
	6/1/93 dmb: flwindow parameter is now real. when true, the valuecallback must
	respond to being called with a nil parameter list by determining whether or not 
	a specific verb requires Frontier to be the current process.
	*/
	
	register hdlhashtable ht;
	
	if (!tablenewsystemtable (efptable, bsname, htable))
		return (false);
	
	ht = *htable; /*copy into register*/
	
	(**ht).flverbsrequirewindow = flwindow;
	
	(**ht).valueroutine = valuecallback;

#if defined(FRONTIER_HEADLESS)
	log_debug(LOG_COMP_STARTUP, "newfunctionprocessor: name=%s table=%p valueroutine=%p flwindow=%d",
	        PSTR(bsname),
	        (void *) ht,
	        (void *) valuecallback,
	        (int) flwindow);
#endif
	
	return (true);
	} /*newfunctionprocessor*/


static boolean hashinsertcstring (bigstring bs, const tyvaluerecord *v) {

	convertcstring (bs);

	return (hashinsert (bs, *v));
	} /*hashinsertcstring*/


static boolean langaddcstringkeyword (bigstring bs, short tokennumber) {
	
	tyvaluerecord val;
	
	initvalue (&val, tokenvaluetype);
	
	val.data.tokenvalue = (short) tokennumber;
	
	return (hashinsertcstring (bs, &val));
	} /*langaddcstringkeyword*/
	

boolean langaddkeyword (bigstring bs, short tokennumber) {
	
	tyvaluerecord val;
	
	initvalue (&val, tokenvaluetype);
	
	val.data.tokenvalue = (short) tokennumber;
	
	return (hashinsert (bs, val));
	} /*langaddkeyword*/
	
/*
boolean langaddkeywordlist (hdlhashtable htable, byte * bskeywords[], short ctkeywords) {
	
	register short i;
	register boolean fl = true;
	
	pushhashtable (htable); 
	
	for (i = 0;  fl && (i < ctkeywords);  i++)
		fl = langaddkeyword ((ptrstring) bskeywords [i], i);
	
	pophashtable ();
	
	return (fl);
	} /%langaddkeywordlist%/
*/

boolean loadfunctionprocessor (short id, langvaluecallback valuecallback) {
	(void) id;
	(void) valuecallback;
	return true; /* headless-only fork: resource-based verb loading removed */
	} /*loadfunctionprocessor*/


#ifdef FRONTIER_HEADLESS
static boolean initenvironment (hdlhashtable ht) {

	/*
	 * Headless/runtime-test build: populate the environment table with
	 * conservative defaults without touching platform APIs or external
	 * processes. The full application replaces these values at startup.
	 */

	bigstring bs;

	langassignbooleanvalue (ht, str_isMac, false);
	langassignbooleanvalue (ht, str_isWindows, false);
	langassignbooleanvalue (ht, str_isMacOsClassic, false);
	langassignbooleanvalue (ht, str_isServer, false);
	langassignbooleanvalue (ht, str_isCarbon, false);
	langassignbooleanvalue (ht, str_isPike, false);
	langassignbooleanvalue (ht, str_isRadio, false);
	langassignbooleanvalue (ht, str_isOpmlEditor, false);
	langassignbooleanvalue (ht, str_isFrontier, true);

	langassignlongvalue (ht, str_osMajorVersion, 0);
	langassignlongvalue (ht, str_osMinorVersion, 0);
	langassignlongvalue (ht, str_osPointVersion, 0);

	copyctopstring ("0.0.0", bs);
	langassignstringvalue (ht, str_osVersionString, bs);
	langassignstringvalue (ht, str_osFullNameForDisplay, bs);
	langassignstringvalue (ht, str_osBuildNumber, bs);
	langassignstringvalue (ht, str_osFlavor, bs);
	langassignstringvalue (ht, str_winServicePackNumber, bs);

	langassignlongvalue (ht, str_maxTcpConnections, 0);

	return (true);
}
#else
static boolean initenvironment ( hdlhashtable ht ) {

	//
	// 2007-06-29 creedon: isMacOsClassic is always false now that we don't
	//				   support the Mac Classic envrionment any longer
	//
	//				   for Mac, bug fix for OS version numbers like 10.4.10
	//
	// 2004-11-19 creedon: added isServer
	//
	//				   for Mac, added osBuildNumber, osFullNameForDisplay
	//				   ( now a calculated value )
	//
	
	bigstring bsos, bsversion;
	boolean isServer;
	
		Handle hcommand, hreturn;
		bigstring bs;
		long x;
		
		newemptyhandle ( &hreturn );
		
		bundle { // system version
		
			getsystemversionstring ( bsversion, NULL );
			
			bundle { // major
			
				bigstring bsSystemVersionMajor;
				
				nthfield ( bsversion, 1, '.', bsSystemVersionMajor );
				
				stringtonumber ( bsSystemVersionMajor, &x );
				
				langassignlongvalue ( ht, str_osMajorVersion, x );
				
				}
					
			bundle { // minor
			
				bigstring bsSystemVersionMinor;
				
				nthfield ( bsversion, 2, '.', bsSystemVersionMinor );
				
				stringtonumber ( bsSystemVersionMinor, &x );
				
				langassignlongvalue ( ht, str_osMinorVersion, x );
				
				}
					
			bundle { // bug fix
			
				bigstring bsSystemVersionBugFix;
				
				nthfield ( bsversion, 3, '.', bsSystemVersionBugFix );
				
				stringtonumber ( bsSystemVersionBugFix, &x );
				
				langassignlongvalue ( ht, str_osPointVersion, x );
				
				}
					
			}
			
		langassignbooleanvalue (ht, str_isMac, true);
		
		langassignbooleanvalue (ht, str_isWindows, false);
		
		// langassignstringvalue (ht, str_osFlavor, zerostring);
		
		langassignbooleanvalue (ht, str_isCarbon, true);
		
		bundle { // get mac os build number
		
			newtexthandle ( "\psw_vers -buildVersion", &hcommand );
			
			unixshellcall ( hcommand, hreturn );
			
			texthandletostring ( hreturn, bs );
			
			sethandlesize ( hreturn, 0 );
			
			setstringlength ( bs, stringlength ( bs ) - 1 );
			
			langassignstringvalue ( ht, str_osBuildNumber, bs );
			
			}
			
		bundle { // get os full display name
		
			copystring ( "\psw_vers -productName", bs ); 
			
			sethandlecontents ( stringbaseaddress ( bs ), stringlength ( bs ), hcommand );
			
			unixshellcall ( hcommand, hreturn );
			
			texthandletostring ( hreturn, bsos );
			
			setstringlength ( bsos, stringlength ( bsos ) - 1 );
			
			}
			
		disposehandle ( hcommand );
		
		disposehandle ( hreturn );
		
		bundle { // is server
		
			if ( equalstrings ( bsos, "\pMac OS X Server" ) )
				isServer = true;
			else
				isServer = false;
			}
			

	
	langassignbooleanvalue ( ht, str_isMacOsClassic, false );
	
	langassignbooleanvalue ( ht, str_isServer, isServer );
		
	langassignlongvalue ( ht, str_maxTcpConnections, maxconnections ); // 7.0b37 PBS: max TCP connections
	
	langassignstringvalue ( ht, str_osFullNameForDisplay, bsos );

	langassignstringvalue ( ht, str_osVersionString, bsversion );

	#ifdef PIKE

		#ifndef OPMLEDITOR
		
			langassignbooleanvalue (ht, str_isPike, true);
			langassignbooleanvalue (ht, str_isRadio, true); /*7.0b37 PBS: system.environment.isRadio*/
			langassignbooleanvalue (ht, str_isOpmlEditor, false); /*2005-04-06 dluebbert: system.environment.isOPML*/
			langassignbooleanvalue (ht, str_isFrontier, false); /*2005-04-06 dluebbert: system.environment.isFrontier*/
			
		#else // OPMLEDITOR
		
			langassignbooleanvalue (ht, str_isPike, false);
			langassignbooleanvalue (ht, str_isRadio, false); /*7.0b37 PBS: system.environment.isRadio*/
			langassignbooleanvalue (ht, str_isOpmlEditor, true); /*2005-04-06 dluebbert: system.environment.isOPML*/
			langassignbooleanvalue (ht, str_isFrontier, false); /*2005-04-06 dluebbert: system.environment.isFrontier*/
			
		#endif // OPMLEDITOR
		
	#else //!PIKE

		langassignbooleanvalue (ht, str_isPike, false);
		langassignbooleanvalue (ht, str_isRadio, false); /*7.0b37 PBS: system.environment.isRadio*/
		langassignbooleanvalue (ht, str_isOpmlEditor, false); /*2005-04-06 dluebbert: system.environment.isOPML*/
		langassignbooleanvalue (ht, str_isFrontier, true); /*2005-04-06 dluebbert: system.environment.isFrontier*/
		
	#endif //!PIKE

	return ( true );
		
}
#endif /* FRONTIER_HEADLESS */



static boolean initCharsetsTable (hdlhashtable cSetsTable)
{
	OSStatus err;
	ItemCount ct, actual_ct, i;
	
	err = TECCountAvailableTextEncodings( &ct );
	if ( err != noErr ) {
		return (true);  // don't kill the whole startup
	}
	
    TextEncoding enc, encOut;
    if (ct == 0) {
        return (true);
    }
    TextEncoding* availEncodings = (TextEncoding*) malloc(sizeof(TextEncoding) * ct);
    if (availEncodings == NULL) {
        return (true);
    }
	bigstring ianaName, displayName;
	unsigned long lenDisplayName;
	RegionCode reg;
	
    err = TECGetAvailableTextEncodings ( availEncodings, ct, &actual_ct );
    if ( err != noErr )
    { free(availEncodings); return (true); }  // we don't want to kill the whole startup here
	
    for ( i = 0; i < actual_ct; i++ ) {
		enc = availEncodings[ i ];
		
		/*
			get the internet "iana" name for the encoding
		*/
		err = TECGetTextEncodingInternetName( enc, ianaName );
		if ( err != noErr )
			continue;
		
		/*
			now convert the "iana" name back into the canonical encoding value
			(more than one encoding will point to the same iana name, 
			this makes sure that we use the right one)
		*/
		err = TECGetTextEncodingFromInternetName( &enc, ianaName );
		if ( err != noErr )
			continue;
		
		/*
			get the encoding's display name, which is the value of the table cell
		*/
		err = GetTextEncodingName( enc, kTextEncodingFullName, verUS, kTextEncodingMacRoman, 255, &lenDisplayName, &reg, &encOut, (TextPtr) displayName + 1 );
		if ( err != noErr )
			continue;
		
		displayName[ 0 ] = (unsigned char) lenDisplayName;
		
		langassignstringvalue( cSetsTable, ianaName, displayName );
	}
	
	return (true);
	

	} /* initCharsetsTable */


boolean inittablestructure (void) {
	
	/*
	do this just after you initialize lang.c.  
	
	we build the initial structure of hashtables for CanCoon.
	
	5.1.4 dmb: do the environmenttable
	*/
	
	hdlhashtable htable; 
	
    if (!newhashtable (&htable)) /*this is where everything starts*/
        return (false);

    /*
     * Headless/runtime context: establish the process-global root table.
     * The classic app path sets this during database/file open, but our
     * headless test harness needs a usable root for scope chains.
     */
    roottable = htable;

    pushhashtable (htable); /*set lang.c global*/
	
	// do the compiler table
	
	if (!tablenewsystemtable (htable, nameinternaltable, &internaltable))
		goto error;
	
	if (!tablenewsystemtable (internaltable, nameefptable, &efptable))
		goto error;
	
	if (!tablenewsystemtable (internaltable, namelangtable, &langtable))
		goto error;
	
	if (!tablenewsystemtable (internaltable, namestacktable, &runtimestacktable))
		goto error;
	
	if (!tablenewsystemtable (internaltable, namesemaphoretable, &semaphoretable))
		goto error;
	
		if (!tablenewsystemtable (internaltable, namethreadtable, &threadtable))
			goto error;
		
		if (!tablenewsystemtable (internaltable, namefilewindowtable, &filewindowtable))
			goto error;
	
	// now do the environment table
	
	if (!tablenewsystemtable (htable, nameenvironmenttable, &environmenttable))
		goto error;
	
	if (!initenvironment (environmenttable))
		goto error;
	
	// and now the charsets table
	
	if (!tablenewsystemtable (htable, namecharsetstable, &charsetstable))
		goto error;
	
	initCharsetsTable (charsetstable);  /* don't die if the charsets can't be initialized */
	
	pophashtable ();
	
	return (true);
	
	error:
	
	pophashtable ();
	
	disposehashtable (htable, false);
	
	internaltable = efptable = langtable = runtimestacktable = semaphoretable = threadtable = filewindowtable = environmenttable = nil;
	
	return (false);
	} /*inittablestructure*/


static boolean langaddnilconst (bigstring bs) {
	
	tyvaluerecord val;
	
	initvalue (&val, novaluetype);
	
	return (hashinsert (bs, val));
	} /*langaddnilconst*/


static boolean langaddlongconst (bigstring bs, long x) {
	
	tyvaluerecord val;
	
	setlongvalue (x, &val);
	
	return (hashinsert (bs, val));
	} /*langaddlongconst*/


static boolean langaddstringconst (bigstring bs, bigstring x) {
	
	tyvaluerecord val;
	
	setstringvalue (x, &val);
	
	if (!hashinsert (bs, val))
		return (false);
	
	exemptfromtmpstack (&val);
	
	return (true);
	} /*langaddstringconst*/


	

static boolean langadddirectionconst (bigstring bs, tydirection x) {
	
	tyvaluerecord val;
	
	setdirectionvalue (x, &val);
	
	return (hashinsert (bs, val));
	} /*langadddirectionconst*/


static boolean langaddbooleanconst (bigstring bs, boolean x) {
	
	tyvaluerecord val;
	
	setbooleanvalue (x, &val);
	
	return (hashinsert (bs, val));
	} /*langaddbooleanconst*/


static boolean langaddtypeconst (bigstring bs, tyvaluetype x) {
	
	tyvaluerecord val;
	
	setostypevalue (langgettypeid (x), &val);
	
    return (hashinsert (bs, val));
    } /*langaddtypeconst*/

/* C-string helpers: safely convert to bigstring on stack for constant insertion */
static boolean add_nil_c (const char *name) {
    bigstring _bs; copyctopstring(name, _bs); return langaddnilconst(_bs);
}
static boolean add_long_c (const char *name, long x) {
    bigstring _bs; copyctopstring(name, _bs); return langaddlongconst(_bs, x);
}
static boolean add_dir_c (const char *name, tydirection d) {
    bigstring _bs; copyctopstring(name, _bs); return langadddirectionconst(_bs, d);
}
static boolean __attribute__((unused)) add_bool_c (const char *name, boolean b) {
	bigstring _bs; copyctopstring(name, _bs); return langaddbooleanconst(_bs, b);
}
static boolean __attribute__((unused)) add_type_c (const char *name, tyvaluetype t) {
	bigstring _bs; copyctopstring(name, _bs); return langaddtypeconst(_bs, t);
}
static boolean add_string_const_c (const char *name, bigstring val) {
    bigstring _bs; copyctopstring(name, _bs); return langaddstringconst(_bs, val);
}
static boolean add_keyword_c (const char *name, short token) {
    bigstring _bs; copyctopstring(name, _bs); return langaddkeyword(_bs, token);
}


#ifdef FRONTIER_HEADLESS
#define add(x,y) if (!add_keyword_c ((x), (y))) return (false)
#else
#define add(x,y) if (!langaddcstringkeyword ((ptrstring) x, y)) return (false)
#endif

/* Avoid writing into string literals: use cstring wrappers */
#define addnil(x) if (!add_nil_c (x)) return (false)

#define addlong(x,y) if (!add_long_c (x, y)) return (false)

#define addint(x,y) if (!langaddintconst ((ptrstring) x, y)) return (false)

#define adddirection(x,y) if (!add_dir_c (x, y)) return (false)

#define addboolean(x,y) if (!langaddbooleanconst ((ptrstring) x, y)) return (false)

#define addtype(x,y) if (!langaddtypeconst ((ptrstring) x, y)) return (false)

#define addstring(x,y) if (!langaddstringconst ((ptrstring) x, y)) return (false)


static boolean langinitconsttable (void) {
	
	/*
	2.1b1 dmb: added shortType as a special case; redundant with intType, 
	but more consistent with user termiology
	
	2.1b2 dmb: added nil constant
	*/
	
	tyvaluetype type;
	bigstring bs;

#ifdef FRONTIER_HEADLESS
	/* Headless: initialize a minimal but useful constants table. */
#endif
	
#ifdef FRONTIER_HEADLESS
	log_debug(LOG_COMP_STARTUP, "langinitconsttable: start");
#endif
    if (!tablenewsystemtable (langtable, (ptrstring) "\x09" "constants", &hconsttable))
        return (false);

    pushhashtable (hconsttable); /*converted to constants by the scanner*/

#ifdef FRONTIER_HEADLESS
	log_debug(LOG_COMP_STARTUP, "constants: adding nil/booleans/directions");
#endif
    addnil ("nil");

    addlong ("infinity", longinfinity);

    adddirection ("up", up);

    adddirection ("down", down);

    adddirection ("left", left);

    adddirection ("right", right);

    adddirection ("flatup", flatup);

    adddirection ("flatdown", flatdown);

    adddirection ("nodirection", nodirection);

    adddirection ("pageup", pageup);

    adddirection ("pagedown", pagedown);

    adddirection ("pageleft", pageleft);

    adddirection ("pageright", pageright);

    addboolean (bstrue, (boolean) true);

    addboolean (bsfalse, (boolean) false);

#ifdef FRONTIER_HEADLESS
	log_debug(LOG_COMP_STARTUP, "constants: expanding full type constants");
#endif
	
	for (type = novaluetype; type < ctvaluetypes; type++)
		
		switch (type) {
			
			case olddoublevaluetype: /*don't define constants for these types*/
			case externalvaluetype:
			
			
			case headvaluetype:
			
			
			case oldstringvaluetype: /*8/13*/
			case passwordvaluetype: /*9/17*/
			case unused2valuetype:
				break;
			
			default:
				langgettypestring (type, bs);
				
				lastword (bs, chspace, bs);
				
				pushstring ((ptrstring) "\x04Type", bs);
				
				addtype (bs, type);
				
				break;
    }
    addtype (BIGSTRING ("\x09shortType"), intvaluetype); /*special case to match coercion verb*/
	
    add_string_const_c ("machinePPC", machinePPC);
    add_string_const_c ("machine68K", machine68K);
    add_string_const_c ("machineX86", machinex86);
	
    add_string_const_c ("osMacOS", osMacOS);
	//Code change by Timothy Paustian Tuesday, July 11, 2000 9:41:39 PM
	//We add a detection for the carbon environment
    add_string_const_c ("osMacCn", osCarbon);
    add_string_const_c ("osWin95", osWin95);
    add_string_const_c ("osWinNT", osWinNT);
	
	pophashtable ();
    
    return (true);
    } /*langinitconsttable*/


static boolean langinitbuiltintable (void) {
	
	if (!tablenewsystemtable (langtable, (ptrstring) "\x08" "builtins", &hbuiltinfunctions))
		return (false);

#ifdef FRONTIER_HEADLESS
	log_debug(LOG_COMP_STARTUP, "langinitbuiltintable: installing builtins (headless)");
#endif

	pushhashtable (hbuiltinfunctions); /*converted to function ops by the parser*/
	
	add ("appleevent", appleeventfunc);
	
	add ("complexevent", complexeventfunc);
	
	add ("finderevent", findereventfunc);
	
	add ("tableevent", tableeventfunc);
	
	add ("objspec", objspecfunc);
	
	add ("setobj", setobjspecfunc);
	
	add ("pack", packfunc);
	
	add ("unpack", unpackfunc);
	
	add ("defined", definedfunc);
	
	add ("typeof", typeoffunc);
	
	add ("sizeof", sizeoffunc);
	
	add ("nameof", nameoffunc);
	
	add ("parentof", parentoffunc);

	add ("indexof", indexoffunc);
	
	add ("gestalt", gestaltfunc);
	
	add ("syscrash", syscrashfunc);
	
	
	add ("myMoof", myMooffunc);
	
	
	pophashtable ();
	
	return (true);
	} /*langinitbuiltintable*/


static boolean langinitkeywordtable (void) {
	
	/*
	3/6/92 dmb: added "with" token
	*/
	
	if (!tablenewsystemtable (langtable, (ptrstring) "\x08" "keywords", &hkeywordtable))
		return (false);


#ifdef FRONTIER_HEADLESS
	log_debug(LOG_COMP_STARTUP, "langinitkeywordtable: installing keywords (headless)");
    /* Avoid writing into string literals; build a C string in bigstring buffer. */
    #define ADD_KW(name, tok) do { bigstring _bs; memset(_bs, 0, sizeof(_bs)); strncpy((char*)_bs, (name), lenbigstring); if (!langaddcstringkeyword(_bs, (tok))) return (false); } while(0)
#else
    #define ADD_KW(name, tok) add((name), (tok))
#endif

    pushhashtable (hkeywordtable); /*converted to tokens by the scanner*/
	
    ADD_KW ("equals", equalsfunc);
	
    ADD_KW ("notequals", notequalsfunc);
	
    ADD_KW ("greaterthan", greaterthanfunc);
	
    ADD_KW ("lessthan", lessthanfunc);
	
    ADD_KW ("not", notfunc);
	
    ADD_KW ("and", andfunc);
	
    ADD_KW ("or", orfunc);
	
    ADD_KW ("beginswith", beginswithfunc);
	
    ADD_KW ("endswith", endswithfunc);
	
    ADD_KW ("contains", containsfunc);
	
    ADD_KW ("loop", loopfunc);
	
    ADD_KW ("fileloop", fileloopfunc);
	
    ADD_KW ("while", whilefunc);
	
    ADD_KW ("in", infunc);
	
    ADD_KW ("break", breakfunc);
	
    ADD_KW ("continue", continuefunc);
	
    ADD_KW ("return", returnfunc);
	
    ADD_KW ("if", iffunc);
	
    ADD_KW ("then", thenfunc);
	
    ADD_KW ("else", elsefunc);
	
    ADD_KW ("bundle", bundlefunc);
	
    ADD_KW ("local", localfunc);
	
    ADD_KW ("on", onfunc);
	
    ADD_KW ("case", casefunc);
	
    ADD_KW ("kernel", kernelfunc);
	
    ADD_KW ("for", forfunc);
	
    ADD_KW ("to", tofunc);
	
    ADD_KW ("downto", downtofunc);
	
    ADD_KW ("with", withfunc);
	
    ADD_KW ("try", tryfunc);
    
    pophashtable ();
    
    #undef ADD_KW
    return (true);
	} /*langinitkeywordtable*/


static boolean langinstallresources (void) {
	
	if (!langinitconsttable ())
		return (false);
	
	if (!langinitbuiltintable ())
		return (false);
	
	if (!langinitkeywordtable ())
		return (false);
	
	return (true);
	} /*langinstallresources*/


#ifndef FRONTIER_HEADLESS
/* In headless mode, langinitverbs() is provided by headless_lang_verbs.c */
boolean langinitverbs (void) {

    if (!langinstallresources ())
        return (false);

    return (langinitbuiltins ());
    } /*langinitverbs*/
#endif

boolean initlang (void) {

    shellpushmemoryhook (&hashflushcache);
    
    /* Assign typed no-op callbacks to avoid UB from mismatched function pointers */
    langcallbacks.symbolchangedcallback      = &cb_noop_symbolchanged;
    langcallbacks.symbolunlinkingcallback   = &cb_noop_tablenode;
    langcallbacks.symboldeletedcallback     = &cb_noop_address;
    langcallbacks.symbolinsertedcallback    = &cb_noop_symbolinserted;
    langcallbacks.comparenodescallback      = &cb_noop_compare;
    langcallbacks.debuggercallback          = &cb_noop_treenode;
    langcallbacks.debugerrormessagecallback = &cb_noop_errmsg;
    langcallbacks.scriptkilledcallback      = &cb_false_bool;
	
	newclearhandle (longsizeof (tyerrorstack), (Handle *) &langcallbacks.scripterrorstack);
	
	/* 4.1b4 dmb: no longer push this be default. langrun now does it.
	langpusherrorcallback (nil, 0L);
	*/
	
	/*
	langcallbacks.scripterrorcallback = &truenoop;
	
	langcallbacks.scripterrorrefcon = (long) 0;
	*/
	
    langcallbacks.scriptcompilecallback     = &cb_noop_hashnode_treenode;
    langcallbacks.backgroundtaskcallback    = &cb_true_bool;
	
	langcallbacks.pushtablecallback = (langtablerefcallback) &langdefaultpushtable;
	
	langcallbacks.poptablecallback = (langtablecallback) &langdefaultpoptable;
	
    langcallbacks.pushsourcecodecallback    = &cb_noop_sourcecode;
    langcallbacks.popsourcecodecallback     = &cb_noop_void;
    langcallbacks.saveglobalscallback       = &cb_noop_void;
    langcallbacks.restoreglobalscallback    = &cb_noop_void;
    langcallbacks.errormessagecallback      = &cb_noop_errmsg;
	
	langcallbacks.errormessagerefcon = nil;
	
    langcallbacks.clearerrorcallback        = &cb_noop_void;
    langcallbacks.msgverbcallback           = &cb_noop_verb;
    langcallbacks.codereplacedcallback      = &cb_noop_treenodes;
	
    langcallbacks.idvaluecallback           = nil;
    langcallbacks.partialeventloopcallback  = &cb_false_short;
    langcallbacks.processeventcallback      = &cb_false_event; /*4.1b13 dmb - new*/
	
	if (!newclearhandle (longsizeof (tytablestack), (Handle *) &hashtablestack))
		return (false);
	
	return (true);
	} /*initlang*/


#ifdef FRONTIER_HEADLESS
/**
 * langinitresources_headless - Public wrapper to initialize language resources in headless mode
 *
 * Headless CLI needs to initialize keywords, built-in functions, and constants before
 * registering language verbs. This public function exposes the initialization that's
 * normally done inside langinitverbs() in GUI mode.
 */
boolean langinitresources_headless(void) {
	if (!langinitconsttable()) {
		log_error(LOG_COMP_STARTUP, "langinitresources_headless: langinitconsttable failed");
		return false;
	}

	if (!langinitbuiltintable()) {
		log_error(LOG_COMP_STARTUP, "langinitresources_headless: langinitbuiltintable failed");
		return false;
	}

	if (!langinitkeywordtable()) {
		log_error(LOG_COMP_STARTUP, "langinitresources_headless: langinitkeywordtable failed");
		return false;
	}

	log_debug(LOG_COMP_STARTUP, "langinitresources_headless: initialization complete");
	return true;
}
#endif
