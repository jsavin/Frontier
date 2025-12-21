
/*	$Id$    */

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

#include "frontier.h"
#include "standard.h"

#include "error.h"
#include "memory.h"
#include "strings.h"
#include "ops.h"
#include "resources.h"
#include "lang.h"
#include "langexternal.h"
#include "langinternal.h"
#include "langsystem7.h"
#include "process.h"
#include "kernelverbs.h"
#include "file.h"
#include "filealias.h"
#include "tablestructure.h"
#include "kernelverbdefs.h"
#include "shell.rsrc.h"
#include "byteorder.h"		/* 2006-04-16 aradke: for SWAP_REZ_BYTE_ORDER */
#include "oplist.h"			/* 2006-04-23 creedon */



	#define chpathseparator ':'
	


boolean flsupportslargevolumes = false; /*true if volumes over 2 GB are supported by the OS*/




typedef enum tyfiletoken { /*verbs that are processed by file.c*/
	
	filecreatedfunc, 
	
	filemodifiedfunc, 
	
	filetypefunc, 
	
	filecreatorfunc,
	
	setfilecreatedfunc, 
	
	setfilemodifiedfunc, 
	
	setfiletypefunc,
	
	setfilecreatorfunc,
	
	fileisfolderfunc,
	
	fileisvolumefunc,
	
	fileislockedfunc, 
	
	filelockfunc,
	
	fileunlockfunc,
	
	filecopyfunc, 
	
	filecopydataforkfunc,
	
	filecopyresourceforkfunc,
	
	filedeletefunc,
	
	filerenamefunc,
	
	fileexistsfunc,
	
	filesizefunc,
	
	filefullpathfunc,

	filegetpathfunc,
	
	filesetpathfunc,
	
	filefrompathfunc, 
	
	folderfrompathfunc, 
	
	getsystempathfunc,
	
	getspecialpathfunc,
	
	newfunc,
	
	newfolderfunc,
	
	newaliasfunc,
	
	sfgetfilefunc, 
	
	sfputfilefunc,
	
	sfgetfolderfunc,
	
	sfgetdiskfunc,
	
	filegeticonposfunc,
	
	fileseticonposfunc,
	
	getshortversionfunc,
	
	setshortversionfunc,
	
	getlongversionfunc,
	
	setlongversionfunc,
	
	filegetcommentfunc,
	
	filesetcommentfunc,
	
	filegetlabelfunc,
	
	filesetlabelfunc,
	
	filefindappfunc,
	
	/*
	fileeditlinefeedsfunc,
	*/
	
	fileisbusyfunc,
	
	filehasbundlefunc,
	
	filesetbundlefunc,
	
	fileisaliasfunc,
	
	fileisvisiblefunc,
	
	filesetvisiblefunc,
	
	filefollowaliasfunc,
	
	filemovefunc,
	
	/*
	filesinfolderfunc,
	*/
	
	volumeejectfunc,
	
	volumeisejectablefunc, 
	
	volumefreespacefunc,
	
	volumesizefunc,
	
	volumeblocksizefunc,
	
	filesonvolumefunc,
	
	foldersonvolumefunc,
	
	unmountvolumefunc,
	
	mountservervolumefunc,
	
	/*
	filelaunchfunc,
	*/
	
	/*start of new verbs added by DW, 7/27/91*/
	
		findinfilefunc,
		
		countlinesfunc,
		
		openfilefunc,
		
		closefilefunc,
		
		endoffilefunc,
		
		setendoffilefunc,
		
		getendoffilefunc,

		setpositionfunc,

		getpositionfunc,

		readlinefunc,
		
		writelinefunc,
		
		readfunc,
		
		writefunc,
		
		comparefunc,
	
	// end of new verbs added by DW, 7/27/91
	
	writewholefilefunc,
		
	getpathcharfunc,

	volumefreespacedoublefunc,

	volumesizedoublefunc,
	
	getmp3infofunc,
	
	readwholefilefunc,			// 2006-04-11 aradke
	
	getlabelindexfunc,			// 2006-04-23 creedon
	
	setlabelindexfunc,			// 2006-04-23 creedon
	
	getlabelnamesfunc,			// 2006-04-23 creedon
	
	getposixpathfunc,			// 2006-10-07 creedon
	
	ctfileverbs
	
	} tyfiletoken;


typedef enum tyreztoken {
	
	rezgetresourcefunc,
	
	rezputresourcefunc,
	
	rezgetnamedresourcefunc,
	
	rezputnamedresourcefunc,
	
	rezcountrestypesfunc,
	
	rezgetnthrestypefunc,
	
	rezcountresourcesfunc,
	
	rezgetnthresourcefunc,
	
	rezgetnthresinfofunc,
	
	rezresourceexistsfunc,
	
	reznamedresourceexistsfunc,
	
	rezdeleteresourcefunc,
	
	rezdeletenamedresourcefunc,
	
	rezgetresourceattrsfunc,
	
	rezsetresourceattrsfunc,
	
	ctrezverbs
	} tyreztoken;




