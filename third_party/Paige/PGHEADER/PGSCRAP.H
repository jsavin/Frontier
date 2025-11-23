/* Header file for pgScrap. This defines the external functions and types for machine-specific
scrap operations. However, the basic API is portable. */

#ifndef PGSCRAP_H
#define PGSCRAP_H

#include "Paige.h"
#include "pgEmbed.h"

// Extern scrap types:

enum {
   pg_void_scrap,
   pg_native_scrap,
   pg_text_scrap,
   pg_unicode_scrap,
   pg_embed_scrap
};

#ifdef MAC_PLATFORM

#define PG_TEXT_OSTYPE        'TEXT'
#define PG_PICT_OSTYPE        'PICT'
#define PG_UNICODE_OSTYPE     'UCOD'

typedef OSType    pg_os_type;

#endif

#ifdef WINDOWS_PLATFORM

#define PG_TEXT_OSTYPE     CF_TEXT
#ifdef WIN32_COMPILE
#define PG_UNICODE_OSTYPE  CF_UNICODETEXT
#else
#define PG_UNICODE_OSTYPE  CF_TEXT
#endif

#define PG_PICT_OSTYPE     CF_METAFILEPICT

#ifdef WIN16_COMPILE
typedef WORD         pg_os_type;
#else
typedef UINT         pg_os_type;
#endif

#endif

#ifdef UNIX_PLATFORM

#include <stdint.h>

#define PG_TEXT_OSTYPE        0
#define PG_PICT_OSTYPE        0
#define PG_UNICODE_OSTYPE     0

typedef uint32_t pg_os_type;

#endif

#define SCRAP_KEY_QTY	(PLATFORM_SPECIFIC_KEY - 2)

#ifdef __cplusplus
extern   "C" {
#endif

extern PG_PASCAL (pg_ref) pgGetScrap (pg_globals_ptr globals, pg_os_type native_format, embed_callback def_embed_callback);
extern PG_PASCAL (pg_boolean) pgScrapAvail (pg_os_type native_format);
extern PG_PASCAL (void) pgPutScrap (pg_ref the_scrap, pg_os_type native_format, short scrap_type);

#ifdef WINDOWS_PLATFORM
extern PG_PASCAL (HMETAFILE) pgBitmapToMetafile(HBITMAP bitmap, LPRECT bounds); // PDA:
#endif // WINDOWS_PLATFORM

#ifdef __cplusplus
   }
#endif

#endif
