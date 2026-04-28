
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
#include "quickdraw.h"
#include "font.h"
#include "resources.h"
#include "strings.h"
#include "dialogs.h"
#include "shell.rsrc.h"
#include "frontierconfig.h"
#include "shell.h"
#include "byteorder.h"	/* 2006-04-16 aradke: swap byte order in loadconfigresource */


#define configresourcetype 'cnfg'


tyconfigrecord config;

short iddefaultconfig = idtableconfig;




static void initconfigrecord (tyconfigrecord *configrecord) {
	
	register ptrconfigrecord p = configrecord;
	
	clearbytes (p, longsizeof (tyconfigrecord)); /*initalize all fields to 0*/
	
	(*p).flhorizscroll = true;
	
	(*p).flvertscroll = true;
	
	(*p).flmessagearea = true;
	
	(*p).flgrowable = true;
	
	(*p).messageareafraction = 3;
	
	(*p).filecreator = typeunknown;
	
	(*p).filetype = typeunknown;

	(*p).defaultfont = systemFont;

	(*p).defaultsize = 12;
	} /*initconfigrecord*/


void loadconfigresource (short configresnum, tyconfigrecord *cr) {
	
	/*
	2.1b5 dmb: release the config resource when done with it
	
	2006-04-16 aradke: swap byte-order on Intel Macs
	*/
	
	register Handle h;
	bigstring bs;
	
	h = getresourcehandle (configresourcetype, configresnum);

	if (h != nil) {
	
		moveleft (*h, cr, sizeof (tyconfigrecord)); 
	
		releaseresourcehandle (h);
		
		reztomemshort ((*cr).flhorizscroll);
	
		reztomemshort ((*cr).flvertscroll);
	
		reztomemshort ((*cr).flwindowfloats);
	
		reztomemshort ((*cr).flmessagearea);
	
		reztomemshort ((*cr).flinsetcontentrect);
	
		reztomemshort ((*cr).flnewonlaunch);
	
		reztomemshort ((*cr).flopenresfile);
	
		reztomemshort ((*cr).fldialog);
	
		reztomemshort ((*cr).flgrowable);
	
		reztomemshort ((*cr).flcreateonnew);
	
		reztomemshort ((*cr).flwindoidscrollbars);
	
		reztomemshort ((*cr).flstoredindatabase);
	
		reztomemshort ((*cr).flparentwindowhandlessave);
	
		reztomemshort ((*cr).fleraseonresize);
	
		reztomemshort ((*cr).fldontconsumefrontclicks);
	
		reztomemshort ((*cr).flcolorwindow);

		reztomemshort ((*cr).messageareafraction);

		reztomemlong ((*cr).filecreator);	/* OSType */
		
		reztomemlong ((*cr).filetype);	/* OSType */
		
		reztomemrect ((*cr).rmin);
		
		reztomemshort ((*cr).templateresnum);
		
		reztomemshort ((*cr).defaultfont);
		
		reztomemshort ((*cr).defaultsize);
		
		reztomemshort ((*cr).defaultstyle);
		
		reztomemshort ((*cr).idbuttonstringlist);
		
		reztomemrect ((*cr).defaultwindowrect);
		
		getstringlist (fontnamelistnumber, (*cr).defaultfont, bs);
		
		fontgetnumber (bs, &(*cr).defaultfont);
		
		centerrectondesktop (&(*cr).defaultwindowrect);
		}
	else
		initconfigrecord (cr);
	} /*loadconfigresource*/
	
	
/*
boolean saveconfigresource (short configresnum, tyconfigrecord *cr) {

	Handle h;
	
	h = GetResource (configresourcetype, configresnum);
	
	if (h != nil) { /%resource already exists%/
	
		moveleft ((ptrchar) cr, (ptrchar) *h, longsizeof (tyconfigrecord)); 
		
		ChangedResource (h);
		}
	else {
		if (!newfilledhandle ((ptrchar) cr, longsizeof (tyconfigrecord), &h)) 
			return (false);
		
		AddResource (h, configresourcetype, configresnum, emptystring);
		}
		
	return (ResError () == noErr);
	} /%saveconfigresource*/
	
	
boolean getprogramname (bigstring bs) {
	
	return (getstringlist (defaultlistnumber, programname, bs));
	} /*getprogramname*/
	
	
boolean getuntitledfilename (bigstring bs) {
	
	//
	// 2006-09-15 creedon; push space character instead of dash
	//
	// 5.0d6 dmb: added numeric sequencing
	//
	
	static long untitledsequencer = 0;
	
	if (!getstringlist (defaultlistnumber, untitledfilename, bs))
		return (false);
	
	if (++untitledsequencer > 1) {
	
		pushchar ( ' ', bs );
		
		pushlong (untitledsequencer, bs);
		}
	
	return (true);
	
	} // getuntitledfilename
	
	
boolean getdefaultfilename (bigstring bs) {
	
	return (getstringlist (defaultlistnumber, defaultfilename, bs));
	} /*getdefaultfilename*/
	
/*
boolean getusername (bigstring bs) {
	
	return (getstringlist (defaultlistnumber, username, bs));
	} /%getusername*/
	
	
void initconfig (void) {
	
	initconfigrecord (&config);
	} /*initconfig*/
	
	
	