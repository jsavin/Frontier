
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

#define shellundoinclude /*so other includes can tell if we've been loaded*/



#pragma pack(2)
typedef struct tystack {

	short topstack;

	short basesize;

	short elemsize;
	
			
		byte stack [1];
	
	} tystack, *ptrstack, **hdlstack;


typedef boolean (*undocallback) (Handle, boolean);


typedef struct tyundorecord {
	
	undocallback undoroutine;
	
	Handle hundodata;
	
	boolean flactionstep; /*is this step an action record?*/
	} tyundorecord;


typedef struct tyactionrecord {
	
	long ixaction;
	
	long globaldata;
	
	boolean flaction; /*always true for actionrecords*/
	} tyactionrecord;


typedef struct tyundostack {

	short topundo;
	
	short basesize;
	
	short elemsize;
	
	/*
	union {
		tyundorecord undostep;
		
		tyactionrecord actionstep;
		} u;
	*/
		
	short ixaction;
	
	long globaldata;
	
	
		tyundorecord undostep [1];
		
	} tyundostack, *ptrundostack, **hdlundostack;
#pragma options align=reset


/*function prototypes*/

extern boolean pushundostep (undocallback, Handle);

extern boolean pushundoaction (short);

extern boolean popundoaction (void);

extern boolean undolastaction (boolean);

extern boolean redolastaction (boolean);

extern boolean getundoaction (short *);

extern boolean getredoaction (short *);

extern void killundo (void);

extern boolean newundostack (hdlundostack *);

extern boolean disposeundostack (hdlundostack);

extern void initundo (void);


/*global variables*/

extern hdlundostack shellundostack;

extern hdlundostack shellredostack;


