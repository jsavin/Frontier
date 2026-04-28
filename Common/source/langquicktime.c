
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-present Frontier contributors

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

/*
7.0b4 PBS 08/09/00: QuickTime verbs.
*/

#include "frontier.h"
#include "standard.h"


#include "memory.h"
#include "strings.h"
#include "ops.h"
#include "resources.h"
#include "timedate.h"
#include "lang.h"
#include "langinternal.h"
#include "langhtml.h"
#include "langexternal.h"
#include "langsystem7.h"
#include "tableinternal.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "kernelverbs.h"
#include "kernelverbdefs.h"
#include "oplist.h"
#include "player.h"


typedef enum tyquicktimeverbtoken { /*verbs that are processed by langquicktime.c*/
	
	quicktimeopenfunc,
	quicktimeplayfunc,
	quicktimestopfunc,
	quicktimeisplayingfunc,
	
	ctquicktimeverbs
	} tyquicktimeverbtoken;


/*Functions*/


static boolean quicktimeisplayingverb (hdltreenode hp1, tyvaluerecord *v) {
	
	/*
	QuickTime video playback is no longer supported in this version.
	*/
	
	if (!langcheckparamcount (hp1, 0))
		return (false);
			
	// QuickTime is no longer supported - always return false
	(*v).data.flvalue = false;
	
	return (true);
	} /*quicktimeisplayingverb*/


static boolean quicktimeopenverb (hdltreenode hp1, tyvaluerecord *v) {
#pragma unused(v)

	/*
	QuickTime video playback is no longer supported in this version.
	*/
	
	tyfilespec fs;

	flnextparamislast = true;
	
	if (!getfilespecvalue (hp1, 1, &fs))
		return (false);

	// QuickTime is no longer supported
	langerrormessage (BIGSTRING ("\x3A" "Can't open the file because QuickTime is no longer supported."));
	
	return (false);
	} /*quicktimeopenverb*/


static boolean quicktimeplayverb (hdltreenode hp1, tyvaluerecord *v) {
	
	/*
	QuickTime video playback is no longer supported in this version.
	*/
	
	if (!langcheckparamcount (hp1, 0))
		return (false);
			
	// QuickTime is no longer supported
	langerrormessage (BIGSTRING ("\x3A" "Can't play the movie because QuickTime is no longer supported."));
	
	return (false);
	} /*quicktimeplayverb*/
	

static boolean quicktimestopverb (hdltreenode hp1, tyvaluerecord *v) {
	
	/*
	QuickTime video playback is no longer supported in this version.
	*/
	
	if (!langcheckparamcount (hp1, 0))
		return (false);
			
	// QuickTime is no longer supported
	langerrormessage (BIGSTRING ("\x3A" "Can't stop the movie because QuickTime is no longer supported."));
	
	return (false);
	} /*quicktimestopverb*/


static boolean quicktimefunctionvalue (short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror) {
	
	/*
	7.0b4 PBS: switch statement for QuickTime verbs.
	*/
	
	hdltreenode hp1 = hparam1;
	tyvaluerecord *v = vreturned;
	
	setbooleanvalue (false, v); /*by default, string functions return false*/
	
	switch (token) {
		
		case quicktimeopenfunc:
			return (quicktimeopenverb (hp1, v));
		
		case quicktimeplayfunc:
			return (quicktimeplayverb (hp1, v));
		
		case quicktimestopfunc:
			return (quicktimestopverb (hp1, v));
		
		case quicktimeisplayingfunc:
			return (quicktimeisplayingverb (hp1, v));
		
		default:
			getstringlist (langerrorlist, unimplementedverberror, bserror);
			
			return (false);
		} /*switch*/
	} /*quicktimefunctionvalue*/
	
	
boolean quicktimeinitverbs (void) {
	
	/*
	7.0b4 PBS: new QuickTime verbs.
	*/
		
	return (loadfunctionprocessor (idquicktimeverbs, &quicktimefunctionvalue));
	} /*quicktimeinitverbs*/


