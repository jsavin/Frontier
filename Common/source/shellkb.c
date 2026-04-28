
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

#include "menu.h"
#include "ops.h"
#include "kb.h"
#include "sounds.h"
#include "shell.h"
#include "shellhooks.h"
#include "shellmenu.h"



unsigned long timelastkeystroke = 0; /*the value of tickcount for last keystroke*/

static boolean flouchlocked = false;	/*4.0b7 dmb*/



static boolean shellfilterfontkey (long *codeword) {
	
	/*
	2005-12-28 lawton, creedon : if option key is down bail out, allows command-option-u to find its item in its menubar
	
	filter cmd-shift-B,I,U,O,S to a style-menu item.
	*/
	
	register short ixitem;
	register short ixmenu;
	
	if ((!keyboardstatus.flcmdkey) || (!keyboardstatus.flshiftkey) || keyboardstatus.floptionkey) 
		return (false);
	
	if (shellmenuhandle (fontmenu) == nil) /*font menu isn't present*/
		return (false);
	
	switch (uppercasechar (keyboardstatus.chkb)) {
		
		case 'P':
			ixmenu = stylemenu;
			
			ixitem = plainitem;
			
			break;
		
		case 'B':
			ixmenu = stylemenu;
			
			ixitem = bolditem;
			
			break;
				
		case 'I':
			ixmenu = stylemenu;
			
			ixitem = italicitem;
			
			break;
				
		case 'U':
			ixmenu = stylemenu;
			
			ixitem = underlineitem;
			
			break;
				
		case 'O':
			ixmenu = stylemenu;
			
			ixitem = outlineitem;
			
			break;
				
		case 'S':
			ixmenu = stylemenu;
			
			ixitem = shadowitem;
			
			break;
			
		case ',':
			ixmenu = sizemenu;
			
			ixitem = pointdownitem;
			
			break;
			
		case '.':
			ixmenu = sizemenu;
			
			ixitem = pointupitem;
			
			break;
			
		case '[':
			ixmenu = fontmenu;
			
			ixitem = getprevmenuitem (shellmenuhandle (fontmenu));
			
			break;
				
		case ']':
			ixmenu = fontmenu;
			
			ixitem = getnextmenuitem (shellmenuhandle (fontmenu));
			
			break;
			
		default:
			return (false);
		} /*switch*/
	
	*codeword = ((long) ixmenu << 16) + ixitem;
	
	return (true);
	} /*shellfilterfontkey*/


enum { /*key codes*/
	
	clearkey = 0x47,
	
	helpkey = 0x72,
	
	homekey = 0x73,
	
	endkey = 0x77,
	
	pgupkey = 0x74,
	
	pgdnkey = 0x79
	};


static boolean shellfilterfunctionkey (long *codeword) {
	
	register short item;
	register short menu = editmenu;
	
	switch (keyboardstatus.keycode) {
		
		case 'z':
			item = undoitem;
			
			break;
		
		case 'x':
			item = cutitem;
			
			break;
		
		case 'c':
			item = copyitem;
			
			break;
		
		case 'v':
			item = pasteitem;
			
			break;
		
		case clearkey:
			item = clearitem;
			
			break;
		
		case helpkey:
			item = helpitem;
			
			menu = virtualmenu;
			
			break;
			
		case homekey:
			item = homeitem;
			
			menu = virtualmenu;
			
			break;
			
		case endkey:
			item = enditem;
			
			menu = virtualmenu;
			
			break;
			
		case pgupkey:
			item = pageupitem;
			
			menu = virtualmenu;
			
			break;
			
		case pgdnkey:
			item = pagedownitem;
			
			menu = virtualmenu;
			
			break;
		
		default:
			return (false);
		} /*switch*/
	
	*codeword = ((long) menu << 16) + item;
	
	return (true);
	} /*shellfilterfunctionkey*/


void shellhandlekeystroke (void) {
	
	/*
	if it's a cmd-key we first offer it to the frontmost window, if he doesn't
	consume it, we pass it on to the menubar.
	
	if it's shift-enter, we close the frontmost window.
	
	otherwise we pass the keystroke to the front window's handler.
	
	7/18/90 DW: add hooks.  they're allowed to consume a keystroke by returning
	false.
	
	11/8/90 DW: add support for style keys -- cmd-shift BIOUS.
	
	12/6/91 dmb: give menubar priority over cmdkeyfilter
	*/
	
	unsigned int chkb;
		register boolean fl;
		long codeword;
	
	timelastkeystroke = gettickcount ();
	
	setkeyboardstatus (shellevent.message, shellevent.what, shellevent.modifiers); /*fill keyboardstatus record with info about the event*/
	
	if (!keyboardstatus.flautokey)	/*4.0b7 dmb: new non-repeating ouchs*/
		flouchlocked = false;

	if (shellfilterfunctionkey (&codeword) || shellfilterfontkey (&codeword)) {
		
		fl = shellpushfrontglobals ();
		
		shellupdatemenus ();
		
		HiliteMenu (HiWord (codeword));
		
		shellhandlemenu (codeword);
		
		if (fl)
			shellpopglobals ();
			
		return;
		}
	
	if (!shellcallkeyboardhooks ())
		return;
	
	chkb = keyboardstatus.chkb; /*move into register*/

	if (keyboardstatus.flcmdkey && (chkb >= chspace)) {
		
		if (chkb == '.') { /*cmd-period is consumed here*/
		
			keyboardsetescape ();
			
			return;
			}
		
		/*
		if (shellpushfrontglobals ()) { /%a window is open%/
			
			fl = (*shellglobals.cmdkeyfilterroutine) (keyboardstatus.chkb);
			
			shellpopglobals ();
			
			if (!fl) /%cmd-key was consumed%/
				return;
			}
		*/
		
		fl = shellpushfrontglobals ();
		
		shellupdatemenus (); /*be sure the menus are properly checked and highlighted*/
		
		if (!shellhandlemenu (MenuKey (chkb))) { /*cmd-key not consumed by menubar*/
			
			if (fl)
				(*shellglobals.cmdkeyfilterroutine) (chkb);
			}
		
		if (fl)
			shellpopglobals ();
		
		return;
		}
		
	if (shellpushfrontglobals ()) { /*a window is open to close or receive keystroke*/
	
		if (shellcalleventhooks (&shellevent, shellwindow)) /*keystroke wasn't consumed*/
			(*shellglobals.keystrokeroutine) ();
		
		shellpopglobals ();
		}
	} /*shellhandlekeystroke*/


boolean shellfilterscrollkey (byte chkey) {

	switch (chkey) {

		case chhome:
			(*shellglobals.scrollroutine) (down, false, longinfinity);
			
			break;
		
		case chend:
			(*shellglobals.scrollroutine) (up, false, longinfinity);
			
			break;
		
		case chpageup:
			(*shellglobals.scrollroutine) (down, true, 1);
			
			break;
		
		case chpagedown:
			(*shellglobals.scrollroutine) (up, true, 1);
			
			break;

		default:
			return (false);
		}
	
	return (true);
	} /*shellfilterscrollkey*/


void shellouch (void) {
	
	/*
	ouch in response to a keystroke -- unless it was a repeat key
	*/
	
	if (!flouchlocked) {
		
		ouch ();
		
		flouchlocked = true;
		}	
	} /*shellouch*/


