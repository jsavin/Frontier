
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

#include "shell.h"
#include "shellprivate.h"




#define ctblocks 5 /*we can remember blocked events up to 5 levels deep*/

static UInt16 topblocks = 0;

static UInt16 blockstack [ctblocks];

static UInt16 blockedevents = 0;



UInt16 shellblockedevents (void) {
	
	return (blockedevents);
	} /*shellblockedevents*/


boolean shellblocked (UInt16 mask) {
	
	return ((blockedevents & mask) != 0);
	} /*shellblocked*/


boolean shellpushblock (UInt16 mask, boolean flblock) {
	
	if (topblocks >= ctblocks)
		return (false);
	
	blockstack [topblocks++] = blockedevents;
	
	if (flblock)
		blockedevents |= mask;
	else
		blockedevents &= ~mask;
	
	return (true);
	} /*shellpushblock*/


boolean shellpopblock (void) {
	
	if (topblocks <= 0)
		return (false);
	
	blockedevents = blockstack [--topblocks];
	
	return (true);
	} /*shellpopblock*/
	


static UInt16 shelleventsdisable = 0;


boolean shelleventsblocked (void) {
	
	return (shelleventsdisable > 0);
	} /*shelleventsblocked*/
	
	
boolean shellblockevents (void) {
	
	++shelleventsdisable;
	
	return (true);
	} /*shellblockevents*/
	

boolean shellpopevents (void) {
	
	if (shelleventsdisable <= 0)
		return (false);
	
	--shelleventsdisable;
	
	return (true);
	} /*shellpopevents*/
	
	
