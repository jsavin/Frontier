/* memory_portable.h - portable shim for Frontier memory.h API under FRONTIER_PORTABLE */
/* 2025-11-30 Codex: Pull in frontier.h first to avoid macro redefinitions when stubs are disabled. */
#ifndef FRONTIER_MEMORY_PORTABLE_H
#define FRONTIER_MEMORY_PORTABLE_H

#include "frontier.h"
#include <string.h>
 
/* Map Handle and core primitives */
#ifndef FRONTIER_ALLOW_PORTABLE_STUBS
#define FRONTIER_ALLOW_PORTABLE_STUBS 1
#endif

#if FRONTIER_ALLOW_PORTABLE_STUBS

static inline boolean newhandle(long sz, Handle* ph) { *ph = NewHandle((size_t)sz); return (*ph)!=NULL; }
static inline boolean newemptyhandle(Handle* ph) { *ph = NewHandle(0); return (*ph)!=NULL; }
static inline void disposehandle(Handle h) { DisposeHandle(h); }
static inline void lockhandle(Handle h) { HLock(h); }
static inline void unlockhandle(Handle h) { HUnlock(h); }
static inline boolean validhandle(Handle h) { if (h == NULL) return true; return GetHandleSize(h) >= 0; }
static inline long gethandlesize(Handle h) { return (long)GetHandleSize(h); }
static inline boolean sethandlesize(Handle h, long sz) { return ClassicSetHandleSize(h, (size_t)sz)!=0; }
static inline boolean minhandlesize(Handle h, long sz) { long cur = gethandlesize(h); return cur >= sz || sethandlesize(h, sz); }
Handle ClassicDupHandle(Handle h);
static inline boolean copyhandle(Handle h, Handle* ph) { *ph = ClassicDupHandle(h); return (*ph)!=NULL; }

static inline void moveleft(ptrvoid src, ptrvoid dst, long ct) { memmove(dst, src, (size_t)ct); }
static inline void moveright(ptrvoid src, ptrvoid dst, long ct) { memmove(dst, src, (size_t)ct); }
static inline void clearbytes(ptrvoid p, long ct) { memset(p, 0, (size_t)ct); }

static inline void texthandletostring(Handle h, bigstring bs) {
    setstringlength(bs, 0);
    if (!h) return;
    long len = gethandlesize(h);
    if (len <= 0) return;
    char* p = (char*)(*h);
    long copy = (len > lenbigstring) ? lenbigstring : len;
    setstringlength(bs, (short)copy);
    memcpy(stringbaseaddress(bs), p, (size_t)copy);
    (void)0;
}
#endif /* FRONTIER_ALLOW_PORTABLE_STUBS */

#endif
