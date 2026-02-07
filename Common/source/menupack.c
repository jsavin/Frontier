
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

/* 2025-11-24 Codex: BE menu script/outline addresses for v7 saves. */


#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "font.h"
#include "cursor.h"
#include "quickdraw.h"
#include "scrap.h"
#include "frontierwindows.h"
#include "op.h"
#include "opinternal.h"
#include "menueditor.h"
#include "menuinternal.h"
#include "db_format.h" /* 2025-11-23 Codex: BE helpers for menu metadata */
#include "byteorder.h"	/* 2006-04-08 aradke: endianness conversion macros */
#include "byteorder_helpers.h" /* 2026-02-02 Codex: Consolidated host/disk byte order helpers */
#include "logging.h"    /* For structured logging of v7→legacy truncation warnings */
#include <limits.h>     /* For SHRT_MAX */
#include <inttypes.h>   /* For PRIu32, PRIx64, PRId64 format macros */
#include "lang.h"       /* For langassign* functions and langpackvalue/langunpackvalue */
#include "langexternal.h" /* For langexternalnewvalue, idscriptprocessor */
#include "opverbs.h"    /* For opvaltoscript */
#include "tableinternal.h" /* For tablenewtablevalue */

	

boolean megetmenuiteminfo (hdlheadrecord hnode, tymenuiteminfo *item) {
	
	if (!opgetrefcon (hnode, item, sizeof (tymenuiteminfo)))
		return (false);
	
	disktomemlong ((*item).linkedscript.adrlink);
	
	return (true);
	} /*megetmenuiteminfo*/


boolean mesetmenuiteminfo (hdlheadrecord hnode, const tymenuiteminfo *item) {
	
	tymenuiteminfo info = *item;

	db_format_write_be32(&info.linkedscript.adrlink, (uint32_t) info.linkedscript.adrlink);
	
	return (opsetrefcon (hnode, &info, sizeof (info)));
	} /*mesetmenuiteminfo*/


boolean mecopyrefconroutine (hdlheadrecord hsource, hdlheadrecord hdest) {
	
	/*
	5.0a3 dmb: use meloadscriptoutline, not meloadoutline. the latter loads
	the main menubar outline, not script outlines. I don't know why it didn't 
	break in 4.x, but it breaks now.
	*/
	
	hdloutlinerecord ho;
	tymenuiteminfo item;
	hdloutlinerecord hcopy;
	dbaddress adr;
	boolean flignore;
	
	(**hdest).hrefcon = nil; /*default*/
	
	if ((**hsource).hrefcon == nil)
		return (true);
	
	megetmenuiteminfo (hsource, &item);
	
	ho = item.linkedscript.houtline;
	
	if (ho != nil) { /*copy the in-memory version*/
			
		if (!opcopyoutlinerecord (ho, &hcopy))
			return (false);
		}
		
	else { /*load the original in from disk*/
		
		adr = item.linkedscript.adrlink;
		
		if (adr == nildbaddress) /*no script linked into this line*/
			hcopy = nil;
			
		else {
			hdlmenurecord hm = (hdlmenurecord) (**op_get_outlinedata()).outlinerefcon;
			
			if (!meloadscriptoutline (hm, hsource, &hcopy, &flignore)) // 5.0a3 dmb - was: meloadoutline (adr, &hcopy)
				return (false);
			}
		}
	
	if (hcopy != nil)
		(**hcopy).fldirty = true; /*force save on this guy's script*/
	
	item.linkedscript.houtline = hcopy;
	
	item.linkedscript.adrlink = nildbaddress; /*hasn't been allocated yet*/
	
	mesetmenuiteminfo (hdest, &item);
	
	return (true);
	} /*mecopyrefconroutine*/


static boolean headleveloffsetvisit (hdlheadrecord hnode, ptrvoid refcon) {
	
	long *headleveloffset = (long *) refcon;
	
	(**hnode).headlevel += *headleveloffset;
	
	return (true);
	} /*headleveloffsetvisit*/


boolean metextualizerefconroutine (hdlheadrecord hnode, Handle htext) {
	
	/*
	5.1.5b16 dmb: get menu record from op_get_outlinedata(), not global
	*/
	
	register hdloutlinerecord ho;
	register boolean fl;
	register hdlheadrecord hsummit;
	hdloutlinerecord houtline;
	boolean fljustloaded;
	long headleveloffset;
	hdlmenurecord hm = (hdlmenurecord) (**op_get_outlinedata()).outlinerefcon;
	
	if (!meloadscriptoutline (hm, hnode, &houtline, &fljustloaded)) 
		return (false);
	
	ho = houtline; /*move into register*/
	
	if (ho == nil) /*no script linked into the menu item*/
		return (true);
	
	hsummit = (**ho).hsummit;
	
	headleveloffset = (**hnode).headlevel + 1; /*add extra indentation for clipboard*/
	
	opsiblingvisiter (hsummit, false, &headleveloffsetvisit, &headleveloffset);
	
	fl = opoutlinetotextscrap (ho, false, htext);
	
	if (fljustloaded)
		opdisposeoutline (ho, false);
	
	else {
		
		headleveloffset = -headleveloffset; /*prepare for reversal*/
		
		opsiblingvisiter (hsummit, false, &headleveloffsetvisit, &headleveloffset);
		}
	
	return (fl);
	} /*metextualizerefconroutine*/


static boolean mereleaserefconroutine_context (const db_context *ctx, hdlheadrecord hnode, boolean fldisk) {

	/* 2025-12-23: Refactored to use explicit context instead of push/pop pattern */

	/*
	5.1.4 dmb: set the right database when fldisk
	*/

	tymenuiteminfo item;

	megetmenuiteminfo (hnode, &item);

	opdisposeoutline (item.linkedscript.houtline, fldisk);

	if (fldisk) {

		dbpushreleasestack (item.linkedscript.adrlink, outlinevaluetype);
		}

	return (true);
	} /*mereleaserefconroutine_context*/


boolean mereleaserefconroutine (hdlheadrecord hnode, boolean fldisk) {

	return mereleaserefconroutine_context (NULL, hnode, fldisk);
	} /*mereleaserefconroutine*/

#pragma pack(2)
typedef struct typackinfo {

	Handle hpackedscripts; /*for pack/unpackscriptvisit routines*/

	long ixpackedscripts; /*for unpacking only*/
	} typackinfo, *ptrpackinfo;
#pragma options align=reset


