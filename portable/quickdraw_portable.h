/* quickdraw_portable.h - QuickDraw compatibility helpers for headless builds */
#ifndef PORTABLE_QUICKDRAW_H
#define PORTABLE_QUICKDRAW_H

#if defined(FRONTIER_HEADLESS)

#include "shelltypes.h"
#include "byteorder.h"
#include <stdint.h>

extern void smashrect(Rect r);

static inline void portable_rect_to_diskrect(const Rect *src, struct diskrect *dest) {
    if (!src || !dest)
        return;
    dest->top = src->top;
    dest->left = src->left;
    dest->bottom = src->bottom;
    dest->right = src->right;
    memtodiskshort(dest->top);
    memtodiskshort(dest->left);
    memtodiskshort(dest->bottom);
    memtodiskshort(dest->right);
}

static inline void portable_diskrect_to_rect(const struct diskrect *src, Rect *dest) {
    if (!src || !dest)
        return;
    dest->top = src->top;
    dest->left = src->left;
    dest->bottom = src->bottom;
    dest->right = src->right;
    disktomemshort(dest->top);
    disktomemshort(dest->left);
    disktomemshort(dest->bottom);
    disktomemshort(dest->right);
}

#ifndef recttodiskrect
#define recttodiskrect portable_rect_to_diskrect
#endif

#ifndef diskrecttorect
#define diskrecttorect portable_diskrect_to_rect
#endif

static inline void portable_rgb_to_diskrgb(const RGBColor *src, struct diskrgb *dest) {
    if (!src || !dest)
        return;
    dest->red = (short)src->red;
    dest->green = (short)src->green;
    dest->blue = (short)src->blue;
    memtodiskshort(dest->red);
    memtodiskshort(dest->green);
    memtodiskshort(dest->blue);
}

static inline void portable_diskrgb_to_rgb(const struct diskrgb *src, RGBColor *dest) {
    if (!src || !dest)
        return;
    short red = src->red;
    short green = src->green;
    short blue = src->blue;
    disktomemshort(red);
    disktomemshort(green);
    disktomemshort(blue);
    dest->red = (UInt16)(uint16_t)(unsigned short)red;
    dest->green = (UInt16)(uint16_t)(unsigned short)green;
    dest->blue = (UInt16)(uint16_t)(unsigned short)blue;
}

#ifndef rgbtodiskrgb
#define rgbtodiskrgb portable_rgb_to_diskrgb
#endif

#ifndef diskrgbtorgb
#define diskrgbtorgb portable_diskrgb_to_rgb
#endif

#endif /* FRONTIER_HEADLESS */

#endif /* PORTABLE_QUICKDRAW_H */
