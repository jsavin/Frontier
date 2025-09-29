/*
 * osincludes_portable.h - Minimal OS abstraction for non-macOS builds.
 */

#ifndef OSINCLUDES_PORTABLE_H
#define OSINCLUDES_PORTABLE_H

#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "portable_handles.h"

#ifndef UInt16
typedef uint16_t UInt16;
#endif

#ifndef SInt16
typedef int16_t SInt16;
#endif

#ifndef UInt32
typedef uint32_t UInt32;
#endif

#ifndef SInt32
typedef int32_t SInt32;
#endif

#ifndef UInt8
typedef uint8_t UInt8;
#endif

#ifndef SInt8
typedef int8_t SInt8;
#endif

#ifndef Str255
typedef unsigned char Str255[256];
#endif

#ifndef ConstStr255Param
typedef const unsigned char *ConstStr255Param;
#endif

#ifndef Fixed
typedef int32_t Fixed;
#endif

#ifndef RGBColor
typedef struct RGBColor {
    UInt16 red;
    UInt16 green;
    UInt16 blue;
} RGBColor;
#endif

#ifdef _WIN32
    #include <windows.h>
    #ifndef Boolean
    typedef unsigned char Boolean;
    #endif
#else
    #ifndef Boolean
    typedef unsigned char Boolean;
    #endif
    #ifndef ELASTERRNO
    #define ELASTERRNO ELAST
    #endif
#endif

#ifndef UInt64
typedef uint64_t UInt64;
#endif

#ifndef OSStatus
typedef int32_t OSStatus;
#endif

#ifndef ItemCount
typedef uint32_t ItemCount;
#endif

#ifndef TextEncoding
typedef uint32_t TextEncoding;
#endif

#ifndef TextEncodingBase
typedef uint32_t TextEncodingBase;
#endif

#ifndef TextEncodingVariant
typedef uint32_t TextEncodingVariant;
#endif

#ifndef TextEncodingFormat
typedef uint32_t TextEncodingFormat;
#endif

#ifndef RegionCode
typedef uint32_t RegionCode;
#endif

#ifndef TextEncodingNameSelector
typedef uint32_t TextEncodingNameSelector;
#endif

#ifndef TECObjectRef
typedef void *TECObjectRef;
#endif

#ifndef ResType
typedef uint32_t ResType;
#endif

#ifndef FontInfo
typedef struct FontInfo {
    short ascent;
    short descent;
    short leading;
    short widMax;
} FontInfo;
#endif

#ifndef extended80
typedef struct extended80 {
    uint8_t bytes[10];
} extended80;
#endif

#ifndef CGrafPtr
typedef void *CGrafPtr;
#endif

#ifndef THz
typedef void *THz;
#endif

#ifndef AliasHandle
typedef Handle AliasHandle;
#endif

#ifndef pascal
#define pascal
#endif

#ifndef iBeamCursor
#define iBeamCursor 1
#endif

#ifndef watchCursor
#define watchCursor 2
#endif

#ifndef kTextEncodingFullName
#define kTextEncodingFullName 0
#endif

#ifndef kTextEncodingMacRoman
#define kTextEncodingMacRoman 0
#endif

#ifndef kTextUnsupportedEncodingErr
#define kTextUnsupportedEncodingErr (-30874)
#endif

#ifndef kTextMalformedInputErr
#define kTextMalformedInputErr (-32768)
#endif

#ifndef kTextUndefinedElementErr
#define kTextUndefinedElementErr (-32767)
#endif

#ifndef kTECNoConversionPathErr
#define kTECNoConversionPathErr (-32766)
#endif

#ifndef kTECPartialCharErr
#define kTECPartialCharErr (-32765)
#endif


#ifndef StringPtr
typedef unsigned char *StringPtr;
#endif

#ifndef StringHandle
typedef StringPtr *StringHandle;
#endif

#ifndef nil
#define nil NULL
#endif

#ifndef UniChar
typedef uint16_t UniChar;
#endif

#ifndef UniCharCount
typedef uint32_t UniCharCount;
#endif

#ifndef HFSUniStr255
typedef struct HFSUniStr255 {
    uint16_t length;
    UniChar unicode[255];
} HFSUniStr255;
#endif

#ifndef FSRef
typedef struct FSRef {
    uint8_t hidden[80];
} FSRef;
#endif

#ifndef ProcessSerialNumber
typedef struct ProcessSerialNumber {
    uint32_t highLongOfPSN;
    uint32_t lowLongOfPSN;
} ProcessSerialNumber;
#endif

#ifndef Pattern
typedef struct Pattern {
    uint8_t pat[8];
} Pattern;
#endif

#ifndef RgnHandle
typedef void *RgnHandle;
#endif

#ifndef ControlHandle
typedef void *ControlHandle;
#endif

#ifndef MenuHandle
typedef void *MenuHandle;
#endif

#ifndef DialogPtr
typedef void *DialogPtr;
#endif

#ifndef GrafPtr
typedef void *GrafPtr;
#endif

#ifndef WindowPtr
typedef void *WindowPtr;
#endif

#ifndef OSType
typedef uint32_t OSType;
#endif

#ifndef Point
typedef struct Point { int16_t v; int16_t h; } Point;
#endif

#ifndef Rect
typedef struct Rect { int16_t top; int16_t left; int16_t bottom; int16_t right; } Rect;
#endif

#ifndef EventRecord
typedef struct EventRecord {
    UInt16 what;
    UInt32 message;
    UInt32 when;
    Point where;
    UInt16 modifiers;
} EventRecord;
#endif

#ifndef topLeft
#define topLeft(r)  (((Point *) &(r))[0])
#endif

#ifndef botRight
#define botRight(r) (((Point *) &(r))[1])
#endif

#endif /* OSINCLUDES_PORTABLE_H */