static boolean mesavescriptvisit (hdlheadrecord hnode, ptrvoid refcon) {
#pragma unused (refcon)

	register hdlheadrecord h = hnode;
	//ptrpackinfo packinfo = (ptrpackinfo) refcon;
	register hdloutlinerecord ho;
	tymenuiteminfo item;
	
	rollbeachball ();
	
	assert (menudata != nil);
	
	if (!megetmenuiteminfo (h, &item)) /*nothing linked, keep visiting*/
		return (true);
	
	ho = item.linkedscript.houtline; /*copy into register*/
	
	if (ho == nil) /*if nil, nothing to worry about saving*/
		return (true);
	
	if ((**ho).fldirty) { /*needs saving*/
		
		if (!mesaveoutline (ho, &item.linkedscript.adrlink)) /*memory or disk error, stop visiting*/
			return (false);
		}
	
	if (ho != (**menudata).scriptoutline) { /*don't reclaim the active script'*/
		
		opdisposeoutline (ho, false); /*reclaim the script*/
	
		item.linkedscript.houtline = nil; /*force us to look to disk*/
		}
		
	else { /*'just clear its madechanges bit*/
		
		windowsetchanges ((**menudata).scriptwindow, false);
		
		(**ho).fldirty = false;
		}
	
	return (mesetmenuiteminfo (h, &item));
	} /*mesavescriptvisit*/
	

static boolean mesaveasscriptvisit (hdlheadrecord hnode, ptrvoid refcon) {
#pragma unused (refcon)

	/*
	for save as, we need to write a packed version of every script to 
	the new file.  db.c takes care of redirecting reads & writes as 
	necessary
	*/
	
	register hdlheadrecord h = hnode;
	hdloutlinerecord ho;
	tymenuiteminfo item;
	dbaddress adr;
	boolean fltempload = false;
	
	rollbeachball ();
	
	if (!megetmenuiteminfo (h, &item)) /*nothing linked, keep visiting*/
		return (true);
	
	ho = item.linkedscript.houtline; /*copy into register*/
	
	if (flconvertingolddatabase)
		if (!meloadscriptoutline (menudata, h, &ho, &fltempload)) /*error loading script*/
			return (false);
	
	if (ho != nil) {
	
		if (!mesaveoutline (ho, &adr)) /*memory or disk error, stop visiting*/
			return (false);
		}
	else {
		if (!dbcopy (item.linkedscript.adrlink, &adr)) /*copy packed version to new file*/
			return (false);
		}
	
	if (fltempload)
		opdisposeoutline (ho, false);
	
	item.linkedscript.adrlink = adr; /*link in current database will be restored by caller*/
	
	return (mesetmenuiteminfo (h, &item));
	} /*mesaveasscriptvisit*/


static boolean mepackscriptvisit (hdlheadrecord hnode, ptrvoid refcon) {
	
	/*
	if there's a script attached to hnode, pack it onto the end of 
	the hpackedscripts handle.
	*/
	
	register hdlheadrecord h = hnode;
	ptrpackinfo packinfo = (ptrpackinfo) refcon;
	register hdloutlinerecord ho;
	tymenuiteminfo item;
	Handle hpackedoutline;
	register dbaddress adr;
	boolean fl;
	
	if (!megetmenuiteminfo (h, &item)) /*nothing linked, keep visiting*/
		return (true);
	
	ho = item.linkedscript.houtline; /*copy into register*/
	
	if (ho != nil)
		return (oppackoutline (ho, &(*packinfo).hpackedscripts));
	
	adr = item.linkedscript.adrlink;
	
	if (adr == nildbaddress) /*no linked script*/
		return (true);
	
	if (!dbrefhandle (adr, &hpackedoutline)) 
		return (false);
	
	fl = pushhandle (hpackedoutline, (*packinfo).hpackedscripts);
	
	disposehandle (hpackedoutline);
	
	return (fl);
	} /*mepackscriptvisit*/


typedef struct tysavedmenuinfo_v7 {
	uint16_t versionnumber;      /* 2 bytes */
	uint8_t  _pad0[6];           /* 6 bytes padding to 8-byte boundary */
	uint64_t adroutline;         /* 8 bytes, big-endian on disk */
	int64_t  lnumcursor;         /* 8 bytes, big-endian on disk */
	uint32_t flags;              /* 4 bytes, big-endian on disk */
	uint32_t menuactiveitem;     /* 4 bytes, big-endian on disk */
	uint8_t  _reserved[1024];    /* 1024 bytes reserved for future use */
} tysavedmenuinfo_v7;

_Static_assert(sizeof(tysavedmenuinfo_v7) == 1056, "v7 menu struct must be exactly 1056 bytes");


static boolean mesavemenustructure_legacy (hdlmenurecord hm, dbaddress *adr) {

	/*
	Legacy (v6) save path: save the menu structure with 32-bit BE addresses.

	everything has already been set up:  everything pushed and dehoisted.
	save all of the data associated with the menubar, by visiting every
	node in the menu structure, saving off all linked scripts, and then
	saving the menubar outline itself

	after a script is saved, we dispose of the in-memory structure, unless it
	is the active script, or we're doing a Save As.
	*/

	hdlheadrecord hsummit;
	register boolean fl;
	tysavedmenuinfo info;

	opoutermostsummit (&hsummit);

	if (fldatabasesaveas)
		fl = opsiblingvisiter (hsummit, false, &mesaveasscriptvisit, nil);
	else
		fl = opsiblingvisiter (hsummit, false, &mesavescriptvisit, nil);

	assert (opvalidate (op_get_outlinedata()));

	if (!fl)
		return (false);

	dbaddress new_outline_adr;

	clearbytes (&info, sizeof (info));

	info.versionnumber = conditionalshortswap (1);

	info.adroutline = (**hm).adroutline;

	if (!mesaveoutline (op_get_outlinedata(), &info.adroutline))
		return (false);

	/* Capture the updated host-order address before BE32 conversion */
	new_outline_adr = info.adroutline;

	db_format_write_be32(&info.adroutline, (uint32_t) info.adroutline);

	fl = dbassign (adr, sizeof (tysavedmenuinfo), &info);

	/* Update in-memory address with new outline address */
	if (fl)
		(**hm).adroutline = new_outline_adr;

	return (fl);
	} /*mesavemenustructure_legacy*/


