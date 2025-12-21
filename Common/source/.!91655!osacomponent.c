
/*	$Id$    */

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

#include "frontier.h"
#include "standard.h"

/*This file has been forked to protect the shipping version. The Carbon version is at the top, the "good"
version at the bottom. This file should be reconciled later.*/
/*
	2004-10-27 aradke: Reconciled the Carbon and Classic versions.
*/

//#pragma options (pack_enums) /* 2002-10-13 AR: pragma not supported by CodeWarrior */

#include <land.h>
#include <UserTalk.h>
#include "dialogs.h"
#include "error.h"
#include "file.h"
#include "font.h"
#include "kb.h"
#include "launch.h"
#include "memory.h"
#include "ops.h"
#include "resources.h"
#include "strings.h"
#include "timedate.h"
#include "lang.h"
#include "langexternal.h"
#include "langinternal.h"
#include "langipc.h"
#include "langsystem7.h"
#include "shellhooks.h"
#include "scripts.h"
#include "process.h"
#include "processinternal.h"
#include "tablestructure.h"
#include "osacomponent.h"
#include "osadroplet.h"
#include "osainternal.h"
#include "osamenus.h"
#include "osaparseaete.h"
#include "osawindows.h"
#include <SetUpA5.h>
#include "byteorder.h"
#include "aeutils.h"


/* forward declarations for static functions */

static boolean osabackgroundtask (boolean);

static boolean osadebugger (hdltreenode);

static boolean osapartialeventloop (UInt16);

static pascal OSErr osaclientactive (long refcon);

static pascal OSErr osaclientsend (const AppleEvent *, AppleEvent *, AESendMode, AESendPriority, long, AEIdleUPP, AEFilterUPP, long);

static pascal Boolean osaclientidleproc (EventRecord *ev, long *sleep, RgnHandle *mousergn);

static pascal ComponentResult cmpclose (Handle storage, ComponentInstance self);

static pascal OSAError osaLoad (hdlcomponentglobals	hglobals,
			const AEDesc*		scriptData,
			long				modeFlags,
			OSAID*				resultingCompiledScriptID);

static pascal ComponentResult cmpcando (short selector);

static pascal ComponentResult cmpversion (void);

static pascal OSAError osaStore (
			hdlcomponentglobals	hglobals,
			OSAID				compiledScriptID, 
			DescType			desiredType,
			long				modeFlags,
			AEDesc*				resultingScriptData);

static pascal OSAError osaExecute (
			hdlcomponentglobals	hglobals,
			OSAID				compiledScriptID,
			OSAID				contextID,
			long				modeFlags,
			OSAID*				resultingScriptValueID);

static pascal OSAError osaSetScriptInfo (
			hdlcomponentglobals	hglobals,
			OSAID				scriptID,
			OSType				selector,
			long				value);
			
static pascal OSAError osaGetScriptInfo (
			hdlcomponentglobals	hglobals,
			OSAID				scriptID,
			OSType				selector,
			long*				result);
			
static pascal OSAError osaCompile (
			hdlcomponentglobals	hglobals,
			const AEDesc*		sourceData,
			long				modeFlags,
			OSAID*				scriptID);
			
static pascal OSAError osaGetSource (
			hdlcomponentglobals	hglobals,
			OSAID				scriptID,
			DescType			desiredType,
			AEDesc*				resultingSourceData);
			
static pascal OSAError osaCoerceFromDesc (
			hdlcomponentglobals	hglobals,
			const AEDesc*		scriptData,
			long				modeFlags,
			OSAID*				resultingScriptID);


static pascal OSAError osaCoerceToDesc (
			hdlcomponentglobals	hglobals,
			OSAID				scriptID,
			DescType			desiredType,
			long				modeFlags,
			AEDesc*				result);
			
static pascal OSAError osaStartRecording (
			hdlcomponentglobals	hglobals,
			OSAID				*compiledScriptToModifyID);

static pascal OSAError osaStopRecording (
			hdlcomponentglobals	hglobals,
			OSAID				compiledScriptID);

static pascal OSAError osaScriptingComponentName (
			hdlcomponentglobals	hglobals,
			AEDesc*				resultingScriptingComponentName);

static pascal OSAError osaLoadExecute (
			hdlcomponentglobals	hglobals,
			const AEDesc*		scriptData,
			OSAID				contextID,
			long				modeFlags,
			OSAID*				resultingScriptValueID);
static pascal OSAError osaMakeContext (
			hdlcomponentglobals	hglobals,
			const AEDesc*		contextName,
			OSAID				parentContext,
			OSAID*				resultingContextID);
			
static pascal OSAError osaDisplay (
			hdlcomponentglobals	hglobals,
			OSAID				scriptValueID,
			DescType			desiredType,
			long				modeFlags,
			AEDesc*				resultingText);
	

static pascal OSAError osaSetResumeDispatchProc (
				hdlcomponentglobals	hglobals,
				AEEventHandlerUPP	resumeDispatchProc,
				long				refCon);
				
static pascal OSAError osaGetResumeDispatchProc (
				hdlcomponentglobals	hglobals,
				AEEventHandlerUPP*	resumeDispatchProc,
				long*				refCon);
				
static pascal OSAError osaExecuteEvent (
				hdlcomponentglobals	hglobals,
				AppleEvent*			event,
				OSAID				contextID,
				long				modeFlags,
				OSAID*				resultingScriptValueID);

static pascal OSAError osaDoEvent (
			hdlcomponentglobals	hglobals,
			AppleEvent*			event,
			OSAID				contextID,
			long				modeFlags,
			AppleEvent*			reply);
			
static pascal OSAError osaSetDebugProc (
			hdlcomponentglobals	hglobals,
			OSADebugUPP			debugProc,
			long				refCon);
			
static pascal OSAError osaDebug (
			hdlcomponentglobals	hglobals,
			OSType				selector,
			const AEDesc*		selectorData,
			DescType			desiredType,
			AEDesc*				resultingDebugInfoOrDataToSet);

static pascal OSAError osaGetSendProc (
			hdlcomponentglobals	hglobals,
			OSASendUPP*			sendProc,
			long*				refCon);

static pascal OSAError osaSetCreateProc (
			hdlcomponentglobals			hglobals,
			OSACreateAppleEventUPP		createProc,
			long						refCon);
			
static pascal OSAError osaGetCreateProc (
			hdlcomponentglobals			hglobals,
			OSACreateAppleEventUPP*		createProc,
			long*						refCon);
			


	/* proc infos for building routine descriptors and universal procedure pointers */
	
	enum {
			cmpcloseProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(Handle)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(ComponentInstance)))
		};
		
		enum {
			cmpcandoProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(short)))
		};
		
		enum {
			cmpversionProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
		};
		
		enum {
			osaLoadProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(AEDesc *)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(long)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(OSAID *)))
		};
		
		enum {
			osaStoreProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(DescType)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(long)))
				 | STACK_ROUTINE_PARAMETER(5, SIZE_CODE(sizeof(AEDesc *)))
		};
		
		enum {
			osaExecuteProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(long)))
				 | STACK_ROUTINE_PARAMETER(5, SIZE_CODE(sizeof(OSAID*)))
		};
		
		enum {
			osaDisplayProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(DescType)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(long)))
				 | STACK_ROUTINE_PARAMETER(5, SIZE_CODE(sizeof(AEDesc *)))
		};
		
		enum {
			osaScriptErrorProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSType)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(DescType)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(AEDesc *)))
		};
		
		enum {
			osaDisposeProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSAID)))
		};
		
		enum {
			osaSetScriptInfoProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(OSType)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(long)))
		};
		
		enum {
			osaGetScriptInfoProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(OSType)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(long *)))
		};
		
		enum {
			osaCompileProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(AEDesc *)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(long)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(OSAID*)))
		};
		
		enum {
			osaGetSourceProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(DescType)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(AEDesc *)))
		};
		
		enum {
			osaCoerceFromDescProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(AEDesc*)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(long)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(OSAID*)))
		};
		
		enum {
			osaCoerceToDescProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(DescType)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(long)))
				 | STACK_ROUTINE_PARAMETER(5, SIZE_CODE(sizeof(AEDesc*)))
		};
		
		enum {
			osaStartRecordingProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSAID *)))
		};
		
		enum {
			osaStopRecordingProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSAID)))
		};
		
		enum {
			osaScriptingComponentNameProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(AEDesc *)))
		};
		
		enum {
			osaLoadExecuteProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(AEDesc*)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(long)))
				 | STACK_ROUTINE_PARAMETER(5, SIZE_CODE(sizeof(OSAID*)))
		};
		
		enum {
			osaCompileExecuteProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(AEDesc*)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(long)))
				 | STACK_ROUTINE_PARAMETER(5, SIZE_CODE(sizeof(OSAID*)))
		};
		
		enum {
			osaDoScriptProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(AEDesc*)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(DescType)))
				 | STACK_ROUTINE_PARAMETER(5, SIZE_CODE(sizeof(long)))
				 | STACK_ROUTINE_PARAMETER(6, SIZE_CODE(sizeof(AEDesc*)))
		};
		
		enum {
			osaMakeContextProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(AEDesc*)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(OSAID*)))
		};
		
		enum {
			osaSetResumeDispatchProcProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(AEEventHandlerUPP)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(long)))
		};
		
		enum {
			osaGetResumeDispatchProcProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(AEEventHandlerUPP*)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(long*)))
		};
		
		enum {
			osaExecuteEventProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(AppleEvent*)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(long)))
				 | STACK_ROUTINE_PARAMETER(5, SIZE_CODE(sizeof(OSAID*)))
		};
		
		enum {
			osaDoEventProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(AppleEvent*)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(OSAID)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(long)))
				 | STACK_ROUTINE_PARAMETER(5, SIZE_CODE(sizeof(AppleEvent*)))
		};
		
		enum {
			osaSetActiveProcProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSAActiveUPP)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(long)))
		};
		
		enum {
			osaSetDebugProcProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSADebugUPP)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(long)))
		};
		
		enum {
			osaDebugProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSType)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(AEDesc*)))
				 | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(DescType)))
				 | STACK_ROUTINE_PARAMETER(5, SIZE_CODE(sizeof(AEDesc*)))
		};
		
		enum {
			osaSetSendProcProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSASendUPP)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(long)))
		};
		
		enum {
			osaGetSendProcProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSASendUPP*)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(long*)))
		};
		
		enum {
			osaSetCreateProcProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSACreateAppleEventUPP)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(long)))
		};
		enum {
			osaGetCreateProcProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(OSACreateAppleEventUPP*)))
				 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(long*)))
		};
		enum {
			osaSetDefaultTargetProcInfo = kPascalStackBased
				 | RESULT_SIZE(SIZE_CODE(sizeof(OSAError)))
				 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(hdlcomponentglobals)))
				 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(AEAddressDesc*)))
		};


		
		/*
			For Carbon we have to build univeral procedure pointers at runtime.
			So we just declare the UPPs here.
		*/
		
		ComponentRoutineUPP	cmpcloseDesc;
		ComponentRoutineUPP	cmpcandoDesc;
		ComponentRoutineUPP	cmpversionDesc;
		ComponentRoutineUPP	osaLoadDesc;
		ComponentRoutineUPP	osaStoreDesc;
		ComponentRoutineUPP	osaExecuteDesc;
		ComponentRoutineUPP	osaDisplayDesc;
		ComponentRoutineUPP	osaScriptErrorDesc;
		ComponentRoutineUPP	osaDisposeDesc;
		ComponentRoutineUPP	osaSetScriptInfoDesc;
		ComponentRoutineUPP	osaGetScriptInfoDesc;
		ComponentRoutineUPP	osaCompileDesc;
		ComponentRoutineUPP	osaGetSourceDesc;
		ComponentRoutineUPP	osaCoerceFromDescDesc;
		ComponentRoutineUPP	osaCoerceToDescDesc;
		ComponentRoutineUPP	osaStartRecordingDesc;
		ComponentRoutineUPP	osaStopRecordingDesc;
		ComponentRoutineUPP	osaScriptingComponentNameDesc;
		ComponentRoutineUPP	osaLoadExecuteDesc;
		ComponentRoutineUPP	osaCompileExecuteDesc;
		ComponentRoutineUPP	osaDoScriptDesc;
		ComponentRoutineUPP	osaMakeContextDesc;
		ComponentRoutineUPP	osaSetResumeDispatchProcDesc;
		ComponentRoutineUPP	osaGetResumeDispatchProcDesc;
		ComponentRoutineUPP	osaExecuteEventDesc;
		ComponentRoutineUPP	osaDoEventDesc;
		ComponentRoutineUPP	osaSetActiveProcDesc;
		ComponentRoutineUPP	osaSetDebugProcDesc;
		ComponentRoutineUPP	osaDebugDesc;
		ComponentRoutineUPP	osaSetSendProcDesc;
		ComponentRoutineUPP	osaGetSendProcDesc;
		ComponentRoutineUPP	osaSetCreateProcDesc;
		ComponentRoutineUPP	osaGetCreateProcDesc;
	
		OSAActiveUPP	osaclientactiveDesc;
		AEIdleUPP	osaclientidleDesc;
		OSASendUPP	osaclientsendDesc;
		
		#define osaclientactiveUPP (osaclientactiveDesc)
		#define osaclientidleUPP (osaclientidleDesc)
		#define osaclientsendUPP (osaclientsendDesc)

	/*
	static RoutineDescriptor osaSetDefaultTargetDesc = BUILD_ROUTINE_DESCRIPTOR (osaSetDefaultTargetProcInfo, osaSetDefaultTarget);
	*/

