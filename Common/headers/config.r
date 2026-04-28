
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

type'cnfg' {
	
	integer nohorizscroll = 0, horizscroll = 1; /*window has horiz scrollbar?*/
	
	integer novertscroll = 0, vertscroll = 1;
	
	integer dontfloat = 0, windowfloats = 1; /*is it a floating palette window?*/
	
	integer nomessagearea = 0, messagearea = 1; /*allocate space for a message area?*/
	
	integer dontinsetcontentrect = 0, insetcontentrect = 1; /*if true we inset by 3 pixels*/
	
	integer nonewonlaunch = 0, newonlaunch = 1;
	
	integer dontopenresfile = 0, openresfile = 1;
	
	integer normalwindow = 0, dialogwindow = 1; /*do a GetNewDialog on creating one of these windows?*/
	
	integer notgrowable = 0, isgrowable = 1; /*provide a grow box for window*/
	
	integer dontcreateonnew = 0, createonnew = 1;
	
	integer nowindoidscrollbars = 0, windoidscrollbars = 1;
	
	integer notstoredindatabase = 0, storedindatabase = 1;
	
	integer handlesownsave = 0, parentwindowhandlessave = 1;
	
	integer donteraseonresize = 0, eraseonresize = 1;
	
	integer consumefrontclicks = 0, dontconsumefrontclicks = 1;
	
	integer monochromewindow = 0, colorwindow = 1;
	
	integer onehalf = 2, onethird = 3, onequarter = 4;
	
	literal longint; /*filecreator*/
	
	literal longint; /*filetype*/
	
	integer; 
	
	Rect; /*for growable windows, the minimum size allowed*/
	
	integer; /*version on disk indexes into a STR# list*/
	
	integer size9 = 9, size12 = 12;
	
	integer plain = 0;
	
	integer; /*if 0, no buttons for this window type*/
	
	Rect; /*new windows come up in this spot*/
	};
