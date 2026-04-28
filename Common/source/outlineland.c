
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

#include <Packages.h>
#include <GestaltEqu.h>
#include "op.h"
#include "opinternal.h"
#include "wpengine.h"
#include "outlinewires.h"
#include "outlinefinder.h"
#include "launch.h"




#define fastinserttoken	'fins'

#define namecolwidth	180
#define kindcolwidth	40

#define findersmalliconlist 131 /*miscellaneous small icons*/


enum {
	
	desktopicon = 0,
	diskicon,
	appleshareicon,
	whitefoldericon,
	blackfoldericon,
	applicationicon,
	documenticon,
	stationeryicon,
	trashcanicon,
	macintoshicon,
	lockedicon,
	whiteopenfoldericon,
	blackopenfoldericon
	} tyfindericon;




typedef struct tydata { /*what app.appdata points to*/
	
	boolean flactive;
	
	hdloutlinerecord heditrecord;
	
	boolean displaydirty; /*if true, window is updated on next idle loop*/
	
	Handle hinsertedtext; /*used in implementing the fast insert event*/
	
	PicHandle macpicture; /*used for printing*/
	} tydata, **hdldata;


typedef struct tyoptionsrecord { /*information we save in every data file*/
	
	short selstart, selend;
	
	char waste [64];
	} tyoptionsrecord, **hdloptionsrecord;



	
static boolean setglobals (void) {
	
	hdldata hd = (hdldata) app.appdata;
	
	if (hd != nil) { /*a window is open*/
	
		op_set_outlinedata((**hd).heditrecord);
		
		outlinewindow = (**app.appwindow).macwindow;
		
		outlinewindowinfo = app.appwindow;
		
		if (opeditsetglobals ())
			wpsetglobals ();
		}
	
	return (true);
	} /*setglobals*/


static boolean preexpand (hdlheadrecord hnode, short ctlevels) {
	
	register hdloutlinerecord ho = op_get_outlinedata();
	
	if ((**hnode).headlinkright != hnode)
		return (true);
	
	if ((**hnode).flfolder) {
		
		opstartinternalchange ();
		
		opfinderexpand (hnode, ctlevels);
		
		opendinternalchange ();
		}
	
	return (true);
	} /*preexpand*/


static boolean postcollapse (hdlheadrecord hnode) {
	
	killundo ();
	
	opstartinternalchange ();
	
	opdeletesubs (hnode);
	
	opendinternalchange ();
	
	return (true);
	} /*postcollapse*/


static short geticonnum (hdlheadrecord hnode) {
	
	tyfinderinfo info;
	
	if (opgetrefcon (hnode, &info, sizeof (info))) {
		
		if (info.flvolume)
			return (diskicon);
		
		if (info.flfolder)
			return (whitefoldericon);
		
		if (info.filetype == 'APPL')
			return (applicationicon);
		
		if (info.flstationery)
			return (stationeryicon);
		}
	
	if (hnode == (**op_get_outlinedata()).hsummit)
		return (macintoshicon);
	
	return (documenticon);
	} /*geticonnum*/


static boolean measureline (hdlheadrecord hnode) {
	
	} /*measureline*/


static ostypetostring (OSType type, bigstring bs) {
	
	bs [0] = 4;
	
	moveleft (&type, &bs [1], 4);
	} /*ostypetostring*/

	
static boolean drawline (hdlheadrecord hnode) {
	
	register hdlheadrecord h = hnode;
	Point pt;
	Rect r;
	short listnum;
	short iconnum;
	tyfinderinfo info;
	byte typestring [6];
	boolean fltextmode = opeditingtext (hnode);
	
	opgetrefcon (hnode, &info, sizeof (info));
	
	GetPen (&pt);
	
	setrect (&r, pt.v - 12, pt.h - 2, pt.v + 4, pt.h + 14);
	
	listnum = findersmalliconlist;
	
	if (fltextmode)
		++listnum;
	
	plotsmallicon (r, listnum, geticonnum (h), false);
	
	movepento (pt.h + widthsmallicon, pt.v);
	
	if (info.flalias)		
 		pushstyle ((**op_get_outlinedata()).fontnum, (**op_get_outlinedata()).fontsize, italic);
	
	if (fltextmode)
		opeditupdate ();
	else
		pendrawstring ((**h).headstring);
	
	if (info.flalias)
		popstyle (); 
	
	movepento (pt.h + namecolwidth, pt.v);
	
	ostypetostring (info.filetype, typestring);
	
	pendrawstring (typestring);
	
	return (true);
	} /*drawline*/


