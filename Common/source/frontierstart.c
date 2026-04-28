
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

#include "frontier.h"
#include "standard.h"

	#include "OSXSpecifics.h"
	#include "player.h" /*7.0b4: QuickTime Player*/

#include "about.h"
#include "frontierconfig.h"
#include "db.h" /*stats*/
#include "file.h" /*6.1b15 AR: filestart*/
#include "shell.h"
#include "lang.h"
#include "langinternal.h"
#include "langexternal.h"
#include "tableverbs.h"
#include "opverbs.h"
#include "scripts.h"
#include "menuverbs.h"
#include "pictverbs.h"
#include "wpverbs.h"
#include "cancoon.h"
#include "command.h"
	#include "osacomponent.h"
#include "frontierstart.h"


boolean frontierstart (void) {
	
	/*
	do all initialization and starting up. on failure, exit to shell; only
	return to caller on success
	
	3.0.4b6 dmb: call osacomponentstart from here, after the shell
	is more thoroughly initialized.
	*/
	
	iddefaultconfig = idcancoonconfig;
//	iddefaultconfig = idscriptconfig;
	
	if (!filestart ()) /*6.1b16*/
		return (false);

	if (!opstart ())
		return (false);
	
	if (!menustart ())
		return (false);
	
	if (!tablestart ())
		return (false);
	
	if (!scriptstart ())
		return (false);
		
	if (!pictstart ())
		return (false);
	
	if (!wpstart ())
		return (false);
	
	
	if (!aboutstart ())
		return (false);
		

/*
		useQDText(0); // set to 1 for Quartz rendering.
*/
	
		if (!playerstart ()) /*7.0b4 PBS: QuickTime Player*/
			return (false);
	
	
	
	if (!cmdstart ())
		return (false);
	
	if (!langdialogstart ())
		return (false);
	
	if (!langerrorstart ())
		return (false);
	
	if (!statsstart ())
		return (false); 
	
	
	if (!ccstart ())
		return (false);
	
	
	//#if !TARGET_API_MAC_CARBON
	//Code change by Timothy Paustian Saturday, July 8, 2000 9:47:28 PM
	//The carbon version just does not play nice with the component manager.
	//at some point I have to figure this out.
	if (!osacomponentstart ())
		;	// don't quit if this doesn't work
	//#endif

	
	if (!shellstart ())
		return (false);
	
	return (true);
	} /*frontierstart*/



