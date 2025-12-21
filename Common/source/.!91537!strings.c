
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

/* 2025-11-24 Codex: Normalize BE writes/coverage for v7 portability. */
/* 2025-12-09 Codex: Guard TEC converter disposal so failed converter creation during migration does not crash. */

#include "frontier.h"
#include "standard.h"

#include <stdint.h>
#include <stdlib.h>

#include "font.h"
#include "memory.h"
#include "quickdraw.h"
#include "strings.h"
#include "ops.h"
#include "resources.h"
#include "shell.rsrc.h"
#include "tablestructure.h"
#include "timedate.h"
#include "langinternal.h" /* 2006-02-26 creedon */
#include "db_format.h" /* 2025-11-23 Codex: BE helper for hex serialization */
#include "byteorder.h"	/* 2006-04-08 aradke: endianness conversion macros */


#define ctparseparams 4

#define hextoint(ch) (isdigit(ch)? (ch - '0') : (getlower (ch) - 'a' + 10))
	
#define stringerrorlist 263

/*
	constants related to text-conversion functions
*/
#define cs_utf8			BIGSTRING( "\xA5" "utf-8" )
#define cs_utf16		BIGSTRING( "\x06" "utf-16" )
#define cs_iso88591		BIGSTRING( "\xA0" "iso-8859-1" )
#define cs_macintosh	BIGSTRING( "\x09" "macintosh" )


byte zerostring [] = "\0"; /*use this to conserve constant space*/

unsigned char lowercasetable[256];

static hdlstring parseparams [ctparseparams] = {nil, nil, nil, nil};

static hdlstring dirstrings [ctdirections];

static byte bshexprefix [] = STR_hexprefix;


/* declarations */

static boolean converttextencoding( Handle, Handle, const long, const long, long * );
static boolean getTextEncodingIDFromIANA( bigstring, long * );



/* definitions */

boolean equalstrings (const bigstring bs1, const bigstring bs2) {

	/*
	return true if the two strings (pascal type, with length-byte) are
	equal.  return false otherwise.
	*/

	register ptrbyte p1 = (ptrbyte) stringbaseaddress (bs1);
	register ptrbyte p2 = (ptrbyte) stringbaseaddress (bs2);
	register short ct = stringlength (bs1);
	
	if (ct != stringlength (bs2)) /*different lengths*/
		return (false);
	
	while (--ct >= 0) 
		
		if (*p1++ != *p2++)
		
			return (false);
		
	return (true); /*loop terminated*/
	} /*equalstrings*/


boolean equaltextidentifiers (byte * string1, byte * string2, short len) {
	while (--len >= 0) 
		
		if (getlower (*string1++) != getlower (*string2++))
		
			return (false);
	
	return (true); /*loop terminated*/
	} /*equaltextidentifiers*/


boolean equalidentifiers (const bigstring bs1, const bigstring bs2) {
	
	register long ct = stringlength (bs1);
	
	if (ct != stringlength (bs2)) /*different lengths*/

		return (false);

	else {

		register ptrbyte p1 = ((ptrbyte) bs1) + ct;
		register ptrbyte p2 = ((ptrbyte) bs2) + ct;

		while (--ct >= 0) 

			if (getlower (*p1--) != getlower (*p2--))
			
				return (false);
		}
	
	return (true); /*loop terminated*/
	} /*equalidentifiers*/



short comparestrings (bigstring bs1, bigstring bs2) {

	/*
	3.0.2b1 dmb: use std c lib as much as possible
	*/
	
	register short len1 = stringlength (bs1);
	register short len2 = stringlength (bs2);
	register short n;
	
	n = memcmp (stringbaseaddress (bs1), stringbaseaddress (bs2), min (len1, len2));
	
	if (n == 0)
		n = sgn (len1 - len2);

	/*
	AR 2004-08-24: memcmp is not guaranteed to return -1 || 0 || +1,
	but some of our callers expected -1 as the only negative value,
	make it so...
	*/
	if (n < -1)
		return (-1);
	
	return (n);
	} /*comparestrings*/

/*
short comparestrings (bigstring bs1, bigstring bs2) {

	#*
	return zero if the two strings (pascal type, with length-byte) are
	equal.  return -1 if bs1 is less than bs2, or +1 if bs1 is greater than
	bs2.
	
	use the Machintosh international utility routine IUCompString, which 
	performs dictionary string comparison and accounts for accented characters
	%/
	
	return (IUCompString (bs1, bs2));
	} #*comparestrings*/



