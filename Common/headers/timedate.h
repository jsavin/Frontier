
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

#ifndef timedateinclude
#define timedateinclude

#include <stdint.h>  /* for int64_t in frontier_time_t typedef */

/*
 * 2025-12-28 Codex: Forward-declare bigstring if not yet defined.
 * Avoid including standard.h to prevent circular dependencies.
 * bigstring is defined as Str255 in standard.h or standard_portable.h.
 */
#ifndef bigstring
typedef unsigned char bigstring[256];
#endif

#ifndef boolean
typedef unsigned char boolean;
#endif

#pragma pack(2)
typedef struct tyinternationalinfo {
	char * longDaysOfWeek[10];
	char * shortDaysOfWeek[10];
	char * longMonths[13];
	char * shortMonths[13];
	char * longYears[12];		//Year descriptions
	char * morning;
	char * evening;
	char * military;
	char * currency;			//for the US this is $
	char * intlCurrency;		//for the US this is USD
	char * shortDateFormatPattern;
	char * longDateFormatPattern;
	char * decimal;
	char * thousands;
	char * list;
	char * timesep;
	char * datesep;
	short numberOfDays;			//Usally 7 plus 3 for Yesterday, Today, and Tomorrow
	short numberOfMonths;
	short daysInMonth[13];		//For Gregorian calendar February is listed as 28 and corrected in code.
	short numberOfYears;
	boolean defaultTimeFormat;	//false = 12hour
	} tyinternationalinfo, * tyinternationalinfoptr;
#pragma options align=reset


/* #define getlongermilliseconds() (unsigned long long)FastMilliseconds() */
#define getmilliseconds() (long)FastMilliseconds()

/*
 * Frontier epoch offset: seconds from Unix epoch (1970-01-01) to Mac epoch (1904-01-01)
 * = 66 years * 365.25 days/year * 24 hours/day * 3600 seconds/hour
 * = 2,082,844,800 seconds
 */
#define FRONTIER_EPOCH_TO_UNIX_OFFSET 2082844800LL

/*
 * Frontier time type: 64-bit signed integer representing seconds since 1904-01-01 00:00:00 UTC
 * This ensures portability across platforms (time_t can be 32-bit or 64-bit).
 * Range: Effectively unlimited (±292 billion years from 1904).
 *
 * Documentation:
 * - docs/frontier_time_t_standard.md (developer guide for using frontier_time_t)
 * - planning/phase3/date_time_format_standard.md (architectural decision context)
 */
typedef int64_t frontier_time_t;

/*prototypes*/

extern void timestamp (long *);

extern unsigned long timenow (void);

/*
 * Get current time as 64-bit Frontier timestamp (seconds since 1904-01-01 00:00:00 UTC)
 * This is the recommended function for all new code that stores timestamps.
 * Returns frontier_time_t (int64_t) for cross-platform portability.
 *
 * Use this instead of time(NULL) + manual epoch conversion to ensure boundary
 * conversions happen in one place (the timedate module).
 *
 * See: docs/frontier_time_t_standard.md
 */
extern frontier_time_t timenow64 (void);

extern boolean setsystemclock (unsigned long);

extern boolean timegreaterthan (unsigned long, unsigned long);

extern boolean timelessthan (unsigned long, unsigned long);

extern boolean timetotimestring (int64_t, bigstring, boolean);

extern boolean timetodatestring (int64_t, bigstring, boolean);

extern boolean stringtotime (bigstring, unsigned long *);

extern int64_t datetimetoseconds (short, short, short, short, short, short);

extern void secondstodatetime (int64_t, short *, short *, short *, short *, short *, short *);

extern void secondstodayofweek (int64_t, short *);


extern unsigned long nextmonth(unsigned long date);

extern unsigned long nextyear(unsigned long date);

extern unsigned long prevmonth(unsigned long date);

extern unsigned long prevyear(unsigned long date);

extern unsigned long firstofmonth(unsigned long date);

extern unsigned long lastofmonth(unsigned long date);

extern short daysInMonth (short month, short year);

extern void shortdatestring (unsigned long date, bigstring bs);

extern void longdatestring (unsigned long date, bigstring bs);

extern void abbrevdatestring (unsigned long date, bigstring bs);

extern void getdaystring (short dayofweek, bigstring bs, boolean flFullname);

extern long getcurrenttimezonebias(void);

extern boolean isLeapYear (short year);


#endif /*timedateinclude*/
