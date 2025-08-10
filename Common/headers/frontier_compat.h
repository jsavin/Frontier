/*
 * frontier_compat.h - Minimal compatibility layer for Frontier
 *
 * This header provides only the essential type definitions needed for
 * Frontier's Classic Mac OS dependencies to work with modern macOS.
 *
 * Phase 0.4: Compiler Compatibility - Minimal Approach
 * 
 * TEST_INCLUDE: This file is being included successfully
 */

#ifndef FRONTIER_COMPAT_H
#define FRONTIER_COMPAT_H

// Test definition to verify this file is being included
#define FRONTIER_COMPAT_INCLUDED 1

// On Apple platforms, rely on SDK headers for classic types to avoid redefinition.
#ifdef __APPLE__
    #define FRONTIER_COMPAT_INCLUDED 1
    // Do not predefine classic Mac types here; SDK headers will provide them.
    // Leave this header effectively empty on Apple to prevent type conflicts.
    
    // Still provide basic compiler/arch tags for conditional code if needed
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

    #if defined(__arm64__) || defined(__aarch64__)
        #define FRONTIER_ARCH_ARM64 1
    #elif defined(__x86_64__)
        #define FRONTIER_ARCH_X86_64 1
    #elif defined(__ppc__) || defined(__powerpc__)
        #define FRONTIER_ARCH_PPC 1
    #else
        #define FRONTIER_ARCH_UNKNOWN 1
    #endif

#else

// 1. Include only minimal system headers to avoid complex include chains (non-Apple)
#include <stdint.h>

// 2. QuickTime removed - feature disabled in modern version
// No QuickTime types needed

// 3. Define missing types that are causing compilation errors
// These are the specific types that Frontier headers need

// Legacy memory management types
typedef char* Ptr;
typedef Ptr* Handle;

// Additional compatibility types
typedef unsigned char Boolean;

typedef uint8_t  UInt8;  typedef int8_t  SInt8;
typedef uint16_t UInt16; typedef int16_t SInt16;
typedef uint32_t UInt32; typedef int32_t SInt32;
typedef uint64_t UInt64; typedef int64_t SInt64;

// 4. Define the specific missing types that are causing errors
// These are the types that Frontier headers expect but can't find

// Legacy Mac OS types
typedef Handle RgnHandle;
typedef struct { unsigned char data[8]; } Pattern;
typedef Handle ControlHandle;
typedef Handle MenuHandle;

// File system types
typedef struct FSRef { unsigned char data[80]; } FSRef;
typedef struct HFSUniStr255 { unsigned short length; unsigned short data[255]; } HFSUniStr255;

// 5. Compiler detection for conditional compilation
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

#endif // __APPLE__

#endif // FRONTIER_COMPAT_H