static boolean getpathvalue (hdltreenode hparam1, short pnum, ptrfilespec fspath) {
	
	/*
	get a path parameter for the parameter list.
	
	2.1b2 dmb: now that we use filespecs everywhere, we don't have much to do!
	*/
	
	return (getfilespecvalue (hparam1, pnum, fspath));
	
	/*
	tyvaluerecord v;
	bigstring bspath;
	
	if (!getparamvalue (hparam1, pnum, &v))
		return (false);
	
	switch (v.valuetype) {
		
		case stringvaluetype:
			
			pullstringvalue (&v, bspath);
			
			filecheckdefaultpath (bspath);
			
			if (!pathtofilespec (bspath, fspath)) {
				
				filenotfounderror (bspath);
				
				return (false);
				}
			
			break;
		
		default:
			if (!coercetofilespec (&v))
				return (false);
			
			*fspath = **(*v).data.filespecvalue;
			
			break;
	
		return (true);
		}
	*/
	
	return (true);
	} /*getpathvalue*/


static boolean getvolumevalue (hdltreenode hparam1, short pnum, ptrfilespec fsvol) {
	
	//
	// get a volume path parameter for the parameter list.
	//
	// make sure that a colon is included so a volume name isn't interpreted as
	// a partial path
	//
	// 2.1b11 dmb:	ooops, we were copying bsvol into fsvol.name, potentially 
	//				overflowing the str64. fileparsevolname now returns the vol
	//				name.  note that if the caller needs to distinguish between
	//				volumes and non-volumes, it can't call us.
	//
	// 2.1b8 dmb:	don't use pathtofilespec to convert the volume name, because
	//				FSMakeFSSpec will prompt the user to insert the disk if the
	//				volume has been ejected
	
	
	bigstring bsvol;
	tyvaluerecord v;
	
	if (!getparamvalue (hparam1, pnum, &v))
		return (false);
	
	switch (v.valuetype) {
		
		case stringvaluetype: // already a string, easy case
			
			pullstringvalue (&v, bsvol);
			
			if ( ! fileparsevolname ( bsvol, fsvol ) ) {
				
				setoserrorparam (bsvol);
				
				oserror (errorVolume);
				
				return (false);
				}
			
			break;
		
		default:
			if (!coercetofilespec (&v))
				return (false);
			
			*fsvol = **v.data.filespecvalue;
			
			break;
		
		return (true);
		}
	
	return (true);
	} // getvolumevalue


static boolean copyfileverb (boolean fldata, boolean flresources, hdltreenode hparam1, tyvaluerecord *v) {

	//
	// 2006-06-18 creedon: for Mac, FSRef-ize
	//
	
	tyfilespec fs1, fs2;
	
	if (!getpathvalue (hparam1, 1, &fs1)) // fs1 holds the source path
		return (false);
	
	flnextparamislast = true;
	
	if (!getpathvalue (hparam1, 2, &fs2)) // fs2 holds the dest path
		return (false);
	
	if (equalfilespecs (&fs1, &fs2)) {
		
		(*v).data.flvalue = true;	// easy case is making a copy of itself
		}
	else {
			
		(*v).data.flvalue = copyfile ( &fs1, &fs2, fldata, flresources );
		}
	
	return (true);
	
	} // copyfileverb


static boolean filefrompathverb (hdltreenode hparam1, tyvaluerecord *vreturned) {
	
	//
	// 2006-06-19 creedon: for Mac, FSRef-ized
	//
	// 2.1b3 dmb: be sure to add colon to name of folder even when filespec is being used.
	//
	// 2.1b2 dmb:	do string manipulation if given a string, but otherwise work with filespecs. less critical than with
	//			folderfrompath, but might avoid full path string overflow
	//
	
	tyvaluerecord v;
	tyfilespec fs;
	bigstring bs;
	boolean flfolder;
	
	flnextparamislast = true;
	
	if (!getparamvalue (hparam1, 1, &v))
		return (false);
	
	switch (v.valuetype) {
	
		case stringvaluetype:
			pullstringvalue (&v, bs);

			flfolder = endswithpathsep(bs);

			if (flfolder)
				setstringlength (bs, stringlength (bs) - 1);
			
			filefrompath (bs, bs); // bs now holds the filename
			
			break;
		
		default:
			if ( ! coercetofilespec ( &v ) )
				return ( false );
			
			fs = **v.data.filespecvalue;

				
				macgetfilespecnameasbigstring(&fs, bs);

			
			
			fileexists (&fs, &flfolder); // don't care about return, just flfolder value
			
			
			break;
		}

	if (flfolder)
		pushchar (chpathseparator, bs);
	
	return (setstringvalue (bs, vreturned));
	} // filefrompathverb


static boolean folderfrompathverb (hdltreenode hparam1, tyvaluerecord *vreturned) {
	
	//
	// 2006-08-24 creedon: for Mac, FSRef-ized
	//
	// 2.1b2 dmb:	do string manipulation if given a string, but otherwise work with filespecs. in addition to avoiding string
	//			overflow, this preserves ability to distinguish between identically-named volumes
	//
	
	tyvaluerecord v;
	bigstring bs;
	
	flnextparamislast = true;
	
	if (!getparamvalue (hparam1, 1, &v))
		return (false);
	
	
		if (v.valuetype != stringvaluetype) {
		
			tyfilespec fs, fsparent;
			OSErr err;
			
			if (!coercetofilespec(&v))
				return (false);
			
			fs = **v.data.filespecvalue;
			
			err = macgetfilespecparent(&fs, &fsparent);
			
			if (err != noErr) {
				oserror(err);
				return (false);
				}
				
			return (setfilespecvalue(&fsparent,vreturned));
			}
	
	
	if (!coercetostring (&v))
		return (false);

	pullstringvalue (&v, bs);
	
	cleanendoffilename (bs);
	
	folderfrompath (bs, bs); /*bs now holds the foldername*/
	
	return (setstringvalue (bs, vreturned));
	} // folderfrompathverb


