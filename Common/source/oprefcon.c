
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

#include "shell.rsrc.h"
#include "shell.h"
#include "cursor.h"
#include "font.h"
#include "langinternal.h"
#include "kb.h"
#include "memory.h"
#include "op.h"
#include "opinternal.h"
#include "lang.h" /*7.0b4 PBS*/
#include "op_context.h"
#include <assert.h>


extern boolean tablevaltotable (tyvaluerecord val, hdlhashtable *htable, hdlhashnode hnode);

extern void pullstringvalue (const tyvaluerecord *v, bigstring bsval);

/**
 * opsetrefcon_ctx - Set refcon data on outline node (context-aware)
 *
 * @param ctx - Operation context (required)
 * @param hnode - Node to modify
 * @param pdata - Refcon data to copy
 * @param lendata - Length of refcon data
 * @return true if successful
 */
boolean opsetrefcon_ctx (op_context_t *ctx, hdlheadrecord hnode, ptrvoid pdata, long lendata) {

	assert(ctx != NULL);
	op_context_version_bump(ctx);

	/*
	each node in the structure can have a handle linked into it through the hrefcon
	field.  the application can allocate and access this handle himself, or use these
	routines.  when the structure is saved (oppack), the handle is saved with the
	node, and restored when the outline is unpacked.

	7/21/92 dmb: make sure the refcon is _exactly_ the right size, not any bigger
	*/

	Handle hrefcon = (**hnode).hrefcon;
	long len = lendata;

	if (hrefcon == nil) { /*never been set before*/

		Handle hnew;

		//if (!newclearhandle (len, &hnew))
		if (!newfilledhandle (pdata, len, &hnew)) // 7.31.97 dmb
			return (false);

		(**hnode).hrefcon = hrefcon = hnew;

		return (true);
		}

	if (gethandlesize (hrefcon) != len) { /*not the right size for the data*/

		if (!sethandlesize (hrefcon, len))
			return (false);
		}

	moveleft (pdata, *hrefcon, len);

	return (true);
	} /*opsetrefcon_ctx*/


/**
 * opsetrefcon - Set refcon data on outline node (backward-compatible wrapper)
 *
 * Wrapper for code that doesn't use operation context yet.
 * Allocates temporary context internally.
 */
boolean opsetrefcon (hdlheadrecord hnode, ptrvoid pdata, long lendata) {

	op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);

	if (ctx == NULL)
		return (false);

	boolean result = opsetrefcon_ctx(ctx, hnode, pdata, lendata);
	op_context_release(ctx);
	return result;
	} /*opsetrefcon*/
	
	
boolean opgetrefcon (hdlheadrecord hnode, ptrvoid pdata, long lendata) {
	
	/*
	get the refcon data from the indicated node.  always returns true -- if there
	isn't enough data in the refcon field, just fill pdata with the indicated 
	number of 0's.
	
	10/23/90 dmb: if the refcon field is nil, return false (but still fill pdata 
	with zeros).  many callers expect this.
	*/
	
	Handle hrefcon = NULL;
	long lenrefcon;

	// kw - 2006-01-19 - after crash - was "Handle hrefcon = (**hnode).hrefcon;"
	if (hnode != nil)
		hrefcon = (**hnode).hrefcon;

	lenrefcon = gethandlesize (hrefcon); /*handles nil*/
	
	if (lenrefcon < lendata) /*not enough data, make sure it's all zero*/
		clearbytes (pdata, lendata);
	
	if (hrefcon == nil)
		return (false);
	
	if (lenrefcon > lendata) /*somehow there's more data available than caller wants*/
		lenrefcon = lendata;
	
	moveleft (*hrefcon, pdata, lenrefcon);
	
	return (true);
	} /*opgetrefcon*/
	
	
void opemptyrefcon (hdlheadrecord hnode) {
	
	if ((**hnode).hrefcon == nil) /*nothing to do*/
		return;
		
	opdirtyoutline ();
		
	disposehandle ((**hnode).hrefcon);
	
	(**hnode).hrefcon = nil;
	} /*opemptyrefcon*/
	
	
boolean ophasrefcon (hdlheadrecord hnode) {
	
	return ((**hnode).hrefcon != nil);
	} /*ophasrefcon*/


