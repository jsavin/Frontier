#ifndef PORTABLE_FRONTIER_H
#define PORTABLE_FRONTIER_H

#include "standard_portable.h"
#include "standard.h"
#include "classic_handle.h"

/* Portable shim for frontier.h includes */
/* Map legacy Handle macros to ClassicHandle API */

/* Handle type and macros */
#define NewHandle(size) ClassicNewHandle(size)
#define DisposeHandle(h) ClassicDisposeHandle(h)
#define HLock(h) ClassicHLock(h)
#define HUnlock(h) ClassicHUnlock(h)
#define GetHandleSize(h) ClassicGetHandleSize(h)
#define SetHandleSize(h, size) ClassicSetHandleSize(h, size)
#define DupHandle(h) ClassicDupHandle(h)

/* OSType already defined in standard_portable.h */

#endif /* PORTABLE_FRONTIER_H */


