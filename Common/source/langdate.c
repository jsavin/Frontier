
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

#include "memory.h"
#include "frontierconfig.h"
#include "error.h"
#include "ops.h"
#include "strings.h"
#include "frontierwindows.h"
#include "shell.h"
#include "shellhooks.h"
#include "oplist.h"
#include "lang.h"
#include "langinternal.h"
#include "langexternal.h"
#include "langsystem7.h"
#include "langipc.h"
#include "langwinipc.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "process.h"
#include "processinternal.h"
#include "kernelverbs.h"
#include "kernelverbdefs.h"
#include "timedate.h"

static char * dayofweeknames[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static char * monthnames[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

#define STR_P_MONTHLIST 		BIGSTRING ("\x7A""{\"January\", \"February\", \"March\", \"April\", \"May\", \"June\", \"July\", \"August\", \"September\", \"October\", \"November\", \"December\"}")
#define STR_P_DAYOFWEEKLIST 	BIGSTRING ("\x4E""{\"Sunday\", \"Monday\", \"Tuesday\", \"Wednesday\", \"Thursday\", \"Friday\", \"Saturday\"}")
#define STR_P_USERPREFSDATES 	BIGSTRING ("\x10""user.prefs.dates")
#define STR_P_MONTHNAMES 		BIGSTRING ("\x0A""monthNames")
#define STR_P_DAYNAMES 			BIGSTRING ("\x08""dayNames")
#define STR_P_PREFS				BIGSTRING ("\x05""prefs")
#define STR_P_DATES				BIGSTRING ("\x05""dates")
#define STR_P_USER				BIGSTRING ("\x04""user")
#define STR_P_GMT				BIGSTRING ("\x04"" GMT")
#define STR_P_COMMA				BIGSTRING ("\x02"", ")

/*These should go elsewhere...?*/
#define STR_P_MONTHNUMERROR 	BIGSTRING ("\x40""Can't convert ^0 to a string because it is not between 1 and 12.")
#define STR_P_DAYNUMERROR 		BIGSTRING ("\x3F""Can't convert ^0 to a string because it is not between 1 and 7.")


/* Return a string that looks like: Sat, 29 Nov 1997 00:51:47 GMT */
 
boolean datenetstandardstring (int64_t localdate, tyvaluerecord *vreturn) {

	handlestream s;
	short day, month, year, hour, minute, second;
	short dayofweek;
	bigstring bs;
	long ctz = getcurrenttimezonebias();
	int64_t gmtdate = localdate - ctz;
	
	openhandlestream (nil, &s);

	if ((localdate >= 0) && (gmtdate < 0)) /* check for wrap-around */
		gmtdate = 0L;

	secondstodayofweek (gmtdate, &dayofweek);
	/* Defensive clamp: some platforms may yield 0 for weekday on extreme dates */
	if (dayofweek < 1 || dayofweek > 7)
		dayofweek = 1; /* default to Sunday */
	
	if (!writehandlestream (&s, dayofweeknames[dayofweek - 1], 3))
		goto exit;

	if (!writehandlestreamstring (&s, STR_P_COMMA))
		goto exit;

	secondstodatetime (gmtdate, &day, &month, &year, &hour, &minute, &second);

	/* Defensive clamp for month index */
	if (month < 1 || month > 12)
		month = 1;

	numbertostring ((long) day, bs);

	if (stringlength (bs) == 1)
		insertchar ('0', bs);

	if (!writehandlestreamstring (&s, bs))
		goto exit;

	if (!writehandlestreamchar (&s, ' '))
		goto exit;
	
	if (!writehandlestream (&s, monthnames[month - 1], 3))
		goto exit;

	if (!writehandlestreamchar (&s, ' '))
		goto exit;

	numbertostring ((long) year, bs);

	if (!writehandlestreamstring (&s, bs))
		goto exit;

	if (!writehandlestreamchar (&s, ' '))
		goto exit;

	numbertostring ((long) hour, bs);

	while (stringlength (bs) < 2)
		insertchar ('0', bs);

	if (!writehandlestreamstring (&s, bs))
		goto exit;

	if (!writehandlestreamchar (&s, ':'))
		goto exit;

	numbertostring ((long) minute, bs);

	while (stringlength (bs) < 2)
		insertchar ('0', bs);

	if (!writehandlestreamstring (&s, bs))
		goto exit;

	if (!writehandlestreamchar (&s, ':'))
		goto exit;

	numbertostring ((long) second, bs);

	while (stringlength (bs) < 2)
		insertchar ('0', bs);

	if (!writehandlestreamstring (&s, bs))
		goto exit;

	if (!writehandlestreamstring (&s, STR_P_GMT))
		goto exit;

	setheapvalue (closehandlestream (&s), stringvaluetype, vreturn);
	
	return (true);

exit:
	
	disposehandlestream (&s);
	
	return (false);
	}/*datenetstandardstring*/


/* Get a string representation for the month name, based on user.prefs.dates.monthNames */

boolean datemonthtostring (long ix, tyvaluerecord *vreturn) {

	hdlhashtable hdatestable;
	boolean fl;
	tyvaluerecord vlist;
	hdlhashnode hnode;
	
	if ((ix < 1) || (ix > 12)) {

		bigstring bs, bsmonthnum;
		
		copystring (STR_P_MONTHNUMERROR, bs);
		
		numbertostring (ix, bsmonthnum);
		
		parsedialogstring (bs, bsmonthnum, nil, nil, nil, bs);
	
		langerrormessage (bs);
		
		return (false);
		}
	
	disablelangerror ();
	
	fl = langfastaddresstotable (roottable, STR_P_USERPREFSDATES, &hdatestable);
	
	enablelangerror ();
	
	if (!fl || !hashtablelookup (hdatestable, STR_P_MONTHNAMES, &vlist, &hnode)) { /*create it*/
		
		hdlhashtable husertable, hprefstable;
		Handle h;
		
		if (!langsuretablevalue (roottable, STR_P_USER, &husertable))
			return (false);

		if (!langsuretablevalue (husertable, STR_P_PREFS, &hprefstable))
			return (false);

		if (!langsuretablevalue (hprefstable, STR_P_DATES, &hdatestable))
			return (false);
		
		if (!newtexthandle (STR_P_MONTHLIST, &h))
			return (false);
		
		setheapvalue (h, stringvaluetype, &vlist);

		if (!coercevalue (&vlist, listvaluetype))
			return (false);
		
		exemptfromtmpstack (&vlist);
		
		if (!hashtableassign (hdatestable, STR_P_MONTHNAMES, vlist)) {
			opdisposelist (vlist.data.listvalue);
			return (false);
			}
		}
	
	if (!coercevalue (&vlist, listvaluetype))
		return (false);
		
	if (!langgetlistitem (&vlist, ix, nil, vreturn))
		return (false);
	
	if (!coercevalue (vreturn, stringvaluetype))
		return (false);
	
	return (true);
	}/*datemonthtostring*/


/* Get a string representation for the day of week, based on user.prefs.dates.dayNames */

boolean datedayofweektostring (long ix, tyvaluerecord *vreturn) {

	hdlhashtable hdatestable;
	boolean fl;
	tyvaluerecord vlist;
	hdlhashnode hnode;
	
	if ((ix < 1) || (ix > 7)) {

		bigstring bs, bsdaynum;
		
		copystring (STR_P_DAYNUMERROR, bs);
		
		numbertostring (ix, bsdaynum);
		
		parsedialogstring (bs, bsdaynum, nil, nil, nil, bs);
	
		langerrormessage (bs);
		
		return (false);
		}
	
	disablelangerror ();
	
	fl = langfastaddresstotable (roottable, STR_P_USERPREFSDATES, &hdatestable);
	
	enablelangerror ();
	
	if (!fl || !hashtablelookup (hdatestable, STR_P_DAYNAMES, &vlist, &hnode)) { /*create it*/
		
		hdlhashtable husertable, hprefstable;
		Handle h;
		
		if (!langsuretablevalue (roottable, STR_P_USER, &husertable))
			return (false);

		if (!langsuretablevalue (husertable, STR_P_PREFS, &hprefstable))
			return (false);

		if (!langsuretablevalue (hprefstable, STR_P_DATES, &hdatestable))
			return (false);
		
		if (!newtexthandle (STR_P_DAYOFWEEKLIST, &h))
			return (false);
		
		setheapvalue (h, stringvaluetype, &vlist);

		if (!coercevalue (&vlist, listvaluetype))
			return (false);
		
		exemptfromtmpstack (&vlist);
		
		if (!hashtableassign (hdatestable, STR_P_DAYNAMES, vlist)) {
			opdisposelist (vlist.data.listvalue);
			return (false);
			}
		}
	
	if (!coercevalue (&vlist, listvaluetype))
		return (false);
		
	if (!langgetlistitem (&vlist, ix, nil, vreturn))
		return (false);
	
	if (!coercevalue (vreturn, stringvaluetype))
		return (false);
	
	return (true);
	}/*datedayofweektostring*/


/*
on versionLessThan (vs1, vs2) {
	1/6/98 by DW
		fixed this case:
			date.versionLessThan ("2.0b9", "2.0")
				true
	on explodeVersion (s, adrtable) {
		new (tableType, adrtable);
		adrtable^.mainVersionNum = 0;
		adrtable^.stageNum = 0;
		adrtable^.subVersionNum = 0;
		
		local (mainVersionString = "");
		local (stageChars = {'d', 'a', 'b', 'f'});
		while sizeof (s) > 0 {
			if stageChars contains s [1] {
				break};
			mainVersionString = mainVersionString + s [1];
			s = string.delete (s, 1, 1)};
		
		case string.countFields (mainVersionString, '.') {
			0 { //no string
				mainVersionString = "0.0.0"};
			1 { //no dots
				mainVersionString = mainVersionString + ".0.0"};
			2 { //one dot
				mainVersionString = mainVersionString + ".0"}};
		while mainVersionString contains "." {
			mainVersionString = mainVersionString - "."};
		adrtable^.mainVersionNum = number (mainVersionString);
		
		if s == "" {
			return};
		
		local (stage = 0, charstodelete = 0);
		case string.lower (s [1]) {
			'd' {
				stage = 1;
				charstodelete = 1};
			'a' {
				stage = 2;
				charstodelete = 1};
			'b' {
				stage = 3;
				charstodelete = 1};
			'f' {
				stage = 4;
				if string.lower (s [2]) == 'c' {
					charstodelete = 2}
				else {
					charstodelete = 1}}};
		s = string.delete (s, 1, charstodelete);
		adrtable^.stageNum = stage;
		
		adrtable^.subVersionNum = number (s)};
*/

static void explodeversion (bigstring bsv, unsigned long *mainversion, unsigned long *subversion) {
	
	long i, x, ix, len;
	bigstring bs;
	
	for (i = 1; i <= stringlength (bsv); i++)
		if ((bsv[i] == 'a') || (bsv[i] == 'b') || (bsv[i] == 'd') || (bsv[i] == 'f'))
			break;

	if (textnthword (stringbaseaddress (bsv), i-1, 1, '.', false, &ix, &len)) {		
		x = 0;
		
		midstring (bsv, ix+1, len, bs); /* off by one ??? */
		
		stringtonumber (bs, &x);
	
		*mainversion += x * 256 * 256 *256;
		}

	if (textnthword (stringbaseaddress (bsv), i-1, 2, '.', false, &ix, &len)) {
		x = 0;
	
		midstring (bsv, ix+1, len, bs);
		
		stringtonumber (bs, &x);
	
		*mainversion += x * 256 * 256;
		}

	if (textnthword (stringbaseaddress (bsv), i-1, 3, '.', false, &ix, &len)) {
		x = 0;
	
		midstring (bsv, ix+1, len, bs);
		
		stringtonumber (bs, &x);
	
		*mainversion += x;
		}
	
	if (i >= stringlength (bsv)) /*there's no subversion*/
		return;

	switch (bsv[i]) {
			
		case 'd':
			*subversion = 0 * 255 * 256 * 256;
			break;
	
		case 'a':
			*subversion = 1 * 255 * 256 * 256;
			break;
			
		case 'b':
			*subversion = 2 * 255 * 256 * 256;
			break;
			
		case 'f':
			if (bsv[i+1] == 'c')
				*subversion = 4 * 255 * 256 * 256;
			else
				return;
			break;
			
		default:
			return;
		}
	
	// 7.1b44 dmb, DON'T munge input string: deletestring (bsv, 1, i);
	midstring (bsv, i + 1, stringlength (bsv) - i, bs);

	x = 0;
	
	stringtonumber (bs, &x);
	
	*subversion += x;
	
	return;	
	} /*explodeversion*/
	

boolean dateversionlessthan (bigstring bsv1, bigstring bsv2, tyvaluerecord *v) {

	unsigned long m1 = 0;
	unsigned long m2 = 0;
	unsigned long s1 = 0xffffffff;
	unsigned long s2 = 0xffffffff;
	
	explodeversion (bsv1, &m1, &s1);
	
	explodeversion (bsv2, &m2, &s2);
	
	if (m1 != m2)
		return (setbooleanvalue ((m1 < m2), v));

	if (s1 != s2)
		return (setbooleanvalue ((s1 < s2), v));
	
	return (setbooleanvalue (false, v)); /*they're equal*/
	} /*versionlessthan*/
