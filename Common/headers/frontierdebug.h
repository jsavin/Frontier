
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

/* targets: none, about window, log file, modal dialog, debugger */

#define LAND_LOGTARGET_NONE			0
#define LAND_LOGTARGET_ABOUT		1
#define LAND_LOGTARGET_FILE			2
#define LAND_LOGTARGET_DIALOG		4
#define LAND_LOGTARGET_DEBUGGER		8


/* levels: off, everything, medium, low */

#define LAND_LOGLEVEL_OFF	0
#define LAND_LOGLEVEL_1		1
#define LAND_LOGLEVEL_2		2
#define LAND_LOGLEVEL_3		3


/* configuration for categories */

#define LAND_TCPLOG_NAME	"tcp"
#define LAND_TCPLOG_LEVEL	LAND_LOGLEVEL_OFF
#define LAND_TCPLOG_TARGET	LAND_LOGTARGET_NONE

#define LAND_DBLOG_NAME		"db"
#define LAND_DBLOG_LEVEL	LAND_LOGLEVEL_OFF
#define LAND_DBLOG_TARGET	LAND_LOGTARGET_NONE

#define LAND_THREADSLOG_NAME		"threads"
#define LAND_THREADSLOG_LEVEL		LAND_LOGLEVEL_OFF
#define LAND_THREADSLOG_TARGET		LAND_LOGTARGET_NONE

#define LAND_GENERALLOG_NAME		"general"
#ifndef LAND_GENERALLOG_LEVEL
#define LAND_GENERALLOG_LEVEL		LAND_LOGLEVEL_OFF
#endif
#ifndef LAND_GENERALLOG_TARGET
#define LAND_GENERALLOG_TARGET		LAND_LOGTARGET_NONE
#endif


/* basic definitions */

#define LAND_MSG_LVL(m, t, n, lvl)		logmessage_level ((m), __FILE__, __LINE__, (t), (n), (lvl))
#define LAND_ASSERT(e, t, n)	((void) ((e) ? 0 : (logassert ((#e), __FILE__, __LINE__, (t), (n))))


/* definitions for GENERAL category */

#if (LAND_GENERALLOG_LEVEL >= LAND_LOGLEVEL_1)
	#define MSG_1(x)		LAND_MSG_LVL((x), LAND_GENERALLOG_TARGET, LAND_GENERALLOG_NAME, 1)
	#define ASSERT_1(x)		LAND_ASSERT((x), LAND_GENERALLOG_TARGET, LAND_GENERALLOG_NAME))
#else
	#define MSG_1(x)
	#define ASSERT_1(x)
#endif

#if (LAND_GENERALLOG_LEVEL >= LAND_LOGLEVEL_2)
	#define MSG_2(x)		LAND_MSG_LVL((x), LAND_GENERALLOG_TARGET, LAND_GENERALLOG_NAME, 2)
	#define ASSERT_2(x)		LAND_ASSERT((x), LAND_GENERALLOG_TARGET, LAND_GENERALLOG_NAME))
#else
	#define MSG_2(x)
	#define ASSERT_2(x)
#endif

#if (LAND_GENERALLOG_LEVEL >= LAND_LOGLEVEL_3)
	#define MSG_3(x)		LAND_MSG_LVL((x), LAND_GENERALLOG_TARGET, LAND_GENERALLOG_NAME, 3)
	#define ASSERT_3(x)		LAND_ASSERT((x), LAND_GENERALLOG_TARGET, LAND_GENERALLOG_NAME))
#else
	#define MSG_3(x)
	#define ASSERT_3(x)
#endif


/* definitions for TCP category */

#if (LAND_TCPLOG_LEVEL >= LAND_LOGLEVEL_1)
	#define TCP_MSG_1(x)		LAND_MSG_LVL((x), LAND_TCPLOG_TARGET, LAND_TCPLOG_NAME, 1)
	#define TCP_ASSERT_1(x)		LAND_ASSERT((x), LAND_TCPLOG_TARGET, LAND_TCPLOG_NAME))
#else
	#define TCP_MSG_1(x)
	#define TCP_ASSERT_1(x)
#endif

