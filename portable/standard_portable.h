/* standard_portable.h - Minimal portable types/macros for core */
// 2025-11-09 Codex: Guard classic typedefs with explicit macros to prevent duplicate definitions.
// 2025-12-06 Codex: Raise longinfinity to 64-bit max to align with modern numeric plan.

#ifndef FRONTIER_STANDARD_PORTABLE_H
#define FRONTIER_STANDARD_PORTABLE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>

/* Pull string token macros (works in portable too) */
#include "../Common/headers/stringdefs.h"

/* Include portable type abstractions before shelltypes.h */
#include "portable_types.h"

/* Include portable shelltypes for portable context to get hdlintarray and other types */
#include "shelltypes_portable.h"

/* Include text encoding portability layer */
#include "text_encoding_portable.h"

/* Boolean constants are now defined in portable_types.h */

#define infinity 32767
#define BIGSTRING(s) ((unsigned char *)(s))
#ifndef lenbigstring
#define lenbigstring 255
#endif

/* Basic types */
#ifndef FRONTIER_PORTABLE_DEFINED_BYTE
typedef unsigned char byte;
#define FRONTIER_PORTABLE_DEFINED_BYTE 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_WORD
typedef unsigned short word;
#define FRONTIER_PORTABLE_DEFINED_WORD 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_DWORD
typedef unsigned long dword;
#define FRONTIER_PORTABLE_DEFINED_DWORD 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_UINT
typedef unsigned int uint;
#define FRONTIER_PORTABLE_DEFINED_UINT 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_SINT
typedef int sint;
#define FRONTIER_PORTABLE_DEFINED_SINT 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_SLONG
typedef long slong;
#define FRONTIER_PORTABLE_DEFINED_SLONG 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_ULONG
typedef unsigned long ulong;
#define FRONTIER_PORTABLE_DEFINED_ULONG 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_UCHAR
typedef unsigned char uchar;
#define FRONTIER_PORTABLE_DEFINED_UCHAR 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_USHORT
typedef unsigned short ushort;
#define FRONTIER_PORTABLE_DEFINED_USHORT 1
#endif

#if !defined(OS_PORTABLE_HAS_STR255)
#ifndef FRONTIER_PORTABLE_DEFINED_STR255
typedef unsigned char Str255[256];
#define FRONTIER_PORTABLE_DEFINED_STR255 1
#endif
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_BIGSTRING
typedef Str255 bigstring;
#define FRONTIER_PORTABLE_DEFINED_BIGSTRING 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_PTRVOID
typedef void* ptrvoid;
#define FRONTIER_PORTABLE_DEFINED_PTRVOID 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_PTRBYTE
typedef unsigned char* ptrbyte;
#define FRONTIER_PORTABLE_DEFINED_PTRBYTE 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_PTRSTRING
typedef unsigned char* ptrstring;
#define FRONTIER_PORTABLE_DEFINED_PTRSTRING 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_PTRCHAR
typedef char* ptrchar;
#define FRONTIER_PORTABLE_DEFINED_PTRCHAR 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_HDLSTRING
typedef unsigned char** hdlstring;
#define FRONTIER_PORTABLE_DEFINED_HDLSTRING 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_HDLREGION
typedef RgnHandle hdlregion;
#define FRONTIER_PORTABLE_DEFINED_HDLREGION 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_XPPATTERN
typedef Pattern xppattern;
#define FRONTIER_PORTABLE_DEFINED_XPPATTERN 1
#endif

/* These types are now defined in portable_types.h */
/* Forward declare filespec for portable stubs */
struct tyfilespec;
/* ptrfilespec and hdlintarray are defined in shelltypes.h when included */

/* Essential types are defined in shelltypes.h when included */

#ifndef FRONTIER_PORTABLE_DEFINED_RECTPARAM
typedef const struct Rect* rectparam; /* minimal for portable prototypes */
#define FRONTIER_PORTABLE_DEFINED_RECTPARAM 1
#endif

/* Direction enums and related used widely in headers */
#ifndef ctdirections
typedef enum tydirection {
    nodirection = 0,
    up = 1,
    down = 2,
    left = 3,
    right = 4,
    flatup = 5,
    flatdown = 6,
    sorted = 8,
    pageup = 9,
    pagedown = 10,
    pageleft = 11,
    pageright = 12
} tydirection;
#define ctdirections 12 /*for arrays indexed on directions*/
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_TYBITDIRECTION
typedef enum tybitdirection {
    upbit = 0x01,
    downbit = 0x02,
    leftbit = 0x04,
    rightbit = 0x08
} tybitdirection;
#define FRONTIER_PORTABLE_DEFINED_TYBITDIRECTION 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_TYLINESPACING
typedef enum tylinespacing {
    singlespaced = 1,
    oneandalittlespaced = 2,
    oneandaquarterspaced = 3,
    oneandahalfspaced = 4,
    doublespaced = 5,
    triplespaced = 6
} tylinespacing;
#define FRONTIER_PORTABLE_DEFINED_TYLINESPACING 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_TYJUSTIFICATION
typedef enum tyjustification {
    leftjustified,
    centerjustified,
    rightjustified,
    fulljustified,
    unknownjustification
} tyjustification;
#define FRONTIER_PORTABLE_DEFINED_TYJUSTIFICATION 1
#endif

/* These types are now defined in portable_types.h */

#ifndef FRONTIER_PORTABLE_DEFINED_HDLFILENUM
typedef short hdlfilenum;
#define FRONTIER_PORTABLE_DEFINED_HDLFILENUM 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_CALLBACK
typedef boolean (*callback)(void);
#define FRONTIER_PORTABLE_DEFINED_CALLBACK 1
#endif
#ifndef pascal
#define pascal
#endif

