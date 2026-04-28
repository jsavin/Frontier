
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

#define launchinclude

#ifndef shelltypesinclude

	#include "shelltypes.h"

#endif

/*typedefs*/

#pragma pack(2)
typedef struct tylaunchcallbacks {
	
	callback waitcallback; /*called while waiting for process manager change to take effect*/
	} tylaunchcallbacks;
#pragma options align=reset


/*globals*/

extern tylaunchcallbacks launchcallbacks;


/*prototypes*/

extern boolean launchapplication (const ptrfilespec, const ptrfilespec, boolean flbringtofront);

extern boolean findrunningapplication (OSType *, bigstring, typrocessid *);

extern boolean activateapplication (bigstring);

extern boolean getfrontapplication (bigstring, boolean);

extern short countapplications (void);

extern boolean getnthapplication (short, bigstring);

extern boolean getapplicationfilespec ( bigstring, ptrfilespec );

extern boolean executeresource (ResType, short, bigstring);


extern OSType getprocesscreator (void);

extern typrocessid getcurrentprocessid (void);

extern boolean iscurrentapplication (typrocessid);

extern boolean isfrontapplication (typrocessid);

extern boolean activateapplicationwindow (typrocessid, WindowPtr);




