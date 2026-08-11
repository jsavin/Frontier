
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

/* 2025-11-24 Codex: Normalize BE writes/coverage for v7 portability. */


#include "frontier.h"
#include "standard.h"

#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "strings.h"
#include "ops.h"
#include "yytab.h" /*token defines*/
#include "lang.h"
#include "langinternal.h"
#include "langparser.h"
#include "db_format.h" /* 2025-11-23 Codex: BE helpers for OSTypes */
#include "byteorder.h"	/* 2006-04-08 aradke: endianness conversion macros */
#include "logging.h"



#define chstartcomment (byte) chcomment

#define chendscanstring (byte)0 /*returned when we've run out of text*/

unsigned long ctscanlines; /*number of lines that have been scanned, for error reporting*/

unsigned short ctscanchars; /*number of chars passed over on current line, for error reporting*/


/*
PR1 of REPL error context chain: per-token snapshot of the most recently
scanned token. langscanner captures lasttokenline / lasttokenstart at the
beginning of each token and lasttokenend just before returning. The error
machinery in lang.c (langseterrorcallbackline) copies these into the top
error-stack frame so error responses can carry tokenStart / tokenEnd
columns alongside line / charnum.

These are file-scope (not static) so lang.c can declare them extern. The
GIL serializes language execution so no thread locality is needed for PR1;
a multi-threaded eval future would move these to tythreadglobals.
*/
unsigned long lasttokenline = 0;
unsigned short lasttokenstart = 0;
unsigned short lasttokenend = 0;


static Handle hscanstring; /*this is the text that we're parsing*/

static boolean fllinebasedscan;

static long ixparsestring; /*index of next character to be returned*/

static long lenparsestring; /*the number of characters in the parse string*/

static boolean flsenteol;


bigstring bstoken; /*for viewing with the debugger, text of last token*/




boolean isfirstidentifierchar (byte ch) {
	
	/*
	could this character be the first character in an identifier?
	*/
	
    return (isalpha ((int)ch) || (ch == '_'));
	
	/*
		((ch >= 'a') && (ch <= 'z')) ||

		((ch >= 'A') && (ch <= 'Z')) ||

		(ch == '_'));
	*/
	} /*isfirstidentifierchar*/
	
	
boolean isidentifierchar (byte ch) {
	
	/*
	could the character be the second through nth character 
	in an identifier?
	*/
	
	if (isfirstidentifierchar (ch))
		return (true);
	
    if (isdigit ((int)ch))
		return (true);
		
	if (ch == chtrademark)
		return (true);
		
	return (false);
	} /*isidentifierchar*/


boolean langisidentifier (bigstring bs) {
	
	/*
	called externally to determine when quoting in necessary in path 
	construction
	
	4.1b2 dmb: check the constants table too
	*/
	
	register short ct = stringlength (bs);
	register byte *s = bs;
	tyvaluerecord val;
	hdlhashnode hnode;
	
	if (ct == 0) /*empty string*/
		return (false);
	
	if (!isfirstidentifierchar (*++s))
		return (false);
	
	while (--ct > 0)
		if (!isidentifierchar (*++s))
			return (false);
	
	if (hashtablelookup (hkeywordtable, bs, &val, &hnode)) /*it's a keyword*/
		return (false);
	
	if (hashtablelookup (hconsttable, bs, &val, &hnode)) /*dmb 4.1b2 - it's a constant*/
		return (false);
	
	return (true);
	} /*langisidentifier*/



#if (odbengine==0)
static boolean midinsertchar (byte ch, bigstring bs, short ixinsert) {
	
	byte bs1 [4];  /*rab 3/21/97 was 2*/
	
	if (ixinsert > lenbigstring)
		return (false);
	
	if (stringlength (bs) == lenbigstring) /*overflow -- push character off end*/
		
		setstringlength (bs, lenbigstring - 1);
	
	setstringwithchar (ch, bs1);
	
	return (midinsertstring (bs1, bs, ixinsert)); /*should always be true*/
	} /*midinsertchar*/


boolean langdeparsestring (bigstring bs, byte chendquote) {
	
	/*
	add any necessary escape sequences to make the string compilable. 
	return true if we don't have to truncate the string to do so.
	*/
	
	register byte ch;
	register short ix;
	register short ct = stringlength (bs);
	byte bshex [16]; /*should only need 7 bytes*/
	
	for (ix = 1; --ct >= 0; ++ix) {
		
		ch = bs [ix];
		
		if (isprint (ch)) { /*look for the few printable characters that might need quoting*/
			
			switch (ch) {
				
				case '\\':
					break;
				
				case '\'':
				case '\"':
				case chclosecurlyquote:
					if (ch != chendquote) /*don't always need to quote these*/
						ch = 0;
					
					break;
					
				default:
					ch = 0;
					
					break;
				}
			
			if (ch == 0) /*no quote necessary*/
				continue;
			}
		
		if (ch >= 128) /*extended ascii*/
			continue;
		
		if (!midinsertchar ('\\', bs, ix++))
			return (false);
		
		switch (ch) {
			
			case '\n':
				ch = 'n';
				
				break;
			
			case '\r':
				ch = 'r';
				
				break;
			
			case '\t':
				ch = 't';
				
				break;
			
			case '\\':
			case '\'':
			case '\"':
			case chclosecurlyquote:
				break;
			
			default:
				if (!midinsertchar ('x', bs, ix++))
					return (false);
				
				numbertohexstring (ch, bshex);
				
				if (!midinsertchar (bshex [5], bs, ix++)) /*skip over 0x00*/
					return (false);
				
				ch = bshex [6];
				
				break;
			}
		
		if (ix > lenbigstring)
			return (false);
		
		bs [ix] = ch;
		}
	
	return (true);
	} /*langdeparsestring*/


