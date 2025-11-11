#ifndef PGREGION_H
#define PGREGION_H

/* This file provides cross-platform macros for region manipulation.
PORTING NOTE: If there is no equivilent API for any of these macros, make it a function
and created the emulating code in your platform-specific code. */


#include "CPUDEFS.H"

#ifdef MAC_PLATFORM
#define pgCreateRgn()  NewRgn()
#define pgSectRgn(rgn1, rgn2, rgn3) SectRgn(rgn1, rgn2, rgn3)
#define pgUnionRgn(rgn1, rgn2, rgn3) UnionRgn(rgn1, rgn2, rgn3)
#define pgSubRgn(rgn1, rgn2, rgn3) DiffRgn(rgn1, rgn2, rgn3)
#define pgDisposeRgn(rgn) DisposeRgn(rgn)
#define pgEmptyRgn(rgn) EmptyRgn(rgn)
#define pgSetEmptyRgn(rgn) SetEmptyRgn(rgn)
#define pgCopyRgn(rgn1, rgn2) CopyRgn(rgn1, rgn2)
#define pgRectToRgn(rgn, r) RectRgn(rgn, r)
#define pgOffsetRgn(rgn, h, v) OffsetRgn(rgn, h, v)
#define pgInsetRgn(rgn, h, v) InsetRgn(rgn, h, v)

#endif


#ifdef WINDOWS_PLATFORM

#define pgCreateRgn() CreateRectRgn(0,0,0,0)
#define pgSectRgn(rgn1, rgn2, rgn3) CombineRgn((HRGN)rgn3, (HRGN)rgn1, (HRGN)rgn2, RGN_AND)
#define pgUnionRgn(rgn1, rgn2, rgn3) CombineRgn((HRGN)rgn3, (HRGN)rgn1, (HRGN)rgn2, RGN_OR)
#define pgSubRgn(rgn1, rgn2, rgn3) CombineRgn((HRGN)rgn3, (HRGN)rgn1, (HRGN)rgn2, RGN_DIFF)
#define pgDisposeRgn(rgn) DeleteObject(rgn)
#define pgSetEmptyRgn(rgn) SetRectRgn((HRGN) rgn, 0, 0, 0, 0)
#define pgCopyRgn(rgn1, rgn2) CombineRgn((HRGN) rgn2, (HRGN) rgn1, (HRGN) rgn1, RGN_COPY)
#define pgOffsetRgn(rgn, h, v) OffsetRgn((HRGN)rgn, h, v)

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (void)pgRectToRgn(pg_region rgn, RECT PG_FAR *r);
extern PG_PASCAL (void) pgInsetRgn (pg_region rgn, short dh, short dv);
extern PG_PASCAL (pg_boolean) pgEmptyRgn(pg_region rgn);

#ifdef __cplusplus
	}
#endif

#endif
#ifdef UNIX_PLATFORM

#include <stdlib.h>

typedef struct pg_unix_region {
    Rect bounds;
    pg_boolean empty;
} pg_unix_region;

static inline pg_unix_region *pg_unix_region_cast(pg_region rgn) {
    return (pg_unix_region *)rgn;
}

static inline pg_region pg_unix_create_region(void) {
    pg_unix_region *region = (pg_unix_region *)malloc(sizeof(pg_unix_region));
    if (!region)
        return NULL;
    region->empty = TRUE;
    region->bounds.top = region->bounds.left = region->bounds.bottom = region->bounds.right = 0;
    return (pg_region)region;
}

static inline void pg_unix_dispose_region(pg_region rgn) {
    free(pg_unix_region_cast(rgn));
}

static inline void pg_unix_copy_region(pg_region src, pg_region dst) {
    pg_unix_region *s = pg_unix_region_cast(src);
    pg_unix_region *d = pg_unix_region_cast(dst);
    *d = *s;
}

static inline void pg_unix_rect_to_region(pg_region rgn, const Rect *r) {
    pg_unix_region *dest = pg_unix_region_cast(rgn);
    dest->bounds = *r;
    dest->empty = FALSE;
}

static inline void pg_unix_set_empty(pg_region rgn) {
    pg_unix_region *region = pg_unix_region_cast(rgn);
    region->empty = TRUE;
}