static boolean mesavemenustructure_v7 (hdlmenurecord hm, dbaddress *adr) {

	/*
	V7 save path: save the menu structure with 64-bit BE addresses.
	Uses tysavedmenuinfo_v7 disk format.
	*/

	hdlheadrecord hsummit;
	register boolean fl;
	tysavedmenuinfo_v7 v7info;
	dbaddress outline_adr;

	opoutermostsummit (&hsummit);

	if (fldatabasesaveas)
		fl = opsiblingvisiter (hsummit, false, &mesaveasscriptvisit, nil);
	else
		fl = opsiblingvisiter (hsummit, false, &mesavescriptvisit, nil);

	assert (opvalidate (op_get_outlinedata()));

	if (!fl)
		return (false);

	outline_adr = (**hm).adroutline;

	if (!mesaveoutline (op_get_outlinedata(), &outline_adr))
		return (false);

	clearbytes (&v7info, sizeof (v7info));

	v7info.versionnumber = host_to_disk_uint16 (2);
	v7info.adroutline = host_to_disk_uint64 ((uint64_t) outline_adr);
	v7info.lnumcursor = host_to_disk_uint64 (0); /* cursor position saved with outline */
	v7info.flags = host_to_disk_uint32 ((uint32_t) ((**hm).flautosmash ? flautosmash_mask : 0));
	v7info.menuactiveitem = host_to_disk_uint32 ((uint32_t) (**hm).menuactiveitem);

	fl = dbassign (adr, sizeof (tysavedmenuinfo_v7), &v7info);

	if (fl)
		(**hm).adroutline = outline_adr;

	return (fl);
	} /*mesavemenustructure_v7*/


static boolean mesavemenustructure (hdlmenurecord hm, dbaddress *adr) {

	if (db_format_mode_current().use_64bit_format)
		return mesavemenustructure_v7 (hm, adr);
	else
		return mesavemenustructure_legacy (hm, adr);
	} /*mesavemenustructure*/


/* Legacy (v<=6) pack/unpack helpers */
static boolean mepackmenustructure_legacy (tysavedmenuinfo *info, Handle *hpacked) {
	
	/*
	analogous to mesavemenustructure above, except we're packing everything 
	into memory.
	
	10/6/91 dmb: mergehandles now consumes both source handles.
	
	4.1b7 dmb: oppack was fixed so that it won't dispose of the handle we 
	allocated if it fails. so we must dispose it here on error.
	*/
	
	register boolean fl;
	Handle hpackedmenu;
	hdlheadrecord hsummit;
	typackinfo packinfo;
	
	if (!newemptyhandle (&packinfo.hpackedscripts)) /*handle where scripts will be saved*/
	 	return (false);
	
	opoutermostsummit (&hsummit);
	
	fl = opsiblingvisiter (hsummit, false, &mepackscriptvisit, &packinfo);
	
	assert (opvalidate (op_get_outlinedata()));
	
	if (fl)
		fl = newfilledhandle (info, sizeof (tysavedmenuinfo), &hpackedmenu);
	
	if (fl) {
		
		fl = oppack (&hpackedmenu);
		
		if (fl)
			fl = mergehandles (hpackedmenu, packinfo.hpackedscripts, hpacked);
		else
			disposehandle (hpackedmenu); /*4.1b7 dmb*/
		
		packinfo.hpackedscripts = nil; /*it's been consumed*/
		}
	
	disposehandle (packinfo.hpackedscripts);
	
	return (fl);
	} /*mepackmenustructure_legacy*/


boolean mesavemenurecord (hdlmenurecord hmenurecord, boolean flpreservelinks, boolean flmemory, dbaddress *adr, Handle *hpacked) {
	
	/*
	save the menu record handle in the database.
	
	dmb 9/21/90:  unfortunately, menudata global is heavily entrenched, set it at the
	beginning of the routine.
	
	dmb 10/23/90: the new flmemory parameter determines whether we're saving to disk 
	(using adr), or packing to memory (using hpacked)
	*/
	
	register hdlmenurecord hm = hmenurecord;
	register hdloutlinerecord ho = (**hm).menuoutline;
	register boolean fl;
	hdlheadrecord hcursor;
	tysavedmenuinfo info;
	register WindowPtr w;
	Rect r;
	int64_t lnumcursor;
	
	opvalidate (ho);
	
	menudata = hm; 
	
	clearbytes (&info, sizeof (info));
	
	info.versionnumber = conditionalshortswap (1);
	
	info.adroutline = (**hm).adroutline;
	
	info.vertmin = conditionalshortswap ((**ho).vertscrollinfo.min);
	
	info.vertmax = conditionalshortswap ((**ho).vertscrollinfo.max);
	
	info.vertcurrent = conditionalshortswap ((**ho).vertscrollinfo.cur);
	
	w = (**hm).scriptwindow;
	
	if (w == nil) 
		r = (**hm).scriptwindowrect;
	else
		getglobalwindowrect (w, &r);
	
	recttodiskrect (&r, &info.scriptwindowrect);
	
	if ((**hm).flautosmash)
		info.flags |= flautosmash_mask;
	
	recttodiskrect (&(**hm).menuwindowrect, &info.menuwindowrect);
	
	diskgetfontname ((**hm).defaultscriptfontnum, info.defaultscriptfontname);
	
	info.defaultscriptfontsize = conditionalshortswap ((**hm).defaultscriptfontsize);
	
	oppushoutline (ho);
	
	hcursor = (**ho).hbarcursor;
	
	opgetnodeline (hcursor, &lnumcursor);

	info.lnumcursor = lnumcursor; /* 64-bit in-memory format, byte-swapping happens in pack functions */
	
	oppopallhoists (); /*pop hoists, save state to be restored after saving*/
	
	(**ho).hbarcursor = hcursor; /*scotch tape!*/
	
	fl = true;
	
	if (flpreservelinks) {
		
		/*
		we're saving to another file, and need to preserve the memory version 
		of the menubar.  we'll make a copy that we can traverse and save 
		with the refcon links set up for the destination file.
		since we don't want to copy the scripts, and don't want them disposed 
		when we dispose the copy, we need to patch in op's default refcon
		callbacks while we do the copy.
		*/
		
		hdloutlinerecord hcopy;
		
		(**ho).copyrefconcallback = &opcopyrefconroutine;
		
		(**ho).releaserefconcallback = &opdefaultreleaserefconroutine;
		
		opsaveeditbuffer ();
		
		fl = opcopyoutlinerecord (ho, &hcopy);
		
		oprestoreeditbuffer ();
		
		(**ho).copyrefconcallback = &mecopyrefconroutine;
		
		(**ho).releaserefconcallback = &mereleaserefconroutine;
		
		if (!fl)
			goto exit;
		
		ho = opsetoutline (hcopy); /*work on copy*/
		}
	
	if (fl) {

		/* 2025-12-23: Refactored to use explicit context instead of push/pop pattern */

		if (flmemory) {

			fl = mepackmenustructure (&info, hpacked);
			}
		else {
			fl = mesavemenustructure (hm, adr);
			}
		}
	
	if (flpreservelinks) {
		
		opdisposeoutline (ho, false); /*dispose copy*/
		
		opsetoutline ((**hm).menuoutline); /*restore original*/
		}
	
	exit:
	
	oprestorehoists (); /*restore the hoist state*/
	
	oppopoutline (); /*pop outline globals*/
	
	return (fl);
	} /*mesavemenurecord*/


