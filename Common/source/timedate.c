
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

#if defined(FRONTIER_HEADLESS) && !defined(_WIN32)
#define _GNU_SOURCE  /* For timegm() on Linux */
#endif

#include "frontier.h"
#include "standard.h"

#include "error.h"
#include "memory.h"
#include "strings.h"
#include "ops.h"
#include "langinternal.h"
#include "shell.h"

#include "timedate.h"

#ifndef FRONTIER_HEADLESS
	#include "MacDateHelpers.h"
	#define tydate DateTimeRec
#else
	/* Define tydate for headless builds */
	#define tydate tydaterec
#endif

#if defined(FRONTIER_HEADLESS)
#include <time.h>
#endif



typedef struct tyValidationOrder {
	short item;
	short type;
} tyValidationOrder, * tyValidationOrderPtr;

typedef unsigned long tyCharacterAttributes;


static short daysInMonthsArray[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

#define DateTimeSet_kDayOfWeek		0x0100
#define DateTimeSet_kYear			0x0040
#define DateTimeSet_kMonth			0x0020
#define DateTimeSet_kDay			0x0010
#define DateTimeSet_kHour			0x0008
#define DateTimeSet_kMinute			0x0004
#define DateTimeSet_kSecond			0x0002
#define DateTimeSet_kHundredth		0x0001

#define DurationUnit_kDays			0x0010

#define err_kTooManyNumberFields	20000
#define err_kTooFewValues			20001
#define err_kOutOfOrder				20002
#define err_kItemUsed				20003
#define err_kInvalidPunctuation		20004
#define err_kHundredthValueInvalid  20005
#define err_kSecondValueInvalid		20006
#define err_kMinuteValueInvalid		20007
#define err_kHourValueInvalid		20008
#define err_kAMorPMon24HourClock	20009

#define CharAttr_kShortDate			0x00000001
#define CharAttr_kLongDate			0x00000002
#define CharAttr_kTime				0x00000004

#define SCAN_DOW	1
#define SCAN_MONTH	2
#define SCAN_YEAR	4

#define VALTYPE_NONE		0
#define VALTYPE_KEYMONTHNAME1	1
#define VALTYPE_KEYMONTHNAME2	2
#define VALTYPE_KEYDAYNAME1	3
#define VALTYPE_KEYDAYNAME2	4
#define VALTYPE_KEYYEARNAME	5
#define VALTYPE_KEYTIMEAM	6
#define VALTYPE_KEYTIMEPM	7
#define VALTYPE_KEYTIME24	8
#define VALTYPE_CURRENCY	9
#define VALTYPE_BLANK	       10
#define VALTYPE_NUMERIC        11
#define VALTYPE_PUNC	       12
#define VALTYPE_NONNUMERIC     13

#define VALORDER_NUMERIC	1
#define VALORDER_FIXEDNUMERIC	2
#define VALORDER_KEYWORD	3

#define VALFLAG_ISDOLLARS	(0x1000)
#define VALFLAG_ISCENTS 	(0x2000)

#pragma pack(2)
typedef struct tyvalidationtoken
   {
   short ty;
   char * pos;
   short count;
   long val;
   unsigned long flag;
   } tyvalidationtoken, * tyvalidationtokenptr;
#pragma options align=reset



#pragma pack(2)
typedef struct tydaterec {
	short year;
	short month;
	short dayOfWeek;
	short day;
	short hour;
	short minute;
	short second;
	short hundredths;
	} tydaterec, * tydaterecptr;

typedef struct tyvalidationerror {
	long errorNumber;
	long stringPosition;
	char * auxilaryPointer;
	} tyvalidationerror, * tyvalidationerrorptr;
#pragma options align=reset



boolean isLeapYear (short year) {
	/* this procedure assumes the existance of the Gregorian calendar only */
	/* This basically means it's not valid for years before the adoption of
	   the Gregorian calendar.  This adoption varies by country, starting in
	   Europe as early as the 1400's and as late as the 1900's in Russia. */

	boolean res;

	res = false;

	if ((year % 4) == 0) {
		res = true;

		if ((year % 100) == 0) {
			res = false;

			if ((year % 400) == 0) {
				res = true;
				}
			}
		}

	return (res);
	} /*isLeapYear*/


short daysInMonth (short month, short year) {
	short res;

	/*this procedure assumes month valid from 1 to 12*/

	res = daysInMonthsArray [month-1];

	if (month == 2) {
		if (isLeapYear (year))
			++res;
		}

	return (res);
	} /* daysInMonth */





void timestamp (long *ptime) {

    #if defined(FRONTIER_HEADLESS)
        /* Portable implementation: Use time() + epoch conversion */
        time_t unix_time = time(NULL);
        *ptime = (long)(unix_time + FRONTIER_EPOCH_TO_UNIX_OFFSET);
    #else
        *ptime = (CFAbsoluteTimeGetCurrent() + kCFAbsoluteTimeIntervalSince1904);
    #endif

	} /*timestamp*/
	
	
unsigned long timenow (void) {

	/*
	2.1b4 dmb; more convenient than timestamp for most callers
	*/

    #if defined(FRONTIER_HEADLESS)
        /* Portable implementation: Use time() + epoch conversion */
        time_t unix_time = time(NULL);
        return (unsigned long)(unix_time + FRONTIER_EPOCH_TO_UNIX_OFFSET);
    #else
        unsigned long now = (CFAbsoluteTimeGetCurrent() + kCFAbsoluteTimeIntervalSince1904);
        return (now);
    #endif
	} /*timenow*/


frontier_time_t timenow64 (void) {

	/*
	Get current time as frontier_time_t (64-bit timestamp since 1904).
	This is the recommended function for all new code that stores timestamps.

	For FRONTIER_HEADLESS builds: Use portable time() + epoch conversion
	For macOS builds: Use CFAbsoluteTimeGetCurrent() which already uses 1904 epoch

	Returns: Seconds since 1904-01-01 00:00:00 UTC as int64_t

	See: docs/frontier_time_t_standard.md
	*/

	#if defined(FRONTIER_HEADLESS)
		/* Portable implementation: Unix time() + epoch conversion */
		time_t unix_time = time(NULL);
		return (frontier_time_t)unix_time + FRONTIER_EPOCH_TO_UNIX_OFFSET;
	#else
		/* macOS implementation: Use CoreFoundation */
		return (frontier_time_t)(CFAbsoluteTimeGetCurrent() + kCFAbsoluteTimeIntervalSince1904);
	#endif
	} /*timenow64*/


boolean setsystemclock (unsigned long secs) {

	/*
	3/10/97 dmb: set the system clock, using Macintosh time
	conventions (seconds since 12:00 PM Jan 1, 1904)
	*/

    #if defined(FRONTIER_HEADLESS)
        /* Headless mode doesn't support setting system clock */
        (void)secs;  /* Suppress unused parameter warning */
        return false;
    #else
        return (!oserror(unsupportedOSErr));
    #endif

	} /*setsystemclock*/


static void
adjustforcurrenttimezone (unsigned long *ptime)
{
	/*
	5.1b23 dmb: avoid wraparound for near-zero dates
	*/

	unsigned long adjustedtime = *ptime + getcurrenttimezonebias ();

	if (sgn (*ptime) == sgn (adjustedtime))
		*ptime = adjustedtime;
	} /*adjustforcurrenttimezone*/


boolean
timegreaterthan (
		unsigned long time1,
		unsigned long time2)
{
	return (time1 > time2);
	} /*timegreaterthan*/


boolean
timelessthan (
		unsigned long time1,
		unsigned long time2)
{
	return (time1 < time2);
} /*timelessthan*/


boolean timetotimestring (int64_t ptime, bigstring bstime, boolean flwantseconds) {

        #if defined(FRONTIER_HEADLESS)
        /*
        2025-12-08 Codex: Headless fallback without CFDateFormatter.
        Match classic output: "M/D/YYYY; H:MM:SS AM" when seconds requested,
        otherwise "H:MM AM".
        */
        const int64_t frontier_epoch_offset = 2082844800LL; /* seconds between 1904 and 1970 */
        time_t unix_secs = (ptime > frontier_epoch_offset) ? (time_t) (ptime - frontier_epoch_offset) : (time_t) 0;
        struct tm tmbuf;
        if (localtime_r(&unix_secs, &tmbuf) == NULL) {
            setemptystring(bstime);
            return true;
        }
        int hour12 = tmbuf.tm_hour % 12;
        if (hour12 == 0)
            hour12 = 12;
        const char *ampm = (tmbuf.tm_hour >= 12) ? "PM" : "AM";
        char buf[32];
        if (flwantseconds)
            snprintf(buf, sizeof(buf), "%d:%02d:%02d %s", hour12, tmbuf.tm_min, tmbuf.tm_sec, ampm);
        else
            snprintf(buf, sizeof(buf), "%d:%02d %s", hour12, tmbuf.tm_min, ampm);
        copyctopstring(buf, bstime);
        return true;
        #else
        CFLocaleRef locale = CFLocaleCopyCurrent();
        CFDateFormatterRef timeFormatter = CFDateFormatterCreate(kCFAllocatorDefault, locale, kCFDateFormatterNoStyle, flwantseconds ? kCFDateFormatterMediumStyle : kCFDateFormatterShortStyle);
        CFTimeInterval at = (CFTimeInterval)((double) ptime - (double) kCFAbsoluteTimeIntervalSince1904);
        CFStringRef timeString = CFDateFormatterCreateStringWithAbsoluteTime(kCFAllocatorDefault, timeFormatter, at);
        CFStringGetPascalString(timeString, bstime, sizeof(bigstring), kCFStringEncodingMacRoman);
        CFRelease(timeString);
        CFRelease(timeFormatter);
        CFRelease(locale);

        return (true);
        #endif

	} /*timetotimestring*/


boolean timetodatestring (int64_t ptime, bigstring bsdate, boolean flabbreviate) {

        #if defined(FRONTIER_HEADLESS)
        /*
        2025-12-08 Codex: Headless fallback without CFDateFormatter – format Mac-epoch seconds
        as M/D/YYYY (non-padded month/day) to mirror classic output.
        */
        const int64_t frontier_epoch_offset = 2082844800LL; /* seconds between 1904 and 1970 */
        time_t unix_secs = (ptime > frontier_epoch_offset) ? (time_t) (ptime - frontier_epoch_offset) : (time_t) 0;
        struct tm tmbuf;
        if (localtime_r(&unix_secs, &tmbuf) == NULL) {
            setemptystring(bsdate);
            return true;
        }
        int month = tmbuf.tm_mon + 1;
        int day = tmbuf.tm_mday;
        int year = tmbuf.tm_year + 1900;
        char buf[32];
        snprintf(buf, sizeof(buf), "%d/%d/%04d", month, day, year);
        copyctopstring(buf, bsdate);
        return true;
        #else
        CFLocaleRef locale = CFLocaleCopyCurrent();
        CFDateFormatterRef formatter = CFDateFormatterCreate(kCFAllocatorDefault, locale, flabbreviate ? kCFDateFormatterMediumStyle : kCFDateFormatterShortStyle, kCFDateFormatterNoStyle);
        CFTimeInterval at = (CFTimeInterval)((double) ptime - (double) kCFAbsoluteTimeIntervalSince1904);
        CFStringRef dateString = CFDateFormatterCreateStringWithAbsoluteTime(kCFAllocatorDefault, formatter, at);
        
        CFStringGetPascalString(dateString, bsdate, sizeof(bigstring), kCFStringEncodingMacRoman);
        
        CFRelease(dateString);
        CFRelease(formatter);
        CFRelease(locale);

        return (true);
        #endif

	} /*timetodatestring*/


boolean stringtotime (bigstring bsdate, unsigned long *ptime) {

	/*
	9/13/91 dmb: use the script manager to translate a string to a
	time in seconds since 12:00 AM 1904.

	if a date is provided, but no time, the time is 12:00 AM

	if a time is provided, but no date, the date is 1/1/1904.

	we return true if any time/date information was extracted.
	*/

    #if defined(FRONTIER_HEADLESS)
        /* Headless implementation: minimal stub */
        (void)bsdate;  /* Suppress unused parameter warning */
        *ptime = 0;
        return false;  /* TODO: Implement proper date parsing for headless mode */
    #else
        boolean flUseGMT = false;
        long idx;

        idx = stringlength (bsdate);

        while (getstringcharacter(bsdate, idx-1) == ' ')
            --idx;

        if (idx > 3) {
            if ((getstringcharacter(bsdate, idx - 3) == 'G') && (getstringcharacter(bsdate, idx - 2) == 'M') && (getstringcharacter(bsdate, idx -1) == 'T')) {
                flUseGMT = true;
            }
        }

        *ptime = 0; /*default return value*/


        CFStringRef dateString = CFStringCreateWithPascalString(kCFAllocatorDefault, bsdate, kCFStringEncodingMacRoman);
        CFAbsoluteTime absoluteTime = stringToDate(dateString);
        CFRelease(dateString);
        if (absoluteTime > 0) {
            *ptime = (absoluteTime + kCFAbsoluteTimeIntervalSince1904);
            if (flUseGMT) {
                adjustforcurrenttimezone (ptime);
            }
            return true;
        } else {
            return false;
        }
    #endif
	} /*stringtotime*/


int64_t datetimetoseconds (short day, short month, short year, short hour, short minute, short second) {

	/*
	5.0a12 dmb: Win version, must handle hour, minute, second wraparound
	2025-12-15 Codex: Portable implementation using standard C library
	2025-12-15: Migrated return type from long to int64_t for Y2040 safety
	*/

    #if defined(FRONTIER_HEADLESS)
        /* Portable implementation using timegm/mktime */
        struct tm t;
        memset(&t, 0, sizeof(t));
        t.tm_mday = day;
        t.tm_mon = month - 1;  /* tm_mon is 0-based */
        t.tm_year = year - 1900;  /* tm_year is years since 1900 */
        t.tm_hour = hour;
        t.tm_min = minute;
        t.tm_sec = second;
        t.tm_isdst = -1;  /* Ignored by timegm/mkgmtime (always interprets as UTC) */

        /* Use timegm for UTC time (portable on Unix/Linux/macOS) */
        #if defined(_WIN32)
        time_t unix_secs = _mkgmtime(&t);
        #else
        time_t unix_secs = timegm(&t);
        #endif

        if (unix_secs == (time_t)-1)
            return 0;

        /* Convert from Unix epoch (1970) to Mac epoch (1904) */
        return (int64_t)(unix_secs + FRONTIER_EPOCH_TO_UNIX_OFFSET);
    #else
        unsigned long secs = convertDateTimeToSeconds(day, month, year, hour, minute, second) + kCFAbsoluteTimeIntervalSince1904;
        return (int64_t)secs;
    #endif
	} /*datetimetoseconds*/


void secondstodatetime (int64_t secs, short *day, short *month, short *year, short *hour, short *minute, short *second) {

    #if defined(FRONTIER_HEADLESS)
        /* Portable implementation using gmtime_r */
        time_t unix_secs = (secs > FRONTIER_EPOCH_TO_UNIX_OFFSET) ? (time_t)(secs - FRONTIER_EPOCH_TO_UNIX_OFFSET) : (time_t)0;

        struct tm tmbuf;
        #if defined(_WIN32)
        if (gmtime_s(&tmbuf, &unix_secs) != 0) {
            if (day) *day = 0;
            if (month) *month = 0;
            if (year) *year = 0;
            if (hour) *hour = 0;
            if (minute) *minute = 0;
            if (second) *second = 0;
            return;
        }
        #else
        if (gmtime_r(&unix_secs, &tmbuf) == NULL) {
            if (day) *day = 0;
            if (month) *month = 0;
            if (year) *year = 0;
            if (hour) *hour = 0;
            if (minute) *minute = 0;
            if (second) *second = 0;
            return;
        }
        #endif

        if (day) *day = tmbuf.tm_mday;
        if (month) *month = tmbuf.tm_mon + 1;  /* tm_mon is 0-based */
        if (year) *year = tmbuf.tm_year + 1900;
        if (hour) *hour = tmbuf.tm_hour;
        if (minute) *minute = tmbuf.tm_min;
        if (second) *second = tmbuf.tm_sec;
    #else
        CFAbsoluteTime timeInterval = ((uint32_t) secs) - kCFAbsoluteTimeIntervalSince1904;
        convertSecondsToDateTime(timeInterval, day, month, year, hour, minute, second);
    #endif
	} /*secondstodatetime*/


void secondstodayofweek (int64_t secs, short *dayofweek) {

    #if defined(FRONTIER_HEADLESS)
        /* Portable implementation using gmtime_r */
        time_t unix_secs = (secs > FRONTIER_EPOCH_TO_UNIX_OFFSET) ? (time_t)(secs - FRONTIER_EPOCH_TO_UNIX_OFFSET) : (time_t)0;

        struct tm tmbuf;
        #if defined(_WIN32)
        if (gmtime_s(&tmbuf, &unix_secs) != 0) {
            if (dayofweek) *dayofweek = 1;  /* Default to Sunday */
            return;
        }
        #else
        if (gmtime_r(&unix_secs, &tmbuf) == NULL) {
            if (dayofweek) *dayofweek = 1;  /* Default to Sunday */
            return;
        }
        #endif

        /* tm_wday: 0=Sunday, 1=Monday, ..., 6=Saturday */
        /* Frontier uses: 1=Sunday, 2=Monday, ..., 7=Saturday */
        if (dayofweek) *dayofweek = tmbuf.tm_wday + 1;
    #else
        CFAbsoluteTime timeInterval = ((uint32_t)secs) - kCFAbsoluteTimeIntervalSince1904;
        *dayofweek = convertSecondsToDayOfWeek(timeInterval);
    #endif
	} /*secondstodayofweek*/


static void fixdate (tydate * date) {
		date->minute = date->minute + (date->second / 60);
		date->second = date->second % 60;
		date->hour = date->hour + (date->minute / 60);
		date->minute = date->minute % 60;
		date->day = date->day + (date->hour / 24);
		date->hour = date->hour % 24;

		/* we pre-adjust the month to ensure that it is in a valid range */
		date->year = date->year + ((date->month - 1) / 12);
		date->month = ((date->month - 1) % 12) + 1;

		while (date->day > daysInMonth(date->month, date->year)) {
			date->day = date->day - daysInMonth(date->month, date->year);

			if (date->month < 12) {
				++(date->month);
				}
			else {
				date->month = 1;
				++(date->year);
				}
			}

	} /*fixdate*/


unsigned long nextmonth(unsigned long date) {

	/*
	6.0a10 dmb: limit day to max days for new month
	*/

    #if defined(FRONTIER_HEADLESS)
        /* Headless implementation: Add approximately 30 days (simplified) */
        /* TODO: Implement proper month arithmetic for headless mode */
        return date + (30 * 24 * 60 * 60);
    #else
        CFAbsoluteTime timeInterval = date - kCFAbsoluteTimeIntervalSince1904;
        CFAbsoluteTime incrementedTime = incrementDateByMonth(timeInterval, 1);
        return (incrementedTime + kCFAbsoluteTimeIntervalSince1904);
    #endif

	} /*nextmonth*/

unsigned long nextyear(unsigned long date) {

    #if defined(FRONTIER_HEADLESS)
        /* Headless implementation: Add 365 days (simplified, ignores leap years) */
        /* TODO: Implement proper year arithmetic for headless mode */
        return date + (365 * 24 * 60 * 60);
    #else
        CFAbsoluteTime timeInterval = date - kCFAbsoluteTimeIntervalSince1904;
        CFAbsoluteTime incrementedTime = incrementDateByYear(timeInterval, 1);
        return (incrementedTime + kCFAbsoluteTimeIntervalSince1904);
    #endif

	} /*nextyear*/

unsigned long prevmonth(unsigned long date) {

	/*
	6.0a10 dmb: limit day to max days for new month
	*/

    #if defined(FRONTIER_HEADLESS)
        /* Headless implementation: Subtract approximately 30 days (simplified) */
        /* TODO: Implement proper month arithmetic for headless mode */
        return date - (30 * 24 * 60 * 60);
    #else
        CFAbsoluteTime timeInterval = date - kCFAbsoluteTimeIntervalSince1904;
        CFAbsoluteTime incrementedTime = incrementDateByMonth(timeInterval, -1);
        return (incrementedTime + kCFAbsoluteTimeIntervalSince1904);
    #endif

	} /*prevmonth*/

unsigned long prevyear(unsigned long date) {

    #if defined(FRONTIER_HEADLESS)
        /* Headless implementation: Subtract 365 days (simplified, ignores leap years) */
        /* TODO: Implement proper year arithmetic for headless mode */
        return date - (365 * 24 * 60 * 60);
    #else
        CFAbsoluteTime timeInterval = date - kCFAbsoluteTimeIntervalSince1904;
        CFAbsoluteTime incrementedTime = incrementDateByYear(timeInterval, 1);
        return (incrementedTime + kCFAbsoluteTimeIntervalSince1904);
    #endif

	} /*prevyear*/

unsigned long firstofmonth(unsigned long date) {

    #if defined(FRONTIER_HEADLESS)
        /* Headless implementation: Stub - return the same date */
        /* TODO: Implement proper first-of-month calculation for headless mode */
        return date;
    #else
        CFAbsoluteTime timeInterval = date - kCFAbsoluteTimeIntervalSince1904;
        CFAbsoluteTime newTime = getFirstDayOfMonth(timeInterval);
        return newTime + kCFAbsoluteTimeIntervalSince1904;
    #endif

	} /*firstofmonth*/

unsigned long lastofmonth(unsigned long date) {

    #if defined(FRONTIER_HEADLESS)
        /* Headless implementation: Stub - return the same date */
        /* TODO: Implement proper last-of-month calculation for headless mode */
        return date;
    #else
        CFAbsoluteTime timeInterval = date - kCFAbsoluteTimeIntervalSince1904;
        CFAbsoluteTime newTime = getLastDayOfMonth(timeInterval);
        return newTime + kCFAbsoluteTimeIntervalSince1904;
    #endif

	} /*lastofmonth*/


#ifndef FRONTIER_HEADLESS
#define DATE_STRING(date, bs, format) \
    CFLocaleRef locale = CFLocaleCopyCurrent();\
    CFDateFormatterRef formatter = CFDateFormatterCreate(kCFAllocatorDefault, locale, format, kCFDateFormatterNoStyle);\
    CFAbsoluteTime timeInterval = date - kCFAbsoluteTimeIntervalSince1904;\
    CFStringRef dateString = CFDateFormatterCreateStringWithAbsoluteTime(kCFAllocatorDefault, formatter, timeInterval);\
    CFStringGetPascalString(dateString, bs, sizeof(bigstring), kCFStringEncodingMacRoman);\
    CFRelease(dateString);\
    CFRelease(formatter);\
    CFRelease(locale);
#endif


void shortdatestring (unsigned long date, bigstring bs) {
    #if defined(FRONTIER_HEADLESS)
        /* Headless implementation: Use timetodatestring */
        timetodatestring((int64_t)date, bs, false);
    #else
        DATE_STRING(date, bs, kCFDateFormatterShortStyle)
    #endif
	} /*shortdatestring*/

void longdatestring (unsigned long date, bigstring bs) {
    #if defined(FRONTIER_HEADLESS)
        /* Headless implementation: Use timetodatestring */
        timetodatestring((int64_t)date, bs, true);
    #else
        DATE_STRING(date, bs, kCFDateFormatterLongStyle)
    #endif
	} /*longdatestring*/

void abbrevdatestring (unsigned long date, bigstring bs) {
    #if defined(FRONTIER_HEADLESS)
        /* Headless implementation: Use timetodatestring */
        timetodatestring((int64_t)date, bs, true);
    #else
        DATE_STRING(date, bs, kCFDateFormatterMediumStyle)
    #endif
	} /*abbrevdatestring*/

void getdaystring (short dayofweek, bigstring bs, boolean flFullname) {
		switch (dayofweek) {
			case 1:
				copyctopstring ("Sunday", bs);
				break;

			case 2:
				copyctopstring ("Monday", bs);
				break;

			case 3:
				copyctopstring ("Tuesday", bs);
				break;

			case 4:
				copyctopstring ("Wednesday", bs);
				break;

			case 5:
				copyctopstring ("Thursday", bs);
				break;

			case 6:
				copyctopstring ("Friday", bs);
				break;

			case 7:
				copyctopstring ("Saturday", bs);
				break;

			default:
				copyctopstring ("Day of week number is invalid (not between 1 and 7)", bs);
				break;
			}

		if (! flFullname)
			setstringlength (bs, 3);

	} /*getdaystring*/

long getcurrenttimezonebias(void) {

    #if defined(FRONTIER_HEADLESS)
        /* Headless implementation: Return 0 (UTC) */
        /* TODO: Implement proper timezone detection for headless mode */
        return 0;
    #else
        MachineLocation ml;
        long res;

        ReadLocation (&ml);

        res = ml.u.gmtDelta & 0x00FFFFFF;

        if ((res & 0x00800000) == 0x00800000)  //if this is a negative number extend the sign bits
            res = res | 0xFF000000;

        return (res);
    #endif

	} /*getcurrenttimezonebias*/
