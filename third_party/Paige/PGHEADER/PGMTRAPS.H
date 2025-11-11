#ifndef PGMTRAPS_H
#define PGMTRAPS_H


/*************** PAIGE machine-dependent memory definitions ******************/

/* This file defines all memory-related functions such as allocation, resizing,
etc. for memory_ref types. It is controlled by CPUDefs.h that defines the
platform.  */

/* File I/O has moved to pgIO.h - TRS OITC */


#include "CPUDefs.h"
#include "pgMemMgr.h"

#define DEF_MASTER_QTY		256

#ifdef MAC_PLATFORM

#include <Types.h>
#include <Files.h>
#include <Memory.h>

#define MASTER_ENTRY_SIZE	sizeof(memory_ref)
#define MASTER_ENTRY_NULL	MEM_NULL;
#define SPARE_TIRE_SIZE		0x00014000

#define pgMemoryPtr(ref) (mem_rec_ptr) *((Handle) ref)
#define pgFreePtr(ref) {}
#define pgAllocMemory(bytesize) (generic_var)NewHandle(bytesize)
#define pgAllocMemoryClear(bytesize) (generic_var)NewHandleClear(bytesize)
#define pgLockMemory(ref) HLock((Handle) ref)
#define pgLockMemoryHi(ref) HLockHi((Handle) ref)
#define pgUnlockMemory(ref) HUnlock((Handle) ref)
#define pgResizeMemory(globals, ref, bytesize) { \
		SetHandleSize((Handle) ref, bytesize);          \
		}
#define pgMemorySize(ref) GetHandleSize((Handle) ref)
#define pgFreeMemory(ref) DisposeHandle((Handle) ref)
#define pgMemoryError(globals) MemError()
#define pgRecoverMemory(ptr) (memory_ref)RecoverHandle((Ptr) ptr)

#endif

#ifdef WINDOWS_PLATFORM

#ifdef WINDOWS_EMULATE_COMPILE

#include "WinMac.h"

#else
#include <WINDOWS.H>
#endif

#include <string.h>


#define MASTER_ENTRY_SIZE	(sizeof(HANDLE))
#define MASTER_ENTRY_NULL	(HANDLE)NULL;
#define SPARE_TIRE_SIZE		0x00000400
#define MAX_HANDLE_ENTRIES	(DEF_MASTER_QTY - 1)

#define pgMemoryPtr(ref) (mem_rec_ptr) GlobalLock((HANDLE) *((HANDLE PG_FAR *)ref))
#define pgFreePtr(ref) GlobalUnlock(*((HANDLE PG_FAR *)ref))
#define pgAllocMemory(bytesize) GlobalAlloc(GMEM_MOVEABLE, bytesize)
#define pgAllocMemoryClear(bytesize) GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytesize)
#define pgLockMemory(ref) {}
#define pgLockMemoryHi(ref) {}
#define pgUnlockMemory(ref) {}
#define pgResizeMemory(globals, ref, bytesize) {  \
	if (*((HANDLE PG_FAR *)ref) = GlobalReAlloc((HANDLE) *((HANDLE PG_FAR *)ref), bytesize, 0)) \
		globals->last_error = NO_ERROR;	\
	else								\
		globals->last_error = NO_MEMORY_ERR; \
		}

#define pgMemorySize(ref) (long)GlobalSize(*((HANDLE PG_FAR *)ref))
#define pgFreeMemory(ref) GlobalFree(*((HANDLE PG_FAR *)ref))
#define pgMemoryError(globals) globals->last_error


#ifdef WIN32_COMPILE
#define BlockMove(src, dest, size) MoveMemory(dest, src, size)
#define pgRecoverMemory(ptr) ((HGLOBAL)GlobalHandle(ptr))
#else
#define pgRecoverMemory(ptr) ((HGLOBAL)LOWORD(GlobalHandle(SELECTOROF(ptr))))
#define BlockMove(src, dest, size) hmemcpy(dest, src, size)
#endif

/* Misc. */

#ifndef WINDOWS_EMULATE_COMPILE
#define Debugger() DebugBreak()
#define DebugStr OutputDebugString
#define ExitToShell()  PostQuitMessage(1)
#endif

#endif

