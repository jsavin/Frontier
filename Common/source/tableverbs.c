
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

#include "frontier.h"
#include "standard.h"

#include "file.h"
#include "memory.h"
#include "resources.h"
#include "strings.h"
#include "shell.h"
#include "opinternal.h"
#include "oplist.h"
#include "langinternal.h"
#include "langsystem7.h"
#include "tablestructure.h"
#include "tableinternal.h"
#include "tableverbs.h"
#include "db_format.h"
#include "kernelverbdefs.h"
#include "headless_selection.h"
#include "logging.h"
// 2025-11-28 Codex: Use db_context wrappers when packing tables to avoid TLS globals.
// 2025-12-29 Phase 3: Add headless selection context for table navigation verbs




typedef enum tytabletoken { /*verbs that are processed by table.c*/
	
	movefunc,
	
	copyfunc,
	
	renamefunc,
	
	moveandrenamefunc,
	
	/*
	lockfunc,
	
	islockedfunc,
	*/
	
	assignfunc,
	
	validatefunc,
	
	sortbyfunc,
	
	getcursorfunc,
	
	getselectionfunc,
	
	gofunc,
	
	gotofunc,
	
	gotonamefunc,
	
	jettisonfunc,
	
	packtablefunc,
	
	emptytablefunc,
	
	getdisplaysettings,

	setdisplaysettings,

	sortorderfunc,

	countvisiblerowsfunc,

	cttableverbs
	} tytabletoken;



static boolean gettableparam (hdltreenode hfirst, short pnum, hdlhashtable *htable, bigstring bs, hdltablevariable *hv, hdlhashnode *hnode) {
	
	/*
	5.0.2b13 dmb: on failure, generate an error; don't set global tableverberrornum
	*/
	
	tyvaluerecord val;
	short berrornum = 0;
	bigstring bserror;
	
	*hv = nil; /*default*/
	
	if (!getvarparam (hfirst, pnum, htable, bs)) /*name of table*/
		return (false);
	
	if (!langsymbolreference (*htable, bs, &val, hnode))
		return (false);
	
	if (!gettablevariable (val, hv, &berrornum)) {
		
		getstringlist (tableerrorlist, berrornum, bserror);
		
		langerrormessage (bserror);
		
		return (false);
		}
	
	return (true);
	} /*gettableparam*/


boolean gettablevalue (hdltreenode hfirst, short pnum, hdlhashtable *htable) {

	hdltablevariable hv;
	bigstring bsname;
	hdlhashnode hnode;
	db_context ctx;

	if (!gettableparam (hfirst, pnum, htable, bsname, &hv, &hnode))
		return (false);

	db_context_init(&ctx);
	if (!tableverbinmemory (&ctx, (hdlexternalvariable) hv, hnode)) /*couldn't swap it into memory*/
		return (false);
	
	*htable = (hdlhashtable) (**hv).variabledata;
	
	return (true);
	} /*gettablevalue*/


static boolean tablevalidateverb (hdltreenode hparam1, tyvaluerecord *v) {
	
	/*
	11/14/90 DW: validating a table is now something you can do from a script.
	*/
	
	hdlhashtable htable;
	
	flnextparamislast = true;
	
	if (!gettablevalue (hparam1, 1, &htable))
		return (false);
		
	(*v).data.flvalue = tablevalidate (htable, false);
	
	return (true);
	} /*tablevalidateverb*/


static boolean tablemoveverb (hdltreenode hparam1, tyvaluerecord *v) {

	/*
	table.move (address, tableaddress): boolean; move the indicated table
	entry to the given table

	9/30/91 dmb: use hashassign, not hashinsert, so existing item is overwritten

	10/3/91 dmb: use new fllanghashassignprotect flag to override protection

	5.0a15 dmb: on success, return address of moved value

	5.1.4 dmb: generate errors if item doesn't exist

	Phase 4A: Records mutation for version tracking
	*/

	hdlhashtable ht1, ht2;
	bigstring bs;
	tyvaluerecord val;
	boolean fl;
	hdlhashnode hnode;

	if (!getvarparam (hparam1, 1, &ht1, bs))
		return (false);

	flnextparamislast = true;

	if (!gettablevalue (hparam1, 2, &ht2))
		return (false);

	pushhashtable (ht1);

	fl = hashlookup (bs, &val, &hnode);

	if (fl)
		hashdelete (bs, false, false); /*don't toss the value*/

	pophashtable ();

	if (!fl) {

		langparamerror (unknownidentifiererror, bs);

		return (false);
		}

	if (!hashtableassign (ht2, bs, val))
		return (false);

	if (!setaddressvalue (ht2, bs, v))
		return (false);

	/* Phase 4A: Record mutation - item moved from ht1 to ht2 */
	table_context_record_mutation((struct hdlhashtable *) ht1, table_mutation_delete);
	table_context_record_mutation((struct hdlhashtable *) ht2, table_mutation_insert);

	return (true);
	} /*tablemoveverb*/


