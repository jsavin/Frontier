
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

/*
	here's the scoop: when Frontier is servicing a component call, 
	running a script, it's A5 is set up.  when running a dialog, the 
	system gets confused if A5 isn't the same as that of the current 
	application.  (one way to make it flake out is to click in the 
	menu bar to pull down the dimmed menus.)  fortunately, we always 
	have the "correct" value at hand -- the low-mem global CurrentA5.
	
	these macros expand on those in Think C's SetUpA4.h by adding two 
	flavors of setting up A5, one to set it to CurrentA4, the other to 
	set it to the A5 that has been stashed away by calling RememberA5. 
	the latter value is Frontier's A5, which must be remembered before 
	setting A5 to CurrentA5.
	
	get it?  it works!
	
	2006-04-01 aradke: removed assembler code for Mac/68k and prepare for Mac/x86
*/

	
//Code change by Timothy Paustian Wednesday, July 12, 2000 1:59:26 PM
//A5 worlds have no relvance in Carbon so just define them away.

	#define RememberA5()
	
	#define SetUpThisA5(A5) nil
	
	#define SetUpAppA5() nil
		
	#define SetUpCurA5() nil
		
	#define RestoreA5(savedA5)
	
	#define pushA5()
	
	#define popA5()