boolean mesetupmenurecord (tysavedmenuinfo *info, hdloutlinerecord houtline, hdlmenurecord *hmenurecord) { 
	
	/*
	create a new menu record with the given outline and setup info
	
	2/26/91 dmb: commented-out assignment into outline's vertical scroll 
	position, and use of info.lnumcursor.  all this stuff should already 
	be saved with the outline.  note: if it turns out that we need to 
	assign lnumcursor into movecursorto, we need to defeat it during 
	searches
	
	4/22/91 dmb: menu outline now points to menu variable
	*/
	
	register tysavedmenuinfo *lpi = info;
	register hdlmenurecord hm;
	register hdloutlinerecord ho;
	short fontnum;
	
	if (!newclearhandle (sizeof (tymenurecord), (Handle *) hmenurecord))
		return (false);
	
	hm = *hmenurecord; /*copy into register*/
	
	ho = houtline; /*copy into register*/
	
	mesetcallbacks (ho); /*link in callback routines*/
	
	(**hm).menuoutline = ho; 
	
	(**ho).outlinerefcon = (long) hm; /*pointing is mutual*/
	
	(**hm).adroutline = (*lpi).adroutline; /*keep address around for save*/
	
	diskrecttorect (&(*lpi).scriptwindowrect, &(**hm).scriptwindowrect);
	
	(**hm).flautosmash = ((*lpi).flags & flautosmash_mask) != 0;
	
	diskrecttorect (&(*lpi).menuwindowrect, &(**hm).menuwindowrect);
	
	diskgetfontnum ((*lpi).defaultscriptfontname, &fontnum);
	
	(**hm).defaultscriptfontnum = fontnum;
	
	(**hm).defaultscriptfontsize = conditionalshortswap((*lpi).defaultscriptfontsize);
	
	(**hm).menuactiveitem = menuoutlineitem;
	
	return (true);
	} /*mesetupmenurecord*/


static boolean meunpackscriptvisit (hdlheadrecord hnode, ptrvoid refcon) {
	
	/*
	4/30/91 dmb: set dirty bit on outline to make sure it gets saved
	*/
	
	register hdlheadrecord h = hnode;
	ptrpackinfo packinfo = (ptrpackinfo) refcon;
	hdloutlinerecord houtline;
	tymenuiteminfo item;
	
	if (!megetmenuiteminfo (h, &item)) /*nothing linked, keep visiting*/
		return (true);
	
	if (!item.linkedscript.adrlink && !item.linkedscript.houtline) /*no linked script*/
		return (true);
	
	if (!opunpack ((*packinfo).hpackedscripts, &(*packinfo).ixpackedscripts, &houtline)) /*memory error, stop visiting*/
		return (false);
	
	(**houtline).fldirty = true; /*make sure it gets saved*/
	
	item.linkedscript.houtline = houtline; /*in memory*/
	
	item.linkedscript.adrlink = nildbaddress; /*never saved to disk*/
	
	return (mesetmenuiteminfo (h, &item));
	} /*meunpackscriptvisit*/


static boolean meunpackmenustructure_legacy (Handle hpacked, hdlmenurecord *hmenurecord) {
	
	/*
	analagous to meloadmenurecord below, except the entire structure is 
	to be unpacked from the handle hpacked.
	
	9/25/91 dmb: use new testheapspace instead of haveheapspace for preflighting. 
	the latter doesn't generate an error, so we could have failed silently.
	*/
	
	Handle hpackedmenu;
	hdlheadrecord hsummit;
	hdloutlinerecord ho;
	register boolean fl;
	tysavedmenuinfo info;
	long ix = 0;
	typackinfo packinfo;
	
	assert (sizeof (tysavedmenuinfo) == sizeof (tyOLD42savedmenuinfo));
	
	if (!unmergehandles (hpacked, &hpackedmenu, &packinfo.hpackedscripts)) /*consumes hpacked*/
		return (false);
	
//	oppushoutline (nil); /*preserve global*/
	
	fl = loadfromhandle (hpackedmenu, &ix, sizeof (info), &info);
	
	info.adroutline = nildbaddress; /*never been saved*/
	
	if (fl)
		fl = opunpack (hpackedmenu, &ix, &ho);
	
//	ho = outlinedata; /*save in register.  may be nil*/
	
	disposehandle (hpackedmenu);
	
	if (fl)
		fl = mesetupmenurecord (&info, ho, hmenurecord);
	
	if (fl)
		fl = testheapspace (gethandlesize (packinfo.hpackedscripts)); /*a little preflighting*/
	
	if (fl) {
		
		packinfo.ixpackedscripts = 0; /*offset into hpackedscripts*/
		
		oppushoutline (ho);
		
		opoutermostsummit (&hsummit);
		
		fl = opsiblingvisiter (hsummit, false, &meunpackscriptvisit, &packinfo);
		
		oppopoutline ();
		}
	
	disposehandle (packinfo.hpackedscripts);
	
	if (!fl)
		opdisposeoutline (ho, false); /*checks for nil*/
	
//	oppopoutline (); /*restore global*/
	
	return (fl);
	} /*meunpackmenustructure_legacy*/

/* Modern (v7+) pack/unpack. For now, reuse legacy implementation but keep the fork explicit. */
/* Modern (v7+) pack/unpack with BE64 addresses and reserved padding. */

/* Byte order helpers now in byteorder_helpers.h (2026-02-02 consolidation) */

/*
 * Legacy (v6 and earlier) disk format structure.
 * The key difference from tysavedmenuinfo is that dbaddress was 32-bit on disk.
 * This structure must match what was written by Frontier 4.x-9.x on v6 databases.
 */
#pragma pack(2)
typedef struct tysavedmenuinfo_disk_legacy {
	short versionnumber;         /* structure version on disk */
	int32_t adroutline;          /* 32-bit address of menubar outline (BE on disk) */
	short vertmin, vertmax, vertcurrent;  /* scrollbar state */
	diskrect scriptwindowrect;   /* script window position */
	short flags;
	short menuactivelayer;
	short lnumcursor;
	diskfontstring defaultscriptfontname;
	short defaultscriptfontsize;
	diskrect menuwindowrect;     /* menu window position */
	char waste[42];              /* padding for growth */
} tysavedmenuinfo_disk_legacy;
#pragma options align=reset