void parsesetscanstring (Handle htext, boolean fllinebased) {
	
	hscanstring = htext; /*copy into global*/
	
	fllinebasedscan = fllinebased;
	
	lenparsestring = gethandlesize (htext);
	
	ixparsestring = 0;
	
	ctscanlines = 1;
	
	ctscanchars = 0;
	
	flsenteol = false;
	} /*parsesetscanstring*/


unsigned long parsegetscanoffset (unsigned long ctlines, unsigned short ctchars) {
	
	/*
	convert ctlines/ctchars to a zero-based, absolute offset consistent 
	with the parsing of a non-linebased scan string
	*/
	
	return (((((unsigned long) ctlines) - 1) << 16) + ctchars);
	} /*parsegetscanoffset*/


void parsesetscanoffset (unsigned long offset) {
	
	/*
	convert a zero-based, absolute offset to a ctlines/ctchars pair consistent 
	with the parsing of a non-linebased scan string, and set our globals to 
	that value (for error reporting)
	*/
	
	ctscanlines = 1 + (offset >> 16);
	
	ctscanchars = offset & 0xffff;
	} /*parsesetscanoffset*/


static boolean parsestringempty (void) {
	
	return (ixparsestring >= lenparsestring);
	} /*parsestringempty*/
	
	
static byte parsepopchar (void) {
	
	/*
	return the first character from the parse string and remove it
	from the string.  if there are no characters in the string we 
	return the null character.
	
	5/7/93 dmb: support non-linebased scans.  instead of returns, we 
	start a new "line" when the character count is about to overflow.
	
	Dave, sorry about the ?: construct -- it's the only one in the product!  :)
	*/
	
	register ptrbyte p;
	register byte ch;
	
	if (ixparsestring >= lenparsestring) /*string is empty*/
		return (chendscanstring);
	
	p = (ptrbyte) *hscanstring + ixparsestring++;
	
	ch = *p;
	
	ctscanchars++; /*for error reporting*/

//************ RAB ADDED 10/29/97
	if (ixparsestring < lenparsestring) {
		p = (ptrbyte) *hscanstring + ixparsestring;
		if (*p == chlinefeed) {  //lose it
			++ixparsestring;
			++ctscanchars;
			}
		}
	
	if (fllinebasedscan? (ch == chreturn) : (ctscanchars == 0xffff)) { /*passed over another line, or overflowing*/
		
		ctscanlines++; /*for error reporting*/
		
		ctscanchars = 0; /*the number of chars we've passed over in the text*/
		}
	
	return (ch);
	} /*parsepopchar*/


static byte parsefirstchar (void) {
	
	/*
	return the character at the head of the parse stream without 
	popping it.
	*/
	
	register long ix = ixparsestring;
	register ptrbyte p;
	
	if (ix >= lenparsestring)
		return (chendscanstring);

	p = (ptrbyte) *hscanstring + ix;
	
	return (*p);
	} /*parsefirstchar*/
	
	
static byte parsenextchar (void) {
	
	/*
	return the character at the head of the parse stream without 
	popping it.
	*/
	
	register long ix = ixparsestring + 1;
	register ptrbyte p;
	
	if (ix >= lenparsestring)
		return (chendscanstring);
	
	p = (ptrbyte) *hscanstring + ix;
	
	return (*p);
	} /*parsenextchar*/


static void parsepopidentifier (bigstring bs) {
	
	/*
	pull characters off the front of the input stream as long as
	we're still getting identifier characters.  
	*/
	
	setstringlength (bs, 0);
	
	while (true) {
		
		if (!isidentifierchar (parsefirstchar ())) /*finished accumulating identifier*/
			return;
		
		pushchar (parsepopchar (), bs); /*add char to the end of the string*/
		} /*while*/
	} /*parsepopidentifier*/