short compareidentifiers (bigstring bs1, bigstring bs2) {

	/*
	2004-11-09 aradke: cross between comparestrings and equalidentifiers,
	useful for sorting identifiers.

	return zero if the two strings (pascal type, with length-byte) are
	equal.  return -1 if bs1 is less than bs2, or +1 if bs1 is greater than
	bs2. the comparison is not case-sensitive.
	*/
	
	register short n = min (stringlength (bs1), stringlength (bs2)) + 1;
	register ptrbyte p1 = (ptrbyte) stringbaseaddress (bs1);
	register ptrbyte p2 = (ptrbyte) stringbaseaddress (bs2);
	
	while (--n)
		if (getlower (*p1++) != getlower (*p2++))
			return ((getlower (*--p1) < getlower (*--p2)) ? -1 : +1); /*rewind*/
	
	return (sgn (stringlength (bs1) - stringlength (bs2)));
	} /*compareidentifiers*/


boolean stringlessthan (register bigstring bs1, register bigstring bs2) {
	
	return (comparestrings (bs1, bs2) < 0);
	} /*stringlessthan*/

	
boolean pushstring (bigstring bssource, bigstring bsdest) {

	/*
	insert the source string at the end of the destination string.
	
	assume the strings are pascal strings, with the length byte in
	the first character of the string.
	
	return false if the resulting string would be too long.
	*/
	
	register short lensource = stringlength (bssource);
	register short lendest = stringlength (bsdest);
	register byte *psource, *pdest;
	
	if ((lensource + lendest) > lenbigstring) /*resulting string would be too long*/
		return (false);
		
	pdest = stringbaseaddress (bsdest) + lendest;
	
	psource = stringbaseaddress (bssource);
	
	setstringlength (bsdest, lendest + lensource);
	
	moveleft (psource, pdest, lensource);
	
	return (true);
	} /*pushstring*/


boolean deletestring (bigstring bs, short ixdelete, short ctdelete) {
	
	/*
	delete ct chars in the indicated string, starting with the character
	at 1-based offset ix.
	*/
	
	register short len = stringlength (bs);
	register long ctmove;
	register ptrbyte pfrom, pto;		
	
	if ((ixdelete > len) || (ixdelete < 1))
		return (false);
		
	if (ctdelete <= 0)
		return (ctdelete == 0);
		
	--ixdelete;	// make zero-based

	ctmove = len - ixdelete - ctdelete;
	 
	if (ctmove > 0) {
		
		pfrom = stringbaseaddress (bs) + ixdelete + ctdelete;
		
		pto = stringbaseaddress (bs) + ixdelete;
		
		moveleft (pfrom, pto, ctmove);
		}
	
	setstringlength (bs, len - ctdelete);
	
	return (true);
	} /*deletestring*/


boolean deletefirstchar (bigstring bs) {	
	
	return (deletestring (bs, 1, 1));
	} /*deletefirstchar*/
	
	
short popleadingchars (bigstring bs, byte ch) {
	
	/*
	2.1b1 dmb: return the number of characters popped
	*/
	
	register short len = stringlength (bs);
	register short i;
	
	for (i = 0; i < len; i++) {
		
		if (getstringcharacter (bs, i) != ch) {
			
			deletestring (bs, 1, i);
			
			return (i);
			}
		} /*for*/
		
	setemptystring (bs);
	
	return (len);
	} /*popleadingchars*/


short poptrailingchars (bigstring bs, byte ch) {
	
	/*
	5.1.3 dmb: pop the trailing characters, and return the new length
	*/
	
	while (!isemptystring (bs) && (lastchar (bs) == ch))
		setstringlength (bs, stringlength (bs) - 1);
	
	return (stringlength (bs));
	} /*popleadingchars*/


boolean pushchar (byte ch, bigstring bs) {
	
	/*
	insert the character at the end of a pascal string.
	*/
	
	register short len;
	
	len = stringlength(bs); 
	
	if (len >= lenbigstring)
		return (false);
	
	setstringcharacter(bs, len, ch);
	
	setstringlength(bs, len+1);
	
	return (true);
	} /*pushchar*/
	
	
boolean pushspace (bigstring bs) {

	/*
	an oft-repeated function, add a space at the end of bs.
	*/
	
	return (pushchar (chspace, bs));
	} /*pushspace*/
	
	