/* Verify the legacy struct size matches v6 format expectations (112 bytes) */
_Static_assert(sizeof(tysavedmenuinfo_disk_legacy) == 112, "v6 menu struct must be exactly 112 bytes");

static boolean mepackmenustructure_v7(tysavedmenuinfo *legacy, Handle *hpacked) {
	tysavedmenuinfo_v7 modern;
	clearbytes(&modern, sizeof(modern));

	modern.versionnumber = host_to_disk_uint16(2);
	modern.adroutline = host_to_disk_uint64((uint64_t) legacy->adroutline);
	modern.lnumcursor = host_to_disk_uint64((uint64_t) legacy->lnumcursor);
	modern.flags = host_to_disk_uint32((uint32_t) legacy->flags);
	modern.menuactiveitem = host_to_disk_uint32((uint32_t) legacy->menuactivelayer); /* menuactivelayer in tysavedmenuinfo maps to menuactiveitem in v7 */

	Handle hpackedmenu = nil;
	Handle hpackedoutline = nil;
	Handle hpackedscripts = nil;
	boolean fl = false;
	boolean pushed_outline = false;

	if (!newfilledhandle(&modern, sizeof(modern), &hpackedmenu))
		goto exit;

	if (!newemptyhandle(&hpackedscripts))
		goto exit;

	/* Pack linked scripts (outline refcons) into hpackedscripts. */
	{
		if (menudata != nil && (**menudata).menuoutline != nil) {
			oppushoutline((**menudata).menuoutline);
			pushed_outline = true;
		}
		hdlheadrecord hsummit;
		typackinfo packinfo;
		packinfo.hpackedscripts = hpackedscripts;
		packinfo.ixpackedscripts = 0;
		opoutermostsummit(&hsummit);
		if (!opsiblingvisiter(hsummit, false, &mepackscriptvisit, &packinfo))
			goto exit;
		hpackedscripts = packinfo.hpackedscripts; /* may have moved */
		if (pushed_outline) {
			oppopoutline();
			pushed_outline = false;
		}
	}

	/* Pack menu outline itself. */
	hpackedoutline = nil;
	if (!oppack(&hpackedoutline))
		goto exit;

	/* Merge: menu header + outline + scripts */
	if (!mergehandles(hpackedmenu, hpackedoutline, &hpackedmenu))
		goto exit;
	hpackedoutline = nil; /* consumed by merge */
	if (!mergehandles(hpackedmenu, hpackedscripts, hpacked))
		goto exit;
	hpackedscripts = nil; /* consumed by merge */

	fl = true;

exit:
	if (pushed_outline)
		oppopoutline();
	if (!fl) {
		if (hpackedmenu) disposehandle(hpackedmenu);
		if (hpackedoutline) disposehandle(hpackedoutline);
		if (hpackedscripts) disposehandle(hpackedscripts);
	}
	return fl;
}

static boolean meunpackmenustructure_v7(Handle hpacked, hdlmenurecord *hmenurecord) {
	tysavedmenuinfo_v7 modern;
	long ix = 0;
	hdloutlinerecord ho = nil;
	Handle hpackedscripts = nil;
	boolean fl = false;
	boolean pushed_outline = false;

	if (!loadfromhandle(hpacked, &ix, sizeof(modern), &modern))
		return false;

	tysavedmenuinfo legacy;
	clearbytes(&legacy, sizeof(legacy));
	legacy.versionnumber = 1; /* not used downstream */
	legacy.adroutline = (dbaddress) disk_to_host_uint64(modern.adroutline);

	/* Legacy format uses 16-bit shorts. Values beyond SHRT_MAX cannot be represented.
	 * Rather than silently corrupt data, we fail the operation. */
	uint64_t lnumcursor_value = disk_to_host_uint64(modern.lnumcursor);
	if (lnumcursor_value > SHRT_MAX) {
		log_error(LOG_COMP_DB, "menupack: cannot convert lnumcursor %llu to 16-bit legacy format (value exceeds SHRT_MAX)",
		          (unsigned long long)lnumcursor_value);
		return false;
	}
	legacy.lnumcursor = (short) lnumcursor_value;

	uint32_t flags_value = disk_to_host_uint32(modern.flags);
	if (flags_value > SHRT_MAX) {
		log_error(LOG_COMP_DB, "menupack: cannot convert flags %u to 16-bit legacy format (value exceeds SHRT_MAX)",
		          (unsigned)flags_value);
		return false;
	}
	legacy.flags = (short) flags_value;

	uint32_t menuactiveitem_value = disk_to_host_uint32(modern.menuactiveitem);
	if (menuactiveitem_value > SHRT_MAX) {
		log_error(LOG_COMP_DB, "menupack: cannot convert menuactiveitem %u to 16-bit legacy format (value exceeds SHRT_MAX)",
		          (unsigned)menuactiveitem_value);
		return false;
	}
	legacy.menuactivelayer = (short) menuactiveitem_value;

	/* Remaining handle contains outline + scripts. */
	if (!opunpack(hpacked, &ix, &ho))
		goto exit;

	/* Any trailing data are packed scripts; walk and attach. */
	if (!loadhandleremains(ix, hpacked, &hpackedscripts))
		goto exit;

	if (!mesetupmenurecord(&legacy, ho, hmenurecord))
		goto exit;

	/* Replay script unpack on the attached outline. */
	if (hpackedscripts != nil) {
		oppushoutline(ho);
		pushed_outline = true;
		hdlheadrecord hsummit;
		typackinfo packinfo;
		packinfo.hpackedscripts = hpackedscripts;
		packinfo.ixpackedscripts = 0;
		opoutermostsummit(&hsummit);
		if (!opsiblingvisiter(hsummit, false, &meunpackscriptvisit, &packinfo))
			goto exit;
		oppopoutline();
		pushed_outline = false;
	}

	fl = true;

exit:
	if (pushed_outline)
		oppopoutline();
	if (hpackedscripts) disposehandle(hpackedscripts);
	if (!fl && ho != nil)
		opdisposeoutline(ho, false);
	return fl;
}

/* Public entrypoints: dispatch based on current format mode. */
boolean mepackmenustructure (tysavedmenuinfo *info, Handle *hpacked) {
	db_format_mode mode = db_format_mode_current();
	if (mode.use_64bit_format)
		return mepackmenustructure_v7(info, hpacked);
	return mepackmenustructure_legacy(info, hpacked);
}

