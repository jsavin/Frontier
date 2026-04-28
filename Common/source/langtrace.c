
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

#include "kb.h"
#include "mouse.h"
#include "strings.h"
#include "shell.h"
#include "langinternal.h"




/*
static boolean fllangtrace = false; 
*/


#ifdef fldebug
	#define fllangtrace 0
#else
	#define fllangtrace 0
#endif

extern bigstring bstoken; /*text of last token, see langscan.c*/

/*
#if fllangtrace
	#ifdef MACVERSION
		#define messageset(bs) shellfrontwindowmessage (bs)
	#endif

	#ifdef WIN95VERSION
		static FILE * tracefile = NULL;
		#define messageset(bs)	\
				do {if (tracefile == NULL) \
					tracefile = fopen ("langtrace.txt", "w+"); \
					fprintf (tracefile, "%s\n", bs);} while (0)
	#endif
#endif
*/


#if fllangtrace
	

	#define STR_lineterminator "\r\n"


	static FILE * tracefile = NULL;


	void langstarttrace (void) {

		if (tracefile == NULL)
			tracefile = fopen ("frontier_langtrace.txt", "a+"); /* append */

		fprintf (tracefile, STR_lineterminator);
		} /*langstarttrace*/
		
		
	void langendtrace (void) {

		if (tracefile)
			fclose (tracefile);
		
		tracefile = NULL;
		} /*langendtrace*/


	void langtrace (bigstring bs) {
		
		nullterminate (bstoken);

		nullterminate (bs);

		fprintf (tracefile, ("'%s' ==> %s" STR_lineterminator), &bstoken[1], &bs[1]);
		
		if (shiftkeydown ())
			waitmouseclick ();
		} /*langtrace*/


#else


	void langstarttrace (void) { }

	void langendtrace (void) { }

	void langtrace (bigstring bs) {
#pragma unused (bs)
}

#endif

/*
void langsyntaxtrace (boolean fl) {
	
	#ifdef fldebug
	
	fllangtrace = fl; /%set global that turns the syntax trace on or off%/
	
	if (fl) 
		langstarttrace ();
	
	#endif
	} /%langsyntaxtrace%/
*/	

