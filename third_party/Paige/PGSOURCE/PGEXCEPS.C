/* Paige exception handling functions.

Original source files created by Tom Shaw, OITC. Modified by Gar for DataPak
Software, Inc. to be more consistent with Paige syntax and parameter naming
conventions.   Copyright 1993-1994    			*/

/* July 19, 1994 Changed to add support calls to aid in interfacing Paige and Paige's
	failure mechanisms with C++, SOM and other languages failure mechanisms by Tom Shaw, OITC  */

#include "pgMemMgr.h"

#ifdef MAC_PLATFORM
#pragma segment pgbasic4
#endif

#include "pgExceps.h"
#include "pgErrors.h"

#ifdef MAC_PLATFORM
#include <Segload.h>
#include <OSUtils.h>
#include <Script.h>
#endif

#ifdef PG_DEBUG

#ifdef SOURCE_LEVEL_DEBUG
#define DEBUG_MESSAGE(ref, str) pgSourceDebugBreak(ref, (char *)str)
#else
#define DEBUG_MESSAGE(unused, str) DebugStr(str)
#endif
#endif

static void no_handler (pgm_globals_ptr globals);


/* pgPushHandler gets invoked by PG_TRY macro to set up for catching failures. */

PG_PASCAL (void) pgPushHandler (pgm_globals_ptr globals, pg_fail_info_ptr fail_info)
{
	fail_info->next = globals->top_fail_info;
	globals->top_fail_info = fail_info;
	globals->last_error = NO_ERROR;
	globals->last_ref = NULL;
}


/* pgFailure is called once an error is detected. */

PG_PASCAL (void) pgFailure (pgm_globals_ptr globals, pg_error error, long message)
{
	pg_fail_info_ptr	handler;

#ifdef PG_DEBUG
	globals->debug_check = 0;
#endif

	if (globals->top_fail_info) {

		globals->last_error = error;
		globals->last_message = message;
		handler = globals->top_fail_info;
		globals->top_fail_info = handler->next;
		longjmp(handler->regs, error);
	}
	else {

#ifdef SOURCE_LEVEL_DEBUG
#ifdef WINDOWS_PLATFORM
		DEBUG_MESSAGE(MEM_NULL, " PG_CATCH Exception Raised");
#else
		DEBUG_MESSAGE(MEM_NULL, "\pPG_CATCH Exception Raised");
#endif
#else
#endif
		no_handler(globals);
	}
}


/* pgSuccess is called to "pop" the current fail info record (pushed by
pgPushTryHandler).  */

PG_PASCAL (void) pgSuccess (pgm_globals_ptr globals)
{
	if (globals->top_fail_info)
		globals->top_fail_info = globals->top_fail_info->next;
	else no_handler(globals);
}



/* pgSetFailInfo places new info into the top handler. */

PG_PASCAL (void) pgSetFailInfo( pgm_globals_ptr globals, pg_error new_error,
		long new_msg, void PG_FAR *new_ref)
{
	globals->last_error = new_error;
	
	if (globals->last_message == 0)
		globals->last_message = new_msg;
	if (globals->last_ref == NULL)
		globals->last_ref = new_ref;
}


/* pgFailNotError compares acceptable_error to actual_error and, if they do not
match pgFailure is invoked, otherwise nothing happens.  */

PG_PASCAL (void) pgFailNotError (pgm_globals_ptr globals, pg_error acceptable_error,
		pg_error actual_error)
{
	if (actual_error != NO_ERROR)
		if (actual_error != acceptable_error)
			pgFailure(globals, actual_error, 0);
}


#ifdef NO_C_INLINE

/* pgSetFailureHandler sets the failure handler proc in globals */

PG_PASCAL (void) pgSetFailureHandler( pgm_globals_ptr globals, pg_error_handler handler)
{
	globals->last_handler = handler;
}


/* pgFailZero is called to check for a general zero following an OS allocation,
and if MEM_NULL the failure mechanism is called, otherwise nothing happens. */

PG_PASCAL (void) pgFailZero (pgm_globals_ptr globals, generic_var value)
{
	if (!value)
		pgFailure(globals, NO_MEMORY_ERR, 0);
}


/* pgFailError invokes the error mechanism if error != 0. Otherwise it does
nothing.  */

PG_PASCAL (void) pgFailError (pgm_globals_ptr globals, pg_error error)
{
	if (error != NO_ERROR)
		pgFailure(globals, error, 0);
}


/* pgFailNIL is called to check for a NULL pointer following a memory allocation,
and if NULL the failure mechanism is called, otherwise nothing happens. */

