
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

#define menuinclude /*so that other modules can tell that we've been included*/

#ifndef shelltypesinclude
	
	#include "shelltypes.h"

#endif


#define chcommand commandMark

#define insertsubmenu -1

#define insertatend -2


/*prototypes*/

extern void drawmenubar (void);

extern void disposemenu (hdlmenu);

extern hdlmenu getresourcemenu (short);

extern boolean getcommandkeystring (byte, tykeyflags, bigstring);

//Code change by Timothy Paustian Saturday, April 29, 2000 9:30:20 PM
//Changed for UH 3.3.1, newmenu conflicts with headers def in Menus.h
extern hdlmenu Newmenu (short id, bigstring bstitle);

extern hdlmenu getmenuhandle (short);

extern boolean insertmenu (hdlmenu, long);

extern boolean inserthierarchicmenu (hdlmenu, short);

extern void removemenu (short);

extern long trackmenu (Point);

extern boolean sethierarchicalmenuitem (hdlmenu hmenu, short itemnumber, hdlmenu hsubmenu, short idsubmenu);

extern boolean gethierarchicalmenuitem (hdlmenu hmenu, short ixmenu, hdlmenu *hsubmenu);

extern void setmenutitleenable (hdlmenu, short, boolean);

extern void setmenuitemenable (hdlmenu, short, boolean);

extern boolean getmenutitleenable (hdlmenu, short);

extern boolean getmenuitemenable (hdlmenu, short);

extern void disablemenuitem (hdlmenu, short);

extern void enablemenuitem (hdlmenu, short);

extern short countmenuitems (hdlmenu);

extern void enableallmenuitems (hdlmenu, boolean);

extern void hilitemenu (short);

extern void checkmenuitem (hdlmenu, short, boolean);

extern boolean menuitemmarked (hdlmenu, short);

extern void markmenuitem (hdlmenu, short, short);

extern void stylemenuitem (hdlmenu, short, short);

extern boolean setmenutitle (hdlmenu, bigstring);

extern boolean setmenuitem (hdlmenu, short, bigstring);

extern boolean getmenuitem (hdlmenu, short, bigstring);

extern boolean setmenuitemcommandkey (hdlmenu, short, short);

extern void getmenuitemcommandkey (hdlmenu, short, short *);
//Code change by Timothy Paustian Saturday, April 29, 2000 9:31:06 PM
//Changed for UH 3.3.1 conflicts with insertmenuitem in Menus.h
extern boolean Insertmenuitem (hdlmenu, short, bigstring);

extern boolean deletemenuitem (hdlmenu, short);

extern boolean deleteallmenuitems (hdlmenu, short);

extern boolean deletelastmenuitem (hdlmenu);

extern boolean pushmenuitem (hdlmenu, short, bigstring, short);

extern boolean pushresourcemenuitems (hdlmenu, short, OSType);

extern boolean pushdottedlinemenuitem (hdlmenu);

extern boolean newtempmenu (hdlmenu *, short *);

extern short getprevmenuitem (hdlmenu);

extern short getnextmenuitem (hdlmenu);

extern boolean initmenusystem (void);

extern boolean deletemenuitems (hdlmenu, short, short); /* 2005-10-01 creedon */

extern void disableallmenuitems (hdlmenu hmenu); /* 2005-10-01 creedon */

