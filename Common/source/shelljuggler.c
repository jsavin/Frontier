
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

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "cursor.h"
#include "launch.h"
#include "scrap.h"
#include "shell.h"
#include "shellprivate.h"
#include "langinternal.h"
#include "scripts.h"
#include "process.h"



static boolean flshellactive = true; /*under multifinder, are we in the foreground?*/




boolean shellisactive (void) {
	
	return (flshellactive);
	} /*shellisactive*/


boolean shellactivate (void) {
	
	/*
	2/10/92 dmb: added entrypoint for shellsysverbs and dialog operations
	
	1/22/93 dmb: use langpartialeventloop when a script is running
	
	2.1b12 dmb: don't return quickly if shellactive is true. we may be 
	trying to bring a client process to the front. besided, the overhead 
	of activating an already-active application is small, and this isn't 
	a performance bottleneck anyway.
	*/
	
	/*
	if (flshellactive)
		return (true);
	*/
	
	if (!activateapplication (nil)) /*bring ourselves to the front*/
		return (false);
	
	
	if (fldisableyield)
		return (true);
	

	while (!flshellactive) { /*wait 'till we're actually in front*/
		
		if (flscriptrunning)
			langpartialeventloop (osMask | updateMask | activMask);
		else
			shellpartialeventloop (osMask | updateMask | activMask);
		}
	
	return (true);
} /*shellactivate*/


static boolean shelljugglervisit (WindowPtr w, ptrvoid refcon) {
#pragma unused (refcon)

	shellpushglobals (w);
	
	(*shellglobals.resumeroutine) (flshellactive);
	
	shellpopglobals ();
	
	return (true);
	} /*shelljugglervisit*/
	
	
void shellhandlejugglerevent (void) {
	
	/*
	7/13/90 DW: deactivate the front window when we're swapped into the
	background on a juggler event.  activate it when we're swapped back 
	into the foreground.

	2006-04-17 aradke: updated for Intel Macs using endianness-agnostic code
	*/
	
	register boolean flresume;
	long message = shellevent.message;
	
	if (((message >> 24) & 0xff) == suspendResumeMessage) { /*suspend or resume subevent*/
		
		boolean fl;
		
		flresume = ((message & resumeFlag) != 0); /*copy into register*/
		
		flshellactive = flresume; /*set global*/
		
		if (flresume) { /*force update of mouse cursor*/
		
			setcursortype (cursorisdirty); /*we don't know what state it's in*/
			
			shellforcecursoradjust (); /*make it appear as if mouse moved*/
			}
		
		fl = shellpushfrontglobals ();
		
		if (fl)
			shellactivatewindow (shellwindow, flshellactive);

		
		if (fl)
			shellpopglobals ();
		
		/* 4.0b7 dmb: new feature, just like startup & shutdown scripts */
		if (flresume)
			scriptrunresumescripts ();
		else
			scriptrunsuspendscripts ();
		
		shellvisittypedwindows (-1, &shelljugglervisit, nil); /*send message to all open windows*/
		}
	} /*shellhandlejugglerevent*/
	
	
