/* ADR-005: Headless thread globals for parameter state macros */

#ifndef HEADLESS_THREADGLOBALS_IMPLEMENTATION
#define HEADLESS_THREADGLOBALS_IMPLEMENTATION
#endif

#include "standard.h"
#include "processinternal.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"

/*
 * Headless thread globals setup
 *
 * The hthreadglobals variable is declared as hdlthreadglobals (a double-pointer,
 * because it's a Handle). The macros in processinternal.h dereference it twice:
 *   #define flnextparamislast ((**hthreadglobals).flnextparamislast)
 *
 * To make this work in a static context, we need:
 *   1. A static structure to hold the actual data
 *   2. A static pointer to that structure
 *   3. hthreadglobals points to the pointer (making it a double-pointer)
 */
static tythreadglobals headless_threadglobals_data;
static tythreadglobals *headless_threadglobals_ptr = &headless_threadglobals_data;
hdlthreadglobals hthreadglobals = &headless_threadglobals_ptr;

/*
 * Temporarily undefine the macros so we can access struct fields directly
 * in ALL functions in this file. The macros are restored at the end.
 */
#undef flnextparamislast
#undef flparamerrorenabled
#undef flcoerceexternaltostring
#undef flinhibitnilcoercion
#undef fllocaldotparamsonly
#undef bsfunctionname
#undef fllanghashassignprotect
#undef fllangexternalvalueprotect

/*
 * headless_init_threadglobals - Initialize critical fields
 *
 * Called during headless runtime initialization. Sets fields that must be
 * non-zero for correct operation. Matches subset of newthreadglobals() from
 * process.c but without Handle allocation.
 */
void headless_init_threadglobals(void) {
	/* ADR-005: Parameter handling state initialization */
	headless_threadglobals_data.flnextparamislast = false;
	headless_threadglobals_data.flparamerrorenabled = true;    /* CRITICAL: must default to true */
	headless_threadglobals_data.flcoerceexternaltostring = false;
	headless_threadglobals_data.flinhibitnilcoercion = false;
	headless_threadglobals_data.fllocaldotparamsonly = false;
	setemptystring(headless_threadglobals_data.bsfunctionname);
	headless_threadglobals_data.fllanghashassignprotect = false;
	headless_threadglobals_data.fllangexternalvalueprotect = false;

	/* Thread ID: main thread gets idapplicationthread (2), matching legacy Frontier */
	headless_threadglobals_data.idthread = (hdlthread) 2;

	/* ADR-006: Outline context initialization */
	headless_threadglobals_data.outlinedata = nil;
	headless_threadglobals_data.topoutlinestack = 0;
	for (int i = 0; i < ctoutlinestack; i++) {
		headless_threadglobals_data.outlinestack[i] = nil;
	}

	/* Other fields initialized by runtime (langstartup.c, db_format.c):
	 *   - htablestack: Set during lang initialization
	 *   - langcallbacks: Set during verb initialization
	 *   - current_working_directory: Set by file_working_dir system
	 */
}

/*
 * Cooperative threading support: allocation, disposal, save/restore
 *
 * These functions implement the cooperative threading model from legacy Frontier
 * (process.c:2255-2285 processpsuedothread) for headless mode.
 *
 * The pattern:
 * 1. Allocate new thread globals
 * 2. Save main thread globals (headless_save_threadglobals)
 * 3. Restore new thread globals (headless_restore_threadglobals)
 * 4. Run thread code synchronously
 * 5. Save new thread globals
 * 6. Restore main thread globals
 * 7. Dispose new thread globals
 */

/*
 * headless_new_threadglobals - Allocate and initialize new thread globals
 *
 * Allocates a tythreadglobals structure using the Handle pattern (double pointer).
 * Initializes critical fields matching newthreadglobals() from process.c:1363-1432.
 * Does NOT set idthread — caller sets from registry's user_thread_id.
 *
 * Returns: Handle to new thread globals, or nil on failure
 */