#if (LAND_TCPLOG_LEVEL >= LAND_LOGLEVEL_2)
	#define TCP_MSG_2(x)		LAND_MSG_LVL((x), LAND_TCPLOG_TARGET, LAND_TCPLOG_NAME, 2)
	#define TCP_ASSERT_2(x)		LAND_ASSERT((x), LAND_TCPLOG_TARGET, LAND_TCPLOG_NAME))
#else
	#define TCP_MSG_2(x)
	#define TCP_ASSERT_2(x)
#endif

#if (LAND_TCPLOG_LEVEL >= LAND_LOGLEVEL_3)
	#define TCP_MSG_3(x)		LAND_MSG_LVL((x), LAND_TCPLOG_TARGET, LAND_TCPLOG_NAME, 3)
	#define TCP_ASSERT_3(x)		LAND_ASSERT((x), LAND_TCPLOG_TARGET, LAND_TCPLOG_NAME))
#else
	#define TCP_MSG_3(x)
	#define TCP_ASSERT_3(x)
#endif


/* definitions for DB category */

#if (LAND_DBLOG_LEVEL >= LAND_LOGLEVEL_1)
	#define DB_MSG_1(x)		LAND_MSG_LVL((x), LAND_DBLOG_TARGET, LAND_DBLOG_NAME, 1)
	#define DB_ASSERT_1(x)		LAND_ASSERT((x), LAND_DBLOG_TARGET, LAND_DBLOG_NAME))
#else
	#define DB_MSG_1(x)
	#define DB_ASSERT_1(x)
#endif

#if (LAND_DBLOG_LEVEL >= LAND_LOGLEVEL_2)
	#define DB_MSG_2(x)		LAND_MSG_LVL((x), LAND_DBLOG_TARGET, LAND_DBLOG_NAME, 2)
	#define DB_ASSERT_2(x)		LAND_ASSERT((x), LAND_DBLOG_TARGET, LAND_DBLOG_NAME))
#else
	#define DB_MSG_2(x)
	#define DB_ASSERT_2(x)
#endif

#if (LAND_DBLOG_LEVEL >= LAND_LOGLEVEL_3)
	#define DB_MSG_3(x)		LAND_MSG_LVL((x), LAND_DBLOG_TARGET, LAND_DBLOG_NAME, 3)
	#define DB_ASSERT_3(x)		LAND_ASSERT((x), LAND_DBLOG_TARGET, LAND_DBLOG_NAME))
#else
	#define DB_MSG_3(x)
	#define DB_ASSERT_3(x)
#endif


/* definitions for THREADS category */

#if (LAND_THREADSLOG_LEVEL >= LAND_LOGLEVEL_1)
	#define THREADS_MSG_1(x)		LAND_MSG_LVL((x), LAND_THREADSLOG_TARGET, LAND_THREADSLOG_NAME, 1)
	#define THREADS_ASSERT_1(x)		LAND_ASSERT((x), LAND_THREADSLOG_TARGET, LAND_THREADSLOG_NAME))
#else
	#define THREADS_MSG_1(x)
	#define THREADS_ASSERT_1(x)
#endif

#if (LAND_THREADSLOG_LEVEL >= LAND_LOGLEVEL_2)
	#define THREADS_MSG_2(x)		LAND_MSG_LVL((x), LAND_THREADSLOG_TARGET, LAND_THREADSLOG_NAME, 2)
	#define THREADS_ASSERT_2(x)		LAND_ASSERT((x), LAND_THREADSLOG_TARGET, LAND_THREADSLOG_NAME))
#else
	#define THREADS_MSG_2(x)
	#define THREADS_ASSERT_2(x)
#endif

#if (LAND_THREADSLOG_LEVEL >= LAND_LOGLEVEL_3)
	#define THREADS_MSG_3(x)		LAND_MSG_LVL((x), LAND_THREADSLOG_TARGET, LAND_THREADSLOG_NAME, 3)
	#define THREADS_ASSERT_3(x)		LAND_ASSERT((x), LAND_THREADSLOG_TARGET, LAND_THREADSLOG_NAME))
#else
	#define THREADS_MSG_3(x)
	#define THREADS_ASSERT_3(x)
#endif


/* function templates */

extern void logmessage_level (char *, char *, long, long, char *, int);

extern long logassert (char *, char *, long, long, char *);

extern void logstartup (void);

extern void logshutdown (void);
