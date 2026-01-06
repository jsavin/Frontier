
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

#include "file.h"
#include "memory.h"
#include "strings.h"
#include "timedate.h"
#include "resources.h"
#include "langinternal.h"
#include "db_format.h"
#include "langexternal.h"
#include "tableinternal.h"
#include "tableverbs.h"
#include "tablestructure.h"
#include "byteorder.h"	/* 2006-04-08 aradke: endianness conversion macros */
#include "logging.h"
// 2025-11-28 Codex: Apply db_context wrappers when loading HASH resources.


/*
the routines in this file determine the overall structure of the system
symbol tables in CanCoon.
*/



byte nameinternaltable [] = STR_compiler;

byte namemenubar [] = STR_menubar;

byte namebuiltinstable [] = STR_builtins;

byte namepathstable [] = STR_paths;

byte nameverbstable [] = STR_verbs;

byte nameiacgluetable [] = STR_apps;

byte nameiachandlertable [] = STR_traps;

byte nameagentstable [] = STR_agents;

byte nameresourcestable [] = STR_misc;

byte nameefptable [] = STR_kernel;

byte namelangtable [] = STR_language;

byte namestacktable [] = STR_stack;

byte namesemaphoretable [] = STR_semaphores; /*4.0b7 dmb*/

byte namethreadtable [] = STR_threads;	/* 4.0.1b1 dmb*/

byte namefilewindowtable [] = STR_filewindows; // 5.0d16 dmb

byte nameroottable [] = STR_root;

byte namestartuptable [] = STR_startup;

byte namesuspendtable [] = STR_suspend;

byte nameresumetable [] = STR_resume;

byte nameshutdowntable [] = STR_shutdown;

byte namesystembranch [] = STR_system;

byte namemenubartable [] = STR_menubars;

static byte namemacintoshtable [] = STR_macintosh;

static byte nameobjectmodeltable [] = STR_objectmodel;

static byte nametemptable [] = STR_temp;

byte nameenvironmenttable [] = STR_environment;

byte namecharsetstable [] = STR_charsets;



Handle rootvariable = nil;

hdlhashtable roottable = nil;

hdlhashtable systemtable = nil;

hdlhashtable internaltable = nil;

hdlhashtable efptable = nil;

hdlhashtable langtable = nil;

hdlhashtable runtimestacktable = nil;

hdlhashtable semaphoretable = nil;

hdlhashtable threadtable = nil;

hdlhashtable filewindowtable = nil;

hdlhashtable builtinstable = nil;

hdlhashtable pathstable = nil;

hdlhashtable verbstable = nil;

hdlhashtable iacgluetable = nil;

hdlhashtable iachandlertable = nil;

hdlhashtable resourcestable = nil;

hdlhashtable agentstable = nil;

/*
hdlhashtable usertable = nil;
*/

hdlhashtable menubartable = nil;

hdlhashtable objectmodeltable = nil;

hdlhashtable environmenttable = nil;

hdlhashtable charsetstable = nil;



boolean getsystemtablescript (short idscript, bigstring bsscript) {
	
	return (getstringlist (idsystemtablescripts, idscript, bsscript));
	} /*getsystemtablescript*/


static boolean checktable (hdlhashtable htable, bigstring bs, boolean flcreate, hdlhashtable *hsubtable) {
	
	/*
	locate the table named bs in the htable.  return in hsubtable the table you're 
	looking for.  if flcreate is true we create the table if it doesn't exist.
	
	return false if we couldn't find or create the table.
	
	3/4/91 dmb: if we create a new table, look for a packed hash resource 
	with a matching named, and try to unpack the values
	*/
	
	register hdlhashtable *ht = hsubtable;
	
	if (findnamedtable (htable, bs, ht)) /*no problem, it exists*/
		goto exit;
	
	*ht = nil;
	
	if (!flcreate)
		return (false);
	
	if (!tablenewsubtable (htable, bs, ht))
		return (false);
	
	#if MACVERSION && !defined (odbengine)
	
	Handle hpacked;
	hpacked = filegetresource (filegetapplicationrnum (), 'HASH', 0, bs);
	
	if (hpacked != nil) { /*try unpacking from resource*/
		
		DetachResource (hpacked);
		
        db_context context;
        db_context_init(&context);
		hashunpacktable_context (&context, hpacked, true, *ht); /*he always disposes of hpackedtable*/
		}
	
	#endif
	
	exit:
	
	#ifdef smartmemory
	
	(***ht).fllocked = true; /*currently only respected by purgetable code in langhash.c*/
	
	#endif
	
	return (true);
	} /*checktable*/