boolean pushlong (long num, bigstring bsdest) {

	bigstring bsint;
	
	numbertostring (num, bsint);
	
	return (pushstring (bsint, bsdest));
	} /*pushlong*/
	

boolean pushint (short num, bigstring bsdest) {
	
	return (pushlong ((long) num, bsdest));
	} /*pushint*/

/*
pushboolean (boolean flboo, bigstring bsdest) {
	
	bigstring bsboo;
	
	if (flboo)
		copystring ((ptrstring) "\ptrue", bsboo);
	else
		copystring ((ptrstring) "\pfalse", bsboo);
		
	pushstring (bsboo, bsdest);
	} #*pushboolean*/
	
	
/*
pushstringresource (short listnum, short resnum, bigstring bs) {
	
	bigstring bsres;
	
	GetIndString (bsres, listnum, resnum);
	
	pushstring (bsres, bs);
	} #*pushstringresource*/
	
	
boolean insertstring (bigstring bssource, bigstring bsdest) {
	
	/*
	insert the source string at the beginning of the destination string.
	
	return false if the resulting string would be longer than 255 chars.
	*/
	
	register short len1 = stringlength (bssource), len2 = stringlength (bsdest);
	bigstring bs;
	
	if ((len1 + len2) > lenbigstring) /*resulting string would be too long*/
		return (false);
		
	copystring (bssource, bs);
	
	pushstring (bsdest, bs);
	
	copystring (bs, bsdest);
	
	return (true);
	} /*insertstring*/


boolean insertchar (byte ch, bigstring bsdest) {
	
	register byte *pdest = bsdest;
	register short len = stringlength (pdest);
	
	if (len == lenbigstring)
		return (false);
	
	moveright (stringbaseaddress (pdest), stringbaseaddress (pdest) + 1, len);
	
	setstringlength (pdest, len + 1);
	
	setstringcharacter (pdest, 0, ch);
	
	return (true);
	} /*insertchar*/


void midstring (bigstring bssource, short ix, short len, bigstring bsdest) {
	
	/*
	2.1b2 dmb: to make calling easier, protect again negative lengths
	*/
	
	if (len <= 0)
		setemptystring (bsdest);
	
	else {
		
		setstringlength (bsdest, len);
		
		moveleft (stringbaseaddress (bssource) + ix - 1, stringbaseaddress (bsdest), (long) len);
		}
	} /*midstring*/


boolean textfindreplace (Handle hfind, Handle hreplace, Handle hsearch, boolean flreplaceall, boolean flunicase) {

	/*
	dmb: this could be upgraded to use a handlestream
	
	6.1d4 AR: Updated to use a handlestream as per dmb's suggestion

	6.1d7 AR: Updated to handle unicase searching. Minor optimizations.
	*/
	
	long ctfind, ctreplace;
	handlestream s;
	
	openhandlestream (hsearch, &s);
	
	lockhandle (hreplace);
	
	ctreplace = gethandlesize (hreplace);
	
	ctfind = gethandlesize (hfind);
	
	while (true) {
		
		s.pos = flunicase
			? searchhandleunicase (hsearch, hfind, s.pos, s.eof)
			: searchhandle (hsearch, hfind, s.pos, s.eof);

		if (s.pos < 0) /*no match found*/
			break;

		if (!mergehandlestreamdata (&s, ctfind, *hreplace, ctreplace)) {
			
			closehandlestream (&s);

			unlockhandle (hreplace);
			
			return (false); //usually an out of memory error
			}
		
		if (!flreplaceall)
			break;
		}
	
	unlockhandle (hreplace);
	
	closehandlestream (&s);

	return (true);
	} /*textfindreplace*/


boolean stringfindreplace (bigstring bsfind, bigstring bsreplace, Handle hsearch, boolean flreplaceall, boolean flunicase) {

	/*
	6.1b6 AR: Wrapper for textfindreplace. I got tired of replicating this functionality.
	*/

	Handle hfind = nil;
	Handle hreplace = nil;
	boolean fl;

	fl = newtexthandle (bsfind, &hfind);

	fl = fl && newtexthandle (bsreplace, &hreplace);

	fl = fl && textfindreplace (hfind, hreplace, hsearch, flreplaceall, flunicase);

	disposehandle (hfind);

	disposehandle (hreplace);

	return (fl);
	}/*stringfindreplace*/


