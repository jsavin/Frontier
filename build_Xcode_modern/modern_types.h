/*
 * modern_types.h - Modern type compatibility layer for Frontier
 *
 * This header provides compatibility definitions for legacy Mac OS types
 * that are no longer available in modern macOS, enabling 64-bit compilation.
 *
 * Phase 0.4: Compiler Compatibility
 */

#ifndef MODERN_TYPES_H
#define MODERN_TYPES_H

// Include modern frameworks first to get proper definitions
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreServices/CoreServices.h>
#include <Carbon/Carbon.h>

// Legacy handle type compatibility
// Use conditional compilation to avoid conflicts with system headers

#ifndef RgnHandle
typedef struct OpaqueRgnHandle* RgnHandle;
#endif

#ifndef ControlHandle
typedef struct OpaqueControl* ControlHandle;
#endif

#ifndef MenuHandle
typedef struct OpaqueMenu* MenuHandle;
#endif

// Pattern type - use system definition if available, otherwise define
#ifndef Pattern
typedef struct Pattern Pattern;
#endif

// Legacy Mac OS types that may not be available
#ifndef Point
typedef struct {
    short v;
    short h;
} Point;
#endif

#ifndef Rect
typedef struct {
    short top;
    short left;
    short bottom;
    short right;
} Rect;
#endif

// Event record structure
#ifndef EventRecord
typedef struct {
    EventKind what;
    long message;
    long when;
    Point where;
    EventModifiers modifiers;
} EventRecord;
#endif

// Additional legacy types that may be needed
#ifndef GrafPtr
typedef struct CGrafPort* GrafPtr;
#endif

#ifndef WindowPtr
typedef struct CGrafPort* WindowPtr;
#endif

#ifndef DialogPtr
typedef struct CGrafPort* DialogPtr;
#endif

// Memory management types
#ifndef Handle
typedef Ptr* Handle;
#endif

#ifndef Ptr
typedef char* Ptr;
#endif

// File system types
#ifndef FSRef
struct FSRef {
    UInt8 hidden[80];
};
#endif

#ifndef HFSUniStr255
struct HFSUniStr255 {
    UInt16 length;
    UniChar unicode[255];
};
typedef struct HFSUniStr255 tyfsname;
#endif

// Additional compatibility types
#ifndef Boolean
typedef unsigned char Boolean;
#endif

#ifndef UInt8
typedef unsigned char UInt8;
#endif

#ifndef UInt16
typedef unsigned short UInt16;
#endif

#ifndef UInt32
typedef unsigned int UInt32;
#endif

#ifndef SInt8
typedef signed char SInt8;
#endif

#ifndef SInt16
typedef signed short SInt16;
#endif

#ifndef SInt32
typedef signed int SInt32;
#endif

// Compiler detection for conditional compilation
#if defined(__clang__)
    #define FRONTIER_COMPILER_CLANG 1
    #define FRONTIER_COMPILER_VERSION __clang_major__
#elif defined(__GNUC__)
    #define FRONTIER_COMPILER_GCC 1
    #define FRONTIER_COMPILER_VERSION __GNUC__
#elif defined(_MSC_VER)
    #define FRONTIER_COMPILER_MSVC 1
    #define FRONTIER_COMPILER_VERSION _MSC_VER
#else
    #define FRONTIER_COMPILER_UNKNOWN 1
#endif

// Architecture detection
#if defined(__arm64__) || defined(__aarch64__)
    #define FRONTIER_ARCH_ARM64 1
#elif defined(__x86_64__)
    #define FRONTIER_ARCH_X86_64 1
#elif defined(__ppc__) || defined(__powerpc__)
    #define FRONTIER_ARCH_PPC 1
#else
    #define FRONTIER_ARCH_UNKNOWN 1
#endif

#endif // MODERN_TYPES_H