static boolean linksystemtable (hdlhashtable hsystem, bigstring bstable, hdlhashtable htable) {
	
	/*
	link htable into the system table using the given name
	*/

	Handle hdata = (Handle) (**htable).hashtablerefcon;
	
	if (!langsetexternalsymbol (hsystem, bstable, idtableprocessor, hdata))
		return (false);
	
	langexternaldontsave (hsystem, bstable);
	
	return (true);
	} /*linksystemtable*/


boolean linksystemtablestructure (hdlhashtable hroot) {
	
	/*
	have a look at the system table -- there's a table called compiler.
	
	it's assumed to be read-only, so it can be linked into every open cancoon
	file.  this routine links the variable that represents the compiler table
	into the system table of the root table passed in as a parameter.
	
	3/25/91 dmb: use checktable instead of findnamedtable to find the system 
	table so that one will be created if it's missing.  this allows us to 
	change the name of the system table without breaking old files.
	
	5.1 dmb: create the temp table (not shared between roots)
	
	5.1.4 dmb: do the environment table, break out linksystemtable code.
	
	the environment table is created in langstartup, as is the compiler table
	*/
	
	hdlhashtable hsystem, htemp;
	
	// make sure the system table exists
	if (!checktable (hroot, namesystembranch, true, &hsystem))
		return (false);
	
	// link in the compiler table, not part of saved structure
	if (!linksystemtable (hsystem, nameinternaltable, internaltable))
		return (false);
	
	// link in the environment table, not part of saved structure
	if (!linksystemtable (hsystem, nameenvironmenttable, environmenttable))
		return (false);
	
	// link in the charsets table, not part of saved structure
	if (!linksystemtable (hsystem, namecharsetstable, charsetstable))
		return (false);
	
	// create the temp table, not part of saved structure
	if (checktable (hsystem, nametemptable, true, &htemp))
		langexternaldontsave (hsystem, nametemptable);
	
	return (true);
	} /*linksystemtablestructure*/


boolean resolve_system_paths (hdlhashtable hroot) {

	/*
	2025-12-26 Codex: Eagerly resolve all address values in system.paths
	after linksystemtablestructure().

	This must be called AFTER linksystemtablestructure() links internaltable
	into system.compiler, so that stringtoaddress() can resolve paths like
	"system.compiler.lang" correctly.

	Background: Address values are saved to disk as strings only (never pointers).
	During unpacking, they're created as "unresolved" with htable=-1 marker.
	This function eagerly resolves system.paths entries so verb lookup works correctly.
	*/

	hdlhashtable hsystem, hpaths;
	hdlhashnode h;

	log_trace(LOG_COMP_LANG, "resolve_system_paths called with hroot=%p", (void*)hroot);

	// Find system table
	if (!findnamedtable (hroot, namesystembranch, &hsystem)) {
		log_trace(LOG_COMP_LANG, "No system table found, skipping resolution");
		return (true); // No system table, nothing to do
	}

	// Find system.paths table
	if (!findnamedtable (hsystem, namepathstable, &hpaths)) {
		log_trace(LOG_COMP_LANG, "No paths table found, skipping resolution");
		return (true); // No paths table, nothing to do
	}

	log_info(LOG_COMP_LANG, "Resolving system.paths addresses after linksystemtablestructure()");

	// Iterate through all entries in system.paths
	int entry_count = 0;
	int unresolved_count = 0;
	int resolved_count = 0;

	for (h = (**hpaths).hfirstsort; h != nil; h = (**h).sortedlink) {

		tyvaluerecord *val = &(**h).val;
		entry_count++;

		// Check if it's an unresolved address
		if (val->valuetype == addressvaluetype && (**h).flunresolvedaddress) {

			bigstring bspath;
			unresolved_count++;

			// Get the path from the address value
			if (!getaddresspath (*val, bspath)) {
				log_warn(LOG_COMP_LANG, "Failed to get address path for system.paths entry %d", entry_count);
				continue;
			}

			char cpath[512];
			copyptocstring(bspath, cpath);
			log_trace(LOG_COMP_LANG, "BEFORE RESOLUTION - system.paths entry: %s", cpath);

			// Resolve the unresolved address
			// Use langexpandtodotparams to resolve the path to an htable
			hdlhashtable htable_resolved = nil;
			bigstring bs_resolved;

			copystring(bspath, bs_resolved);

			pushhashtable(roottable);
			boolean fl = langexpandtodotparams(bs_resolved, &htable_resolved, bs_resolved);
			pophashtable();

			if (fl && htable_resolved != nil) {

				// Update the htable in the address handle
				hdlstring hstring = val->data.addressvalue;
				long ixtable = stringlength(bspath) + 1;

				// Write the resolved htable directly into the handle
				hdlhashtable *phtable = (hdlhashtable *)((*hstring) + ixtable);
				*phtable = htable_resolved;

				(**h).flunresolvedaddress = false;
				resolved_count++;

				log_debug(LOG_COMP_LANG, "Resolved system.paths entry: %s -> htable=%p", cpath, (void*)htable_resolved);
				} else {
				log_warn(LOG_COMP_LANG, "Failed to resolve system.paths entry: %s", cpath);
				}
			}
		}

	log_info(LOG_COMP_LANG, "Resolved %d of %d system.paths entries", resolved_count, unresolved_count);

	// Verify resolution by checking the flags again
	int still_unresolved = 0;
	for (h = (**hpaths).hfirstsort; h != nil; h = (**h).sortedlink) {
		if ((**h).flunresolvedaddress)
			still_unresolved++;
	}

	if (still_unresolved > 0) {
		log_warn(LOG_COMP_LANG, "After resolution, %d system.paths entries still unresolved", still_unresolved);
	}

	return (true);
	} /*resolve_system_paths*/


