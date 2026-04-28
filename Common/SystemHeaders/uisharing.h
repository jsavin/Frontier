
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

#define uisharinginclude


#define noMenuSharing 0x1 /*masks for uisInit*/
#define noWindowSharing 0x2


/*core routines for UI Sharing clients*/

	Boolean uisInit (ProcPtr, short, OSType, unsigned short);
	
	Boolean uisHandleEvent (EventRecord *, Boolean *);
	
	Boolean uisEdit (short);
	
	Boolean uisSharedMenuHit (short, short);
	
	Boolean uisIsSharedWindow (WindowPtr);
	
	Boolean uisCloseSharedWindow (WindowPtr);
	
	void uisCloseAllSharedWindows (void);
	
	void uisClose (void);
	

/*for apps that want to use Iowa Runtime as their dialog manager*/

	#define uisInitEvent 1000 /*constants for (*ev).what in callbacks*/
	#define uisMajorRecalcEvent 1001
	#define uisMinorRecalcEvent 1002
	#define uisButtonHitEvent 1003
	#define uisCloseEvent 1004
	#define uisRunScriptEvent 1005
	#define uisCancelEvent 1006
	#define uisSetHandleEvent 1007

	typedef void (*uisEventCallback) (EventRecord *);

	Boolean uisRunModalHandle (Handle, Boolean, Str255, short, short, uisEventCallback);

	Boolean uisRunModalResource (short, Boolean, Str255, short, short, uisEventCallback);

	Boolean uisOpenCardResource (short, Boolean, Str255, short, short, uisEventCallback);

	Boolean uisOpenHandle (Handle, Boolean, Str255, short, short, uisEventCallback);

	Boolean uisSetObjectValue (Handle, Str255, Handle);

	Boolean uisGetObjectValue (Handle, Str255, Handle *);

	Boolean uisGetObjectHandle (Handle, Str255, Handle *);

	Boolean uisRecalcObject (Handle);
	
	Boolean uisUpdate (Handle);
	
/*for a stub app whose sole purpose is to run a card*/

	Boolean uisStubStart (void);





