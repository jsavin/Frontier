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

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

#define infinity 32767
#define BIGSTRING(s) ((unsigned char *)(s))
#ifndef lenbigstring
#define lenbigstring 255
#endif

/* Basic types */
typedef unsigned char byte;
typedef unsigned char boolean;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned int uint;
typedef int sint;
typedef long slong;
typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned short ushort;
/* Standard types like uint32_t, int8_t are already defined by system headers */
typedef uint32_t OSType; /* Needed by shelltypes.h */

typedef unsigned char Str255[256];
typedef Str255 bigstring;
typedef void* ptrvoid;
typedef unsigned char* ptrbyte;
typedef unsigned char* ptrstring;
typedef unsigned char** hdlstring;

typedef struct { short top, left, bottom, right; } Rect;
typedef struct { unsigned char pat[8]; } Pattern;
typedef struct { void* dummy; } RgnHandle; /* placeholder */
typedef struct { void* dummy; } ControlHandle; /* placeholder */
typedef struct { void* dummy; } MenuHandle; /* placeholder */
typedef void* WindowPtr; /* placeholder */
typedef void* GrafPtr; /* placeholder */
/* Forward declare filespec for portable stubs */
struct tyfilespec;
/* ptrfilespec and hdlintarray are defined in shelltypes.h when included */

/* Define essential types needed by portable core */
typedef short **hdlintarray;
typedef struct tyfilespec* ptrfilespec;

typedef const struct Rect* rectparam; /* minimal for portable prototypes */

typedef struct HFSUniStr255 { uint16_t length; uint16_t unicode[255]; } HFSUniStr255; /* minimal */
typedef struct { uint8_t data[80]; } FSRef; /* minimal */
typedef struct { long highLongOfPSN; long lowLongOfPSN; } ProcessSerialNumber; /* minimal */

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

/* Misc classic Mac-ish types used in prototypes; portable shims */
typedef struct { short v, h; } Point;
typedef struct { unsigned short red, green, blue; } RGBColor;
typedef int Fixed;
typedef int OSErr;
typedef unsigned int ResType;
typedef void* DialogPtr;
typedef struct { unsigned short ascent, descent, widMax, leading; } FontInfo;
typedef unsigned short UInt16;
typedef unsigned char UInt8;
typedef short SInt16;
typedef int32_t SInt32;
typedef uint32_t UInt32;
typedef struct { uint8_t data[16]; } EventRecord; /* placeholder */
typedef struct { unsigned char bytes[10]; } extended80;

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