boolean meunpackmenustructure (Handle hpacked, hdlmenurecord *hmenurecord) {
	/* Peek at the current mode; legacy loader handles v<=6 payloads. */
	db_format_mode mode = db_format_mode_current();
	if (mode.use_64bit_format)
		return meunpackmenustructure_v7(hpacked, hmenurecord);
	return meunpackmenustructure_legacy(hpacked, hmenurecord);
}


boolean meloadmenurecord_internal (const db_context *ctx, dbaddress adr,
                                    hdlmenurecord *hmenurecord) {
	/*
	Load menu record with explicit database context.

	Preconditions:
	  - ctx specifies database and format mode
	  - adr is valid menu record address

	Postconditions:
	  - *hmenurecord contains loaded menu structure
	  - Returns true on success, false on failure

	Note: v6 databases store menu info with 32-bit addresses, while the in-memory
	tysavedmenuinfo has 64-bit dbaddress. We must read the correct disk format
	and convert.
	*/

	hdloutlinerecord houtline;
	tysavedmenuinfo info;
	dbaddress outline_adr;

	if (!ctx) {
		db_context default_ctx;
		db_context_init(&default_ctx);
		return meloadmenurecord_internal(&default_ctx, adr, hmenurecord);
	}

	boolean is_v7_format = ctx->mode.use_64bit_format;

#if defined(FRONTIER_HEADLESS)
	log_debug(LOG_COMP_OP, "meloadmenurecord_internal: START adr=0x%llx ctx_db=%p is_v7=%d",
	        (unsigned long long)adr, (void*)ctx->database, (int)is_v7_format);
#endif

	if (is_v7_format) {
		/* v7 format: read the v7 disk format and convert to memory format */
		tysavedmenuinfo_v7 v7_info;

#if defined(FRONTIER_HEADLESS)
		log_debug(LOG_COMP_OP, "meloadmenurecord_internal: reading v7 adr=0x%llx sizeof(v7_info)=%zu",
		        (unsigned long long)adr, sizeof(v7_info));
#endif

		if (!dbreference_context (ctx, adr, sizeof (v7_info), &v7_info)) {
#if defined(FRONTIER_HEADLESS)
			log_error(LOG_COMP_OP, "meloadmenurecord_internal: dbreference_context FAILED (v7) adr=0x%llx",
			        (unsigned long long)adr);
#endif
			return (false);
		}

		/* Convert v7 disk format (64-bit, big-endian) to in-memory format */
		clearbytes (&info, sizeof (info));
		info.versionnumber = disk_to_host_uint16(v7_info.versionnumber);
		outline_adr = (dbaddress) disk_to_host_uint64(v7_info.adroutline);
		info.adroutline = outline_adr;

		/* lnumcursor is 64-bit in both v7 disk format and memory format */
		info.lnumcursor = (int64_t) disk_to_host_uint64(v7_info.lnumcursor);

		/* flags: bit flags stored as uint32 on disk, fits in short for defined flags.
		 * menuactiveitem → menuactivelayer: renamed field, same semantics (active menu layer index).
		 * Validate both fit in short to catch corrupted data. */
		uint32_t flags_value = disk_to_host_uint32(v7_info.flags);
		if (flags_value > SHRT_MAX) {
#if defined(FRONTIER_HEADLESS)
			log_error(LOG_COMP_OP, "meloadmenurecord_internal: flags value %" PRIu32 " exceeds SHRT_MAX", flags_value);
#endif
			return (false);
		}
		info.flags = (short) flags_value;

		uint32_t menuactive_value = disk_to_host_uint32(v7_info.menuactiveitem);
		if (menuactive_value > SHRT_MAX) {
#if defined(FRONTIER_HEADLESS)
			log_error(LOG_COMP_OP, "meloadmenurecord_internal: menuactivelayer value %" PRIu32 " exceeds SHRT_MAX", menuactive_value);
#endif
			return (false);
		}
		info.menuactivelayer = (short) menuactive_value;

		/* Note: v7 format intentionally omits GUI-only fields (vertmin/max/current, window rects,
		 * font settings). These are zeroed by clearbytes() above - correct for both headless and
		 * desktop builds since v7 format does not store them. */

#if defined(FRONTIER_HEADLESS)
		log_debug(LOG_COMP_OP, "meloadmenurecord_internal: v7 versionnumber=%d outline_adr=0x%" PRIx64 " lnumcursor=%" PRId64,
		        (int)info.versionnumber, (uint64_t)outline_adr, info.lnumcursor);
#endif
	}
	else {
		/* v6 format: read the legacy disk format with 32-bit addresses */
		tysavedmenuinfo_disk_legacy legacy_info;

#if defined(FRONTIER_HEADLESS)
		log_debug(LOG_COMP_OP, "meloadmenurecord_internal: reading v6 adr=0x%llx sizeof(legacy_info)=%zu",
		        (unsigned long long)adr, sizeof(legacy_info));
#endif

		if (!dbreference_context (ctx, adr, sizeof (legacy_info), &legacy_info)) {
#if defined(FRONTIER_HEADLESS)
			log_error(LOG_COMP_OP, "meloadmenurecord_internal: dbreference_context FAILED (v6) adr=0x%llx",
			        (unsigned long long)adr);
#endif
			return (false);
		}

		/* Convert legacy 32-bit disk format to in-memory 64-bit format */
		/* All values in the legacy struct are stored big-endian on disk */
		clearbytes (&info, sizeof (info));
		info.versionnumber = conditionalshortswap(legacy_info.versionnumber);
		/* Legacy 32-bit address is stored big-endian on disk */
		outline_adr = (dbaddress) conditionallongswap (legacy_info.adroutline);
#if defined(FRONTIER_HEADLESS)
		log_debug(LOG_COMP_OP, "meloadmenurecord_internal: v6 versionnumber=%d outline_adr=0x%llx",
		        (int)info.versionnumber, (unsigned long long)outline_adr);
#endif
		info.adroutline = outline_adr;
		info.vertmin = conditionalshortswap(legacy_info.vertmin);
		info.vertmax = conditionalshortswap(legacy_info.vertmax);
		info.vertcurrent = conditionalshortswap(legacy_info.vertcurrent);
		/* scriptwindowrect is a diskrect with 4 shorts */
		info.scriptwindowrect.top = conditionalshortswap(legacy_info.scriptwindowrect.top);
		info.scriptwindowrect.left = conditionalshortswap(legacy_info.scriptwindowrect.left);
		info.scriptwindowrect.bottom = conditionalshortswap(legacy_info.scriptwindowrect.bottom);
		info.scriptwindowrect.right = conditionalshortswap(legacy_info.scriptwindowrect.right);
		info.flags = conditionalshortswap(legacy_info.flags);
		info.menuactivelayer = conditionalshortswap(legacy_info.menuactivelayer);
		info.lnumcursor = conditionalshortswap(legacy_info.lnumcursor);
		/* diskfontstring is a pascal string - first byte is length, don't swap */
		memcpy(info.defaultscriptfontname, legacy_info.defaultscriptfontname, sizeof(diskfontstring));
		info.defaultscriptfontsize = conditionalshortswap(legacy_info.defaultscriptfontsize);
		/* menuwindowrect is a diskrect with 4 shorts */
		info.menuwindowrect.top = conditionalshortswap(legacy_info.menuwindowrect.top);
		info.menuwindowrect.left = conditionalshortswap(legacy_info.menuwindowrect.left);
		info.menuwindowrect.bottom = conditionalshortswap(legacy_info.menuwindowrect.bottom);
		info.menuwindowrect.right = conditionalshortswap(legacy_info.menuwindowrect.right);
	}

#if defined(FRONTIER_HEADLESS)
	log_debug(LOG_COMP_OP, "meloadmenurecord_internal: outline_adr=0x%llx", (unsigned long long)outline_adr);
#endif

	if (!meloadoutline_internal (ctx, outline_adr, &houtline)) {
#if defined(FRONTIER_HEADLESS)
		/* Expected in headless mode - menus are intentionally deferred */
		log_debug(LOG_COMP_OP, "meloadmenurecord_internal: meloadoutline_internal FAILED");
#endif
		return (false);
	}

	if (!mesetupmenurecord (&info, houtline, hmenurecord)) {
#if defined(FRONTIER_HEADLESS)
		log_debug(LOG_COMP_OP, "meloadmenurecord_internal: mesetupmenurecord FAILED");
#endif
		opdisposeoutline (houtline, false);

		return (false);
	}

#if defined(FRONTIER_HEADLESS)
	log_debug(LOG_COMP_OP, "meloadmenurecord_internal: SUCCESS hmenurecord=%p", (void*)*hmenurecord);
#endif
	return (true);
} /*meloadmenurecord_internal*/