#define kOSAScriptIsBeingEdited			'edit'
	// Selector returns boolean.

#define kOSAScriptIsBeingRecorded		'recd'
	// Selector returns boolean.

#pragma pack(2)
typedef struct tyservercomponent {
	
	struct tyservercomponent **hnext;
	
	ComponentInstance instance;
	
	ProcessSerialNumber clientpsn; /*3.015*/
	
	OSType type;
	} tyservercomponent, *ptrservercomponent, **hdlservercomponent;


typedef struct tyclientlist {
	
	hdlcomponentglobals hfirst;
	
	} tyclientlist, **hdlclientlist;
#pragma options align=reset


enum { /*recording strings*/
	
	nullstring = 1,
	
	afterstring,
	
	beforestring,
	
	beginningofstring,
	
	endofstring,
	
	replacingstring,
	
	insertionlocstring,
	
	withobjectmodelstring,
	
	bringtofrontstring,
	
	sysbringapptofrontstring,
	
	idstring,
	
	appleeventstring,
	
	noverbtablestring,
	
	multipleclientsstring,
	
	specificclientstring
	};


Component osacomponent = nil;

boolean flosashutdown = false;


static hdlservercomponent hserverlist = nil;

static hdlclientlist hclientlist = nil;


static byte bssource [] = "\p_source";

static byte bscode [] = "\p_code";


/*
static byte bscontext [] = "\p_context";

static byte bsmodeflags [] = "\p_flags";
*/



/*
typedef struct tystubrecord {
	
	unsigned short jmp;
	
	ProcPtr adr;
	} tystubrecord, **hdlstubrecord;
*/

#pragma pack(2)
typedef struct tystylerecord {
	
	short ctstyles;
	
	ScrpSTElement styles [1];
	} tystylerecord;
#pragma options align=reset


static THz homezone;


static short homeresfile;

static ProcessSerialNumber homepsn;


hdlcomponentglobals osaglobals = nil;

static long osacoercionhandlerinstalled = 0;

static long osabackgroundtime = 0;



void disposecomponentglobals (hdlcomponentglobals hglobals) {
	
	/*
	dispose globals entirely.
	
	need to swap in our tablestack while disposing storage table in case 
	global environment isn't hospitable
	*/
	
	register hdlcomponentglobals hcg = hglobals;
	register hdltablestack hs = hashtablestack; /*save*/
	long ctbytes = longinfinity;
	
	if (hcg == nil)
		return;
	
	listunlink ((hdllinkedlist) hclientlist, (hdllinkedlist) hcg);
	
	hashtablestack = (**(**hcg).clientthreadglobals).htablestack;
		
  DisposeOSAActiveUPP((**hcg).activeproc);
  DisposeOSASendUPP((**hcg).sendproc);
  DisposeOSACreateAppleEventUPP((**hcg).createproc);
  
  DisposeComponentFunctionUPP((**hcg).cmpcloseUPP);
  DisposeComponentFunctionUPP((**hcg).cmpcandoUPP);
  DisposeComponentFunctionUPP((**hcg).cmpversionUPP);
  DisposeComponentFunctionUPP((**hcg).osaLoadUPP);
  DisposeComponentFunctionUPP((**hcg).osaStoreUPP);
  DisposeComponentFunctionUPP((**hcg).osaExecuteUPP);
  DisposeComponentFunctionUPP((**hcg).osaDisplayUPP);
  DisposeComponentFunctionUPP((**hcg).osaScriptErrorUPP);
  DisposeComponentFunctionUPP((**hcg).osaDisposeUPP);
  DisposeComponentFunctionUPP((**hcg).osaSetScriptInfoUPP);
  DisposeComponentFunctionUPP((**hcg).osaGetScriptInfoUPP);
  DisposeComponentFunctionUPP((**hcg).osaCompileUPP);
  DisposeComponentFunctionUPP((**hcg).osaGetSourceUPP);
  DisposeComponentFunctionUPP((**hcg).osaCoerceFromDescUPP);
  DisposeComponentFunctionUPP((**hcg).osaCoerceToDescUPP);
  DisposeComponentFunctionUPP((**hcg).osaStartRecordingUPP);
  DisposeComponentFunctionUPP((**hcg).osaStopRecordingUPP);
  DisposeComponentFunctionUPP((**hcg).osaScriptingComponentNameUPP);
  DisposeComponentFunctionUPP((**hcg).osaLoadExecuteUPP);
  DisposeComponentFunctionUPP((**hcg).osaCompileExecuteUPP);
  DisposeComponentFunctionUPP((**hcg).osaDoScriptUPP);
  DisposeComponentFunctionUPP((**hcg).osaMakeContextUPP);
  DisposeComponentFunctionUPP((**hcg).osaSetResumeDispatchProcUPP);
  DisposeComponentFunctionUPP((**hcg).osaGetResumeDispatchProcUPP);
  DisposeComponentFunctionUPP((**hcg).osaExecuteEventUPP);
  DisposeComponentFunctionUPP((**hcg).osaDoEventUPP);
  DisposeComponentFunctionUPP((**hcg).osaSetActiveProcUPP);
  DisposeComponentFunctionUPP((**hcg).osaSetDebugProcUPP);
  DisposeComponentFunctionUPP((**hcg).osaDebugUPP);
  DisposeComponentFunctionUPP((**hcg).osaSetSendProcUPP);
  DisposeComponentFunctionUPP((**hcg).osaGetSendProcUPP);
  DisposeComponentFunctionUPP((**hcg).osaSetCreateProcUPP);
  DisposeComponentFunctionUPP((**hcg).osaGetCreateProcUPP);
		
	disposehashtable ((**hcg).storagetable, false);
	
	hashtablestack = hs; /*restore*/
	
	// assert ((**hcg).clientthreadglobals != getcurrentthreadglobals ());
	
	disposethreadglobals ((**hcg).clientthreadglobals);
	
	/*
	disposehandle ((Handle) (**hcg).hMSglobals);
	*/
	
	disposehandle ((Handle) hcg);
	
	hashflushcache (&ctbytes); // 5.1b23 dmb: don't reuse any handles allocated
	} /*disposecomponentglobals*/


pascal OSErr osadefaultactiveproc (long refcon) {
#pragma unused (refcon)
	/*
	see if user pressed cmd-period.
	
	this gets called in client's context, so don't call any of our routines. in 
	any case, keyboardescape has its own timing mechanism that we don't want.
	
	4.1b13 dmb: use new iscmdperiodevent
	*/
	
	EventRecord ev;
  
	//Code change by Timothy Paustian Friday, June 16, 2000 1:35:10 PM
	//Changed to Opaque call for Carbon
	//updated to new call
  EventTypeSpec eventType;
  eventType.eventClass = kEventClassKeyboard;
  eventType.eventKind = kEventRawKeyDown;
  EventRef ref = AcquireFirstMatchingEventInQueue(GetCurrentEventQueue(), 1, &eventType, kEventQueueOptionsNone);
  if (ref) {
    ConvertEventRefToEventRecord(ref, &ev);
    
    if (iscmdperiodevent (ev.message, ev.what, ev.modifiers)) {
      return (userCanceledErr);
    }
  }
	return (noErr);
	} /*osadefaultactiveproc*/


static pascal OSErr
osadefaultcreate (
		AEEventClass		 class,
		AEEventID			 id,
		const AEAddressDesc	*target,
		short				 returnID,
		long				 transactionID,
		AppleEvent			*result,
		long				 refcon)
{
#pragma unused (refcon)

	return (AECreateAppleEvent(class, id, target, returnID, transactionID, result));
	} /*osadefaultcreate*/


static pascal OSErr
osadefaultsend (
		const AppleEvent	*event,
		AppleEvent			*reply,
		AESendMode			 sendmode,
		AESendPriority		 priority,
		long				 timeout,
		AEIdleUPP			 idleproc,
		AEFilterUPP			 filterproc,
		long				 refcon)
{
#pragma unused (refcon)

	return (AESend (event, reply, sendmode, priority, timeout, idleproc, filterproc));
	} /*osadefaultsend*/



	//Code change by Timothy Paustian Friday, July 21, 2000 10:52:57 PM
	//I think I can get away with this because only frontier code calls it.


OSAActiveUPP osadefaultactiveDesc = nil;
OSACreateAppleEventUPP osadefaultcreateDesc = nil;
OSASendUPP osadefaultsendDesc = nil;

#define osadefaultactiveUPP (osadefaultactiveDesc)
#define osadefaultcreateUPP (osadefaultcreateDesc)
#define osadefaultsendUPP (osadefaultsendDesc)