boolean dropnonalphas (bigstring bs) {


	/*
	5.1.4 dmb: strip non alphas and return true if the string isn't empty
	*/
	
	register short ct = stringlength (bs);
	
	while (--ct >= 0) {
		
		if (!isalnum (getstringcharacter (bs, ct)))
			deletestring (bs, ct + 1, 1);
		}
	
	return (!isemptystring (bs));
	} /*dropnonalphas*/


boolean streamdropnonalphas (handlestream *s) {


	/*
	5.1.4 dmb: we needed this two places, to here it is
	*/
	
	for ((*s).pos = 0; (*s).pos < (*s).eof; ) {
		
		if (isalnum ((*(*s).data) [(*s).pos]))
			++(*s).pos;
		else
			pullfromhandlestream (s, 1, nil);
		}
	
	return (true);
	} /*streamdropnonalphas*/


boolean scanstring (byte ch, bigstring bs, short *ix) {
	
	/*
	return in ix the index in the string of the first occurence
	of chscan.
	
	return false if it wasn't found, true otherwise.
	
	dmb 10/26/90: p is now initialized correctly to bs + i, not bs + 1
	
	dmb 1/22/97: using new string macros, be careful to preserve 1-basedness
	*/
	
	register short i;
	register ptrbyte p;
	register byte c = ch;
	register short len = stringlength (bs);
	
	for (i = *ix - 1, p = stringbaseaddress (bs) + i; i < len; i++) 
		
		if (*p++ == c) {
			
			*ix = i + 1;
			
			return (true);
			}
			
	return (false);
	} /*scanstring*/


boolean stringfindchar (byte ch, bigstring bs) {
	
	/*
	simpler entrypoint for scanstring when caller just wants 
	to know if ch appears anywhere in bs
	*/
	
	short ix = 1;
	
	return (scanstring (ch, bs, &ix));
	} /*stringfindchar*/


boolean stringreplaceall (char ch1, char ch2, bigstring bs) {
	
	/*
	replace all instances of ch1 in bs with ch2
	
	5.0d14 dmb: scanstring is 1-based, setstringcharacter is 0-based.
	*/
	
	short ix = 1;
	
	while (scanstring (ch1, bs, &ix))
		setstringcharacter (bs, ix - 1, ch2);
	
	return (true);
	} /*stringreplaceall*/


boolean stringswapall (char ch1, char ch2, bigstring bs) {

	/*
	2009-08-30 aradke: replace all instances of ch1 with ch2 and vice versa, useful for converting posix file paths
	*/
	
	int ct = stringlength(bs);
	unsigned char* p = stringbaseaddress(bs);
	
	while (ct > 0) {
	
		if (*p == ch1)
			*p = ch2;
		else if (*p == ch2)
			*p = ch1;
		
		p++;
		ct--;
		}
	
	return (true);
	} /*stringswapall*/


boolean textlastword (ptrbyte ptext, long len, byte chdelim, bigstring bsdest) {
	
	/*
	copy the last word from bs, and put it into bsdest.
	
	search backwards from the end of the source string until you find
	chdelim.
	*/
	
	register long i;
	
	for (i = len; i > 0; i--) {
		
		if (ptext [i - 1] == chdelim)
			break;
		} /*for*/
	
	texttostring (ptext + i, len - i, bsdest);
	
	return (true);
	} /*textlastword*/


boolean textfirstword (ptrbyte ptext, long len, byte chdelim, bigstring bsdest) {
	
	/*
	copy the first word from bs, and put it into bsdest.
	
	search forwards from the beginning of the source string until you 
	find chdelim.
	*/
	
	register long i;
	
	for (i = 0; i < len; i++) {
		
		if (ptext [i] == chdelim)
			break;
		} /*for*/
	
	texttostring (ptext, i, bsdest);
	
	return (true);
	} /*textfirstword*/