boolean meloadmenurecord (dbaddress adr, hdlmenurecord *hmenurecord) {
	db_context ctx;

	/* Detect if we're reading from a legacy (v6) database.
	   During migration, the global mode may be v7, but we need to read
	   v6 data with v6 header sizes (8 bytes, not 12). */
	if (db_format_is_legacy_db(databasedata)) {
		db_context_init_legacy_read(&ctx, databasedata);
	} else {
		db_context_init(&ctx);
	}

	return meloadmenurecord_internal(&ctx, adr, hmenurecord);
} /*meloadmenurecord*/


static void medisposescrap (hdloutlinerecord houtline) {
	
	opdisposeoutline (houtline, false);
	} /*medisposescrap*/


static boolean meexportscrap (hdloutlinerecord houtline, tyscraptype totype, Handle *hexport, boolean *fltempscrap) {
	
	tysavedmenuinfo info;
	boolean fl;
	
	*fltempscrap = true; /*usually the case*/
	
	switch (totype) {
		
		case menuscraptype: /*export flat version for system scrap*/
			
			oppushoutline (houtline);
			
			clearbytes (&info, sizeof (info));
			
			fl = mepackmenustructure (&info, hexport);
			
			oppopoutline ();
			
			return (fl);
		
		case opscraptype:
			*hexport = (Handle) houtline; /*op and script scraps are the same*/
			
			*fltempscrap = false; /*it's the original, not a copy*/
			
			return (true);
		
		case textscraptype:
			return (opoutlinetonewtextscrap (houtline, hexport));
		
		default:
			return (false);
		}
	} /*meexportscrap*/


boolean mesetscraproutine (hdloutlinerecord houtline) {
	
	return (shellsetscrap ((Handle) houtline, menuscraptype,
								(shelldisposescrapcallback) &medisposescrap,
								(shellexportscrapcallback) &meexportscrap));
	} /*mesetscraproutine*/


boolean megetscraproutine (hdloutlinerecord *houtline, boolean *fltempscrap) {
	
	Handle hscrap;
	tyscraptype scraptype;
	
	if (!shellgetscrap (&hscrap, &scraptype))
		return (false);
	
	if (scraptype == menuscraptype) {
		
		*houtline = (hdloutlinerecord) hscrap;
		
		*fltempscrap = false; /*we're returning a handle to the actual scrap*/
		
		return (true);
		}
	
	return (shellconvertscrap (opscraptype, (Handle *) houtline, fltempscrap));
	} /*megetscraproutine*/


boolean mescraphook (Handle hscrap) {
	
	/*
	if our private type is on the external clipboard, set the internal 
	scrap to it.
	*/
	
	if (getscrap (menuscraptype, hscrap)) {
		
		hdlmenurecord hmenurecord;
		
		if (meunpackmenustructure (hscrap, &hmenurecord)) {
			
			mesetscraproutine ((**hmenurecord).menuoutline);
			
			(**hmenurecord).menuoutline = nil;
			
			medisposemenurecord (hmenurecord, false);
			}
		
		return (false); /*don't call any more hooks*/
		}
	
	return (true); /*keep going*/
	} /*mescraphook*/


/*
 * V7 Menu Refcon Table Functions
 *
 * These functions create and unpack the v7 format for menu item refcons.
 * The v7 format stores menu item data as a packed table with:
 * - keyBinding (char): command key character or 0 if none
 * - modifiers (table): {shift, control, option, command} booleans
 * - handlerScript (script): the script object or nil if none
 *
 * 2026-02-03: Initial implementation for menu v6->v7 migration.
 */

/* String constants for table keys - use BIGSTRING macro for pointer to string literals
 * Note: Hex escapes are greedy in C, so "\x07control" becomes "\x7c" + "ontrol".
 * Use string concatenation to break: "\x07" "control" */
#define bsKeyBinding      BIGSTRING("\x0a" "keyBinding")
#define bsModifiers       BIGSTRING("\x09" "modifiers")
#define bsHandlerScript   BIGSTRING("\x0d" "handlerScript")
#define bsShift           BIGSTRING("\x05" "shift")
#define bsControl         BIGSTRING("\x07" "control")
#define bsOption          BIGSTRING("\x06" "option")
#define bsCommand         BIGSTRING("\x07" "command")