boolean headless_init_system_paths (hdlhashtable hroot) {

	/*
	2026-01-06 Codex: Populate system.paths with processor shortcuts.

	Problem: Bare verb names like "op", "target", "table" don't resolve because
	         system.paths table is empty.

	Solution: Iterate all processor tables in efptable (system.compiler.kernel.*)
	         and create address values pointing to each processor, adding them
	         to system.paths.

	Result: defined(op) returns true, and bare calls like "op.firstSummit()" work.

	This must be called AFTER linksystemtablestructure() links efptable into
	system.compiler, but BEFORE resolve_system_paths() resolves the addresses.
	*/

	hdlhashtable hsystem, hpaths, hinternal, hefptable;
	hdlhashnode h;
	int processor_count = 0;

	log_trace(LOG_COMP_LANG, "headless_init_system_paths called with hroot=%p", (void*)hroot);

	// Find system table
	if (!findnamedtable (hroot, namesystembranch, &hsystem)) {
		log_warn(LOG_COMP_LANG, "No system table found, cannot init system.paths");
		return (false);
	}

	// Find system.compiler (internaltable)
	if (!findnamedtable (hsystem, nameinternaltable, &hinternal)) {
		log_warn(LOG_COMP_LANG, "No system.compiler table found, cannot init system.paths");
		return (false);
	}

	// Find system.compiler.kernel (efptable)
	if (!findnamedtable (hinternal, nameefptable, &hefptable)) {
		log_warn(LOG_COMP_LANG, "No system.compiler.kernel table found, cannot init system.paths");
		return (false);
	}

	// Find or create system.paths
	if (!findnamedtable (hsystem, namepathstable, &hpaths)) {
		// Create system.paths if it doesn't exist
		if (!tablenewsubtable (hsystem, namepathstable, &hpaths)) {
			log_error(LOG_COMP_LANG, "Failed to create system.paths table");
			return (false);
		}
		log_debug(LOG_COMP_LANG, "Created system.paths table at %p", (void*)hpaths);
	}

	log_info(LOG_COMP_LANG, "Populating system.paths with processor shortcuts from efptable");

	// Iterate all processor tables in efptable (system.compiler.kernel.*)
	for (h = (**hefptable).hfirstsort; h != nil; h = (**h).sortedlink) {

		tyvaluerecord *val = &(**h).val;

		// Only process table entries
		if (val->valuetype != externalvaluetype)
			continue;

		// Get the processor name (e.g., "op", "target", "table")
		bigstring bs_processor_name;
		gethashkey(h, bs_processor_name);

		// Create address value pointing to this processor
		// Address values store the PARENT table handle and the CHILD name
		// So we pass hefptable (system.compiler.kernel) and the processor name ("op")
		// Later, when resolving, langgettableval will look up "op" in hefptable
		// Use setexemptaddressvalue to avoid tmpstack operations (runtime not initialized yet)
		tyvaluerecord addr_val;
		if (!setexemptaddressvalue(hefptable, bs_processor_name, &addr_val)) {
			char cname[256];
			copyptocstring(bs_processor_name, cname);
			log_warn(LOG_COMP_LANG, "Failed to create address value for processor: %s", cname);
			continue;
		}

		// Add to system.paths with processor short name (e.g., "op")
		if (!hashtableassign(hpaths, bs_processor_name, addr_val)) {
			char cname[256];
			copyptocstring(bs_processor_name, cname);
			log_warn(LOG_COMP_LANG, "Failed to assign processor to system.paths: %s", cname);
			continue;
		}

		processor_count++;

		char cname[256];
		copyptocstring(bs_processor_name, cname);
		log_debug(LOG_COMP_LANG, "Added system.paths.%s -> %s", cname, cname);
	}

	log_info(LOG_COMP_LANG, "Populated system.paths with %d processor shortcuts", processor_count);

	return (true);
	} /*headless_init_system_paths*/


