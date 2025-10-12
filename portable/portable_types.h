#ifndef PORTABLE_TYPES_H
#define PORTABLE_TYPES_H

#include <stdint.h>

/* Portable type abstractions for platform-specific concepts */
/* These provide portable equivalents that can be implemented by platform-specific code later */

/* Core boolean type */
typedef unsigned char boolean;
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

/* Basic geometric types */
typedef struct {
    short top, left, bottom, right;
} Rect;

typedef struct {
    short v, h;
} Point;

/* Handle types - portable abstractions for Mac handles */
typedef struct { void* dummy; } ControlHandle;
typedef struct { void* dummy; } MenuHandle;
typedef struct { void* dummy; } RgnHandle;

/* Window and graphics types */
typedef void* WindowPtr;
typedef void* GrafPtr;
typedef void* DialogPtr;

/* File system abstractions */
typedef struct {
    uint16_t length;
    uint16_t unicode[255]; /* UTF-16 characters - portable representation */
} PortableUniStr255;

typedef struct {
    uint8_t data[80]; /* Portable file reference data */
} PortableFileRef;

typedef struct {
    long highLong;
    long lowLong;
} PortableProcessID;

/* Mac-specific type equivalents for portable context */
typedef uint32_t OSType;
typedef struct { unsigned char pat[8]; } Pattern;
typedef struct { unsigned short red, green, blue; } RGBColor;
typedef int Fixed;
typedef int OSErr;
typedef unsigned int ResType;
typedef struct { unsigned short ascent, descent, widMax, leading; } FontInfo;
typedef unsigned short UInt16;
typedef unsigned char UInt8;
typedef short SInt16;
typedef int32_t SInt32;
typedef uint32_t UInt32;
typedef struct { uint8_t data[16]; } EventRecord;
typedef struct { unsigned char bytes[10]; } extended80;

/* Type aliases for compatibility with existing code */
/* These will be conditionally defined based on platform */
#if defined(FRONTIER_PORTABLE) || defined(FRONTIER_HEADLESS)
    /* Use portable types */
    typedef PortableUniStr255 HFSUniStr255;
    typedef PortableFileRef FSRef;
    typedef PortableProcessID ProcessSerialNumber;
    typedef PortableUniStr255 tyfsname;
    typedef PortableUniStr255* tyfsnameptr;
#else
    /* On native Mac, use the real types */
    /* These should be defined by system headers */
#endif

#endif /* PORTABLE_TYPES_H */