static boolean tablecopyverb (hdltreenode hparam1, tyvaluerecord *v) {

	/*
	table.copy (address, tableaddress): boolean; copy the indicated table
	entry to the given table

	9/30/91 dmb: use hashassign, not hashinsert, so existing item is overwritten

	10/3/91 dmb: use new fllanghashassignprotect flag to override protection

	5.0a15 dmb: on success, return address of moved value

	5.1.4 dmb: generate errors if item doesn't exist

	Phase 4A: Records mutation for version tracking
	*/

	hdlhashtable ht1, ht2;
	bigstring bs;
	tyvaluerecord val;
	boolean fl;
	Handle hpacked;
	hdlhashnode hnode;

	if (!getvarparam (hparam1, 1, &ht1, bs))
		return (false);

	flnextparamislast = true;

	if (!gettablevalue (hparam1, 2, &ht2))
		return (false);

	fl = hashtablelookup (ht1, bs, &val, &hnode);

	if (!fl) {

		langparamerror (unknownidentifiererror, bs);

		return (false);
		}
		//return (true); /*not fatal error; false is returned to caller*/

	if (!langpackvalue (val, &hpacked, hnode)) /*error packing -- probably out of memory*/
		return (false);

	fl = langunpackvalue (hpacked, &val);

	disposehandle (hpacked);

	if (fl) {

		/*
		fllanghashassignprotect = false;
		*/

		fl = hashtableassign (ht2, bs, val);

		/*
		fllanghashassignprotect = true;
		*/

		if (!fl)
			disposevaluerecord (val, true);
		}

	if (fl)
		fl = setaddressvalue (ht2, bs, v);

	/* Phase 4A: Record mutation - entry copied to target table */
	if (fl)
		table_context_record_mutation((struct hdlhashtable *) ht2, table_mutation_copy);

	return (fl);
	} /*tablecopyverb*/


static boolean tablerenameverb (hdltreenode hparam1, tyvaluerecord *v) {

	/*
	table.rename (address, string): boolean; rename the indicated table
	entry to the given name

	12/28/91 dmb: resort the table after changing its name

	8/26/92 dmb: if the table is displayed in a window, use tableresort
	to ensure clean update

	5.0a15 dmb: on success, return address of renamed value

	5.1.4 dmb: disallow rename if new name is already in use; generate
	errors if item doesn't exist

	Phase 4A: Records mutation for version tracking
	*/

	hdlhashtable htable;
	bigstring bs, bsname;
	hdlhashnode hnode;

	if (!getvarparam (hparam1, 1, &htable, bs))
		return (false);

	flnextparamislast = true;

	if (!getstringvalue (hparam1, 2, bsname))
		return (false);

	if (!hashtablelookupnode (htable, bs, &hnode)) {

		langparamerror (unknownidentifiererror, bs);

		return (false);
		}

	if (!equalidentifiers (bs, bsname)) {

		if (hashtablesymbolexists (htable, bsname)) {

			lang2paramerror (badrenameerror, bs, bsname);

			return (false);
			}

		if (!hashsetnodekey (htable, hnode, bsname))
			return (false);
		}

	hashresort (htable, hnode);

	if (!setaddressvalue (htable, bsname, v))
		return (false);

	/* Phase 4A: Record mutation - key renamed */
	table_context_record_mutation((struct hdlhashtable *) htable, table_mutation_rename);

	return (true);
	} /*tablerenameverb*/


static boolean tablemoveandrenameverb (hdltreenode hparam1, tyvaluerecord *v) {

	/*
	5.0a15 dmb: on success, return address of moved value

	5.1.4 dmb: generate errors if item doesn't exist

	Phase 4A: Records mutation for version tracking
	*/

	hdlhashtable ht1, ht2;
	bigstring bs1, bs2;
	tyvaluerecord val;
	boolean fl;
	hdlhashnode hnode;

	if (!getvarparam (hparam1, 1, &ht1, bs1))
		return (false);

	flnextparamislast = true;

	if (!getvarparam (hparam1, 2, &ht2, bs2))
		return (false);

	pushhashtable (ht1);

	fl = hashlookup (bs1, &val, &hnode);

	if (fl)
		hashdelete (bs1, false, false); /*don't toss the value*/

	pophashtable ();

	if (!fl) {

		langparamerror (unknownidentifiererror, bs1);

		return (false);
		}

	if (!hashtableassign (ht2, bs2, val))
		return (false);

	if (!setaddressvalue (ht2, bs2, v))
		return (false);

	/* Phase 4A: Record mutation - item moved and renamed between tables */
	table_context_record_mutation((struct hdlhashtable *) ht1, table_mutation_delete);
	table_context_record_mutation((struct hdlhashtable *) ht2, table_mutation_moveandrename);

	return (true);
	} /*tablemoveandrenameverb*/


/*
static boolean tablefindverb (hdltreenode hparam1, tyvaluerecord *v) {
	
	/%
	table.find (adr, bs): boolean; search the (external) value at adr 
	for the string bs.  if adr is a table, do *not* search subordinate 
	tables
	%/
	
	hdlhashtable htable;
	bigstring bsname, bs;
	tyvaluerecord val;
	boolean flzoom;
	boolean flsave;
	
	if (!getvarparam (hparam1, 1, &htable, bsname))
		return (false);
	
	flnextparamislast = true;
	
	if (!getstringvalue (hparam1, 2, searchparams.bsfind))
		return (false);
	
	startnewsearch (false, false);
	
	pushhashtable (htable);
	
	if (hashlookup (bsname, &val)) {
		
		flsave = searchparams.flonelevel;
		
		searchparams.flonelevel = true;
		
		(*v).data.flvalue = langexternalsearch (val, &flzoom);
		
		searchparams.flonelevel = flsave;
		}
	
	pophashtable ();
	
	return (true);
	} /%tablefindverb%/
*/