static boolean mouseinline (hdlheadrecord hnode) {
	
	register hdloutlinerecord ho = op_get_outlinedata();
	Point pt = mousestatus.localpt;
	FSSpec fs;
	boolean flredraw = false;
	
	if (!(**ho).fltextmode) { /*changed!*/
		
		(**ho).flcursorneedsdisplay = true; /*make sure opmoveto does something*/
		
		(**ho).fltextmode = true;
		
		flredraw = true;
		}
	
	opclearallmarks ();
	
	opmoveto (hnode); /*potentially re-position the bar cursor*/ 
	
	if (flredraw)
		opinvalnode (hnode);
	
	opupdatenow (); /*be sure any changes are visible now*/
	
	if (pt.h > optextindent (hnode) - 3)
		opeditclick (pt);
	
	else {
		if ((**hnode).flhfsnode) {
			
			opeditselectall ();
			
			if (mousestatus.fldoubleclick) {
				
				if (opgetfilespec (hnode, &fs))
					launchusingfinder (fs);
				}
			}
		}
	
	return (false); /*we've handled it*/
	} /*mouseinline*/

	
static boolean hasdynamicsubs (hdlheadrecord hnode) {
	
	tyfinderinfo info;
	
	if (opgetrefcon (hnode, &info, sizeof (info)))
		return (info.ctfiles > 0);
	
	return (false);
	} /*hasdynamicsubs*/


static boolean getlinedisplayinfo (hdlheadrecord hnode, Rect *prect, short *textstart) {
	
	/*
	make adjust rect and textstart to account for our icons
	*/
	
	*textstart += widthsmallicon;
	
	/*
	(*prect).right += widthsmallicon;
	*/
	
	(*prect).right = *textstart + widthsmallicon + namecolwidth + kindcolwidth;
	
	return (true);
	} /*opgetnodedisplayinfo*/


static boolean textchanged (hdlheadrecord hnode, bigstring bsorig) {
	
	return (opfindertextchanged (hnode, bsorig));
	} /*textchanged*/


static boolean lineinserted (hdlheadrecord hnode) {
	
	return (opfinderlineinserted (hnode));
	} /*lineinserted*/


static boolean linedeleted (hdlheadrecord hnode) {
	
	return (opfinderlinedeleted (hnode));
	} /*linedeleted*/


static boolean setscrollbars (void) {
	
	register hdloutlinerecord ho = op_get_outlinedata();
	register hdlappwindow ha = app.appwindow;
	
	setscrollbarinfo ((**ha).vertbar, (**ho).vertmin, (**ho).vertmax, (**ho).vertcurrent);
	
	setscrollbarinfo ((**ha).horizbar, (**ho).horizmin, (**ho).horizmax, (**ho).horizcurrent);
	} /*setscrollbars*/


static void setupoutline (void) {
	
	register hdloutlinerecord ho = op_get_outlinedata();
	
	(**ho).flwindowopen = true;
	
	(**ho).preexpandcallback = &preexpand;
	
	(**ho).postcollapsecallback = &postcollapse;
	
	(**ho).getlinedisplayinfocallback = (callback) &getlinedisplayinfo;
	
	/*
	(**ho).measurelinecallback = (callback) &measureline;
	*/
	
	(**ho).drawlinecallback = (callback) &drawline; /*the normal text-drawing routine*/
	
	(**ho).mouseinlinecallback = &mouseinline;
	
	(**ho).hasdynamicsubscallback = (callback) &hasdynamicsubs;
	
	(**ho).textchangedcallback = (callback) &textchanged;
	
	(**ho).insertlinecallback = (callback) &lineinserted;
	
	(**ho).deletelinecallback = (callback) &linedeleted;
	
	(**ho).copyrefconcallback = (callback) &opfindercopyrefcon;
	
	(**ho).releaserefconcallback = (callback) &opfinderreleaserefcon;
	
	(**ho).validatedragcallback = &opfindervalidatedrag;
	
	(**ho).predragcallback = &opfinderpredrag;
	
	(**ho).dragcopycallback = &opfinderdragcopy;
	
	(**ho).setscrollbarsroutine = setscrollbars;
	
	opgetdisplayinfo ();
	
	(**(**ho).hsummit).flfolder = true;
	} /*setupoutline*/

	