#ifdef UNIX_PLATFORM

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct pg_unix_global_handle_struct {
    size_t size;
    unsigned char *raw;
} pg_unix_global_handle_struct, *pg_unix_global_handle;

static inline unsigned char *pg_unix_payload(pg_unix_global_handle handle) {
    return handle ? handle->raw + sizeof(pg_unix_global_handle) : NULL;
}

static inline pg_unix_global_handle pg_unix_alloc_handle(size_t size, int clear) {
    pg_unix_global_handle handle = (pg_unix_global_handle)malloc(sizeof(*handle));
    if (!handle)
        return NULL;
    size_t total = sizeof(pg_unix_global_handle) + size;
    unsigned char *raw = clear ? (unsigned char *)calloc(1, total)
                               : (unsigned char *)malloc(total);
    if (!raw) {
        free(handle);
        return NULL;
    }
    memcpy(raw, &handle, sizeof(pg_unix_global_handle));
    handle->size = size;
    handle->raw = raw;
    return handle;
}

static inline void pg_unix_free_handle(pg_unix_global_handle handle) {
    if (!handle)
        return;
    free(handle->raw);
    free(handle);
}

static inline unsigned char *pg_unix_lock(pg_unix_global_handle handle) {
    return pg_unix_payload(handle);
}

static inline void pg_unix_unlock(pg_unix_global_handle handle) {
    (void)handle;
}

static inline pg_unix_global_handle pg_unix_realloc_handle(pg_unix_global_handle handle, size_t size) {
    if (!handle)
        return NULL;
    size_t total = sizeof(pg_unix_global_handle) + size;
    unsigned char *raw = (unsigned char *)realloc(handle->raw, total);
    if (!raw)
        return NULL;
    memcpy(raw, &handle, sizeof(pg_unix_global_handle));
    handle->raw = raw;
    handle->size = size;
    return handle;
}

static inline size_t pg_unix_handle_size(pg_unix_global_handle handle) {
    return handle ? handle->size : 0;
}

static inline pg_unix_global_handle pg_unix_handle_from_ptr(void *ptr) {
    if (!ptr)
        return NULL;
    unsigned char *raw = ((unsigned char *)ptr) - sizeof(pg_unix_global_handle);
    return *((pg_unix_global_handle *)raw);
}

#define MASTER_ENTRY_SIZE	(sizeof(pg_unix_global_handle))
#define MASTER_ENTRY_NULL	NULL
#define SPARE_TIRE_SIZE		0x00014000
#define MAX_HANDLE_ENTRIES	(DEF_MASTER_QTY - 1)

#define pgMemoryPtr(ref) (mem_rec_ptr) pg_unix_lock(*((pg_unix_global_handle *)(ref)))
#define pgFreePtr(ref) pg_unix_unlock(*((pg_unix_global_handle *)(ref)))
#define pgAllocMemory(bytesize) (generic_var)pg_unix_alloc_handle((bytesize), 0)
#define pgAllocMemoryClear(bytesize) (generic_var)pg_unix_alloc_handle((bytesize), 1)
#define pgLockMemory(ref) {}
#define pgLockMemoryHi(ref) {}
#define pgUnlockMemory(ref) {}
#define pgResizeMemory(globals, ref, bytesize) {  \
	pg_unix_global_handle *_slot = (pg_unix_global_handle *)(ref); \
	pg_unix_global_handle _resized = pg_unix_realloc_handle(*_slot, (bytesize)); \
	if (_resized) { \
		*_slot = _resized; \
		(globals)->last_error = NO_ERROR;	\
	} else {								\
		(globals)->last_error = NO_MEMORY_ERR; \
		} }

#define pgMemorySize(ref) pg_unix_handle_size(*((pg_unix_global_handle *)(ref)))
#define pgFreeMemory(ref) pg_unix_free_handle((pg_unix_global_handle)(ref))
#define pgMemoryError(globals) ((globals)->last_error)
#define pgRecoverMemory(ptr) (memory_ref)pg_unix_handle_from_ptr(ptr)
#define BlockMove(src, dest, size) memmove((dest), (src), (size))
#define BlockMoveData(src, dest, size) memmove((dest), (src), (size))
#define ExitToShell() abort()

#endif

#endif
