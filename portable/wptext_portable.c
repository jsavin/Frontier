#include "../Common/headers/frontier.h"

#ifndef UNIX_COMPILE
#define UNIX_COMPILE 1
#endif
#ifndef C_LIBRARY
#define C_LIBRARY 1
#endif
#ifndef NO_OS_INLINE
#define NO_OS_INLINE 1
#endif

#include "wptext_portable.h"

#define Rect PaigeRect
#define Point PaigePoint
#define RgnHandle PaigeRgnHandle
#include "PAIGE.H"
#include "PGMEMMGR.H"
#include "PGTRAPS.H"
#undef Rect
#undef Point
#undef RgnHandle

static pgm_globals g_wp_mem_globals;
static pg_globals g_wp_globals;
static Boolean g_wp_initialized = false;

Boolean wp_portable_init(void) {
    if (g_wp_initialized)
        return true;

    pgFillBlock(&g_wp_mem_globals, sizeof(g_wp_mem_globals), 0);
    pgFillBlock(&g_wp_globals, sizeof(g_wp_globals), 0);

    pgMemStartup(&g_wp_mem_globals, 0);
    pgInit(&g_wp_globals, &g_wp_mem_globals);

    g_wp_initialized = true;
    return true;
}

void wp_portable_shutdown(void) {
    if (!g_wp_initialized)
        return;

    pgShutdown(&g_wp_globals);
    pgMemShutdown(&g_wp_mem_globals);
    g_wp_initialized = false;
}
