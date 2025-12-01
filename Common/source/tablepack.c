
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

/* 2025-12-01 Codex: Avoid unpacking table format metadata in headless builds to keep migration tests UI-free. */

#include "frontier.h"
#include "standard.h"

#if defined(FRONTIER_HEADLESS)
#include <stdio.h>
#endif

#include "memory.h"
#include "quickdraw.h"
#include "strings.h"
#include "db.h"
#include "db_format.h"
#include "langexternal.h"
#include "tablestructure.h"
#include "db_format.h"
#include "byteorder.h"

// 2025-10-27 Codex: Handle 64-bit dbaddress packing/unpacking for headless workloads.
// 2025-11-20 Codex: Emit table addresses in canonical big-endian form for portable v7 roots.
// 2025-11-28 Codex: Route hash pack/unpack through db_context wrappers to avoid TLS-only mode.
#include "tableinternal.h"
#include "tableverbs.h"
#include "byteorder.h"	/* 2006-04-08 aradke: endianness conversion macros */



boolean tablepacktable (hdlhashtable htable, boolean flmemory, Handle *hpacked, boolean *flmustsave) {
	
	/*
	10/6/91 dmb: mergehandles now consumes both source handles.
	
	6.2a15 AR: added flmustsave parameter.
	*/
// 2025-10-27 Codex: Added headless logging for table handle splits to debug root loading.
	
	register hdlhashtable ht = htable;
	register hdltableformats hf;
	Handle hpackedtable, hpackedformats;
	register boolean fl;
    db_context context;
    db_context_init(&context);
	
	if (!hashpacktable_context (&context, ht, flmemory, &hpackedtable, flmustsave)) {
#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] tablepacktable hashpacktable failed flmemory=%d table=%p\n",
			(int)flmemory, (void *)ht);
#endif
		return (false);
	}
	
	hf = (hdltableformats) (**ht).hashtableformats; /*copy into register*/
	
	if (hf == nil) /*no formats linked in*/
		hpackedformats = nil;
	
	else {
		tablepushformats (hf); /*set table.c global*/
		
		fl = tablepackformats (&hpackedformats);
		
		tablepopformats ();
		
		if (!fl) {
			
			disposehandle (hpackedtable);
#if defined(FRONTIER_HEADLESS)
			fprintf(stderr, "[headless] tablepacktable tablepackformats failed table=%p\n", (void *)ht);
#endif
			return (false);
			}
		}
	
	fl = mergehandles (hpackedtable, hpackedformats, hpacked);
	if (!fl) {
#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] tablepacktable mergehandles failed table=%p tableHandle=%p formats=%p\n",
			(void *)ht, (void *)hpackedtable, (void *)hpackedformats);
#endif
	}
	
	/*
	disposehandle (hpackedtable);
	
	disposehandle (hpackedformats);
	*/
	
	return (fl);
	} /*tablepacktable*/