/* Cursor constants referenced by headers */
#ifndef iBeamCursor
#define iBeamCursor 1
#endif
#ifndef watchCursor
#define watchCursor 2
#endif

#define stringbaseaddress(bs) ((bs)+1)
#define setstringlength(bs,len) ((bs)[0] = (unsigned char)(len))
#define stringlength(bs) ((unsigned char)(bs)[0])
#define setstringwithchar(ch,bs) do { (bs)[0]=1; (bs)[1]=(ch); } while(0)
#define getstringcharacter(bs,pos) ((bs)[(pos)+1])
#define setstringcharacter(bs,pos,ch) do { (bs)[(pos)+1] = (ch); } while(0)
#define lastchar(bs) ((bs)[stringlength(bs)])
#define chnul ((char)0)
#define chbacktab ((char)0)
#define chenter ((char)3)
#define chbackspace ((char)8)
#define chtab ((char)9)
#define chlinefeed ((char)10)
#define chreturn ((char)13)
#define chspace ((char)32)
#define chuparrow ((char)30)
#define chdownarrow ((char)31)
#define chdelete ((char)127)
#define chsinglequote ((char)39)
#define chdoublequote ((char)34)
#define chclosecurlyquote ((char)0xD3)
#define chtrademark ((byte)0xAA)
#define chopencurlyquote ((byte)0xD2)
#define chnotequals ((byte)0xAD)
#define chdivide ((byte)0xD6)
#define chcomment ((byte)0xC7)
#define chendcomment ((byte)0xC8)
#ifndef nil
#define nil 0
#endif

#ifndef setemptystring
#define setemptystring(bs) (setstringlength(bs,0))
#endif

#ifndef isemptystring
#define isemptystring(bs) (stringlength(bs)==0)
#endif

#ifndef stringsize
#define stringsize(bs) (stringlength (bs) + 1)
#endif

#ifndef longsizeof
#define longsizeof(x) (long)sizeof(x)
#endif

#ifndef emptystring
#define emptystring ((ptrstring)"\0")
#endif

#ifndef sgn
#define sgn(x) ((x) < 0? -1 : ((x) > 0? 1 : 0))
#endif

#ifndef fldebug
#define fldebug 0

/* Allow callers to disable inline stub fallbacks when linking real portable implementations. */
#ifndef FRONTIER_ALLOW_PORTABLE_STUBS
#define FRONTIER_ALLOW_PORTABLE_STUBS 1
#endif
#endif

#ifndef intinfinity
#define intinfinity 32767
#endif
#ifndef intminusinfinity
#define intminusinfinity (-32768)
#endif
#ifndef longinfinity
#define longinfinity ((long)0x7FFFFFFFFFFFFFFFLL)
#endif

#ifndef FixRound
#define FixRound(x) (x)
#endif
#ifndef FixRatio
#define FixRatio(a,b) ((a)*(65536)/(b))
#endif
#ifndef FixMul
#define FixMul(a,b) ((int)((((long long)(a))*((long long)(b)))>>16))
#endif

#ifndef sysbeep
#define sysbeep() do{}while(0)
#endif

/* Mark that standard macros have been defined to prevent redefinition in Common/SystemHeaders/standard.h */
#define FRONTIER_STANDARD_MACROS_DEFINED

static inline GrafPtr GetQDGlobalsThePort(void){ return (GrafPtr)0; }
static inline void SetPort(GrafPtr p){ (void)p; }

#ifndef noErr
#define noErr 0
#endif

/* Portable stubs for file/path helpers used by langvalue */
#if defined(FRONTIER_PORTABLE) && FRONTIER_ALLOW_PORTABLE_STUBS && !defined(FRONTIER_HEADLESS)
/* Portable file helper stubs - excluded from headless mode which uses file_portable.c implementations */
static inline boolean pathtofilespec(bigstring bs, struct tyfilespec* fs){ (void)bs; (void)fs; return false; }
static inline boolean filenotfounderror(void){ return false; }
static inline boolean filespectopath(const struct tyfilespec* fs, bigstring bs){ (void)fs; setemptystring(bs); return false; }
static inline boolean equalfilespecs(const struct tyfilespec* a, const struct tyfilespec* b){ (void)a; (void)b; return false; }
static inline boolean getfsfile(const struct tyfilespec* pfs, bigstring name){ (void)pfs; setemptystring(name); return false; }
static inline boolean langportable_err_noop(unsigned char* bs, void* refcon){ (void)bs; (void)refcon; return true; }
#endif

/* Misc macros used in core */
#ifndef bundle
#define bundle /**/
#endif


/* Utility macros */
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef odd
#define odd(x) ((x) & 0x0001)
#endif

#ifndef even
#define even(x) (!odd(x))
#endif

#ifndef loword
#define loword(x) ((x) & 0x0000ffff)
#endif

#ifndef hiword
#define hiword(x) ((x) >> 16)
#endif

#ifndef makelong
#define makelong(lo, hi) ((hi) << 16 | (lo))
#endif

/* conditionalshortswap is defined in byteorder.h - do not duplicate here */

#ifndef diskwordstomemlong
#define diskwordstomemlong(lo, hi) makelong(conditionalshortswap(lo), conditionalshortswap(hi))
#endif

#ifndef memlongtodiskwords
#define memlongtodiskwords(x, lo, hi) do { \
	lo = conditionalshortswap(loword(x)); \
	hi = conditionalshortswap(hiword(x)); \
} while (0)
#endif

#endif /* FRONTIER_STANDARD_PORTABLE_H */