static boolean gettypelistvalue (hdltreenode hparam1, short pnum, tysftypelist *filetypes, ptrsftypelist *x) {
#pragma unused(pnum)
	/*
	2.1b4 dmb: new feature: accept a list of filetype for sfgetfile
	*/
	
	tyvaluerecord val;
	tyvaluerecord vitem;
	long ctitems;
	short i;
//	OSType toss;
	OSType filetype;
	
	if (!getparamvalue (hparam1, 3, &val))
		return (false);
	
	*x = filetypes; /*assume success & point to the typelist buffer*/
	
	switch (val.valuetype) {
		
		case listvaluetype:
			if (!langgetlistsize (&val, &ctitems))
				return (false);
			
			ctitems = min (ctitems, maxsftypelist);
			
			(*filetypes).cttypes = (short)ctitems;
			
			for (i = 0; i < ctitems; ++i) {
				
				if (!langgetlistitem (&val, i + 1, nil, &vitem))
					return (false);
				
				if (!coercetoostype (&vitem))
					return (false);
				
				(*filetypes).types [i] = vitem.data.ostypevalue;
				}
			
			break;
		
		default:
			if (!coercetoostype (&val))
				return (false);
			
			filetype = val.data.ostypevalue;
			
			if (filetype == 0) /*no file type specified*/
				*x = nil;
			
			else {
				
				(*filetypes).cttypes = 1;
				
				(*filetypes).types [0] = filetype;
				}
		}
	
	return (true);
	} /*gettypelistvalue*/


static boolean filedialogverb (tysfverb sfverb, hdltreenode hparam1, tyvaluerecord *vreturned) {
	
	//
	// put up one of the "standard file" dialogs.  if sfverb is sfputfileverb we use the "put" dialog, otherwise the "get" dialog.
	//
	// we take at least one parameter -- the name of a variable to receive the full path specified by the user.
	//
	// if it's the getfile dialog, we take a second parameter -- it indicates the type of the file.
	//
	// 2006-08-16 creedon: FSRef-ized
	//
	// 2005-10-06 creedon: added creator parameter
	//
	// 1991-12-27 dmb: in all cases, check the current value of the filename variable, and pass it on to sf dialog so it can
	//				potentially set default directory.
	//
	
	bigstring bsprompt;
	bigstring bsvarname;
	tyfilespec fs;
	tyvaluerecord val;
	tysftypelist filetypes;
	ptrsftypelist typelist = nil;
	hdlhashtable htable;
	boolean fl;
	OSType ostype, oscreator = kNavGenericSignature;
	bigstring bsext;
	hdlhashnode hnode;
	
	if (!getstringvalue (hparam1, 1, bsprompt))
		return (false);
	
	if (sfverb != sfgetfileverb)
		flnextparamislast = true;
	
	if (!getvarparam (hparam1, 2, &htable, bsvarname)) // returned filename holder
		return (false);
	
	if (sfverb == sfgetfileverb) { // get extra parameters for get file dialog, indicating file type(s) and file creator
		
		short ctconsumed = 3;
		short ctpositional = 3;
		tyvaluerecord lval;

		if (!gettypelistvalue (hparam1, 3, &filetypes, &typelist))
			return (false);
			
		flnextparamislast = true;
		
		setostypevalue (oscreator, &lval);

		if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x07""creator"), &lval))
			return (false);
	
		oscreator = lval.data.ostypevalue;

		}
	
	clearfilespec (&fs);
	
	//
	// if (idstringvalue (htable, bsvarname, bsfname))
	// 	filecheckdefaultpath (bsfname);
	//
	
	if (hashtablelookup (htable, bsvarname, &val, &hnode)) {
		
		if (!copyvaluerecord (val, &val))
			return (false);
		
		disablelangerror ();
		
		if (coercetofilespec (&val))
			fs = **val.data.filespecvalue;
		
		enablelangerror ();
		}

	if (sfverb == sfputfileverb) {
	
		bigstring bs;
		
		getfsfile ( &fs, bs );
		
		lastword ( bs, '.', bsext);

		if (!((stringlength (bs) == stringlength (bsext)) || (stringlength (bsext) > 4))) { // extension
			stringtoostype (bsext, &ostype);
			filetypes.cttypes = 1;
			filetypes.types [0] = ostype;
			typelist = &filetypes;
			}
		}
	
	setbooleanvalue (false, vreturned); 
	
	if (!sfdialog (sfverb, bsprompt, typelist, &fs, oscreator)) // user hit cancel
		return (true);
	
	if (!setfilespecvalue (&fs, &val))
		return (false);
	
	pushhashtable (htable);
	
	fl = langsetsymbolval (bsvarname, val);
	
	pophashtable ();
	
	if (!fl)
		return (false);
	
	exemptfromtmpstack (&val);
	
	(*vreturned).data.flvalue = true; // the user did select a file
	
	return (true);
	
	} // filedialogverb