static boolean tableassignverb (hdltreenode hparam1, tyvaluerecord *v) {

	/*
	10/3/91 dmb: use new fllanghashassignprotect flag to override protection

	Phase 4A: Records mutation for version tracking
	*/

	hdlhashtable htable;
	bigstring bsname;
	tyvaluerecord val;
	boolean fl;

	if (!getvarparam (hparam1, 1, &htable, bsname))
		return (false);

	flnextparamislast = true;

	if (!getparamvalue (hparam1, 2, &val))
		return (false);

	if (!copyvaluerecord (val, &val))
		return (false);

	fllanghashassignprotect = false;

	fl = hashtableassign (htable, bsname, val);

	fllanghashassignprotect = true;

	if (!fl)
		return (false);

	exemptfromtmpstack (&val);

	(*v).data.flvalue = true;

	/* Phase 4A: Record mutation for context tracking */
	table_context_record_mutation((struct hdlhashtable *) htable, table_mutation_modify);

	return (true);
	} /*tableassignverb*/



static boolean tablepacktableverb (hdltreenode hparam1, tyvaluerecord *v) {
	
	/*
	this is really for internal use only; packes a table and creates a 
	named HASH resource that can be loaded in tablestructure
	*/
	
	hdlhashtable htable;
	bigstring bs;
	register boolean fl;
	Handle hpacked;
	boolean fldummy;
	
	if (!gettablevalue (hparam1, 1, &htable))
		return (false);
	
	flnextparamislast = true;
	
	if (!getstringvalue (hparam1, 2, bs))
		return (false);
	
    db_context context;
    db_context_init(&context);
	fl = hashpacktable_context (&context, htable, true, &hpacked, &fldummy);
	
	if (fl) {
		
		lockhandle (hpacked);
		
		fl = filewriteresource (filegetapplicationrnum (), 'HASH', -1, bs, gethandlesize (hpacked), *hpacked);
		
		disposehandle (hpacked);
		}
	
	(*v).data.flvalue = fl;
	
	return (true);
	} /*tablepacktableverb*/


static boolean tableemptytableverb (hdltreenode hparam1, tyvaluerecord *v) {

	/*
	4/25/96 4.0b7 dmb: kernel implementation for speed

	Phase 4A: Records mutation for version tracking
	*/

	hdlhashtable htable;
	short ct;

	flnextparamislast = true;

	if (!gettablevalue (hparam1, 1, &htable))
		return (false);

	ct = emptyhashtable (htable, true);

	if (!setintvalue (ct, v))
		return (false);

	/* Phase 4A: Record mutation - table cleared */
	if (ct > 0)  /* Only record if something was actually deleted */
		table_context_record_mutation((struct hdlhashtable *) htable, table_mutation_emptytable);

	return (true);
	} /*tableemptytableverb*/


static boolean tablegetselvisit (hdlheadrecord hnode, ptrvoid refcon) {
	
	hdllistrecord hlist = (hdllistrecord) refcon;
	hdlhashtable ht;
	bigstring bs;
	tyvaluerecord val;
	
	if (!tablegetiteminfo (hnode, &ht, bs, nil, nil))
		return (false);
	
	if (!setaddressvalue (ht, bs, &val))
		return (false);
	
	if (!langpushlistval (hlist, nil, &val))
		return (false);
	
	disposevaluerecord (val, true); // don't let them accumulate
	
	return (true);
	} /*tablegetselvisit*/


static boolean tablegetselectionverb (hdltreenode hp1, tyvaluerecord *v) {
	
	hdllistrecord hlist;
	
	if (!langcheckparamcount (hp1, 0)) /*too many parameters were passed*/
		return (false);
	
	if (!opnewlist (&hlist, false))
		return (false);
	
	if (!opvisitmarked (down, &tablegetselvisit, (ptrvoid) hlist)) {
		
		opdisposelist (hlist);
		
		return (false);
		}
	
	return (setheapvalue ((Handle) hlist, listvaluetype, v));
	} /*tablegetselectionverb*/


static boolean tablegetdisplaysettingsverb (hdltreenode hp1, tyvaluerecord *v) {
	
	hdltableformats hf = tableformatsdata;
	hdlhashtable hsettings;
	tylinelayout layout;
	
	flnextparamislast = true;
	
	if (!gettablevalue (hp1, 1, &hsettings))
		return (false);
	
	layout = (**hf).linelayout;
	
	if (!layout.flinitted) // no layout assigned, get deault
		clayinitlinelayout (&layout);
	
	if (!claylayouttotable (&layout, hsettings))
		return (false);
	
	return (setbooleanvalue (true, v));
	} /*tablegetdisplaysettingsverb*/


static boolean tablesetdisplaysettingsverb (hdltreenode hp1, tyvaluerecord *v) {
	
	hdlhashtable hsettings;
	tylinelayout layout;
	
	flnextparamislast = true;
	
	if (!gettablevalue (hp1, 1, &hsettings))
		return (false);
	
	if (!claytabletolayout (hsettings, &layout))
		return (false);
	
	claysetlinelayout (tableformatswindowinfo, &layout);

	return (setbooleanvalue (true, v));
	} /*tablesetdisplaysettingsverb*/


