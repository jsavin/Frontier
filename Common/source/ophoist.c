
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

#include "op.h"
#include "opinternal.h"


#pragma pack(2)
typedef struct tyrestorablehoist {
	
	hdlheadrecord hhoisted; /*the head that was hoisted, whose subs are the summits*/
	
	hdlheadrecord hcursor; /*the location of the bar cursor*/
	} tyrestorablehoist;
#pragma options align=reset
	

tyrestorablehoist savedhoists [cthoists]; /*for saving and restoring the hoist state*/

short ctsavedhoists = 0;




void ophoistdisplay (void) {
	
	/*
	7/8/92 dmb: call opresetscrollbars after opgetscrollbarinfo to force update
	*/
	
	if (!opdisplayenabled ())
		return;
	
	opsetdisplaydefaults (op_get_outlinedata());
	
	opdirtymeasurements (); //6.0a14 dmb
	
	opsetctexpanded (op_get_outlinedata());
	
	opgetscrollbarinfo (true);
	
	opredrawscrollbars ();
	
	opinvaldisplay ();
	
	oploadeditbuffer ();
	
	/*opupdatenow ();*/ /*uncomment for debugging*/
	} /*ophoistdisplay*/
	

static boolean oppushhoistvisit (hdlheadrecord hnode, ptrvoid refcon) {
#pragma unused (refcon)

	/*
	assume we are only being called for the 1st level subheads of the guy
	being hoisted.  we reset his left pointer to indicate that he's a summit,
	and his level to 0.  then we reset the levels of all his subordinate
	nodes.
	*/
	
	(**hnode).headlinkleft = hnode; /*wire him back to himself*/
	
	(**hnode).flexpanded = true; /*all 0-th level items are expanded*/
	
	(**hnode).headlevel = 0;
	
	opresetlevels (hnode);
	
	return (true);
	} /*oppushhoistvisit*/
	

boolean oppushhoist (hdlheadrecord hnode) {
	
	register hdloutlinerecord ho = op_get_outlinedata();
	hdlheadrecord hsummit = (**hnode).headlinkright;
	tyhoistelement item;
	
	if ((**ho).tophoist >= cthoists)
		return (false);
		
	if (hsummit == hnode) /*has no subs*/
		return (false);
	
	opunloadeditbuffer ();
	
	item.hhoisted = hnode;
	
	item.hbarcursor = (**ho).hbarcursor;
	
	item.hsummit = (**ho).hsummit;
	
	item.lnumbarcursor = (**ho).lnumbarcursor;
	
	item.hline1 = (**ho).hline1;
	
	(**ho).hoiststack [(**ho).tophoist++] = item;
	
	oprecursivelyvisit (hnode, 1, &oppushhoistvisit, nil);
	
	(**ho).hsummit = hsummit; /*first sub is the first summit*/
	
	if (!opcontainsnode (hsummit, (**ho).hbarcursor))
		(**ho).hbarcursor = hsummit;
	
	(**ho).hline1 = hsummit;
	
	ophoistdisplay (); /*a really thorough smash of all data structures*/
	
	opdirtyoutline ();
	
	return (true);
	} /*oppushhoist*/
		
	
boolean oppophoist (void) {

	register hdloutlinerecord ho = op_get_outlinedata();
	hdlheadrecord nomad, nextnomad;
	hdlheadrecord hhoisted; 
	hdlheadrecord hfirstsummit;
	tyhoistelement item;
	
	if ((**ho).tophoist <= 0)
		return (false);
	
	opunloadeditbuffer ();
	
	item = (**ho).hoiststack [--(**ho).tophoist];
	
	hhoisted = item.hhoisted;
	
	nomad = hfirstsummit = (**ho).hsummit; /*start with the current first summit*/
	
	while (true) { /*re-link old summits to point at their old parents*/
		
		(**nomad).headlinkleft = hhoisted; /*point at his old parent*/
		
		nextnomad = (**nomad).headlinkdown;
		
		if (nextnomad == nomad) /*reached the end of the list*/
			break;
			
		nomad = nextnomad;
		} /*while*/
	
	(**hhoisted).headlinkright = hfirstsummit; /*old 1st summit might have been X'd*/
	
	opresetlevels (hhoisted);
	
	(**ho).hbarcursor = item.hbarcursor;
	
	(**ho).hsummit = item.hsummit;
	
	(**ho).lnumbarcursor = item.lnumbarcursor;
	
	(**ho).hline1 = item.hline1;

	ophoistdisplay (); /*a really thorough smash of all data structures*/
	
	opdirtyoutline ();
	
	return (true);
	} /*oppophoist*/
	

boolean oppopallhoists (void) {
	
	/*
	restore the structure to a normal, nothing-hoisted state so that the caller can
	save the structure.  pop the hoists in such a way that they can be restored after
	the saving is finished.
	
	3/1/91 dmb: in order to support nested calls -- which needs to happen when a 
	menubar is saved, for instance, we can't use globals so save the hoist 
	information.  instead, we re-use the hoist stack, and set tophoist to the 
	negative of its original value
	*/
	
	register hdloutlinerecord ho = op_get_outlinedata();
	tyhoistelement item;
	short i;
	short cthoisted;
	
	cthoisted = (**ho).tophoist;
	
	if (cthoisted <= 0)
		return (false);
	
	for (i = cthoisted - 1; i >= 0; i--) {
		
		item.hhoisted = ((**ho).hoiststack [i]).hhoisted;
		
		item.hbarcursor = (**ho).hbarcursor;
		
		oppophoist ();
		
		(**ho).hoiststack [i] = item; /*this is now just beyond the end of the stack*/
		} /*for*/
	
	(**ho).tophoist = -cthoisted;
	
	return (true);
	} /*oppopallhoists*/
	
	
void oprestorehoists (void) {
	
	/*
	after the saving process is finished, call this routine to restore the hoist
	state of the structure.
	*/
	
	register hdloutlinerecord ho = op_get_outlinedata();
	short i;
	short cthoisted;
	tyhoistelement item;
	
	cthoisted = -(**ho).tophoist;
	
	if (cthoisted == 0)
		return;
	
	(**ho).tophoist = 0; /*nothing is really pushed*/
	
	for (i = 0; i < cthoisted; i++) {
		
		item = (**ho).hoiststack [i]; /*grab saved hoist info from beyond end of stack*/
		
		oppushhoist (item.hhoisted);
		
		(**ho).hbarcursor = item.hbarcursor;
		} /*for*/
	
	ophoistdisplay (); /*a really thorough smash of all data structures*/
	} /*oprestorehoists*/
	
	
void opoutermostsummit (hdlheadrecord *hsummit) {
	
	/*
	punch through the illusion created by hoisting, and return to the caller
	a handle to the topmost summit in the structure.
	
	beware that the headlinkleft's of some of the nodes may not be what you'd
	expect without hoisting, but the right and down pointers are valid for
	traversals.
	*/
	
	register hdloutlinerecord ho = op_get_outlinedata();
	
	if ((**ho).tophoist <= 0) /*nothing hoisted*/
		*hsummit = (**ho).hsummit;
	else
		*hsummit = ((**ho).hoiststack [0]).hsummit;
	} /*opoutermostsummit*/
	
	
	
	
	
	