static boolean getstringorintvalue (hdltreenode hfirst, short pnum, boolean flstring, short *intval, bigstring bsval) {
	
	/*
	if the parameter value is a string, set intval to -1 and return the string; 
	otherwise, return the value as an integer and set bsval to the empty string.
	
	7/29/91 dmb: now strictly enforce flstring; if true, paramter must coerce to a 
	string, otherwise to an int.
	*/
	
	if (flstring) {
		
		if (!getstringvalue (hfirst, pnum, bsval))
			return (false);
		
		*intval = -1;
		}
	else {
		
		if (!getintvalue (hfirst, pnum, intval))
			return (false);
		
		setemptystring (bsval);
		}
	
	return (true);
	} /*getstringorintvalue*/


static boolean getresourceverb (hdltreenode hparam1, boolean flnamed, tyvaluerecord *v) {

	/*
	2005-09-02 creedon: added support for fork parameter, see resources.c: openresourcefile and pushresourcefile
	*/ 
	
	tyfilespec fs;
	OSType type;
	short id, forktype;
	short ctconsumed = 4;
	short ctpositional = 4;
	Handle h;
	hdlhashtable htable;
	bigstring bs, bsvarname;
	tyvaluerecord val;
	
	if (!getpathvalue (hparam1, 1, &fs)) 
		return (false);
	
	if (!getostypevalue (hparam1, 2, &type))
		return (false);
	
	if (!getstringorintvalue (hparam1, 3, flnamed, &id, bs))
		return (false);
	
	if (!getvarparam (hparam1, 4, &htable, bsvarname)) /*returned handle holder*/
		return (false);
	
	flnextparamislast = true;
	
	setintvalue (resourcefork, &val); /* defaults to 1 */

	if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x04""fork"), &val))
		return (false);
	
	forktype = val.data.intvalue;

	if (!loadresourcehandle (&fs, type, id, bs, &h, forktype)) {
		
		(*v).data.flvalue = false;
		
		return (true);
		}
	
	if (!insertinhandle (h, 0L, &type, sizeof (type))) { /*out of memory*/
		
		disposehandle (h);
		
		return (false);
		}
	
	if (!langsetbinaryval (htable, bsvarname, h))
		return (false);
	
	(*v).data.flvalue = true;
	
	return (true);
	} /*getresourceverb*/


static boolean putresourceverb (hdltreenode hparam1, boolean flnamed, tyvaluerecord *v) {

	//
	// 2006-06-17 creedon: FSRef-ized
	//
	// 2005-09-02 creedon: added support for fork parameter, see resources.c: openresourcefile and pushresourcefile
	// 
	
	tyfilespec fs;
	OSType type, bintype;
	short id, forktype;
	short ctconsumed = 4;
	short ctpositional = 4;
	Handle hbinary;
	bigstring bs;
	tyvaluerecord val;
	
	if (!getpathvalue (hparam1, 1, &fs)) 
		return (false);
	
	if (!getostypevalue (hparam1, 2, &type))
		return (false);
	
	if (!getstringorintvalue (hparam1, 3, flnamed, &id, bs))
		return (false);
	
	if (!getbinaryvalue (hparam1, 4, false, &hbinary))
		return (false);
	
	pullfromhandle (hbinary, 0L, sizeof (bintype), &bintype);
	
	flnextparamislast = true;
	
	setintvalue (resourcefork, &val); /* defaults to 1 */

	if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x04""fork"), &val))
		return (false);

	forktype = val.data.intvalue;

	(*v).data.flvalue = saveresourcehandle (&fs, type, id, bs, hbinary, forktype);
	
	return (true);
	
	} // putresourceverb


static boolean countrestypesverb (hdltreenode hparam1, tyvaluerecord *v) {
	
	/*
	2005-09-02 creedon: added support for fork parameter, see resources.c: openresourcefile and pushresourcefile
	*/ 
	
	tyfilespec fs;
	short cttypes, forktype;
	short ctconsumed = 1;
	short ctpositional = 1;
	tyvaluerecord val;
	
	if (!getpathvalue (hparam1, 1, &fs)) 
		return (false);
	
	flnextparamislast = true;
	
	setintvalue (resourcefork, &val); /* defaults to 1 */

	if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x04""fork"), &val))
		return (false);
	
	forktype = val.data.intvalue;

	if (!getnumresourcetypes (&fs, &cttypes, forktype))
		cttypes = 0;
	
	setlongvalue (cttypes, v);
	
	return (true);
	} /*countrestypesverb*/


static boolean getnthrestypeverb (hdltreenode hparam1, tyvaluerecord *v) {
	
	/*
	2005-09-02 creedon: added support for fork parameter, see resources.c: openresourcefile and pushresourcefile
	*/ 
	
	tyfilespec fs;
	short n, forktype;
	short ctconsumed = 3;
	short ctpositional = 3;
	OSType type;
	hdlhashtable htable;
	bigstring bsvarname;
	tyvaluerecord val;
	boolean fl;
	
	if (!getpathvalue (hparam1, 1, &fs)) 
		return (false);
	
	if (!getintvalue (hparam1, 2, &n))
		return (false);
	
	if (!getvarparam (hparam1, 3, &htable, bsvarname)) /*returned handle holder*/
		return (false);
	
	flnextparamislast = true;
	
	setintvalue (resourcefork, &val); /* defaults to 1 */

	if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x04""fork"), &val))
		return (false);
	
	forktype = val.data.intvalue;

	if (!getnthresourcetype (&fs, n, &type, forktype)) { /*not a fatal error*/
		
		(*v).data.flvalue = false;
		
		return (true);
		}
	
	setostypevalue (type, &val);
	
	fl = langsetsymboltableval (htable, bsvarname, val);
	
	(*v).data.flvalue = fl;
	
	return (fl);
	} /*getnthrestypeverb*/