boolean augment_database_tables_with_efp (hdlhashtable hroot) {

	/*
	2025-12-27 Codex: Augment database tables with EFP implementations to enable bare verb resolution.

	Problem: system.paths points to database tables (e.g., system.compiler.lang) which have no valueroutines.
	         EFP tables are created at system.compiler.["kernel"].* with valueroutines but aren't in system.paths.
	         Bare verb calls like new() fail because path lookup finds tables without callbacks.

	Solution: Merge EFP table entries into database tables:
	         - Copy valueroutine callback from EFP table to database table
	         - Copy all verb entries from EFP table to database table
	         - Mark copied entries as fldontsave (C pointers shouldn't persist)

	Result: Database tables get both persistence AND runtime callbacks, bare verbs work.
	*/

	hdlhashtable hsystem, hpaths, hinternal, hefptable;
	hdlhashnode h;
	int augmented_count = 0;

	log_trace(LOG_COMP_LANG, "augment_database_tables_with_efp ENTER with hroot=%p", (void*)hroot);

	// Find system table
	if (!findnamedtable (hroot, namesystembranch, &hsystem)) {
		log_warn(LOG_COMP_LANG, "No system table found, skipping augmentation");
		return (true); // No system table, nothing to augment
	}

	// Find system.paths table
	if (!findnamedtable (hsystem, namepathstable, &hpaths)) {
		log_warn(LOG_COMP_LANG, "No system.paths table found, skipping augmentation");
		return (true); // No paths table, nothing to augment
	}

	// Find system.compiler table
	if (!findnamedtable (hsystem, nameinternaltable, &hinternal)) {
		log_error(LOG_COMP_LANG, "No system.compiler table found, cannot augment");
		return (false);
	}

	// Find system.compiler.["kernel"] (efptable)
	if (!findnamedtable (hinternal, nameefptable, &hefptable)) {
		log_error(LOG_COMP_LANG, "No system.compiler.[\"kernel\"] table found, cannot augment");
		return (false);
	}

	log_debug(LOG_COMP_LANG, "Found efptable at %p, iterating system.paths for augmentation", (void*)hefptable);

	// Iterate all entries in system.paths
	for (h = (**hpaths).hfirstsort; h != nil; h = (**h).sortedlink) {
		tyvaluerecord *val;
		hdlhashtable htable_target;
		bigstring bs_path;
		bigstring bs_lastcomponent;

		val = &(**h).val;
		if (val->valuetype != addressvaluetype)
			continue; // Only process address values

		// Get the path string from the address value
		if (!getaddresspath (*val, bs_path)) {
			log_warn(LOG_COMP_LANG, "Failed to extract path from address value, skipping");
			continue;
		}

		// Resolve the address to get the database table
		if (!getaddressvalue (*val, &htable_target, bs_lastcomponent)) {
			log_warn(LOG_COMP_LANG, "Failed to resolve address '%.*s', skipping",
			         (int)bs_path[0], bs_path+1);
			continue;
		}

		// Extract just the last component from the path (e.g., "lang" from "system.compiler.lang")
		// bs_lastcomponent currently contains the full path, we need just the final segment
		bigstring bs_finalcomponent;
		short i, lastdot = 0;
		for (i = 1; i <= bs_lastcomponent[0]; i++) {
			if (bs_lastcomponent[i] == '.')
				lastdot = i;
		}
		if (lastdot > 0) {
			// Copy from after the last dot to the end
			short len = bs_lastcomponent[0] - lastdot;
			bs_finalcomponent[0] = len;
			for (i = 1; i <= len; i++) {
				bs_finalcomponent[i] = bs_lastcomponent[lastdot + i];
			}
		} else {
			// No dot found, use the whole string
			copystring(bs_lastcomponent, bs_finalcomponent);
		}

		if (bs_finalcomponent[0] == 0) {
			log_warn(LOG_COMP_LANG, "Empty last component from path '%.*s', skipping",
			         (int)bs_path[0], bs_path+1);
			continue;
		}
		// Look for matching EFP table in efptable (e.g., efptable["lang"])
		hdlhashtable hefp_processor;
		if (!findnamedtable (hefptable, bs_finalcomponent, &hefp_processor)) {
			log_trace(LOG_COMP_LANG, "No EFP processor found for '%.*s', skipping",
			          (int)bs_finalcomponent[0], bs_finalcomponent+1);
			continue; // No EFP for this processor, that's OK
		}

		log_debug(LOG_COMP_LANG, "Augmenting '%.*s' with EFP from system.compiler.[\"kernel\"].%.*s",
		          (int)bs_path[0], bs_path+1,
		          (int)bs_finalcomponent[0], bs_finalcomponent+1);

		// Copy valueroutine from EFP table to database table
		if ((**hefp_processor).valueroutine != nil) {
			(**htable_target).valueroutine = (**hefp_processor).valueroutine;
			log_debug(LOG_COMP_LANG, "Copied valueroutine %p to database table",
			          (void*)(**hefp_processor).valueroutine);
		}

		// Copy all entries from EFP table to database table
		hdlhashnode hefp_node;
		int entries_copied = 0;
		for (hefp_node = (**hefp_processor).hfirstsort; hefp_node != nil; hefp_node = (**hefp_node).sortedlink) {
			bigstring bs_entryname;
			tyvaluerecord entry_val;

			// Get entry name
			gethashkey (hefp_node, bs_entryname);

			// Check if entry already exists in database table
			hdlhashnode existing_node;
			if (hashtablelookupnode (htable_target, bs_entryname, &existing_node)) {
				log_trace(LOG_COMP_LANG, "Entry '%.*s' already exists in database table, skipping",
				          (int)bs_entryname[0], bs_entryname+1);
				continue; // Don't overwrite existing entries
			}

			// Copy the value
			entry_val = (**hefp_node).val;

			// Insert into database table
			pushhashtable (htable_target);
			if (hashinsert (bs_entryname, entry_val)) {
				// Mark as fldontsave (C pointers shouldn't persist)
				hdlhashnode new_node;
				if (hashtablelookupnode (htable_target, bs_entryname, &new_node)) {
					(**new_node).fldontsave = true;
					entries_copied++;
					log_trace(LOG_COMP_LANG, "Copied entry '%.*s' (marked fldontsave)",
					          (int)bs_entryname[0], bs_entryname+1);
				}
			} else {
				log_warn(LOG_COMP_LANG, "Failed to insert entry '%.*s'",
				         (int)bs_entryname[0], bs_entryname+1);
			}
			pophashtable ();
		}

		log_info(LOG_COMP_LANG, "Augmented '%.*s' with %d entries from EFP table",
		         (int)bs_finalcomponent[0], bs_finalcomponent+1, entries_copied);
		augmented_count++;
	}

	log_info(LOG_COMP_LANG, "Database table augmentation complete: %d tables augmented", augmented_count);

	return (true);
	} /*augment_database_tables_with_efp*/


