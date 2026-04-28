
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

#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "shell.h"
#include "memory.h"
#include "logging.h"
#include "langexternal.h"


/*
jackets around routines that are implemented as callbacks.
*/


boolean flerrorscriptrunning = false;


boolean langpushlocalchain (hdlhashtable *htable) {
	
	/*
	9/23/91 dmb: to clean up langfunccall, we've distributed magictable 
	handling around so that a locals table can be created and populated 
	without actually pushing it into the local chain.  our task here is 
	simply to make sure that it's consumed, whether or not an error occurs.
	*/
	
	register boolean fl;
	
	fl = (*langcallbacks.pushtablecallback) (htable);
	
	/*stacktracer (hashgetstackdepth ());*/ /*debugging code*/
	
	log_trace(LOG_COMP_GENERAL, "langpushlocalchain: hmagictable=%p callback=%p",
	          (void*)hmagictable, (void*)langmagictabledisposecallback);

	if (hmagictable != nil) { /*make sure we've consumed any magic, even on error*/

		log_debug(LOG_COMP_GENERAL, "langpushlocalchain: consuming hmagictable=%p", (void*)hmagictable);

		disposehashtable (hmagictable, false);

		hmagictable = nil;
		}
	
	return (fl);
	} /*langpushlocalchain*/
	
	
boolean langpoplocalchain (hdlhashtable hcheck) {

	register hdlhashtable ht = currenthashtable;
	register boolean fl;

	assert (hcheck == ht);

	/*
	 * Call REPL callback to sync variables BEFORE disposing the local table.
	 * This allows the REPL to capture any variables that were created in
	 * the `with` block before they are disposed.
	 */
	if (langmagictabledisposecallback != nil) {
		log_debug(LOG_COMP_GENERAL, "langpoplocalchain: calling REPL callback for table=%p", (void*)ht);
		(*langmagictabledisposecallback)(ht);
	}

	fl = (*langcallbacks.poptablecallback) (ht);

	/*stacktracer (hashgetstackdepth ());*/ /*debugging code*/

	return (fl);
	} /*langpoplocalchain*/
	

boolean langbackgroundtask (boolean flresting) {
	
	/*
	a jacket around the running of background tasks -- we must save and restore
	some globals on the C stack, otherwise things break!
	
	1/18/93 dmb: added flresting parameter
	*/
	
	boolean fl;
	
	#ifdef fldebug
	
	boolean flsavedbreak;
	
	flsavedbreak = flbreak;
	
	#endif
	
	fl = (*langcallbacks.backgroundtaskcallback) (flresting);
	
	assert (flbreak == flsavedbreak); /*restore globals*/
	
	return (fl);
	} /*langbackgroundtask*/


boolean languserescaped (boolean flchecknow) {
	
	/*
	9/27/92 dmb: added flichecknow parameter
	*/
	
	return ((*langcallbacks.scriptkilledcallback) (flchecknow));
	} /*languserescaped*/


boolean langdebuggercall (hdltreenode hnode) {
	
	/*
	if we return false, it's a signal to the caller to exit without an error
	dialog.  the user has decided to stop running the current script.
	
	there didn't seem to be a better place to explain why we don't break at
	compound lines -- like the ones which contain loop, if, fileloop, etc.
	
	this is why: the compiler doesn't generate an accurate line number for
	these.  the line number is the first one after the closing right curly
	bracket.  if the debugger stopped on an if, it would put the cursor on
	the first statement after the if!  this is fine for error reporting,
	but is not too useful for debugging.  so we have a rule, the cursor
	only stops on "meaty" lines, ones which actually do some command or
	arithmetic.  not controlling the flow of the program.  of course it's
	easy to discern the flow from where the cursor ends up.
	
	1/18/91 dmb: parser modifications now generate accurate line numbers 
	for everything except modules.
	
	2/13/91 dmb: don't debug noops, localops, bundleops
	
	7/29/91 dmb: save and restore flreturn & flbreak, just like backgroundtask
	
	5/20/92 dmb: leave it up to the callback to decide which lines are "meaty".  
	pass the node itself to the callback instead of the line & character.
	*/
	
	register hdltreenode h = hnode;
	boolean fl;
	boolean flsavedcontinue;
	
	flsavedcontinue = flcontinue;
	
	fl = (*langcallbacks.debuggercallback) (h);
	
	assert (flcontinue == flsavedcontinue); /****should be able to nuke this stuff*/
	
	return (fl);
	} /*langdebuggercall*/


boolean langsaveglobals (void) {
	
	return ((*langcallbacks.saveglobalscallback) ());
	} /*langsaveglobals*/


boolean langrestoreglobals (void) {
	
	return ((*langcallbacks.restoreglobalscallback) ());
	} /*langrestoreglobals*/


boolean langpushsourcecode (hdlhashtable htable, hdlhashnode hnode, bigstring bs) {

	return ((*langcallbacks.pushsourcecodecallback) (htable, hnode, bs));
	} /*langpushsourcecode*/