static boolean parsepopnumber (tyvaluerecord *val) {
	
	/*
	pull characters off the front of the input stream as long as
	we're still getting digits.  when we hit the first non-digit,
	convert what we got into a long and return it.
	
	we expect at least one numeric digit to be there, and do not
	provide for an error return.
	
	5/29/91 dmb: support hex constants in the form "0xhhhhhhhh"
	
	10/21/91 dmb: detect overly-large numbers, like 999999999999.
	*/
	
	bigstring bsnumber;
	register boolean flhex = false;
	register boolean flfloat = false;
	
	if (parsefirstchar () == '0') { /*check for hex constant*/
		
		parsepopchar ();
		
		if (parsefirstchar () == 'x') {
			
			parsepopchar ();
			
			flhex = true;
			}
		}
	
	setemptystring (bsnumber);
	
	while (true) {
		
		register byte ch = parsefirstchar ();
		
		if ((ch == '.') && !flfloat)
			flfloat = true;
		
		else {
			
			if (!isdigit (ch) && !(flhex && isxdigit (ch))) {
				
				if (flfloat) {
					
					double d;
					Handle x;
					
					stringtofloat (bsnumber, &d);
					
					if (!newfilledhandle (&d, longsizeof (d), &x))
						return (false);
					
					initvalue (val, doublevaluetype);
					
					(*val).data.binaryvalue = x;
					}
				else {
					long x;
					bigstring bstest;
					
					if (flhex) {
						
						if (stringlength (bsnumber) > 10)
							goto overflow;
						
						hexstringtonumber (bsnumber, &x);
						}
					else {
						stringtonumber (bsnumber, &x);
						
						popleadingchars (bsnumber, '0');
						
						numbertostring (x, bstest);
						
						popleadingchars (bstest, '0');
						
						if (!equalstrings (bsnumber, bstest))
							goto overflow;
						}
					
					setlongvalue (x, val);
					}
				
				#ifdef fldebug
				
				copystring (bsnumber, bstoken); /*for debugging*/
				
				#endif
				
				return (true);
				}
			}
		
		pushchar (parsepopchar (), bsnumber);
		} /*while*/
	
	overflow:
	
	langparamerror (numbertoolargeerror, bsnumber);
	
	return (false);
	} /*parsepopnumber*/


static byte parsepopescapesequence (void) {
	
	/*
	get the next string character out of the input stream, i.e. a 
	character that is part of a string or character constant.  this is 
	where we handle backslashes for special characters
	*/
	
	register byte ch;
	bigstring bs;
	long x;
	
	ch = parsepopchar ();
	
	switch (ch) {
		
		case 'n':
			return ('\n');
		
		case 'r':
			return ('\r');
		
		case 't':
			return ('\t');
		
		case '\\':
			return ('\\');
		
		case '\'':
			return ('\'');
		
		case '\"':
			return ('\"');
		
		case 'x':
			if (!isxdigit (parsefirstchar ()))
				return (0);
			
			ch = parsepopchar ();
			
			setstringwithchar (ch, bs);
			
			if (isxdigit (parsefirstchar ()))
				pushchar (parsepopchar (), bs);
			
			hexstringtonumber (bs, &x);
			
			return ((byte) x);
		
		default:
			return (ch);
		}
	} /*parsepopescapesequence*/


static boolean buildtexthandle (bigstring bs, Handle *htext) {
	
	/*
	5.0.2 dmb: move the characters from bs to the given text handle.
	
	either create or add to htext.
	*/
	
	boolean fl;

	if (*htext == nil)
		fl = newtexthandle (bs, htext);
	else
		fl = pushtexthandle (bs, *htext);
	
	return (fl);
	} /*buildtexthandle*/


static boolean parsepopstringconst (Handle *htext) {
	
	/*
	pop a string constant off the front of the input stream and
	return a handle to the string allocated in the heap.
	
	return with an error if the string wasn't properly
	terminated, or if there was an allocation error.
	
	5/6/93: don't allow a string to span input lines
	
	2.1b2 dmb: handle escape sequences
	
	2.1b6 dmb: if an escape sequence is for a nul character, we 
	can't tell if that's chendscanstring or not; so check for 
	that before parsing '\'s.  we'll find out soon enough if we're 
	really out of text to scan

	5.0.2 dmb: don't limit string literals to 255 chars; use new 
	buildtexthandle for continuation
	*/
	
	register byte ch, chstop;
	bigstring bs;
	unsigned long lnum;
	unsigned short cnum;
	
	chstop = parsepopchar (); /*pop off opening doublequote, our terminator*/
	
	lnum = ctscanlines;
	
	cnum = ctscanchars;
	
	if (chstop == chopencurlyquote)
		chstop = chclosecurlyquote;
	else
		chstop = (byte) '"';
	
	*htext = nil;

	setstringlength (bs, 0);
	
	while (true) {
		
		ch = parsepopchar ();
		
		if (ch == chstop) /*properly terminated string*/
			return (buildtexthandle (bs, htext));
		
		if (ch == chreturn) /*don't allow string to span lines*/
			break;
		
		if (ch == chendscanstring) /*ran out of characters*/
			break;
		
		if (ch == '\\')
			ch = parsepopescapesequence ();
		
		if (!pushchar (ch, bs)) { /*add the char to the end of the string*/
			
			if (!buildtexthandle (bs, htext))
				return (false);

			setstringwithchar (ch, bs);
			}
		} /*while*/
	
	ctscanlines = lnum; /*make error message point at string start*/
	
	ctscanchars = cnum;
	
	langerror (stringnotterminatederror);
	
	return (false);
	} /*parsepopstringconst*/
	