boolean unlinksystemtablestructure (void) {

	/*
	try extracting system table from root before disposal.
	*/
	
	pushhashtable (systemtable);
	
	hashdelete (nameinternaltable, false, false);
	
	pophashtable ();
	
	return (true);
	} /*unlinksystemtablestructure*/


boolean tablenewtable (hdltablevariable *hvariable, hdlhashtable *htable) {
	
	/*
	create a new hashtable variable with an empty hashtable linked into it.
	*/
	
	register hdltablevariable hv;
	register hdlhashtable ht;
	
	if (!newtablevariable (true, 0L, hvariable, false))
		return (false);
	
	hv = *hvariable; /*copy into register*/
	
	if (!newhashtable (htable)) {
		
		disposehandle ((Handle) hv);
		
		return (false);
		}
	
	ht = *htable; /*copy into register*/
	
	(**ht).timecreated = (**ht).timelastsave = timenow64 (); // 5.0a23 dmb
	
	/*leave the hashtableformats record unallocated, handle == nil*/
	
	(**hv).variabledata = (long) ht; /*link the table into the variable rec*/
	
	(**ht).hashtablerefcon = (long) hv; /*the pointing is mutual*/
	
	(**ht).fldirty = true; /*it's never been saved*/


	/* Phase 4A: Initialize table context for version tracking */
	(**ht).context = NULL; /* Defensive: zero before init */

	if (!table_context_init(&(**ht).context)) {
		/* Safe to dispose: disposehandle handles partially-initialized tables correctly
		   (hashtable structure was fully allocated via newhashtable above) */
		disposehandle ((Handle) hv);
		disposehandle ((Handle) ht);
		return (false);
	}
	return (true);
	} /*tablenewtable*/