static boolean newrecord (void) {
	
	hdlappwindow ha = app.appwindow;
	hdldata hdata = nil;
	bigstring bs;
	boolean fl;
	
	if (!newclearhandle (longsizeof (tydata), (Handle *) &hdata))
		return (false);
	
	setstringlength (bs, 0);
	
	/*
	pushstyle ((**ha).defaultfont, (**ha).defaultsize, (**ha).defaultstyle);
	*/
	
	outlinewindowinfo = ha;
	
	fl = opnewrecord ((**ha).contentrect);
	
	/*
	popstyle ();
	*/
	
	if (!fl)
		goto error;
	
	/*
	(**op_get_outlinedata()).dirtyroutine = (callback) &madechanges;
	*/
	
	(**hdata).heditrecord = op_get_outlinedata();
	
	app.appdata = (Handle) hdata;
	
	setupoutline ();
	return (true);
	
	error:
	
	disposehandle ((Handle) hdata);
	
	return (false);
	} /*newrecord*/


static boolean disposerecord (void) {
	
	hdldata hd = (hdldata) app.appdata;
	
	killundo ();
	
	opdisposeoutline ((**hd).heditrecord, false);
	
	disposehandle ((Handle) hd);
	
	return (true);
	} /*disposerecord*/


static boolean activate (boolean flactive) {
	
	hdldata hd = (hdldata) app.appdata;
	
	(**hd).flactive = flactive;
	
	opactivate (flactive);
	
	return (true);
	} /*activate*/
	
	
static boolean update (void) {
	
	opupdate ();
	
	return (true);
	} /*update*/


static short comparestrings (bigstring bs1, bigstring bs2) {

	/*
	return zero if the two strings (pascal type, with length-byte) are
	equal.  return -1 if bs1 is less than bs2, or +1 if bs1 is greater than
	bs2.
	
	use the Machintosh international utility routine IUCompString, which 
	performs dictionary string comparison and accounts for accented characters
	*/
	
	return (IUCompString (bs1, bs2));
	} /*comparestrings*/


static hdlheadrecord op1stsibling (hdlheadrecord hnode) {
	
	while (opchaseup (&hnode)) {}
	
	return (hnode);
	} /*op1stsibling*/


static boolean opfindhead (hdlheadrecord hfirst, bigstring bs, hdlheadrecord *hnode) {
	
	/*
	search starting with hfirst and step through its siblings looking
	for a headline that exactly matches bs.
	*/
	
	register hdlheadrecord nomad, nextnomad;
	bigstring bsnomad;
	bigstring bsfind;
	bigstring bsbest;
	register hdlheadrecord hbest = nil; /*no candidate found*/
	
	*hnode = nil; /*default returned value*/
	
	nomad = op1stsibling (hfirst);
	
	copystring (bs, bsfind); /*work on a copy*/
	
	alllower (bsfind); /*unicase*/
	
	while (true) {
		
		getheadstring (nomad, bsnomad);
		
		alllower (bsnomad);
		
		switch (comparestrings (bsnomad, bsfind)) {
			
			case 0: /*string equal*/
				*hnode = nomad;
				
				return (true);
			
			case -1: /*key less than name*/
				break;
			
			case +1: /*key greather than name*/
				if ((hbest == nil) || (comparestrings (bsnomad, bsbest) == -1)) {
					
					copystring (bsnomad, bsbest);
					
					hbest = nomad;
					}
			}
		
		nextnomad = (**nomad).headlinkdown;
		
		if (nextnomad == nomad)
			break;
			
		nomad = nextnomad;
		} /*while*/
	
	if (hbest == nil) /*didn't find any item that would follow bsname*/
		hbest = nomad; /*select last name*/
	
	*hnode = hbest;
	
	return (false);
	} /*opfindhead*/