static void parsepopcomment (void) {

	/*
	consume characters up to and including the next chendcomment.

	comments cannot span more than one line, so endofline also causes
	us to return.

	2026-05-11 jes (Issue #586): terminate on chlinefeed (bare LF) as
	well as chreturn. The 1997 RAB workaround in parsepopchar (langscan.c
	lines ~336-342) peek-eats LFs *after* every returned char, which
	means a bare LF immediately following a comment body char never
	reaches us as `ch` here — the LF is silently consumed and the
	comment runs on to swallow the next statement. Two layers of
	detection are needed:

	  1. Check parsefirstchar BEFORE each parsepopchar — catches LF as
	     the next char to read (e.g., comment starts and is immediately
	     followed by LF, or LF that survived parsepopchar's peek-eat).

	  2. After parsepopchar, detect whether the peek-eat in parsepopchar
	     just consumed a trailing LF — by observing a 2-char advance of
	     ixparsestring when only 1 char was returned. If so, that LF
	     was the line terminator and we should return.

	parsepopchar itself is intentionally NOT modified — earlier attempts
	(PR #609) to change its LF-eating produced 43 integration regressions
	across cross-domain consumers that depended on it.
	*/

	register byte ch;
	register long ixbefore;

	while (true) {

		ch = parsefirstchar ();

		if ((ch == chreturn) || (ch == chlinefeed)) {
			parsepopchar (); /*consume the terminator before returning*/
			return;
			}

		if (ch == chendscanstring)
			return;

		ixbefore = ixparsestring;

		ch = parsepopchar ();

		if (ch == chendcomment)
			return;

		/*
		If parsepopchar advanced the index by 2, it peek-ate a trailing
		LF after the char it returned. That LF is the comment terminator.
		*/
		if (ixparsestring - ixbefore == 2)
			return;
		} /*while*/
	} /*parsepopcomment*/


static boolean parsepopblanks (void) {

	/*
	pop all the leading white space.  return false if the input stream is
	empty, true otherwise.

	5.0a12 dmb: handle // comments

	2026-05-11 jes (Issue #586): treat chlinefeed (bare LF) as whitespace
	on par with chreturn. The RAB-1997 LF-eat in parsepopchar swallows
	most LFs before they reach the scanner, but bare LF at start of input
	or LF in a "\n\n" blank-line pair still reaches us as a token char
	and was previously reported as illegaltokenerror. Accepting LF here
	is symmetrical to chreturn and does not affect parsepopchar.
	*/

	register byte ch;

	while (true) {

		if (parsestringempty ())
			return (false);

		ch = parsefirstchar ();

		if ((ch == chstartcomment) || (ch == '/' && parsenextchar () == '/')) {

			parsepopcomment (); /*pop everything up to and including endcomment char*/
			}

		else {
			if ((ch != ' ') && (ch != chtab) && (ch != chreturn) && (ch != chlinefeed))
				return (true);

			parsepopchar (); /*consume a whitespace character*/
			}
		} /*while*/
	} /*parsepopblanks*/
	

static boolean parsepopcharconst (tyvaluerecord *val) {
	
	/*
	pop a character constant in the form of '<char>' off the input
	stream.  return with error if there was no char following the 
	first ' or if the character immediately following <char> is not another '.
	
	3/29/91 dmb: handle 4-character ostype constants as well 1-char.
	
	2.1b2 dmb: handle escape sequences
	
	2.1b6 dmb: see comment in parsepopstringconst; check for the end 
	of the scan string before parsing escape sequences
	*/
	
	register byte ch;
	register short len = 0;
	byte ch4 [4];
	OSType osvalue;
	unsigned long lnum;
	unsigned short cnum;
	
	parsepopchar (); /*get rid of first single-quote*/
	
	lnum = ctscanlines;
	
	cnum = ctscanchars;
	
	while (true) {
		
		ch = parsepopchar ();
		
		if (ch == chsinglequote) { /*end of char const*/
			
			switch (len) {
				
				case 1: /*normal character const*/
					
					setcharvalue (ch4 [0], val);
					
					#ifdef fldebug
					
					pushchar ((*val).data.chvalue, bstoken); /*for debugging*/
					
					#endif
					
					return (true);
				
				case 4: /*ostype (long) character const*/
					
					moveleft (ch4, &osvalue, 4L);
					
					db_format_write_be32(&osvalue, (uint32_t) osvalue);

					setostypevalue (osvalue, val);
					
					#ifdef fldebug
					
					ostypetostring (osvalue, bstoken); /*for debugging*/
					
					#endif
					
					return (true);
				
				default:
					goto error;
				}
			}
		
		if (len == 4) /*about to get too large*/
			goto error;
		
		if (ch == chendscanstring) /*test before doing esc sequences*/
			goto error;
		
		if (ch == '\\')
			ch = parsepopescapesequence ();
		
		ch4 [len++] = ch; /*add character to constant*/
		} /*while*/
	
	error:
	
	ctscanlines = lnum; /*make error message point at string start*/
	
	ctscanchars = cnum;
	
	langerror (badcharconsterror);
	
	return (false);
	} /*parsepopcharconst*/
	

static tokentype langscanner_inner (hdltreenode *nodetoken);