boolean opattributesgetoneattribute (hdlheadrecord hnode, bigstring bsattname, tyvaluerecord *val) {
	
	/*
	7.0b16 PBS: get one value from a packed attributes table.
	*/
	
	tyvaluerecord vattributes;
	tyvaluerecord v;
	hdlhashtable htable;
	hdlhashnode hn = nil;
	hdlhashnode hnatt;
	boolean fl = false;

	disablelangerror ();
	
	if (!opattributesgetpackedtablevalue (hnode, &vattributes))
		
		return (false);
	
	if (!tablevaltotable (vattributes, &htable, hn))
		
		goto exit;
	
	if (hashtablelookup (htable, bsattname, &v, &hnatt)) {
	
		copyvaluerecord (v, val);
		
		disposevaluerecord (v, false);
		
		exemptfromtmpstack (val);

		fl = true;
		} /*if*/
	
	exit:
	
	disposevaluerecord (vattributes, false);
	
	enablelangerror ();
	
	return (fl);	
	} /*opattributesgetoneattribute*/


boolean opattributesgetpackedtablevalue (hdlheadrecord hnode, tyvaluerecord *val) {
	
	/*
	7.0b16 PBS: get the table packed as a binary in a headline's refcon.
	*/
	
	Handle hrefcon = (**hnode).hrefcon;
	tyvaluerecord linkedval;
	boolean fl = false;
	
	if (!ophasrefcon (hnode)) {/*if no refcon, no attributes*/
	
		langerrormessage (BIGSTRING ("\x39""Can't get attributes because this headline has no refcon."));
		
		goto exit1;
		}
	
	if (!langunpackvalue (hrefcon, &linkedval)) { /*try to unpack the refcon*/
		
		langerrormessage (BIGSTRING ("\x3e""Can't get attributes because of an error unpacking the refcon."));

		goto exit1;
		}
	
	if (linkedval.valuetype != binaryvaluetype) { /*must be a binary*/
		
		langerrormessage (BIGSTRING ("\x3e""Can't get attributes because the refcon is not of binary type."));

		goto exit2;
		}
	
	if (!langunpackvalue (linkedval.data.binaryvalue, val)) { /*it's a packed binary: unpack it.*/
		
		langerrormessage (BIGSTRING ("\x3e""Can't get attributes because of an error unpacking the refcon."));

		goto exit2;
		}
	
	fl = true;

	exit2:
	
	disposevaluerecord (linkedval, false);
	
	exit1:
	
	return (fl);
	} /*opattributesgetpackedtable*/


boolean opattributesgettypestring (hdlheadrecord hnode, bigstring bstype) {
	
	/*
	7.0b4 PBS: Get the node type of a headline.
	Unpack the refcon, look in the table for a type attribute, set bstype
	equal to that string.
	
	7.0b14 PBS: Fixed memory leaks.
	*/
	
	Handle hrefcon = (**hnode).hrefcon;
	tyvaluerecord linkedval;
	tyvaluerecord val;
	hdlhashtable htable;
	hdlhashnode hn = nil;
	hdlhashnode hnheadlinetype;
	tyvaluerecord valheadlinetype;
	boolean fl = false;
	
	disablelangerror ();

	if (!ophasrefcon (hnode)) //if no refcon, not attributes
	
		goto exit3;

	if (!langunpackvalue (hrefcon, &linkedval)) //try to unpack the refcon
	
		goto exit3;
	
	if (linkedval.valuetype != binaryvaluetype) //must be a binary
		
		goto exit2;
	
	if (!langunpackvalue (linkedval.data.binaryvalue, &val)) //it's a packed binary: unpack it.
		
		goto exit2;
	
	if (val.valuetype != externalvaluetype) //it must be a table
		
		goto exit1;
	
	if (!tablevaltotable (val, &htable, hn))
		
		goto exit1;
	
	if (hashtablelookup (htable, BIGSTRING ("\x04""type"), &valheadlinetype, &hnheadlinetype)) {
	
		pullstringvalue (&valheadlinetype, bstype);
		
		fl = true;
		} /*if*/
	
	exit1: /*7.0b14 PBS: fix memory leaks*/
	
	disposevaluerecord (val, false);
	
	exit2:
	
	disposevaluerecord (linkedval, false);
	
	exit3:
	
	enablelangerror ();

	return (fl);
	} /*opattributesgettypestring*/