static boolean countresourcesverb (hdltreenode hparam1, tyvaluerecord *v) {
	
	/*
	2005-09-02 creedon: added support for fork parameter, see resources.c: openresourcefile and pushresourcefile
	*/ 
	
	tyfilespec fs;
	OSType type;
	short ctresources, forktype;
	short ctconsumed = 2;
	short ctpositional = 2;
	tyvaluerecord val;
	
	if (!getpathvalue (hparam1, 1, &fs)) 
		return (false);
	
	if (!getostypevalue (hparam1, 2, &type))
		return (false);
	
	flnextparamislast = true;
	
	setintvalue (resourcefork, &val); /* defaults to 1 */

	if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x04""fork"), &val))
		return (false);
	
	forktype = val.data.intvalue;

	if (!getnumresources (&fs, type, &ctresources, forktype))
		ctresources = 0;
	
	setlongvalue (ctresources, v);
	
	return (true);
	} /*countresourcesverb*/


static boolean getnthresourceverb (hdltreenode hparam1, tyvaluerecord *v) {

	/*
	2005-09-02 creedon: added support for fork parameter, see resources.c: openresourcefile and pushresourcefile
	*/ 
	
	tyfilespec fs;
	OSType type;
	short n, id, forktype;
	short ctconsumed = 5;
	short ctpositional = 5;
	Handle h;
	hdlhashtable ht1, ht2;
	bigstring bs, bs1, bs2;
	boolean fl;
	tyvaluerecord val;

	if (!getpathvalue (hparam1, 1, &fs)) 
		return (false);
	
	if (!getostypevalue (hparam1, 2, &type))
		return (false);
	
	if (!getintvalue (hparam1, 3, &n))
		return (false);
	
	if (!getvarparam (hparam1, 4, &ht1, bs1)) /*returned name holder*/
		return (false);
	
	if (!getvarparam (hparam1, 5, &ht2, bs2)) /*returned handle holder*/
		return (false);
		
	flnextparamislast = true;
	
	setintvalue (resourcefork, &val); /* defaults to 1 */

	if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x04""fork"), &val))
		return (false);
	
	forktype = val.data.intvalue;

	if (!getnthresourcehandle (&fs, type, n, &id, bs, &h, forktype)) {
		
		(*v).data.flvalue = false;
		
		return (true);
		}
	
	pushhashtable (ht1);
	
	fl = langsetstringval (bs1, bs);
	
	pophashtable ();
	
	if (!fl)
		return (false);
	
	if (!insertinhandle (h, 0L, &type, sizeof (type))) {
		
		disposehandle (h);
		
		return (false);
		}
	
	if (!langsetbinaryval (ht2, bs2, h))
		return (false);
	
	(*v).data.flvalue = true;
	
	return (true);
	} /*getnthresourceverb*/


static boolean getnthresinfoverb (hdltreenode hparam1, tyvaluerecord *v) {
	
	/*
	2005-12-26 creedon: commented out param count check
	
	2005-09-02 creedon: added support for fork parameter, see resources.c: openresourcefile and pushresourcefile
	
	6/2/92 dmb: created.
	*/
	
	tyfilespec fs;
	OSType type;
	short n, id, forktype;
	short ctconsumed = 5;
	short ctpositional = 5;
	hdlhashtable ht1;
	bigstring bs, bs1;
	boolean fl;
	tyvaluerecord val;
	
	/* if (!langcheckparamcount (hparam1, 5))
		return (false); */
	
	if (!getpathvalue (hparam1, 1, &fs)) 
		return (false);
	
	if (!getostypevalue (hparam1, 2, &type))
		return (false);
	
	if (!getintvalue (hparam1, 3, &n))
		return (false);
	
	setintvalue (resourcefork, &val); /* defaults to 1 */

	if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x04""fork"), &val))
		return (false);
	
	forktype = val.data.intvalue;

	if (!getnthresourcehandle (&fs, type, n, &id, bs, nil, forktype)) {
		
		(*v).data.flvalue = false;
		
		return (true);
		}
	
	if (!langsetlongvarparam (hparam1, 4, id))
		return (false);
	
	if (!getvarparam (hparam1, 5, &ht1, bs1)) /*returned name holder*/
		return (false);
	
	pushhashtable (ht1);
	
	fl = langsetstringval (bs1, bs);
	
	pophashtable ();
	
	if (!fl)
		return (false);
	
	(*v).data.flvalue = true;
	
	return (true);
	} /*getnthresinfoverb*/