static tokentype langscanner (hdltreenode *nodetoken) {

	/*
	PR1 of REPL error context chain: thin wrapper around the original
	scanner body. The inner scanner sets lasttokenline / lasttokenstart
	once parsepopblanks has skipped leading whitespace (so the start
	column points at the FIRST character of the token, not the last
	byte of whitespace before it). After the inner scanner returns,
	the wrapper captures lasttokenend from ctscanchars (which now
	points one past the last consumed character).

	The error machinery in lang.c (langseterrorcallbackline) copies
	these three values into the top of the error stack so error
	responses carry the exact span the parser was looking at when the
	error fired.

	The inner scanner has ~30 return sites; wrapping it is dramatically
	safer than threading a goto exit through each return path.

	Only update lasttokenend when the inner consumed a real token. The
	non-token paths (0 == out-of-text, eoltoken == synthetic end-of-line)
	bypass the lasttokenline/lasttokenstart capture above; without this
	guard we'd leave a stale start paired with a fresh end, producing
	tokenEnd < tokenStart or a tokenEnd on a different line than
	tokenStart.
	*/

	tokentype token = langscanner_inner (nodetoken);

	if (token != 0 && token != eoltoken
		&& lasttokenline == ctscanlines
		&& ctscanchars >= lasttokenstart)
		lasttokenend = ctscanchars;

	return (token);
	} /*langscanner*/


static tokentype langscanner_inner (hdltreenode *nodetoken) {

	/*
	scan the input string for the next token, return the character
	that we stopped on in chtoken.

	if it's an identifier or a constant, nodetoken will be non-nil.

	11/22/91 dmb: return zero when out of text, after returning exactly
	one (non-zero) eoltoken.

	5.0.2b10 dmb: exempt const identifier from tmp stack before pushing it
	into code tree. it will be disposed on error
	*/
	
	register byte ch, chfirst, chsecond;
	bigstring bs;
	tyvaluerecord val;
	hdlhashnode hnode;
	
	*nodetoken = nil; /*default*/
	
	#ifdef fldebug
	
	setstringlength (bstoken, 0); 
	
	#endif
	
	if (!parsepopblanks ()) { /*ran out of text*/

		if (flsenteol)
			return (0);

		flsenteol = true; /*we're about to...*/

		return (eoltoken);
		}

	/*
	PR1 of REPL error context chain: capture token start position
	(line + column) AFTER parsepopblanks has skipped leading whitespace
	and comments. lasttokenend is captured by the langscanner wrapper
	after the inner returns.
	*/
	lasttokenline = ctscanlines;
	lasttokenstart = ctscanchars;

	chfirst = ch = parsefirstchar (); /*lookahead at the next character*/
	
	if (ch == chsinglequote) { /*a single-quote, character constant*/
		
		if (!parsepopcharconst (&val))
			return (errortoken);
		
		if (!newconstnode (val, nodetoken))
			return (0 /*errortoken*/);
			
		return (constanttoken);
		}
	
	if ((ch == chdoublequote) || (ch == chopencurlyquote)) { /*a string constant*/
		
		initvalue (&val, stringvaluetype);
		
		if (!parsepopstringconst (&val.data.stringvalue))
			return (errortoken);
		
		#ifdef fldebug
		
		texthandletostring (val.data.stringvalue, bstoken); /*8/13*/
		
		#endif
		
		if (!newconstnode (val, nodetoken))
			return (0 /*errortoken*/);
		
		return (constanttoken);
		}
	
	if (isdigit (ch)) {
		
		if (!parsepopnumber (&val)) /*might be a long or a float*/
			return (errortoken);
		
		if (!newconstnode (val, nodetoken))
			return (0 /*errortoken*/);
		
		return (constanttoken);
		}
	
	if (isfirstidentifierchar (ch)) {
		
		register boolean fl;
		
		parsepopidentifier (bs);
		
		#ifdef fldebug
		
		copystring (bs, bstoken);
		
		#endif
		
        fl = hashtablelookup (hkeywordtable, bs, &val, &hnode);
		
		if (fl) 
			return ((tokentype) val.data.tokenvalue); /*it's a reserved word*/
		
        fl = hashtablelookup (hconsttable, bs, &val, &hnode);
#ifdef FRONTIER_HEADLESS
        {
            const char *dbg = getenv("FRONTIER_DEBUG_SCAN");
            if (dbg && *dbg) {
                char identbuf[64];
                copyptocstring(bs, identbuf);
                if (strcmp(identbuf, "true") == 0 || strcmp(identbuf, "false") == 0 || strcmp(identbuf, "stringType") == 0) {
                    log_trace(LOG_COMP_PARSE, "ident '%s' const_lookup=%s", identbuf, fl ? "hit" : "miss");
                }
            }
        }
#endif
		
		if (fl) { /*it's a pre-defined constant*/
			
			if (!copyvaluerecord (val, &val))
				return (0);
			
			exemptfromtmpstack (&val);
			
            if (!newconstnode (val, nodetoken))
                return (0 /*errortoken*/);
			
			return (constanttoken);
			}
		
        initvalue (&val, stringvaluetype);
#ifdef FRONTIER_HEADLESS
        {
            const char *dbg = getenv("FRONTIER_DEBUG_SCAN");
            if (dbg && *dbg) {
                char identbuf2[64];
                copyptocstring(bs, identbuf2);
                if (strcmp(identbuf2, "true") == 0 || strcmp(identbuf2, "false") == 0 || strcmp(identbuf2, "stringType") == 0) {
                    log_trace(LOG_COMP_PARSE, "ident '%s' -> identifier (not constant)", identbuf2);
                }
            }
        }
#endif
		
		if (!newtexthandle (bs, &val.data.stringvalue))
			return (0 /*errortoken*/);
		
		if (!newidnode (val, nodetoken))
			return (0 /*errortoken*/);
		
		return (identifiertoken);
		}
	
	parsepopchar (); /*consume the token char*/
	
	chsecond = parsefirstchar (); /*may need to look ahead to determine this token*/
	
	#ifdef fldebug
	
	pushchar (chfirst, bstoken);
	
	#endif
	
	switch (chfirst) {
		
		case ',': case '(': case ')': case ';': case '{': case '}': case '.':
		
		case ':':
		
		case '[': case ']': case '@': case '^':
			return (chfirst); /*the ascii value is the token*/
		
        case (byte) 0xD7: /* legacy Mac <= */
			return ('.');
		
		case chnotequals:
			return (NEtoken);
			
		case '*':
			return (multiplytoken);
		
		case '/': case chdivide:
			return (dividetoken);
		
		case '%':
			return (modtoken);
		
        case (byte) 0xBC: /* legacy Mac <= */
			return (LEtoken);
			
        case (byte) 0xBE: /* legacy Mac >= */
			return (GEtoken);
			
		case '+':
			if (chsecond == '+') {
				
				parsepopchar (); /*consume the second char*/
				
				#ifdef fldebug
				
				pushchar ('+', bstoken);
				
				#endif
				
				return (plusplustoken);
				}
				
			return (addtoken);
			
		case '-':
			if (chsecond == '-') {
				
				#ifdef fldebug
				
				pushchar ('-', bstoken);
				
				#endif
				
				parsepopchar (); /*consume the second char*/
				
				return (minusminustoken);
				}
				
			return (subtracttoken);
			
		case '=':
			if (chsecond == '=') {
				
				parsepopchar (); /*consume the second char*/
				
				#ifdef fldebug
				
				pushchar ('=', bstoken);
				
				#endif
				
				return (EQtoken);
				}
				
			return (assigntoken);
			
		case '&':
			if (chsecond == '&') {
				
				parsepopchar (); /*consume the second char*/
				
				#ifdef fldebug
				
				pushchar ('&', bstoken);
				
				#endif
				
				return (andandtoken);
				}
				
			return (bitandtoken);
			
		case '|':
			if (chsecond == '|') {
				
				parsepopchar (); /*consume the second char*/
				
				#ifdef fldebug
				
				pushchar ('|', bstoken);
				
				#endif
				
				return (orortoken);
				}
				
			return (bitortoken);
			
		case '<':
			if (chsecond == '=') {
				
				parsepopchar (); /*consume the second char*/
				
				#ifdef fldebug
				
				pushchar ('=', bstoken);
				
				#endif
				
				return (LEtoken);
				}
				
			return (LTtoken);
			
		case '>':
			if (chsecond == '=') {
				
				parsepopchar (); /*consume the second char*/
				
				#ifdef fldebug
				
				pushchar ('=', bstoken);
				
				#endif
				
				return (GEtoken);
				}
				
			return (GTtoken);
			
		case '!':
			if (chsecond == '=') {
				
				parsepopchar (); /*consume the second char*/
				
				#ifdef fldebug
				
				pushchar ('=', bstoken);
				
				#endif
				
				return (NEtoken);
				}
				
			return (nottoken);
		} /*switch*/
	
	setstringwithchar (chfirst, bstoken);
	
	//assert (ixparsestring <= lenparsestring && hscanstring);
	
	langparamerror (illegaltokenerror, bstoken); /*all the legal tokens are caught above*/
	
	return (errortoken);
	} /*langscanner*/


