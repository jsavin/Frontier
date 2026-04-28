
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

#include "strings.h"
#include "langexternal.h"
#include "tableinternal.h"
#include "tableverbs.h"




static short tablecomparenames (hdlhashnode hnode1, hdlhashnode hnode2) {
	
	/*
	2004-11-09 aradke: optimized by calling compareidentifiers which does
	an in-place case-insensitive compare instead of copying both key strings,
	converting both copies completely to lowercase, and running a case-sensitive
	compare on the resulting lowercase strings.
	*/

	return (compareidentifiers ((**hnode1).hashkey, (**hnode2).hashkey));
	
	/*
	bigstring bs1, bs2;
	
	gethashkey (hnode1, bs1);

	gethashkey (hnode2, bs2);
	
	alllower (bs1); /%comparison is unicase%/
	
	alllower (bs2);
	
	return (comparestrings (bs1, bs2));
	*/
	} /*tablecomparenames*/



static short tablecomparekinds (hdlhashnode hnode1, hdlhashnode hnode2) {
	
	tyvaluerecord val1, val2;
	register tyvaluetype t1, t2;
	
	val1 = (**hnode1).val;
	
	val2 = (**hnode2).val;
	
	t1 = val1.valuetype;
	
	t2 = val2.valuetype;
	
	if ((t1 == externalvaluetype) && (t2 == externalvaluetype)) {
		
		register hdlexternalhandle h1 = (hdlexternalhandle) val1.data.externalvalue;
		register hdlexternalhandle h2 = (hdlexternalhandle) val2.data.externalvalue;
		register tyexternalid id1 = (tyexternalid) (**h1).id;
		register tyexternalid id2 = (tyexternalid) (**h2).id;
		
		if (id1 == id2)
			return (tablecomparenames (hnode1, hnode2));
			
		return (langexternalcomparetypes (id1, id2));
		}
	
	if (t1 == externalvaluetype) /*scalars sort before externals*/
		return (1);
		
	if (t2 == externalvaluetype) /*scalars sort before externals*/
		return (-1);
		
	if (t1 == t2)
		return (tablecomparenames (hnode1, hnode2));
		
	return (sgn (t1 - t2));
	} /*tablecomparekinds*/


static short tablecomparevalues (hdlhashtable htable, hdlhashnode hnode1, hdlhashnode hnode2) {
	
	/*
	2.1b2 dmb: new version, corrects bugs that led to semi-random sorts.  now 
	only values of the same kind are compared, and the valuetype is a strict 
	secondary sort.
	*/
	
	register hdlhashnode h1 = hnode1;
	register hdlhashnode h2 = hnode2;
	tyvaluerecord val1, val2, vreturned;
	register boolean fllessthan = false;
	register boolean flequal = false;
	boolean flcomparable;
	
	flcomparable = (**h1).val.valuetype == (**h2).val.valuetype;
	
	if (flcomparable) {
		
		pushhashtable (htable); /*any temps are allocated in this table*/
		
		disablelangerror (); /*protect us from error dialogs*/
		
		copyvaluerecord ((**h1).val, &val1);
		
		copyvaluerecord ((**h2).val, &val2);
		
		flcomparable = LTvalue (val1, val2, &vreturned);
		
		cleartmpstack (); /*temps no longer needed*/
		
		fllessthan = vreturned.data.flvalue;
		
		if (flcomparable && !fllessthan) {
			
			copyvaluerecord ((**h1).val, &val1);
			
			copyvaluerecord ((**h2).val, &val2);
			
			flcomparable = EQvalue (val1, val2, &vreturned);
			
			cleartmpstack (); /*temps no longer needed*/
			
			flequal = vreturned.data.flvalue;
			}
		
		enablelangerror ();
		
		pophashtable ();
		}
	
	if ((!flcomparable) || flequal) 
		return (tablecomparekinds (hnode1, hnode2));
	
	if (fllessthan)
		return (-1);
	else
		return (1);
	} /*tablecomparevalues*/
	

short tablecomparenodes (hdlhashtable htable, hdlhashnode hnode1, hdlhashnode hnode2) {
	
	/*
	3/31/93 dmb: return a signed value indicating less than (-1), equality (0), 
	or greater than (1). modified all routines that we call to do the same.
	*/
	
		
		switch ((**htable).sortorder) {
			
			case sortbyname: 
				return (tablecomparenames (hnode1, hnode2));
				
			case sortbyvalue:
				return (tablecomparevalues (htable, hnode1, hnode2));
				
			case sortbykind:
				return (tablecomparekinds (hnode1, hnode2));
			
			default:
				return (0);
			} /*switch*/
		
	
	} /*tablecomparenodes*/

	

static hdlhashnode nextnodecompare;
static langcomparenodescallback origcomparenodescallback;


static short tableoverridecomparenodes (hdlhashtable htable, hdlhashnode hnode1, hdlhashnode hnode2) {
#pragma unused (htable, hnode1)

	if (hnode2 == nextnodecompare)
		return (-1);
	else
		return (1);
	} /*tableoverridecomparenodes*/


void tableoverridesort (hdlhashnode hnext) {
	
	/*
	temporarily override the current sort, forcing the next node insertion 
	to "sorted" before the indicated node
	*/
	
	origcomparenodescallback = langcallbacks.comparenodescallback;
	
	langcallbacks.comparenodescallback = &tableoverridecomparenodes;
	
	nextnodecompare = hnext; /*set global for tableeditcomparenodes*/
	} /*tableoverridesort*/


void tablerestoresort (void) {

	langcallbacks.comparenodescallback = origcomparenodescallback; /*restore*/
	} /*tablerestoresort*/