boolean newcomponentglobals (Component self, long clienta5, hdlcomponentglobals *hglobals) {
	
	/*
	encapsulate as much state information as possible for this component instance.
	
	we include an entire set of thread globals to avoid ad-hoc saving & restoring of 
	shell and lang globals & callbacks.
	
	5.0b7 dmb: allocate storage table in our own heap; langhash caches them
	*/
	
	register hdlcomponentglobals hcg;
	register hdlthreadglobals htg;
	hdlhashtable storagetable;
	hdlthreadglobals hthreadglobals;
	ProcessSerialNumber psn;
	OSType appid;
	Boolean flsame;
	boolean fl;
	
	if (!newclearhandle (sizeof (tycomponentglobals), (Handle *) hglobals))
		return  (false);
	
	hcg = *hglobals;
	
	listlink ((hdllinkedlist) hclientlist, (hdllinkedlist) hcg);
	
	GetCurrentProcess (&psn);
	
	appid = getprocesscreator ();
	
	if ((SameProcess (&psn, &homepsn, &flsame) == noErr) && flsame)
		(**hcg).isHomeProcess = true;
	
	(**hcg).self = self;
	
	(**hcg).clienta5 = clienta5;
	
	//Code change by Timothy Paustian Friday, June 16, 2000 1:39:07 PM
	//Changed to Opaque call for Carbon - you can't use this in carbon anyway
	
		
	(**hcg).clientid = appid;
	
	(**hcg).clientpsn = psn;
	
		
	fl = newhashtable (&storagetable);
	
	
	if (!fl) {
	
		disposehandle ((Handle) hcg);
	
		return (false);
		}
	
	(**hcg).storagetable = storagetable;
	
	
  (**hcg).activeproc = NewOSAActiveUPP(osadefaultactiveproc);
  (**hcg).createproc = NewOSACreateAppleEventUPP(osadefaultcreate);
  (**hcg).sendproc = NewOSASendUPP(osadefaultsend);

  //Code change by Timothy Paustian Sunday, September 3, 2000 9:57:20 PM
  //We have to create theses all and store them in the globals variables.

  (**hcg).cmpcloseUPP = NewComponentFunctionUPP(cmpclose, cmpcloseProcInfo);
  (**hcg).cmpcandoUPP = NewComponentFunctionUPP((ProcPtr) cmpcando, cmpcandoProcInfo);
  (**hcg).cmpversionUPP = NewComponentFunctionUPP(cmpversion, cmpversionProcInfo);
  (**hcg).osaLoadUPP = NewComponentFunctionUPP(osaLoad, osaLoadProcInfo);
  (**hcg).osaStoreUPP = NewComponentFunctionUPP(osaStore, osaStoreProcInfo);
  (**hcg).osaExecuteUPP = NewComponentFunctionUPP(osaExecute, osaExecuteProcInfo);
  (**hcg).osaDisplayUPP = NewComponentFunctionUPP(osaDisplay, osaDisplayProcInfo);
  (**hcg).osaScriptErrorUPP = NewComponentFunctionUPP(osaScriptError, osaScriptErrorProcInfo);
  (**hcg).osaDisposeUPP = NewComponentFunctionUPP(osaDispose, osaDisposeProcInfo);
  (**hcg).osaSetScriptInfoUPP = NewComponentFunctionUPP(osaSetScriptInfo, osaSetScriptInfoProcInfo);
  (**hcg).osaGetScriptInfoUPP = NewComponentFunctionUPP(osaGetScriptInfo, osaGetScriptInfoProcInfo);
  (**hcg).osaCompileUPP = NewComponentFunctionUPP(osaCompile, osaCompileProcInfo);
  (**hcg).osaGetSourceUPP = NewComponentFunctionUPP(osaGetSource, osaGetSourceProcInfo);
  (**hcg).osaCoerceFromDescUPP = NewComponentFunctionUPP(osaCoerceFromDesc, osaCoerceFromDescProcInfo);
  (**hcg).osaCoerceToDescUPP = NewComponentFunctionUPP(osaCoerceToDesc, osaCoerceToDescProcInfo);
  (**hcg).osaStartRecordingUPP = NewComponentFunctionUPP(osaStartRecording, osaStartRecordingProcInfo);
  (**hcg).osaStopRecordingUPP = NewComponentFunctionUPP(osaStopRecording, osaStopRecordingProcInfo);
  (**hcg).osaScriptingComponentNameUPP = NewComponentFunctionUPP(osaScriptingComponentName, osaScriptingComponentNameProcInfo);
  (**hcg).osaLoadExecuteUPP = NewComponentFunctionUPP(osaLoadExecute, osaLoadExecuteProcInfo);
  (**hcg).osaCompileExecuteUPP = NewComponentFunctionUPP(osaCompileExecute, osaCompileExecuteProcInfo);
  (**hcg).osaDoScriptUPP = NewComponentFunctionUPP(osaDoScript, osaDoScriptProcInfo);
  (**hcg).osaMakeContextUPP = NewComponentFunctionUPP(osaMakeContext, osaMakeContextProcInfo);
  (**hcg).osaSetResumeDispatchProcUPP = NewComponentFunctionUPP(osaSetResumeDispatchProc, osaSetResumeDispatchProcProcInfo);
  (**hcg).osaGetResumeDispatchProcUPP = NewComponentFunctionUPP(osaGetResumeDispatchProc, osaGetResumeDispatchProcProcInfo);
  (**hcg).osaExecuteEventUPP = NewComponentFunctionUPP(osaExecuteEvent, osaExecuteEventProcInfo);
  (**hcg).osaDoEventUPP = NewComponentFunctionUPP(osaDoEvent, osaDoEventProcInfo);
  (**hcg).osaSetActiveProcUPP = NewComponentFunctionUPP(osaSetActiveProc, osaSetActiveProcProcInfo);
  (**hcg).osaSetDebugProcUPP = NewComponentFunctionUPP(osaSetDebugProc, osaSetDebugProcProcInfo);
  (**hcg).osaDebugUPP = NewComponentFunctionUPP(osaDebug, osaDebugProcInfo);
  (**hcg).osaSetSendProcUPP = NewComponentFunctionUPP(osaSetSendProc, osaSetSendProcProcInfo);
  (**hcg).osaGetSendProcUPP = NewComponentFunctionUPP(osaGetSendProc, osaGetSendProcProcInfo);
  (**hcg).osaSetCreateProcUPP = NewComponentFunctionUPP(osaSetCreateProc, osaSetCreateProcProcInfo);
  (**hcg).osaGetCreateProcUPP = NewComponentFunctionUPP(osaGetCreateProc, osaGetCreateProcProcInfo);
  
	if (!newthreadglobals (&hthreadglobals)) {
		
		disposecomponentglobals (hcg);
		
		return (false);
		}
	
	htg = hthreadglobals;
	
	(**hcg).clientthreadglobals = htg;	// 4.1b3 dmb: use local
	
	(**htg).htable = storagetable;
	
	(**htg).applicationid = appid;
	
		// 2/28/97 dmb: set up langcallbacks here so they'll always be in effect
		
		(**htg).langcallbacks.backgroundtaskcallback = &osabackgroundtask;
		
		(**htg).langcallbacks.debuggercallback = &osadebugger;
		
		(**htg).langcallbacks.pushsourcecodecallback = &scriptpushsourcecode;
		
		(**htg).langcallbacks.popsourcecodecallback = &scriptpopsourcecode;
		
		(**htg).langcallbacks.partialeventloopcallback = &osapartialeventloop;
		
		(**htg).fldisableyield = true;
	
	return (true);
	} /*newcomponentglobals*/


OSAError osageterror (void) {
	
	long n;
	
	n = getoserror ();
	
	if (n == noErr)
		n = errOSAScriptError;
	
	return (n);
	} /*osageterror*/


static boolean inosasource (void) {
	
	/*
	very carefully call back to the client's debugging proc.
	*/
	
	register hdlerrorstack hs = langcallbacks.scripterrorstack;
	
	if ((hs != nil) && ((**hs).toperror > 1))
		if ((**hs).stack [(**hs).toperror - 1].errorrefcon != -1L) /*we're in a call to another script*/
			return (false);
	
	return (true);
	} /*inosasource*/


static boolean osaerrormessage (bigstring bs, ptrvoid refcon) {
#pragma unused (refcon)

	AEDesc
		list,
		rec,
		desc;
	long n;
	OSErr err;
	
	list = (**osaglobals).errordesc;
	
	AEDisposeDesc (&list);
	
	n = osageterror ();
	
	err = AECreateList (nil, 0, true, &list);
	
	if (err != noErr) {
		
			newdescnull (&list, typeNull);
		
		}
	else {
		
		err = AEPutKeyPtr (&list, kOSAErrorNumber, typeLongInteger, (Ptr) &n, sizeof (n));
		
		err = AEPutKeyPtr (&list, kOSAErrorMessage, typeChar, (Ptr) bs + 1, stringlength (bs));
		
		err = AEPutKeyPtr (&list, kOSAErrorBriefMessage, typeChar, (Ptr) bs + 1, stringlength (bs));
		
		err = AECreateList (nil, 0, true, &rec);
		
		if (err == noErr) {
			
			long ix;
			
			if (inosasource ())
				ix = langgetsourceoffset (ctscanlines, ctscanchars);
			else
				ix = 0;
			
			err = AEPutKeyPtr (&rec, keyOSASourceStart, typeLongInteger, (Ptr) &ix, sizeof (ix));
			
			err = AEPutKeyPtr (&rec, keyOSASourceEnd, typeLongInteger, (Ptr) &ix, sizeof (ix));
			
			err = AECoerceDesc (&rec, typeOSAErrorRange, &desc);
			
			AEDisposeDesc (&rec);
			
			if (err == noErr) {
				
				err = AEPutKeyDesc (&list, kOSAErrorRange, &desc);
				
				AEDisposeDesc (&desc);
				}
			}
		}
	
	(**osaglobals).errordesc = list;
	
	return (false); /*consume the error*/
	} /*osaerrormessage*/


static pascal OSErr
coerceTEXTtoSTXT (
		DescType	 fromtype,
		Ptr			 pdata,
		long		 size,
		DescType	 totype,
		long		 refcon,
		AEDesc		*result) {
#pragma unused(fromtype, refcon)
	/*
	2.1b2 dmb: don't use clearbytes so we don't have to set up a5
	*/
	
	tystylerecord stylerecord;
	OSErr err;
	AEDesc list;
	register ScrpSTElement *pstyle;
	
	#ifdef fldebug	// 2006-04-04 - kw --- this was fldegug
	
	if (totype != typeStyledText)
		DebugStr ("\punexpected coercion");
	
	#endif
	
	stylerecord.ctstyles = 1;
	
	pstyle = &stylerecord.styles [0];
	
	(*pstyle).scrpStartChar = 0;
	
	(*pstyle).scrpHeight = 14;
	
	(*pstyle).scrpAscent = 12;
	
	(*pstyle).scrpFont = geneva;
	
	(*pstyle).scrpSize = 9;
	
	(*pstyle).scrpFace = 0;
	
	(*pstyle).scrpColor.red = 0;
	
	(*pstyle).scrpColor.green = 0;
	
	(*pstyle).scrpColor.blue = 0;
	
	err = AECreateList (nil, 0, true, &list);
	
	if (err == noErr) {
		
		err = AEPutKeyPtr (&list, 'ksty', 'styl', (Ptr) &stylerecord, sizeof (stylerecord));
		
		if (err == noErr)
			err = AEPutKeyPtr (&list, 'ktxt', typeChar, pdata, size);
		
		if (err == noErr)
			err = AECoerceDesc (&list, typeStyledText, result);
		
		AEDisposeDesc (&list);
		}
	
	return (err);
	} /*coerceTEXTtoSTXT*/