tokentype parsegettoken (hdltreenode *nodetoken) {
	
	/*
	a bottleneck that makes debugging easier.  
	
	if you want to see the string that generated the current token, 
	display "bstoken" -- its a global�
	*/
	
	register tokentype token;
	
	token = langscanner (nodetoken);
	
	return (token);
	} /*parsegettoken*/


boolean langstripstructuremarkers (Handle hin, Handle *hout) {

	/*
	Produce a fresh copy of UserTalk source text with structural { } ; markers
	stripped from line ends. The outline-builder splits the result by CR into
	nodes; the outline-export pass (oplangtextvisit) re-emits { } ; based on
	outline level transitions on read-back. If those markers survive into a
	node's text AND the export side also emits them, the result is doubled
	markers ({{, ;;, }}) and the script no longer compiles. So storage
	convention is: node text contains content only — no trailing structural
	markers.

	Input contract:
	  - hin: borrowed handle, untouched.
	  - The caller has normalized line endings to CR. This function does NOT
	    re-normalize; if your caller didn't, fix the caller (every path that
	    reaches scripttexttooutlineroutine goes through opnormalizelineendings_cr
	    first, so this should be a non-issue in practice).
	  - Indentation may be tabs OR spaces; we only look at "is this line a
	    comment" (skip-strip) — we do not infer or use indent levels. The
	    outline-builder (opgetlinetext) handles indent inference separately.

	Output contract:
	  - On success: *hout owns a freshly-allocated handle. Caller disposes.
	  - On failure: *hout = nil, returns false.

	Per-line algorithm:
	  1. Skip leading whitespace to find the line's first semantic byte.
	  2. If the whole line is a comment (starts with « or //) — leave it
	     untouched. Comments may legitimately contain { } ; as text.
	  3. Otherwise find the start of any trailing comment by scanning forward
	     while tracking string-literal state. The portion of the line up to
	     that point (or end-of-line) is the "content".
	  4. From the end of content, strip a contiguous trailing run of
	     { } ; space tab. This catches both end-of-line markers
	     (`local (i);`) and end-of-content-followed-by-comment forms
	     (`on foo () { //note` — strips the ` { ` before `//note`, leaving
	     `on foo () //note`).
	  5. Preserve any trailing comment verbatim.

	Replaces langstriptextsyntax, which used a scanner-token-based heuristic
	("a brace separated from its next token by a CR is structural"). That
	worked for the historical script-export format but mis-classified inline
	braces in modern source — `on foo (x) { //comment\r\tlocal (i = 1);` had
	its `{` stripped because the scanner skipped over the comment and CR
	before seeing the next token, even though the brace was load-bearing.
	*/

	long size;
	long readix = 0;
	long writeix = 0;
	ptrbyte buf;
	Handle hcopy = nil;

	*hout = nil;

	if (!copyhandle (hin, &hcopy))
		return (false);

	size = gethandlesize (hcopy);

	if (size <= 0) {
		*hout = hcopy;
		return (true);
		}

	buf = (ptrbyte) (*hcopy);

	while (readix < size) {

		long linestart = readix;
		long lineend;
		long indent = 0;
		long commentstart; /*offset where any trailing comment begins; == content-end*/
		long stripfrom;
		boolean fliscomment = false;
		boolean flinstring = false;

		/*find end-of-line — CR or end-of-buffer*/
		while (readix < size && buf [readix] != chreturn)
			++readix;

		lineend = readix;

		/*skip leading whitespace to find the line's first semantic byte*/
		while (linestart + indent < lineend) {
			byte ch = buf [linestart + indent];
			if (ch == chtab || ch == chspace)
				++indent;
			else
				break;
			}

		/*detect whole-line comment — « or // as first non-whitespace byte*/
		if (linestart + indent < lineend) {
			byte ch1 = buf [linestart + indent];
			if (ch1 == chcomment
			    || (ch1 == '/'
			        && (linestart + indent + 1) < lineend
			        && buf [linestart + indent + 1] == '/'))
				fliscomment = true;
			}

		commentstart = lineend; /*default: no trailing comment, content runs to EOL*/

		if (!fliscomment) {

			/*scan forward from the indent boundary to find the start of any
			trailing comment (« or //) outside a string literal. UserTalk
			has three literal delimiters, and all three must be tracked here
			or a comment marker inside one gets misread as a real comment:
			ASCII " (chdoublequote, 0x22); the Mac smart-quote pair
			chopencurlyquote (0xD2) / chclosecurlyquote (0xD3); and
			chsinglequote (0x27) for character and string4 constants
			(parsepopstringconst / the single-quote branch below both accept
			these). Without that, a string like "a // b" with "" delimiters
			works, but «a // b» or 'Ç' would get its comment byte misread as
			a comment start, truncating the rest of the line. Losing the
			trailing structural-marker strip that way leaves a `{` in the
			stored node text that the outline export re-emits from the level
			transition, so each reinstall adds another brace (#866).
			chclose is the delimiter that ends the literal we are inside.*/
			long i = linestart + indent;
			byte chclose = 0;
			while (i < lineend) {
				byte ch = buf [i];
				if (flinstring) {
					/*\ escapes the next byte (e.g. \" inside ASCII strings).
					Same convention applies inside curly-quote and
					single-quote strings; the scanner doesn't distinguish.*/
					if (ch == '\\' && i + 1 < lineend) {
						i += 2;
						continue;
						}
					if (ch == chclose)
						flinstring = false;
					++i;
					continue;
					}
				if (ch == '"' || ch == chopencurlyquote || ch == (byte) chsinglequote) {
					flinstring = true;
					chclose = (ch == chopencurlyquote) ? (byte) chclosecurlyquote : ch;
					++i;
					continue;
					}
				if (ch == chcomment
				    || (ch == '/' && i + 1 < lineend && buf [i + 1] == '/')) {
					commentstart = i;
					break;
					}
				++i;
				}
			}

		/*strip trailing run of {, }, ;, space, tab from the content portion
		(linestart+indent .. commentstart). Leave the comment span untouched.

		Refinement (#621): stop stripping if doing so would leave a one-line
		nested block (a `{` in the kept content that has no matching `}`
		within the same kept content). Example: `try {new (...)}}` — naively
		stripping all four trailing chars leaves `try {new (...)`, an
		unbalanced opening brace. The outline-export pass (oplangtextvisit)
		re-derives structural braces from outline LEVEL transitions, but
		this line stays at one level (its body is inline, not a child node),
		so the export can't know to emit a matching `}`. The result is a
		dropped closing brace on read-back.

		Algorithm: scan the would-be-kept content for unmatched `{` (ones
		that don't have a later `}` partner within the kept content, ignoring
		string literals). If any are unmatched, stop stripping earlier so
		the trailing `}` that pairs with the inline `{` survives. We keep
		stripping as long as the kept content's brace balance is non-negative.*/
		stripfrom = commentstart;

		if (!fliscomment) {

			while (stripfrom > linestart + indent) {
				byte ch = buf [stripfrom - 1];
				if (ch != '{' && ch != '}' && ch != ';' && ch != chspace && ch != chtab)
					break;

				if (ch == '}') {
					/*Tentatively strip; check that the remaining content
					(linestart+indent .. stripfrom-1) still has balanced or
					surplus closing braces. If not, the brace we are about
					to strip is needed to close an inline one-line block;
					stop. Count {/} in the kept content, respecting
					string-literal state for the ", « and ' forms (escape
					with backslash applies to all three). Single quotes
					must be tracked here for the same reason as in the
					comment-start scan above: a brace inside a character
					or string4 constant is not structural (#866).
					k_close is the delimiter ending the current literal.*/
					long try_from = stripfrom - 1;
					long open_ct = 0;
					long close_ct = 0;
					boolean in_str = false;
					byte k_close = 0;
					boolean esc = false;
					long k;
					for (k = linestart + indent; k < try_from; ++k) {
						byte kc = buf [k];
						if (esc) { esc = false; continue; }
						if (in_str) {
							if (kc == '\\') esc = true;
							else if (kc == k_close)
								in_str = false;
							continue;
							}
						if (kc == '"' || kc == chopencurlyquote || kc == (byte) chsinglequote) {
							in_str = true;
							k_close = (kc == chopencurlyquote) ? (byte) chclosecurlyquote : kc;
							continue;
							}
						if (kc == '{') ++open_ct;
						else if (kc == '}') ++close_ct;
						}
					if (open_ct > close_ct)
						break;
					}
				--stripfrom;
				}
			}

		/*copy kept content [linestart .. stripfrom) into the write cursor*/
		{
			long ct = stripfrom - linestart;
			if (writeix != linestart && ct > 0)
				moveleft (buf + linestart, buf + writeix, ct);
			writeix += ct;
			}

		/*copy any trailing comment [commentstart .. lineend)*/
		if (commentstart < lineend) {
			long ct = lineend - commentstart;
			if (writeix != commentstart && ct > 0)
				moveleft (buf + commentstart, buf + writeix, ct);
			writeix += ct;
			}

		/*copy the CR delimiter (if present), then advance past it*/
		if (readix < size && buf [readix] == chreturn) {
			buf [writeix++] = chreturn;
			++readix;
			}
		}

	if (writeix != size) {
		if (!sethandlesize (hcopy, writeix)) {
			/*shrink should never fail in practice, but if it does we'd
			be returning a handle whose declared size points past valid
			data. Dispose and fail rather than hand back a stale tail.*/
			disposehandle (hcopy);
			return (false);
			}
		}

	*hout = hcopy;

	return (true);
	} /*langstripstructuremarkers*/


