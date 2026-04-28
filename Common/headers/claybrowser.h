
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

#ifndef claybrowserinclude
#define claybrowserinclude


#define odbbrowser true
#define filebrowser false
             

/*these macros map our names onto the app0 - app4 bits in each hdlheadrecord*/
	
	#define flnodeisfolder appbit2
	#define flnodeunderlined appbit3
	#define flnodeonscrap appbit4
	#define tmpbit2 appbit5
	#define flnodeneedsbuild appbit6
	#define flnewlyinsertednode appbit7

#if odbbrowser

	#include "db.h"
	#include "lang.h"
	#include "opinternal.h"
	#include "tableformats.h"
	
	typedef hdldatabaserecord tybrowservol;
	
	typedef hdlhashtable tybrowserdir;
#pragma pack(2)
	typedef struct tybrowserspec {
		
		tybrowservol vRefNum;
		
		tybrowserdir parID;
		
		bigstring name;
		} tybrowserspec, *ptrbrowserspec;
#pragma options align=reset

#endif

#if filebrowser

	#include "file.h"

	typedef short tybrowservol;
	
	typedef long tybrowserdir;
	
	typedef FSSpec tybrowserspec, *ptrbrowserspec;

#endif

#pragma pack(2)
typedef struct tybrowserinfo { /*one of these is linked into each browser window headrecord*/

	boolean flfolder; /*if true, it's a folder*/
	
	boolean flvolume; /*if true, it's a volume*/
	
	tybrowservol vnum; /*the volume on which it's located*/
	
	tybrowserdir dirid; /*the directory id of the folder that contains it*/

	short suffixnum; /*for duplicate files*/
	
	unsigned long filesize; /*for files, the size of the file, for folders, the number of files it contains*/
	
	long timecreated, timemodified; /*the creation/modification date of the file/folder/volume*/
	
	#if filebrowser
	
	boolean fllocked: 1; /*if true, the file is locked*/
	
	boolean flstationery: 1; /*if true, it's a stationery pad*/
	
	boolean flalias: 1; /*if true, it's an alias file*/
	
	boolean flejectable: 1; /*if true, it's a volume and it's ejectable*/
	
	boolean flhardwarelock: 1; /*for volumes, if true, the device is readonly*/
	
	boolean flremotevolume: 1; /*for volumes, if true, it's a remote volume, accessed over the network*/
	
	boolean flnamelocked: 1; /*true if the name of this file can't be changed*/
	
	boolean flcustomicon: 1; /*true if the file has a custom icon in its resource fork*/
	
	OSType filetype, filecreator; /*identifies the application that created the file*/
	
	short ixlabel; /*the label of the file, as in the Finder, determines color of icon display*/
	
	tyfolderview folderview; /*for folders, view-by-name, date, etc.*/
	
	#endif
	} tybrowserinfo;
#pragma options align=reset

typedef boolean (*tyclayfileloopcallback) (bigstring, tybrowserinfo *, long);


boolean browserinitrecord (hdltableformats);

boolean clayinitializeoutline (void);

boolean claymakespec (tybrowservol vnum, tybrowserdir dirid, bigstring fname, tybrowserspec *fs);

boolean claygetfilespec (hdlheadrecord, tybrowserspec *);

boolean claygetfileinfo (const tybrowserspec *fs, tybrowserinfo *);

boolean clayfolderloop (const tybrowserspec *, boolean, tyclayfileloopcallback, long);

boolean clayrenamefile (tybrowserspec *fs, hdlheadrecord headnode); /*6.2b16 AR: headnode instead of bsnew*/

boolean claygetfilename (const tybrowserspec *pfs, bigstring name);

boolean browserexpand (hdlheadrecord, long);

boolean browserselectfile (ptrfilespec, boolean, hdlheadrecord *);

boolean browserexpandtofile (ptrfilespec);

boolean browserfollowalias (hdlheadrecord);

boolean browsernodeexists (hdlheadrecord, const tybrowserspec *);

boolean browsernewfolder (void);
 
boolean browsernewtextfile (void);

boolean browsernewscript (void);

boolean browsernewoutline (void);

boolean browsernewcopy (tybrowserspec *, bigstring);

boolean browsergetrefcon (hdlheadrecord, tybrowserinfo *);
	
boolean browsersetrefcon (hdlheadrecord, tybrowserinfo *);
	
boolean browsercopyfileinfo (hdlheadrecord, tybrowserinfo *);

boolean browseropenmainwindow (void);

//	boolean browserjumptospecialfolder (tyfoldername);

boolean browseradjustmenus (void);

// void browserupdatemsg (const tybrowserspec *, tyfileinfo *);

boolean browserfileadded (hdlheadrecord, const tybrowserspec *, hdlheadrecord *);

boolean browserloadnode (hdlheadrecord);

boolean browserchecklinelength (short, bigstring);

void browserdrawnodeicon (const Rect *, boolean, hdlheadrecord);

boolean browsersymbolchanged (hdlhashtable, const bigstring, boolean);

boolean browsersymbolinserted (hdlhashtable, const bigstring);

boolean browsersymboldeleted (hdlhashtable, const bigstring);

void browserupdate (void);

boolean browserstart (void);

#endif