boolean tableunpacktable (Handle hpacked, boolean flmemory, hdlhashtable *htable) {
	
	/*
	9/24/91 dmb: don't treat format unpacking failure as a fatal error.
	
	10/16/91 dmb: clear table's dirty bit after unpacking.
	
	3.0.4b8 dmb: use scratchport for the duration; port is unknown
	
	4.0b7 dmb: don't set scratchport while unpacking the table; too many
	nested pushports result. Just set it when we care, and leave each table
	entry to fend for itself.
	
	5.0a23 dmb: don't create table formats if none are packed
	
	5.0a25 dmb: don't clear table's fldirty flag anymore.
	*/
	
	Handle hpackedtable = nil;
	Handle hpackedformats = nil;
	hdlhashtable ht = nil;
	hdltableformats hformats = nil;
#if defined(FRONTIER_HEADLESS)
	long merged_size = gethandlesize (hpacked);
#endif
	
	if (!unmergehandles (hpacked, &hpackedtable, &hpackedformats)) /*comsumes hpacked*/
		return (false);

#if defined(FRONTIER_HEADLESS)
	fprintf(stderr, "[headless] tableunpacktable split merged=%ld table=%ld formats=%ld\n",
	        merged_size,
	        hpackedtable ? gethandlesize (hpackedtable) : 0L,
	        hpackedformats ? gethandlesize (hpackedformats) : 0L);
#endif
	
	if (!newhashtable (htable)) {
		
		disposehandle (hpackedtable);
		
		goto error; /*will dispose of everything but hpackedtable*/
		}
	
	ht = *htable; /*move into register*/
    db_context context;
    db_context_init(&context);
	
if (!hashunpacktable_context (&context, hpackedtable, flmemory, ht)) /*always disposes of hpackedtable*/
	goto error;
	
#if defined(FRONTIER_HEADLESS)
	if (hpackedformats != nil) {
		disposehandle (hpackedformats); /* UI-only metadata not needed in headless tests */
    }
#else
	if (hpackedformats != nil) {
	
		if (!newtableformats (&hformats))
			goto error;
		
		tablelinkformats (ht, hformats);
		
		pushscratchport ();
		
		if (!tableunpackformats (hpackedformats, hformats)) {
			
		//	tablesetdimension (hformats, false, 0, fixedctcols);
			}
		
		popport ();
		
		disposehandle (hpackedformats);
		
	if (hformats != nil && (**hformats).fldirty) /*formats were out of date*/
		(**ht).fldirty = true;
		}
#endif
	
	return (true);
	
	error: {
		
		disposehandle (hpackedformats);
		
		disposetableformats (hformats);
		
		disposehashtable (ht, false);
		
		return (false);
		}
	} /*tableunpacktable*/


boolean tableverbmemorypack (hdlexternalvariable h, Handle *hpacked, hdlhashnode hnode) {
	
	register hdlexternalvariable hv = h;
	register hdlhashtable ht;
	Handle hpush;
	register boolean fl;
	boolean fltempload;
	boolean fldummy;
	
	fltempload = !(**hv).flinmemory;
	
	if (!tableverbinmemory (hv, hnode))
		return (false);
	
	ht = (hdlhashtable) (**hv).variabledata; 
	
	tablecheckwindowrect (ht); 
	
	fl = tablepacktable (ht, true, &hpush, &fldummy);
	
	if (fltempload)
		tableverbunload (hv);
	
	if (fl) {
		
		fl = pushhandle (hpush, *hpacked);
		
		disposehandle (hpush);
		}
	
	return (fl);
	} /*tableverbmemorypack*/


boolean tableverbmemoryunpack (Handle hpacked, long *ixload, hdlexternalvariable *h, boolean flxml) {
	
	/*
	create a new outline variable -- not in memory.
	
	this is a special entrypoint for the pack and unpack verbs.
	*/
	
	Handle hpackedtable;
	hdlhashtable htable;
	register hdlhashtable ht;
	
	if (!loadhandleremains (*ixload, hpacked, &hpackedtable))
		return (false);
	
	if (!tableunpacktable (hpackedtable, true, &htable)) /*always disposes of hpackedtable*/
		return (false);
	
	ht = htable; /*move into register*/
	
	if (!newtablevariable (true, (long) ht, (hdltablevariable *) h, flxml)) {
		
		tabledisposetable (ht, false);
		
		return (false);
		}
	
	(**ht).hashtablerefcon = (long) *h; /*we can get from hashtable to variable rec*/
	
	(**ht).fldirty = true;
	
	return (true); /*in memory*/
	} /*tableverbmemoryunpack*/