hdlthreadglobals headless_new_threadglobals(void) {

	tythreadglobals *pdata = (tythreadglobals *) calloc(1, sizeof(tythreadglobals));

	if (pdata == NULL)
		return nil;

	tythreadglobals **hdata = (tythreadglobals **) malloc(sizeof(tythreadglobals *));

	if (hdata == NULL) {
		free(pdata);
		return nil;
	}

	*hdata = pdata;

	/* Initialize critical fields (matches process.c newthreadglobals) */
	pdata->flparamerrorenabled = true;
	setemptystring(pdata->bsfunctionname);

	/* Copy langcallbacks from main thread so verb dispatch works */
	pdata->langcallbacks = langcallbacks;

	/* Allocate a NEW error stack for this thread (matches process.c:325).
	 * Each thread must have its own scripterrorstack so that errors in
	 * spawned threads don't propagate to the originating thread. The struct
	 * copy above gives us the same Handle as main — we must replace it. */
	{
		hdlerrorstack herrorstack;

		if (!newclearhandle(sizeof(tyerrorstack), (Handle *) &herrorstack)) {
			free(pdata);
			free(hdata);
			return nil;
		}

		pdata->langcallbacks.scripterrorstack = herrorstack;
	}

	/* Allocate a new table stack for this thread */
	pdata->htablestack = nil; /* Will be set during save/restore */

	/* Outline context starts empty */
	pdata->outlinedata = nil;
	pdata->topoutlinestack = 0;

	return (hdlthreadglobals) hdata;
}

/*
 * headless_dispose_threadglobals - Free thread globals memory
 */
void headless_dispose_threadglobals(hdlthreadglobals hg) {

	if (hg == nil)
		return;

	tythreadglobals *pdata = *hg;

	if (pdata != NULL) {

		/* Free the thread's scripterrorstack (we allocated it in new_threadglobals) */
		if (pdata->langcallbacks.scripterrorstack != nil)
			disposehandle((Handle) pdata->langcallbacks.scripterrorstack);

		/* Note: htablestack is NOT freed here. It's a reference to the global
		 * hashtablestack (copied during save/restore), not something we allocated. */

		free(pdata);
	}

	free(hg);
}

/*
 * headless_save_threadglobals - Save current globals into the handle
 *
 * Copies the current runtime global state into the given thread globals handle.
 * This is the "swap out" operation — call before switching to another thread.
 *
 * CRITICAL: Many runtime "globals" are plain C globals (fllangerror, flreturn,
 * etc.), NOT macros into hthreadglobals. We must save FROM the C globals,
 * matching legacy Frontier's copythreadglobals (process.c:1447-1579).
 */
void headless_save_threadglobals(hdlthreadglobals hg) {

	tythreadglobals *dest;

	if (hg == nil)
		return;

	dest = *hg;

	if (dest == NULL)
		return;

	/* Save the global hashtablestack into the thread's copy */
	dest->htablestack = hashtablestack;

	/* Save langcallbacks (struct copy — includes scripterrorstack Handle) */
	dest->langcallbacks = langcallbacks;

	/* Save parameter state — these ARE macros into hthreadglobals, but
	 * we save them explicitly for consistency with the restore path */
	dest->flnextparamislast = (**hthreadglobals).flnextparamislast;
	dest->flparamerrorenabled = (**hthreadglobals).flparamerrorenabled;
	dest->flcoerceexternaltostring = (**hthreadglobals).flcoerceexternaltostring;
	dest->flinhibitnilcoercion = (**hthreadglobals).flinhibitnilcoercion;
	dest->fllocaldotparamsonly = (**hthreadglobals).fllocaldotparamsonly;
	copystring((**hthreadglobals).bsfunctionname, dest->bsfunctionname);
	dest->fllanghashassignprotect = (**hthreadglobals).fllanghashassignprotect;
	dest->fllangexternalvalueprotect = (**hthreadglobals).fllangexternalvalueprotect;

	/* Save outline context */
	dest->outlinedata = (**hthreadglobals).outlinedata;
	dest->topoutlinestack = (**hthreadglobals).topoutlinestack;
	for (int i = 0; i < ctoutlinestack; i++)
		dest->outlinestack[i] = (**hthreadglobals).outlinestack[i];

	/* Save script state flags — THESE ARE C GLOBALS, not hthreadglobals macros.
	 * This is the critical difference from the broken version that read from
	 * *hthreadglobals struct fields (which weren't being updated by the runtime). */
	dest->flreturn = flreturn;
	dest->flbreak = flbreak;
	dest->flcontinue = flcontinue;
	dest->fllangerror = fllangerror;
	dest->herrornode = herrornode;
	dest->flscriptrunning = flscriptrunning;
	/* flthreadkilled: accessed through struct only (C global not linked in headless) */
	dest->flthreadkilled = (**hthreadglobals).flthreadkilled;

	/* Save scan counters — also C globals */
	dest->ctscanlines = ctscanlines;
	dest->ctscanchars = ctscanchars;

	/* Error hook stack (cterrorhooks, errorhooks[]) — NOT saved in headless mode.
	 * shellpusherrorhook/shellpoperrorhook are no-ops (headless_shellhooks.c),
	 * so the error hook stack is never used. */

	/* Save thread ID */
	dest->idthread = (**hthreadglobals).idthread;
}

