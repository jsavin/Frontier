
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

#define kbinclude /*so other includes can tell if we've been loaded*/


#pragma pack(2)
typedef struct tykeystrokerecord {

	unsigned char chkb;
	
	boolean flshiftkey, flcmdkey, floptionkey, flalphalock, flcontrolkey;
		
	short ctmodifiers; /*the number of booleans that are on*/

	short keycode; /*see Toolbox Event Manager -- this is the hardware key code*/
	
	boolean flkeypad: 1; /*if true, keystroke comes from numeric keypad*/
	
	boolean flautokey: 1; /*if true, keystroke is an automatic key*/
	
	tydirection keydirection; 
	} tykeystrokerecord, *ptrkeystrokerecord, **hdlkeystrokerecord;
#pragma options align=reset

extern tykeystrokerecord keyboardstatus;


tydirection keystroketodirection (char ch); /*prototype*/


/*prototypes*/

extern boolean iscmdperiodevent (long eventmessage, long eventwhat, long eventmodifiers);

extern boolean arrowkey (char);

extern tydirection keystroketodirection (char);

extern void setkeyboardstatus (long, long, long);

extern void keyboardclearescape (void);

extern void keyboardsetescape (void);

extern boolean keyboardescape (void);

extern void keyboardpeek (tykeystrokerecord *);

extern boolean enterkeydown (void);

extern boolean optionkeydown (void);

extern boolean cmdkeydown (void);

extern boolean shiftkeydown (void);

extern boolean controlkeydown (void);

extern short getkeyboardstartrepeattime (void);


