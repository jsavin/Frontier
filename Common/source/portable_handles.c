/*
 * portable_handles.c - Cross-platform implementation of classic Handles
 *
 * The Frontier core assumes Mac-style movable handles are available.
 * Modern platforms no longer provide them, so we recreate the
 * semantics completely in user space.  A Handle is implemented as a
 * pointer to a relocatable data pointer backed by a small header.
 * The handle value itself remains stable while the data buffer may be
 * reallocated, matching the behaviour of classic Moveable Handles.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "frontierdefs.h"
#include "portable_handles.h"

#ifdef FRONTIER_USE_PORTABLE_HANDLES

typedef struct frontier_handle_block {
    unsigned char *data;      /* pointer to the movable data buffer */
    size_t size;              /* number of bytes currently in use  */
    unsigned int lock_count;  /* tracks nested HLock/HUnlock usage */
} frontier_handle_block;

/* The Handle that callers see points just past the header. */
static inline frontier_handle_block *frontier_block_from_handle(Handle h) {
    if (!h)
        return NULL;
    return ((frontier_handle_block *)h) - 1;
}

static inline __attribute__((unused)) Handle frontier_handle_from_block(frontier_handle_block *block) {
    return (Handle)(block + 1);
}

Handle CALLBACK frontierAlloc(long userSize) {
    if (userSize < 0)
        userSize = 0;

    frontier_handle_block *block = (frontier_handle_block *)malloc(sizeof(frontier_handle_block) + sizeof(unsigned char *));
    if (!block)
        return NULL;

    unsigned char **handle = (unsigned char **)(block + 1);

    block->size = (size_t)userSize;
    block->lock_count = 0;
    block->data = NULL;

    if (userSize > 0) {
        block->data = (unsigned char *)malloc((size_t)userSize);
        if (!block->data) {
            free(block);
            return NULL;
        }
        memset(block->data, 0, (size_t)userSize);
    }

    *handle = block->data;
    return (Handle)handle;
}

long CALLBACK frontierSize(Handle h) {
    frontier_handle_block *block = frontier_block_from_handle(h);
    if (!block)
        return 0;
    if (block->size > (size_t)LONG_MAX)
        return LONG_MAX;
    return (long)block->size;
}

char *CALLBACK frontierLock(Handle h) {
    frontier_handle_block *block = frontier_block_from_handle(h);
    if (!block)
        return NULL;
    block->lock_count++;
    return (char *)block->data;
}

void CALLBACK frontierUnlock(Handle h) {
    frontier_handle_block *block = frontier_block_from_handle(h);
    if (!block)
        return;
    if (block->lock_count > 0)
        block->lock_count--;
}

Handle CALLBACK frontierReAlloc(Handle h, long userSize) {
    if (!h)
        return frontierAlloc(userSize);

    if (userSize < 0)
        userSize = 0;

    frontier_handle_block *block = frontier_block_from_handle(h);
    unsigned char **handle = (unsigned char **)h;

    if (userSize == 0) {
        free(block->data);
        block->data = NULL;
        block->size = 0;
        *handle = NULL;
        return h;
    }

    unsigned char *new_data = (unsigned char *)realloc(block->data, (size_t)userSize);
    if (!new_data)
        return NULL;

    if ((size_t)userSize > block->size)
        memset(new_data + block->size, 0, (size_t)userSize - block->size);

    block->data = new_data;
    block->size = (size_t)userSize;
    *handle = new_data;
    return h;
}

void CALLBACK frontierFree(Handle h) {
    if (!h)
        return;

    frontier_handle_block *block = frontier_block_from_handle(h);
    free(block->data);
    free(block);
}

Handle TempNewHandle(long userSize, OSErr *outErr) {
    Handle h = frontierAlloc(userSize);
    if (outErr)
        *outErr = h ? noErr : memFullErr;
    return h;
}

void TempDisposeHandle(Handle h) {
    frontierFree(h);
}

#endif /* FRONTIER_USE_PORTABLE_HANDLES */