static boolean resourceexistsverb (hdltreenode hparam1, boolean flnamed, tyvaluerecord *v) {
	
	/*
	2005-09-02 creedon: added support for fork parameter, see resources.c: openresourcefile and pushresourcefile
	*/ 
	
	tyfilespec fs;
	OSType type;
	short id, forktype;
	short ctconsumed = 3;
	short ctpositional = 3;
	bigstring bs;
	tyvaluerecord val;
	
	if (!getpathvalue (hparam1, 1, &fs)) 
		return (false);
	
	if (!getostypevalue (hparam1, 2, &type))
		return (false);
	
	if (!getstringorintvalue (hparam1, 3, flnamed, &id, bs))
		return (false);
	
	flnextparamislast = true;
	
	setintvalue (resourcefork, &val); /* defaults to 1 */

	if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x04""fork"), &val))
		return (false);
	
	forktype = val.data.intvalue;

	(*v).data.flvalue = loadresourcehandle (&fs, type, id, bs, nil, forktype);
	
	return (true);
	} /*resourceexistsverb*/


static boolean getresourceattrsverb (hdltreenode hparam1, boolean flnamed, tyvaluerecord *v) {
	
	/*
	2005-09-02 creedon: added support for fork parameter, see resources.c: openresourcefile and pushresourcefile
	
	2.1b4 dmb: new verb
	*/
	
	tyfilespec fs;
	OSType type;
	short id, attrs, forktype;
	short ctconsumed = 3;
	short ctpositional = 3;
	bigstring bs;
	tyvaluerecord val;

	if (!getpathvalue (hparam1, 1, &fs)) 
		return (false);
	
	if (!getostypevalue (hparam1, 2, &type))
		return (false);
	
	if (!getstringorintvalue (hparam1, 3, flnamed, &id, bs))
		return (false);
	
	flnextparamislast = true;
	
	setintvalue (resourcefork, &val); /* defaults to 1 */

	if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x04""fork"), &val))
		return (false);
	
	forktype = val.data.intvalue;

	if (!getresourceattributes (&fs, type, id, bs, &attrs, forktype))
		return (false);
	
	return (setintvalue (attrs, v));
	} /*getresourceattrsverb*/


static boolean setresourceattrsverb (hdltreenode hparam1, boolean flnamed, tyvaluerecord *v) {
	
	/*
	2005-09-02 creedon: added support for fork parameter, see resources.c: openresourcefile and pushresourcefile
	
	2.1b4 dmb: new verb
	*/
	
	tyfilespec fs;
	OSType type;
	short id, attrs, forktype;
	short ctconsumed = 4;
	short ctpositional = 4;
	bigstring bs;
	tyvaluerecord val;
	
	if (!getpathvalue (hparam1, 1, &fs)) 
		return (false);
	
	if (!getostypevalue (hparam1, 2, &type))
		return (false);
	
	if (!getstringorintvalue (hparam1, 3, flnamed, &id, bs))
		return (false);
	
	if (!getintvalue (hparam1, 4, &attrs))
		return (false);
	
	flnextparamislast = true;
	
	setintvalue (resourcefork, &val); /* defaults to 1 */

	if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x04""fork"), &val))
		return (false);
	
	forktype = val.data.intvalue;

	if (!setresourceattributes (&fs, type, id, bs, attrs, forktype))
		return (false);
	
	(*v).data.flvalue = true;
	
	return (true);
	} /*setresourceattrsverb*/


static boolean deleteresourceverb (hdltreenode hparam1, boolean flnamed, tyvaluerecord *v) {
	
	/*
	2005-09-02 creedon: added support for fork parameter, see resources.c: openresourcefile and pushresourcefile
	*/ 
	
	tyfilespec fs;
	OSType type;
	short id, forktype;
	short ctconsumed = 3;
	short ctpositional = 3;
	bigstring bs;
	tyvaluerecord val;
	
	if (!getpathvalue (hparam1, 1, &fs)) 
		return (false);
	
	if (!getostypevalue (hparam1, 2, &type))
		return (false);
	
	if (!getstringorintvalue (hparam1, 3, flnamed, &id, bs))
		return (false);
	
	flnextparamislast = true;
	
	setintvalue (resourcefork, &val); /* defaults to 1 */

	if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x04""fork"), &val))
		return (false);
	
	forktype = val.data.intvalue;

	(*v).data.flvalue = deleteresource (&fs, type, id, bs, forktype);
	
	return (true);
	} /*deleteresourceverb*/


static boolean geticonposverb (hdltreenode hparam1, tyvaluerecord *v) {
	
	/*
	9/30/91 dmb: use setintvarparam, saves code
	*/
	
	tyfilespec fs;
	Point pos;
	/*
	hdlhashtable htable;
	bigstring bs;
	*/
	
	if (!langcheckparamcount (hparam1, 3))
		return (false);
	
	if (!getpathvalue (hparam1, 1, &fs)) 
		return (false);
	
	if (!getfilepos (&fs, &pos))
		return (false);
	
	if (!setintvarparam (hparam1, 2, pos.h))
		return (false);
	
	if (!setintvarparam (hparam1, 3, pos.v))
		return (false);
	
	/*
	if (!getvarparam (hparam1, 2, &htable, bs)) 
		return (false);
	
	pushhashtable (htable);
	
	hashassignint (bs, pos.h);
	
	pophashtable ();
	
	if (!getvarparam (hparam1, 3, &htable, bs)) 
		return (false);
	
	pushhashtable (htable);
	
	hashassignint (bs, pos.v);
	
	pophashtable ();
	*/
	
	(*v).data.flvalue = true;
	
	return (true);
	} /*geticonposverb*/


