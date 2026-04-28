
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

#ifndef threadsinclude
#define threadsinclude


#define	idnullthread		((hdlthread) 0)
#define	idcurrentthread		((hdlthread) 1)
#define	idapplicationthread	((hdlthread) 2)


typedef struct _thread * hdlthread;

typedef void * tythreadmainparams;

typedef pascal void * (*tythreadmaincallback) (tythreadmainparams);

typedef void (*tythreadglobalscallback) (void *);

#pragma pack(2)
typedef struct tythreadcallbacks {

	tythreadglobalscallback disposecallback;
	
	tythreadglobalscallback swapincallback;
	
	tythreadglobalscallback swapoutcallback;
	} tythreadcallbacks;
#pragma options align=reset

/*globals*/

extern tythreadcallbacks threadcallbacks;


/*prototypes*/

extern boolean canusethreads (void);

extern boolean initmainthread (void *);

extern boolean inmainthread (void);

extern boolean attachtomainthread (long); /*6.2b7 AR*/

extern boolean newthread (tythreadmaincallback, tythreadmainparams, void *, hdlthread *);

extern boolean threadstartup (void);

extern void threadshutdown (void);

extern boolean threadsleep (hdlthread hthread);

extern boolean threadissleeping (hdlthread);

extern boolean threadwake (hdlthread, boolean);

extern boolean threadiswaiting (void);

extern boolean threadyield (boolean);

extern long grabthreadglobals (void);

extern long releasethreadglobals (void);

extern long grabthreadglobalsnopriority (void);

extern long releasethreadglobalsnopriority (void);

extern boolean initthreads (void);

#ifdef fldebug
	extern void checkthreadglobals (void);
#else
	#define checkthreadglobals() ((void *)0)
#endif


#endif