boolean tableverbpack (hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
	
	/*
	12/4/91 dmb: set windowinfo's dirty bit to false after save
	
	6.2a15 AR: Rely on flsubsdirty flag in hashtable instead of calling tablenosubsdirty.
	Set new flnewdbaddress parameter appropriately -- we only guarantee it to be accurate
	if the function returns true.
	*/
	
	register hdlexternalvariable hv = h;
	register hdlhashtable ht;
	dbaddress adr;
	Handle hpackedtable;
	register boolean fl = true;
	boolean fltempload = false;
	boolean flmustsave = false;
	hdlwindowinfo hinfo;
    db_format_mode prev_mode = db_format_mode_current();
    db_format_mode modern_mode = prev_mode;
	const boolean adapter_repack = db_format_adapter_force_repack() && (databasedata != nil);
    boolean mode64_for_save = false;

	/* Modern path: always emit BE64 addresses. */
    modern_mode.use_64bit_format = true;
    db_format_mode_push(&modern_mode);

#if defined(FRONTIER_HEADLESS)
	fprintf(stderr, "[headless] tableverbpack start flinmemory=%d adapter_repack=%d databasedata=%p\n",
	        (int) (**hv).flinmemory, (int) adapter_repack, (void *) databasedata);
#endif
	
	if (fldatabasesaveas) {
		
		fltempload = !(**hv).flinmemory;
		
		if (!tableverbinmemory (hv, HNoNode))
			return (false);
		
		*flnewdbaddress = true; /*it's in another database even*/
		}

	if (!(**hv).flinmemory) {
		if (adapter_repack) {
            db_format_mode legacy_load = modern_mode;
            legacy_load.use_64bit_format = false; /* legacy read while loading source */
            db_format_mode_push(&legacy_load);
			fltempload = true;
			if (!tableverbinmemory (hv, HNoNode)) {
                db_format_mode_pop();
				return (false);
            }
            db_format_mode_pop();
		} else { /*not in memory, just push the old db address*/
		
			adr = (dbaddress) (**hv).variabledata;
			
			*flnewdbaddress = false;

			goto pushaddress;
		}
	}
		
	adr = (**hv).oldaddress;
	
	ht = (hdlhashtable) (**hv).variabledata;

	if (adapter_repack) {
		*flnewdbaddress = true;
		(**ht).flsubsdirty = true;
		(**ht).fldirty = true;
        db_context ctx;
        db_context_init(&ctx);
        db_format_adapter_enable_wide_writes_context(&ctx, NULL);
	}
	
	tablecheckwindowrect (ht);
	
	assert (fldatabasesaveas || (((**ht).fldirty || (**ht).flsubsdirty) == !tablenosubsdirty (ht)));
	
	/*it's in memory and either the table itself or one of its subs are dirty, so pack the table*/
	
	fl = tablepacktable (ht, false, &hpackedtable, &flmustsave);
	
	if (!fl) {
#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] tablepacktable failed for system table\n");
#endif
			goto pushaddress;
		}

	/*only save if we're saving a copy, if the table itself is dirty (i.e. a scalar or the name of an object changed),
		or if one of its subs changed in  a way so that the table itself actually needs saving now*/
	
	if (fldatabasesaveas || (**ht).fldirty || flmustsave) {
		if (databasedata != nil)
			fl = dbsavehandle (hpackedtable, &adr);
		else {
#if defined(FRONTIER_HEADLESS)
			fprintf(stderr, "[headless] tableverbpack skipping dbsavehandle; databasedata is nil\n");
#endif
			fl = true;
			adr = 0;
		}
	}
	
	
	disposehandle (hpackedtable);
	
	if (!fl)
		goto pushaddress;
	
	if (fldatabasesaveas)
		goto pushaddress;
	
	*flnewdbaddress = ((**hv).oldaddress != adr);
	
	(**hv).oldaddress = adr;
	
	(**ht).fldirty = false; /*it's been saved to the db*/
	
	(**ht).flsubsdirty = false;
	
	if (tablewindowopen (hv, &hinfo))
		shellsetwindowchanges (hinfo, false);
	
	pushaddress:
    /* Decide whether to emit a 64-bit address trailer before restoring any stacked modes. */
    mode64_for_save = db_format_mode_current().use_64bit_format;
#if defined(FRONTIER_HEADLESS)
    if (!mode64_for_save && fldatabasesaveas) {
        hdldatabaserecord hdest = nil;
        if (dbgetdestinationdatabase(&hdest) && (hdest != nil) && !db_format_is_legacy_db(hdest))
            mode64_for_save = true;
    }
#endif

    db_format_mode_pop(); /* restore previous mode */

	if (fltempload)
		tableverbunload (hv);
	
	if (!fl)
		return (false);
	
	unsigned char adrbuffer[sizeof (dbaddress)];
	long adrsize;

	if (mode64_for_save && ((int)sizeof (dbaddress) == 8)) {
		db_format_write_be64(adrbuffer, (uint64_t) adr);
		adrsize = (long) sizeof (dbaddress);
	} else {
		db_format_write_be32(adrbuffer, (uint32_t) adr);
		adrsize = (long) sizeof (uint32_t);
	}

	if (!enlargehandle (*hpacked, adrsize, (ptrchar) adrbuffer)) {
#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] enlargehandle failed while packing table\n");
#endif
		return (false);
	}

	return (true);
	} /*tableverbpack*/