boolean tablenewsubtable (hdlhashtable htable, bigstring bsname, hdlhashtable *hnewtable) {
	
	/*
	create a new hashtable in the table indicated by htable, and return a
	handle to the hashtable record.
	
	the caller can load it full of stuff after we allocate it.  we basically
	just rely on the langhash.c to make the new table, and we link it into the 
	global table.
	
	1/23/91: don't set the dontsave bit anymore.  callers that want it set 
	should be using tablenewsystemtable.
	*/
	
	hdltablevariable hvariable;
	
	if (!tablenewtable (&hvariable, hnewtable))
		return (false);
	
	if (!langsetexternalsymbol (htable, bsname, idtableprocessor, (Handle) hvariable)) {
		
		tableverbdispose ((hdlexternalvariable) hvariable, false);
		
		return (false);
		}
	
	
	(***hnewtable).parenthashtable = htable; /*retain parental link*/
	
	
	return (true);
	} /*tablenewsubtable*/


boolean tablenewsystemtable (hdlhashtable htable, bigstring bs, hdlhashtable *hnewtable) {
	
	register hdlhashtable ht;
	register hdltablevariable hv;
	
	if (!tablenewsubtable (htable, bs, hnewtable))
		return (false);
	
	langexternaldontsave (htable, bs); /*set the don't-save bit in its hashnode*/
	
	ht = *hnewtable; /*copy into register*/
	
	hv = (hdltablevariable) (**ht).hashtablerefcon;
	
	(**hv).flsystemtable = true;
	
	return (true);
	} /*tablenewsystemtable*/


boolean tableloadsystemtable (dbaddress adr, Handle *hvariable, hdlhashtable *htable, boolean flcreate) {
	
	/*
	gets things started -- use this routine to load the initial hierarchic symbol
	table.
	
	create a new table variable, not in memory, with address equal to the indicated
	dbaddress.  then load it into memory and return a handle to the variable record.
	table.
	
	also return the handle to the hash table from the variable record.  we don't want
	to pollute the call with any knowledge of the format of a table variable record.
	
	6/11/90 DW: merge the contents of the root hashtable into the new table.  at this
	point it contains the handlers implemented in wpverbs.c, opverbs.c, fileverbs.c,
	etc.
	
	6/15/90 DW: if adr is 0, we just allocate an empty table and return it.
	
	10/5/90 dmb: no longer merge w/root table.  instead, call linksystemtablestructure
	
	1/21/91 dmb: create SystemLand table when starting with empty table.  also, 
	don't set roottable here.  leave that to caller
	*/
	
	register hdlexternalvariable hv;
	register hdlhashtable ht;
	hdlhashtable hsubtable;

	log_debug(LOG_COMP_TABLE, "tableloadsystemtable adr=0x%llx flcreate=%d",
	          (unsigned long long) adr, (int) flcreate);

	assert (sizeof (tyexternalvariable) == sizeof (tytablevariable));
	
	if (adr == nildbaddress) { /*start an empty table*/
		
		if (!tablenewtable ((hdltablevariable *) hvariable, htable)) /*this will be the root table*/
			return (false);
		
		hv = (hdlexternalvariable) *hvariable;
		
		if (flcreate && !tablenewsubtable (*htable, namesystembranch, &hsubtable)) {
			
			tableverbdispose (hv, true);
			
			return (false);
			}
		}
	else {
		
		if (!newtablevariable (false, adr, (hdltablevariable *) hvariable, false))
			return (false);
		
		hv = (hdlexternalvariable) *hvariable;
		
	if (!tableverbinmemory (NULL, hv, HNoNode)) {
		
		disposehandle ((Handle) hv);
		
		return (false);
		}
		}
	
	ht = (hdlhashtable) (**hv).variabledata;
	
	
	(**hv).id = idtableprocessor; /*so we can make a value out of this variable*/
	
	*htable = ht;
	
	return (true);
	} /*tableloadsystemtable*/