static boolean opnavigationkey (byte chkey) {
	
	/*
	add chkey to the typing buffer bsselect, resetting the buffer 
	depending on elapsed time.  select the table entry that whose name 
	starts with a string equal to or greater than the resulting string.
	the net effect should be much like typing in standard file.
	
	note that statics  are used to retain information between calls.
	*/
	
	static bigstring bsselection;
	static long timelastkey = 0L;
	register byte c = chkey;
	register long timethiskey;
	hdlheadrecord hnode;
	
	timethiskey = appletevent.when;
	
	if ((timethiskey - timelastkey) > (2 * KeyThresh)) /*long enough gap, reset*/
		setemptystring (bsselection);
	
	timelastkey = timethiskey; /*set up for next time*/
	
	pushchar (chkey, bsselection);
	
	opfindhead ((**op_get_outlinedata()).hbarcursor, bsselection, &hnode);
	
	opmoveto (hnode);
	
	return (true);
	} /*opnavigationkey*/


static boolean keystroke (void) { 
	
	register char ch = keyboardstatus.chkb;
	
	if (isprint (ch) && (keyboardstatus.ctmodifiers == 0)) {
		
		if (!(**op_get_outlinedata()).fltextmode) {
			
			opnavigationkey (ch);
			
			return (true);
			}
		}
	
	opkeystroke ();
	
	return (true);
	} /*keystroke*/
	

static boolean mousedown (void) {
	
	opmousedown (mousestatus.localpt);
	
	return (true);
	} /*mousedown*/
	

/*
static boolean scrollto (void) {
	
	hdlappwindow ha = app.appwindow;
	short dh, dv;
	register hdloutlinerecord ho = op_get_outlinedata();
	
	dh = getscrollbarcurrent ((**ha).horizbar) - (**ho).horizcurrent;
	
	dv = getscrollbarcurrent ((**ha).vertbar) / getfontheight () - (**ho).vertcurrent;
	
	switch (sgn (dh)) {
		
		case 1:
			opscroll (left, false, dh);
			
			break;
		
		case -1:
			opscroll (right, false, -dh);
			
			break;
		}
	
	switch (sgn (dv)) {
		
		case 1:
			opscroll (up, false, dv);
			
			break;
		
		case -1:
			opscroll (down, false, -dv);
			
			break;
		}
	
	return (true);
	} /*scrollto*/
	

static boolean packrecord (Handle *hpacked) {
	
	return (oppack (hpacked));
	} /*packrecord*/
	
	
static boolean unpackrecord (Handle hpacked) {
	
	/*
	In the Applet Toolkit, when a file is opened, the app's newrecord callback is called, 
	then the app's unpack callback is called. In Frontier it works differently -- the unpack 
	callback is expected to allocate a new record. 
	*/
	
	hdldata hd = (hdldata) app.appdata;
	register hdlappwindow ha = app.appwindow;
	hdloutlinerecord houtline;
	long ixload = 0;
	hdloutlinerecord ho;
	
	opdisposeoutline ((**hd).heditrecord, false);
	
	(**hd).heditrecord = nil;
	
	if (!opunpack (hpacked, &ixload))
		return (false);
	
	ho = op_get_outlinedata();
	
	(**hd).heditrecord = ho;
	
	(**ho).windowrect = (**ha).windowrect;
	
	(**ho).outlinerect = (**ha).contentrect;
	
	setupoutline ();
	
	opgetdisplayinfo ();
	
	/***scrollto (); /*use current values of scrollbar to determine position*/
	
	return (true);
	} /*unpackrecord*/
	
	
