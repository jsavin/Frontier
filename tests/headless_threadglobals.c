/* ADR-005: Headless thread globals for parameter state macros */

#include "standard.h"
#include "processinternal.h"
#include "strings.h"

/*
 * Headless thread globals setup
 *
 * The hthreadglobals variable is declared as hdlthreadglobals (a double-pointer,
 * because it's a Handle). The macros in processinternal.h dereference it twice:
 *   #define flnextparamislast ((**hthreadglobals).flnextparamislast)
 *
 * To make this work in a static context, we need:
 *   1. A static structure to hold the actual data
 *   2. A static pointer to that structure
 *   3. hthreadglobals points to the pointer (making it a double-pointer)
 */
static tythreadglobals headless_threadglobals_data;
static tythreadglobals *headless_threadglobals_ptr = &headless_threadglobals_data;
hdlthreadglobals hthreadglobals = &headless_threadglobals_ptr;

/*
 * headless_init_threadglobals - Initialize critical fields
 *
 * Called during headless runtime initialization. Sets fields that must be
 * non-zero for correct operation. Matches subset of newthreadglobals() from
 * process.c but without Handle allocation.
 *
 * Critical fields:
 *   - flparamerrorenabled: Must be true (default state)
 *   - bsfunctionname: Must be empty string, not random memory
 *   - Other parameter state: Safe as zero-initialized booleans
 *
 * Note: htablestack and langcallbacks are NOT initialized here. They're
 * set during runtime initialization in langstartup.c and db_format.c.
 */
/*
 * Temporarily undefine the macros so we can initialize the actual struct fields
 */
#undef flnextparamislast
#undef flparamerrorenabled
#undef flcoerceexternaltostring
#undef flinhibitnilcoercion
#undef fllocaldotparamsonly
#undef bsfunctionname
#undef fllanghashassignprotect
#undef fllangexternalvalueprotect

void headless_init_threadglobals(void) {
	/* ADR-005: Parameter handling state initialization */
	headless_threadglobals_data.flnextparamislast = false;
	headless_threadglobals_data.flparamerrorenabled = true;    /* CRITICAL: must default to true */
	headless_threadglobals_data.flcoerceexternaltostring = false;
	headless_threadglobals_data.flinhibitnilcoercion = false;
	headless_threadglobals_data.fllocaldotparamsonly = false;
	setemptystring(headless_threadglobals_data.bsfunctionname);
	headless_threadglobals_data.fllanghashassignprotect = false;
	headless_threadglobals_data.fllangexternalvalueprotect = false;

	/* Other fields initialized by runtime (langstartup.c, db_format.c):
	 *   - htablestack: Set during lang initialization
	 *   - langcallbacks: Set during verb initialization
	 *   - current_working_directory: Set by file_working_dir system
	 */
}

/*
 * Restore the macros for the rest of the codebase
 */
#define flnextparamislast ((**hthreadglobals).flnextparamislast)
#define flparamerrorenabled ((**hthreadglobals).flparamerrorenabled)
#define flcoerceexternaltostring ((**hthreadglobals).flcoerceexternaltostring)
#define flinhibitnilcoercion ((**hthreadglobals).flinhibitnilcoercion)
#define fllocaldotparamsonly ((**hthreadglobals).fllocaldotparamsonly)
#define bsfunctionname ((**hthreadglobals).bsfunctionname)
#define fllanghashassignprotect ((**hthreadglobals).fllanghashassignprotect)
#define fllangexternalvalueprotect ((**hthreadglobals).fllangexternalvalueprotect)
