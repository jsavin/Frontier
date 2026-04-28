
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

#include "scrollbar.h"
#include "quickdraw.h"
#include "shell.h"
#include "shellprivate.h"



void shellupdatescrollbars (hdlwindowinfo hinfo) {
	
	/*
	can be called by an application to update the scrollbars when they need changing
	and the shell isn't expecting a change.
	
	an example -- a range delete operation in a spreadsheet, or a dragging move
	in an outline.
	
	dw to dmb: this routine now takes a hldwindowinfo parameter, not a windowptr.
	*/
	
	register hdlwindowinfo h = hinfo;
	
	setscrollbarinfo ((**h).vertscrollbar, &(**h).vertscrollinfo);
	
	setscrollbarinfo ((**h).horizscrollbar, &(**h).horizscrollinfo);
	
	(**h).fldirtyscrollbars = false;
	} /*shellupdatescrollbars*/
	
	
void shellcheckdirtyscrollbars (void) {
	
	if (!shellpushfrontglobals ()) /*need at least one window open*/
		return;
	
	/*dmb to DW: why was this excluded for dialogs?  no longer valid, but am I breaking something?
	if (!config.fldialog)
	*/
		if ((**shellwindowinfo).fldirtyscrollbars) {
			
			shellupdatescrollbars (shellwindowinfo);
			}
	
	shellpopglobals ();
	} /*shellcheckdirtyscrollbars*/
	
	
void shellsetscrollbars (WindowPtr w) {
	
	/*
	call the driver to set the min, max and current values for the scroll 
	bars.  
	
	8/31/90 DW: we not longer set globals, we now push them.
	*/
	
	shellpushglobals (w);
	
	(*shellglobals.setscrollbarroutine) ();
	
	shellupdatescrollbars (shellwindowinfo);
		
	shellpopglobals ();
	} /*shellsetscrollbars*/


static tydirection scrolldirection (boolean flvert, boolean flup) {

	register tydirection dir;
	
	if (flvert)
		if (flup)
			dir = up;
		else
			dir = down;
	else
		if (flup)
			dir = left;
		else
			dir = right;
	
	return (dir);
	} /*scrolldirection*/






static pascal void shellhorizscroll (hdlscrollbar ctrl, short part) {
	
	register hdlwindowinfo h = shellwindowinfo;
	boolean flleft, flpage;
	register tydirection dir;
	
	if (!scrollbarhit (ctrl, part, &flleft, &flpage))
		return;
	
	/*	
	pushclip ((**h).contentrect);
	*/
	
	dir = scrolldirection (false, flleft);
	
	(*shellglobals.scrollroutine) (dir, flpage, 1);
	
	/*
	popclip ();
	*/
	
	setscrollbarinfo ((**h).horizscrollbar, &(**h).horizscrollinfo);
	} /*shellhorizscroll*/


static pascal void shellvertscroll (hdlscrollbar ctrl, short part) {
	
	register hdlwindowinfo h = shellwindowinfo;
	boolean flup, flpage;
	register tydirection dir;
	
	if (!scrollbarhit (ctrl, part, &flup, &flpage))
		return;
	
	/*
	pushclip ((**h).contentrect);
	*/
	
	dir = scrolldirection (true, flup);
		
	(*shellglobals.scrollroutine) (dir, flpage, 1);
	
	/*	
	popclip ();
	*/
	
	setscrollbarinfo ((**h).vertscrollbar, &(**h).vertscrollinfo);
	} /*shellvertscroll*/


//Code change by Timothy Paustian Friday, June 16, 2000 3:14:00 PM
//Changed to Opaque call for Carbon

	#define shellvertscrollUPP	(&shellvertscroll)
	#define shellhorizscrollUPP	(&shellhorizscroll)
	#define	shelllivescrollupp	(&ScrollThumbActionProc)


/*The live scrolling code descends from Apple sample code, hence the different style of code.*/

enum
{
	// Misc scroll bar constants

	kScrollBarWidth = 16,
	kScrollBarWidthAdjust = kScrollBarWidth - 1,
	
