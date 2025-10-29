#ifndef PORTABLE_TYPES_H
#define PORTABLE_TYPES_H

#include <stdint.h>

/* Portable type abstractions for platform-specific concepts */

/* Core boolean type */
#ifndef boolean
typedef unsigned char boolean;
#endif
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

/* Portable stand-ins for Unicode file names and FSRefs */
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

#if defined(FRONTIER_PORTABLE) || defined(FRONTIER_HEADLESS)
#ifndef PORTABLE_TYFSNAME_DEFINED
typedef PortableUniStr255 tyfsname;
typedef PortableUniStr255* tyfsnameptr;
#define PORTABLE_TYFSNAME_DEFINED 1
#endif
#endif

#endif /* PORTABLE_TYPES_H */
