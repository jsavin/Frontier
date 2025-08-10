/* classic_handle.h - Classic Mac Handle (unsigned char **) semantics for portable build */
#ifndef FRONTIER_CLASSIC_HANDLE_H
#define FRONTIER_CLASSIC_HANDLE_H

#include <stddef.h>

typedef unsigned char** Handle; /* deref yields raw byte pointer */

/* API mirroring legacy macros */
Handle ClassicNewHandle(size_t initial_size);
void ClassicDisposeHandle(Handle h);
void ClassicHLock(Handle h);
void ClassicHUnlock(Handle h);
size_t ClassicGetHandleSize(Handle h);
int ClassicSetHandleSize(Handle h, size_t new_size); /* returns non-zero on success */
Handle ClassicDupHandle(Handle h);

#endif