	kScrollArrowWidth = 16,
	kScrollThumbWidth = 16,
	kTotalWidthAdjust = (kScrollArrowWidth * 2) + kScrollThumbWidth,
	
	kPageOverlap = 10,
	kThumbTrackWidthSlop = 50,
	kThumbTrackLengthSlop = 113

};


static ControlRef   gControl;
static SInt32		gValueSlop;
//static SInt32		gSaveValue;
static SInt32		gStartValue;

static SInt32	CalcValueFromPoint ( ControlRef theControl, Point thePoint );
static boolean flverticalscroll;
static RgnHandle	gSaveClip = nil;
static long lastpoint;

pascal void ScrollThumbActionProc (void);


static void DisableDrawing ( void ) {

	Rect	nullRect = { 0, 0, 0, 0 };

	GetClip ( gSaveClip );
	ClipRect ( &nullRect );
	
	return;
	} /*DisableDrawing*/


static void EnableDrawing ( void ) {

	SetClip ( gSaveClip );
	
	return;
	} /*EnableDrawing*/


SInt32 CalcValueFromPoint (ControlHandle hControl, Point thePoint) {
	
	/*Figure where we are in scroll bar terms.*/

	SInt32 theValue = 0, theRange, theDistance, thePin;
	Rect rectControl, indicatorbounds;
	WindowPtr lpWindow;
	long thumbheight = 16;
	long thumbwidth = 16;
	RgnHandle indicatorregion = NewRgn ();
	long gTotalVSizeAdjust, gTotalWidthAdjust;
	short baselineoffset;
	
	GetControlRegion (hControl, kControlIndicatorPart, indicatorregion);
	GetRegionBounds (indicatorregion, &indicatorbounds);
	
	thumbheight = indicatorbounds.bottom - indicatorbounds.top;
	
	thumbwidth = indicatorbounds.right - indicatorbounds.left;
	
	DisposeRgn (indicatorregion);
	
	gTotalVSizeAdjust = ((kScrollArrowWidth * 2) + thumbheight);
	
	gTotalWidthAdjust = ((kScrollArrowWidth * 2) + thumbwidth);

	lpWindow = shellwindow; // (*hControl)->contrlOwner;
	
	zerorect (&rectControl);
	
	GetBestControlRect (hControl, &rectControl, &baselineoffset);
	
	//rectControl = (*hControl)->contrlRect;
	
	theRange = GetControlMaximum ( hControl ) - GetControlMinimum ( hControl );
	
	if (flverticalscroll) {
	
		// Scroll distance adjusted for scroll arrows and the thumb
		theDistance = rectControl.bottom - rectControl.top - gTotalVSizeAdjust;
		// Pin thePoint to the middle of the thumb
		thePin = rectControl.top + (thumbheight / 2);
		thePin = lastpoint;
		theValue = ((thePoint.v - thePin) * theRange) / theDistance;
		} /*if*/
	
	else { /*horizontal scrolling*/
		
		theDistance = rectControl.right - rectControl.left - gTotalWidthAdjust;

		thePin = rectControl.left + (thumbwidth / 2);
		
		thePin = lastpoint;
		
		theValue = ((thePoint.h - thePin) * theRange) / theDistance;
		} /*else*/

	theValue += gValueSlop;
	
	if ( theValue < GetControlMinimum ( hControl ) )
		theValue = GetControlMinimum ( hControl );
	else if ( theValue > GetControlMaximum ( hControl ) )
		theValue = GetControlMaximum ( hControl );

	return theValue;
	} /*CalcValueFromPoint*/
	
	