boolean textnthword (ptrbyte ptext, long len, long wordnum, byte chdelim, boolean flstrict, long *ixword, long *lenword) {
	
	/*
	6/10/91 dmb: a single word preceeded or followed by chdelim should be 
	counted as just one word
	
	8/7/92 dmb: added flstrict parameter. when set, every delimiter starts a new word, 
	even consecutive characters, possibly yielding empty words
	*/
	
	register long ix;
	register long ixlastword;
	long ctwords = 1;
	boolean fllastwasdelim = true;
	
	ix = 0;
	
	ixlastword = 0;
	
	while (true) { /*scan string*/
		
		if (ix >= len) /*reached end of string*/
			break;
		
		if (ptext [ix] == chdelim) { /*at a delimiter*/
			
			if (ix == len - 1) /*trailing delimiter, don't bump word count*/
				break;
			
			if (flstrict || !fllastwasdelim) {
				
				if (ctwords >= wordnum) /*we've found the end of the right word*/
					break;
				
				ctwords++;
				}
			
			ixlastword = ix + 1; /*next word starts after the delimiter*/
			
			fllastwasdelim = true;
			}
		else
			fllastwasdelim = false;
		
		ix++; /*advance to next character*/
		} /*while*/
	
	/*
	texttostring (ptext + ixlastword, ix - ixlastword, bsword);
	
	if (isemptystring (bsword) && !flstrict)
		return (false);
	*/
	
	*ixword = ixlastword;
	
	*lenword = ix - ixlastword;
	
	if ((*lenword == 0) && !flstrict)
		return (false);
	
	return (ctwords == wordnum);
	} /*textnthword*/


long textcountwords (ptrbyte ptext, long lentext, byte chdelim, boolean flstrict) {
	
	/*
	8/7/92 dmb: added flstrict parameter
	*/
	
	register long wordnum = 1;
//	bigstring bsword;
	long ixword;
	long lenword;
	
	while (true) {
		
		if (!textnthword (ptext, lentext, wordnum, chdelim, flstrict, &ixword, &lenword))
			return (wordnum - 1);
		
		wordnum++;
		} /*while*/
	} /*textcountwords*/


boolean lastword (bigstring bssource, byte chdelim, bigstring bsdest) {
	
	return (textlastword (stringbaseaddress (bssource), stringlength (bssource), chdelim, bsdest));
	} /*lastword*/
	

void poplastword (bigstring bs, byte chdelim) {
	
	bigstring bsword;
	
	if (lastword (bs, chdelim, bsword)) 
	
		setstringlength (bs, stringlength (bs) - stringlength (bsword));
	} /*poplastword*/


boolean firstword (bigstring bssource, byte chdelim, bigstring bsdest) {
	
	return (textfirstword (stringbaseaddress (bssource), stringlength (bssource), chdelim, bsdest));
	} /*firstword*/


boolean nthword (bigstring bs, short wordnum, byte chdelim, bigstring bsword) {
	
	long ixword;
	long lenword;
	
	if (!textnthword (stringbaseaddress (bs), stringlength (bs), wordnum, chdelim, false, &ixword, &lenword))
		return (false);
	
	texttostring (stringbaseaddress (bs) + ixword, lenword, bsword);
	
	return (true);
	} /*nthword*/


boolean nthfield (bigstring bs, short fieldnum, byte chdelim, bigstring bsfield) {
	
	/*
	5.0.2b19 dmb: just like nthword, but flstrict
	*/
	
	long ixfield;
	long lenfield;
	
	if (!textnthword (stringbaseaddress (bs), stringlength (bs), fieldnum, chdelim, true, &ixfield, &lenfield))
		return (false);
	
	texttostring (stringbaseaddress (bs) + ixfield, lenfield, bsfield);
	
	return (true);
	} /*nthfield*/


short countwords (bigstring bs, byte chdelim) {
	
	return (textcountwords (stringbaseaddress (bs), stringlength (bs), chdelim, false));
	} /*countwords*/


boolean textcommentdelete (Handle x) {
	
	/*
	5.0.2b15 dmb: moved guts out of commentdeleteverb. shorten the handle to 
	omit any comments -- ignoring URLs. (smarter than bigstring commentdelete.)
	*/
	
	Handle hcomment;
	byte ch = chcomment;
	long ixcomment, ix2;
	
	//scan for doubleslash
	if (!newtexthandle (BIGSTRING ("\x02//"), &hcomment))
		return (false);
	
	ixcomment = searchhandle (x, hcomment, 0, longinfinity);
	
	while (ixcomment > 0) { //watch out for urls
		
		if ((*x) [ixcomment - 1] != ':')
			break;
		
		ixcomment = searchhandle (x, hcomment, ixcomment + 2, longinfinity);
		}
	
	//scan for Mac comment
	sethandlecontents (&ch, 1L, hcomment);
	
	if (ixcomment >= 0) {
	
		ix2 = searchhandle (x, hcomment, 0, ixcomment);
		
		if (ix2 >= 0)
			ixcomment = ix2;
		}
	else
		ixcomment = searchhandle (x, hcomment, 0, longinfinity);
	
	if (ixcomment >= 0)
		sethandlesize (x, ixcomment);
	
	disposehandle (hcomment);			//RAB: 1/27/98, clean up after one self
	
	return (true);
	} /*textcommentdelete*/