PG_PASCAL (void) pgFailNIL (pgm_globals_ptr globals, void PG_FAR *allocation)
{
	if (!allocation)
		pgFailure(globals, NO_MEMORY_ERR, 0);
}

/* pgFailBoolean calls pgFailure if b is TRUE */

PG_PASCAL (void) pgFailBoolean( pgm_globals_ptr globals, pg_boolean b)
{
	if (b) pgFailure( globals, BOOLEAN_EXCEPTION, 0);

}

/* pgGetError returns the last error */

PG_PASCAL (pg_error) pgGetError( pgm_globals_ptr globals )
{
	return globals->last_error;
}

/* pgGetMessage returns the last message */

PG_PASCAL (long) pgGetMessage( pgm_globals_ptr globals )
{
	return globals->last_message;
}

/* pgGetReference returns the last reference */

PG_PASCAL (void PG_FAR *) pgGetReference( pgm_globals_ptr globals )
{
	return globals->last_ref;
}


/* pgSetReference sets the last reference */

PG_PASCAL (void) pgSetReference( pgm_globals_ptr globals, void PG_FAR * last_ref)
{
	globals->last_ref = last_ref;
}


/* pgReRaise reraises and propogates the error.  */

PG_PASCAL (void) pgReRaise (pgm_globals_ptr globals)
{
	pgFailure(globals, pgGetError(globals), pgGetMessage(globals));
}


#endif


/***************************** MACHINE SPECIFIC CODE ***************************/

/* pgProcessError translates the platform-specific file error into a Paige error code */

PG_PASCAL (pg_error) pgProcessError (pg_error error)
{
#ifdef WINDOWS_PLATFORM
	
	if (error)
		return	error;
	
	return	NO_ERROR;

#else

	switch (error) {
		
		case noErr:
			return	NO_ERROR;
			
	    case dirFulErr:
	    case dskFulErr:
	    	return NO_SPACE_ERR;

	    case ioErr:
			return	IO_ERR;

	    case fnOpnErr:
	    	return	NOT_OPEN_ERR;

	    case eofErr:
	    	return	EOF_ERR;

	    case posErr:
	    	return	EOF_ERR;

	    case wPrErr:
	    	return	WRITE_PROTECT_ERR;
	    	
	    case fLckdErr:
			return	FILE_LOCK_ERR;

		case permErr:
		case wrPermErr:
			return	ACCESS_DENIED_ERR;

	}

	return	IO_ERR;

#endif
}