/*
 * Phase 3: Headless Table Verb Helpers
 *
 * These functions implement table navigation verbs for headless mode using
 * thread-local selection context from Phase 1.
 */

/**
 * table_get_target_hashtable - Get target table for headless operation
 *
 * @param htable_out - Output: target table handle
 * @return true if target found, false otherwise
 *
 * Tries multiple strategies:
 * 1. Check thread-local selection context for current_table
 * 2. Check lang.getTarget() for explicitly set target (headless primary method)
 * 3. If windowed mode available, use window lookup
 * 4. Error if no target can be determined
 */
static boolean table_get_target_hashtable(hdlhashtable *htable_out) {
	table_selection_context_t *ctx = NULL;
	bigstring bsname;

	/* Try thread-local context first */
	ctx = table_selection_acquire();
	if (ctx != NULL && ctx->current_table != NULL) {
		*htable_out = ctx->current_table;
		table_selection_release(ctx);
		return true;
	}

	if (ctx != NULL) {
		table_selection_release(ctx);
	}

	/* Try lang.getTarget() - primary method for headless mode */
	if (langgettarget(htable_out, bsname)) {
		if (*htable_out != NULL) {
			return true;
		}
	}

	/* Fall back to window lookup if display enabled */
	if (opdisplayenabled()) {
		WindowPtr w;
		if (langfindtargetwindow(idtableprocessor, &w)) {
			/* Extract table from window */
			*htable_out = tablegetlinkedhashtable();
			return (*htable_out != NULL);
		}
	}

	/* No target found */
	return false;
} /*table_get_target_hashtable*/


/**
 * table_getcursor_headless - Get cursor position in headless mode
 *
 * @param htable - Target table
 * @param v - Output value (address or empty string)
 * @return true if successful
 */
static boolean table_getcursor_headless(hdlhashtable htable, tyvaluerecord *v) {
	table_selection_context_t *ctx = NULL;
	bigstring key;

	/* Acquire selection context */
	ctx = table_selection_acquire();
	if (ctx == NULL) {
		return setstringvalue(zerostring, v);
	}

	/* Get cursor key */
	if (!table_selection_get_cursor(ctx, key) || key[0] == 0) {
		/* No cursor set */
		table_selection_release(ctx);
		return setstringvalue(zerostring, v);
	}

	/* Validate cursor still exists */
	hdlhashnode hnode;
	if (!hashtablelookupnode(htable, key, &hnode)) {
		/* Cursor invalid (entry deleted) */
		table_selection_release(ctx);
		return setstringvalue(zerostring, v);
	}

	/* Return key name for in-memory tables, or address for database-backed tables */
	boolean fl;

	if ((**htable).fllocaltable) {
		/* In-memory table - return key name as string */
		fl = setstringvalue(key, v);
	} else {
		/* Database-backed table - return address */
		fl = setaddressvalue(htable, key, v);
	}

	table_selection_release(ctx);
	return fl;
} /*table_getcursor_headless*/


/**
 * table_goto_headless - Move to row N in headless mode
 *
 * @param htable - Target table
 * @param row - 1-based row number
 * @param v - Output value (address of target row)
 * @return true if successful
 */
static boolean table_goto_headless(hdlhashtable htable, long row, tyvaluerecord *v) {
	table_selection_context_t *ctx = NULL;
	hdlhashnode target_node = NULL;
	hdlhashtable target_table = NULL;
	bigstring key;

	/* Validate parameters */
	if (htable == NULL) {
		langerrormessage(BIGSTRING("\x10" "No table given"));
		return false;
	}

	if (row < 1) {
		langerrormessage(BIGSTRING("\x1E" "Row number must be 1 or greater"));
		return false;
	}

	/* Acquire selection context */
	ctx = table_selection_acquire();
	if (ctx == NULL) {
		langerrormessage(BIGSTRING("\x20" "Can't get selection context"));
		return false;
	}

	/* Update context table reference */
	ctx->current_table = htable;

	/* Find node at row N (accounting for expansion state) */
	if (!table_selection_get_node_at_row(ctx, htable, row,
	                                     &target_node, &target_table)) {
		table_selection_release(ctx);
		langerrormessage(BIGSTRING("\x18" "Row number out of range"));
		return false;
	}

	/* Get key from node */
	gethashkey(target_node, key);

	/* Update cursor in context */
	copystring(key, ctx->cursor_key);
	ctx->cursor_node = target_node;
	ctx->cursor_flat_index = row;

	/* Clear multi-selection (goto is single-item navigation) */
	table_selection_clear(ctx);

	/* Create address value for return */
	boolean fl = setaddressvalue(target_table, key, v);

	table_selection_release(ctx);
	return fl;
} /*table_goto_headless*/


/**
 * table_gotoname_headless - Move to named entry in headless mode
 *
 * @param htable - Target table
 * @param key - Key name to find
 * @param v - Output value (address of entry)
 * @return true if successful
 */
