
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

#include "about.h"
#include <stdlib.h>
#include <string.h>
#include "frontierdebug.h"
#include "process.h"
#include "processinternal.h"
#include "strings.h"


static const char *debuglogname = "frontierdebuglog.txt";

static FILE * logfile = nil;

static boolean flreentering = false;

static int g_runtime_inited = 0;
static int g_general_level_rt = -1; /* -1: unset; 0..3: threshold */
static long g_general_target_rt = -1; /* -1: unset; else LAND_LOGTARGET_* bitmask */

static void init_runtime_logging_once(void) {
    if (g_runtime_inited)
        return;
    g_runtime_inited = 1;

    const char *lvl = getenv("FRONTIER_LOG_LEVEL");
    if (lvl && *lvl) {
        /* accept numbers 0..3 */
        char *endp = NULL;
        long v = strtol(lvl, &endp, 10);
        if (endp != lvl && v >= 0 && v <= 3) {
            g_general_level_rt = (int)v;
        }
    }

    const char *target = getenv("FRONTIER_LOG_TARGET");
    if (target && *target) {
        /* comma-separated: file,about,dialog,debugger,none */
        long mask = 0;
        const char *p = target;
        while (*p) {
            while (*p == ' ' || *p == '\t' || *p == ',') p++;
            const char *start = p;
            while (*p && *p != ',') p++;
            size_t n = (size_t)(p - start);
            if (n == 4 && strncmp(start, "none", 4) == 0) { mask = 0; }
            else if (n == 4 && strncmp(start, "file", 4) == 0) { mask |= LAND_LOGTARGET_FILE; }
            else if (n == 5 && strncmp(start, "about", 5) == 0) { mask |= LAND_LOGTARGET_ABOUT; }
            else if (n == 6 && strncmp(start, "dialog", 6) == 0) { mask |= LAND_LOGTARGET_DIALOG; }
            else if (n == 8 && strncmp(start, "debugger", 8) == 0) { mask |= LAND_LOGTARGET_DEBUGGER; }
            else { /* ignore unknown */ }
        }
        g_general_target_rt = mask;
    }

    const char *file = getenv("FRONTIER_LOG_FILE");
    if (file && *file) {
        debuglogname = file;
        if (logfile != NULL) {
            fclose(logfile);
            logfile = NULL;
        }
    }
}


/* compiler messages for GENERAL category */

#if (LAND_GENERALLOG_LEVEL >= LAND_LOGLEVEL_3)
	#pragma message ("*********************** GENERAL LOG: Level 3")
#elif (LAND_GENERALLOG_LEVEL >= LAND_LOGLEVEL_2)
	#pragma message ("*********************** GENERAL LOG: Level 2")
#elif (LAND_GENERALLOG_LEVEL >= LAND_LOGLEVEL_1)
	#pragma message ("*********************** GENERAL LOG: Level 1")
/*
#else
	#pragma message ("*********************** GENERAL LOG: --OFF--")
*/
#endif

#if (LAND_GENERALLOG_LEVEL > LAND_LOGLEVEL_OFF)
	#if (LAND_GENERALLOG_TARGET & LAND_LOGTARGET_FILE)
		#pragma message ("*********************** GENERAL LOG: output to FILE")
	#endif
	#if (LAND_GENERALLOG_TARGET & LAND_LOGTARGET_ABOUT)
		#pragma message ("*********************** GENERAL LOG: output to ABOUT WINDOW")
	#endif
	#if (LAND_GENERALLOG_TARGET & LAND_LOGTARGET_DIALOG)
		#pragma message ("*********************** GENERAL LOG: output to DIALOG")
	#endif
	#if (LAND_GENERALLOG_TARGET & LAND_LOGTARGET_DEBUGGER)
		#pragma message ("*********************** GENERAL LOG: output to DEBUGGER")
	#endif
#endif


/* compiler messages for TCP category */

#if (LAND_TCPLOG_LEVEL >= LAND_LOGLEVEL_3)
	#pragma message ("*********************** TCP LOG: Level 3")
#elif (LAND_TCPLOG_LEVEL >= LAND_LOGLEVEL_2)
	#pragma message ("*********************** TCP LOG: Level 2")
#elif (LAND_TCPLOG_LEVEL >= LAND_LOGLEVEL_1)
	#pragma message ("*********************** TCP LOG: Level 1")
/*
#else
	#pragma message ("*********************** TCP LOG: --OFF--")
*/
#endif

#if (LAND_TCPLOG_LEVEL > LAND_LOGLEVEL_OFF)
	#if (LAND_TCPLOG_TARGET & LAND_LOGTARGET_FILE)
		#pragma message ("*********************** TCP LOG: output to FILE")
	#endif
	#if (LAND_TCPLOG_TARGET & LAND_LOGTARGET_ABOUT)
		#pragma message ("*********************** TCP LOG: output to ABOUT WINDOW")
	#endif
	#if (LAND_TCPLOG_TARGET & LAND_LOGTARGET_DIALOG)
		#pragma message ("*********************** TCP LOG: output to DIALOG")
	#endif
	#if (LAND_TCPLOG_TARGET & LAND_LOGTARGET_DEBUGGER)
		#pragma message ("*********************** TCP LOG: output to DEBUGGER")
	#endif
#endif


/* compiler messages for DB category */

#if (LAND_DBLOG_LEVEL >= LAND_LOGLEVEL_3)
	#pragma message ("*********************** DB LOG: Level 3")
#elif (LAND_DBLOG_LEVEL >= LAND_LOGLEVEL_2)
	#pragma message ("*********************** DB LOG: Level 2")
