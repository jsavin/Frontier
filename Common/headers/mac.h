
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

#define macinclude


/*constants*/

#define idgestaltselector 'LAND'
#define idgestaltfunction 128


/*typedefs*/

#pragma pack(2)
typedef struct tymemoryconfig {
	
	long minstacksize; /*minimum stack space required*/
	
	long minheapsize; /*minimum heap space to run program*/
	
	long avghandlesize; /*to determine # of master blocks; use zero for none*/
	
	long reserveforcode; /*approx. size of not-yet-loaded CODE resources*/
	
	long reserved;
	} tymemoryconfig, **hdlmemoryconfig;
#pragma options align=reset


/*globals*/

extern boolean	gHasColorQD;
extern boolean	gCanUseNavServ;

//extern SysEnvRec macworld; /*the machine environment*/

extern tymemoryconfig macmemoryconfig;


/*prototypes*/

extern boolean initmacintosh (void);

extern short countinitialfiles (void);

extern void getinitialfile (short, bigstring, short *);

extern boolean installgestaltfunction (void);

	extern void WriteToConsole (char *s);
	extern void DoErrorAlert(OSStatus status, CFStringRef errorFormatString);

