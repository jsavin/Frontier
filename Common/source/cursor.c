
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

#include "cursor.h"



static tycursortype lastcursor = cursorisdirty;

static tycursortype beachballstate = cursorisbeachball4;

/*
static short earthstate = cursorisearth7;
*/

static long ticklastroll = 0;

static tydirection rolldirection = right;




void setcursortype (tycursortype newcursor) {
	
	/*
	7/30/90 dmb:  don't assume that cursor is never changed behind your back
	*/

	register tycursortype cursor = newcursor;
		CursHandle hcursor;
	
	lastcursor = cursor; /*remember for next time*/

	if (cursor == cursorisdirty)
		return;
	
	if (cursor == cursorisarrow)
		ticklastroll = 0; /*disable rolling until reinitialized*/

	if (cursor == cursorisarrow) {
		
		//Code change by Timothy Paustian Sunday, May 7, 2000 11:00:01 PM
		//Changed to Opaque call for Carbon
		#if ACCESSOR_CALLS_ARE_FUNCTIONS == 1
		Cursor	theArrow;
		GetQDGlobalsArrow(&theArrow);
		SetCursor(&theArrow);
		#else
		SetCursor (&qd.arrow);
		#endif
		return;
		}
	
	hcursor = GetCursor (cursor);
	
	if (hcursor == nil) /*resource error*/
		return;
	
	SetCursor (*hcursor);


	} /*setcursortype*/


void obscurecursor (void) {
	ObscureCursor ();
	} /*obscurecursor*/


static boolean rollingtimerexpired (void) {
	
	register long tc;
	
	if (ticklastroll == 0) /*timer hasn't been initted*/
		return (false);
	
	tc = gettickcount ();


	
	if ((ticklastroll + 6) > tc) /*a tenth of a second hasn't passed since last bump*/
		return (false);
		
	ticklastroll = tc; /*enough time has passed, reset the timer*/
	
	return (true);
	} /*rollingtimerexpired*/


void initbeachball (tydirection dir) {
	
	rolldirection = dir; /*always set*/
	
	if (ticklastroll == 0) { /*beach ball isn't already initted*/
		
		beachballstate = cursorisbeachball4;
		
		InitCursor (); /*make sure it's visible*/
		ticklastroll = gettickcount ();
		}
	} /*initbeachball*/


void rollbeachball (void) {
	
		register tycursortype state;
		
		if (rollingtimerexpired ()) {
			
			state = beachballstate;
			
			if (rolldirection == right) {
				
				if (--state < cursorisbeachball1)
					state = cursorisbeachball4;
				}
			else {
				
				if (++state > cursorisbeachball4) /*wrap around*/
					state = cursorisbeachball1;
				}
			
			setcursortype (state);
			
			beachballstate = state;
			}

	} /*rollbeachball*/


boolean beachballcursor (void) {
	
	/*
	return true if the cursor is one of the beachballs.
	
	12/26/90 dmb: new test accounts for the fact that, after an initbeachball, 
	lastcursor won't be a beach ball until the timer has expired.  this test 
	will return true if either rolling cursor is active (earth or beach ball)
	*/
	
	/*
	return ((lastcursor >= cursorisbeachball1) && (lastcursor <= cursorisbeachball4));
	*/
	
	return (ticklastroll != 0);
	} /*beachballcursor*/

/*
void initearth (void) {
	
	earthstate = cursorisearth1;
	
	ticklastroll = gettickcount ();
	} /%initearth%/


void rollearth (void) {
	
	register short state;
	
	if (rollingtimerexpired ()) {
	
		state = earthstate + 1; 
		
		if (state > cursorisearth7) /%wrap around%/
			state = cursorisearth1;
						
		setcursortype (state);
		
		earthstate = state;
		}
	} /%rollearth%/
*/