static pascal OSErr
coerceTypetoObj (
		AEDesc		*desc,
		DescType	 totype,
		long		 refcon,
		AEDesc		*result) {
#pragma unused(totype, refcon)

	/*
	2.1b1 dmb: if Frontier passes a string4 identifier where an object specifier 
	is required, this coercion handler will take care of it
	*/
	
	AEDesc containerdesc;
	OSErr err;
	
	
	#ifdef fldebug
		if ((*desc).descriptorType != typeType)
			DebugStr ("\punexpected coercion");
	#endif
	
  /*PBS 03/14/02: AE OS X fix.*/
  newdescnull (&containerdesc, typeNull);
	
	err = CreateObjSpecifier (cProperty, &containerdesc, formPropertyID, desc, false, result);
	
	return (err);
	} /*coerceTypetoObj*/


//Code change by Timothy Paustian Friday, June 16, 2000 1:38:13 PM
//Changed to Opaque call for Carbon

AECoercionHandlerUPP	coerceTEXTtoSTXTDesc;
AECoercionHandlerUPP	coerceTypetoObjDesc;

#define coerceTEXTtoSTXTUPP (coerceTEXTtoSTXTDesc)
#define coerceTypetoObjUPP (coerceTypetoObjDesc)

static void osapushfastcontext (hdlcomponentglobals hglobals) {
	
	/*
	4/20/93 dmb: always set client's ccglobals to nil. if root has been closed, 
	we want nil globals to persist, or new globals to be picked up
	
	3.0b14 dmb: handling nested osa contexts
	
	4.1b3 dmb: nesting wasn't thoroughly handled by the hserverosaglobals field.
	added ctpushes field that handles nesting better, assuming it's simple nesting 
	of the same globals. to handle arbitrary, daisy chain globals nesting, we'd 
	need to keep a list or a stack of serverthreadglobals.
	*/
	
	register hdlcomponentglobals hcg = hglobals;
	register hdlthreadglobals htg;
	
	/*
	short rnum = OpenComponentResFile ((**hcg).self);
	*/
	
	if ((**hcg).ctpushes++ > 0) { // 4.1b3 dmb: nesting of same globals
	
		assert (osaglobals == hcg);
		
		assert ((**hcg).serverthreadglobals);
		
		assert ((**hcg).clientthreadglobals == getcurrentthreadglobals ());
		}
	else {
	
		(**hcg).hserverosaglobals = osaglobals; /*in case we're nested*/
		
		osaglobals = hcg; /*make visible to callbacks*/
		
		htg = getcurrentthreadglobals ();
		
		(**hcg).serverthreadglobals = htg;
		
		copythreadglobals (htg); /*save*/
		
		htg = (**hcg).clientthreadglobals;
		
		
		(**htg).hccglobals = nil; /*want to leave them untouched*/
		
		
		swapinthreadglobals (htg);
		}
	
	if (++osacoercionhandlerinstalled == 1) {
    if(coerceTEXTtoSTXTUPP == nil)
      coerceTEXTtoSTXTUPP = NewAECoerceDescUPP((AECoerceDescProcPtr)coerceTEXTtoSTXT);

		AEInstallCoercionHandler (typeChar, typeStyledText, coerceTEXTtoSTXTUPP, 0, false, false);
		}
	
	if (!(**hcg).isHomeProcess) {
	
		(**hcg).clientresfile = CurResFile ();
		
		//Code change by Timothy Paustian Friday, June 16, 2000 2:08:20 PM
		//Changed to Opaque call for Carbon
		//This is not supported in carbon. You cannot use res files in OS X
		//We may have some serious rewriting to do for this.
		
		UseResFile (homeresfile);
		}
	} /*osapushfastcontext*/


static void osapopfastcontext (hdlcomponentglobals hglobals) {
	
	/*
	3.0b14 dmb: handling nested osa contexts
	*/
	
	register hdlcomponentglobals hcg = hglobals;
	
	/*
	CloseComponentResFile (rnum);
	*/
	
	if (--(**hcg).ctpushes == 0) { // 4.1b3 dmb: no nesting of same globals
	
		copythreadglobals ((**hcg).clientthreadglobals);
		
		swapinthreadglobals ((**hcg).serverthreadglobals);
		
		osaglobals = (**hcg).hserverosaglobals; /*in case we were nested*/
		}
	
	if (--osacoercionhandlerinstalled == 0)
	{
		AERemoveCoercionHandler (typeChar, typeStyledText, coerceTEXTtoSTXTUPP, false);

		//Code change by Timothy Paustian Friday, July 21, 2000 11:02:21 PM
		//added dispose of coercion handler
    if(coerceTEXTtoSTXTUPP != nil) {
      DisposeAECoerceDescUPP(coerceTEXTtoSTXTUPP);
      coerceTEXTtoSTXTUPP = nil;
      }
	}
	if (!(**hcg).isHomeProcess) {
		UseResFile ((**hcg).clientresfile);
		}
	} /*osapopfastcontext*/


long osapreclientcallback (hdlcomponentglobals hglobals) {
	
	/*
	before we put up standard file, or the ppc browser, we need to put the 
	clients environment in order, and make sure that the Frontier environment 
	is restored, since it may get swapped in by the process manager.
	
	case in point: Norton Directory Assistance trashes both our heap & the 
	client's if we don't do this.
	
	since we're split into a pre- and post- routine, our caller has to manage 
	A5 for us. (we could probably get around this, but I think it would be 
	even messier.)
	*/
	
	register hdlcomponentglobals hcg = hglobals;
	
	osapopfastcontext (hcg);
	
	return ((**hcg).clienta5);
	} /*osapreclientcallback*/


void osapostclientcallback (hdlcomponentglobals hglobals) {
	
	osapushfastcontext (hglobals);
	} /*osapostclientcallback*/



GNEUPP osainstallpatch (hdlcomponentglobals hglobals) {
#pragma unused (hglobals)

	return (nil);
	} /*osainstallpatch*/


void osaremovepatch (hdlcomponentglobals hglobals, GNEUPP origproc) {
#pragma unused (hglobals, origproc)

	} /*osaremovepatch*/



static boolean osapartialeventloop (UInt16 desiredevents) {
	
	/*
	2.1b8 dmb: we can't call waitnextevent on behalf of the client, because 
	the OSA doesn't provide an API for safely doing so. so we return false.
	
	in case we're being called to handle a process switch, we'd better 
	un-hilite any menu. otherwise, the menu manager gets confused, and 
	the menubar can be trashed when the no-longer-current app calls 
	HiliteMenu (0)
	
	2.1b11 dmb: one way we can call WNE for our client is to send an 
	apple event. so we'll send one to Frontier.
	
	3.0b15 dmb: if we're our own client, sending the noop event won't do 
	use any good -- the AE will be short-circuited and return EventNotHandled, 
	and the idleproc won't be called. so we want to call our client thread's 
	partialeventloop. this should be retrievable through the clientthreadglobals 
	of our osaglobals, but we're going to take a little shortcut and just 
	call shellpartialeventloop directly. not as good as ccpartialeventloop (the 
	normal langpartialeventloop), but it'll do for now. time to ship!
	
	3.0a dmb: must do HiliteMenu (0) even if we are our own client; the 
	problem shows up in Runtime's own shared menus.
	*/
	
	HiliteMenu (0);
	
	if (iscurrentapplication (homepsn))
		return (shellpartialeventloop (desiredevents));
	
	return (langipcnoop ());
	} /*osapartialeventloop*/

static pascal OSErr osacreateevent (AEEventClass class, AEEventID id,
                    const AEAddressDesc *target, short returnID,
                    long transactionID, AppleEvent *result) {

	/*
	2.1b4 dmb: as part of "event sending" support, we need to do this. it 
	should be reasonable to assume that a client's "create" proc won't 
	call WNE, so we don't do quite as much futzing around as when sending 
	events
	*/
	
	register hdlcomponentglobals hcg = osaglobals;
	OSErr err;
	
	osapopfastcontext (hcg);
	
  err = InvokeOSACreateAppleEventUPP (class, id, target, returnID, transactionID, result, (**hcg).createprocrefcon, (**hcg).createproc);
		
	osapushfastcontext (hcg);
	
	return (err);
	} /*osacreateevent*/


static pascal OSErr
osasendevent (
		const AppleEvent	*event,
		AppleEvent			*reply,
		AESendMode			 mode,
		AESendPriority		 priority,
		long				 timeout,
		AEIdleUPP			 idleproc,
		AEFilterUPP			 filterproc )
{
#pragma unused (idleproc, filterproc)
	
	/*
	2/16/93 dmb: in case the event is to be direct dispatched correctly, we need 
	to set up the client's A5 world
	
	2.1b5 dmb: watch out for nil sendProc -- the AS script editor sets this.
	*/
	
	register hdlcomponentglobals hcg = osaglobals;
	OSErr err;
	register GNEUPP getnexteventproc;
	OSASendUPP sendproc;
	
	sendproc = (**hcg).sendproc;
	
	getnexteventproc = (**hcg).getnexteventproc;
	
	osapopfastcontext (hcg);
	
	if (getnexteventproc != nil)
		osaremovepatch (hcg, nil); /*unpatch*/
  err = InvokeOSASendUPP (event, reply, mode, priority, timeout, nil, nil, (**hcg).sendprocrefcon, sendproc);
	
	if (getnexteventproc != nil)
		osainstallpatch (hcg); /*repatch*/
	osapushfastcontext (hcg);
	
	return (err);
	} /*osasendevent*/


static boolean osabackgroundtask (boolean flresting) {
	
	/*
	very carefully call back to the client.
	*/
	
	register hdlcomponentglobals hcg = osaglobals;
	OSErr err;
	
	if ((**hcg).activeproc == nil) /*no callback provided*/
		return (true);
	
	if (langdialogrunning ()) /*no can do*/
		return (true);
	
	if (langerrorenabled ()) // 5.0b7 dmb: another time not to background
		return (true);

	if ((gettickcount () - osabackgroundtime < 20) && (!flresting)) /*not time*/
		return (true);
	
	flscriptresting = flresting;
	
	osapopfastcontext (hcg);
	
	osaremovepatch (hcg, nil); /*unpatch*/
	
  err = InvokeOSAActiveUPP ((**hcg).activeprocrefcon, (**hcg).activeproc);
	
	osainstallpatch (hcg); /*repatch*/
		
	osapushfastcontext (hcg);
	
	flscriptresting = false;
	
	osabackgroundtime = gettickcount ();
	
	return (!oserror (err));
	} /*osabackgroundtask*/


