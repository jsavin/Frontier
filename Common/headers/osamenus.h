
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

#define osamenusinclude

#ifndef __APPLEEVENTS__

	#include <AppleEvents.h>

#endif

#ifndef windowsharinginclude

	// #include <uisharing.h>
	// #include <uisinternal.h>
	
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
	#define msSetEventFilterCallbackCommand 0x200C
	#define msSetMenusInserterCallbackCommand 0x200D
	#define msSetMenusRemoverCallbackCommand 0x200E
	#define msDirtySharedMenusCommand 0x200F
	

#endif


#define __MENUSHARING__ /*so other modules can tell that we've been included*/


/*routines shared between osamenus.c and osacomponent.c*/

extern OSAError osageterror (void); /*osacomponent.c*/

extern pascal OSErr osadefaultactiveproc (long);

extern pascal OSAError osaDispose (hdlcomponentglobals, OSAID);

extern pascal OSAError osaSetActiveProc (hdlcomponentglobals, OSAActiveUPP, long);

extern pascal OSAError osaSetSendProc (hdlcomponentglobals, OSASendUPP, long);

extern pascal OSAError osaCompileExecute (hdlcomponentglobals, const AEDesc *, OSAID, long, OSAID *);

extern pascal OSAError osaDoScript (hdlcomponentglobals, const AEDesc *, OSAID, DescType, long, AEDesc *);

extern boolean osafindclienteventfilter (long, long *);


extern boolean initmenusharingcomponent (void); /*osamenus.c*/




