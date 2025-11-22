#ifndef APPLEEVENT_PORTABLE_H
#define APPLEEVENT_PORTABLE_H

#if defined(__APPLE__) && !defined(FRONTIER_HEADLESS)
#include <Carbon/Carbon.h>
#else
#include "appleevent_portable_shim.h"
#endif

#endif /* APPLEEVENT_PORTABLE_H */