/* Default debug proc (gets called if anything fails) */
PG_PASCAL (void) pgDebugProc (pg_error message, unsigned long what_ref)
{

#ifdef MAC_PLATFORM
#ifndef SOURCE_LEVEL_DEBUG
#pragma unused (message)
#endif
#endif

#ifdef MAC_PLATFORM
	SysBeep(1);
	KeyScript(0);
#endif

#ifdef PG_DEBUG

#ifdef MAC_PLATFORM

	switch (message) {

		case NO_MEMORY_ERR:
		case NOT_ENOUGH_PURGED_ERR:
			DEBUG_MESSAGE(what_ref, "\pOut of memory");
			break;

		case NO_PURGE_FILE_ERR:
			DEBUG_MESSAGE(what_ref, "\pPurge file not open");
			break;

		case LOCKED_BLOCK_ERR:
			DEBUG_MESSAGE(what_ref, "\pAttempt to re-size locked memory");
			break;

		case NIL_ADDRESS_ERR:
			DEBUG_MESSAGE(what_ref, "\pNIL memory_ref");
			break;

		case BAD_ADDRESS_ERR:
			DEBUG_MESSAGE(what_ref, "\pBogus memory_ref address");
			break;

		case BAD_LINK_ERR:
			DEBUG_MESSAGE(what_ref, "\pInternal damage in memory_ref");
			break;

		case CHECKSUM_ERR:
			DEBUG_MESSAGE(what_ref, "\pOverwrite error");
			break;

		case ACCESS_ERR:
			DEBUG_MESSAGE(what_ref, "\pAccess counter invalid for operation");
			break;

		case BAD_REF_ERR:
			DEBUG_MESSAGE(what_ref, "\pBogus memory_ref");
			break;

		case REF_DISPOSED_ERR:
			DEBUG_MESSAGE(what_ref, "\pOperation on disposed memory_ref");
			break;

		case FILE_PURGE_ERR:
			DEBUG_MESSAGE(what_ref, "\pError in purging");
			break;

		case FILE_UNPURGE_ERR:
			DEBUG_MESSAGE(what_ref, "\pError in un-purging");
			break;

		case RANGE_ERR:
			DEBUG_MESSAGE(what_ref, "\pAttempt to access record out of range");
			break;
		
		case PURGED_MEMORY_ERR:
			DEBUG_MESSAGE(what_ref, "\pAttempt to operate on a purged block");
			break;

		case DEBUG_ZERO_ERR:
			DEBUG_MESSAGE(what_ref, "\pUser break, access == 0");
			break;

		case DEBUG_NZ_ERR:
			DEBUG_MESSAGE(what_ref, "\pUser break, access != 0");
			break;

		case NO_ERR_HANDLER_ERR:
			DEBUG_MESSAGE(what_ref, "\pNo exception handler");
			ExitToShell();
			break;

		case STRUCT_INTEGRITY_ERR:
			DEBUG_MESSAGE(what_ref, "\pStructure integrity failed");
			break;
		
		case USER_BREAK_ERR:
			DEBUG_MESSAGE(what_ref, "\pUser break on debug_check");
			break;
		
		case PROGRAM_BUG_ASSERT:
			DEBUG_MESSAGE(what_ref, "\pProgram assertion break");
			break;

		default:
			DEBUG_MESSAGE(what_ref, "\pUnknown error");
			break;
	}
#else

	switch (message) {

		case NO_MEMORY_ERR:
		case NOT_ENOUGH_PURGED_ERR:
			DEBUG_MESSAGE(what_ref, " Out of memory");
			break;

		case NO_PURGE_FILE_ERR:
			DEBUG_MESSAGE(what_ref, " Purge file not open");
			break;

		case LOCKED_BLOCK_ERR:
			DEBUG_MESSAGE(what_ref, " Attempt to re-size locked memory");
			break;

		case NIL_ADDRESS_ERR:
			DEBUG_MESSAGE(what_ref, " NIL memory_ref");
			break;

		case BAD_ADDRESS_ERR:
			DEBUG_MESSAGE(what_ref, " Bogus memory_ref address");
			break;

		case BAD_LINK_ERR:
			DEBUG_MESSAGE(what_ref, " Internal damage in memory_ref");
			break;

		case CHECKSUM_ERR:
			DEBUG_MESSAGE(what_ref, " Overwrite error");
			break;

		case ACCESS_ERR:
			DEBUG_MESSAGE(what_ref, " Access counter invalid for operation");
			break;

		case BAD_REF_ERR:
			DEBUG_MESSAGE(what_ref, " Bogus memory_ref");
			break;

		case REF_DISPOSED_ERR:
			DEBUG_MESSAGE(what_ref, " Operation on disposed memory_ref");
			break;

		case FILE_PURGE_ERR:
			DEBUG_MESSAGE(what_ref, " Error in purging");
			break;

		case FILE_UNPURGE_ERR:
			DEBUG_MESSAGE(what_ref, " Error in un-purging");
			break;

		case RANGE_ERR:
			DEBUG_MESSAGE(what_ref, " Attempt to access record out of range");
			break;
		
		case PURGED_MEMORY_ERR:
			DEBUG_MESSAGE(what_ref, " Attempt to operate on a purged block");
			break;

		case DEBUG_ZERO_ERR:
			DEBUG_MESSAGE(what_ref, " User break, access == 0");
			break;

		case DEBUG_NZ_ERR:
			DEBUG_MESSAGE(what_ref, " User break, access != 0");
			break;

		case NO_ERR_HANDLER_ERR:
			DEBUG_MESSAGE(what_ref, " No exception handler");
			ExitToShell();
			break;

		case STRUCT_INTEGRITY_ERR:
			DEBUG_MESSAGE(what_ref, " Structure integrity failed");
			break;

		case USER_BREAK_ERR:
			DEBUG_MESSAGE(what_ref, " User break on debug_check");
			break;

		case PROGRAM_BUG_ASSERT:
			DEBUG_MESSAGE(what_ref, " Program assertion break");
			break;

		default:
			DEBUG_MESSAGE(what_ref, " Unknown error");
			break;
	}
	
#endif

#ifdef SOURCE_LEVEL_DEBUG
	return;
#endif

#ifdef CONTINUE_AFTER_FAILURE
	return;
#endif

#endif
	ExitToShell();
}


/****************************** Local Functions **************************/


/* This gets called if there are no more handlers in the linked list. */

static void no_handler (pgm_globals_ptr globals)
{
	if (globals->last_handler)
		globals->last_handler(globals);
	else
		globals->debug_proc(NO_ERR_HANDLER_ERR, MEM_NULL);
}