pascal void ScrollThumbActionProc (void) {

	SInt32 theValue;
    hdlscrollbar hscrollbar;
    Point thePoint;
    Rect theRect;
    long ctscroll;
	tydirection dir;
	long currvalue;
	hdlwindowinfo h = shellwindowinfo;
	short baselineoffset;

    if (h == nil) /*defensive driving*/
    	return;

	hscrollbar = (**h).vertscrollbar;
	
	if (!flverticalscroll)
		hscrollbar = (**h).horizscrollbar;
	
	zerorect (&theRect);
	
	GetBestControlRect (hscrollbar, &theRect, &baselineoffset);
 	//theRect = (**hscrollbar).contrlRect;
 	
 	if (flverticalscroll)
		
		insetrect (&theRect, -kThumbTrackLengthSlop, -kThumbTrackWidthSlop);
		
 	else
		
		insetrect (&theRect, -kThumbTrackWidthSlop, -kThumbTrackLengthSlop);
	
    GetMouse (&thePoint);
        
    if (pointinrect (thePoint, theRect))
		
		theValue = CalcValueFromPoint (hscrollbar, thePoint);
		
	else
		
		theValue = gStartValue;
		
	currvalue = (**h).vertscrollinfo.cur;
	
	if (theValue != GetControlValue (hscrollbar)) {	// if we scrolled
		
		EnableDrawing ();

		ctscroll = theValue - GetControlValue (hscrollbar);
		
		dir = scrolldirection (flverticalscroll, ctscroll > 0);	
		
	 	(*shellglobals.scrollroutine) (dir, false, abs (ctscroll));
	 	
		(**h).vertscrollinfo.cur = theValue;

		DisableDrawing ();
		} /*if*/
	
	return;
	} /*ScrollThumbActionProc*/


static OSErr BeginThumbTracking ( ControlRef theControl ) {

	OSErr theErr = noErr;
	Point thePoint;

	gControl = theControl;
	gStartValue = GetControlValue ( theControl );
	
	gValueSlop = 0;
	GetMouse ( &thePoint );
	
	if (flverticalscroll)
		lastpoint = thePoint.v;
	else
		lastpoint = thePoint.h;
	
	gValueSlop = GetControlValue ( theControl ) - CalcValueFromPoint ( theControl, thePoint );
		
	gSaveClip = NewRgn ( );

	DisableDrawing ( );
	
	return theErr;
	} /*BeginThumbTracking*/
	

static void EndThumbTracking ( void ) {
	
//	hdlwindowinfo h = shellwindowinfo;

	EnableDrawing ();

	DisposeRgn ( gSaveClip );

//	if (flverticalscroll)
//		setscrollbarcurrent ((**h).vertscrollbar, (**h).vertscrollinfo.cur);
//	else
//		setscrollbarcurrent ((**h).horizscrollbar, (**h).horizscrollinfo.cur);

	return;
	} /*EndThumbTracking*/


extern void shellinitscroll ();

void shellinitscroll(void)
{
	//Code change by Timothy Paustian Saturday, July 22, 2000 12:04:35 AM
	//Needed in shellscroll
		#if TARGET_RT_MAC_CFM
			shellvertscrollDesc = NewControlActionUPP(shellvertscroll);
			shellhorizscrollDesc = NewControlActionUPP(shellhorizscroll);
			shelllivescrollupp = NewControlActionUPP (ScrollThumbActionProc);
		#endif
}

extern void shellshutdownscroll ();

void shellshutdownscroll(void)
{
		#if TARGET_RT_MAC_CFM
			DisposeControlActionUPP(shellvertscrollDesc);
			DisposeControlActionUPP(shellhorizscrollDesc);
			DisposeControlActionUPP (shelllivescrollupp);
		#endif
}


void shellscroll (boolean flvert, hdlscrollbar sb, short part, Point pt) {
	
//	register WindowPtr w = shellwindow;
	register long oldscrollbarcurrent;
	
	//pushclip ((*w).portRect); /*allow drawing in whole window%/
		
	if (part == kControlIndicatorPart) {
		
		oldscrollbarcurrent = getscrollbarcurrent (sb);
		
			
			gControl = sb;
			
			flverticalscroll = flvert;
			
			gStartValue = oldscrollbarcurrent;
			
			BeginThumbTracking (sb);
			
			TrackControl (sb, pt, (ControlActionUPP) shelllivescrollupp);
			
			EndThumbTracking ();			
		
		}
	
	else {
	
		if (flvert)
		
			TrackControl (sb, pt, shellvertscrollUPP);
			
		else
		
			TrackControl (sb, pt, shellhorizscrollUPP);
		}
	
	/*
	popclip ();
	*/
	} /*shellscroll*/
	