static boolean osadebugger (hdltreenode hnode) {
	
	/*
	very carefully call back to the client's debugging proc.
	*/
	
	register hdlcomponentglobals hcg = osaglobals;
	register hdltreenode hn = hnode;
	tytreetype op;
	OSErr err;
	
	if (languserescaped (false)) /*stop running the script immediately*/
		return (false);
	
	if ((**hcg).debugproc == nil) /*no callback provided*/
		return (true);
	
	if (!debuggingcurrentprocess ())
		return (true);
	
	if (!inosasource ())
		return (true);
	
	op = (**hn).nodetype; /*test for "meaty" op*/
	
	if ((op == moduleop) || (op == noop) || (op == bundleop) || (op == localop)) /*never stop on these*/
		return (true);
	
	(**hcg).debugnode = hn;
	
	osapopfastcontext (hcg);
	
	osaremovepatch (hcg, nil); /*unpatch*/
		
	err = CallOSADebugProc ((**hcg).debugproc, (**hcg).debugprocrefcon);
	
	osainstallpatch (hcg); /*repatch*/
		
	osapushfastcontext (hcg);
	
	return (!oserror (err));
	} /*osadebugger*/



static boolean osaprocessstarted (void) {
	
	/*
	we don't want Frontier's menus to dim when serving osa scripts
	*/
	
	processnotbusy ();
	
	return (true);
	} /*osaprocessstarted*/



static boolean osahandlerunscript (hdlcomponentglobals hglobals, hdltreenode hcode, 
	hdlhashtable hcontext, long modeflags, tyvaluerecord *resultval) {
	
	/*
	12/31/92 dmb: cloned from langipc.c, this will need to deal with 
	lang/threadglobals more carefully in the future
	
	1/21/93 dmb: it now deals with lang/threadglobal very carefully indeed! final 
	touch: we path getnextevent so we can deal with it like a background callback. 
	note that we may need to do this with eventavail, waitnextevent as well.
	
	6/2/93 dmb: no longer override msg verb, since it now works OK along with 
	other frontier process-based verbs
	
	2.1b5 dmb: pass -1 for errorrefcon instead of zero to prevent top level 
	lexical scope from being transparent (just like QuickScript does). for 
	isosasource to work, we also need to hook up push/popsourcecode callbacks.
	
	2.1b12 dmb: restore all langcallbacks that we set
	
	2.1b13 dmb: added processstarted routine to prevent Frontier's menus from
	dimming when running OSA scripts.
	
	3.0b15 dmb: test heap space for 2K before trying to run a process.
	
	3.0.1b1 dmb: generate error message when no file is open
	*/
	
	register hdlcomponentglobals hcg = hglobals;
	hdlprocessrecord hprocess;
	register hdlprocessrecord hp;
	register boolean fl = false;
	GNEUPP origproc;
	long errorrefcon = -1;
	
	if (roottable == nil) {
		
		langerror (nofileopenerror); /*3.0.1b1*/
		
		return (false);
		}
	
	if ((modeflags & kOSAModeTransparentScope) != 0)
		errorrefcon = 0;
		
	if (!newprocess (hcode, true, nil, errorrefcon, &hprocess))
		goto exit;
	
	hp = hprocess; /*copy into register*/
	
	(**hp).processrefcon = (long) hglobals;
	
	(**hp).errormessagecallback = &osaerrormessage;
	
	(**hp).hcontext = hcontext;
	
		
		(**hp).fldebugging = bitboolean (modeflags & kOSAModeDebug);
		
		(**hp).processstartedroutine = &osaprocessstarted;
		
	
	
	osabackgroundtime = gettickcount () + 30;
	
	origproc = osainstallpatch (hcg);
		
	if (testheapspace (2 * 1024)) /*enough memory to run a process*/
		fl = processruncode (hp, resultval);
	else
		fl = false;
	
	osaremovepatch (hcg, origproc);
	
	
	(**hp).hcode = nil; /*we don't own it*/
	
	disposeprocess (hp);
	
	exit:
	
	return (fl);
	} /*osahandlerunscript*/


static pascal boolean osacreatemodulecontext (hdltreenode htree, hdlhashtable *hcontext) {
	
	/*
	create a new table containing any module definitions in htree
	*/
	
	register hdltreenode h = (**htree).param1;
	tyvaluerecord val;
	
	if (!newhashtable (hcontext))
		return (false);
	
	pushhashtable (*hcontext);
	
	for (h = (**htree).param1; h != nil; h = (**h).link) {
		
		if ((**h).nodetype == moduleop)
			evaluatetree (h, &val);
		}
	
	pophashtable ();
	
	return (true);
	} /*osacreatemodulecontext*/


static boolean lookupeventname (hdlcomponentglobals hglobals, AEEventClass class, AEEventID id, bigstring bsname) {
	
	/*
	try to find the name of the specified event in the 'aete' or 'aeut' resource
	
	12/7/93, 3.x dmb: use client's context and check its aete.
	*/
	
	register hdlcomponentglobals hcg = hglobals;
	Handle haete;
	boolean flgotname;
	long paramoffset;
	
	osapopfastcontext (hcg);
	
	haete = GetIndResource ('aete', 1);
	
	flgotname = osaparseaete (haete, class, id, bsname, &paramoffset);
	
	osapushfastcontext (hcg);
	
	if (!flgotname) {
		
		haete = GetIndResource ('aeut', 1);
		
		flgotname = osaparseaete (haete, class, id, bsname, &paramoffset);
		}
	
	return (flgotname);
	
	/*
	h = GetIndResource ('aeut', 1);
	
	if (osaparseaete (h, class, id, bsname, &paramoffset))
		return (true);
	
	return (false);
	*/
	} /*lookupeventname*/


static boolean osahandletrapverb (hdlcomponentglobals hglobals, hdlverbrecord hverb, 
	hdltreenode hmodule, long modeflags, tyvaluerecord *vreturned) {
	
	/*
	2.1b3 dmb: added support for subroutine event
	
	12/7/93 3.x dmb: lookupeventname now takes hglobals
	*/
	
	register boolean fl = false;
	register hdlverbrecord hv = hverb;
	bigstring bsname;
	tyvaluerecord val;
	hdltreenode hname;
	hdltreenode hparams;
	hdltreenode hcode;
	hdlhashtable hcontext;
	AEEventClass class;
	AEEventID id;
	
	if (!osacreatemodulecontext (hmodule, &hcontext))
		return (false);
	
	class = (**hv).verbclass;
	
	id = (**hv).verbtoken;
	
	if ((class == kOSASuite) && (id == kASSubroutineEvent)) {
		
		if (!landgetstringparam (hv, keyASSubroutineName, bsname)) 
			return (false);
		}
	else {
		
		ostypetostring (id, bsname);
		
		poptrailingwhitespace (bsname);
		}
	
	if (!hashtablesymbolexists (hcontext, bsname)) {
		
		// 2/14/97 dmb: what if it's an event intended to be handled by the odb, 
		// not by this script itself?
		// 2/27/97 dmb: answer - osahandleevent will now dispatch the event to Frontier's 
		// main ae handler.
		
		if (!lookupeventname (hglobals, class, id, bsname) || !hashtablesymbolexists (hcontext, bsname)) {
			
			if ((class == 'aevt') && (id == 'oapp')) /*no run handler, exec main body*/
				fl = osahandlerunscript (hglobals, hmodule, hcontext, modeflags, vreturned);
			else
				oserror (errAEEventNotHandled);
			
			goto exit;
			}
		}
	
	initvalue (&val, stringvaluetype);
	
	if (!newtexthandle (bsname, &val.data.stringvalue))
		goto exit;
	
	if (!newidnode (val, &hname))
		goto exit;
	
	if (!langipcbuildparamlist (nil, hv, &hparams)) {
		
		langdisposetree (hname);
		
		goto exit;
		}
	
	if (!pushbinaryoperation (functionop, hname, hparams, &hcode)) /*consumes input parameters*/
		goto exit;
	
	if (!pushbinaryoperation (moduleop, hcode, nil, &hcode)) /*needs this level*/
		goto exit;
	
	fl = osahandlerunscript (hglobals, hcode, hcontext, modeflags, vreturned);
	
	langdisposetree (hcode);
	
	exit:
	
	disposehashtable (hcontext, true);
	
	return (fl);
	} /*osahandletrapverb*/


static boolean osabuildsubroutineevent (bigstring bsname, hdltreenode hparam1, AppleEvent *event) {
	
	/*
	2.1b3 dmb: build an OSA subroutine event given the name and parameter list
	
	2.1b12 dmb: the guts have been moved into langipcbuildsubroutineevent
	*/
	
	AEDesc desc = {typeNull, nil};
	register OSErr err;
	
	err = AECreateAppleEvent (kOSASuite, kASSubroutineEvent, &desc, kAutoGenerateReturnID, kAnyTransactionID, event);
	
	if (oserror (err))
		return (false);
	
	return (langipcbuildsubroutineevent (event, bsname, hparam1)); /*disposes event on error*/
	} /*osabuildsubroutineevent*/


static boolean setstorageval (hdlcomponentglobals hglobals, tyvaluerecord *val, OSAID id) {
#pragma unused (hglobals)

	/*
	add val to the id table using the indicated id. on error, dispose of the value
	*/
	
	bigstring bs;
	
	numbertostring (id, bs);
	
	if (hashassign (bs, *val)) {
		
		exemptfromtmpstack (val);
		
		return (true);
		}
	
	disposevaluerecord (*val, false);
	
	return (false);
	} /*setstorageval*/


static boolean addstorageval (hdlcomponentglobals hglobals, tyvaluerecord *val, OSAID *id) {
	
	*id = (OSAID) ++(**hglobals).idcounter;
	
	return (setstorageval (hglobals, val, *id));
	} /*addstorageval*/


static boolean getstorageval (hdlcomponentglobals hglobals, OSAID id, tyvaluerecord *val, hdlhashnode * hnode) {
#pragma unused (hglobals)

	bigstring bs;
	
	numbertostring (id, bs);
	
	if (!hashlookup (bs, val, hnode)) {
		
		oserror (errOSAInvalidID);
		
		return (false);
		}
	
	return (true);
	} /*getstorageval*/


static boolean deletestorageval (hdlcomponentglobals hglobals, OSAID id) {
#pragma unused (hglobals)

	bigstring bs;
	
	numbertostring (id, bs);
	
	return (hashdelete (bs, true, false));
	} /*deletestorageval*/


