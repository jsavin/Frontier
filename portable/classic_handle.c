/* classic_handle.c - Classic Mac Handle semantics implementation */
/* 2025-10-31 Codex: Rework shim to reuse frontier portable handles and track MemError/MaxBlock state. */

#include "classic_handle.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "portable_handles.h"

typedef struct ClassicHeapState {
    OSErr last_error;
    size_t largest_success;
    size_t last_failure;
    size_t cached_max;
} ClassicHeapState;

static ClassicHeapState gClassicHeap = {
    noErr,
    512 * 1024,              /* assume at least 512 KB available */
    SIZE_MAX,                /* no failure observed yet */
    512 * 1024
};

static void classic_note_success(size_t requested) {
    if (requested == 0)
        requested = 1;
    gClassicHeap.last_error = noErr;
    if (requested > gClassicHeap.largest_success)
        gClassicHeap.largest_success = requested;
    if (requested > gClassicHeap.cached_max)
        gClassicHeap.cached_max = requested;
}

static void classic_note_failure(size_t requested) {
    gClassicHeap.last_error = memFullErr;
    if (requested < gClassicHeap.last_failure)
        gClassicHeap.last_failure = requested;
    if (requested < gClassicHeap.cached_max)
        gClassicHeap.cached_max = requested;
}

static size_t classic_clamp_long(size_t value) {
    if (value > (size_t)LONG_MAX)
        return (size_t)LONG_MAX;
    return value;
}

Handle ClassicNewHandle(size_t initial_size) {
    if (initial_size > (size_t)LONG_MAX)
        initial_size = (size_t)LONG_MAX;

    Handle h = frontierAlloc((long)initial_size);
    if (h)
        classic_note_success(initial_size);
    else
        classic_note_failure(initial_size);
    return h;
}

void ClassicDisposeHandle(Handle h) {
    if (!h)
        return;
    frontierFree(h);
    gClassicHeap.last_error = noErr;
}

void ClassicHLock(Handle h) {
    (void)frontierLock(h);
}

void ClassicHUnlock(Handle h) {
    frontierUnlock(h);
}

size_t ClassicGetHandleSize(Handle h) {
    size_t sz = (size_t)frontierSize(h);
    gClassicHeap.last_error = noErr;
    return sz;
}

int ClassicSetHandleSize(Handle h, size_t new_size) {
    if (!h) {
        classic_note_failure(new_size);
        return 0;
    }

    if (new_size > (size_t)LONG_MAX)
        new_size = (size_t)LONG_MAX;

    Handle resized = frontierReAlloc(h, (long)new_size);
    if (!resized) {
        classic_note_failure(new_size);
        return 0;
    }

    classic_note_success(new_size);
    return 1;
}

Handle ClassicDupHandle(Handle h) {
    if (!h)
        return NULL;

    size_t sz = ClassicGetHandleSize(h);
    if (sz > (size_t)LONG_MAX)
        sz = (size_t)LONG_MAX;
    Handle duplicate = ClassicNewHandle(sz);
    if (!duplicate)
        return NULL;

    if (sz > 0) {
        unsigned char *src = (unsigned char *)(*h);
        unsigned char *dst = (unsigned char *)(*duplicate);
        if (src && dst)
            memcpy(dst, src, sz);
    }

    return duplicate;
}

OSErr ClassicMemError(void) {
    return gClassicHeap.last_error;
}

long ClassicMaxBlock(void) {
    size_t baseline = gClassicHeap.largest_success;
    if (baseline < 256 * 1024)
        baseline = 256 * 1024;

    size_t ceiling;
    if (gClassicHeap.last_failure != SIZE_MAX) {
        ceiling = gClassicHeap.last_failure;
        if (ceiling < baseline)
            ceiling = baseline;
        /* keep a buffer below the last failure */
        size_t margin = ceiling / 16;
        if (margin == 0)
            margin = 4096;
        if (ceiling > margin)
            ceiling -= margin;
    } else {
        ceiling = gClassicHeap.cached_max;
        if (ceiling < baseline)
            ceiling = baseline;
        /* allow some optimistic growth */
        size_t growth = baseline + baseline / 2;
        if (growth > ceiling)
            ceiling = growth;
    }

    ceiling = classic_clamp_long(ceiling);
    gClassicHeap.cached_max = ceiling;
    return (long)ceiling;
}

void ClassicResetHeapTracking(void) {
    gClassicHeap.last_error = noErr;
    gClassicHeap.largest_success = 512 * 1024;
    gClassicHeap.last_failure = SIZE_MAX;
    gClassicHeap.cached_max = 512 * 1024;
}
