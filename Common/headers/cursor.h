
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

#define cursorinclude /*so other includes can tell if we've been loaded*/



typedef enum tycursortype {

	cursorisdirty = -1,
	
	cursorisarrow = -2,
	
	cursorisibeam = iBeamCursor,
	
	cursoriswatch = watchCursor,
	
	cursorisverticalrails = 130,
	
	cursorishorizontalrails = 131,
	
	cursorisslantedrails = 132,
	
	xxxcursorisotherslantedrails = 133,
	
	xxxcursorishorizontalrail = 256,
	
	cursorisbeachball1 = 257, 
	
	cursorisbeachball2 = 258, 
	
	cursorisbeachball3 = 259, 
	
	cursorisbeachball4 = 260,
	
	cursorispopup = 261,
	
	xxxcursorisearth1 = 262, 
	
	xxxcursorisearth2 = 263, 
	
	xxxcursorisearth3 = 264, 
	
	xxxcursorisearth4 = 265, 
	
	xxxcursorisearth5 = 266,
	
	xxxcursorisearth6 = 267, 
	
	xxxcursorisearth7 = 268,
	
	cursorishollowarrow = 269,
	
	cursorfordraggingmove = 270,
	
	xxxcursorissmallquestionmark = 271,
	
	xxxcursorisno = 272,
	
	xxxcursorisbuttonhand = 273,
	
	cursorisgo = 274,
	
	xxxcursorisrightwedge = 275
	} tycursortype;

	
/*prototypes*/

extern void setcursortype (tycursortype);

extern void obscurecursor (void);

extern void initbeachball (tydirection);

extern void rollbeachball (void);

extern boolean beachballcursor (void);

extern void initearth (void);

extern void rollearth (void);




