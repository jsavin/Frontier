// This file is used for source-level debugging with PG_DEBUG enabled.
#include "pgMemMgr.h"

/* pgSourceDebugBreak is used for PAIGE source code developers. Its purpose is to break into
the high-level debugger of the development system. When MemMgr detects an error, it jumps
to this function if you have defined SOURCE_LEVEL_DEBUG in CPUDEFS.H. */
#ifdef SOURCE_LEVEL_DEBUG

char pgSourceDebugBreak (memory_ref offending_ref, char *debug_str)
{
	mem_rec		PG_FAR		*bad_mem_rec;	// This gets coerced to examine it
	char		*examine;
#ifdef WINDOWS_PLATFORM
   char     msg[256];
   int      i;
#endif

/****************************** DEBUG BREAK, MEMORY ERROR!!!  ***********************************/

	examine = debug_str;			// <<<< PLACE DEBUGGER BREAK HERE!
	bad_mem_rec = (mem_rec PG_FAR *)pgMemoryPtr(offending_ref);
	pgFreePtr(offending_ref);
#ifdef WINDOWS_PLATFORM
   lstrcpy(msg, examine);
   lstrcat(msg, ";File=");
   lstrcat(msg, bad_mem_rec->file);
   lstrcat(msg, ";Line=");
   i = lstrlen(msg);
   _itoa(bad_mem_rec->line, &msg[i], 10);
   lstrcat(msg, "\n");
   pgTrace((pg_char_ptr)msg);
   __asm { int 3 };
#endif

/****************************** DEBUG BREAK, MEMORY ERROR!!!  ***********************************/

	return		examine[0];			// (Dummy code so you can stay in debug stepper)
}

#endif
