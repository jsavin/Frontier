/* standard_portable.h - Minimal portable types/macros for core */

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

/* Boolean constants are now defined in portable_types.h */

#define infinity 32767
#define BIGSTRING(s) ((unsigned char *)(s))
#ifndef lenbigstring
#define lenbigstring 255
#endif

/* Basic types */
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned int uint;
typedef int sint;
typedef long slong;
typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned short ushort;

typedef unsigned char Str255[256];
typedef Str255 bigstring;
typedef void* ptrvoid;
typedef unsigned char* ptrbyte;
typedef unsigned char* ptrstring;
typedef unsigned char** hdlstring;

/* These types are now defined in portable_types.h */
/* Forward declare filespec for portable stubs */
struct tyfilespec;
/* ptrfilespec and hdlintarray are defined in shelltypes.h when included */

/* Essential types are defined in shelltypes.h when included */

typedef const struct Rect* rectparam; /* minimal for portable prototypes */

/* Direction enums and related used widely in headers */
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

typedef enum tybitdirection {
    upbit = 0x01,
    downbit = 0x02,
    leftbit = 0x04,
    rightbit = 0x08
} tybitdirection;

/* These types are now defined in portable_types.h */

typedef short hdlfilenum;

typedef boolean (*callback)(void);
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
#define chlinefeed ((char)10)
#define chreturn ((char)13)
#define chclosecurlyquote ((char)0xD3)
#define chtrademark ((char)0xAA)
#define chtab ((char)9)
#define chsinglequote ((char)39)
#define chdoublequote ((char)34)
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

#ifndef fldebug
#define fldebug 0
#endif

#ifndef intinfinity
#define intinfinity 32767
#endif
#ifndef intminusinfinity
#define intminusinfinity (-32768)
#endif
#ifndef longinfinity
#define longinfinity ((long)0x7FFFFFFF)
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

static inline GrafPtr GetQDGlobalsThePort(void){ return (GrafPtr)0; }
static inline void SetPort(GrafPtr p){ (void)p; }

#ifndef noErr
#define noErr 0
#endif

/* Portable stubs for file/path helpers used by langvalue */
#ifdef FRONTIER_PORTABLE
/* Portable file helper stubs */
static inline boolean pathtofilespec(bigstring bs, struct tyfilespec* fs){ (void)bs; (void)fs; return false; }
static inline boolean filenotfounderror(void){ return false; }
static inline boolean filespectopath(const struct tyfilespec* fs, bigstring bs){ (void)fs; setemptystring(bs); return false; }
static inline boolean equalfilespecs(const struct tyfilespec* a, const struct tyfilespec* b){ (void)a; (void)b; return false; }
static inline boolean getfsfile(const struct tyfilespec* pfs, bigstring name){ (void)pfs; setemptystring(name); return false; }
static inline void DebugStr(const unsigned char* s){ (void)s; }
static inline void Debugger(void){}
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

#endif /* FRONTIER_STANDARD_PORTABLE_H */