boolean langpopsourcecode (void) {

	return ((*langcallbacks.popsourcecodecallback) ());
	} /*langpopsourcecode*/



boolean langerrormessage (bigstring bs) {

	/*
	10/31/91 dmb: experimented with calling local "error" routine on all
	errors.  it's quickly becoming apparant that this is not a generally safe
	thing to do, and that certain classes of errors must not trigger this
	call (compiler errors, for example).  If we want to support this in the
	future, a more formal mechanism should be developed.

	4.1b3 dmb: added call to new langseterrorcallbackline for stack tracing (on error)

	2026-01-28: Fixed error logging to respect error suppression (Issue #325).
	When errors are disabled via disablelangerror() (e.g., by defined() verb),
	we should not log them. This allows defined() to check for non-existent
	table entries without generating spurious error messages.
	*/

	if (!langerrorenabled ())
		return (true);

	if (fllangerror) /*one message per script*/
		return (true);

	/* Headless/portable: safely print bigstring without VLAs or overflow.
	   2026-02-04: Check langerrorlogenabled() to suppress logging in try blocks.
	   Try blocks disable logging but still need the callback to capture errors. */
	if (langerrorlogenabled ()) {
		char cs[256]; /* bigstring max is 255; path may truncate (see #423) */
		char cspath[256];
		hdlhashtable hthis = nil;
		bigstring bsname, bspath;

		copyptocstring(bs, cs);

		if (langgetthisaddress (&hthis, bsname) && hthis != nil && langexternalgetfullpath (hthis, bsname, bspath, nil)) {
			copyptocstring(bspath, cspath);
			log_error(LOG_COMP_LANG, "[%s:%lu] %s", cspath, ctscanlines, cs);
		}
		else {
			log_error(LOG_COMP_LANG, "line %lu: %s", ctscanlines, cs);
		}
	}

	fllangerror = true; /*only display once for each script*/

	/*
	2026-03-12: Populate tryerror as a safety net when callback has been
	replaced during a thread context switch.

	When thread context switches occur inside a try body (at
	langbackgroundtask yield points), pushprocess/popprocess can replace
	the errormessagecallback with the incoming thread's callback instead
	of langtryerror. If the error then fires under the wrong callback,
	langtryerror never runs and tryerror stays nil, causing "tryError
	hasn't been defined" in the else block.

	We detect this situation by checking if tryerror is nil (meaning
	we're not currently accumulating a try error) and populate it here.
	If langtryerror does run as the callback, it will find tryerror
	already set and skip its own allocation.

	When NOT inside a try block, evaluatetry is not on the call stack,
	so tryerror will remain set until the next evaluatetry entry clears
	it (evaluatetry already handles non-nil tryerror at cleanup).
	*/
	if (tryerror == nil)
		newtexthandle (bs, &tryerror);


	langseterrorcallbackline ();


	if (!(*langcallbacks.debugerrormessagecallback) (bs, langcallbacks.errormessagerefcon))
		return (false);

	return ((*langcallbacks.errormessagecallback) (bs, langcallbacks.errormessagerefcon));
	} /*langerrormessage*/


boolean langerrorclear (void) {
	
	fllangerror = false;
	
	return ((*langcallbacks.clearerrorcallback) ());
	} /*langerrorclear*/


boolean langcompilescript (hdlhashnode hnode, hdltreenode *hcode) {
	
	return ((*langcallbacks.scriptcompilecallback) (hnode, hcode));
	} /*langcompilescript*/


void langsymbolchanged (hdlhashtable htable, const bigstring bs, hdlhashnode hnode, boolean flvalue) {
	
	/*
	5.1.5b15 dmb: call callback first, so it can tell if table was already dirty
	*/


	(*langcallbacks.symbolchangedcallback) (htable, bs, hnode, flvalue);
	
	dirtyhashtable (htable);
	} /*langsymbolchanged*/

/*
void langsymbolprechange (hdlhashtable htable, const bigstring bs, hdlhashnode hnode, boolean flvalue) {
	
	(*langcallbacks.symbolprechangecallback) (htable, bs, hnode, flvalue);

	}*/ /*langsymbolprechange*/


void langsymbolinserted (hdlhashtable htable, const bigstring bsname, hdlhashnode hnode) {


	(*langcallbacks.symbolinsertedcallback) (htable, bsname, hnode);

	} /*langsymbolinserted*/


void langsymbolunlinking (hdlhashtable htable, hdlhashnode hnode) {
	
	(*langcallbacks.symbolunlinkingcallback) (htable, hnode);

	} /*langsymbolunlinking*/


void langsymboldeleted (hdlhashtable htable, const bigstring bsname) {

	(*langcallbacks.symboldeletedcallback) (htable, bsname);

	} /*langsymboldeleted*/


boolean langpartialeventloop (UInt16 desiredevents) {
	
	return ((*langcallbacks.partialeventloopcallback) (desiredevents));
	} /*langpartialeventloop*/



