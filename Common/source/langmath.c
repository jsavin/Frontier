
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

/* 2026-01-14 Codex: Fix min()/max() comparison bug for ostypevaluetype by separating from longvaluetype cases. */

#include <math.h>

#include "frontier.h"
#include "standard.h"

#include "error.h"
#include "memory.h"
#include "ops.h"
#include "resources.h"
#include "lang.h"
#include "langinternal.h"
#include "langexternal.h"
#include "langsystem7.h"
#include "tablestructure.h"
#include "kernelverbs.h"
#include "kernelverbdefs.h"
#include "shell.rsrc.h"
#include "timedate.h"
#include "langmath.h"

#define matherrorlist 269
#define notimplementederror 1

typedef enum tymathtoken { /*verbs that are processed by langmath.c*/
	
	minfunc,
	
	maxfunc,
	
	sqrtfunc,

	randomfunc,

	cmathverbs
	} tymathtoken;

static boolean mathfunctionvalue (short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror) {
	
	hdltreenode hp1 = hparam1;
	tyvaluerecord *v = vreturned;
	short errornum = 0;
	
	setbooleanvalue (false, v); /*by default, math functions return false*/
	
	switch (token) {
	
		case minfunc: { /* 2004/12/29 smd */
			
			tyvaluerecord v1, v2;
			tyvaluerecord v1copy, v2copy;
			tyvaluerecord * vResult = NULL;
			boolean fl = false;
			
			if (!getreadonlyparamvalue (hp1, 1, &v1))
				break;
			
			flnextparamislast = true;
			
			if (!getreadonlyparamvalue (hp1, 2, &v2))
				break;
			
			if (!copyvaluerecord (v1, &v1copy))  /* so that we don't coerce the original */
				break;
			
			if (!copyvaluerecord (v2, &v2copy))  /* so that we don't coerce the original */
				break;
			
			if (!coercetypes (&v1copy, &v2copy))
			{
				disposevalues (&v1copy, &v2copy);
				
				break;
			}
			
			switch (v1copy.valuetype) {
				
				case novaluetype: {  /* no test needed, nil is nil*/
					initvalue (v, novaluetype);
					
					fl = true;
					
					break;
					}
				
				case booleanvaluetype: /* if v2 is true, then return v1 (false < true) */
					vResult = v2copy.data.flvalue ? &v1 : &v2;
					
					break;
				
				/* all the rest return the greater of the two values */
				case charvaluetype:
					vResult = ( v1copy.data.chvalue <= v2copy.data.chvalue ) ? &v1 : &v2;
					
					break;
							
				case intvaluetype:
					vResult = ( v1copy.data.intvalue <= v2copy.data.intvalue ) ? &v1 : &v2;
					
					break;
				
				case longvaluetype:
					vResult = ( v1copy.data.longvalue <= v2copy.data.longvalue ) ? &v1 : &v2;

					break;

				case ostypevaluetype:
					vResult = ( v1copy.data.ostypevalue <= v2copy.data.ostypevalue ) ? &v1 : &v2;

					break;
				
				case directionvaluetype:
					vResult = ( (short) v1copy.data.dirvalue <= (short) v2copy.data.dirvalue ) ? &v1 : &v2;
					
					break;
					
				case datevaluetype:
					vResult = timegreaterthan( v2copy.data.datevalue, v1copy.data.datevalue ) ? &v1 : &v2;
					
					break;
				
				case singlevaluetype:
					vResult = ( v1copy.data.singlevalue <= v2copy.data.singlevalue ) ? &v1 : &v2;
					
					break;
				
				case doublevaluetype:
					vResult = ( **v1copy.data.doublevalue <= **v2copy.data.doublevalue ) ? &v1 : &v2;
					
					break;
				
				case stringvaluetype:
					vResult = ( comparehandles( v1copy.data.stringvalue, v2copy.data.stringvalue ) == 1 ) ? &v2 : &v1;
					
					break;
				
				default:
					langerror (comparisonnotpossibleerror);
					
					fl = false; /*operation is not defined*/
					
					break;
			} /*switch*/
			
			if ( vResult != NULL )
			{
				if ( copyvaluerecord( *vResult, v ) )
					fl = true;
				
				disposevalues( &v1copy, &v2copy );
			}
			
			return ( fl );
		}
		
		case maxfunc: { /* 2004/12/29 smd */
			
			tyvaluerecord v1, v2;
			tyvaluerecord v1copy, v2copy;
			tyvaluerecord * vResult = NULL;
			boolean fl = false;
			
			if (!getreadonlyparamvalue (hp1, 1, &v1))
				break;
			
			flnextparamislast = true;
			
			if (!getreadonlyparamvalue (hp1, 2, &v2))
				break;
			
			if (!copyvaluerecord (v1, &v1copy))  /* so that we don't coerce the original */
				break;
			
			if (!copyvaluerecord (v2, &v2copy))  /* so that we don't coerce the original */
				break;
			
			if (!coercetypes (&v1copy, &v2copy))
			{
				disposevalues (&v1, &v2);
				
				break;
			}
			
			switch (v1copy.valuetype) {
				
				case novaluetype: {  /* no test needed, nil is nil*/
					initvalue (v, novaluetype);
					
					fl = true;
					
					break;
					}
				
				case booleanvaluetype: /* if v2 is true, then return v1 (false < true) */
					vResult = v1copy.data.flvalue ? &v1 : &v2;
					
					break;
				
				/* all the rest return the greater of the two values */
				case charvaluetype:
					vResult = ( v1copy.data.chvalue >= v2copy.data.chvalue ) ? &v1 : &v2;
					
					break;
							
				case intvaluetype:
					vResult = ( v1copy.data.intvalue >= v2copy.data.intvalue ) ? &v1 : &v2;
					
					break;
				
				case longvaluetype:
					vResult = ( v1copy.data.longvalue >= v2copy.data.longvalue ) ? &v1 : &v2;

					break;

				case ostypevaluetype:
					vResult = ( v1copy.data.ostypevalue >= v2copy.data.ostypevalue ) ? &v1 : &v2;

					break;
				
				case directionvaluetype:
					vResult = ( (short) v1copy.data.dirvalue >= (short) v2copy.data.dirvalue ) ? &v1 : &v2;
					
					break;
					
				case datevaluetype:
					vResult = timegreaterthan( v1copy.data.datevalue, v2copy.data.datevalue ) ? &v1 : &v2;
					
					break;
				
				case singlevaluetype:
					vResult = ( v1copy.data.singlevalue >= v2copy.data.singlevalue ) ? &v1 : &v2;
					
					break;
				
				case doublevaluetype:
					vResult = ( **v1copy.data.doublevalue >= **v2copy.data.doublevalue ) ? &v1 : &v2;
					
					break;
				
				case stringvaluetype:
					vResult = ( comparehandles( v1copy.data.stringvalue, v2copy.data.stringvalue ) != -1 ) ? &v1 : &v2;
					
					break;
				
				default:
					langerror (comparisonnotpossibleerror);
					
					fl = false; /*operation is not defined*/
					
					break;
			} /*switch*/
			
			if ( vResult != NULL )
			{
				if ( copyvaluerecord( *vResult, v ) )
					fl = true;
				
				disposevalues( &v1copy, &v2copy );
			}
			
			return ( fl );
		}
		
		case sqrtfunc: { /* 2004/12/29 smd */
			
			tyvaluerecord v1;
			double d;
			
			flnextparamislast = true;
			
			if (!getdoubleparam (hp1, 1, &v1))
				break;
			
			d = sqrt (**v1.data.doublevalue);
			
			return (setdoublevalue (d, v));
			}

#ifdef FRONTIER_HEADLESS
		case randomfunc: {
			long lower32, upper32;

			if (!getlongvalue (hp1, 1, &lower32))
				break;

			flnextparamislast = true;

			if (!getlongvalue (hp1, 2, &upper32))
				break;

			int64_t lower = (int64_t) lower32;
			int64_t upper = (int64_t) upper32;

			if (lower > upper) {
				langerror (badrandomboundserror);
				return (false);
			}

			/* 2025-12-09 Codex: avoid signed overflow when computing range size. */
			uint64_t span = (uint64_t) (upper - lower) + 1u;
			if (span == 0) { /* wrapped */
				langerror (badrandomboundserror);
				return (false);
			}

			uint64_t r = (uint64_t) (unsigned int) rand ();
			int64_t n = lower + (int64_t) (r % span);
			return (setlongvalue (n, v));
			}
#endif /* FRONTIER_HEADLESS */

		default:
			errornum = notimplementederror;
			
			goto error;
		} /*switch*/
	
	error:
	
	if (errornum != 0) /*get error string*/
		getstringlist (matherrorlist, errornum, bserror);
	
	return (false);
	} /*langmathvalue*/
	

boolean mathinitverbs (void) {
	
	/*
	2004-12-29 smd: new math verbs
	2025-12-08 Codex: headless builds register math verbs programmatically (no resources).
	*/

#ifdef FRONTIER_HEADLESS
	hdlhashtable htable = nil;

	if (!newfunctionprocessor (BIGSTRING ("\pmath"), &mathfunctionvalue, false, &htable))
		return (false);

	pushhashtable (htable);
	if (!langaddkeyword (BIGSTRING ("\pmin"), minfunc))
		goto fail;
	if (!langaddkeyword (BIGSTRING ("\pmax"), maxfunc))
		goto fail;
	if (!langaddkeyword (BIGSTRING ("\psqrt"), sqrtfunc))
		goto fail;
	if (!langaddkeyword (BIGSTRING ("\prandom"), randomfunc))
		goto fail;
	pophashtable ();
	return (true);

fail:
	pophashtable ();
	return (false);
#else
	
	return (loadfunctionprocessor (idmathverbs, &mathfunctionvalue));
#endif
	} /*mathinitverbs*/
