#include "Paige.h"

/* 2025-11-16 Codex: Provide no-op device close for headless UNIX builds. */

#if defined(FRONTIER_TESTS)
#include <stdio.h>
#endif

PG_PASCAL (void) pgCloseDevice (const pg_globals_ptr globals, const graf_device_ptr device)
{
	(void)globals;
	(void)device;
#if defined(FRONTIER_TESTS)
	fprintf(stdout, "[pg-device-stub] pgCloseDevice globals=%p device=%p\n",
		(void *)globals,
		(void *)device);
	fflush(stdout);
#endif
}