static boolean seticonposverb (hdltreenode hparam1, tyvaluerecord *v) {

	tyfilespec fs;
	Point pos;
	
	if (!getpathvalue (hparam1, 1, &fs)) 
		return (false);
	
	if (!getintvalue (hparam1, 2, &pos.h)) 
		return (false);
	
	flnextparamislast = true;
	
	if (!getintvalue (hparam1, 3, &pos.v)) 
		return (false);
	
	(*v).data.flvalue = setfilepos (&fs, pos);
	
	return (true);
	} /*seticonposverb*/


	typedef struct lNumVersion {
	
		//
		// 2007-09-22 creedon: the nonRelRev is not a BCD number, see < http://developer.apple.com/technotes/tn/tn1132.html >
		//
		// 1992-11-17 dmb: this definition of the version resource makes it easier
		//			    to pick apart
		
		#ifdef SWAP_REZ_BYTE_ORDER
		
			/* unsigned short nonRelRev2: 4; // 2nd nibble of revision level
			unsigned short nonRelRev1: 4; // revision level of non-released version */
			
			unsigned short nonRelRev:  8; // revision level of non-released version
			unsigned short stage:	  8; // stage code: dev, alpha, beta, final
			unsigned short bugFixRev:  4; // 3rd part is 1 nibble in BCD
			unsigned short minorRev:   4; // 2nd part is 1 nibble in BCD
			unsigned short majorRev2:  4; // 2nd nibble of 1st part
			unsigned short majorRev1:  4; // 1st part of version number in BCD
			
		#else
		
			unsigned short majorRev1:  4; // 1st part of version number in BCD
			unsigned short majorRev2:  4; // 2nd nibble of 1st part
			unsigned short minorRev:	  4; // 2nd part is 1 nibble in BCD
			unsigned short bugFixRev:  4; // 3rd part is 1 nibble in BCD
			unsigned short stage:	  8; // stage code: dev, alpha, beta, final
			unsigned short nonRelRev:  8; // revision level of non-released version
			
			/* unsigned short nonRelRev1:  4; // revision level of non-released version
			unsigned short nonRelRev2: 4; // 2nd nibble of revision level */
			
		#endif
		
		} lNumVersion;
		
	
	typedef struct lVersRec {
		lNumVersion numericVersion;		/*encoded version number*/
		short countryCode;				/*country code from intl utilities*/
		Str255 shortVersion;			/*version number string - worst case*/
		Str255 reserved;				/*longMessage string packed after shortVersion*/
		} lVersRec, *lVersRecPtr, **lVersRecHndl;
		
	
	static byte bsstages [] = "\pdab";	/*dev, alpha, beta*/
	
	
	#define emptyversionsize ((long) sizeof (lNumVersion) + sizeof (short) + 2)
	
	
	static boolean versionnumtostring (lNumVersion numvers, bigstring bs) {
	
		//
		// 2007-09-22 creedon: bug fix, when 10.1a16 was entered into
		//				   versions.h, function would return 10.1a10,
		//				   problem, non-released revision was being
		//				   treated as BCD instead of number as specified
		//				   in < http://developer.apple.com/technotes/tn/tn1132.html >,
		//				   had to change lNumVersion struct
		//
		// return the packed version number as a string, e.g. "1.0b2". need 
		// definitions above, which is mis-defined in the Think C headers
		//
		
		/*
		lNumVersion numvers;
		
		numvers = *(lNumVersion *) &versionnum;
		*/
		
		setemptystring (bs);
		
		if (numvers.majorRev1 != 0)
			shorttostring (numvers.majorRev1, bs);
			
		pushint (numvers.majorRev2, bs);
		
		pushchar ('.', bs);
		
		pushint (numvers.minorRev, bs);
		
		if (numvers.bugFixRev > 0) {
		
			pushchar ('.', bs);
			
			pushint (numvers.bugFixRev, bs);
			
			}
			
		if (numvers.stage < finalStage) {
			
			pushchar (bsstages [numvers.stage / developStage], bs);
			
			/* if (numvers.nonRelRev1 > 0)
				pushint (numvers.nonRelRev1, bs);
			
			pushint (numvers.nonRelRev2, bs); */
			
			pushint (numvers.nonRelRev, bs);
			
			}
			
		return (true);
		
		} // versionnumtostring
		
	
	typedef byte shortstring [16];
	
	static short stringtobcd (bigstring bs) {
	
		register short i;
		register short n = 0;
		
		for (i = 1; i <= stringlength (bs); ++i)
			n = (n << 4) + (bs [i] - '0');
			
		return (n);
		} /*stringtobcd*/


	static boolean stringtoversionnum (bigstring bs, NumVersion *versionnum) {
		
		/*
		convert the string to a packed version number.  see above.
		*/
		
		NumVersion numvers;
		register short i;
		register byte ch;
		shortstring bsnumber [4]; /*the three rev numbers, as strings*/
		short number [4]; /*the rev numbers*/
		short ixnumber = 0;
		
		numvers.stage = finalStage;
		
		for (i = 0; i < 4; ++i)
			setemptystring (bsnumber [i]);
		
		for (i = 1; i <= stringlength (bs); ++i) {
			
			ch = bs [i];
			
			if (isnumeric (ch)) { /*digit: add to current number string*/
				
				pushchar (ch, bsnumber [ixnumber]);
				
				continue;
				}
			
			if (ixnumber == 3) /*we're full: take what we've got so far*/
				goto exit;
			
			if (ch == '.') { /*decimal point: move on to next number*/
				
				++ixnumber;
				
				continue;
				}
			
			ixnumber = 3; /*only valid data would be stage character & number*/
			
			switch (lowercasechar (ch)) {
				
				case 'a':
					numvers.stage = alphaStage;
					
					break;
				
				case 'b':
					numvers.stage = betaStage;
					
					break;
				
				case 'd':
					numvers.stage = developStage;
					
					break;
				
				default:
					goto exit;
				}
			}
		
		exit:
		
		for (i = 0; i < 4; ++i)
			number [i] = stringtobcd (bsnumber [i]);
		
		numvers.majorRev = number [0];
		
		numvers.minorAndBugRev = (number [1] << 4) + number [2];
		
		numvers.nonRelRev = number [3];
		
		*versionnum = numvers;
		
		return (true);
		} /*stringtoversionnum*/


	boolean filegetprogramversion (bigstring bsversion) {
		
		/*
		12/19/91 dmb: this routine is here because this is the only file that currently 
		knows the format of a 'vers' resource.  it would more logically be in file.c
		*/
		
		lNumVersion versionnumber;
		
		if (filereadresource (filegetapplicationrnum (), 'vers', 1, nil, sizeof (versionnumber), &versionnumber))
			return (versionnumtostring (versionnumber, bsversion));
		
		setemptystring (bsversion);
		
		return (false);
		} /*filegetprogramversion*/


	static boolean getshortversionverb (hdltreenode hparam1, tyvaluerecord *v) {
		
		/*
		file.getversion (path): string; return the version number as a string, e.g. "1.0b2".
		
		2005-09-02 creedon: added support for fork parameter, see resources.c: openresourcefile and pushresourcefile
		*/
		
		tyfilespec fs;
		lNumVersion versionnumber;
		bigstring bs;
		short forktype;
		short ctconsumed = 1;
		short ctpositional = 1;
		tyvaluerecord val;
		
		if (!getpathvalue (hparam1, 1, &fs)) 
			return (false);
		
		flnextparamislast = true;
		
		setintvalue (resourcefork, &val); /* defaults to 1 */

		if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x04""fork"), &val))
			return (false);
		
		forktype = val.data.intvalue;

		if (loadresource (&fs, -1, 'vers', 1, nil, sizeof (versionnumber), &versionnumber, forktype))
			versionnumtostring (versionnumber, bs);
		else
			setemptystring (bs);
		
		return (setstringvalue (bs, v));
		} /*getshortversionverb*/


	static boolean mungeversionstring (VersRecHndl hvers, bigstring bsversion, boolean fllongvers) {
		
		register byte *p;
		long ixvers;
		long ctvers;
		
		p = (**hvers).shortVersion;
		
		if (fllongvers)
			p += stringlength (p) + 1; /*skip to next contiguous string -- the long version*/
		
		ixvers = p - (byte *) *hvers;
		
		ctvers = stringsize (p);
		
		return (mungehandle ((Handle) hvers, ixvers, ctvers, bsversion, stringsize (bsversion)));
		} /*mungeversionstring*/


	static boolean setshortversionverb (hdltreenode hparam1, tyvaluerecord *v) {
		
		/*
		file.setversion (path): boolean; set the short version string.  if a valid version number is specified, set the numeric fields as well
		
		2005-09-02 creedon: added support for fork parameter, see resources.c: openresourcefile and pushresourcefile
		*/
		
		tyfilespec fs;
		bigstring bsversion;
		NumVersion versionnumber;
		VersRecHndl hvers;
		short forktype;
		short ctconsumed = 2;
		short ctpositional = 2;
		tyvaluerecord val;
		
		if (!getpathvalue (hparam1, 1, &fs)) 
			return (false);
		
		if (!getstringvalue (hparam1, 2, bsversion)) 
			return (false);
		
		flnextparamislast = true;
		
		setintvalue (resourcefork, &val); /* defaults to 1 */

		if (!getoptionalparamvalue (hparam1, &ctconsumed, &ctpositional, BIGSTRING ("\x04""fork"), &val))
			return (false);
		
		forktype = val.data.intvalue;

		if (!loadresourcehandle (&fs, 'vers', 1, nil, (Handle *) &hvers, forktype))
			if (!newclearhandle (emptyversionsize, (Handle *) &hvers))
				return (false);
		
		if (stringtoversionnum (bsversion, &versionnumber)) /*convert to BCD if possible*/
			(**hvers).numericVersion = versionnumber;
		
		if (mungeversionstring (hvers, bsversion, false))
			if (saveresourcehandle (&fs, 'vers', 1, nil, (Handle) hvers, forktype))
				(*v).data.flvalue = true;
		
		disposehandle ((Handle) hvers);
		
		return (true);
		} /*setshortversionverb*/


	static boolean getlongversionverb (hdltreenode hparam1, tyvaluerecord *v) {

		/*