static inline pg_boolean pg_unix_empty(pg_region rgn) {
    pg_unix_region *region = pg_unix_region_cast(rgn);
    return region->empty;
}

static inline void pg_unix_offset(pg_region rgn, short h, short v) {
    pg_unix_region *region = pg_unix_region_cast(rgn);
    if (region->empty)
        return;
    region->bounds.top += v;
    region->bounds.bottom += v;
    region->bounds.left += h;
    region->bounds.right += h;
}

static inline void pg_unix_inset(pg_region rgn, short h, short v) {
    pg_unix_region *region = pg_unix_region_cast(rgn);
    if (region->empty)
        return;
    region->bounds.top += v;
    region->bounds.bottom -= v;
    region->bounds.left += h;
    region->bounds.right -= h;
    if (region->bounds.top >= region->bounds.bottom || region->bounds.left >= region->bounds.right)
        region->empty = TRUE;
}

static inline void pg_unix_union(pg_region a, pg_region b, pg_region result) {
    pg_unix_region *ra = pg_unix_region_cast(a);
    pg_unix_region *rb = pg_unix_region_cast(b);
    pg_unix_region *rr = pg_unix_region_cast(result);
    if (ra->empty && rb->empty) {
        rr->empty = TRUE;
        return;
    }
    if (ra->empty) { *rr = *rb; return; }
    if (rb->empty) { *rr = *ra; return; }
    rr->empty = FALSE;
    rr->bounds.top = (ra->bounds.top < rb->bounds.top) ? ra->bounds.top : rb->bounds.top;
    rr->bounds.left = (ra->bounds.left < rb->bounds.left) ? ra->bounds.left : rb->bounds.left;
    rr->bounds.bottom = (ra->bounds.bottom > rb->bounds.bottom) ? ra->bounds.bottom : rb->bounds.bottom;
    rr->bounds.right = (ra->bounds.right > rb->bounds.right) ? ra->bounds.right : rb->bounds.right;
}

static inline void pg_unix_intersect(pg_region a, pg_region b, pg_region result) {
    pg_unix_region *ra = pg_unix_region_cast(a);
    pg_unix_region *rb = pg_unix_region_cast(b);
    pg_unix_region *rr = pg_unix_region_cast(result);
    if (ra->empty || rb->empty) { rr->empty = TRUE; return; }
    rr->bounds.top = (ra->bounds.top > rb->bounds.top) ? ra->bounds.top : rb->bounds.top;
    rr->bounds.left = (ra->bounds.left > rb->bounds.left) ? ra->bounds.left : rb->bounds.left;
    rr->bounds.bottom = (ra->bounds.bottom < rb->bounds.bottom) ? ra->bounds.bottom : rb->bounds.bottom;
    rr->bounds.right = (ra->bounds.right < rb->bounds.right) ? ra->bounds.right : rb->bounds.right;
    rr->empty = (rr->bounds.top >= rr->bounds.bottom || rr->bounds.left >= rr->bounds.right);
}

static inline void pg_unix_subtract(pg_region a, pg_region b, pg_region result) {
    pg_unix_region *ra = pg_unix_region_cast(a);
    pg_unix_region *rr = pg_unix_region_cast(result);
    if (ra->empty) { rr->empty = TRUE; return; }
    *rr = *ra;
}

#define pgCreateRgn() pg_unix_create_region()
#define pgSectRgn(r1,r2,r3) pg_unix_intersect(r1,r2,r3)
#define pgUnionRgn(r1,r2,r3) pg_unix_union(r1,r2,r3)
#define pgSubRgn(r1,r2,r3) pg_unix_subtract(r1,r2,r3)
#define pgDisposeRgn(r) pg_unix_dispose_region(r)
#define pgEmptyRgn(r) pg_unix_empty(r)
#define pgSetEmptyRgn(r) pg_unix_set_empty(r)
#define pgCopyRgn(r1,r2) pg_unix_copy_region(r1,r2)
#define pgRectToRgn(rgn,r) pg_unix_rect_to_region(rgn,r)
#define pgOffsetRgn(rgn,h,v) pg_unix_offset(rgn,h,v)
#define pgInsetRgn(rgn,h,v) pg_unix_inset(rgn,h,v)

#endif /* UNIX_PLATFORM */

#endif /* PGREGION_H */