static boolean getcontentsize (void) {
	
	hdlappwindow ha = app.appwindow;
	long height, width;
	
	opgetoutinesize (&width, &height);
	
	(**ha).zoomheight = height;
	
	(**ha).zoomwidth = width;
	
	return (true);
	} /*getcontentsize*/
	

static boolean windowresize (void) {
	
	hdlappwindow ha = app.appwindow;
	Rect r1 = (**ha).contentrect;
	Rect r2 = (**ha).oldcontentrect;
	
	/*
	InsetRect (&(**ha).contentrect, texthorizinset, textvertinset);
	
	InsetRect (&(**ha).oldcontentrect, texthorizinset, textvertinset);
	*/
	
	opresize ((**ha).contentrect);
	
	/*
	(**ha).contentrect = r1;
	
	(**ha).oldcontentrect = r2;
	*/
	
	return (true);
	} /*windowresize*/
	
	
static boolean selectall (void) {
	
	opselectall ();
		
	return (true);
	} /*selectall*/


static boolean haveselection (void) {
	
	return (true);
	} /*haveselection*/


static boolean setselectioninfo (void) {
	
	return (opsetselectioninfo ());
	} /*setselectioninfo*/
	
		
static boolean setfont (void) {
	
	pushundoaction (undoformatstring);
	
	opsetfont ((**outlinewindowinfo).selectioninfo.fontnum);
	
	return (true);
	} /*setfont*/
	
	
static boolean setfontsize (void) {
	
	pushundoaction (undoformatstring);
	
	opsetsize ((**outlinewindowinfo).selectioninfo.fontsize);
	
	return (true);
	} /*setfontsize*/
	
	
static boolean setstyle (void) {
	
	return (false);
	} /*setstyle*/


static boolean deleteselectedtext (void) {
	
	opclear ();
	
	opfindercommitchanges ();
	
	return (true);
	} /*deleteselectedtext*/


static boolean copy (Handle *hpacked) {
	
	Handle hscrap;
	tyscraptype type;
	boolean flconverted;
	
	if(!opcopy ())
		return (false);
	
	if (!shellgetscrap (&hscrap, &type))
		return (false);
	
	if (type == opscraptype) /*structure scrap*/
		opfinderexportscrap ((hdloutlinerecord) hscrap);
	
	hscrap = nil;
	
	if (exportshellscrap (type, &hscrap, &flconverted)) { /*force conversion to contiguous form*/
		
		assert (flconverted);
		
		if (enlargehandle (hscrap, sizeof (type), &type))
			*hpacked = hscrap;
		else
			disposehandle (hscrap);
		}
	
	/*shelldisposescrap ();*/ /*leave around for ensuing copytext call*/
	
	return (true);	
	} /*copy*/


static boolean paste (Handle hpacked) {
	
	Handle hscrap;
	tyscraptype type;
	
	/*if (opscraphook (hpacked) && wpscraphook (hpacked)) { /*neither hook bit*/
	
	if (!shellgetscrap (&hscrap, &type)) {
		
		if (!popfromhandle (hpacked, sizeof (type), &type))
			return (false);
		
		shellsetscrap (hpacked, type, (callback) &disposehandle, nil);
		}
	
	oppaste ();
	
	/*shelldisposescrap ();*/
	
	return (true);
	} /*paste*/


static boolean copytext (Handle *htext) {
	
	Handle hscrap = nil;
	tyscraptype type;
	boolean flconverted = false;
	
	if (!exportshellscrap (textscraptype, &hscrap, &flconverted)) { /*not part of clipboard sequence*/
		
		if (!opcopy ())
			return (false);
		
		if (!exportshellscrap (textscraptype, &hscrap, &flconverted))
			return (false);
		}
	
	assert (flconverted);
	
	*htext = hscrap;
	
	/*shelldisposescrap ();*/
	
	return (flconverted);
	} /*copytext*/


static boolean pastetext (Handle htext) {
	
	shellsetscrap (htext, textscraptype, (callback) &disposehandle, nil);
	
	oppaste ();
	
	shelldisposescrap ();
	
	return (true);
	} /*pastetext*/

static boolean tryhandlefastverb (void) {
	
	if (app.appwindow == nil)
		return (false);
	
	return (handlefastverb ());
	} /*handlefastverb*/


