
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

#define langtokensinclude



/*
DW 3/20/90:

this file isolates the declarations of the language's verbs.  an earlier version 
had these declarations in lang.h.  but about 30 files include lang.h, so adding
a new verb required all of these to be recompiled.  too bad only two files really
care about the token value -- lang.c and langvalue.c.

so we split the declaration of tyfunctype off into this file.  makes it possible
to add verbs when there's nothing to do in the kitchen or the yard!
*/




typedef enum tyfunctype {
	
	appleeventfunc,
	
	complexeventfunc,
	
	findereventfunc,
	
	tableeventfunc,
	
	objspecfunc,
	
	setobjspecfunc,
	
	packfunc,
	
	unpackfunc,
	
	definedfunc,
	
	typeoffunc,
	
	sizeoffunc,
	
	nameoffunc,
	
	parentoffunc,

	indexoffunc,
	
	gestaltfunc,
	
	syscrashfunc,
	
	myMooffunc,
	
	equalsfunc = 400, /*must agree with numbers in langparser.y*/
	
	notequalsfunc = 401,
	
	greaterthanfunc = 402,
	
	lessthanfunc = 403,
	
	greaterthanorequalfunc = 404,
	
	lessthanorequalfunc = 405,
	
	notfunc = 406,
	
	andfunc = 407,
	
	orfunc = 408,
	
	beginswithfunc = 409,
	
	endswithfunc = 410,
	
	containsfunc = 411,
	
	loopfunc = 500,
	
	fileloopfunc = 501,
	
	infunc = 502,
	 
	breakfunc = 503,
	
	returnfunc = 504,
	
	iffunc = 505,
	
	thenfunc = 506,
	
	elsefunc = 507,
	
	bundlefunc = 508,
	
	localfunc = 509,
	
	onfunc = 510,
	
	whilefunc = 511, 
	
	casefunc = 512,
	
	kernelfunc = 513,
	
	forfunc = 514,
	
	tofunc = 515,
	
	downtofunc = 516,
	
	continuefunc = 517,
	
	withfunc = 518,
	
	tryfunc = 519,
	
	globalfunc = 520
	
	} tyfunctype;