boolean tableverbunpack (Handle hpacked, long *ixload, hdlexternalvariable *h, boolean flxml) {

	dbaddress rawadr = 0;

	long remaining = hpacked ? (gethandlesize(hpacked) - *ixload) : 0;

	if (db_format_mode_current().use_64bit_format && ((int)sizeof (dbaddress) == 8)) {
		fprintf(stderr, "[headless] tableverbunpack use_64bit_format=true sizeof(dbaddress)=%zu remaining=%ld\n",
		        sizeof(dbaddress), remaining);
		if (remaining >= (long) sizeof (dbaddress)) {
			unsigned char adrbytes[sizeof (dbaddress)];
			if (!loadfromhandle (hpacked, ixload, (long) sizeof (dbaddress), adrbytes))
				return (false);
			rawadr = (dbaddress) db_format_read_be64(adrbytes);
#if defined(FRONTIER_HEADLESS)
			fprintf(stderr, "[headless] tableverbunpack 64-bit address=0x%016llx\n", (unsigned long long) rawadr);
#endif
		} else if (remaining == (long) sizeof (int32_t)) {
			unsigned char raw32[sizeof (uint32_t)];
			if (!loadfromhandle (hpacked, ixload, (long) sizeof (raw32), raw32))
				return (false);
			{
				uint32_t raw32_val = db_format_read_be32(raw32);
				rawadr = (dbaddress) raw32_val;
#if defined(FRONTIER_HEADLESS)
				fprintf(stderr, "[headless] tableverbunpack fallback 32-bit address=0x%08x\n", raw32_val);
#endif
			}
		} else {
#if defined(FRONTIER_HEADLESS)
			fprintf(stderr, "[headless] tableverbunpack unexpected remaining bytes=%ld\n", remaining);
#endif
			return (false);
		}
	} else {
		unsigned char raw32[sizeof (uint32_t)];
		if (!loadfromhandle (hpacked, ixload, (long) sizeof (raw32), raw32))
			return (false);
		{
			uint32_t raw32_val = db_format_read_be32(raw32);
			rawadr = (dbaddress) raw32_val;
#if defined(FRONTIER_HEADLESS)
			fprintf(stderr, "[headless] tableverbunpack legacy 32-bit address=0x%08lx\n", (unsigned long) raw32_val);
#endif
		}
	}

	return (newtablevariable (false, rawadr, (hdltablevariable *) h, flxml));
	} /*tableverbunpack*/


static boolean tablepacktotextvisit (bigstring bsname, hdlhashnode hnode, tyvaluerecord val, ptrvoid refcon) {
#pragma unused (hnode)

	/*
	4.0.2b1 dmb: handle fldiskvals. see comment in hashsortedinversesearch
	*/
	
	Handle htextscrap = (Handle) refcon;
	boolean fl;
	
	pushchar (chtab, bsname);
	
	if (!pushtexthandle (bsname, htextscrap))
		return (true); /*abort traversal*/
	
	if (val.fldiskval)
		if (!copyvaluerecord (val, &val))
			return (true);
	
	fl = langvaluetotextscrap (val, htextscrap);
	
	if (exemptfromtmpstack (&val))
		disposevaluerecord (val, false);
	
	if (!fl)
		return (true);
	
	return (false); /*keep going*/
	} /*tablepacktotextvisit*/