long langcommentdelete (byte chdelim, byte *ptext, long ct) {
	
	/*
	scan the string from left to right.  if we find a comment delimiting
	character, return its offset
	
	10/30/91 dmb: ignore comment characters that are embedded in quoted strings
	
	5/4/93 dmb: handle single-quoted strings too
	
	3.0.2b1 dmb: handle escape sequences. note that we don't have to deal with 
	the 0x## form because ## can never be quote characters, and thus can't 
	confuse our string parsing

	5.0a12 dmb: handle // comments
	
	6.0a13 dmb: take ptext, ct instead of a big string, and return the comment
	offset instead of deleting anything. Note: textcommentdelete does _not_ 
	handle quoted strings.
	*/
	
	register long i;
	register byte ch;
	boolean flinstring = false;
	boolean flinescapesequence = false;
	byte chendstring = 0;
	
	for (i = 0; i < ct; i++) {
		
		ch = ptext [i];
		
		if (flinstring) {
			
			if (flinescapesequence)
				flinescapesequence = false; /*it's been consumed*/
			
			else {
				
				if (ch == (byte) '\\')
					flinescapesequence = true;
				else
					flinstring = (ch != chendstring);
				}
			}
		
		else if (ch == (byte) '"') {
			
			flinstring = true;
			
			chendstring = '"';
			}
		
		else if (ch == (byte) 0xD2) {
			
			flinstring = true;
			
			chendstring = (byte) 0xD3;
			}
		
		else if (ch == (byte) '\'') {
			
			flinstring = true;
			
			chendstring = '\'';
			}
		
		else if (ch == chdelim)
			return (i);

		else if (ch == '/') {
			
			if ((i + 1 < ct) && ptext [i+1] == '/')
				return (i);
			}
		}
	
	return (-1);
	} /*langcommentdelete*/


void commentdelete (byte chdelim, bigstring bs) {
	
	/*
	6.0a13 dmb: converted to use new langcommentdelete
	*/
	
	long i;
	
	i = langcommentdelete (chdelim, stringbaseaddress (bs), stringlength (bs));
	
	if (i >= 0)
		setstringlength (bs, i);
	} /*commentdelete*/