static boolean table_gotoname_headless(hdlhashtable htable, const bigstring key, tyvaluerecord *v) {
	table_selection_context_t *ctx = NULL;
	hdlhashnode hnode;

	/* Validate parameters */
	if (htable == NULL) {
		langerrormessage(BIGSTRING("\x10" "No table given"));
		return false;
	}

	if (key[0] == 0) {
		langerrormessage(BIGSTRING("\x10" "Empty key given"));
		return false;
	}

	/* Look up key in table */
	if (!hashtablelookupnode(htable, key, &hnode)) {
		langerrormessage(BIGSTRING("\x0E" "Key not found"));
		return false;
	}

	/* Acquire selection context */
	ctx = table_selection_acquire();
	if (ctx == NULL) {
		langerrormessage(BIGSTRING("\x20" "Can't get selection context"));
		return false;
	}

	/* Update context */
	ctx->current_table = htable;
	copystring(key, ctx->cursor_key);
	ctx->cursor_node = hnode;

	/* Calculate flat index (for consistency) */
	ctx->cursor_flat_index = table_selection_get_row_for_key(ctx, htable, key);

	/* Clear multi-selection */
	table_selection_clear(ctx);

	/* Return address */
	boolean fl = setaddressvalue(htable, key, v);

	table_selection_release(ctx);
	return fl;
} /*table_gotoname_headless*/


/**
 * table_go_headless - Relative cursor movement in headless mode
 *
 * @param htable - Target table
 * @param dir - Direction (up/down)
 * @param count - Number of rows to move
 * @param v - Output value (address of new cursor position)
 * @return true if successful
 *
 * Note: Clamps to first/last row instead of erroring (matches legacy behavior)
 */
static boolean table_go_headless(hdlhashtable htable, tydirection dir, long count, tyvaluerecord *v) {
	table_selection_context_t *ctx = NULL;
	long current_row = 0;
	long target_row = 0;
	long total_rows = 0;

	/* Validate parameters */
	if (htable == NULL) {
		langerrormessage(BIGSTRING("\x10" "No table given"));
		return false;
	}

	/* Validate direction (only up/down supported headlessly) */
	if (dir == left || dir == right) {
		langerrormessage(BIGSTRING("\x2C" "Left/right not supported in headless mode"));
		return false;
	}

	/* Normalize flat directions */
	if (dir == flatup) dir = up;
	if (dir == flatdown) dir = down;

	if (dir != up && dir != down) {
		langerrormessage(BIGSTRING("\x18" "Invalid direction"));
		return false;
	}

	if (count < 0) {
		langerrormessage(BIGSTRING("\x1C" "Count must be non-negative"));
		return false;
	}

	if (count == 0) {
		/* No movement, return current cursor */
		return table_getcursor_headless(htable, v);
	}

	/* Acquire selection context */
	ctx = table_selection_acquire();
	if (ctx == NULL) {
		langerrormessage(BIGSTRING("\x20" "Can't get selection context"));
		return false;
	}

	ctx->current_table = htable;

	/* Get current cursor position */
	if (ctx->cursor_key[0] == 0) {
		/* No cursor set, default to row 1 */
		current_row = 1;
	} else {
		/* Find current row number */
		current_row = table_selection_get_row_for_key(ctx, htable, ctx->cursor_key);
		if (current_row == 0) {
			/* Cursor invalid (entry deleted), reset to row 1 */
			current_row = 1;
		}
	}

	/* Calculate total visible rows */
	if (!table_selection_count_visible_rows(ctx, htable, &total_rows)) {
		table_selection_release(ctx);
		langerrormessage(BIGSTRING("\x1C" "Can't count table rows"));
		return false;
	}

	if (total_rows == 0) {
		table_selection_release(ctx);
		langerrormessage(BIGSTRING("\x10" "Table is empty"));
		return false;
	}

	/* Calculate target row */
	if (dir == down) {
		target_row = current_row + count;
	} else {  /* up */
		target_row = current_row - count;
	}

	/* Clamp to bounds (matches legacy windowed behavior) */
	if (target_row < 1) {
		target_row = 1;
	}

	if (target_row > total_rows) {
		target_row = total_rows;
	}

	/* Release context before calling table_goto (which re-acquires) */
	table_selection_release(ctx);

	/* Move to target row */
	return table_goto_headless(htable, target_row, v);
} /*table_go_headless*/


/**
 * table_getselection_headless - Get selection in headless mode
 *
 * @param htable - Target table
 * @param v - Output value (list of addresses)
 * @return true if successful
 *
 * Returns list of selected items:
 * 1. If multi-selection exists → list of all selected addresses
 * 2. If cursor set (no multi-selection) → single-item list with cursor address
 * 3. If no selection and no cursor → empty list
 */