boolean tablesavesystemtable (Handle hvariable, dbaddress *adr) {
	
	/*
	saves out the root symbol table.  adr should be set to the address it was
	last saved at, we re-use the space if the new table will fit in it, otherwise
	adr returns with a new block, and the old one is released.
	
	dmb 10/2/90: don't rely on oldaddress; not set during Save As.  grab address 
	from handle instead.
	
	5.1.3 dmb: fancier error reporting
	*/
	
	register hdlexternalvariable hv = (hdlexternalvariable) hvariable;
	langerrormessagecallback savecallback;
	ptrvoid saverefcon;
	bigstring bspackerror;
	register boolean fl;
	Handle htmp;
	boolean fldummy;
	
	if (!newemptyhandle (&htmp)) 
		return (false);
	
	if (!flscriptrunning)
		langhookerrors ();
		
	tablepreflightsubsdirtyflag (hv); //6.2a15 AR

	langtraperrors (bspackerror, &savecallback, &saverefcon);

	log_debug(LOG_COMP_TABLE, "tablesavesystemtable enter mode64=%d",
	          db_format_mode_current().use_64bit_format ? 1 : 0);

	fl = tableverbpack (hv, &htmp, &fldummy); /*packs table, saves to db if neccessary, pushes address on htmp*/

	log_debug(LOG_COMP_TABLE, "tableverbpack returned %s", fl ? "true" : "false");
    if (fl && db_format_adapter_force_repack()) {
        /* Ensure legacy-derived addresses are normalized to BE64 for view storage.
         * Call the non-context version directly so the mode persists. */
        db_format_adapter_enable_wide_writes(NULL);
    }

	languntraperrors (savecallback, saverefcon, !fl);
	
	if (!flscriptrunning)
		langunhookerrors ();

	{
    boolean mode64 = db_format_mode_current().use_64bit_format;
#if defined(FRONTIER_HEADLESS)
        /* Save As to modern roots can leave the mode stack in a legacy state; prefer the destination DB type. */
        if (!mode64 && fldatabasesaveas) {
            hdldatabaserecord hdest = nil;
            if (dbgetdestinationdatabase(&hdest) && (hdest != nil) && !db_format_is_legacy_db(hdest))
                mode64 = true;
        }
#endif
    long adrsize = mode64 ? (long) sizeof(dbaddress) : (long) sizeof(uint32_t);
		long hsize = gethandlesize(htmp);
		long ix = hsize - adrsize;
		unsigned char adrbytes[sizeof(dbaddress)];

		log_debug(LOG_COMP_TABLE, "tablesavesystemtable trailer hsize=%ld adrsize=%ld ix=%ld",
		          hsize, adrsize, ix);

		if (ix < 0 || adrsize > (long) sizeof(adrbytes) || hsize < adrsize || !loadfromhandle(htmp, &ix, adrsize, adrbytes)) {
			fl = false;
		} else {
			if (mode64)
				*adr = (dbaddress) db_format_read_be64(adrbytes);
			else
				*adr = (dbaddress) db_format_read_be32(adrbytes);
			sethandlesize(htmp, hsize - adrsize); /* drop the address trailer */
		}

		log_debug(LOG_COMP_TABLE, "tablesavesystemtable adr=%llx adrsize=%ld mode64=%d",
		          fl ? (unsigned long long) *adr : 0ULL,
		          adrsize,
		          mode64 ? 1 : 0);
	}

#if defined(FRONTIER_HEADLESS)
    /* Keep the recorded address as written; normalization can corrupt modern Save As destinations. */
#endif

	disposehandle (htmp); /*we can get the address from the variable record, below*/
	
	if (!fl) {
		
		fllangerror = false;
		
		setstringcharacter (bspackerror, 0, getlower (getstringcharacter (bspackerror, 0)));
		
		poptrailingchars (bspackerror, '.');
		
		if (flscriptrunning)
			langparamerror (tablesavingerror, bspackerror);
		
		else {
			bigstring bs;

			getstringlist (langerrorlist, tablesavingerror, bs);

			parsedialogstring (bs, bspackerror, nil, nil, nil, bs);

			char cmsg[256];
			copyptocstring (bs, cmsg);
			log_error(LOG_COMP_TABLE, "tablesavesystemtable failed: %s", cmsg);

			shellerrormessage (bs);
			}
		}
	/*
	*adr = (**hv).oldaddress;
	*/
	
	return (fl);
	} /*tablesavesystemtable*/