boolean whitespacechar (byte ch) {
	
	return ((ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r'));
	} /*whitespacechar*/
	
	
boolean poptrailingwhitespace (bigstring bs) {
	
	/*
	return true if there were trailing "whitespace" characters to be popped.
	*/
	
	register short i, ct;
	
	ct = stringlength (bs);
	
	for (i = ct; i > 0; i--)
	
		if (!whitespacechar (bs [i])) { /*found a non-blank character*/
			
			setstringlength (bs, i);
			
			return (i < ct);
			}
	
	setemptystring (bs);
	
	return (true); /*string is all blank*/
	} /*poptrailingwhitespace*/
	
	
boolean firstsentence (bigstring bs) { 
	
	/*
	pops all characters after the first period followed by a space.
	
	return true if any chars were popped.
	*/
	
	register short i;
	register short len = stringlength (bs);
	
	for (i = 0; i < len; i++) {
		
		if (getstringcharacter (bs, i) == '.') {
			
			if (i == len - 1) /*no next character, no chars to pop*/
				return (false);
				
			if (whitespacechar (getstringcharacter (bs, i+1))) { /*truncate length and return*/
				
				setstringlength (bs, i+1);
				
				return (true);
				}
			}
		} /*for*/
		
	return (false); /*no chars popped*/
	} /*firstsentence*/

/*  This is now initialized by initstrings and getlower is a macro in strings.h
static boolean initlowercase = false;
char lowercasetable[256];

unsigned char getlower (unsigned char c) { /-fast lowercase functions-/
	int i;

	if (! initlowercase) {
		for (i = 0; i < 256; i++)
			lowercasetable[i] = tolower (i);

		initlowercase = true;
		}

	return (lowercasetable [c]);
	} /-getlower-/

*/

void uppertext (ptrbyte ptext, long ctchars) {
	
	register ptrbyte p = ptext;
	register byte ch;
	
	while (--ctchars >= 0) {
		
		ch = *p;
		
		*p++ = toupper (ch);
		}
	} /*uppertext*/


void lowertext (ptrbyte ptext, long ctchars) {
	
	register ptrbyte p = ptext;
	register byte ch;
	
	while (--ctchars >= 0) {
		
		ch = *p;
		
		*p++ = getlower (ch);
		}
	} /*lowertext*/


void allupper (bigstring bs) {
	
	uppertext (stringbaseaddress (bs), stringlength (bs));
	} /*allupper*/


void alllower (bigstring bs) {
	
	lowertext (stringbaseaddress (bs), stringlength (bs));
	} /*alllower*/
	
	
boolean capitalizefirstchar (bigstring bs) {
	
	register char ch;
	
	if (stringlength (bs) == 0)
		return (true);
		
	ch = getstringcharacter (bs, 0);
	
	if (!islower (ch))
		return (false);
		
	setstringcharacter (bs, 0, toupper (ch));
	
	return (true);
	} /*capitalizefirstchar*/
	
	
boolean isallnumeric (bigstring bs) {
	
	/*
	11/6/92 dmb: allow first character to be a sign instead of a digit
	*/
	
	register short ct = stringlength (bs);
	register ptrbyte p = stringbaseaddress (bs);
	register byte ch;
	
	while (--ct >= 0) {
		
		ch = *p++;
		
		if (!isnumeric (ch)) {
			
			if (ct == stringlength (bs) - 1) { /*checking first character*/
				
				if ((ch == '-') || (ch == '+')) /*sign character -- it's cool*/
					continue;
				}
			
			return (false);
			}
		} /*while*/
		
	return (true); /*composed entirely of numeric chars*/
	} /*isallnumeric*/
	
	
void filledstring (byte ch, short ct, bigstring bs) {
	
	/*
	1/17/97 dmb: recoded with string macros
	*/

	if (ct < 0)
		ct = 0;
	
	setstringlength (bs, ct);

	memset (stringbaseaddress (bs), ch, (long) ct);
	/*
	bs [0] = ct;

	memset (&bs [1], ch, (long) ct);
	*/
	} /*filledstring*/


void padwithzeros (bigstring bs, short len) {
	
	/*
	2003-05-01 AR: insert zeros at beginning of string until
	we have reached the requested length;
	*/
	
	if (len > lenbigstring)
		len = lenbigstring;

	while (len - stringlength (bs) > 0)
		insertchar ('0', bs);

	} /*padwithzeros*/
	
	
void copystring (const bigstring bssource, bigstring bsdest) {

	/*
	create a copy of bssource in bsdest.  copy the length byte and
	all the characters in the source string.

	assume the strings are pascal strings, with the length byte in
	the first character of the string.

	1/17/97 dmb: recoded with string macros
	*/

	register short len;
	
	if (bssource == nil) { /*special case, handled at lowest level*/
		
		setemptystring (bsdest);
		
		return;
		}
	
	len = stringsize (bssource);

	moveleft ((ptrstring) bssource, bsdest, len);


	/*
	len = (short) bssource [0];
	
	for (i = 0; i <= len; i++) 
		bsdest [i] = bssource [i];
	*/
	} /*copystring*/


void copyptocstring (const bigstring bssource, char *sdest) {

	short len = stringlength (bssource);

	memmove (sdest, stringbaseaddress (bssource), len);

	sdest [len] = '\0';
	} /*copyptocstring*/


void copyctopstring (const char *ssource, bigstring bsdest) {

	short len = strlen (ssource);  /*YES: use strlen, this is a C string*/

	memmove (stringbaseaddress (bsdest), ssource, len);

	setstringlength (bsdest, len);
	} /*copyctopstring*/




void copyheapstring (hdlstring hsource, bigstring bsdest) {
	
	/*
	a safe way of copying a string out of the heap into a stack-allocated or
	global string.
	*/
	
	register hdlstring h = hsource;
	
	if (h == nil) { /*nil handles are empty strings*/
		
		setemptystring (bsdest);
		
		return;
		}
	
	// HLock ((Handle) h);
	
	copystring (*h, bsdest);
	
	// HUnlock ((Handle) h);
	} /*copyheapstring*/
	
	
boolean pushheapstring (hdlstring hsource, bigstring bsdest) {
	
	register hdlstring h = hsource;
	register boolean fl;
	
	if (h == nil) /*nil handles are empty strings*/
		return (true);
		
	HLock ((Handle) h);
	
	fl = pushstring (*h, bsdest);
	
	HUnlock ((Handle) h);
	
	return (fl);
	} /*pushheapstring*/


void timedatestring (int64_t ptime, bigstring bs) {
	bigstring bstime;

	timetodatestring (ptime, bs, false);	

	getstringlist (interfacelistnumber, timedateseperatorstring, bstime);

#if defined(FRONTIER_HEADLESS)
	copyctopstring ("; ", bstime);
#else
	/* Fallback if the interface list isn't populated. */
	if (stringlength (bstime) == 0)
		copyctopstring ("; ", bstime);
#endif

	pushstring (bstime, bs);

	timetotimestring (ptime, bstime, true);
		
	pushstring (bstime, bs);


	} /*timedatestring*/


	static byte bsellipses [] = "\x01\xC9";

void ellipsize (bigstring bs, short width) {

	/*
	if the string fits inside the given number of pixels, fine -- do nothing
	and return.
	
	if not, return a string that does fit, with ellipses representing the 
	deleted characters.  ellipses are generated by pressing option-semicolon.

	3/28/97 dmb: rewrote x-platform

	5.0.1 dmb: reenable Win code, but only for long strings to work around a 
	crashing bug I can't figure out.

	5.0.2 dmb: redisable Win code. We've reported thier User Breakpoint in DrawText
	bug, which we can't seem to workaround. I'm making our own code faster instead.
	*/
	
	#ifdef xxxWIN95VERSION
		
		if (stringlength (bs) > 16) {

			RECT r;
			
			r.top = 0;
			r.bottom = 50;
			r.left = 0;
			r.right = width;
			
			pushemptyclip ();
			
			convertpstring (bs);
			
			setWindowsFont();
			
			DrawText (getcurrentDC(), bs, -1, &r, DT_END_ELLIPSIS | DT_MODIFYSTRING | DT_NOPREFIX);
			
			clearWindowsFont();
			
			convertcstring (bs);
			
			popclip ();
			}
	#endif
		{
		byte len;
		
		if (stringpixels (bs) <= width) //nothing to do, the string fits
			return;
		
		len = stringlength (bs); //current length in characters
		
		if (len < 2) //too short to truncate
			return;
		
		width -= stringpixels (bsellipses); //subtract width of ellipses
		
		//cut in half until it's shorter than available width
		do
			setstringlength (bs, len /= 2);
		while
			((len > 1) && (stringpixels (bs) > width));
		
		//undo last halving, then go character by character
		setstringlength (bs, len *= 2);

		while (len > 1) {
			
			setstringlength (bs, --len);

			if (stringpixels (bs) <= width)
				break;
			}

		pushstring (bsellipses, bs);
		}
	} /*ellipsize*/


void parsedialogstring (const bigstring bssource, ptrstring bs0, ptrstring bs1, ptrstring bs2, ptrstring bs3, bigstring bsresult) {
		
	/*
	parse a string with up to four string parameters, following the syntax
	used by the Macintosh Dialog Manager in parsing strings.
	
	return the result of parsing bs in bsresult.
	
	where ^0 appears in text, push bs0 on the result string.
	
	may get fancier later, what if we allow you to imbed UserLand code?
	
	we work on a copy of the source string, so you may pass in the same string in
	bssource and bsresult.
	
	dmb 10/8/90: accept nil parameters
	*/
	
	bigstring bs;
	register short len;
	register short i;
	register byte ch;
	ptrstring params [ctparseparams];
	register short paramnum;
	
	copystring (bssource, bs); /*work on a copy of the source string*/
	
	len = stringlength (bs); /*copy into register*/
	
	params [0] = bs0;
	
	params [1] = bs1;
	
	params [2] = bs2;
	
	params [3] = bs3;
	
	setemptystring (bsresult);
	
	for (i = 1; i <= len; i++) {
		
		ch = bs [i];
		
		if (ch != '^')
			pushchar (ch, bsresult);
			
		else {
			if (i == len) /*the ^ is at the end of the string, no number*/
				return;
			
			paramnum = bs [i + 1] - '0'; /*index into params array*/
			
			if ((paramnum >= 0) && (paramnum <= 3)) {
				
				assert (params [paramnum] != nil); /*string should always be provided*/
				
				pushstring (params [paramnum], bsresult);
				
				i++; /*advance over numeric character*/
				}
			else
				pushchar ('^', bsresult); 
			}
		} /*for*/
	
