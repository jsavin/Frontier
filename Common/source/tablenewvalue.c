
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

#include <standard.h>
#include "memory.h"
#include "lang.h"
#include "shell.rsrc.h"
#include "shellundo.h"
#include "tableinternal.h"
#include "tablestructure.h"



boolean tablemakenewvalue (void) {
	
	/*
	create a new, empty value below the current cell.
	
	to get the node to be inserted below the current position, we use
	tableoverridesort to patch the comparenodes langglobals callback and 
	look for the next node in the table
	*/
	
	register hdlhashtable ht;
	bigstring bsname;
	tyvaluerecord val;
	short newrow;
	hdlhashnode hnode, hnext;
	register boolean fl;
	
	pushundoaction (undotypingstring);
	
	if (tableempty ())
		newrow = 0;
	else
		newrow = (**tableformatsdata).rowcursor + 1;
	
	ht = tablegetlinkedhashtable ();
	
	pushhashtable (ht);
	
	hashgetnthnode (ht, newrow, &hnext); /*ignore result -- will be nil if rowcursor is last*/
	
	tableoverridesort (hnext);
	
	setemptystring (bsname);
	
	clearbytes (&val, longsizeof (val));
	
	fl = hashinsert (bsname, val);
	
	assert (tablevalidate (ht, true));
	
	tablerestoresort ();
	
	// (**ht).flneedsort = true;
	
	pophashtable ();
	
	if (!fl)
		return (false);
	
	if (hashgetnthnode (ht, newrow, &hnode))
		pushundostep ((undocallback) tableredoclear, (Handle) hnode);
	
	if (!tableeditentercell (newrow, namecolumn)) {
		
		undolastaction (false);
		
		return (false);
		}
	
	return (true);
	} /*tablemakenewvalue*/