boolean checktablestructure (boolean flcreate) {

	/*
	if these tables don't exist, we create them.  return true only if everything 
	is laid out as it's supposed to.
	
	2/28/91 dmb: even if we fail, try to locate as many tables as possible

	5.0a16 dmb: last version created system.menus. This version moves paths to system.

	5.0b15 dmb: never auto-create the old "resources" table (system.misc)
	*/
	
	register boolean fl;
	hdlhashtable menustable;
	
		hdlhashtable macintoshtable;
	
	fl = checktable (roottable, namesystembranch, flcreate, &systemtable);
	
	if (fl) {
		
		if (!checktable (systemtable, nameverbstable, flcreate, &verbstable))
			fl = false;
		
		if (!checktable (systemtable, nameagentstable, flcreate, &agentstable))
			fl = false;
		
		if (!checktable (systemtable, nameresourcestable, false, &resourcestable))
			fl = false;
		
		if (!checktable (systemtable, namepathstable, flcreate, &pathstable))
			fl = false;

		if (checktable (systemtable, STR_menus, false, &menustable))
			checktable (menustable, namemenubartable, false, &menubartable); /*don't auto-create*/
		
		
		if (checktable (systemtable, namemacintoshtable, false, &macintoshtable))
			checktable (macintoshtable, nameobjectmodeltable, false, &objectmodeltable);
		
		
		}
	
	if (verbstable != nil) {
		
		if (!checktable (verbstable, namebuiltinstable, flcreate, &builtinstable))
			fl = false;
		
		if (!checktable (verbstable, nameiacgluetable, flcreate, &iacgluetable))
			fl = false;
		
		if (!checktable (verbstable, nameiachandlertable, flcreate, &iachandlertable))
			fl = false;
		}
		
	//usertable = roottable; /*does this work?*/
	
	return (fl);
	} /*checktablestructure*/


boolean cleartablestructureglobals (void) {

	/*
	dmb 9/24/90: clear globals; root table is about to be disposed
	*/

	log_debug(LOG_COMP_TABLE, "cleartablestructureglobals");

	rootvariable = nil;
	
	roottable = nil;
	
	systemtable = nil;
	
	builtinstable = nil;
	
	pathstable = nil;
	
	verbstable = nil;
	
	iacgluetable = nil;
	
	iachandlertable = nil;
	
	resourcestable = nil;
	
	agentstable = nil;
	
	/*
	usertable = nil;
	*/
	
	menubartable = nil;
	
	
	objectmodeltable = nil;
	
	
	/*these are never disposed; they're shared among all files
	
	internaltable = nil;
	
	efptable = nil;
	
	langtable = nil;
	
	runtimestacktable = nil;
	
	threadtable = nil;
	
	filewindowtable = nil;
	*/
	
	return (true);
	} /*cleartablestructureglobals*/


boolean settablestructureglobals (Handle hvariable, boolean flcreatesubs) {
	
	register hdltablevariable hv = (hdltablevariable) hvariable;
	register hdlhashtable ht;
	
	if (hv == nil)
		return (false);
	
	ht = (hdlhashtable) (**hv).variabledata;
	
	if (ht == nil)
		return (false);

	cleartablestructureglobals ();
	
	rootvariable = (Handle) hv;
	
	roottable = ht;
	
	return (checktablestructure (flcreatesubs)); /*sets agentstable, builtinstable, etc.*/
	} /*settablestructureglobals*/
