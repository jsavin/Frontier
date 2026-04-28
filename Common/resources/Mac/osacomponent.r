
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

#include "frontier.r"


#if 0

type 'ASpr' {
	
	integer = $$Countof(longprepositions);
	
	wide array longprepositions {
		
		string					terminology;
		unsigned hex longint	preposition;
		};
	};

resource 'ASpr' (128) {
	
		{
		"Through",	'thgh',
		"Between",	'btwn',
		"Against",		'agst',
		"OutOf",		'outo',
		"InsteadOf",	'isto',
		"AsideFrom",	'asdf',
		"Around",		'arnd',
		"Beside",		'bsid',
		"Beneath",	'bnth',
		"Under",		'undr',
		"Above",		'abve',
		"Below",		'belw',
		"ApartFrom",	'aprt',
		"About",		'abou',
		"Since",		'snce',
		"Until",		'till'
		}
	};

resource 'thng' (128, sysHeap) {
	'osa ', 'LAND', 'LAND',													/* ComponentDescription */
	kOSASupportsCompiling + kOSASupportsConvenience + kOSASupportsEventHandling, 0x00,	/*flags & mask*/
	'STUB', 128,															/* resource where Component code is found */
	'STR ', 128,															/* name string resource */
	'STR ', 129,															/* info string resource */
	'ICON', 129															/* icon resource */
};


data 'STUB' (128, sysHeap) {
	$"4EF9 0000 0000"	/*jmp xxxx*/
};

#endif

resource 'ICON' (129, purgeable) {
	$"00E0 01C0 0110 0220 0113 0410 0114 8410"
	$"0714 8438 0918 8244 F100 8183 8101 0601"
	$"991E C801 A510 3001 A510 0D01 A510 0001"
	$"A310 0001 E011 FC01 A01E 03C1 9010 0031"
	$"8F10 CF0F 8111 4481 8112 8241 8112 8F41"
	$"8112 9041 8112 6041 8112 0C21 8111 1E11"
	$"8112 8609 8111 7005 82E2 AE15 8541 5585"
	$"8A80 0AF9 9500 0151 AA00 0001 FFFF FFFF"
};


resource 'STR#' (512, "component", purgeable) {
	{	/* array StringArray: 5 elements */
		/* [1] */
		"Frontier",							/* 2005-01-04 creedon - removed UserLand and (tm) for open source release */
		/* [2] */
		"UserTalk",
		/* [2] */
		"Frontier UserTalk scripting component",	/* 2005-01-04 creedon - removed UserLand and (tm) for open source release */
		/* [4] */
		"Frontier Menu Sharing component",		/* 2005-01-04 creedon - removed UserLand and (tm) for open source release */
		/* [5] */
		"Frontier Window Sharing component"	/* 2005-01-04 creedon - removed UserLand and (tm) for open source release */
	}
};


resource 'STR#' (513, "recording", purgeable) {
	{	/* array StringArray: 10 elements */
		/* [1] */
		"null",
		/* [2] */
		"after",
		/* [3] */
		"before",
		/* [4] */
		"beginningOf",
		/* [5] */
		"endOf",
		/* [6] */
		"replace",
		/* [7] */
		"insertionLoc",
		/* [8] */
		"with objectModel",
		/* [9] */
		"\tbringToFront ();",
		/* [10] */
		"\tsys.bringAppToFront ('^0');",
		/* [11] */
		"id",
		/* [12] */
		"\tappleEvent (^0, '^1', '^2'",
		/* [13] */
		"Çno verb table found for Ò^0Ó",
		/* [14] */
		"There are ^0 clients connected to FrontierÕs UserTalk component.  Quit anyway?",
		/* [15] */
		"The application Ò^0Ó is connected to FrontierÕs UserTalk component.  Quit anyway?"
	}
};

