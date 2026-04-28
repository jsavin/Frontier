
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

#define scrapinclude /*so other includes can tell if we've been loaded*/

#ifndef shelltypesinclude

	#include "shelltypes.h"

#endif



#define noscraptype 0

#define anyscraptype '****'

#define allscraptypes '++++'

#define textscraptype 'TEXT'

#define pictscraptype 'PICT'

#define hashscraptype 'HASH'

#define opscraptype 'OP  '

#define wpscraptype 'WPTX'

#define menuscraptype 'MNBR'

#define scriptscraptype 'SCPT'


/*function prototypes*/

extern boolean resetscrap (void);

extern short getscrapcount (void);

extern short handlescrapdisposed (void);


extern tyscraptype getscraptype (void);

extern boolean getscrap (tyscraptype, Handle);

extern boolean putscrap (tyscraptype, Handle);

extern boolean openclipboard (void);

extern boolean closeclipboard (void);

extern void initclipboard (void);

