
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

#include "landinternal.h"

	#include "aeutils.h" /*PBS 03/14/02: AE OS X fix.*/



pascal boolean landgetparam (hdlverbrecord hverb, typaramkeyword key, typaramtype type, typaramrecord *param) {
	
	/*
	search the parameters of the verb record for one with the indicated key.
	
	return the value and type of the parameter, and true if it was found.
	
	10/29/90 DW: added support for optional params.  the user sets the landglobals
	field true if the param is to be optional.  if we don't find one with the
	indicated key, we return false to our caller, without returning an error to 
	the IAC caller.  the landglobals field must be reset every for every getparam
	call.  it's implemented as a global because optional parameters are expected
	to be the exception, not the rule -- we don't clutter up our parameter lists
	with this boolean.
	
	10/30/90 DW: if type is notype, we don't care about the type of the parameter,
	we just look for a match for the key.
	
	11/27/90 dmb: the optional parameter field mentioned above is now in the verb 
	record, not in landglobals.
	*/
	
	register hdlverbrecord hv = hverb;
	register boolean fl;
	AppleEvent event, reply;
	
	landsystem7geteventrecords (hv, &event, &reply);
	
	fl = landsystem7getparam (&event, key, type, param);
	
	if (!fl) {
		
		if ((**hv).flnextparamisoptional)
			landseterror (noErr);
		else
			landreturnerror (hv, nosuchparamerror);
		}
	
	(**hv).flnextparamisoptional = false; /*must be reset every time*/
	
	return (fl);
	} /*landgetparam*/


pascal boolean landgetnthparam (hdlverbrecord hverb, short n, typaramrecord *param) {
	
	/*
	return the nth parameter in the verb's parameter list.  we return all the info
	we have about the parameter in the paramrecord.
	
	n is 1-based.
	
	we don't return an error to the caller if the parameter isn't there -- this allows
	the caller to sniff around the parameter list without making any assertions.
	*/
	
	register hdlverbrecord hv = hverb;
	AppleEvent event, reply;
	
	if (n > (**hv).ctparams) /*there aren't that many parameters*/
		return (false);
	
	landsystem7geteventrecords (hv, &event, &reply);
	
	return (landsystem7getnthparam (&event, n, param));
	} /*landgetnthparam*/


pascal boolean landgetintparam (hdlverbrecord hverb, typaramkeyword key, short *x) {
	
	typaramrecord param;
	
	if (!landgetparam (hverb, key, inttype, &param)) 
		return (false);
	
	
		{		
		Handle h;
		
		copydatahandle (&(param.desc), &h);
		
		*x = **(short **) h;
		
		disposehandle (h);
		}
	
	
	landdisposeparamrecord (&param);
	
	return (true);
	} /*landgetintparam*/
	

pascal boolean landgetlongparam (hdlverbrecord hverb, typaramkeyword key, long *x) {
	
	typaramrecord param;
	Handle			 theData;
	boolean			 done = false;

	if (!landgetparam (hverb, key, longtype, &param)) 
		return (false);

	/* kw - 2006-02-19 --- don't dereference the nil handle... */

	if (copydatahandle (&(param.desc), &theData))
	{
		lockhandle(theData);

		*x = **(long **) theData;

		unlockhandle(theData);

		disposehandle (theData);

		done = true;
	}

	landdisposeparamrecord (&param);
	
	return (done);
	} /*landgetlongparam*/
	

pascal boolean landgetstringparam (hdlverbrecord hverb, typaramkeyword key, bigstring x) {
	
	/*
	7/20/92 dmb: always use texttype
	*/
	
	typaramrecord param;
	
	if (!landgetparam (hverb, key, texttype, &param)) 
		return (false);
	
	
		 datahandletostring (&(param.desc), x);
	
	
	landdisposeparamrecord (&param);
	
	return (true);
	} /*landgetstringparam*/


pascal boolean landgettextparam (hdlverbrecord hverb, typaramkeyword key, Handle *x) {
	
	typaramrecord param;
	
	if (!landgetparam (hverb, key, texttype, &param)) 
		return (false);

	
		 copydatahandle (&(param.desc), x);
	

	return (true);
	} /*landgettextparam*/



