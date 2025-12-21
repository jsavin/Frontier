
/*	$Id$    */

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

#include "frontier.h"
#include "standard.h"

#include "cursor.h"
#include "dialogs.h"
#include "error.h"
#include "file.h"
#include "fileloop.h"
#include "kb.h"
#include "memory.h"
#include "ops.h"
#include "quickdraw.h"
#include "process.h"
#include "scrap.h"
#include "strings.h"
#include "opinternal.h"
#include "oplineheight.h"
#include "claybrowser.h"
#include "claycallbacks.h"
#include "claybrowserexpand.h"
#include "claybrowservalidate.h"
#include "claybrowserstruc.h"
#if odbbrowser
	#include "shell.rsrc.h"
	#include "shellundo.h"
	#include "tableinternal.h"
	#include "tableverbs.h"
	#include "wpengine.h"
#endif




boolean browsergetrefcon (hdlheadrecord hnode, tybrowserinfo *info) {
	
	return (opgetrefcon (hnode, info, sizeof (tybrowserinfo)));
	} /*browsergetrefcon*/


boolean browsersetrefcon (hdlheadrecord hnode, tybrowserinfo *info) {
	
	return (opsetrefcon (hnode, info, sizeof (tybrowserinfo)));
	} /*browsersetrefcon*/


boolean browsercopyfileinfo (hdlheadrecord hnode, tybrowserinfo *fileinfo) {
	
	/*
	set the fields of a browser refcon handle that come from 
	a file's fileinfo record. ignore the other fields.
	*/
	
//	tybrowserinfo browserinfo;
	
//	browsergetrefcon (hnode, &browserinfo);
	
	if (!browsersetrefcon (hnode, fileinfo))
		return (false);
	
	return (true);
	} /*browsercopyfileinfo*/


static void filepushsuffixnumber (short suffixnum, bigstring name) {
	
	if (suffixnum > 0) {
	
		pushstring (BIGSTRING ("\x02" " #"), name);
	
		pushint (suffixnum, name);
		}
	} /*filepushsuffixnumber*/


static void filepopsuffixnumber (short suffixnum, bigstring name) {
	
	byte bssuffix [32];
	short ixsuffix;
	
	if (suffixnum > 0) {
	
		copystring (BIGSTRING ("\x02" " #"), bssuffix);
		
		pushint (suffixnum, bssuffix);
		
		ixsuffix = patternmatch (bssuffix, name) - 1;

		if (ixsuffix == stringlength (name) - stringlength (bssuffix))
			setstringlength (name, ixsuffix);
		}
	} /*filepushsuffixnumber*/
		

boolean claygetfilespec (hdlheadrecord hnode, tybrowserspec *fs) { 
	
	/*
	turn a headrecord into a filespec.
	
	6/27/93 DW: per recommendation of Think Reference, if it's a
	volume, just set the vRefNum field of the filespec.
	
	8/30/93 DW: rewrite.
	
	9/21/93 dmb: don't need special case for volumes anymore. Think Ref's 
	recommendation is fine for specifying volumes, but it's event better 
	to specify the root directory of the volume, removing any special cases.
	
	5.1.4 dmb: on failure, clear fs
	*/
	
	tybrowserinfo info;
	bigstring name;
	
	if (!browsergetrefcon (hnode, &info)) {
		
		clearbytes (fs, sizeof (tybrowserspec));
		
		return (false);
		}
	
	/*
	if (info.flvolume) { /%special case%/
		
		(*fs).vRefNum = info.vnum;
		
		(*fs).parID = 0; 
		
		setstringlength ((*fs).name, 0);
		
		return (true);
		}
	*/
	
	opgetheadstring (hnode, name);
	
	// dmb 5.0b9 - was: filepushsuffixnumber (info.suffixnum, name);
	
	return (claymakespec (info.vnum, info.dirid, name, fs));
	} /*claygetfilespec*/


boolean browserloadnode (hdlheadrecord hnode) {

	tybrowserspec fs;
	tybrowserinfo fileinfo;
	
	(**hnode).fldirty = true; /*force update to reflect new info*/
	
	claygetfilespec (hnode, &fs);
	
	if (!claygetfileinfo (&fs, &fileinfo))
		return (false);
	
	(**hnode).flnodeisfolder = fileinfo.flfolder;
	
	return (browsercopyfileinfo (hnode, &fileinfo));
	} /*browserloadnode*/
	
	
boolean browserchecklinelength (short newlen, bigstring bs) {

	bigstring bsalert;
		
	if (newlen <= (short) (**outlinedata).maxlinelen) 
		return (true);
	
	copystring (BIGSTRING ("\x0e" "The file name "), bsalert);
	
	if (stringlength (bs) > 0) {
		
