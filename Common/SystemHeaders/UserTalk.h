
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

/*
Open Scripting Architecture: UserTalk Specific Interface
*/


#ifndef __usertalkinclude__
#define __usertalkinclude__

#ifndef __OSA__
	#include <OSA.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/*types and constants*/

#define typeUserTalk					'LAND'

#define kUserTalkSubtype				typeUserTalk

#define typeUserTalkSourceType			typeChar

#define typeUserTalkStorage				typeUserTalk


/*mode flags*/

#define kOSAModeDebug					0x00010000
#define kOSAModeTransparentScope		0x00020000


/*component selectors*/

#define	kOSASelectSetDebugProc			0x1001
#define	kOSASelectDebug					0x1002


/*setting the DebugProc*/

typedef pascal OSErr (*OSADebugProcPtr) (long refCon);

enum {
	uppOSADebugProcInfo = kPascalStackBased
		 | RESULT_SIZE(SIZE_CODE(sizeof(OSErr)))
		 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(long)))
};

#if USESROUTINEDESCRIPTORS
	typedef UniversalProcPtr OSADebugUPP;
	
	#define CallOSADebugProc(userRoutine, refCon)		\
			CallUniversalProc((UniversalProcPtr)(userRoutine), uppOSADebugProcInfo, (refCon))
	#define NewOSADebugProc(userRoutine)		\
			(OSADebugUPP) NewRoutineDescriptor((ProcPtr)(userRoutine), uppOSADebugProcInfo, GetCurrentISA())
#else
	typedef OSADebugProcPtr OSADebugUPP;
	
	#define CallOSADebugProc(userRoutine, refCon)		\
			(*(userRoutine))((refCon))
	#define NewOSADebugProc(userRoutine)		\
			(OSADebugUPP)(userRoutine)
#endif

pascal OSAError OSASetDebugProc (ComponentInstance		scriptingComponent,
								 OSADebugProcPtr		debugProc,
								 long					refCon)
 FIVEWORDINLINE(0x2F3C, 8, kOSASelectSetDebugProc, 0x7000, 0xA82A);
//	OSAComponentFunctionInline (kOSASelectSetDebugProc, 8);
	
	/*
	this routine is called during script execution if the the kOSAMode
	if debugProc is nil, no debugging will take place subsequently.
	
	errors:
		badComponentInstance	invalid scripting component instance
	 	errOSASystemError
	*/


/*getting debugging information*/

pascal OSAError OSADebug (ComponentInstance	scriptingComponent,
						  OSType			selector,
						  const AEDesc*		selectorData,
						  DescType			desiredType,
						  AEDesc*			resultingDebugInfoOrDataToSet)
 FIVEWORDINLINE(0x2F3C, 16, kOSASelectDebug, 0x7000, 0xA82A);
//	OSAComponentFunctionInline (kOSASelectDebug, 16);
	
	/*
	whenever script execution calls back to the DebugProc, this routine may
	be used to get information about the current debugging state.  the selector 
	describes the type of information desired about the state (various selectors 
	are listed below).  the desiredType indicates the data type of the result
	desired for that selector.
	
	errors:
		badComponentInstance	invalid scripting component instance
	 	errOSASystemError
		errOSABadSelector:		selector not supported by scripting component
	 	errAECoercionFail:		desiredType not supported by scripting component
	*/


/*OSADebug selectors*/

#define kOSADebugGetData					'getd'
	/*
	this selector is used to determine the value of a variable or other 
	property of the debugging context.
	
	required desiredTypes:
		typeChar
		typeObjectSpecifier
	*/

#define kOSADebugSetData					'setd'
	/*
	this selector is used to set the value of a variable or other 
	property of the debugging context. with this selector only, 
	resultingDebugInfoOrDataToSet contains input data, and is not 
	modified by the call.
	
	required desiredTypes:
		typeChar
		typeObjectSpecifier
	*/

#define kOSADebugRange						'drng'
	/*
	this selector is used to determine the source text range (start and 
	end positions) of the current point of execution.  with this selector, 
	selectorData is ignored and may be nil.
	
	required desiredType:
		typeOSADebugRange
	*/

#define typeOSADebugRange					typeOSAErrorRange
	/*
	an AERecord type containing keyOSASourceStart and keyOSASourceEnd fields
	of type long.
	*/

#define keyOSADebugRangeStart				keyOSASourceStart
#define keyOSADebugRangeEnd					keyOSASourceEnd
	/*
	fields of a typeOSADebugRange record of typeLongInteger.
	*/


#ifdef __cplusplus
}
#endif

#endif