#elif (LAND_DBLOG_LEVEL >= LAND_LOGLEVEL_1)
	#pragma message ("*********************** DB LOG: Level 1")
/*
#else
	#pragma message ("*********************** DB LOG: --OFF--")
*/
#endif

#if (LAND_DBLOG_LEVEL > LAND_LOGLEVEL_OFF)
	#if (LAND_DBLOG_TARGET & LAND_LOGTARGET_FILE)
		#pragma message ("*********************** DB LOG: output to FILE")
	#endif
	#if (LAND_DBLOG_TARGET & LAND_LOGTARGET_ABOUT)
		#pragma message ("*********************** DB LOG: output to ABOUT WINDOW")
	#endif
	#if (LAND_DBLOG_TARGET & LAND_LOGTARGET_DIALOG)
		#pragma message ("*********************** DB LOG: output to DIALOG")
	#endif
	#if (LAND_DBLOG_TARGET & LAND_LOGTARGET_DEBUGGER)
		#pragma message ("*********************** DB LOG: output to DEBUGGER")
	#endif
#endif


/* compiler messages for THREADS category */

#if (LAND_THREADSLOG_LEVEL >= LAND_LOGLEVEL_3)
	#pragma message ("*********************** THREADS LOG: Level 3")
#elif (LAND_THREADSLOG_LEVEL >= LAND_LOGLEVEL_2)
	#pragma message ("*********************** THREADS LOG: Level 2")
#elif (LAND_THREADSLOG_LEVEL >= LAND_LOGLEVEL_1)
	#pragma message ("*********************** THREADS LOG: Level 1")
/*
#else
	#pragma message ("*********************** THREADS LOG: --OFF--")
*/
#endif

#if (LAND_THREADSLOG_LEVEL > LAND_LOGLEVEL_OFF)
	#if (LAND_THREADSLOG_TARGET & LAND_LOGTARGET_FILE)
		#pragma message ("*********************** THREADS LOG: output to FILE")
	#endif
	#if (LAND_THREADSLOG_TARGET & LAND_LOGTARGET_ABOUT)
		#pragma message ("*********************** THREADS LOG: output to ABOUT WINDOW")
	#endif
	#if (LAND_THREADSLOG_TARGET & LAND_LOGTARGET_DIALOG)
		#pragma message ("*********************** THREADS LOG: output to DIALOG")
	#endif
	#if (LAND_THREADSLOG_TARGET & LAND_LOGTARGET_DEBUGGER)
		#pragma message ("*********************** THREADS LOG: output to DEBUGGER")
	#endif
#endif


/* functions */


static void logtofile (char *str, char *category) {

	unsigned long ticks = gettickcount ();
	static unsigned long lastticks = 0;
		long idthread = (long) (**getcurrentthread ()).idthread;
		static long idlastthread = 0;

	if (logfile == NULL) {
		logfile = fopen (debuglogname, "a");
		}

	if (idthread != idlastthread) {
		fprintf (logfile, "\n");
		idlastthread = idthread;
		}


		fprintf (logfile, "%08lX (%04ld) | %08lX | %s | %s\n", (unsigned long) ticks, (ticks - lastticks), idthread, category, str);

	lastticks = ticks;

	fflush (logfile);
	}/*logtofile*/


static void logtoaboutwindow (char *str) {

	bigstring bs;
	
	copyctopstring (str, bs);

	aboutsetmiscstring (bs);
	}/*logtoaboutwindow*/


static void logtodialog (char *str) {
#pragma unused(str)

	}/*logtodialog*/


static void logtodebugger (char *str) {

	bigstring bs;
	
	copyctopstring (str, bs);
	
	DebugStr (bs);
	}/*logtodebugger*/


static void logtotargets (char *str, long targetflags, char *category) {

	if (flreentering)
		return;
	
	flreentering = true;

	/* runtime override of target for GENERAL category */
	init_runtime_logging_once();
	if (g_general_target_rt >= 0) {
		if (strcmp(category, LAND_GENERALLOG_NAME) == 0) {
			targetflags = g_general_target_rt;
		}
	}
			
	if (targetflags & LAND_LOGTARGET_FILE)
		logtofile (str, category);

	if (targetflags & LAND_LOGTARGET_ABOUT)
		logtoaboutwindow (str);
		
	if (targetflags & LAND_LOGTARGET_DIALOG)
		logtodialog (str);
		
	if (targetflags & LAND_LOGTARGET_DEBUGGER)
		logtodebugger (str);
	
	flreentering = false;
	}/*logtotargets*/


void logmessage_level (char *msg, char *file, long line, long targetflags, char *category, int level) {
	
	char str[400];
	
	str[0] = 0;
	
	/* runtime level threshold for GENERAL category */
	init_runtime_logging_once();
	if (g_general_level_rt >= 0) {
		if (strcmp(category, LAND_GENERALLOG_NAME) == 0) {
			if (level > g_general_level_rt) {
				return; /* filtered out */
			}
		}
	}

	sprintf (str, "%s [%s,%ld]", msg, file, line);
	
	logtotargets (str, targetflags, category);
	}/*logmessage_level*/
	

long logassert (char *expr, char *file, long line, long targetflags, char *category) {
	
	char str[400];
	
	str[0] = 0;
	
	sprintf (str, "Assertion failed: %s [%s,%ld]", expr, file, line);
	
	logtotargets (str, targetflags, category);
	
	return (0);
	}/*logassert*/


void logstartup () {

	/*perhaps get location of app here, so we can more precisely place our log file*/

	}/*logstartup*/


void logshutdown () {

	if (logfile != NULL)
		fclose (logfile);
	}/*DBTRACKERCLOSE*/