static boolean storagevaltodesc (tyvaluerecord *val, OSType desctype, AEDesc *result) {
	
	/*
	create a descriptor containing a copy of val's data, coercing 
	to the requested type
	*/
	
	register tyvaluerecord *v;
	AEDesc desc;
	tyvaluerecord vtemp;
	OSErr err;
	tyvaluetype valtype;
	
	if (!copyvaluerecord (*val, &vtemp))
		return (false);
	
	v = &vtemp; /*copy into register*/
	
	if (desctype == typeStyledText)
		valtype = stringvaluetype;
	else
		valtype = langgetvaluetype (desctype);
	
	if (langgoodbinarytype (valtype)) { /*desired type is a valid Frontier type*/
		
		if (!coercevalue (v, valtype)) { /*apply UserTalk coercion*/
			
			disposevaluerecord (*v, true);
			
			return (false);
			}
		}
	
	if (!valuetodescriptor (v, &desc))
		return (false);
	
	if ((desctype != typeWildCard) && (desctype != desc.descriptorType)) { /*AE coercion needed*/
		
		err = AECoerceDesc (&desc, desctype, result);
		
		AEDisposeDesc (&desc);
		
		return (!oserror (err));
		}
	
	*result = desc;
	
	return (true);
	} /*storagevaltodesc*/


static boolean osagetcontext (hdlcomponentglobals hglobals, OSAID id, hdlhashtable *hcontext) {
	
	tyvaluerecord vcontext;
	hdlhashnode hnode;
	
	if (id == kOSANullScript)
		*hcontext = nil;
	
	else {
		
		if (!getstorageval (hglobals, id, &vcontext, &hnode))
			return (false);
		
		if (!langexternalvaltotable (vcontext, hcontext, hnode)) {
			
			oserror (errOSAInvalidID);
			
			return (false);
			}
		}
	
	return (true);
	} /*osagetcontext*/


static boolean osanewvalue (tyexternalid id, Handle hdata, tyvaluerecord *val) {
	
	/*
	5.1b23 dmb: langexternalnewvalue does stuff with prefs that may load an odb table 
	into memory. we need our own zone.
	*/
	
	boolean fl;

	fl = langexternalnewvalue (id, hdata, val);

  return (fl);
	} /*osanewvalue*/


static pascal OSAError
osaLoad (
		hdlcomponentglobals	 hglobals,
		const AEDesc		*scriptData,
		long				 modeFlags,
		OSAID				*resultingCompiledScriptID)
{
#pragma unused (modeFlags)
	
	/*
	2.1b1 dmb: don't insist that the loaded value is a context (i.e. a table)
	*/
	Handle		descData = nil;
	tyvaluerecord vscript, vsource, vcode;
	hdlhashtable hcontext;
	Handle hsource;
	hdltreenode hcode;
	Handle hdata;
	DescType subtype;
	boolean fl = false;
	OSAError err = noErr;
	hdlhashnode hnodesource;
	hdlhashnode hnodecode;
	
  if (!copydatahandle ((AEDesc*)scriptData, &descData))	/* AE OS X fix */
    return (memFullErr);
	
	switch (scriptData->descriptorType) {
		
		case typeLAND:
			
			fl = langunpackvalue (descData, &vscript);
			
			break;
		
		case typeOSAGenericStorage:
	
			err = OSAGetStorageType ((AEDataStorage) descData, &subtype);
			
			if (err != noErr)
				break;
			
			if (subtype != typeLAND) {
				err = errOSABadStorageType;
				break;
				}
			
			if (!copyhandle (descData, &hdata)) {
				err = memFullErr;
				break;
				}
			
			#ifdef SWAP_BYTE_ORDER
			{
				/*
				2006-04-17 aradke: This is a MAJOR HACK (FIXME)!
				
					Compiled osa scripts of the generic storage type have a 12-byte trailer:
					
					Bytes 0-3 contain the signature of the OSA component that knows
					how to execute the script, e.g. 'LAND' for the UserTalk component.
					The meaning of bytes 4-5 is unknown.
					Bytes 6-7 seem to contain the length of the trailer as a 16bit integer.
					Bytes 8-11 contain 0xFADEDEAD as a magic signature for the trailer.
								
					Example: 4C41 4E44 0001 000C FADE DEAD (see 'scpt' resource #1024 in iowaruntime.r)
					
					OSARemoveStorageType is supposed to remove this trailer if it is present.
					However, it doesn't perform byte-order swapping on the length bytes in the trailer.
					The call below ends up removing 3072 bytes instead of 12 bytes from hdata.
					
					Until we figure out how to properly deal with this bug(?), we use our own
					implementation of OSARemoveStorageType on Intel Macs.
				*/
				
				long hlen = gethandlesize (hdata);
				char * p = *hdata;
				long marker;
				
				p += (hlen - 4);
				
				marker = *(long *) p;
				
				disktomemlong (marker);
				
				if (marker == 0xFADEDEAD) {
					sethandlesize (hdata, hlen - 12);	//remove storage type trailer
					}
			}
			#else
				err = OSARemoveStorageType ((AEDataStorage) hdata);
			#endif
			
			if (err == noErr)
				fl = langunpackvalue (hdata, &vscript);

			disposehandle (hdata);
			
			break;
		
		default:
			err = errOSABadStorageType;
		}

  if(descData != nil) {

    disposehandle(descData);	/* AE OS X fix */

    descData = nil;
    }
	
	if (err != noErr)
		return (err);
	
	if (fl) {

		if (langexternalvaltotable (vscript, &hcontext, HNoNode)) {
			
			pushhashtable (hcontext);
			
			if (!hashlookup (bscode, &vcode, &hnodecode) && hashlookup (bssource, &vsource, &hnodesource)) { /*no code, but have source*/
				
				fl = copyhandle (vsource.data.stringvalue, &hsource);
				
				if (fl)
					fl = langbuildtree (hsource, false, &hcode); /*syntax error*/
				
				if (fl) {
					
					initvalue (&vcode, codevaluetype);
					
					vcode.data.codevalue = hcode;
					
					fl = hashinsert (bscode, vcode);
					}
				}
			
			(**hcontext).fldirty = false;
			
			pophashtable ();
			}
		}

	if (fl)
		fl = addstorageval (hglobals, &vscript, resultingCompiledScriptID);
	
	if (!fl)
		return (osageterror ());
	
	return (noErr);
	} /*osaLoad*/


static pascal OSAError osaStore (
			hdlcomponentglobals	hglobals,
			OSAID				compiledScriptID, 
			DescType			desiredType,
			long				modeFlags,
			AEDesc*				resultingScriptData) {
	
	/*
	4/26/93 dmb: support kOSAModePreventGetSource
	*/
	
	tyvaluerecord val;
	AEDesc desc;
	DescType descType;
	hdlhashtable hcontext;
	hdlhashnode hnode;
	boolean flunlinkedsource = false;
	boolean fl;
	OSAError err;
	Handle hpacked = nil;
	
	
	if (compiledScriptID == kOSANullScript)
		return (errOSAInvalidID);
	
	if ((desiredType != typeOSAGenericStorage) && (desiredType != typeLAND))
		return (errOSABadStorageType);
	
	if (!getstorageval (hglobals, compiledScriptID, &val, &hnode))
		return (errOSAInvalidID);
	
	if (langexternalvaltotable (val, &hcontext, hnode)) {
		
		if (modeFlags & kOSAModePreventGetSource) {
			
			pushhashtable (hcontext);
			
			flunlinkedsource = hashunlink (bssource, &hnode);
			
			pophashtable ();
			}
		}

	fl = langpackvalue (val, &hpacked, HNoNode);
	
	if (flunlinkedsource)
		hashinsertnode (hnode, hcontext);
	
	if (!fl)
		return (memFullErr);

	if (desiredType == typeOSAGenericStorage) {
		
		err = OSAAddStorageType ((AEDataStorage) hpacked, typeLAND);

		if (err != noErr) {
			disposehandle (hpacked);
			return (err);
			}
		
		descType = typeOSAGenericStorage;
		}
	else {
		descType = typeLAND;
		}

  fl = newdescwithhandle (&desc, descType, hpacked);
	
	*resultingScriptData = desc;
	
	return (noErr);
	} /*osaStore*/


pascal OSAError osaDispose (
			hdlcomponentglobals	hglobals,
			OSAID				scriptID) {
	
	boolean fl;
	
	fl = deletestorageval (hglobals, scriptID);
	
	if (!fl)
		return (osageterror ());
	
	return (noErr);
	} /*osaDispose*/


static pascal OSAError osaSetScriptInfo (
			hdlcomponentglobals	hglobals,
			OSAID				scriptID,
			OSType				selector,
			long				value) {
	
	tyvaluerecord vscript;
	hdlhashtable hcontext;
	hdlhashnode hnode;
	
	if (scriptID == kOSANullScript)
		return (errOSAInvalidID);
	
	if (!getstorageval (hglobals, scriptID, &vscript, &hnode))
		return (errOSAInvalidID);
	
	switch (selector) {
		
		case kOSAScriptIsModified:
			if (!langexternalvaltotable (vscript, &hcontext, hnode))
				return (errOSABadSelector);
			
			(**hcontext).fldirty = value != 0;
			
			break;
		
		default:
			return (errOSABadSelector);
		}
	
	return (noErr);
	} /*osaSetScriptInfo*/


static pascal OSAError osaGetScriptInfo (
			hdlcomponentglobals	hglobals,
			OSAID				scriptID,
			OSType				selector,
			long*				result) {
	
	/*
	2.1b4 dmb: added code for kOSACanGetSource and kASHasOpenHandler selectors
	*/
	
	register hdlcomponentglobals hcg = hglobals;
	tyvaluerecord vscript;
	tyvaluerecord vcode;
	hdlhashtable hcontext;
	hdlhashnode hnode;
	
	*result = 0;
	
	if (scriptID == kOSANullScript)
		return (errOSAInvalidID);
	
	if (!getstorageval (hcg, scriptID, &vscript, &hnode))
		return (errOSAInvalidID);
	
	if (!langexternalvaltotable (vscript, &hcontext, hnode))
		hcontext = nil;
	
	switch (selector) {
		
		case kOSAScriptIsModified:
			if (hcontext == nil)
				return (errOSABadSelector);
			
			*result = (**hcontext).fldirty;
			
			break;
		
		case kOSAScriptIsBeingEdited:
			break;
		
		case kOSAScriptIsBeingRecorded:
			*result = (**hcg).recordingstate.flrecording;
			
			break;
		
		case kOSAScriptIsTypeCompiledScript:
			break;
		
		case kOSAScriptIsTypeScriptValue:
			*result = hcontext == nil;
			
			break;
		
		case kOSAScriptIsTypeScriptContext:
			*result = hcontext != nil;
			
			break;
		
		case kOSAScriptBestType:
			*result = typeChar;
			
			break;
		
		case kOSACanGetSource:
			if (hcontext == nil)
				return (errOSABadSelector);
			
			*result = hashtablesymbolexists (hcontext, bssource);
			
			break;
		
		case kASHasOpenHandler:
			if (hcontext == nil)
				return (errOSABadSelector);
			
			if (!hashtablelookup (hcontext, bscode, &vcode, &hnode))
				break;
			
			if (!osacreatemodulecontext (vcode.data.codevalue, &hcontext))
				return (false);
			
			*result = hashtablesymbolexists (hcontext, "\podoc") || hashtablesymbolexists (hcontext, "\popen");
			
			disposehashtable (hcontext, true);
			
			break;
		
		default:
			return (errOSABadSelector);
		}
	
	return (noErr);
	} /*osaGetScriptInfo*/


