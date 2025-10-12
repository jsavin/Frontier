/*
 * portable_handles.h - Cross-platform Handle compatibility layer
 *
 * These declarations back the classic Mac OS Handle APIs with a
 * modern, relocatable heap implementation so the Frontier core can
 * run headlessly on platforms that no longer provide Moveable
 * Handles.  The implementation lives in Common/source/portable_handles.c.
 */

#ifndef FRONTIER_PORTABLE_HANDLES_H
#define FRONTIER_PORTABLE_HANDLES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Supply the legacy Mac types when the SDK does not. */
#if !defined(__APPLE__) && !defined(_WIN32)
typedef unsigned char *Ptr;
typedef Ptr *Handle;
typedef int OSErr;
typedef unsigned char Boolean;

#ifndef noErr
#define noErr 0
#endif

#ifndef memFullErr
#define memFullErr (-108)
#endif
#else
#ifndef Ptr
typedef unsigned char *Ptr;
#endif
#ifndef Handle
typedef Ptr *Handle;
#endif
#ifndef OSErr
typedef int OSErr;
#endif
#endif /* non-Apple / non-Windows */

#ifndef noErr
#define noErr 0
#endif

#ifndef memFullErr
#define memFullErr (-108)
#endif

#ifndef CALLBACK
#define CALLBACK
#endif

Handle CALLBACK frontierAlloc(long userSize);
Handle CALLBACK frontierReAlloc(Handle h, long userSize);
void   CALLBACK frontierFree(Handle h);
long   CALLBACK frontierSize(Handle h);
char * CALLBACK frontierLock(Handle h);
void   CALLBACK frontierUnlock(Handle h);

Handle TempNewHandle(long userSize, OSErr *outErr);
void   TempDisposeHandle(Handle h);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FRONTIER_PORTABLE_HANDLES_H */
