
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

#include "frontier.h"
#include "standard.h"

#include "ops.h"
#include "resources.h"
#include "strings.h"
#include "logging.h"
#include "langinternal.h"



boolean fllangerror = false;  /*if true, the langerror dialog has already appeared*/

unsigned short langerrordisable = 0; /*it's possible to temporarily disable lang errors*/

unsigned short langerrorlogdisable = 0; /*suppress logging but allow error callbacks (for try blocks)*/

boolean flreplmode = false;  /*if true, we're in interactive REPL mode (set by CLI)*/




void disablelangerror (void) {

	++langerrordisable;
	} /*disablelangerror*/


void enablelangerror (void) {

	--langerrordisable;
	} /*enablelangerror*/


boolean langerrorenabled (void) {

	return (langerrordisable == 0);
	} /*langerrorenabled*/


void disablelangerrorlog (void) {
	/*
	2026-02-04: Disable error logging without disabling error callbacks.
	Used by try blocks - errors should be caught but not logged.
	*/
	++langerrorlogdisable;
	} /*disablelangerrorlog*/


void enablelangerrorlog (void) {

	--langerrorlogdisable;
	} /*enablelangerrorlog*/


boolean langerrorlogenabled (void) {

	return (langerrorlogdisable == 0);
	} /*langerrorlogenabled*/


/*
static boolean fllocalerrorhook = false;

langerrordispatch (bigstring bs) { /%finish this someday%/
	
	hdltreenode hcode;
	
	if (!fllocalerrorhook && langgetlocalhandlercode ((ptrstring) "\perror", &hcode)) {
		
		fllocalerrorhook = true;
		
		fllocalerrorhook = false;
		
		return;
		}
	
	langerrormessage (bs);
	} /%langerrordispatch%/
*/

void langerror (short stringnum) {
	
	bigstring bs;
	
	getstringlist (langerrorlist, stringnum, bs);
	
	langerrormessage (bs);
	} /*langerror*/


void lang3paramerror (short stringnum, const bigstring bs1, const bigstring bs2, const bigstring bs3) {
	
	bigstring bs;
	
	getstringlist (langerrorlist, stringnum, bs);
	
	parsedialogstring (bs, (ptrstring) bs1, (ptrstring) bs2, (ptrstring) bs3, nil, bs);
	
	langerrormessage (bs);
	} /*lang3paramerror*/


void langparamerror (short stringnum, const bigstring bsparam) {
	
	lang3paramerror (stringnum, bsparam, nil, nil);
	} /*langparamerror*/


void lang2paramerror (short stringnum, const bigstring bs1, const bigstring bs2) {
	
	lang3paramerror (stringnum, bs1, bs2, nil);
	} /*lang2paramerror*/


void langlongparamerror (short stringnum, long x) {
	
	byte bslong [64];
	
	numbertostring (x, bslong);
	
	lang3paramerror (stringnum, bslong, nil, nil);
	} /*langlongparamerror*/


void langostypeparamerror (short stringnum, OSType x) {
	
	byte bsid [6];
	
	ostypetostring (x, bsid);
	
	lang3paramerror (stringnum, bsid, nil, nil);
	} /*langostypeparamerror*/


void parseerror (const char *cs) {
	/*
	 * Issue #716 item 1: parseerror takes a NUL-terminated C string from
	 * yacc/lex (see yyerror in langparser.y / langparser.c). Convert to a
	 * Pascal bigstring for lang3paramerror's parsedialogstring formatter.
	 *
	 * Pre-fix the prototype was `bigstring bs`, so yyerror cast its
	 * `const char *s` to `(ptrstring) s` and this function cast it back
	 * to `(const char *)`. The double cast laundered the real type
	 * through a misleading Pascal-typed parameter for no reason.
	 *
	 * copyctopstring (post-#707) clamps payload to 255 bytes and returns
	 * false on truncation. Long syntax-error messages from bison (e.g.
	 * the multi-fragment "syntax error, unexpected ... expecting ..."
	 * variants) can exceed that; surface as a warning so the truncation
	 * is observable rather than silent.
	 */
	bigstring bscopy;

	if (!copyctopstring (cs, bscopy)) {
		log_warn (LOG_COMP_PARSE,
		          "parseerror: yacc message exceeded 255 bytes and was truncated");
		}
	langparamerror (parsererror, bscopy);
	} /*parseerror*/