static boolean tryhandleverb (void) {
	
	if (app.appwindow == nil)
		return (false);
	
	return (handleverb ());
	} /*handleverb*/


	


static boolean idle (void) {
	
	/*
	this code is guaranteed to be running when minapp is the current context,
	not from a fast event handler. so we process display oriented tasks 
	generated by fast event handlers in this callback.
	*/
	
	hdlappwindow ha = app.appwindow;
	hdldata hd = (hdldata) app.appdata;
	Point pt = mousestatus.localpt;
	
	if (hd == nil) {
		
		setcursortype (cursorisarrow);
		
		return (true);
		}
	
	opidle ();
	
	opfindercommitchanges ();
	
	opsetcursor ();
	
	return (true);
	} /*idle*/


static boolean checkgestalts (void) {
	
	long attrs;
	
	if (Gestalt (gestaltFindFolderAttr, &attrs) != noErr)
		return (false);
	
	if ((attrs & (1 << gestaltFindFolderPresent)) == 0)
		return (false);
	
	return (true);
	} /*checkgestalts*/


void main (void) {

	clearbytes (&app, longsizeof (app)); /*init all fields to 0*/
	
	app.creator = 'USOP';
	
	app.filetype = 'OPTX';
	
	app.notsaveable = true;
	
	app.resizeable = true;
	
	app.eraseonresize = true;
	
	app.horizscroll = true;
	
	app.vertscroll = true;
	
	app.usecolor = true;
	
	app.defaultfont = monaco;
	
	app.defaultsize = 9;
	
	app.defaultstyle = 0;
	
	app.setglobalscallback = &setglobals;
	
	app.newrecordcallback = &newrecord;
	
	app.updatecallback = &update;
	
	app.activatecallback = (tyappbooleancallback) &activate;
	
	app.disposerecordcallback = &disposerecord;
	
	app.keystrokecallback = &keystroke;
	
	app.mousecallback = &mousedown;
	
	app.idlecallback = &idle;

	app.packcallback = &packrecord;
	
	app.unpackcallback = &unpackrecord;
	
	app.windowresizecallback = &windowresize;
	
	/*
	app.scrolltocallback = &scrollto;
	*/
	
	app.scrollcallback = &opscroll;
	
	app.resetscrollcallback = &opresetscrollbars;
	
	app.getcontentsizecallback = &getcontentsize;
	
	app.selectallcallback = &selectall;
	
	app.haveselectioncallback = &haveselection;
	
	app.setselectioninfocallback = &setselectioninfo;
	
	app.setfontcallback = &setfont;
	
	app.setsizecallback = &setfontsize;
	
	app.setstylecallback = &setstyle;
	
	app.clearcallback = &deleteselectedtext;
	
	app.copycallback = &copy;
	
	app.pastecallback = &paste;
	
	app.gettextcallback = &copytext;
	
	app.puttextcallback = &pastetext;
	
	app.getundoglobalscallback = &opeditgetundoglobals;
	
	app.setundoglobalscallback = &opeditsetundoglobals;
	
	app.iacmessagecallback = &tryhandleverb;
	
	app.iacfastmessagecallback = &tryhandlefastverb;
	
	/*
	
	app.gettextcallback = &copytext;
	
	app.puttextcallback = &pastetext;
	
	app.openprintcallback = &openprint;
	
	app.pagesetupcallback = &pagesetup;
	
	app.printpagecallback = (tyappshortcallback) &printpage;
	
	app.closeprintcallback = &closeprint;
	
	app.opendoccallback = &snuffyopendoc;
	*/
	
	/*
	app.menucallback = &snuffymenufilter;
	*/
	
	appletinitmanagers ();
	
	if (!checkgestalts ())
		return;
	
	initprint (); /*wp needs to know how big the paper rectangle is*/
	
	wpinit ();
	
	opprefs.fltabkeyreorg = false;
	
	if (!newappwindow ("\pOutLine Finder", true))
		return;
	
	runapplet ();
	
	shelldisposescrap ();
	} /*main*/