static pascal OSAError osaSetResumeDispatchProc (
				hdlcomponentglobals	hglobals,
				AEEventHandlerUPP	resumeDispatchProc,
				long				refCon) {
	
	/*
	2.1b4 dmb: we're not currently using the dispatch proc. UserTalk doesn't 
	have an equivalent to AS's "continue" statement, but a script can do 
	a scripterror (-1708) to get exectueevent to return that error, in which 
	case the client should invoke its own dispatch proc
	
	in any case, we're keeping track of what the caller sets up, just for 
	the hell of it.
	*/
	
	register hdlcomponentglobals hcg = hglobals;
	
	(**hcg).resumedispatchproc = resumeDispatchProc;
	
	(**hcg).resumedispatchprocrefcon = refCon;
	
	return (noErr);
	} /*osaSetResumeDispatchProc*/


static pascal OSAError
osaGetResumeDispatchProc (
		hdlcomponentglobals	 hglobals,
		AEEventHandlerUPP	*resumeDispatchProc,
		long				*refCon)
{
#pragma unused (hglobals)
	/*
	we're never invoking the resumedispatch proc, so be honest and 
	return these constants
	*/
	
	*resumeDispatchProc = (AEEventHandlerUPP) kOSANoDispatch;
	
	*refCon = 0;
	
	return (noErr);
	} /*osaGetResumeDispatchProc*/


pascal OSAError osaSetActiveProc (
			hdlcomponentglobals	hglobals,
			OSAActiveUPP		activeProc,
			long				refCon) {
	
	register hdlcomponentglobals hcg = hglobals;
	
	(**hcg).activeproc = activeProc;
	
	(**hcg).activeprocrefcon = refCon;
	
	return (noErr);
	} /*osaSetActiveProc*/


pascal OSAError osaSetSendProc (
			hdlcomponentglobals	hglobals,
			OSASendUPP			sendProc,
			long				refCon) {
	
	register hdlcomponentglobals hcg = hglobals;
	
	if (sendProc == nil)
		sendProc = osadefaultsendUPP;
	
	(**hcg).sendproc = sendProc;
	
	(**hcg).sendprocrefcon = refCon;
	
	return (noErr);
	} /*osaSetSendProc*/


static pascal OSAError osaGetSendProc (
			hdlcomponentglobals	hglobals,
			OSASendUPP*			sendProc,
			long*				refCon) {
	
	register hdlcomponentglobals hcg = hglobals;
	
	*sendProc = (**hcg).sendproc;
	
	*refCon = (**hcg).sendprocrefcon;
	
	return (noErr);
	} /*osaGetSendProc*/


static pascal OSAError osaSetCreateProc (
			hdlcomponentglobals			hglobals,
			OSACreateAppleEventUPP		createProc,
			long						refCon) {
	
	register hdlcomponentglobals hcg = hglobals;
	
	if (createProc == nil)
		createProc = osadefaultcreateUPP;
	
	(**hcg).createproc = createProc;
	
	(**hcg).createprocrefcon = refCon;
	
	return (noErr);
	} /*osaSetCreateProc*/


static pascal OSAError osaGetCreateProc (
			hdlcomponentglobals			hglobals,
			OSACreateAppleEventUPP*		createProc,
			long*						refCon) {
	
	register hdlcomponentglobals hcg = hglobals;
	
	*createProc = (**hcg).createproc;
	
	*refCon = (**hcg).createprocrefcon;
	
	return (noErr);
	} /*osaGetCreateProc*/




static pascal OSAError osacompiledesc (
			const AEDesc*		sourceData,
			tyvaluerecord*		vcode) {
	
	Handle htext;
	hdltreenode hcode;
	
	if ((*sourceData).descriptorType != typeChar)
		return (errAECoercionFail);
	
	/*PBS 03/14/02: AE OS X fix.*/
	
  if (!copydatahandle ((AEDesc*) sourceData, &htext)) /*don't let langbuildtree consume caller's text*/
    return (memFullErr);
	
	if (!langbuildtree (htext, false, &hcode)) /*syntax error*/
		return (osageterror ());
	
	initvalue (vcode, codevaluetype);
	
	(*vcode).data.codevalue = hcode;
	
	return (noErr);
	} /*osacompiledesc*/


static pascal OSAError osaCompile (
			hdlcomponentglobals	hglobals,
			const AEDesc*		sourceData,
			long				modeFlags,
			OSAID*				scriptID) {
	
	/*
	first cut: handle contexts, but not augmenting contexts.
	*/
	
	OSAError err = noErr;
	hdlhashtable hcontext;
	tyvaluerecord vcode, vcontext, vsource;
	OSAID id = *scriptID;
	Handle htext;
	boolean flkeepsource = false;
	hdlhashnode hnode;
	
	err = osacompiledesc (sourceData, &vcode);
	
	if (err != noErr)
		return (err);
	
	if ((modeFlags & kOSAModePreventGetSource) == 0) { /*bit not set*/
		
		flkeepsource = true;
		
		/*PBS 03/14/02: AE OS X fix.*/
    
    if (!copydatahandle ((AEDesc*) sourceData, &htext)) /*make copy for eventual getsource*/
      return (memFullErr);
		
		if (!setheapvalue (htext, stringvaluetype, &vsource))
			return (osageterror ());
		}
	
	if (id == kOSANullScript) {
		
		if (!osanewvalue (idtableprocessor, nil, &vcontext))
			return (memFullErr);
		
		langexternalvaltotable (vcontext, &hcontext, HNoNode);
		}
	else {
		
		if (!getstorageval (hglobals, id, &vcontext, &hnode))
			return (osageterror ());
		
		if (!langexternalvaltotable (vcontext, &hcontext, hnode))
			return (errOSAInvalidID);
		}
	
	pushhashtable (hcontext);
	
	hashassign (bscode, vcode);
	
	if (flkeepsource)
		flkeepsource = hashassign (bssource, vsource);
	
	pophashtable ();
	
	if (flkeepsource)
		exemptfromtmpstack (&vsource);
	
	if (id == kOSANullScript) {
		
		if (!addstorageval (hglobals, &vcontext, scriptID))
			return (osageterror ());
		}
	
	return (noErr);
	} /*osaCompile*/


static pascal OSAError osaGetSource (
			hdlcomponentglobals	hglobals,
			OSAID				scriptID,
			DescType			desiredType,
			AEDesc*				resultingSourceData) {
	
	tyvaluerecord val;
	hdlhashtable hcontext;
	hdlhashnode hnode;
	
	if (scriptID == kOSANullScript)
		return (errOSAInvalidID);
	
	if (!osagetcontext (hglobals, scriptID, &hcontext))
		goto error;
	
	if (!hashtablelookup (hcontext, bssource, &val, &hnode))
		return (errOSASourceNotAvailable);
	
	if (val.valuetype == novaluetype)
		return (errOSASourceNotAvailable);
	
	if (!storagevaltodesc (&val, desiredType, resultingSourceData))
		goto error;
	
	return (noErr);
	
	error:
		return (osageterror ());
	
	} /*osaGetSource*/


static pascal OSAError
osaCoerceFromDesc (
		hdlcomponentglobals	 hglobals,
		const AEDesc		*scriptData,
		long				 modeFlags,
		OSAID				*resultingScriptID)
{
#pragma unused (modeFlags)
	
	/*
	3.0a dmb: fixed leak when val is an externalvaluetype.
	*/
	
	AEDesc desc;
	tyvaluerecord val;
	register boolean fl;
	
	if (oserror (AEDuplicateDesc (scriptData, &desc)))
		goto error;
	
	if (langgetvaluetype (desc.descriptorType) >= outlinevaluetype) {
		
		val.valuetype = externalvaluetype;
		
		/*PBS 03/14/02: AE OS X fix.*/
    {
    Handle h;
    
    copydatahandle (&desc, &h);
    
    fl = langexternalmemoryunpack (h, (hdlexternalhandle *) &val.data.externalvalue);
    
    disposehandle (h);
    }
		
		AEDisposeDesc (&desc);
		
		if (!fl)
			goto error;
		}
	else {
		
		if (!setdescriptorvalue (desc, &val))
			goto error;
		}
	
	if (!addstorageval (hglobals, &val, resultingScriptID))
		goto error;
	
	return (noErr);
	
	error:
		return (osageterror ());
	} /*osaCoerceFromDesc*/


static pascal OSAError
osaCoerceToDesc (
		hdlcomponentglobals	 hglobals,
		OSAID				 scriptID,
		DescType			 desiredType,
		long				 modeFlags,
		AEDesc				*result)
{
#pragma unused (modeFlags)
	
	tyvaluerecord val;
	hdlhashnode hnode;
	
	if (scriptID == kOSANullScript)
		return (errOSAInvalidID);
	
	if (!getstorageval (hglobals, scriptID, &val, &hnode))
		goto error;
	
	if (!storagevaltodesc (&val, desiredType, result))
		goto error;
	
	return (noErr);
	
	error:
		return (osageterror ());
	
	} /*osaCoerceToDesc*/


static Handle getcomponentstringhandle (short id) {
	
	/*
	2.1b11 dmb: return a handle to the specified string in the component 
	string list
	*/
	
	bigstring bs;
	
	if (!getstringlist (componentlistnumber, id, bs))
		return (nil);
	
	return ((Handle) NewString (bs));
	} /*getcomponentstringhandle*/


static boolean getrecordingstring (short id, bigstring bs) {
	
	return (getstringlist (recordinglistnumber, id, bs));
	} /*getrecordingstring*/


static pascal OSErr
coerceInsltoTEXT (
		const AEDesc	*desc,
		DescType		 totype,
		long			 refcon,
		AEDesc			*result)
{
#pragma unused(totype, refcon)

	/*
	2.1b2 dmb: this is installed as a typeInsertionLoc -> typeObjectSpecifier 
	coercer, but it's actually generating source text for the recorder.  it's 
	not installed as a -> typeChar coercer to prevent getobjectmodeldisplaystring 
	from putting quotes around the result
	
	2.1b3 dmb: if position isn't one of the standard 5, call insertionLoc
	*/
	
	AEDesc rec;
	AEDesc obj;
	byte bsverb [16];
	byte bspos [64];
	bigstring bsobj;
	bigstring bs;
	tyvaluerecord val;
	short ix;
	OSErr err;
	DescType type;
	long size;
	OSType pos;
		
	#ifdef fldebug // 2006-04-04 - kw --- this was fldegug
	
	if ((*desc).descriptorType != typeInsertionLoc)
		DebugStr ("\punexpected coercion");
	
	#endif
	
	err = AECoerceDesc (desc, typeAERecord, &rec);
	
	if (err != noErr)
		goto exit;
	
	err = AEGetKeyPtr (&rec, keyAEPosition, typeEnumeration, &type, &pos, sizeof (pos), &size);
	
	if (err == noErr)
		err = AEGetKeyDesc (&rec, keyAEObject, typeWildCard, &obj);
	
	AEDisposeDesc (&rec);
	
	if (err != noErr)
		goto exit;
	
	setemptystring (bspos);
	
	switch (pos) {
		
		case kAEBefore:
			ix = beforestring; break;
		
		case kAEBeginning:
			ix = beginningofstring; break;
		
		case kAEAfter:
			ix = afterstring; break;
		
		case kAEEnd:
			ix = endofstring; break;
		
		case kAEReplace:
			ix = replacingstring; break;
		
		default:
			ix = insertionlocstring;
			
			setostypevalue (pos, &val);
			
			getobjectmodeldisplaystring (&val, bspos);
			
			pushstring ("\p, ", bspos);
			
			break;
		}
	
	getrecordingstring (ix, bsverb);
	
	setdescriptorvalue (obj, &val);
	
	getobjectmodeldisplaystring (&val, bsobj);
	
	disposevaluerecord (val, true);
	
	if (isemptystring (bsobj))
		copystring ("\p\"\"", bsobj);
	
	parsedialogstring ("\p^0 (^1^2)", bsverb, bspos, bsobj, nil, bs);
	
	err = AECreateDesc (typeChar, bs + 1, stringlength (bs), result);
	
exit:
	
	return (err);
	} /*coerceInsltoTEXT*/