static boolean table_getselection_headless(hdlhashtable htable, tyvaluerecord *v) {
	table_selection_context_t *ctx = NULL;
	hdllistrecord hlist = NULL;
	boolean fl = false;

	/* Acquire selection context */
	ctx = table_selection_acquire();
	if (ctx == NULL) {
		langerrormessage(BIGSTRING("\x20" "Can't get selection context"));
		return false;
	}

	/* Update current table from context */
	if (ctx->current_table == NULL) {
		ctx->current_table = htable;
	}

	/* Create result list */
	if (!opnewlist(&hlist, false)) {
		table_selection_release(ctx);
		return false;
	}

	/* Case 1: Multi-selection exists */
	if (ctx->ct_selected > 0) {
		long i;

		for (i = 0; i < ctx->ct_selected; i++) {
			bigstring key;
			hdlhashnode hnode;
			tyvaluerecord addrval;

			/* Get key from selection list */
			if (!table_selection_get_nth_selected(ctx, i, key)) {
				opdisposelist(hlist);
				table_selection_release(ctx);
				return false;
			}

			/* Validate key still exists */
			if (hashtablelookupnode(htable, key, &hnode)) {
				/* Add address to result list */
				if (!setaddressvalue(htable, key, &addrval)) {
					opdisposelist(hlist);
					table_selection_release(ctx);
					return false;
				}

				if (!langpushlistval(hlist, nil, &addrval)) {
					opdisposelist(hlist);
					table_selection_release(ctx);
					return false;
				}
			}
		}

		fl = true;
		goto done;
	}

	/* Case 2: Cursor is set */
	if (ctx->cursor_key[0] > 0) {
		hdlhashnode hnode;
		tyvaluerecord addrval;

		/* Validate cursor still exists */
		if (hashtablelookupnode(htable, ctx->cursor_key, &hnode)) {
			if (!setaddressvalue(htable, ctx->cursor_key, &addrval)) {
				opdisposelist(hlist);
				table_selection_release(ctx);
				return false;
			}

			if (!langpushlistval(hlist, nil, &addrval)) {
				opdisposelist(hlist);
				table_selection_release(ctx);
				return false;
			}

			fl = true;
			goto done;
		}
	}

	/* Case 3: No selection, no cursor → return empty list */
	fl = true;

done:
	table_selection_release(ctx);

	if (fl) {
		return setheapvalue((Handle)hlist, listvaluetype, v);
	} else {
		opdisposelist(hlist);
		return false;
	}
} /*table_getselection_headless*/