boolean langaddapplescriptsyntax (Handle hscript) {
	
	/*
	add vertical bars where necessary to "quote" UserTalk dotted 
	identifiers for AppleScript.
	*/
	
	boolean fldone = false;
	hdltreenode hnode;
	tokentype token, lasttoken = 0;
	long ixstart;
	long ix1 = 0;
	unsigned long line1 = 0;
	bigstring bsid;
	boolean flgotid = false;
	boolean flgotdottedid = false;
	byte chbar = '|';
	
	parsesetscanstring (hscript, true);
	
	disablelangerror ();
	
	while (!fldone) {
		
		ixstart = ixparsestring;
		
		token = langscanner (&hnode);
		
		switch (token) {
			
			/*
			case errortoken:
				flgotbar == firstchar (bstoken) == (byte) '|';
				
				flgotid = false;
				
				break;
			*/
			
			case eoltoken:
				fldone = true;
				
				break;
			
			case identifiertoken:
				if (flgotid && lasttoken == '.') { /*continuation of a dotted id*/
					
					flgotdottedid = true;
					}
				else { /*treat as beginning of a new id*/
					
					flgotid = true;
					
					flgotdottedid = false;
					
					pullstringvalue (&(**hnode).nodeval, bsid);
					
					ix1 = ixparsestring - stringlength (bsid);
					
					line1 = ctscanlines;
					}
				
				break;
			
			case '.':
				if (lasttoken == identifiertoken) /*don't have dottedid, until we get another identifier*/
					flgotdottedid = false;
				else
					flgotid = false;
				
				break;
			
			case '(':
				if (flgotid && flgotdottedid && ctscanlines == line1) { /*we're there*/
					
					insertinhandle (hscript, ixstart, &chbar, sizeof (chbar)); /*do second bar 1st*/
					
					insertinhandle (hscript, ix1, &chbar, sizeof (chbar));
					
					lenparsestring += 2; /*make adjustments*/
					
					ixparsestring += 2; /*ditto*/
					}
				
				flgotid = false;
				
				break;
			
			default:
				/*
				flgotbar = false;
				*/
				
				flgotid = false;
				
				break;
			}
		
		langdisposetree (hnode); /*we don't need it anymore*/
		
		lasttoken = token;
		}
	
	enablelangerror ();
	
	return (true);
	} /*langaddapplescriptsyntax*/



/*
yyoverflow (bsevent, p1, size1, p2, size2, p3, size3, p4) bigstring bsevent; ptrbyte p1, p2, p3; short size1, size2, size3; {
	
	DebugStr ("\pyyoverflow");
	} /%yyoverflow%/
*/

#endif





	
	
	
