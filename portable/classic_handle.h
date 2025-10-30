/* classic_handle.h - Classic Mac Handle (unsigned char **) semantics for portable build */
/* 2025-10-31 Codex: Added MemError/MaxBlock tracking API for portable builds. */
#ifndef FRONTIER_CLASSIC_HANDLE_H
#define FRONTIER_CLASSIC_HANDLE_H

#include <stddef.h>

#include "portable_handles.h"

/* Handle type is supplied by portable_handles.h */

/* API mirroring legacy macros */
Handle ClassicNewHandle(size_t initial_size);
void ClassicDisposeHandle(Handle h);
void ClassicHLock(Handle h);
void ClassicHUnlock(Handle h);
size_t ClassicGetHandleSize(Handle h);
int ClassicSetHandleSize(Handle h, size_t new_size); /* returns non-zero on success */
Handle ClassicDupHandle(Handle h);
OSErr ClassicMemError(void);
long ClassicMaxBlock(void);
void ClassicResetHeapTracking(void);

#endif