boolean tablefunctionvalue (short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror) {

	/*
	bridges table.c with the language.  the name of the verb is bs, its first parameter
	is hparam1, and we return a value in vreturned.

	we use a limited number of support routines from lang.c to get parameters and
	to return values.

	return false only if the error is serious enough to halt the running of the script
	that called us, otherwise error values are returned through the valuerecord, which
	is available to the script.

	if we return false, we try to provide a descriptive error message in the
	returned string bserror.

	11/14/91 dmb: getcursorfunc returns address, not full path

	10/3/92 dmb: commented out setcolwidthfunc. (this verb still isn't "offical")

	4/2/93 dmb: added jettisonfunc

	6/1/93 dmb: when vreturned is nil, return whether or not verb token must
	be run in the Frontier process

	5.1.5b11 dmb: fixed gotofunc silent failure
	*/

	register tyvaluerecord *v = vreturned;
	register boolean fl = false;
	WindowPtr targetwindow;

	log_debug(LOG_COMP_TABLE, "tablefunctionvalue: ENTRY token=%d hparam1=%p vreturned=%p bserror=%p",
	          token, (void*)hparam1, (void*)vreturned, (void*)bserror);
	
	if (v == nil) { /*need Frontier process?*/
		
		switch (token) {

			case sortbyfunc:
			case getcursorfunc:
			case getselectionfunc:
			case gotofunc:
			case gotonamefunc:
			case gofunc:
			case countvisiblerowsfunc:
			case getdisplaysettings:
			case setdisplaysettings:
				return (true);

			default:
				return (false);
			}
		}
	
	setbooleanvalue (false, v); /*by default, table functions return false*/
	
	switch (token) { /*these verbs don't require an open table window*/
		
		/*
		case findfunc:
			if (!tablefindverb (hparam1, v))
				goto error;
			
			return (true);
		*/
		
		case validatefunc:
			return (tablevalidateverb (hparam1, v));
		
		case movefunc:
			return (tablemoveverb (hparam1, v));
		
		case copyfunc:
			return (tablecopyverb (hparam1, v));
		
		case renamefunc:
			return (tablerenameverb (hparam1, v));
		
		case moveandrenamefunc:
			return (tablemoveandrenameverb (hparam1, v));
		
		/*
		case lockfunc:
		case islockedfunc:
		*/
		
		case assignfunc:
			return (tableassignverb (hparam1, v));
			
		case packtablefunc:
			return (tablepacktableverb (hparam1, v));

		case emptytablefunc:
			return (tableemptytableverb (hparam1, v));
		
		case jettisonfunc: { /*toss an object w/out forcing it into memory. for database recovery.*/
			hdlhashtable htable;
			bigstring bs;

			if (!getvarparam (hparam1, 1, &htable, bs)) /*name of table*/
				return (false);

			pushhashtable (htable);

			(*v).data.flvalue = hashdelete (bs, true, false);

			pophashtable ();

			return (true);
			}

		/* Phase 3 headless table navigation verbs */
		case getcursorfunc: {
			hdlhashtable htable;

			if (!langcheckparamcount (hparam1, 0)) /*too many parameters were passed*/
				return (false);

			/* Get target table */
			if (!table_get_target_hashtable(&htable)) {
				setstringvalue(zerostring, v);
				return (true);
			}

			/* Dispatch based on mode */
			if (opdisplayenabled()) {
				/* Windowed mode */
				bigstring bs;
				tyvaluerecord val;
				hdlhashnode hhashnode;

				if (!tablegetcursorinfo(&htable, bs, &val, &hhashnode))
					setstringvalue(zerostring, v);
				else
					setaddressvalue(htable, bs, v);
			} else {
				/* Headless mode */
				table_getcursor_headless(htable, v);
			}

			return (true);
			}

		case getselectionfunc: {
			hdlhashtable htable;

			/* Get target table */
			if (!table_get_target_hashtable(&htable)) {
				langerrormessage(BIGSTRING("\x18" "No table is current"));
				return (false);
			}

			/* Dispatch based on mode */
			if (opdisplayenabled()) {
				/* Windowed mode */
				return tablegetselectionverb(hparam1, v);
			} else {
				/* Headless mode */
				return table_getselection_headless(htable, v);
			}
			}

		case gotofunc: {
			long row;
			hdlhashtable htable;

			log_debug(LOG_COMP_TABLE, "tablefunctionvalue: gotofunc case reached");

			flnextparamislast = true;

			if (!getlongvalue(hparam1, 1, &row)) {
				log_error(LOG_COMP_TABLE, "tablefunctionvalue: gotofunc - getlongvalue failed");
				return (false);
			}

			log_debug(LOG_COMP_TABLE, "tablefunctionvalue: gotofunc - row=%ld", row);

			/* Get target table */
			if (!table_get_target_hashtable(&htable)) {
				log_error(LOG_COMP_TABLE, "tablefunctionvalue: gotofunc - no target table, returning error");
				langerrormessage(BIGSTRING("\x18" "No table is current"));
				return (false);
			}

			log_debug(LOG_COMP_TABLE, "tablefunctionvalue: gotofunc - got target table %p", (void*)htable);

			/* Dispatch based on mode */
			if (opdisplayenabled()) {
				/* Windowed mode - use existing implementation */
				hdlheadrecord hsummit;
				if (opnthsummit(row, &hsummit)) {
					opclearallmarks();
					opmoveto(hsummit);
					(*v).data.flvalue = true;
				}
			} else {
				/* Headless mode - use selection context */
				log_debug(LOG_COMP_TABLE, "tablefunctionvalue: gotofunc - calling table_goto_headless");
				table_goto_headless(htable, row, v);
			}

			log_debug(LOG_COMP_TABLE, "tablefunctionvalue: gotofunc - returning true");
			return (true);
			}

		case gotonamefunc: {
			hdlhashtable htable;
			bigstring bs;

			flnextparamislast = true;

			if (!getstringvalue(hparam1, 1, bs))
				return (false);

			/* Get target table */
			if (!table_get_target_hashtable(&htable)) {
				langerrormessage(BIGSTRING("\x18" "No table is current"));
				return (false);
			}

			/* Dispatch based on mode */
			if (opdisplayenabled()) {
				/* Windowed mode - use existing implementation */
				(*v).data.flvalue = tablemovetoname(htable, bs);
			} else {
				/* Headless mode */
				table_gotoname_headless(htable, bs, v);
			}

			return (true);
			}

		case gofunc: {
			tydirection dir;
			long count;
			hdlhashtable htable;

			if (!getdirectionvalue(hparam1, 1, &dir))
				return (false);

			flnextparamislast = true;

			if (!getlongvalue(hparam1, 2, &count))
				return (false);

			/* Get target table */
			if (!table_get_target_hashtable(&htable)) {
				langerrormessage(BIGSTRING("\x18" "No table is current"));
				return (false);
			}

			/* Dispatch based on mode */
			if (opdisplayenabled()) {
				/* Windowed mode */
				opsettextmode(false);

				if (dir == down)
					dir = flatdown;
				if (dir == up)
					dir = flatup;

				(*v).data.flvalue = opmotionkey(dir, count, false);
			} else {
				/* Headless mode */
				table_go_headless(htable, dir, count, v);
			}

			return (true);
			}

		case countvisiblerowsfunc: {
			hdlhashtable htable;
			long count;
			table_selection_context_t *ctx = NULL;

			if (!langcheckparamcount(hparam1, 0))
				return (false);

			/* Get target table */
			if (!table_get_target_hashtable(&htable)) {
				langerrormessage(BIGSTRING("\x18" "No table is current"));
				return (false);
			}

			/* Acquire selection context for expansion state */
			ctx = table_selection_acquire();
			if (ctx == NULL) {
				langerrormessage(BIGSTRING("\x20" "Can't get selection context"));
				return (false);
			}

			ctx->current_table = htable;

			/* Count visible rows (respects expansion state) */
			if (!table_selection_count_visible_rows(ctx, htable, &count)) {
				table_selection_release(ctx);
				langerrormessage(BIGSTRING("\x1C" "Can't count table rows"));
				return (false);
			}

			table_selection_release(ctx);

			setlongvalue(count, v);

			return (true);
			}

		case sortbyfunc: {
			hdlhashtable htable;
			bigstring bssort, bstitle;
			short ixcol;

			flnextparamislast = true;

			if (!getstringvalue(hparam1, 1, bssort))
				return (false);

			/* Get target table */
			if (!table_get_target_hashtable(&htable)) {
				langerrormessage(BIGSTRING("\x18" "No table is current"));
				return (false);
			}

			/* Parse column name (case-insensitive) */
			alllower(bssort);

			for (ixcol = namecolumn; ixcol <= kindcolumn; ++ixcol) {
				tablegettitlestring(ixcol, bstitle);
				alllower(bstitle);

				if (equalstrings(bssort, bstitle)) {
					/* Found matching column - set sort order */

					/* Dispatch based on mode */
					if (opdisplayenabled()) {
						/* Windowed mode - use existing implementation */
						bigstring bs;
						tablegetcursorinfo(&htable, bs, nil, nil);
						(*v).data.flvalue = tablesetsortorder(htable, ixcol);
					} else {
						/* Headless mode - set sort and resort (hold context across operation) */
						boolean result = false;
						table_selection_context_t *ctx = table_selection_acquire();

						if (ctx == NULL) {
							langerrormessage(BIGSTRING("\x20" "Failed to acquire table context"));
							goto cleanup;
						}

						bigstring cursor_key;
						boolean had_cursor = (ctx->cursor_key[0] > 0);

						setemptystring(cursor_key);  /* Initialize to empty string */

						if (had_cursor) {
							copystring(ctx->cursor_key, cursor_key);
						}

						/* Set sort order and resort table */
						(**htable).sortorder = ixcol;
						hashresort(htable, nil);

						/* Restore cursor and recalculate flat index */
						if (had_cursor) {
							ctx->current_table = htable;
							copystring(cursor_key, ctx->cursor_key);
							ctx->cursor_flat_index = table_selection_get_row_for_key(ctx, htable, cursor_key);
						}

						result = true;

					cleanup:
						if (ctx != NULL)
							table_selection_release(ctx);

						(*v).data.flvalue = result;
					}

					return (true);
				}
			}

			/* Invalid column name - return error */
			langerrormessage(BIGSTRING("\x1E" "Invalid sort column name"));
			return (false);
			}

		case sortorderfunc: {
			hdlhashtable htable;
			bigstring bs;
			short ixcol;

			if (!langcheckparamcount(hparam1, 0))
				return (false);

			/* Get target table */
			if (!table_get_target_hashtable(&htable)) {
				langerrormessage(BIGSTRING("\x18" "No table is current"));
				return (false);
			}

			/* Get sort order from hashtable (works in both modes) */
			tablegetsortorder(htable, &ixcol);

			/* Validate column index */
			if (ixcol < namecolumn || ixcol > kindcolumn) {
				log_error(LOG_COMP_TABLE, "Invalid sort order: %d (valid range: %d-%d)",
				          ixcol, namecolumn, kindcolumn);
				langerrormessage(BIGSTRING("\x1A" "Invalid sort order state"));
				return (false);
			}

			/* Map column index to name */
			tablegettitlestring(ixcol, bs);

			return setstringvalue(bs, v);
			}

		case getdisplaysettings: {
			/* Dispatch based on mode */
			if (opdisplayenabled()) {
				/* Windowed mode - get actual display settings */
				return tablegetdisplaysettingsverb(hparam1, v);
			} else {
				/* Headless mode - no display settings available */
				return setbooleanvalue(false, v);
			}
			}

		case setdisplaysettings: {
			/* Dispatch based on mode */
			if (opdisplayenabled()) {
				/* Windowed mode - set actual display settings */
				return tablesetdisplaysettingsverb(hparam1, v);
			} else {
				/* Headless mode - accept but ignore settings */
				hdlhashtable hsettings;

				flnextparamislast = true;

				if (!gettablevalue(hparam1, 1, &hsettings))
					return (false);

				/* Silently accept the settings (no-op in headless) */
				return setbooleanvalue(true, v);
			}
			}
		} /*switch*/
	
	/*all other verbs require a table window in front*/
	
	if (!langfindtargetwindow (idtableprocessor, &targetwindow)) {
		
		getstringlist (tableerrorlist, notableerror, bserror);
		
		return (false);
		}
	
	shellpushglobals (targetwindow); /*following verbs assume that an table is pushed*/
	
	(*shellglobals.gettargetdataroutine) (idtableprocessor); /*set table globals*/
	
	switch (token) {

		/*
		case setcolwidthfunc: {
			short colnum, colwidth;

			if (!getintvalue (hparam1, 1, &colnum))
				break;

			flnextparamislast = true;

			if (!getintvalue (hparam1, 2, &colwidth))
				break;

			(*v).data.flvalue = (*(**tableformatsdata).adjustcolwidthroutine) (colnum - 1, colwidth);

			tablesmashdisplay ();

			fl = true;

			break;
			}

		case centertablefunc: {
			boolean flcenter;

			flnextparamislast = true;

			if (!getbooleanvalue (hparam1, 1, &flcenter))
				break;

			(*v).data.flvalue = tablesetcenter (flcenter);

			fl = true;

			break;
			}
		*/

		/* sortbyfunc and sortorderfunc moved to headless-compatible section above */

		default:
			break;
		} /*switch*/
	
	shellupdatescrollbars (shellwindowinfo);
	
	shellpopglobals ();
	
	return (fl);
	} /*tablefunctionvalue*/


#ifndef FRONTIER_HEADLESS
/* Windowed mode: register with tablefunctionvalue callback */
boolean tableinitverbs (void) {

	return (loadfunctionprocessor (idtableverbs, &tablefunctionvalue));
	} /*tableinitverbs*/
#endif /* !FRONTIER_HEADLESS */
/* Note: Headless mode provides its own tableinitverbs() in tests/headless_table_verbs.c
 * which registers with headless_table_verbs_callback instead. */



