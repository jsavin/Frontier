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

typedef struct pg_unix_slot_struct {
    pg_unix_global_handle handle;
} pg_unix_slot_struct, *pg_unix_slot;

#define PG_UNIX_REF_SIZE (sizeof(pg_unix_slot))

static inline unsigned char *pg_unix_payload(pg_unix_slot slot) {
    return (slot && slot->handle) ? slot->handle->raw + PG_UNIX_REF_SIZE : NULL;
}

static inline void pg_unix_store_slot(pg_unix_slot slot) {
    if (!slot || !slot->handle)
        return;
    memcpy(slot->handle->raw, &slot, sizeof(slot));
}

static inline pg_unix_slot pg_unix_alloc_handle(size_t size, int clear) {
    pg_unix_global_handle handle = (pg_unix_global_handle)malloc(sizeof(*handle));
    if (!handle)
        return NULL;
    size_t total = PG_UNIX_REF_SIZE + size;
    unsigned char *raw = clear ? (unsigned char *)calloc(1, total)
                               : (unsigned char *)malloc(total);
    if (!raw) {
        free(handle);
        return NULL;
    }
    handle->size = size;
    handle->raw = raw;
    pg_unix_slot slot = (pg_unix_slot)malloc(sizeof(*slot));
    if (!slot) {
        free(handle->raw);
        free(handle);
        return NULL;
    }
    slot->handle = handle;
    pg_unix_store_slot(slot);
    return slot;
}

static inline void pg_unix_free_handle(pg_unix_slot slot) {
    if (!slot)
        return;
    if (slot->handle) {
        free(slot->handle->raw);
        free(slot->handle);
    }
    free(slot);
}

static inline unsigned char *pg_unix_lock(pg_unix_slot slot) {
    return pg_unix_payload(slot);
}

static inline void pg_unix_unlock(pg_unix_slot slot) {
    (void)slot;
}

static inline pg_unix_slot pg_unix_realloc_handle(pg_unix_slot slot, size_t size) {
    if (!slot || !slot->handle)
        return NULL;
    size_t total = PG_UNIX_REF_SIZE + size;
    unsigned char *raw = (unsigned char *)realloc(slot->handle->raw, total);
    if (!raw)
        return NULL;
    slot->handle->raw = raw;
    slot->handle->size = size;
    pg_unix_store_slot(slot);
    return slot;
}

static inline size_t pg_unix_handle_size(pg_unix_slot slot) {
    return (slot && slot->handle) ? slot->handle->size : 0;
}

static inline pg_unix_slot pg_unix_handle_from_ptr(void *ptr) {
    if (!ptr)
        return NULL;
    unsigned char *raw = ((unsigned char *)ptr) - PG_UNIX_REF_SIZE;
    pg_unix_slot slot = NULL;
    memcpy(&slot, raw, sizeof(slot));
    return slot;
}

#define MASTER_ENTRY_SIZE	(sizeof(pg_unix_slot))
#define MASTER_ENTRY_NULL	NULL
#define SPARE_TIRE_SIZE		0x00014000
#define MAX_HANDLE_ENTRIES	(DEF_MASTER_QTY - 1)

#define pgMemoryPtr(ref) (mem_rec_ptr) pg_unix_lock(*((pg_unix_slot *)(ref)))
#define pgFreePtr(ref) pg_unix_unlock(*((pg_unix_slot *)(ref)))
#define pgAllocMemory(bytesize) (generic_var)pg_unix_alloc_handle((bytesize), 0)
#define pgAllocMemoryClear(bytesize) (generic_var)pg_unix_alloc_handle((bytesize), 1)
#define pgLockMemory(ref) {}
#define pgLockMemoryHi(ref) {}
#define pgUnlockMemory(ref) {}
#define pgResizeMemory(globals, ref, bytesize) {  \
	pg_unix_slot *_slot = (pg_unix_slot *)(ref); \
	pg_unix_slot _resized = pg_unix_realloc_handle(*_slot, (bytesize)); \
	if (_resized) { \
		*_slot = _resized; \
		(globals)->last_error = NO_ERROR;	\
	} else {								\
		(globals)->last_error = NO_MEMORY_ERR; \
		} }

#define pgMemorySize(ref) pg_unix_handle_size(*((pg_unix_slot *)(ref)))
#define pgFreeMemory(ref) do { \
	pg_unix_slot *_slot = (pg_unix_slot *)(ref); \
	if (_slot && *_slot) { \
		pg_unix_free_handle(*_slot); \
		*_slot = NULL; \
	} \
} while (0)
#define pgMemoryError(globals) ((globals)->last_error)
#define pgRecoverMemory(ptr) (memory_ref)pg_unix_handle_from_ptr(ptr)
#define BlockMove(src, dest, size) memmove((dest), (src), (size))
#define BlockMoveData(src, dest, size) memmove((dest), (src), (size))
#define ExitToShell() abort()

#endif

#endif