boolean mecreaterefcontable_v7 (byte cmdkey, tykeyflags modifiers,
                                 hdloutlinerecord hscript,
                                 Handle *hpackedtable) {
	/*
	 * Create a v7 table refcon from menu item data.
	 *
	 * The table contains:
	 * - keyBinding (char): command key character or 0 if none
	 * - modifiers (table): {shift, control, option, command} booleans
	 * - handlerScript (script): the script object or nil if none
	 *
	 * Implementation:
	 * 1. Create temporary in-memory hash table
	 * 2. Add keyBinding char value
	 * 3. Create modifiers sub-table with boolean flags
	 * 4. If hscript != nil, create script external and add as handlerScript
	 * 5. Pack the table using langpackvalue()
	 * 6. Dispose the temporary table
	 *
	 * Returns true on success, false on failure.
	 */

	hdlhashtable htable = nil;
	hdlhashtable hmodifiers = nil;
	tyvaluerecord tableval = {0};  /* Initialize to prevent uninitialized access in cleanup */
	boolean fl = false;

	*hpackedtable = nil;

	/* Create the main table */
	if (!tablenewtablevalue(&htable, &tableval))
		goto exit;

	/* Add keyBinding (always present, 0 means no keybinding) */
	if (!langassigncharvalue(htable, bsKeyBinding, cmdkey))
		goto exit;

	/* Create and populate the modifiers sub-table */
	if (!langassignnewtablevalue(htable, bsModifiers, &hmodifiers))
		goto exit;

	/* Add modifier boolean flags */
	if (!langassignbooleanvalue(hmodifiers, bsShift, (modifiers & keyshift) != 0))
		goto exit;
	if (!langassignbooleanvalue(hmodifiers, bsControl, (modifiers & keycontrol) != 0))
		goto exit;
	if (!langassignbooleanvalue(hmodifiers, bsOption, (modifiers & keyoption) != 0))
		goto exit;
	if (!langassignbooleanvalue(hmodifiers, bsCommand, (modifiers & keycommand) != 0))
		goto exit;

	/* Add handlerScript if present */
	if (hscript != nil) {
		tyvaluerecord scriptval;

		/* Create a script external value from the outline.
		 * Note: We need to copy the outline since langexternalnewvalue takes ownership.
		 */
		hdloutlinerecord hscriptcopy;
		if (!opcopyoutlinerecord(hscript, &hscriptcopy))
			goto exit;

		if (!langexternalnewvalue(idscriptprocessor, (Handle)hscriptcopy, &scriptval)) {
			opdisposeoutline(hscriptcopy, false);
			goto exit;
		}

		/* hashtableassign() transfers ownership of scriptval on success.
		 * On failure, we must dispose scriptval ourselves. */
		if (!hashtableassign(htable, bsHandlerScript, scriptval)) {
			disposevaluerecord(scriptval, false);
			goto exit;
		}
	}

	/* Pack the table */
	if (!langpackvalue(tableval, hpackedtable, HNoNode))
		goto exit;

	fl = true;

exit:
	/* Clean up the temporary table.
	 * Note: tableval holds the external wrapper around htable.
	 * Disposing the value will clean up the table and any nested tables. */
	if (tableval.valuetype == externalvaluetype) {
		disposevaluerecord(tableval, false);
	}

	return fl;
} /*mecreaterefcontable_v7*/


boolean meunpackrefcontable_v7 (Handle hpackedtable,
                                 byte *cmdkey,
                                 tykeyflags *modifiers,
                                 hdloutlinerecord *hscript) {
	/*
	 * Unpack a v7 table refcon and extract menu item info.
	 * Inverse of mecreaterefcontable_v7().
	 *
	 * Implementation:
	 * 1. Unpack the table using langunpackvalue()
	 * 2. Look up keyBinding, convert to byte
	 * 3. Look up modifiers sub-table, extract boolean flags
	 * 4. Look up handlerScript, if exists extract the script outline
	 * 5. Dispose the unpacked value
	 *
	 * Returns true on success, false on failure.
	 */

	tyvaluerecord tableval;
	hdlhashtable htable;
	hdlhashnode hnode;
	boolean fl = false;

	/* Initialize outputs */
	*cmdkey = 0;
	*modifiers = keynormal;
	*hscript = nil;

	/* Unpack the table */
	if (!langunpackvalue(hpackedtable, &tableval))
		return false;

	if (tableval.valuetype != externalvaluetype) {
		disposevaluerecord(tableval, false);
		return false;
	}

	/* Get the hash table from the external value */
	if (!langexternalvaltotable(tableval, &htable, HNoNode)) {
		disposevaluerecord(tableval, false);
		return false;
	}

	/* Extract keyBinding */
	{
		tyvaluerecord keyval;
		if (hashtablelookup(htable, bsKeyBinding, &keyval, &hnode)) {
			if (keyval.valuetype == charvaluetype) {
				*cmdkey = keyval.data.chvalue;
			}
		}
	}

	/* Extract modifiers */
	{
		tyvaluerecord modval;
		if (hashtablelookup(htable, bsModifiers, &modval, &hnode)) {
			hdlhashtable hmodifiers;
			if (langexternalvaltotable(modval, &hmodifiers, HNoNode)) {
				tyvaluerecord boolval;
				tykeyflags flags = keynormal;

				if (hashtablelookup(hmodifiers, bsShift, &boolval, &hnode)) {
					if (boolval.valuetype == booleanvaluetype && boolval.data.flvalue)
						flags |= keyshift;
				}
				if (hashtablelookup(hmodifiers, bsControl, &boolval, &hnode)) {
					if (boolval.valuetype == booleanvaluetype && boolval.data.flvalue)
						flags |= keycontrol;
				}
				if (hashtablelookup(hmodifiers, bsOption, &boolval, &hnode)) {
					if (boolval.valuetype == booleanvaluetype && boolval.data.flvalue)
						flags |= keyoption;
				}
				if (hashtablelookup(hmodifiers, bsCommand, &boolval, &hnode)) {
					if (boolval.valuetype == booleanvaluetype && boolval.data.flvalue)
						flags |= keycommand;
				}

				*modifiers = flags;
			}
		}
	}

	/* Extract handlerScript if present */
	{
		tyvaluerecord scriptval;
		if (hashtablelookup(htable, bsHandlerScript, &scriptval, &hnode)) {
			if (scriptval.valuetype == externalvaluetype) {
				hdloutlinerecord houtline;
				if (opvaltoscript(scriptval, &houtline)) {
					/* Copy the outline since we're about to dispose the table */
					if (!opcopyoutlinerecord(houtline, hscript)) {
						log_error(LOG_COMP_OP, "meunpackrefcontable_v7: failed to copy script outline");
						disposevaluerecord(tableval, false);
						return false;
					}
				}
			}
		}
	}

	fl = true;

	/* Dispose the unpacked value (this will clean up the table) */
	disposevaluerecord(tableval, false);

	return fl;
} /*meunpackrefcontable_v7*/
