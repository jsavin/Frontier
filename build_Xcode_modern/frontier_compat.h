/*
 * frontier_compat.h - Comprehensive compatibility layer for Frontier
 *
 * This header provides compatibility for Frontier's Classic Mac OS dependencies
 * to work with modern macOS, enabling 64-bit compilation.
 *
 * Phase 0.4: Compiler Compatibility
 */

#ifndef FRONTIER_COMPAT_H
#define FRONTIER_COMPAT_H

// 1. Include all system headers first to get proper definitions
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>

// 2. QuickTime removed - feature disabled in modern version
// No QuickTime types needed

// 3. Define missing types that system headers don't provide
// Note: FSRef and HFSUniStr255 are already defined by system headers
// No additional definitions needed

// 4. Define legacy memory management types
#ifndef Handle
typedef Ptr* Handle;
#endif

#ifndef Ptr
typedef char* Ptr;
#endif

// 5. Additional compatibility types
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

// 6. Legacy Apple event types (use system definitions)
// AppleEvent and AEDescList are already defined by system headers

// 7. Legacy types that might be missing (only define if not already defined)
// Note: Most of these are already provided by system headers
// Only define if truly missing to avoid conflicts

// 8. QuickTime types removed - feature disabled
// No QuickTime types needed

// 9. Compiler detection for conditional compilation
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

// 10. Architecture detection
#if defined(__arm64__) || defined(__aarch64__)
    #define FRONTIER_ARCH_ARM64 1
#elif defined(__x86_64__)
    #define FRONTIER_ARCH_X86_64 1
#elif defined(__ppc__) || defined(__powerpc__)
    #define FRONTIER_ARCH_PPC 1
#else
    #define FRONTIER_ARCH_UNKNOWN 1
#endif

#endif // FRONTIER_COMPAT_H
