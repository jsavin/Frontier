
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

#ifndef shelltypesinclude
#define shelltypesinclude

#ifdef FRONTIER_PORTABLE
#include "../portable/shelltypes_portable.h"
#else
#include "standard.h"

#ifdef HEADERTRACE
#pragma message( "**Compiling " __FILE__ )
#endif

#define diskfontnamelength 32 /*number of bytes for a font name stored on disk*/

typedef char diskfontstring [diskfontnamelength + 1];

typedef struct diskrect {
	
	short top;
	
	short left;
	
	short bottom;
	
	short right;
	} diskrect;

typedef struct diskrgb {
	
	short red;
	
	short green;
	
	short blue;
	} diskrgb;


typedef ControlHandle hdlscrollbar;


	typedef MenuHandle hdlmenu;


#pragma pack(2)
typedef struct tytextdisplayinfo { 

	short h; /*the horizontal pen position for all lines*/
	
	short v; /*the vertical pen position for the first line*/
	
	short lh; /*the uniform lineheight of all lines*/
	
	short screenlines; /*the number of lines that fit within the current window's rectangle*/
	
	Rect r; /*the rectangle within which everything is displayed*/
	
	short horizscrollpixels; /*number of pixels to scroll by for each horiz scroll*/
	} tytextdisplayinfo;
#pragma options align=reset


typedef short **hdlintarray;

#pragma pack(2)
typedef struct typopuprecord { /*this record isn't currently used*/
	
	Rect popuprect; /*where the whole popup structure is displayed*/
	} typopuprecord, *ptrpopuprecord, **hdlpopuprecord;
#pragma options align=reset


typedef OSType tyscraptype;


typedef ProcessSerialNumber typrocessid;


typedef enum clickflags { /*these match #defines in Paige.h*/

	clicknormal = 0,
	
	clickextend = 0x0001, /* extend the selection */
	
	clickwords = 0x0002, /* select whole words only */

	clickparas = 0x0004, /* select whole paragraphs only */

	clicklines = 0x0008, /* highlight whole lines */

	clickvertical = 0x0010, /* allow vertical selection */

	clickdiscontiguous = 0x0020, /* enable discontiguous selection */

	clickstyle = 0x0040, /* select whole style range */

	clickcontrol = 0x0200, /* word advance for arrows, Home-End to doc top and bottom */

	clickoption = 0x0400, /* option key held down */

	clickcommand = 	0x0800	/* alt key (Windows) or command key (Mac) */
	
	} tyclickflags;


typedef enum tykeyflags { /*these match tyclickflags, but apply to keystrokes*/

	keynormal = 0,
	
	keyshift = 0x0001,

	keycontrol = 0x0200,

	keyoption = 0x0400,

	keycommand = 0x0800
	
	} tykeyflags;


#pragma pack(2)
typedef struct tybuttonstatus {
	
	boolean fldisplay; /*should button be displayed at all?*/
	
	boolean flenabled; /*if displayed, is it active (or dimmed)?*/
	
	boolean flbold; /*if displayed, should text style be bold?*/
	} tybuttonstatus;
#pragma options align=reset



	/*
	2009-09-03 aradke: FSSpec was deprecated on Mac OS X and FSRef is its replacement.
		FSSpec could represent a non-existant file as long as its parent directory existed, but FSRef can not.
		Therefore, we have to extend FSRef with a string to hold the name of the non-existant file.
		The FSRef field usually points to the parent folder instead of to the file itself (even if the file exists).
		However, volumes have no parent folder so the FSRef field points to the volume itself in that case.
		
		We chose a fixed-size Unicode string for the name field to simplify memory management.
		If we used a CFStringRef we would have to keep track of copying and releasing in lots of places.

		Reference: Apple Technical Note TN2078 -- Migrating to FSRefs & long Unicode names from FSSpecs
		http://developer.apple.com/legacy/mac/library/technotes/tn2002/tn2078.html
	*/

#pragma pack(2)
	typedef struct tyfilespecflags {
	
		//boolean	flvalid;	// true: ref and name have been set -- false: empty filespec
		boolean flvolume;	// true: ref points to volume -- false: ref points to parent of file/folder
		
		} tyfilespecflags;
	
#ifndef PORTABLE_TYFSNAME_DEFINED
	typedef struct HFSUniStr255 tyfsname, *tyfsnameptr;
#define PORTABLE_TYFSNAME_DEFINED 1
#endif
	
typedef struct tyfilespec {

	tyfilespecflags	flags;
	FSRef ref;
	tyfsname name;
		
		} tyfilespec;

	typedef tyfilespec *ptrfilespec, **hdlfilespec;
#pragma options align=reset
		

#endif /* FRONTIER_PORTABLE */

#ifndef SHELLTYPES_DEFINED
#define SHELLTYPES_DEFINED 1
#endif

#endif
