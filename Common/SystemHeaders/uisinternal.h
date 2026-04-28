
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

#define uisinternalinclude


#ifndef __COMPONENTS__
	
	#include <Components.h>

#endif


#ifndef uisharinginclude

	#include <uisharing.h>
	
#endif


/*window sharing stuff -- for communication with Iowa Runtime component and others*/

	#define wsInterfaceVersion 0x00010001

	#define wsComponentType 'SHUI'
	
		#define wsComponentSubType 'WIND' 
	
	#define wsStubStartCommand 1 
	#define wsRecalcObjectCommand 2
	#define wsUnusedCommand 3
	#define wsRunFromHandleCommand 4
	#define wsRunModalHandleCommand 5 
	#define wsEventHandlerCommand 6
	#define wsWindowIsCardCommand 7
	#define wsCloseWindowCommand 8
	#define wsSetObjectValueCommand 9
	#define wsGetObjectValueCommand 10
	#define wsGetObjectHandleCommand 11
	#define wsUpdateCommand 12
	#define wsEditCommand 13

#pragma pack(2)
	typedef struct tyWindowSharingGlobals {
		
		ComponentInstance windowserver;
	
		OSErr errorcode; /*a copy of the system error code after a ws call*/
		
		Boolean flcloseallwindows; /*true when server gets an option-click in a window's close box*/
		} tyWindowSharingGlobals;
#pragma options align=reset
	
	extern tyWindowSharingGlobals wsGlobals;


/*menu sharing stuff -- for communication with the Frontier 2.1 OSA component*/

	#define msComponentType 'SHMN'
	#define msComponentSubType 0
	
	#define msInitSharedMenusCommand 0x2001 
	#define msSharedMenuHitCommand 0x2002
	#define msSharedScriptRunningCommand 0x2003
	#define msCancelSharedScriptCommand	0x2004
	#define msCheckSharedMenusCommand 0x2005
	#define msDisposeSharedMenusCommand 0x2007
	#define msIsSharedMenuCommand 0x2008
	#define msEnableSharedMenusCommand 0x2009
	#define msRunSharedMenuItemCommand 0x200A
	#define msSetScriptErrorCallbackCommand 0x200B
	#define msSetIdleProcCallbackCommand 0x200C

#pragma pack(2)
	typedef struct tyMenuSharingGlobals { 
	
		ComponentInstance menuserver; 
		
		short idinsertafter;
		} tyMenuSharingGlobals;
#pragma options align=reset

	extern tyMenuSharingGlobals msGlobals;
	
