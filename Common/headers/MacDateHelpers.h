/*    $Id$    */

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

#ifndef MacDateHelpers_h
#define MacDateHelpers_h

#include <CoreFoundation/CoreFoundation.h>

CFAbsoluteTime stringToTime(CFStringRef dateString);
CFAbsoluteTime stringToDateTime(CFStringRef dateString, CFDateFormatterStyle dateStyle);
CFAbsoluteTime stringToDate(CFStringRef dateString);
CFAbsoluteTime convertDateTimeToSeconds(int16_t day, int16_t month, int16_t year, int16_t hour, int16_t minute, int16_t second);
void convertSecondsToDateTime(CFAbsoluteTime secs, int16_t *day, int16_t *month, int16_t *year, int16_t *hour, int16_t *minute, int16_t *second);
int16_t convertSecondsToDayOfWeek(CFAbsoluteTime secs);
CFAbsoluteTime incrementDateByDay(CFAbsoluteTime time, int16_t days);
CFAbsoluteTime incrementDateByMonth(CFAbsoluteTime time, int16_t months);
CFAbsoluteTime incrementDateByYear(CFAbsoluteTime time, int16_t years);
CFAbsoluteTime getFirstDayOfMonth(CFAbsoluteTime time);
CFAbsoluteTime getLastDayOfMonth(CFAbsoluteTime time);

#endif /* MacDateHelpers_h */
