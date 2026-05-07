
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

typedef struct odb_ * odbref;

/* ODB list record — tracks open guest databases (in-memory only, not a disk format).
 * MUST use pack(2) to match legacy Frontier struct layout. Without pack(2), natural
 * alignment shifts the odb field by 6 bytes, causing corrupted handle dereferences
 * when db.setvalue/getvalue follow fileMenu.open in the same script.
 * Shared between dbverbs.c and headless_filemenu_verbs.c. */
#pragma pack(2)
typedef struct tyodblistrecord {
	struct tyodblistrecord **hnext;
	tyfilespec fs;
	hdlfilenum fref;
	boolean flreadonly;
	odbref odb;
} tyodbrecord, *ptrodbrecord, **hdlodbrecord;
#pragma options align=reset

_Static_assert(sizeof(tyodbrecord) == 614,
	"tyodbrecord size changed — pack(2) layout must match dbverbs.c expectations");

extern hdlodbrecord hodblist;

typedef enum odbValueType  {
	
	unknownT = '\?\?\?\?',
	
	charT = 'char',
	
	shortT = 'shor',
	
	longT = 'long',
	
	binaryT = 'data',
	
	booleanT = 'bool',
	
	tokenT = 'tokn',
	
	dateT = 'date',
	
	addressT = 'addr',
	
	codeT = 'code',
	
	extendedT = 'exte',
	
	stringT = 'TEXT',
	
	externalT = 'xtrn',
	
	directionT = 'dir ',
	
	string4T = 'type',
	
	pointT = 'QDpt',
	
	rectT = 'qdrt',
	
	patternT = 'tptn',
	
	rgbT = 'cRGB',
	
	fixedT = 'fixd',
	
	singleT = 'sing',
	
	doubleT = 'doub',
	
	objspecT = 'obj ',
	
	filespecT = 'fss ',
	
	aliasT = 'alis',
	
	enumeratorT = 'enum',
	
	listT = 'list',
	
	recordT = 'reco',
	
	outlineT = 'optx',
	
	wptextT = 'wptx',
	
	tableT = 'tabl',
	
	scriptT = 'scpt',
	
	menubarT = 'mbar',
	
	pictureT = 'pict'
	
	} odbValueType;


typedef union odbValueData {
	
	boolean flvalue;
	
	unsigned char chvalue;
	
	short intvalue;
	
	long longvalue;
	
	unsigned long datevalue;
	
	tydirection dirvalue;
	
	OSType ostypevalue;
	
	Handle stringvalue;
	
	Handle addressvalue;
	
	Handle binaryvalue;
	
	Handle externalvalue;
	
	Point pointvalue;
	
	Rect **rectvalue;
	
	Pattern **patternvalue;
	
	RGBColor **rgbvalue;
	
	Fixed fixedvalue;
	
	float singlevalue;
	
	double **doublevalue;
	
	Handle objspecvalue;
	
	FSSpec **filespecvalue;
	
	Handle aliasvalue; /*AliasHandle*/
	
	OSType enumvalue;
	
	Handle listvalue;
	
	Handle recordvalue;
	} odbValueData;


/*note: must have 68k struct alignment compiler option set*/

typedef struct odbValueRecord {
	
	odbValueType valuetype;
	
	odbValueData data;
	} odbValueRecord;

extern pascal boolean odbUpdateOdbref (WindowPtr w, odbref odb);

extern pascal boolean odbAccessWindow (WindowPtr w, odbref *odb);

extern pascal boolean odbNewFile (hdlfilenum);

extern pascal boolean odbOpenFile (hdlfilenum, odbref *odb, boolean flreadonly);

extern pascal boolean odbSaveFile (odbref odb);

extern pascal Handle odbGetRootVariable (odbref odb);

extern pascal boolean odbCloseFile (odbref odb);

/* db.compactDatabase: write a v7→v7 compacted copy. See odbengine.c for full
 * semantics. After this returns the source's in-memory state is indeterminate;
 * caller must close + reopen the source to keep using it. */
extern pascal boolean odbCompactDatabase (odbref odb, const char *dst_path);

extern pascal boolean odbDefined (odbref odb, bigstring bspath);

extern pascal boolean odbDelete (odbref odb, bigstring bspath);

extern pascal boolean odbGetType (odbref odb, bigstring bspath, OSType *type);

extern pascal boolean odbGetValue (odbref odb, bigstring bspath, odbValueRecord *value);

extern pascal boolean odbSetValue (odbref odb, bigstring bspath, odbValueRecord *value);

extern pascal boolean odbNewTable (odbref odb, bigstring bspath);

extern pascal boolean odbCountItems (odbref odb, bigstring bspath, long *count);

extern pascal boolean odbGetNthItem (odbref odb, bigstring bspath, long n, bigstring bsname);

extern pascal boolean odbGetModDate (odbref odb, bigstring bspath, int64_t *date);

extern pascal void odbInitValue (odbValueRecord *value);

extern pascal void odbDisposeValue (odbref odb, odbValueRecord *value);

extern pascal void odbGetError (bigstring bs);

extern pascal boolean odbInstallEventHandler (void);