/*
 * headless_restore_threadglobals - Restore globals from the handle
 *
 * Copies state from the given thread globals handle into the runtime globals.
 * This is the "swap in" operation — call to activate a thread's context.
 *
 * CRITICAL: Must restore ALL C globals that copythreadglobals saved.
 * Many runtime variables (fllangerror, flreturn, flscriptrunning, etc.) are
 * plain C globals, NOT macros into hthreadglobals. Matching legacy Frontier's
 * swapinthreadglobals (process.c:1582-1699).
 */
void headless_restore_threadglobals(hdlthreadglobals hg) {

	tythreadglobals *src;

	if (hg == nil)
		return;

	src = *hg;

	if (src == NULL)
		return;

	/* Switch hthreadglobals to point at the new thread's data */
	hthreadglobals = hg;

	/* Restore the global hashtablestack from this thread's copy */
	hashtablestack = src->htablestack;

	/* Restore langcallbacks (struct copy — includes scripterrorstack Handle) */
	langcallbacks = src->langcallbacks;

	/* Restore script state flags — THESE ARE C GLOBALS that must be
	 * explicitly written. This is the fix for error propagation from
	 * spawned threads to the originating thread. */
	flreturn = src->flreturn;
	flbreak = src->flbreak;
	flcontinue = src->flcontinue;
	fllangerror = src->fllangerror;
	herrornode = src->herrornode;
	flscriptrunning = src->flscriptrunning;
	/* flthreadkilled: NOT a C global in headless (process.c not linked).
	 * It's already in the struct pointed to by hthreadglobals (set above). */

	/* Restore scan counters — also C globals */
	ctscanlines = src->ctscanlines;
	ctscanchars = src->ctscanchars;

	/* Error hook stack — not used in headless mode (stubs are no-ops) */

	/* Parameter state and outline context are accessed through hthreadglobals
	 * macros (which dereference hthreadglobals, now pointing to this thread's
	 * data). No explicit copy needed for those. */
}

/*
 * Restore the macros for the rest of the codebase
 */
#define flnextparamislast ((**hthreadglobals).flnextparamislast)
#define flparamerrorenabled ((**hthreadglobals).flparamerrorenabled)
#define flcoerceexternaltostring ((**hthreadglobals).flcoerceexternaltostring)
#define flinhibitnilcoercion ((**hthreadglobals).flinhibitnilcoercion)
#define fllocaldotparamsonly ((**hthreadglobals).fllocaldotparamsonly)
#define bsfunctionname ((**hthreadglobals).bsfunctionname)
#define fllanghashassignprotect ((**hthreadglobals).fllanghashassignprotect)
#define fllangexternalvalueprotect ((**hthreadglobals).fllangexternalvalueprotect)

/* ADR-006: Outline context macros removed - use accessor functions instead
 * (op_get_outlinedata, op_set_outlinedata, etc.)
 */