//Code change by Timothy Paustian Friday, June 16, 2000 1:38:13 PM
//Changed to Opaque call for Carbon
AECoercionHandlerUPP	coerceInsltoTEXTDesc;

#define coerceInsltoTEXTUPP (coerceInsltoTEXTDesc);

static pascal OSErr
sendrecordingevent (
		hdlcomponentglobals		 hglobals,
		AEEventID				 id)
{
#pragma unused (hglobals)
	
	/*
	2.1b5 dmb: don't use sendproc for these events; they're not part of 
	script execution
	*/
	
	AEDesc desc;
	OSErr err;
	ProcessSerialNumber psn;
	AppleEvent event, reply;
	
	GetCurrentProcess (&psn);
	
	err = AECreateDesc (typeProcessSerialNumber, (Ptr) &psn, sizeof (psn), &desc);
	
	if (err == noErr) {
		
		err = AECreateAppleEvent (kCoreEventClass, id, &desc, kAutoGenerateReturnID, kAnyTransactionID, &event);
		
		AEDisposeDesc (&desc);
		
		if (err == noErr) {
			
			err = AESend (&event, &reply, 
				
				(AESendMode) kAEDontRecord + kAECanSwitchLayer + kAECanInteract + kAENoReply, 
				
				(AESendPriority) kAENormalPriority, (long) kAEDefaultTimeout, nil, nil);
			
			AEDisposeDesc (&event);
			
			AEDisposeDesc (&reply);
			}
		}
	
	return (err);
	} /*sendrecordingevent*/


static pascal OSErr sendrecordedtextevent (hdlcomponentglobals hcg, bigstring bs) {
	
	/* 
	2.1b5 dmb: maintain our own version of the recorded text
	
	2.1b8 dmb: set up currentA5 and client context for AESend call.
	it will be short-circuited to client app, so it's like a callback.
	*/
	
	AppleEvent event, reply;
	AEDesc desc;
	ProcessSerialNumber psn;
	OSErr err;
	Handle htext;
	bigstring bssend;
	
	copystring (bs, bssend);
	
	pushchar (chreturn, bssend);
	
	htext = (**hcg).recordingstate.hrecordedtext;
	
	if (htext != nil)
		pushtexthandle (bssend, htext); /*if this fails, so will subsequent calls*/
	
	psn.highLongOfPSN = 0;
	
	psn.lowLongOfPSN = kCurrentProcess;
	
	err = AECreateDesc (typeProcessSerialNumber, (Ptr) &psn, sizeof (psn), &desc);
	
	if (err == noErr) {
		
		err = AECreateAppleEvent (kOSASuite, kOSARecordedText, &desc, kAutoGenerateReturnID, kAnyTransactionID, &event);
		
		AEDisposeDesc (&desc);
		
		if (err == noErr) {
			
			err = AEPutParamPtr (&event, keyDirectObject, typeChar, (Ptr) bssend + 1, stringlength (bssend));
			
			osapreclientcallback (hcg);
			
			err = AESend (&event, &reply, 
				
				(AESendMode) kAENoReply + kAEDontRecord, 
				
				(AESendPriority) kAENormalPriority, (long) kNoTimeOut, nil, nil);
					
			osapostclientcallback (hcg);
			
			AEDisposeDesc (&event);
			
			AEDisposeDesc (&reply);
			}
		}
	
	return (err);
	} /*sendrecordedtextevent*/


static pascal OSErr pusheventparameter (const AppleEvent *event, AEKeyword key, boolean flpushkey, bigstring bsparam, bigstring bsevent) {
	
	AEDesc desc;
	tyvaluerecord val;
	bigstring bsval;
	byte bskey [6];
	byte bscoerce [16];
	OSErr err;
	
	err = AEGetParamDesc (event, key, typeWildCard, &desc);
	
	if (err != noErr)
		return (err);
	
	if (!setdescriptorvalue (desc, &val))
		return (getoserror ());
	
	getobjectmodeldisplaystring (&val, bsval);
	
	switch (val.valuetype) { /*see if coercion is needed*/
		
		case filespecvaluetype:
		case aliasvaluetype:
			langgettypestring (val.valuetype, bscoerce);
			
			pushstring ("\p (", bscoerce);
			
			insertstring (bscoerce, bsval);
			
			pushchar (')', bsval);
			
			break;
		
		default:
			break;
		}
	
	disposevaluerecord (val, false);
	
	if (flpushkey) {
		
		pushstring ("\p, '", bsevent);
		
		ostypetostring (key, bskey);
		
		pushstring (bskey, bsevent);
		
		pushchar ('\'', bsevent);
		}
	
	if (bsevent [stringlength (bsevent)] != '(') /*not 1st item in param list*/
		pushstring ("\p, ", bsevent);
	
	if (!isemptystring (bsparam)) {
		
		pushstring (bsparam, bsevent);
		
		pushstring ("\p: ", bsevent);
		}
	
	pushstring (bsval, bsevent);
	
	return (noErr);
} /*pusheventparameter*/


static pascal OSErr
handlerecordableevent (
		const AppleEvent	*event,
		AppleEvent			*reply,
		SInt32				 refcon)
{
#pragma unused (reply)

	/*
	map the event to a line of source code, and send the text in a Recorded Text event
	
	2.1b2: added bringToFront calls
	
	2.1b3: include braces & semicolons; script.c will strip them out
	
	2.1b4: added ugly special case for comment events. if there's no glue for an 
	event, use appleevent verb. allocate 8 bytes for ostype strings to leave room 
	for single quotes and length.
	
	2.1b5: do everything in our heap. finding the app table is especially important; 
	we don't want our tables loaded in another heap. the same probably applies to any 
	resource strings that might be used, though we can probably preload what we need 
	if we want to keep the clients heap set for same reason.
	
	2.1b7: ignore errors from sendrecordedtextevent. we want to continue 
	accumulating our version of the text no matter what.
	
	3.0b15 dmb: call SetResLoad (false) before opening recorded app's resource 
	fork, or all of its preload resources will be loaded into our heap
	
	3.0a dmb: if no app table is found, add comment to that effect to "with" 
	statement.
	*/
	
	register hdlcomponentglobals hcg = (hdlcomponentglobals) refcon;
	AEDesc desc;
	OSErr err;
	AEEventClass class;
	AEEventID id;
	DescType type;
	long size;
	boolean flgotname;
	long paramoffset;
	bigstring bs;
	bigstring bsname;
	bigstring bsparam;
	byte bssignature [8];
	byte bsclass [8];
	byte bsid [8];
	hdlhashtable happtable = nil;
	FSSpec fs;
	OSType signature;
	ProcessSerialNumber psn;
	boolean flisfront;
	boolean flpushkeys;
	boolean flignore;
	short rnum = 0;
	Handle haete = nil;
	
	
	err = landsystem7getsenderinfo (event, &psn, &fs, &signature);
	
	if (err != noErr) {
		
		return (err);
		}
	
	osapushfastcontext (hcg);

  coerceInsltoTEXTDesc = NewAECoerceDescUPP(coerceInsltoTEXT);
  AEInstallCoercionHandler (typeInsertionLoc, typeObjectSpecifier, coerceInsltoTEXTDesc, 0, true, false);
	
	if (signature == (**hcg).recordingstate.lastappid)
		happtable = (**hcg).recordingstate.lastapptable;
	
	else {
		
		if ((**hcg).recordingstate.lastappid != 0) {
			
			sendrecordedtextevent (hcg, "\p\t};");
			
			/*
			if (err != noErr)
				goto exit;
			*/
			}
		
		flisfront = isfrontapplication (psn);
		
		getrecordingstring (withobjectmodelstring, bs);
		
		if (langipcfindapptable (signature, false, &happtable, bsname)) {
			
			pushstring ("\p, ", bs);
			
			pushstring (bsname, bs);
			
			pushstring ("\p {", bs);
			
			sendrecordedtextevent (hcg, bs);
			
			/*
			if (err != noErr)
				goto exit;
			*/
			
			if (flisfront) {
				
				getrecordingstring (bringtofrontstring, bs);
				
				sendrecordedtextevent (hcg, bs);
				
				/*
				if (err != noErr)
					goto exit;
				*/
				}
			}
		else {
			
			pushstring ("\p { ", bs);
			
			getrecordingstring (noverbtablestring, bsparam); /*3.0a*/
			
			getprocessname (psn, bsname, &flignore);
			
			parsedialogstring (bsparam, bsname, nil, nil, nil, bsparam);
			
			pushstring (bsparam, bs);
			
			sendrecordedtextevent (hcg, bs);
			
			/*
			if (err != noErr)
				goto exit;
			*/
			
			if (flisfront) {
				
				ostypetostring (signature, bssignature);
				
				getrecordingstring (sysbringapptofrontstring, bs);
				
				parsedialogstring (bs, bssignature, nil, nil, nil, bs);
				
				sendrecordedtextevent (hcg, bs);
				}
			}
		
		(**hcg).recordingstate.lastappid = signature;
		
		(**hcg).recordingstate.lastapptable = happtable;
		}
	
	err = AEGetAttributePtr (event, keyEventClassAttr, typeType, &type, &class, sizeof (class), &size);
	
	if (err == noErr)
		err = AEGetAttributePtr (event, keyEventIDAttr, typeType, &type, &id, sizeof (id), &size);
	
	if (err != noErr)
		goto exit;
	
	setemptystring (bsparam);
	
	flgotname = false;
	
	if ((class == kASAppleScriptSuite) && (id == kASCommentEvent)) { /*special case*/
		
		setemptystring (bs);
		
		err = AEGetParamDesc (event, keyDirectObject, typeChar, &desc);
		
		if (err == noErr) {
			
			/*PBS 03/14/02: AE OS X fix.*/
      datahandletostring (&desc, bs);
			