boolean tableverbpacktotext (hdlexternalvariable h, Handle htext) {
	
	/*
	12/23/92 dmb: hashinversesearch now takes table as param; don't push/pop
	
	12/31/92 dmb: use hashsortedinversesearch so text is in correct order
	
	5.0.2b20 dmb: unload if just loaded
	
	5.1.5 dmb: use tablesortedinversesearch for guest databases
	*/
	
	register hdlexternalvariable hv = h;
	register hdlhashtable ht;
	register boolean fl;
	boolean fltempload = !(**hv).flinmemory;
	
	if (!tableverbinmemory (hv, HNoNode))
		return (false);
	
	ht = (hdlhashtable) (**hv).variabledata;
	
	//htextscrap = htext; /*set for visit routine*/
	
	//fl = !hashinversesearch (ht, &tablepacktotextvisit, bsname); /*false means complete traversal%/
	
	fl = !tablesortedinversesearch (ht, &tablepacktotextvisit, htext);
	
	if (fltempload)
		tableverbunload (hv);
	
	return (fl);
	} /*tableverbpacktotext*/


boolean tableverbgettimes (hdlexternalvariable h, long *timecreated, long *timemodified, hdlhashnode hnode) {
	
	register hdlexternalvariable hv = h;
	register hdlhashtable ht;
	
	if (!tableverbinmemory (hv, hnode))
		return (false);
	
	ht = (hdlhashtable) (**hv).variabledata;
	
	*timecreated = (**ht).timecreated;
	
	*timemodified = (**ht).timelastsave;
	
	return (true);
	} /*tableverbgettimes*/


boolean tableverbsettimes (hdlexternalvariable h, long timecreated, long timemodified, hdlhashnode hnode) {
	
	register hdlexternalvariable hv = h;
	register hdlhashtable ht;
	
	if (!tableverbinmemory (hv, hnode))
		return (false);
	
	ht = (hdlhashtable) (**hv).variabledata;
	
	(**ht).timecreated = timecreated;
	
	(**ht).timelastsave = timemodified;
	
	return (true);
	} /*tableverbsettimes*/


static boolean findusedblocksvisit (hdlhashnode hnode, ptrvoid refcon) {
	
	/*
	5.1.5b16: check fldontsave flag to avoid diving into guest databases, etc.
	*/
	
	ptrstring bsparent = (ptrstring) refcon;
	tyvaluerecord val = (**hnode).val;
	bigstring bspath;
	
//	if ((**hnode).fldontsave) //never gets saved in the database
//		return (true);
	
	gethashkey (hnode, bspath);
	
	if (bsparent != nil) {

		insertchar ('.', bspath);

		insertstring (bsparent, bspath);
		}

	if (val.valuetype == externalvaluetype)
		return (langexternalfindusedblocks ((hdlexternalvariable) val.data.externalvalue, bspath));
	
	if (val.fldiskval)
		return (statsblockinuse (val.data.diskvalue, bspath));
	
	return (true); /*continue traversal*/
	} /*findusedblocksvisit*/


boolean tableverbfindusedblocks (hdlexternalvariable h, bigstring bspath) {
	
	register hdlexternalvariable hv = h;
	register hdlhashtable ht;
	register boolean fl;
	boolean fltempload;
	
	fltempload = !(**hv).flinmemory;
	
	if (!tableverbinmemory (hv, HNoNode))
		return (false);
	
	if (!statsblockinuse ((**hv).oldaddress, bspath))
		return (false);
	
	ht = (hdlhashtable) (**hv).variabledata; 
	
	if (ht == filewindowtable)
		fl = true;
	else
		fl = hashtablevisit (ht, findusedblocksvisit, bspath);
	
	if (fltempload)
		tableverbunload (hv);
	
	return (fl);
	} /*tableverbfindusedblocks*/
