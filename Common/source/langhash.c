
/*	$Id$    */

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

/* 2025-11-24 Codex: Normalize BE writes/coverage for v7 portability. */
/* 2025-12-02 Codex: Skip free-block drops for externals during Save-As repack so migrated scripts persist. */
/* 2025-12-07 Codex: Add env-gated materialization trace with path logging. */
/* 2025-12-09 Codex: Harden hashunpack bounds and repack v7 records via manual BE buffer for name-corruption chase. */


#include "frontier.h"
#include "standard.h"

#include "error.h"
#include "logging.h"  /* Phase 2 logging infrastructure migration */
#include "memory.h"
#include "strings.h"
#include "font.h"
#include "ops.h"
#include <stddef.h> /* for offsetof static asserts */
#include <stdlib.h> /* getenv for materialize tracing */
#include <string.h> /* memcpy for BE64 double/int conversions */
#if !defined(FRONTIER_HEADLESS)
#include "quickdraw.h"
#include "resources.h"
#include "langipc.h"
#include "langsystem7.h"
#endif
#include "lang.h"
#include "langinternal.h"
#include "langexternal.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "tableexternal_common.h"
#include "oplist.h"
/* 2025-11-20 Codex: Skip loadfromhandle when less than one record remains so EOF scans stay silent. */
#include "timedate.h"
#include "db_format.h" /* 2025-11-23 Codex: BE helpers for table metadata */
#if defined(FRONTIER_TESTS)
#include "langhash_test.h"
#endif
// 2025-11-28 Codex: Use db_context when dereferencing externals during hash packing.
#include "byteorder.h"	/* 2006-04-08 aradke: endianness conversion macros */

/* Forward declare disk record types needed for static assertions */
typedef union tydiskvaluedata_v7 {
	uint64_t longvalue;      /* for int/long/token, etc. */
	uint64_t datevalue;      /* 64-bit Mac epoch */
	uint64_t doublebits;     /* IEEE 754 double bits */
	int32_t dirvalue;        /* directions stay 32-bit */
	struct { int16_t v; int16_t h; } pointvalue; /* 16-bit coords */
	uint32_t ostypevalue;    /* 32-bit OSType */
	uint32_t enumvalue;      /* 32-bit enum */
	int32_t fixedvalue;      /* fixed stays 32-bit */
	int32_t tokenvalue;      /* token stays 32-bit (in practice 16-bit) */
} tydiskvaluedata_v7;

typedef struct tydisksymbolrecord_v7 {
	int32_t ixkey;       /* index into string handle */
	uint8_t valuetype;   /* copied from value record */
	uint8_t version;     /* packed flags/version */
	uint16_t _pad;       /* align to 8-byte data */
	tydiskvaluedata_v7 data; /* inline scalar storage */
} tydisksymbolrecord_v7, *ptrdisksymbolrecord_v7, **hdldisksymbolrecord_v7;

/* Ensure the manual BE layout matches the on-disk record definition. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(tydisksymbolrecord_v7) == 16, "tydisksymbolrecord_v7 must be 16 bytes");
_Static_assert(offsetof(tydisksymbolrecord_v7, data) == 8, "tydisksymbolrecord_v7.data offset must be 8");
#endif
#if defined(FRONTIER_HEADLESS)
#include "../portable/wptext_portable.h"
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#define WP_PLACEHOLDER_TEXT "WPText not migrated because it was too old to read."
extern boolean getstringlist(short listid, short stringid, bigstring bs);
extern void recttodiskrect(Rect *, diskrect *);
extern void rgbtodiskrgb(const RGBColor *, diskrgb *);
extern void diskrecttorect(const diskrect *, Rect *);
extern void diskrgbtorgb(const diskrgb *, RGBColor *);
extern boolean langpackfileval(const tyvaluerecord *val, Handle *hpacked);
extern boolean langunpackfileval(Handle hpacked, tyvaluerecord *v);
extern boolean aliastofilespec(AliasHandle alias, tyfilespec *fs);
extern long filespecsize(tyfilespec fs);
#endif
#include <stdint.h>
#include <limits.h>
#if !defined(FRONTIER_HEADLESS)
	#include "aeutils.h" /*PBS 03/14/02: AE OS X fix.*/
#endif

// 2025-10-27 Codex: Added headless logging to inspect serialized table handles during root load.
// 2025-11-16 Codex: Added helpers to materialize disk-backed table values before packing.

#if defined(FRONTIER_HEADLESS)
static boolean langhash_convert_wordprocessor_external(hdlexternalvariable hv, const char *path_hint, tyvaluerecord *replacement) {
	if (hv == nil || replacement == NULL)
		return false;

	const char *log_path = (path_hint != NULL && path_hint[0] != '\0') ? path_hint : "<unknown>";

	log_trace(LOG_COMP_HASH, "wp-convert begin hv=%p path=%s", (void *)hv, log_path);

	Handle hplain_utf8 = nil;
	if (!wp_portable_extract_plaintext(hv, &hplain_utf8)) {
		bigstring bsplaceholder;
		wp_portable_note_drop_logged(hv, log_path);
		copyctopstring(WP_PLACEHOLDER_TEXT, bsplaceholder);
		log_trace(LOG_COMP_HASH, "wp-convert fallback hv=%p path=%s", (void *)hv, log_path);
		return setstringvalue(bsplaceholder, replacement);
	}

	long utf8_len = gethandlesize(hplain_utf8);
	if (!setheapvalue(hplain_utf8, stringvaluetype, replacement)) {
		disposehandle(hplain_utf8);
		return false;
	}

	if (wp_portable_external_was_legacy_ws(hv))
		wp_portable_note_conversion_logged(hv, log_path);

	log_trace(LOG_COMP_HASH, "wp-convert ok hv=%p path=%s bytes=%ld", (void *)hv, log_path, utf8_len);
	return true;
}

static boolean langhash_prepare_wordprocessor_value(bigstring bsname, hdlhashnode hnode, tyvaluerecord *val) {
	hdlexternalvariable hv;
	bigstring bspath;
	char pathbuf[512];

	if (val == NULL || (*val).valuetype != externalvaluetype)
		return true;

	hv = (hdlexternalvariable)(*val).data.externalvalue;
	if (hv == nil)
		return true;

	if ((**hv).id != idwordprocessor)
		return true;

	if (!langexternalgetfullpath(currenthashtable, bsname, bspath, nil))
		copystring(bsname, bspath);

	/* Pascal string length byte must fit plus NUL. */
	if ((size_t) bspath[0] >= (sizeof pathbuf) - 1)
		return false; /* path would overflow */
	copyptocstring(bspath, pathbuf);

	tyvaluerecord replacement;
	if (!langhash_convert_wordprocessor_external(hv, pathbuf, &replacement))
		return false;

	disposevaluerecord(*val, false);
	*val = replacement;
	(**hnode).val = replacement;
	return true;
}
#endif

static boolean langhash_materialize_table_internal(hdlhashtable htable, const char *path);
static boolean langhash_materialize_value(tyvaluerecord *val, const char *path);
static boolean langhash_materialize_external(tyvaluerecord *val, const char *path);
/* Exposed for debug logging in tableunpacktable errors. */
const char *langhash_materialize_current_path = NULL;
static char langhash_materialize_path_buf[512];
#if defined(FRONTIER_HEADLESS)
/* Hash unpack diagnostics */
static FILE *hashunpack_log = NULL;
static boolean hashunpack_log_init = false;
static void close_hashunpack_log(void) {
	if (hashunpack_log != NULL) {
		fclose(hashunpack_log);
		hashunpack_log = NULL;
	}
}
static boolean is_safe_log_path(const char *path) {
	if (path == NULL || *path == '\0')
		return false;
	/* reject absolute paths and parent traversals */
	if (path[0] == '/')
		return false;
	if (strstr(path, "..") != NULL)
		return false;
	return true;
}
#endif
static boolean langhash_materialize_trace_enabled(void) {
	static short initialized = 0;
	static boolean enabled = false;
	if (!initialized) {
		const char *env = getenv("FRONTIER_MATERIALIZE_TRACE");
		enabled = (env != NULL && env[0] != '\0' && env[0] != '0');
		initialized = 1;
	}
	return enabled;
}

boolean langhash_materialize_disk_values(hdlhashtable htable) {
	return langhash_materialize_table_internal(htable, "root");
}

static boolean langhash_materialize_table_internal(hdlhashtable htable, const char *path) {
	if (htable == nil)
		return true;

	hdlhashnode nomad = (**htable).hfirstsort;
	while (nomad != nil) {
		tyvaluerecord *val = &(**nomad).val;
		bigstring bsname;
		char nodepath[512];
		const char *prior_path = langhash_materialize_current_path;

		gethashkey(nomad, bsname);
			size_t need = (size_t) bsname[0] + 1; /* name + dot/null */
			if (path != NULL && path[0] != '\0')
				need += strlen(path) + 1; /* dot + existing path */
			if (need >= sizeof(nodepath)) {
#if defined(FRONTIER_HEADLESS)
				if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_HASH)) {
					log_trace(LOG_COMP_HASH, "materialize path overflow path=%s name=%.*s need=%zu limit=%zu",
		        path ? path : "<nil>", (int) bsname[0], (char *) &bsname[1],
		        need, sizeof(nodepath));
				}
#endif
				return false; /* avoid overflow on deep nesting */
			}

			if (path != NULL && path[0] != '\0')
				snprintf(nodepath, sizeof(nodepath), "%s.%.*s", path, (int) bsname[0], (char *) &bsname[1]);
			else
				snprintf(nodepath, sizeof(nodepath), "%.*s", (int) bsname[0], (char *) &bsname[1]);

		strncpy(langhash_materialize_path_buf, nodepath, sizeof(langhash_materialize_path_buf) - 1);
		langhash_materialize_path_buf[sizeof(langhash_materialize_path_buf) - 1] = '\0';
		langhash_materialize_current_path = langhash_materialize_path_buf;

		if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_HASH)) {
			log_trace(LOG_COMP_HASH, "materialize visit path=%s type=%d disk=%d",
		        nodepath, (int) val->valuetype, val->fldiskval ? 1 : 0);
		}

		if (!langhash_materialize_value(val, nodepath)) {
#if defined(FRONTIER_HEADLESS)
			if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_HASH)) {
				log_trace(LOG_COMP_HASH, "materialize value failed path=%s type=%d",
				        nodepath, (int) val->valuetype);
			}
#endif
			return false;
		}
		if (val->valuetype == externalvaluetype) {
			if (!langhash_materialize_external(val, nodepath)) {
#if defined(FRONTIER_HEADLESS)
				if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_HASH)) {
					log_trace(LOG_COMP_HASH, "materialize external failed path=%s type=%d",
					        nodepath, (int) val->valuetype);
				}
#endif
				return false;
			}
		}

		langhash_materialize_current_path = prior_path;
		nomad = (**nomad).sortedlink;
	}
	return true;
}

static boolean langhash_materialize_value(tyvaluerecord *val, const char *path) {
	if (val == NULL)
		return true;
	if (!(*val).fldiskval)
		return true;

	if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_HASH)) {
		log_trace(LOG_COMP_HASH, "materialize path=%s type=%d",
		        path ? path : "<nil>", (int) val->valuetype);
	}

	const char *prior_path = langhash_materialize_current_path;
	boolean set_local_path = false;

	if (path != NULL && langhash_materialize_current_path == NULL) {
		strncpy(langhash_materialize_path_buf, path, sizeof(langhash_materialize_path_buf) - 1);
		langhash_materialize_path_buf[sizeof(langhash_materialize_path_buf) - 1] = '\0';
		langhash_materialize_current_path = langhash_materialize_path_buf;
		set_local_path = true;
	}

	tyvaluerecord copy;
	if (!copyvaluerecord(*val, &copy)) {
		if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_HASH)) {
			log_trace(LOG_COMP_HASH, "materialize copy failed path=%s type=%d",
			        path ? path : "<nil>", (int) val->valuetype);
		}
		if (set_local_path)
			langhash_materialize_current_path = prior_path;
		return false;
	}

	copy.fltmpstack = false;
	copy.fltmpdata = false;

	disposevaluerecord(*val, false);
	*val = copy;
	if (set_local_path)
		langhash_materialize_current_path = prior_path;
	return true;
}

static boolean langhash_materialize_external(tyvaluerecord *val, const char *path) {
	hdlexternalvariable hv = (hdlexternalvariable)(*val).data.externalvalue;
	if (hv == nil)
		return true;

	const char *prior_path = langhash_materialize_current_path;
	char local_path_buf[256];

	if (path != NULL) {
		strncpy(local_path_buf, path, sizeof(local_path_buf) - 1);
		local_path_buf[sizeof(local_path_buf) - 1] = '\0';
		langhash_materialize_current_path = local_path_buf;
	}

	switch ((**hv).id) {
#if defined(FRONTIER_HEADLESS)
		case idwordprocessor: {
			if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_HASH)) {
				log_trace(LOG_COMP_HASH, "materialize external path=%s id=wordprocessor",
				        path ? path : "<nil>");
			}
			tyvaluerecord replacement;
			if (!langhash_convert_wordprocessor_external(hv, "<materialize>", &replacement)) {
				langhash_materialize_current_path = prior_path;
				return false;
			}
			disposevaluerecord(*val, false);
			*val = replacement;
			langhash_materialize_current_path = prior_path;
			return true;
		}
#endif
		case idtableprocessor: {
			if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_HASH)) {
				log_trace(LOG_COMP_HASH, "materialize external path=%s id=table",
				        path ? path : "<nil>");
			}
			if (!tableverbinmemory(NULL, hv, HNoNode)) {
				log_error(LOG_COMP_HASH, "materialize external table load failed path=%s",
				        path ? path : "<nil>");
				langhash_materialize_current_path = prior_path;
				return false;
			}
			hdlhashtable child = (hdlhashtable)(**hv).variabledata;
			/* During adapter_repack (migration), mark materialized tables as dirty to force save */
			if (db_format_mode_current().adapter_repack && child != nil) {
				(**child).fldirty = true;
				(**child).flsubsdirty = true;
			}
			boolean ok = langhash_materialize_table_internal(child, path);
			langhash_materialize_current_path = prior_path;
			return ok;
		}
		default:
			langhash_materialize_current_path = prior_path;
			return true;
	}
}

/* Enable to dump detailed serializer diagnostics. */
/* #define DEBUG_SERIALIZER 1 */


#if defined(__clang__) || defined(__GNUC__)
#pragma pack(push, 2)
#else
#pragma pack(2)
#endif

typedef union tydiskvaluedata {
	int32_t longvalue;
	uint32_t datevalue;
	int32_t dirvalue;
	int16_t intvalue;
	uint16_t uintvalue;
	unsigned char chvalue;
	struct { int16_t v; int16_t h; } pointvalue;
} tydiskvaluedata;

/* v7+ value payloads: 64-bit numerics, BE on disk */
typedef struct tydisksymbolrecord { /*stored at offset 0 in the db file*/
	int32_t ixkey; /*in the string handle, where is this symbol's name?*/
	uint8_t valuetype; /*copied from the symbol's value record*/
	uint8_t version;
	tydiskvaluedata data; /*if a string, this stores an index into the string handle*/
} tydisksymbolrecord, *ptrdisksymbolrecord, **hdldisksymbolrecord;

typedef struct tyOLD42disksymbolrecord {
	int32_t ixkey; /*in the string handle, where is this symbol's name?*/
	uint8_t valuetype; /*copied from the symbol's value record*/
	uint8_t version : 4;
	uint8_t unused : 3;
	uint8_t flsorted : 1; /*were these records packed in sort order?*/
	tydiskvaluedata data; /*if a string, this stores an index into the string handle*/
} tyOLD42disksymbolrecord, *ptrOLD42disksymbolrecord, **hdlOLD42disksymbolrecord;


// 5.0.1: bumped version number so we can clear uninitialized flags
// 2025-11-19 Codex: bump to 0x04 to reserve 1KB of header padding for future metadata.
// 2025-12-05: bump to 0x05 for 64-bit timestamps (beyond 2040 support)
#define tablediskversion 0x05


/* Legacy v0x04 structure with 32-bit timestamps - for reading old databases */
typedef struct tydisktablerecord_v4 {
	int16_t version;
	int16_t sortorder;
	uint32_t timecreated;
	uint32_t timelastsave;
	int32_t flags;
} tydisktablerecord_v4;

/* Temporarily override pack(2) to allow 8-byte alignment for 64-bit timestamps */
#if defined(__clang__) || defined(__GNUC__)
#pragma pack(push, 8)
#else
#pragma pack(8)
#endif

/* Modern v0x05 structure with 64-bit timestamps - for v7 databases */
typedef struct tydisktablerecord { /*current disk format*/
	int16_t version;           /* 2 bytes - 0x05 */
	int16_t sortorder;         /* 2 bytes */
	uint32_t _pad;             /* 4 bytes - padding for 8-byte alignment */
	uint64_t timecreated;      /* 8 bytes - 64-bit timestamp */
	uint64_t timelastsave;     /* 8 bytes - 64-bit timestamp */
	int32_t flags;             /* 4 bytes */
	/* Followed by TABLE_HEADER_RESERVED_BYTES (1024 bytes) */
} tydisktablerecord, *ptrdisktablerecord, **hdldisktablerecord;

/* Restore pack(2) for following structures */
#if defined(__clang__) || defined(__GNUC__)
#pragma pack(pop)
#else
#pragma pack(2)
#endif

typedef enum tylinetableitemflags {

	flxml = 0x8000

	} tylinetableitemflags;

#define	maxinlinescalarsize	1023
#define	diskvalsizeflag		(-1)

typedef struct tydiskvaluerecord {		/*4.0.1b1 dmb*/
	long sizeflag;	/*always -1*/
	dbaddress adr;	/*address of actual scalar value*/
	} tydiskvaluerecord;


static inline int32_t host_to_disk_int32(int32_t value) {
#if defined(SWAP_BYTE_ORDER)
	long temp = (long) value;
	db_format_write_be32(&temp, (uint32_t) temp);
	return (int32_t) temp;
#else
	return value;
#endif
}

static inline int16_t host_to_disk_int16(int16_t value) {
#if defined(SWAP_BYTE_ORDER)
	short temp = (short) value;
	memtodiskshort (temp);
	return (int16_t) temp;
#else
	return value;
#endif
}

static inline int32_t disk_to_host_int32(int32_t value) {
#if defined(SWAP_BYTE_ORDER)
	long temp = (long) value;
	disktomemlong (temp);
	return (int32_t) temp;
#else
	return value;
#endif
}

static inline int16_t disk_to_host_int16(int16_t value) {
#if defined(SWAP_BYTE_ORDER)
	short temp = (short) value;
	disktomemshort (temp);
	return (int16_t) temp;
#else
	return value;
#endif
}

static inline int64_t host_to_disk_int64(int64_t value) {
#if defined(SWAP_BYTE_ORDER)
	uint64_t temp = (uint64_t) value;
	db_format_write_be64(&temp, temp);
	return (int64_t) temp;
#else
	return value;
#endif
}

static inline int64_t disk_to_host_int64(int64_t value) {
#if defined(SWAP_BYTE_ORDER)
	uint64_t temp = (uint64_t) value;
	temp = db_format_read_be64((const unsigned char *) &temp);
	return (int64_t) temp;
#else
	return value;
#endif
}

static inline uint64_t host_to_disk_double_bits(double value) {
	uint64_t bits = 0;
	memcpy(&bits, &value, sizeof(bits));
	db_format_write_be64(&bits, bits);
	return bits;
}

static inline double disk_to_host_double_bits(uint64_t bits) {
	uint64_t host = db_format_read_be64((const unsigned char *) &bits);
	double value = 0.0;
	memcpy(&value, &host, sizeof(value));
	return value;
}

static inline dbaddress host_to_disk_dbaddress(dbaddress value) {
#if defined(SWAP_BYTE_ORDER)
	if (sizeof (dbaddress) == 8) {
		unsigned long long temp = (unsigned long long) value;
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
		temp = __builtin_bswap64(temp);
#else
		temp = ((temp & 0x00000000000000FFULL) << 56) |
		       ((temp & 0x000000000000FF00ULL) << 40) |
		       ((temp & 0x0000000000FF0000ULL) << 24) |
		       ((temp & 0x00000000FF000000ULL) << 8)  |
		       ((temp & 0x000000FF00000000ULL) >> 8)  |
		       ((temp & 0x0000FF0000000000ULL) >> 24) |
		       ((temp & 0x00FF000000000000ULL) >> 40) |
		       ((temp & 0xFF00000000000000ULL) >> 56);
#endif
		return (dbaddress) temp;
	}
	return (dbaddress) host_to_disk_int32((int32_t) value);
#else
	return value;
#endif
}

static inline dbaddress disk_to_host_dbaddress(dbaddress value) {
#if defined(SWAP_BYTE_ORDER)
	if (sizeof (dbaddress) == 8) {
		unsigned long long temp = (unsigned long long) value;
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
		temp = __builtin_bswap64(temp);
#else
		temp = ((temp & 0x00000000000000FFULL) << 56) |
		       ((temp & 0x000000000000FF00ULL) << 40) |
		       ((temp & 0x0000000000FF0000ULL) << 24) |
		       ((temp & 0x00000000FF000000ULL) << 8)  |
		       ((temp & 0x000000FF00000000ULL) >> 8)  |
		       ((temp & 0x0000FF0000000000ULL) >> 24) |
		       ((temp & 0x00FF000000000000ULL) >> 40) |
		       ((temp & 0xFF00000000000000ULL) >> 56);
#endif
		return (dbaddress) temp;
	}
	return (dbaddress) disk_to_host_int32((int32_t) value);
#else
	return value;
#endif
}

static boolean write_disk_uint32 (handlestream *s, uint32_t value) {
	uint32_t disk = (uint32_t) host_to_disk_int32((int32_t) value);
	return writehandlestream (s, &disk, (long) sizeof (disk));
}

static boolean read_disk_uint32 (Handle hload, long *ixload, uint32_t *out) {
	uint32_t disk = 0;
	if (!loadfromhandle (hload, ixload, (long) sizeof (disk), &disk))
		return (false);
	*out = (uint32_t) disk_to_host_int32 ((int32_t) disk);
	return (true);
}

static boolean write_disk_dbaddress (handlestream *s, dbaddress value, boolean use_64bit) {
	if (use_64bit) {
		dbaddress disk = host_to_disk_dbaddress (value);
		return writehandlestream (s, &disk, (long) sizeof (dbaddress));
	}
	assert (value >= 0 && (unsigned long long) value <= 0xFFFFFFFFULL);
	int32_t disk = host_to_disk_int32 ((int32_t) value);
	return writehandlestream (s, &disk, (long) sizeof (disk));
}

static boolean read_disk_dbaddress (Handle hload, long *ixload, dbaddress *out, boolean use_64bit) {
	if (use_64bit) {
		 dbaddress disk = 0;
		if (!loadfromhandle (hload, ixload, (long) sizeof (dbaddress), &disk))
			return (false);
		*out = disk_to_host_dbaddress (disk);
		return (true);
	}
	uint32_t legacy = 0;
	if (!read_disk_uint32 (hload, ixload, &legacy))
		return (false);
	*out = (dbaddress) legacy;
	return (true);
}

static boolean write_disk_scalar_reference (handlestream *s, dbaddress adr, boolean use_64bit) {
	int32_t diskflag = host_to_disk_int32 (diskvalsizeflag);
	if (!writehandlestream (s, &diskflag, (long) sizeof (diskflag)))
		return (false);
	return write_disk_dbaddress (s, adr, use_64bit);
}

static void diskvalue_from_value_legacy(const tyvaluerecord *val, tydiskvaluedata *out) {
	clearbytes(out, sizeof(*out));
	switch (val->valuetype) {
		case novaluetype:
			out->chvalue = (unsigned char) val->data.chvalue;
			break;
		case booleanvaluetype:
			out->chvalue = (unsigned char) (val->data.flvalue ? 1 : 0);
			break;
		case charvaluetype:
			out->chvalue = (unsigned char) val->data.chvalue;
			break;
		case intvaluetype:
		case tokenvaluetype:
			out->intvalue = host_to_disk_int16((int16_t) val->data.intvalue);
			break;
		case pointvaluetype:
			out->pointvalue.h = host_to_disk_int16((int16_t) val->data.pointvalue.h);
			out->pointvalue.v = host_to_disk_int16((int16_t) val->data.pointvalue.v);
			break;
		case directionvaluetype:
			out->dirvalue = host_to_disk_int32((int32_t) val->data.dirvalue);
			break;
		case datevaluetype:
			out->datevalue = (uint32_t) host_to_disk_int32((int32_t) val->data.datevalue);
			break;
		case longvaluetype:
		case ostypevaluetype:
		case enumvaluetype:
		case fixedvaluetype:
			out->longvalue = host_to_disk_int32((int32_t) val->data.longvalue);
			break;
		case singlevaluetype: {
			union { float f; int32_t i; } u;
			u.f = val->data.singlevalue;
			out->longvalue = host_to_disk_int32(u.i);
			break;
		}
	default:
		break;
	}
}

static void diskvalue_from_value_v7(const tyvaluerecord *val, tydiskvaluedata_v7 *out) {
	clearbytes(out, sizeof(*out));
	switch (val->valuetype) {
	case novaluetype:
	case booleanvaluetype:
	case charvaluetype:
		/* Small scalar types share the 64-bit longvalue slot to keep the v7 record compact and simple. */
		out->longvalue = host_to_disk_int64((int64_t) val->data.chvalue);
		break;
		case intvaluetype:
		case tokenvaluetype:
			out->longvalue = host_to_disk_int64((int64_t) val->data.intvalue);
			break;
		case pointvaluetype:
			out->pointvalue.h = host_to_disk_int16((int16_t) val->data.pointvalue.h);
			out->pointvalue.v = host_to_disk_int16((int16_t) val->data.pointvalue.v);
			break;
		case directionvaluetype:
			out->dirvalue = host_to_disk_int32((int32_t) val->data.dirvalue);
			break;
		case datevaluetype:
			out->datevalue = (uint64_t) host_to_disk_int64((int64_t) val->data.datevalue);
			break;
		case longvaluetype:
		case ostypevaluetype:
		case enumvaluetype:
		case fixedvaluetype:
			out->longvalue = host_to_disk_int64((int64_t) val->data.longvalue);
			break;
		case singlevaluetype: {
			double d = (double) val->data.singlevalue; /* widen to double on disk */
			out->doublebits = host_to_disk_double_bits(d);
			break;
		}
		case doublevaluetype: {
			double d = **val->data.doublevalue;
			out->doublebits = host_to_disk_double_bits(d);
			break;
		}
		default:
			break;
	}
}

static void diskvalue_to_value_legacy(const tydiskvaluedata *disk, tyvaluerecord *val) {
	switch (val->valuetype) {
		case novaluetype:
			val->data.chvalue = disk->chvalue;
			break;
		case booleanvaluetype:
			val->data.flvalue = (disk->chvalue != 0);
			val->data.chvalue = disk->chvalue;
			break;
		case charvaluetype:
			val->data.chvalue = disk->chvalue;
			break;
		case intvaluetype:
		case tokenvaluetype:
			val->data.intvalue = disk_to_host_int16(disk->intvalue);
			break;
		case pointvaluetype:
			val->data.pointvalue.h = disk_to_host_int16(disk->pointvalue.h);
			val->data.pointvalue.v = disk_to_host_int16(disk->pointvalue.v);
			break;
		case directionvaluetype:
			val->data.dirvalue = disk_to_host_int32(disk->dirvalue);
			break;
		case datevaluetype:
			val->data.datevalue = (unsigned long) disk_to_host_int32(disk->datevalue);
			break;
		case longvaluetype:
		case ostypevaluetype:
		case enumvaluetype:
		case fixedvaluetype:
			val->data.longvalue = disk_to_host_int32(disk->longvalue);
			break;
		case singlevaluetype: {
			union { int32_t i; float f; } u;
			u.i = disk_to_host_int32(disk->longvalue);
			val->data.singlevalue = u.f;
			break;
		}
		default:
			break;
	}
}

static void diskvalue_to_value_v7(const tydiskvaluedata_v7 *disk, tyvaluerecord *val) {
	switch (val->valuetype) {
		case novaluetype:
			val->data.chvalue = (unsigned char) disk_to_host_int64((int64_t) disk->longvalue);
			break;
		case booleanvaluetype:
			val->data.flvalue = (disk_to_host_int64((int64_t) disk->longvalue) != 0);
			val->data.chvalue = (unsigned char) disk_to_host_int64((int64_t) disk->longvalue);
			break;
		case charvaluetype:
			val->data.chvalue = (unsigned char) disk_to_host_int64((int64_t) disk->longvalue);
			break;
		case intvaluetype:
		case tokenvaluetype:
			val->data.intvalue = (int64_t) disk_to_host_int64((int64_t) disk->longvalue);
			break;
		case pointvaluetype:
			val->data.pointvalue.h = disk_to_host_int16(disk->pointvalue.h);
			val->data.pointvalue.v = disk_to_host_int16(disk->pointvalue.v);
			break;
		case directionvaluetype:
			val->data.dirvalue = disk_to_host_int32(disk->dirvalue);
			break;
	case datevaluetype:
			val->data.datevalue = disk_to_host_int64((int64_t) disk->datevalue);
			break;
		case longvaluetype:
		case ostypevaluetype:
		case enumvaluetype:
		case fixedvaluetype:
			val->data.longvalue = disk_to_host_int64((int64_t) disk->longvalue);
			break;
		case singlevaluetype: {
			double d = disk_to_host_double_bits(disk->doublebits);
			val->data.singlevalue = (float) d;
			break;
		}
		case doublevaluetype: {
			double d = disk_to_host_double_bits(disk->doublebits);
			setdoublevalue (d, val);
			break;
		}
		default:
			break;
	}
}

hdlhashtable currenthashtable = nil;

hdltablestack hashtablestack = nil;

boolean fllanghashassignprotect = false;

boolean fllangexternalvalueprotect = false;	/*4.1b4 dmb: new global, disable protection*/


static boolean flunpackingtable = 0;

//static Handle h1; /*global for packing and unpacking routines -- holds the binary info*/

//static Handle h2; /*global for packing and unpacking routines -- holds the text info*/

static hdlhashnode hnewnode; /*global for hashinsertaddress*/

static boolean flexternalmemorypack = false;

static hdldatabaserecord hexternalpackdatabase;



static hdlhashtable hfirstfreetable = nil; /*private free list for hash tables*/


#ifdef fldebug

static long cthashtablesallocated = 0;

#endif




boolean newhashtable (hdlhashtable *htable) {
	
	/*
	all fields are initialized to 0, no initialization code needed here.
	
	9/23/91 dmb: now look for magic table.  this allows a table to be stuffed 
	full of values in a location that is removed from and/or independent of 
	our caller.  in particular, langfunccall can populate a locals table without 
	adding it to the local chain; evaluatelist picks up the table indirectly by 
	pushing a local chain, which eventually tries to allocate a hash table.
	
	5.0d15 dmb: preserve new cttmpstack field. We're assuming that the reused 
	table pool is mostly for local tables that actually need temp stacks.
	*/
	
	if (hmagictable != nil) {
		
		*htable = hmagictable;
		
		hmagictable = nil;
		
		return (true);
		}
	
	#ifdef fldebug
	
	++cthashtablesallocated;
	
	#endif
	
	if (hfirstfreetable != nil) { /*let's reuse one that's been allocated*/
		
		hdlhashtable ht = hfirstfreetable;
		short ct;
		
		hfirstfreetable = (**hfirstfreetable).prevhashtable;
		
		ct = (**ht).cttmpstack;
		
		clearhandle ((Handle) ht);
		
		(**ht).cttmpstack = ct;
		
		*htable = ht;
		
		return (true);
		}
	
	if (!newclearhandle (sizeof (tyhashtable), (Handle *) htable))
		return (false);
	
	(***htable).timecreated = timenow ();
	
	return (true);
	} /*newhashtable*/
	
	
short hashgetstackdepth (void) {
	
	/*
	if there are 12 tables in the current chain, return 12.
	*/
	
	register hdlhashtable x = currenthashtable;
	register short ct = 0;
	
	while (x != nil) {
		
		ct++;
		
		x = (**x).prevhashtable;
		} /*while*/
		
	return (ct);
	} /*hashgetstackdepth*/


void chainhashtable (hdlhashtable htable) {
	
	/*
	chain and unchain implement a stack of symbol tables.  the newest table is
	pointed to by currenthashtable, a global.  the global symbol table, the last in
	the list, points to nil.
	*/
	
	register hdlhashtable ht = htable;
	
	(**ht).prevhashtable = currenthashtable;
	
	(**ht).flchained = true;
	
	currenthashtable = ht;
	} /*chainhashtable*/


void unchainhashtable (void) {
	
	/*
	5.1.5b14 dmb: reset prevhashtable to nil
	*/
	
	register hdlhashtable ht = currenthashtable;
	register hdlhashtable hprev = (**ht).prevhashtable;
	
	(**ht).prevhashtable = nil;
	
	(**ht).flchained = false;
	
	currenthashtable = hprev;
	} /*unchainhashtable*/


	

hdlhashtable sethashtable (hdlhashtable hset) {
	
	/*
	5.0.2b6 dmb: utility routine for pushing using local storage
	*/
	
	hdlhashtable hprev = currenthashtable;
	
	currenthashtable = hset;
	
	return (hprev);
	} /*sethashtable*/


boolean pushhashtable (hdlhashtable h) {
	
	/*
	5.1.2 dmb: handle nil hs instead of asserting that it's not, If it's nil,
	the process globals have been disposed.
	*/
	
	register hdltablestack hs = hashtablestack;
	
	if (hs == nil)
		return (false);
	
	if (!langcheckstacklimit (idtablestack, (**hs).toptables, cthashtables)) /*overflow!*/
		return (false);
	
	(**hs).stack [(**hs).toptables++] = currenthashtable;
	
	currenthashtable = h;
	
	/*stacktracer (toptables);*/
	
	return (true);
	} /*pushhashtable*/


boolean pophashtable (void) {
	
	register hdltablestack hs = hashtablestack;
	
	if ((**hs).toptables <= 0) {
		
		shellinternalerror (idtoomanypophashtables, STR_too_many_pophashtables);
		
		return (false);
		}
	
	currenthashtable = (**hs).stack [--(**hs).toptables];
	
	/*stacktracer (toptables);*/
	
	return (true);
	} /*pophashtable*/


boolean pushouterlocaltable (void) {
	
	/*
	push the local table with the most global scope -- the only table that 
	is global to the current process, but unique to it.
	*/
	
	register hdlhashtable ht = currenthashtable;
	register hdlhashtable hprev;
	
	assert (ht != nil);
	
	assert ((**ht).fllocaltable); /*current hash table should be a local table*/
	
	while (true) {
		
		hprev = (**ht).prevhashtable;
		
		if ((hprev == nil) || !(**hprev).fllocaltable)
			return (pushhashtable (ht));
		
		ht = hprev;
		} /*while*/
	} /*pushouterlocaltable*/



#ifdef smartmemory


#include "tableverbs.h"

/******/

static boolean hashtablevisitall (hdlhashtable htable, boolean (*visit) (hdlhashnode)) {
	
	/*
	7/25/92 dmb: weird version of table visitation needed for purging tables:
	
	always visit all kids; return true if all kids return true, else false
	*/
	
	register hdlhashnode x;
	register short i;
	register boolean fl = true;
	
	for (i = 0; i < ctbuckets; i++) {
		
		x = (**htable).hashbucket [i];
		
		while (x != nil) {
			
			hdlhashnode nextx = (**x).hashlink;
			
			if (!(*visit) (x)) 
				fl = false;
			
			x = nextx;
			} /*while*/
		} /*for*/
	
	return (fl);
	} /*hashtablevisitall*/	


static boolean checkaddressvisit (hdlhashnode hnode) {
	
	/*
	7/25/92 dmb: weird version of table visitation needed for purging tables:
	
	always visit all kids; return true if all kids return true, else false
	*/
	
	register hdlexternalvariable hv;
	tyvaluerecord val;
	hdlhashtable htable;
	bigstring bs;
	hdltreenode hcode;
	langerrorcallback errorcallback;
	
	val = (**hnode).val;
	
	switch (val.valuetype) {
		
		case addressvaluetype:
			getaddressvalue (val, &htable, bs);
			
			(**htable).flnopurge = true;
			
			break;
		
		case externalvaluetype:
			hv = (hdlexternalvariable) val.data.externalvalue;
			
			if (!(**hv).flinmemory)
				break;
			
			if (langexternalvaltotable (val, &htable)) {
				
				hashtablevisitall (htable, checkaddressvisit);
				
				break;
				}
			
			if (langexternalvaltocode (val, &hcode)) {
				
				if (langfinderrorrefcon ((long) hnode, &errorcallback)) /***/
					(**htable).flnopurge = true;
				
				break;
				}
			
			break;
		}
	
	return (true);
	} /*checkaddressvisit*/	


static boolean purgetablevisit (hdlhashnode hnode) {
	
	/*
	7/25/92 dmb: weird version of table visitation needed for purging tables:
	
	always visit all kids; return true if all kids return true, else false
	*/
	
	register hdlexternalvariable hv;
	register hdlhashtable ht;
	tyvaluerecord val;
	hdlhashtable htable;
	boolean flnopurge;
	
	val = (**hnode).val;
	
	if (val.valuetype != externalvaluetype) 
		return (true);
	
	hv = (hdlexternalvariable) val.data.externalvalue;
	
	if (!(**hv).flinmemory)
		return (true);
	
	if (!langexternalvaltotable (val, &htable))
		return (true);
	
	ht = htable;
	
	flnopurge = (**ht).flnopurge;
	
	(**ht).flnopurge = false; /*must reset every time*/
	
	if ((**ht).fldirty)
		return (false);
	
	
	if ((**ht).flwindowopen)
		return (false);
	
	
	assert (!(**ht).fllocaltable);
	
	if ((**hnode).fldontsave)
		return (false);
	
	if (!hashtablevisitall (ht, purgetablevisit))
		return (false);
	
	if ((**ht).fllocked)
		return (false);
	
	if (flnopurge)
		return (false);
	
	tableverbunload (hv);
	
	return (true);
	} /*purgetablevisit*/	

#endif

boolean hashflushcache (long *ctbytesneeded) {
	
	register hdlhashtable hfreetable;
	
	#ifdef smartmemory
	
	hashtablevisitall (roottable, checkaddressvisit);
	
	hashtablevisitall (roottable, purgetablevisit);
	
	#endif
	
	while (hfirstfreetable != nil) {
		
		hfreetable = hfirstfreetable;
		
		hfirstfreetable = (**hfreetable).prevhashtable;
		
		*ctbytesneeded -= gethandlesize ((Handle) hfreetable);
		
		disposehandle ((Handle) hfreetable);
		}
	
	return (true);
	} /*hashflushcache*/


boolean disposehashnode (hdlhashtable ht, hdlhashnode hnode, boolean fldisposevalue, boolean fldisk) {
	
	/*
	5.1.4 dmb: take htable parameter for database setting for disk scalars
	
	2003-05-22 AR: Call hashunregisteraddressnode at the lowest level
	possible to ensure that we don't encounter invalid hdlhashnodes
	later when we dispose a hashtable.
	*/

	register hdlhashnode hn = hnode;
	
	/*
	if ((**hn).ctlocks > 0) {
		
		(**hn).fldisposewhenunlocked = true;
		
		return (false);
		}
	*/


	if (fldisposevalue) {
		
		boolean flneeddatabase = (fldisk && (**hn).val.fldiskval);
		hdldatabaserecord hdb = nil;

		if (flneeddatabase) {
			
			hdb = tablegetdatabase (ht);

			if (hdb)
				dbpushdatabase (hdb);
			}

		disposevaluerecord ((**hn).val, fldisk);
		
		if (flneeddatabase && hdb)
			dbpopdatabase ();
		}

	disposehandle ((Handle) hn);
	
	return (true);
	} /*disposehashnode*/


void dirtyhashtable (hdlhashtable ht) {
	
	(**ht).fldirty = true;
	
	(**ht).timelastsave = timenow ();
	} /*dirtyhashtable*/

	
static short smashhashtable (hdlhashtable htable, boolean fldisk, boolean flcallback) {
	
	/*
	4.0b7 4/25/96 dmb: pulled this code out of disposehashtable
	so we could make a verb out of it. had to add the flcallback parameter,
	since disposehashtable doesn't want to.
	*/
	
	register hdlhashtable ht = htable;
	register hdlhashnode nomad, nextnomad;
	register short i;
	short ctdisposed = 0;
	bigstring bs;
	
	if (ht == nil) /*easy to dispose of nil table*/
		return (0);
	
	(**ht).hfirstsort = nil;	/*disconnect now so table is valid during disposal*/
	
	for (i = 0; i < ctbuckets; i++) {
		
		nomad = (**ht).hashbucket [i];
		
		(**ht).hashbucket [i] = nil; /*disconnect list so table is valid during disposal*/
		
		while (nomad != nil) {
			
			nextnomad = (**nomad).hashlink;
			
			if (flcallback)
				gethashkey (nomad, bs);
			
			if (flcallback)
				langsymbolunlinking (ht, nomad);
			
			disposehashnode (ht, nomad, true, fldisk);
			
			if (flcallback)
				langsymboldeleted (ht, bs);
			
			++ctdisposed;
			
			nomad = nextnomad;
			} /*while*/
		} /*for*/
	
	dirtyhashtable (ht);

	return (ctdisposed); 
	} /*smashhashtable*/


short emptyhashtable (hdlhashtable htable, boolean fldisk) {

	return (smashhashtable (htable, fldisk, true));
	} /*emptyhashtable*/

boolean disposehashtable (hdlhashtable htable, boolean fldisk) {
	
	/*
	7/10/90 DW: if it's a local table, don't dispose of any code trees linked
	in as values.
	
	1/8/90 dmb: check new flchained flag to postpone disposal
	
	6/10/92 dmb: disconnect bucket list during disposal so table remains valid
	
	9/24/92 dmb: removed special case for code node value disposal.
	disposevaluerecord now knows that it should never dispose code values
	*/
	
	register hdlhashtable ht = htable;
	
	if (ht == nil) /*easy to dispose of nil table*/
		return (true);
	
	#ifdef fldebug
	
	--cthashtablesallocated;
	
	#endif
	
	if (ht == roottable) { /*very serious internal error*/
	
		shellinternalerror (iddisposingsystemtable, STR_trying_to_dispose_global_symbol_table);
		
		return (false);
		}
	
	if ((**ht).flchained) { /*table is in local chain; can't dispose now*/
		
		(**ht).fldisposewhenunchained = true; /*we'll do it later*/
		
		return (true);
		}
	
	pushhashtable (ht);
	
	cleartmpstack ();
	
	pophashtable ();
	
	smashhashtable (ht, fldisk, false);
	/*
	for (i = 0; i < ctbuckets; i++) {
		
		nomad = (**ht).hashbucket [i];
		
		(**ht).hashbucket [i] = nil; /%disconnect list so table is valid during disposal%/
		
		while (nomad != nil) {
			
			/%
			boolean fldisposevalue;
			
			fldisposevalue = (!(**ht).fllocaltable) || ((**nomad).val.valuetype != codevaluetype);
			%/
			
			nextnomad = (**nomad).hashlink;
			
			disposehashnode (nomad, true /%fldisposevalue%/, fldisk);
			
			nomad = nextnomad;
			}
		}
	*/
	
	
	(**ht).prevhashtable = hfirstfreetable;
	
	hfirstfreetable = ht;
	
	/*
	disposehandle ((Handle) ht);
	*/
	
	return (true); 
	} /*disposehashtable*/


short hashfunction (const bigstring bs) {
	
	/*
		3.0.4b8 dmb: need to make locals unsigned to protect against ctype's int's
	*/

//	register unsigned short c;
	register unsigned short len;
//	register ptrstring p = (ptrstring) bs;
	register unsigned short val;

	len = stringlength (bs);
	
	if (len == 0)
		return (0);
	
//	c = p [1];
	
	val = getlower(getstringcharacter(bs,0));
	
//	c = p [len];
	
	val += getlower(getstringcharacter(bs,len-1));
	
	return (val % ctbuckets);
	} /*hashfunction*/


static boolean hashsortedinsert (hdlhashnode hnode) {

	register hdlhashnode hn = hnode;
	register hdlhashtable ht = currenthashtable;
	register hdlhashnode nomad = (**ht).hfirstsort;
	register hdlhashnode nomadprev = nil;
	short comparison;
	
	if (nomad == nil) { /*first guy in sorted list*/
		
		(**ht).hfirstsort = hn;
		
		(**hn).sortedlink = nil;
		
		return (true);
		}
	
	while (true) {
		
		comparison = (*langcallbacks.comparenodescallback) (ht, hn, nomad);
		
		if (comparison < 0) {
			
			if (nomadprev == nil) { /*he's the new first element*/	
				
				(**hn).sortedlink = (**ht).hfirstsort;
				
				(**ht).hfirstsort = hn;
				
				return (true);
				}
				
			(**hn).sortedlink = nomad; /*insert in before nomad, middle of list*/
			
			(**nomadprev).sortedlink = hn;
			
			return (true);
			}
		
		nomadprev = nomad; /*advance to next node in list*/
		
		nomad = (**nomad).sortedlink; /*advance to next node in sorted list*/
		
		if (nomad == nil) { /*insert at end of list*/
			
			(**hn).sortedlink = nil;
			
			(**nomadprev).sortedlink = hn;
			
			return (true);
			}
		} /*while*/
	} /*hashsortedinsert*/
	

static void hashsorteddelete (hdlhashnode hnodedelete) {
	
	register hdlhashtable htable = currenthashtable;
	register hdlhashnode nomad = (**htable).hfirstsort;
	register hdlhashnode nomadprev = nil;
	register hdlhashnode hnode = hnodedelete;
	
	while (nomad != nil) {
		
		if (nomad == hnode) {
			
			if (nomadprev == nil) { /*unlinking first in list*/
			
				(**htable).hfirstsort = (**nomad).sortedlink;
				
				return;
				}
				
			(**nomadprev).sortedlink = (**nomad).sortedlink;
			
			return;
			}
		
		nomadprev = nomad;
		
		nomad = (**nomad).sortedlink;
		} /*while*/
	} /*hashsorteddelete*/
	
	
static boolean hashlinknode (hdlhashtable htable, hdlhashnode hnode) {
	
	register hdlhashnode hn = hnode;
	register hdlhashtable ht = htable;
	register short ixbucket;
	register hdlhashnode hnext;
	
	ixbucket = hashfunction ((**hn).hashkey);
	
	hnext = (**ht).hashbucket [ixbucket];
	
	(**ht).hashbucket [ixbucket] = hnode; /*link new guy at head of list*/
	
	(**hn).hashlink = hnext;
	
	return (true);
	} /*hashlinknode*/
	

boolean hashinsertnode (hdlhashnode hnode, hdlhashtable htable) {
	
	/*
	3/23/93 dmb: don't invoke callback when flunpackingtable flag is set
	*/
	
	register hdlhashnode hn = hnode;
	register hdlhashtable ht = htable;
	bigstring bs;
	
	hashlinknode (ht, hn);
	
	if (flunpackingtable) /*tableunpack will take care of sort links*/
		return (true);
	
	pushhashtable (ht);
	
	hashsortedinsert (hn);
	
	pophashtable ();
	
	dirtyhashtable (ht);
	
	gethashkey (hn, bs);
	
	langsymbolinserted (ht, bs, hn);
	
	return (true);
	} /*hashinsertnode*/
	

boolean hashunlinknode (hdlhashtable htable, hdlhashnode hnode) {
	
	/*
	a sure-fire hash-algorithm-independent way to unlink a node.
	*/
	
	register short i;
	register hdlhashnode nomad, prev;
	
	for (i = 0; i < ctbuckets; i++) {
		
		nomad = (**htable).hashbucket [i];
		
		prev = nil;
		
		while (nomad != nil) {
			
			if (nomad == hnode) /*found it*/
				goto afterloop;
				
			prev = nomad;
			
			nomad = (**nomad).hashlink;
			} /*while*/
		} /*for*/
	
	return (false); /*not found*/
	
	afterloop:
	
	if (prev == nil) 
		(**htable).hashbucket [i] = (**nomad).hashlink;
	else
		(**prev).hashlink = (**nomad).hashlink;
	
	return (true);
	} /*hashunlinknode*/


boolean hashsetnodekey (hdlhashtable htable, hdlhashnode hnode, const bigstring bs) {
	
	if (!sethandlesize ((Handle) hnode, sizeof (tyhashnode) + stringsize (bs)))
		return (false);
	
	hashunlinknode (htable, hnode);
	
	copystring (bs, (**hnode).hashkey);
	
	hashlinknode (htable, hnode);
	
	(**htable).flneedsort = true;
	
	langsymbolchanged (htable, bs, hnode, false); /*value didn't change*/
	
	return (true);
	} /*hashsetnodekey*/


static boolean newhashnode (hdlhashnode *hnode, const bigstring bskey) {
	
	if (!newclearhandle (sizeof (tyhashnode) + stringsize (bskey), (Handle *) hnode))
		return (false);
	
	copystring (bskey, (***hnode).hashkey);
	
	return (true);
	} /*newhashnode*/


boolean hashinsert (const bigstring bs, tyvaluerecord val) {
	
	/*
	5.0.2b10 dmb: make sure we don't put a value with the tmp flag set.
	*/
	
	register hdlhashtable ht = currenthashtable;
	register hdlhashnode h;
	hdlhashnode hnode;
	
	if (isemptystring (bs)) {
		bigstring bspath;
		
		langexternalgetfullpath (currenthashtable, (ptrstring) bs, bspath, nil);
		
		lang2paramerror (illegalnameerror, bspath, bs);
		
		return (false);
		}
	
	if (!newhashnode (&hnode, bs))
		return (false);
	
	h = hnode; /*copy into register*/
	
	hnewnode = h; /*copy into global for hashinsertaddress*/
	
	val.fltmpstack = false; // 5.0.2: caller is responsible for actually removing it
	
	(**h).val = val;
	
	hashinsertnode (h, ht);
	
	return (true);
	} /*hashinsert*/
	

/*
hashmerge (hdlhashtable hsource, hdlhashtable hdest) {
	
	/%
	merge hsource into hdest, leaving hsource empty.  since it consumes 
	no memory (we just unlink nodes and deposit them) it can't fail.
	%/
	
	register short i;
	
	for (i = 0; i < ctbuckets; i++) {
		
		register hdlhashnode x;
		
		x = (**hsource).hashbucket [i];
		
		while (x != nil) { /%chain through the hash list%/
			
			register hdlhashnode nextx; 
			
			nextx = (**x).hashlink;
			
			hashinsertnode (x, hdest);
			
			x = nextx;
			} /%while%/
			
		(**hsource).hashbucket [i] = nil; /%we leave the source table empty%/
		} /%for%/
	} /%hashmerge%/
*/

/** 2/7/91 dmb: new implementation of array references resolves them
	immediately, so we don't have to handlel them here

boolean hashlocatearray (short arrayindex, hdlhashnode *hnode, hdlhashnode *hprev) {
	
	tyvaluerecord val;
	bigstring bsvarname;
	
	if (!hashgetiteminfo (currenthashtable, arrayindex - 1, bsvarname, &val)) {
		
		langlongparamerror (tabletoosmallerror, (long) arrayindex);
		
		return (false);
		}
		
	return (hashlocate (bsvarname, hnode, hprev));
	} /%hashlocatearray%/

	
boolean hashstringtoarrayindex (bigstring bs, short *arrayindex) {

	bigstring bscopy;
		
	if (stringlength (bs) == 0) /%empty names not allowed, defensive driving%/
		return (false);
	
	if (bs [1] != '$') 
		return (false);
		
	copystring (bs, bscopy);
		
	deletefirstchar (bscopy);
		
	if (!stringtoshort (bscopy, arrayindex)) {
			
		langparamerror (badindexname, bs);
			
		return (false);
		}
	
	return (true);
	} /%hashstringtoarrayindex%/
*/

boolean hashlocate (const bigstring bs, hdlhashnode *hnode, hdlhashnode *hprev) {

	/*
	7/15/90 DW: add support for table array-style references.  if the string
	begins with a $, we return the node and prev for the nth guy in the sorted
	list of the table.
	*/
	
	register short ixbucket;
	register hdlhashnode nomad, nomadprev;
	
	/*
	short arrayindex;
	
	if (hashstringtoarrayindex (bs, &arrayindex)) {
	
		return (hashlocatearray (arrayindex, hnode, hprev));
		}
	*/
	
	ixbucket = hashfunction (bs);

	//assert (currenthashtable != nil);

	//assert (validhandle ((Handle) currenthashtable));
	
	nomad = (**currenthashtable).hashbucket [ixbucket];
	
	nomadprev = nil;
	
	while (nomad != nil) {
		
		if (equalidentifiers (bs, (**nomad).hashkey)) {
		
			*hnode = nomad;
			
			*hprev = nomadprev;
			
			return (true);
			}
		
		nomadprev = nomad;
		
		nomad = (**nomad).hashlink;
		} /*while*/
		
	return (false); /*loop terminated, not found*/
	} /*hashlocate*/


boolean hashunlink (const bigstring bs, hdlhashnode *hnode) {
	
	hdlhashnode hprev;
	register hdlhashnode hn;
	
	if (!hashlocate (bs, hnode, &hprev)) {
	
		langparamerror (cantdeleteerror, bs);
		
		return (false);
		}
	
	hn = *hnode; /*copy into register*/
	
	langsymbolunlinking (currenthashtable, hn);
	
	if (hprev == nil) 
		(**currenthashtable).hashbucket [hashfunction (bs)] = (**hn).hashlink;
	else 
		(**hprev).hashlink = (**hn).hashlink;
	
	hashsorteddelete (hn);
	
	dirtyhashtable (currenthashtable);
	
	langsymboldeleted (currenthashtable, bs);
	
	return (true);
	} /*hashunlink*/

/*
boolean hashdeletenode (hdlhashnode *hnode) {
	
	hdlhashnode hprev;
	register hdlhashnode hn;
	
	hashunlinknode (currenthashtable, hnode);
	
	hashsorteddelete (hnode);
	
	dirtyhashtable (currenthashtable);
	
	return (true);
	} /%hashdeletenode%/
*/


boolean hashdelete (const bigstring bs, boolean fldisposevalue, boolean fldisk) {
	
	hdlhashnode hnode, hprev;
	register hdlhashnode hn;
	
	if (!hashlocate (bs, &hnode, &hprev)) {
	
		langparamerror (cantdeleteerror, bs);
		
		return (false);
		}
	
	hn = hnode; /*copy into register*/
	
	langsymbolunlinking (currenthashtable, hn);
	
	if (hprev == nil) 
		(**currenthashtable).hashbucket [hashfunction (bs)] = (**hn).hashlink;
	else 
		(**hprev).hashlink = (**hn).hashlink;
	
	hashsorteddelete (hn);
	
	disposehashnode (currenthashtable, hn, fldisposevalue, fldisk);
	
	dirtyhashtable (currenthashtable);
	
	langsymboldeleted (currenthashtable, bs);
	
	return (true);
	} /*hashdelete*/


boolean hashtabledelete (hdlhashtable htable, bigstring bs) {
	
	boolean fl;

	if (!pushhashtable (htable))
		return (false);
	
	fl = hashdelete (bs, true, true);
	
	pophashtable ();

	return (fl);
	} /*hashtabledelete*/


boolean hashsymbolexists (const bigstring bs) {
	
	hdlhashnode hnode, hprev;
	
	return (hashlocate (bs, &hnode, &hprev));
	} /*hashsymbolexists*/


boolean hashtablesymbolexists (hdlhashtable htable, const bigstring bs) {
	
	boolean fl;
	
	pushhashtable (htable);
	
	fl = hashsymbolexists (bs);
	
	pophashtable ();
	
	return (fl);
	} /*hashtablesymbolexists*/


typedef struct localityinfo {
	
	boolean fllocal;

	hdldatabaserecord hdb;
	} tylocalityinfo, *ptrlocalityinfo;


static boolean hashsetlocalityvisit (hdlhashnode hnode, ptrvoid refcon) {
	
	hdlhashtable ht;
	hdlexternalvariable hv;
	tyvaluerecord val = (**hnode).val;
	ptrlocalityinfo info = (ptrlocalityinfo) refcon;
	
	if (val.valuetype == externalvaluetype) {
		
		hv = (hdlexternalvariable) val.data.externalvalue;
		
		if (currenthashtable != filewindowtable)
			langexternalsetdatabase (hv, (*info).hdb);
		
		if ((**hv).flinmemory && langexternalvaltotable (val, &ht, hnode)) {
			
			(**ht).fllocaltable = (*info).fllocal;
			
			hashtablevisit (ht, &hashsetlocalityvisit, info);
			}
		}
	
	return (true); /*always continue traversal*/
	} /*hashsetlocalityvisit*/


void hashsetlocality (tyvaluerecord *val, boolean fllocal) {
	
	/*
	5.0.2b10 dmb: new routine. when we assign a table value to a local
	table, it and all of its subtables must be local too. Or the converse.
	
	5.0.2b13 dmb: set the table's parent link. we now maintain it strictly.
	
	5.1.4 dmb: deal with database ownership for newly-created externals

	encode address value according to locality
	
	6.2b16 AR: No longer static so it can be called externally (langaddlocals, langevaluate.c)
	*/
	
	hdlhashtable ht;
	hdlexternalvariable hv;
	
	tylocalityinfo info;

	switch ((*val).valuetype) {
		
		case addressvaluetype:

			disablelangerror (); /*08/02/2000 AR*/
			
			setaddressencoding (val, !fllocal);

			enablelangerror ();

			break;
	
		case externalvaluetype:
			
			hv = (hdlexternalvariable) (*val).data.externalvalue;
			
			info.fllocal = fllocal;
			
			if (currenthashtable != filewindowtable) {
				
				info.hdb = tablegetdatabase (currenthashtable);
				
				langexternalsetdatabase (hv, info.hdb);
				}
			
			if (!(**hv).flinmemory || !langexternalvaltotable (*val, &ht, nil))
				break;
			
			(**ht).parenthashtable = currenthashtable;
			
			(**ht).fllocaltable = fllocal;
			
			hashtablevisit (ht, &hashsetlocalityvisit, &info);

			break;

		default:
			break;
		}
	} /*hashsetlocality*/


boolean hashassign (const bigstring bs, tyvaluerecord val) {
	
	/*
	9/23/91 dmb: no longer clear fllangerror, or look at it when 
	hashlocate returns false.  array references are implemented differently 
	now, and hashlocate never generates errors.  clearing fllangerror can 
	have the side effect of hiding an error condition unexpectedly.
	
	5.0b17 dmb: if we're assigning a tmp external, claim the data like 
	a normal tmp. don't copy the data, clean fltmpdata instead. really, our
	caller should be exempting from the tmp stack, but this close to shipping
	let's not assume more than we have to

	5.0.1b1 dmb: the b17 change broke stuff, because the object may be in 
	another table's temp stack. Our caller is responsible for exempting 
	anything assinged into a table. we just need to make sure that the 
	fltmpstack flag is clear for _any_ object we assign to a hashnode

	5.0.1b2 dmb: when disposing a value, set fldisk false for local table items
	
	5.0.2b13 dmb: set fltmpdata false & call hashsetlocality before hashinsert case
	*/
	
	hdlhashnode hnode, hprev;
	tyvaluerecord existingval;
	boolean fllocal = (**currenthashtable).fllocaltable;
	
	/*
	fllangerror = false;
	*/
	
	if (val.fltmpdata) { /*val doesn't own it's data*/
		
		if (val.fltmpstack)
			val.fltmpdata = false;
		else
			if (!copyvaluedata (&val))
				return (false);
		}
	
	val.fltmpstack = false; // 5.0.1: caller is responsible for actually removing it
	
	//if (val.valuetype == externalvaluetype) // 5.0.2: localness of tables must match parent
		hashsetlocality (&val, fllocal);
	
	if (!hashlocate (bs, &hnode, &hprev)) { /*the name doesn't exist or is invalid*/
		
		/*just an undefined variable*/
		
		return (hashinsert (bs, val));
		}
	
	existingval = (**hnode).val;
	
	if (fllanghashassignprotect) { /*protect externals from being smashed by assignment*/
		
		if ((existingval.valuetype == externalvaluetype) && (val.valuetype != externalvaluetype)) {
			bigstring bstype;
			
			langexternaltypestring ((hdlexternalhandle) existingval.data.externalvalue, bstype);
			
			lang2paramerror (badexternalassignmenterror, bstype, bs);
			
			return (false);
			}
		}
	
	/*carefully nuke existing value*/ {
		
		boolean flneeddatabase = (!fllocal && existingval.fldiskval);
		hdldatabaserecord hdb = nil;

		if (flneeddatabase) {
			
			hdb = tablegetdatabase (currenthashtable);

			if (hdb)
				dbpushdatabase (hdb);
			}

		disposevaluerecord (existingval, !fllocal);
		
		if (flneeddatabase && hdb)
			dbpopdatabase ();
		}
	
	(**hnode).val = val;
	
	langsymbolchanged (currenthashtable, bs, hnode, true); /*value changed*/
	
	return (true);
	} /*hashassign*/


boolean hashtableassign (hdlhashtable htable, const bigstring bs, tyvaluerecord val) {
	
	boolean fl;
	
	pushhashtable (htable);
	
	fl = hashassign (bs, val);
	
	pophashtable ();
	
	return (fl);
	} /*hashtableassign*/


boolean hashresolvevalue (hdlhashtable htable, hdlhashnode hnode) {
	
	/*
	3/19/92 dmb: try to resolve an address -- it hasn't been referenced since 
	it was unpacked.
	
	4.0.2b1 dmb: handle disk-based scalar values. load the value and release
	the dbaddress. added htable parameter so we can potentially dirty it 
	
	5.0a23 dmb: on address resolution failure, reset flunresolvedaddress to true
	
	5.0b7 dmb: don't set flunresolvedaddress to true on failure. It breaks 
	the table display. don't know why exactly.

	5.1.4 dmb: no longer resolve addresses automatically. It's now a valid state.
	Exception: the paths table needs high-performance address access

	5.1.4 dmb: dbpushreleasestack must be while database is pushed
	*/
	
	register hdlhashnode hn = hnode;
	boolean fl;
	
	if (htable == pathstable && (**hn).flunresolvedaddress) {

		(**hn).flunresolvedaddress = false; /*clear now to avoid potential recursion*/
		
		lockhandle ((Handle) hn); /*08/02/2000 AR: so it's safe to pass &(**hn).val to setaddressencoding*/
		
		disablelangerror ();
		
		fl = setaddressencoding (&(**hn).val, false);
		
		enablelangerror ();
	
		unlockhandle ((Handle) hn);
		
		if (!fl) {
#if defined(FRONTIER_HEADLESS)
			bigstring bspathtemp;
			copyheapstring ((hdlstring) (**hn).val.data.addressvalue, bspathtemp);
			log_debug(LOG_COMP_HASH, "hashresolvevalue: failed to encode path entry %s", stringbaseaddress (bspathtemp));
#endif
			return (false);
        }
#if defined(FRONTIER_HEADLESS)
		else {
			bigstring bspathtemp;
			hdlhashtable hresolved = nil;
			if (getaddressvalue ((**hn).val, &hresolved, bspathtemp)) {
				log_debug(LOG_COMP_HASH, "hashresolvevalue: resolved %s -> table=%p", stringbaseaddress (bspathtemp), (void *) hresolved);
			}
		}
#endif
		}

	if ((**hn).val.fldiskval) {
		Handle hbinary;
		hdldatabaserecord hdb = tablegetdatabase (htable);
		
		if (hdb)
			dbpushdatabase (hdb);
		
		fl = dbrefhandle ((**hn).val.data.diskvalue, &hbinary);
		
		if (fl)
			dbpushreleasestack ((**hn).val.data.diskvalue, (long) langgettype ((**hn).val));
		
		if (hdb)
			dbpopdatabase ();
		
		if (!fl)
			return (false);
		
		(**htable).fldirty = true;  /*dmb 6/18/96: we released disk value, must force table to be resaved*/
		
		(**hn).val.data.binaryvalue = hbinary;
		
		(**hn).val.fldiskval = false;
		}
	
	return (true);
	} /*hashresolvevalue*/


boolean hashlookup (const bigstring bs, tyvaluerecord *vreturned, hdlhashnode *hnode) {
	
	/*
	3/19/92 dmb: must check for unresolved addresses here
	*/
	
	hdlhashnode hprev;
	
	if (!hashlocate (bs, hnode, &hprev)) 
		return (false);
	
	if (!hashresolvevalue (currenthashtable, *hnode))
		return (false);
	
	*vreturned = (***hnode).val;
	
	return (true);
	} /*hashlookup*/
	

boolean hashtablelookup (hdlhashtable htable, const bigstring bs, tyvaluerecord *vreturned, hdlhashnode *hnode) {
	
	boolean fl;

	if (htable == nil)	/*8.0b48 PBS: it *is* nil sometimes. Too many callers would have to check it,*/
		return (false);	/*so the check is done here.*/
	
	
	pushhashtable (htable);
	
	fl = hashlookup (bs, vreturned, hnode);
	
	pophashtable ();
	
	return (fl);
	} /*hashtablelookup*/


boolean hashlookupnode (const bigstring bs, hdlhashnode *hnode) {
	
	/*
	3/19/92 dmb: must check for unresolved addresses here
	*/
	
	hdlhashnode hprev;
	
	if (!hashlocate (bs, hnode, &hprev))
		return (false);
	
	return (hashresolvevalue (currenthashtable, *hnode));
	} /*hashlookupnode*/


boolean hashtablelookupnode (hdlhashtable htable, const bigstring bs, hdlhashnode *hnode) {
	
	boolean fl;
	
	pushhashtable (htable);
	
	fl = hashlookupnode (bs, hnode);
	
	pophashtable ();
	
	return (fl);
	} /*hashtablelookupnode*/


static boolean hashinsertaddress (bigstring bsname, bigstring bsval) {
	
	/*
	3/19/92 dmb: discovered critical bug: if we try to resolve address references 
	here, using langexpandtodotparams, an address that references the table 
	being unpacked will generate infinite recursion.  That answer is to leave 
	the address in its string format for now, and then resolve the address when 
	it's referenced through a hashlookup.  this necessitated adding a new flag 
	to the hashrecord, and introducing the hnewnode global so we know what node 
	was created by hashinsert.  also, to preserve the original path information 
	and coordinate with getaddressvalue, we adopted a new convention of using 
	a hashtable of -1 to indicate an unresvoled address value.
	*/
	
	tyvaluerecord val;
	
	if (!setaddressvalue ((hdlhashtable) -1, bsval, &val))
		return (false);
	
	if (!hashinsert (bsname, val))
		return (false);
	
	exemptfromtmpstack (&val);
	
	(**hnewnode).flunresolvedaddress = true;
	
	return (true);
	} /*hashinsertaddress*/


/*
static boolean hashinsertaddress (bigstring bsname, bigstring bsval) {
	
	tyvaluerecord val;
	hdlhashtable htable;
	bigstring bs;
	boolean fl;
	
	pushhashtable (roottable);
	
	disablelangerror ();
	
	if (langexpandtodotparams (bsval, &htable, bs))
		fl = setaddressvalue (htable, bs, &val);
	else
		fl = setstringvalue (bsval, &val);
	
	enablelangerror ();
	
	pophashtable ();
	
	if (!fl)
		return (false);
	
	if (!hashinsert (bsname, val))
		return (false);
	
	pushhashtable (roottable);
	
	exemptfromtmpstack (val);
	
	pophashtable ();
	
	return (true);
	} /%hashinsertaddress%/
*/


boolean hashtablevisit (hdlhashtable htable, langtablevisitcallback visit, ptrvoid refcon) {
	
	/*
	###4.0.2b1 warning: scalar node values may now be on disk. callers that may 
	be examining strings values must handle this. (currently these are no such callers.)
	*/
	
	register hdlhashnode x;
	register short i;
	
	for (i = 0; i < ctbuckets; i++) {
		
		x = (**htable).hashbucket [i];
		
		while (x != nil) {
			
			hdlhashnode nextx = (**x).hashlink;
			
			if (!(*visit) (x, refcon)) 
				return (false);
				
			x = nextx;
			} /*while*/
		} /*for*/
	
	return (true);
	} /*hashtablevisit*/


static int hashcompare (const void *h1, const void *h2) {
	
	return ((*langcallbacks.comparenodescallback) (currenthashtable, *(hdlhashnode *)h1, *(hdlhashnode *)h2));
	} /*hashcompare*/


static boolean hashquicksort (hdlhashtable htable) {
	
	/*
	3/31/93 dmb: re-sort the indicated hashtable, using the standard c 
	library quicksort routine. this is really fast in general, but note 
	that it's worst-case performance is an already-sorted list.
	*/
	
	register hdlhashtable ht = htable;
	long ctitems;
	Handle hlist;
	register hdlhashnode h;
	
	hashcountitems (ht, &ctitems);
	
	if (ctitems == 0)
		return (true);
	
	if (!newhandle (ctitems * sizeof (hdlhashnode), &hlist))
		return (false);
	
	lockhandle (hlist);
	
	/*populate the array*/ {
		
		register hdlhashnode *p = (hdlhashnode *) *hlist;
		
		for (h = (**ht).hfirstsort; h != nil; h = (**h).sortedlink)
			*p++ = h;
		}
	
	/*sort it*/ {
		
		pushhashtable (ht);
		
		
			qsort (*hlist, ctitems, sizeof (hdlhashnode), &hashcompare);
		
		
		pophashtable ();
		}
	
	/*link the list*/ {
		
		register hdlhashnode *p = (hdlhashnode *) *hlist;
		
		(**ht).hfirstsort = *p;
		
		while (--ctitems > 0) {
			
			(***p).sortedlink = *(p + 1);
			
			++p;
			}
		
		(***p).sortedlink = nil;
		}
	
	unlockhandle (hlist);

	disposehandle (hlist);
	
	(**ht).flneedsort = false;
	
	return (true);
	} /*hashquicksort*/


boolean hashresort (hdlhashtable htable, hdlhashnode hresort) {
	
	/*
	re-sort the indicated hashtable.  first empty out the sorted list, then 
	visit every node in the table re-inserting it into the sorted list.
	
	3/31/93 dmb: added hresort parameter. if it's not nil, then only that 
	node needs to be resorted. using the quicksort routine, it's especially 
	important not to resort a mostly-sorted list just to move one node.
	*/
	
	register hdlhashtable ht = htable;
	
	if (hresort == nil)
		return (hashquicksort (ht));
	
	pushhashtable (ht);
	
	hashsorteddelete (hresort);
	
	hashsortedinsert (hresort);
	
	pophashtable ();
	
	(**ht).flneedsort = false; // this may be a bug. we don't know if this was only unsorted node
	
	return (true);
	} /*hashresort*/


boolean hashinversesearch (hdlhashtable htable, langinversesearchcallback visit, ptrvoid refcon, bigstring bsname) {

	/*
	perform a relatively slow, content-based search.
	
	we call the visit routine for every node in the current hashtable.
	
	if he returns false, we keep going -- he hasn't found the thing he's looking
	for yet.  if true, we return with the value of the node we stopped on.
	
	we return false if the visit routine never returns true.
	
	###4.0.2b1 warning: scalar node values may now be on disk. callers that may 
	be examining strings values must handle this. (currently these are no such callers.)
	*/
	
	register hdlhashnode nomad;
	register short i;
	
	for (i = 0; i < ctbuckets; i++) {
		
		nomad = (**htable).hashbucket [i];
		
		while (nomad != nil) {
		
			gethashkey (nomad, bsname);
				
			if ((*visit) (bsname, nomad, (**nomad).val, refcon)) /*search is over*/
				return (true);
				
			nomad = (**nomad).hashlink;
			} /*while*/
		} /*for*/
	
	setemptystring (bsname);
	
	return (false); /*never found the node he wanted*/
	} /*hashinversesearch*/


boolean hashsortedinversesearch (hdlhashtable htable, langsortedinversesearchcallback visit, ptrvoid refcon) {

	/*
	like hashinversesearch, except that items are visited in the current 
	sort order
	
	we return false if the visit routine never returns true.
	
	###4.0.2b1 warning: scalar node values may now be on disk. the value 
	that we pass to the visit routine may be unresolved. callers that may 
	be examining strings must handle this. (currently these are langipcgetparamvisit,
	tablefind, and tableverbpacktotext

	*/
	
	register hdlhashnode nomad = (**htable).hfirstsort;
	bigstring bsname;
	
	while (nomad != nil) {
		
		gethashkey (nomad, bsname);
		
		if ((*visit) (bsname, nomad, (**nomad).val, refcon)) /*search is over*/
			return (true);
		
		nomad = (**nomad).sortedlink;
		} /*while*/
	
	return (false); /*never found the node he wanted*/
	} /*hashsortedinversesearch*/


static boolean nodeintablevisit (hdlhashnode hnode, ptrvoid refcon) {
	
	return (hnode != (hdlhashnode) refcon); /*false terminates traversal*/
	} /*nodeintablevisit*/


boolean hashnodeintable (hdlhashnode hnode, hdlhashtable htable) {
	
	/*
	search the indicated table for the node, return true if we find it.
	*/
	
	if (htable == nil) // watch your back!
		return (false);
	
	return (!hashtablevisit (htable, &nodeintablevisit, hnode));
	} /*hashnodeintable*/


static boolean hashpackstring (handlestream *s, bigstring bs, int32_t *ix) {
	if (ix == NULL)
		return (false);
	if ((*s).pos > INT32_MAX)
		return (false);
	if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_HASH)) {
		log_trace(LOG_COMP_HASH, "[packstring] pos=%ld str=%.*s", (*s).pos, (int)bs[0], bs+1);
	}
	*ix = (int32_t)(*s).pos;

	return (writehandlestream (s, (ptrvoid) bs, (long) stringsize (bs)));
	} /*hashpackstring*/


static void hashunpackstring (Handle hget, bigstring bs, long ix) {
	
	/*
	5.0.1 rab: p must point to unsigned
	*/

	register ptrbyte p;

	/* Defensive: ensure ix is within handle before copying. */
	long hsize = gethandlesize (hget);
	if (ix < 0 || ix >= hsize) {
		log_error(LOG_COMP_HASH, "hashunpackstring OOB ix=%ld hsize=%ld", ix, hsize);
		setemptystring (bs);
		return;
	}

	p = (ptrbyte)(*hget + ix);

	/* p[0] is length byte; ensure we don't read past handle. */
	unsigned long needed = (unsigned long) p[0] + 1;
	if (ix + (long) needed > hsize) {
		log_error(LOG_COMP_HASH, "hashunpackstring len OOB ix=%ld len=%lu hsize=%ld", ix, needed, hsize);
		setemptystring (bs);
		return;
	}

	moveleft (p, bs, needed);
	} /*hashunpackstring*/


static boolean hashpackdata (handlestream *s, const void *pdata, long ctbytes, int32_t *ix) {
	if (ix == NULL)
		return (false);
	if ((*s).pos > INT32_MAX)
		return (false);
	if ((ctbytes < 0) || (ctbytes > INT32_MAX))
		return (false);

	*ix = (int32_t) (*s).pos; /*where the text item is stored*/

	if (!write_disk_uint32 (s, (uint32_t) ctbytes))
		return (false);

	return (writehandlestream (s, (void *) pdata, ctbytes)); /*following the 4-byte length is the data*/
	} /*hashpackdata*/
			

static boolean hashpackbinary (handlestream *s, Handle hbinary, int32_t *ix) {
	long ctbytes;

	if (ix == NULL)
		return (false);
	if ((*s).pos > INT32_MAX)
		return (false);

	*ix = (int32_t) (*s).pos; /*where the text item is stored*/

	ctbytes = gethandlesize (hbinary); /*first 4 bytes holds the unsigned length*/
	if ((ctbytes < 0) || (ctbytes > INT32_MAX))
		return (false);

	if (!write_disk_uint32 (s, (uint32_t) ctbytes))
		return (false);

	return (writehandlestreamhandle (s, hbinary)); /*following the 4-byte length is the packed text*/
	} /*hashpackbinary*/


static boolean hashunpackbinary (Handle hget, Handle *hbinary, int32_t ix) {
	long lix = (long) ix;
	uint32_t disklen = 0;

	if (!read_disk_uint32 (hget, &lix, &disklen))
		return (false);

	return (loadfromhandletohandle (hget, &lix, (long) disklen, false, hbinary));
	} /*hashunpackbinary*/


static boolean hashpackscalar (handlestream *s, hdlhashnode hnode, int32_t *ix, boolean use_64bit) {
	tydiskvaluerecord diskvalue;
	Handle hbinary = (**hnode).val.data.binaryvalue;
	long ctbytes;
	boolean fl;

	if (ix == NULL)
		return (false);
	if ((*s).pos > INT32_MAX)
		return (false);

	*ix = (int32_t) (*s).pos; /*where the item is stored*/

	if ((**hnode).val.fldiskval) {	/*already a disk-based scalar. can be tricky*/
		if (flexternalmemorypack) {
			hdldatabaserecord hdb = hexternalpackdatabase;
			if (hdb)
				dbpushdatabase (hdb);
			dbaddress diskadr = (**hnode).val.data.diskvalue;
			fl = dbrefhandle (diskadr, &hbinary);
			if (hdb)
				dbpopdatabase ();
			if (!fl)
				return (false);
			fl = hashpackbinary (s, hbinary, ix);
			disposehandle (hbinary);
			return (fl);
		}

		diskvalue.sizeflag = diskvalsizeflag;
		diskvalue.adr = (**hnode).val.data.diskvalue;
		if (fldatabasesaveas) {
			if (!dbcopy (diskvalue.adr, &diskvalue.adr))
				return (false);
		}
		return (write_disk_scalar_reference (s, diskvalue.adr, use_64bit));
	}

	ctbytes = gethandlesize (hbinary);
	if ((ctbytes < 0) || (ctbytes > INT32_MAX))
		return (false);

	if ((ctbytes > maxinlinescalarsize) && (!flexternalmemorypack)) {
		diskvalue.sizeflag = diskvalsizeflag;
		diskvalue.adr = nildbaddress;
		if (!dbassignhandle (hbinary, &diskvalue.adr))
			return (false);
		if (!fldatabasesaveas) {
			disposevaluerecord ((**hnode).val, true);
			(**hnode).val.fldiskval = true;
			(**hnode).val.data.diskvalue = diskvalue.adr;
		}
		return (write_disk_scalar_reference (s, diskvalue.adr, use_64bit));
	}

	if (!write_disk_uint32 (s, (uint32_t) ctbytes))
		return (false);

	return (writehandlestreamhandle (s, hbinary)); /*following the 4-byte length is the packed text*/
	} /*hashpackscalar*/


static boolean hashunpackscalar (Handle hget, tyvaluerecord *val, int32_t ix, boolean use_64bit) {
	long lix = (long) ix;
	uint32_t disklen = 0;

	if (!read_disk_uint32 (hget, &lix, &disklen))
		return (false);

	if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_HASH)) {
		log_trace(LOG_COMP_HASH, "[unpackscalar] ix=%d disklen=0x%08x type=%d", ix, disklen, val->valuetype);
	}
	if (disklen == (uint32_t) (int32_t) diskvalsizeflag) {
		(*val).fldiskval = true;
		dbaddress diskadr = 0;
		if (!read_disk_dbaddress (hget, &lix, &diskadr, use_64bit))
			return (false);
		(*val).data.diskvalue = diskadr;
		return (true);
	}
	else {
		return (loadfromhandletohandle (hget, &lix, (long) disklen, false, &(*val).data.binaryvalue));
	}
	} /*hashunpackscalar*/


static boolean hashpackexternal (handlestream *s, hdlexternalvariable h, int32_t *ix, boolean *flnewdbaddress, const db_context *ctx) {
	Handle hpacked;
	long ctbytes;
	boolean fl;

	if (ix == NULL)
		return (false);
	if ((*s).pos > INT32_MAX)
		return (false);

	*ix = (int32_t) (*s).pos; /*where the text item is stored*/

	if (flexternalmemorypack)
		fl = langexternalmemorypack (h, &hpacked, HNoNode);
	else
		fl = langexternalpack_internal (ctx, h, &hpacked, flnewdbaddress);  /* Phase 1: Pass explicit context */

	if (!fl)
		return (false);

	ctbytes = gethandlesize (hpacked);
	if ((ctbytes < 0) || (ctbytes > INT32_MAX)) {
		disposehandle (hpacked);
		return (false);
	}

	uint32_t disk_length = (uint32_t) host_to_disk_int32 ((int32_t) ctbytes);

	if (!writehandlestream (s, &disk_length, sizeof (disk_length))) {
		disposehandle (hpacked);
		return (false);
	}

	fl = writehandlestreamhandle (s, hpacked);

	disposehandle (hpacked);

	return (fl);
	} /*hashpackexternal*/


static boolean hashunpackexternal (Handle hget, boolean flmemory, hdlexternalhandle *h, int32_t ix) {
	Handle hpacked;
	boolean fl;
	long lix = (long) ix;
	uint32_t disk_length = 0;

#if defined(FRONTIER_HEADLESS)
	{
		unsigned char *base = (unsigned char *) *hget;
		long total = gethandlesize (hget);
		if (lix >= 0 && (lix + 4) <= total) {
			uint32_t raw_len = ((uint32_t) base[lix] << 24) |
			                   ((uint32_t) base[lix + 1] << 16) |
			                   ((uint32_t) base[lix + 2] << 8) |
			                   ((uint32_t) base[lix + 3]);
			long dump = 4 + (raw_len < 28 ? (long) raw_len : 28L);
			if ((lix + dump) > total)
				dump = total - lix;
			fprintf(stderr, "[headless] hashunpackexternal raw[%ld] len=%u bytes:", lix, (unsigned int) raw_len);
			for (long i = 0; i < dump; ++i)
				fprintf(stderr, " %02x", base[lix + i]);
			fprintf(stderr, "\n");
		} else {
			log_error(LOG_COMP_HASH, "hashunpackexternal bad index %ld (total=%ld)", lix, total);
		}
	}
#endif

	if (!read_disk_uint32 (hget, &lix, &disk_length))
		return (false);

	if (disk_length > (uint32_t) LONG_MAX)
		return (false);

	long ctbytes = (long) disk_length;

	if (!loadfromhandletohandle (hget, &lix, ctbytes, true, &hpacked))
		return (false);

	fl = flmemory ? langexternalmemoryunpack (hpacked, h) : langexternalunpack (hpacked, h);

	disposehandle (hpacked);

	return (fl);
	} /*hashunpackexternal*/


static void hashreporterror (short iderror, bigstring bsname, bigstring bserror) {

	/*
	5.1.4 dmb: embellish the bserror, folding it and bsname into the message iderror
	
	the smart part: for recursion, see if bserror already includes iderror. In that
	case, fold bsname into the path that's already in the message
	*/
		
	bigstring bs;
	
	fllangerror = false; // make sure our error won't be ignored
	
	getstringlist (langerrorlist, iderror, bs);
	
	nthword (bs, 1, '^', bs);
	
	if (patternmatch (bs, bserror) == 1) { // it's already been parsed in
		
		pushchar ('.', bsname);
		
		midinsertstring (bsname, bserror, stringlength (bs) + 1);
		
		langerrormessage (bserror);
		}
	else
		lang2paramerror (iderror, bsname, bserror);
	} /*hashbuilderrormessage*/

#pragma pack(2)
typedef struct typackinforecord {

	handlestream s1;
	handlestream s2;
	boolean flmustsave;
	boolean use_64bit;
	const db_context *context;  /* Phase 1: Explicit context passing to eliminate global mode dependency */
	} typackinforecord;
#pragma options align=reset


/* Legacy pack visitor (v<=6) */
static boolean hashpackvisit_legacy (bigstring bsname, hdlhashnode hnode, tyvaluerecord val, ptrvoid refcon);

/* V7 pack visitor */
static boolean hashpackvisit_v7 (bigstring bsname, hdlhashnode hnode, tyvaluerecord val, ptrvoid refcon);

/* Dispatcher that selects legacy vs v7 based on format flag */
static boolean hashpackvisit (bigstring bsname, hdlhashnode hnode, tyvaluerecord val, ptrvoid refcon);

/* Dispatcher implementation */
static boolean hashpackvisit (bigstring bsname, hdlhashnode hnode, tyvaluerecord val, ptrvoid refcon) {
	typackinforecord *lpi = (typackinforecord *) refcon;
	if (lpi != NULL && lpi->use_64bit)
		return hashpackvisit_v7 (bsname, hnode, val, refcon);
	return hashpackvisit_legacy (bsname, hnode, val, refcon);
}

static boolean hashpackvisit_legacy (bigstring bsname, hdlhashnode hnode, tyvaluerecord val, ptrvoid refcon) {

	/*
	return true to terminate the search, false to continue.

	4/8/93 dmb: added support for code values

	2.1b2 dmb: filespecs must be saved as aliases since volume reference 
	numbers aren't persistent across boots

	3.0.2 dmb: power pc code for doubles

	3.0.2 dmb: fixed memory leak when packing filespecs and code

	3.0.4 dmb: must set rec.data.longvalue for PPC doublevaluetype! fixes db corruption.

	5.1b21 dmb: don't let unresolvable address kill the save

	5.1.3 dmb: smarter error reporting

	6.2a15 AR: added flmustsave parameter
	*/

	typackinforecord *lpi = (typackinforecord *) refcon;
	tydisksymbolrecord rec;
#if defined(FRONTIER_HEADLESS)
	const char *hashpack_fail_file = NULL;
	const char *hashpack_fail_reason = "unknown";
	int hashpack_fail_line = 0;
#define HASH_PACK_FAIL(reason) do { hashpack_fail_file = __FILE__; hashpack_fail_line = __LINE__; hashpack_fail_reason = (reason); goto error; } while (0)
#else
#define HASH_PACK_FAIL(reason) goto error
#endif

#if defined(FRONTIER_HEADLESS)
	if (hnode != nil) { /* tests may call with nil hnode */
		if (!langhash_prepare_wordprocessor_value(bsname, hnode, &(**hnode).val))
			return true;
		val = (**hnode).val;
	}
#endif
	bigstring bsvalue;
	Handle hpacked;
	langerrormessagecallback savecallback;
	ptrvoid saverefcon;
	bigstring bspackerror;
	boolean fl;
	int32_t name_index = 0;
	int32_t data_index = 0;

	assert (sizeof (tydisksymbolrecord) == 10L);

	/*
	if (stringlength (bsname) == 0)
		Debugger ();

	ccmsg (bsname, false);
	*/

	if (hnode != nil && (**hnode).fldontsave && !flexternalmemorypack) /*keep traversing the table*/
		return (false);

	langtraperrors (bspackerror, &savecallback, &saverefcon);

	clearbytes (&rec, sizeof (rec));

	if (!hashpackstring (&lpi->s2, bsname, &name_index))
		HASH_PACK_FAIL("hashpackstring(name)");

	rec.ixkey = host_to_disk_int32 (name_index);

	/*	rec.valuetype = conditionalenumswap(val.valuetype); */
	rec.valuetype = val.valuetype;
#if defined(FRONTIER_HEADLESS)
	if (val.valuetype == listvaluetype) {
		fprintf(stderr, "[headless] hashpackvisit_v7 WRITE list name='%.*s' path=%s\n",
		        bsname[0], bsname + 1,
		        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>");
		unsigned char dumpbuf[sizeof (rec)];
		memcpy(dumpbuf, &rec, sizeof(rec));
		fprintf(stderr, "[headless]   rec bytes:");
		for (size_t i = 0; i < sizeof(rec); ++i)
			fprintf(stderr, " %02x", dumpbuf[i]);
		fprintf(stderr, "\n");
	}
#endif

	switch (val.valuetype) {
		case oldstringvaluetype: {
			copyheapstring ((hdlstring) val.data.stringvalue, bsvalue);

			data_index = 0;
			if (!hashpackstring (&lpi->s2, bsvalue, &data_index))
				HASH_PACK_FAIL("hashpackstring(oldstring)");

			rec.data.longvalue = host_to_disk_int32 (data_index);
			break;
		}

		case addressvaluetype: {
			disablelangerror ();

			fl = getaddresspath (val, bsvalue);

			enablelangerror ();

			if (!fl) /* on error, bsvalue should at least be item's name; don't break the save */
				;

			data_index = 0;
			if (!hashpackstring (&lpi->s2, bsvalue, &data_index))
				HASH_PACK_FAIL("hashpackstring(address)");

			rec.data.longvalue = host_to_disk_int32 (data_index);
			break;
		}

	#ifdef oldMACVERSION	
		case filespecvaluetype: { /*need to save as a (minimal) alias*/

			register hdlfilespec x = val.data.filespecvalue;
			tyfilespec fs = **x;
			AliasHandle halias = nil;

			disablelangerror ();

			if (filespectoalias (&fs, true, &halias)) {

				rec_version = 1; /*all versions were zero until now*/

				x = (hdlfilespec) halias;
			}
			enablelangerror ();

			data_index = 0;
			if (!hashpackbinary (&lpi->s2, (Handle) x, &data_index))
				HASH_PACK_FAIL("hashpackbinary(filespec->alias)");

			rec.data.longvalue = host_to_disk_int32 (data_index);

			disposehandle ((Handle) halias); /*3.0.2*/

			break;
		}
	#endif

		case rectvaluetype: {
			diskrect rdisk;

			recttodiskrect (*val.data.rectvalue, &rdisk);

			data_index = 0;
			if (!hashpackdata (&lpi->s2, &rdisk, sizeof (rdisk), &data_index))
				HASH_PACK_FAIL("hashpackdata(rect)");

			rec.data.longvalue = host_to_disk_int32 (data_index);
			break;
		}

		case rgbvaluetype: {
			diskrgb rgbdisk;

			rgbtodiskrgb (*val.data.rgbvalue, &rgbdisk);

			data_index = 0;
			if (!hashpackdata (&lpi->s2, &rgbdisk, sizeof (rgbdisk), &data_index))
				HASH_PACK_FAIL("hashpackdata(rgb)");

			rec.data.longvalue = host_to_disk_int32 (data_index);
			break;
		}

	#if noextended
		case doublevaluetype: {
			double x = **val.data.doublevalue;
			extended80 x80;

				dtox80 (&x, &x80);

			data_index = 0;
			if (!hashpackdata (&lpi->s2, &x80, sizeof (x80), &data_index))
				HASH_PACK_FAIL("hashpackdata(double)");

			rec.data.longvalue = host_to_disk_int32 (data_index);
			break;
		}
	#else
		case doublevaluetype:
	#endif

	#ifdef oldWIN95VERSION	
		case filespecvaluetype:
		case aliasvaluetype:
	#endif
		case stringvaluetype:
		case passwordvaluetype:
		case patternvaluetype:
		case objspecvaluetype:
		case binaryvaluetype:
			data_index = 0;
				if (!hashpackscalar (&lpi->s2, hnode, &data_index, lpi->use_64bit)) {
#if defined(FRONTIER_HEADLESS)
					tyvaluerecord *node_val = &(**hnode).val;
					fprintf(stderr, "[headless] hashpackscalar diagnostics name='%.*s' valuetype=%d fldiskval=%d fldatabasesaveas=%d flexternalmemorypack=%d disk=0x%llx handle=%p\n",
						(int)bsname[0], (char *)&bsname[1],
						(int)(*node_val).valuetype,
						(int)(*node_val).fldiskval,
						(int)fldatabasesaveas,
						(int)flexternalmemorypack,
						(unsigned long long)(*node_val).data.diskvalue,
						(void *)(*node_val).data.binaryvalue);
#endif
					HASH_PACK_FAIL("hashpackscalar");
				}

			rec.data.longvalue = host_to_disk_int32 (data_index);
			break;

		case listvaluetype:
		case recordvaluetype:
				if (!oppacklist (val.data.listvalue, &hpacked))
					HASH_PACK_FAIL("oppacklist");

			data_index = 0;
				if (!hashpackbinary (&lpi->s2, hpacked, &data_index))
					HASH_PACK_FAIL("hashpackbinary(list/record)");

			rec.version = 2;

			disposehandle (hpacked);

			rec.data.longvalue = host_to_disk_int32 (data_index);
			break;

		case filespecvaluetype:
		case aliasvaluetype:
			if (!langpackfileval (&val, &hpacked))
				HASH_PACK_FAIL("langpackfileval");

			data_index = 0;
			if (!hashpackbinary (&lpi->s2, hpacked, &data_index))
				HASH_PACK_FAIL("hashpackbinary(file/alias)");

			rec.version = 2;

			disposehandle (hpacked);

			rec.data.longvalue = host_to_disk_int32 (data_index);
			break;

		case codevaluetype:
			if (!langpacktree (val.data.codevalue, &hpacked))
				HASH_PACK_FAIL("langpacktree");

			data_index = 0;
			if (!hashpackbinary (&lpi->s2, hpacked, &data_index))
				HASH_PACK_FAIL("hashpackbinary(code)");

			disposehandle (hpacked); /*3.0.2*/

			rec.data.longvalue = host_to_disk_int32 (data_index);
			break;

        case externalvaluetype: {
            boolean flnewdbaddress = false;

            hdlexternalvariable hv = (hdlexternalvariable) val.data.externalvalue;
        if (hv != nil && !(**hv).flinmemory) {
            /* When repacking a legacy root during Save As, the destination file is empty and
               legacy 32-bit addresses can look “free” against the BE64 allocator. Skip the
               free-block drop in that case so script externals are repacked instead of lost. */
            const boolean skip_free_check =
                fldatabasesaveas && (db_format_adapter_force_repack() || db_format_adapter_is_active());
#if defined(FRONTIER_HEADLESS)
            if (equalstrings(bsname, (ptrstring) "\x03" "now") || equalstrings(bsname, (ptrstring) "\x08" "idleTime")) {
                fprintf(stderr,
                        "[headless] hashpackexternal inspect name='%.*s' flinmemory=%d skip_free_check=%d adr=0x%llx old=0x%llx\n",
                        (int) bsname[0],
                        (char *) &bsname[1],
                        (int) (**hv).flinmemory,
                        (int) skip_free_check,
                        (unsigned long long) (**hv).variabledata,
                        (unsigned long long) (**hv).oldaddress);
            }
#endif

            if (!skip_free_check) {
                dbaddress adr = (dbaddress) (**hv).variabledata;
                Handle htmp = nil;
                boolean okref = false;
                db_context context;
                db_context_init(&context);

                if (adr != nildbaddress && adr != 0)
                    okref = dbrefhandle_context(&context, adr, &htmp);

                if (htmp != nil)
                    disposehandle(htmp);

                if (!okref) {
                    (**hnode).fldontsave = true; /* skip this node when packing */
#if defined(FRONTIER_HEADLESS)
                    fprintf(stderr,
                            "[headless] hashpackexternal dropping name='%.*s' adr=0x%llx (free/unreadable)\n",
                            (int) bsname[0],
                            (char *) &bsname[1],
                            (unsigned long long) adr);
#endif
                    val.fldiskval = false;
                    (**hv).flinmemory = true;
                    (**hv).variabledata = 0;
                    (**hv).oldaddress = nildbaddress;
                    return false;
                }
            }
        }

			data_index = 0;
				if (!hashpackexternal (&lpi->s2, (hdlexternalvariable) val.data.externalvalue, &data_index, &flnewdbaddress, lpi->context)) {
#if defined(FRONTIER_HEADLESS)
					hdlexternalvariable diag = (hdlexternalvariable) val.data.externalvalue;
					int external_id = 0;
					if (diag != nil)
						external_id = (**diag).id;
					fprintf(stderr, "[headless] hashpackexternal diagnostics name='%.*s' external=%p id=%d\n",
						(int)bsname[0], (char *)&bsname[1],
						(void *)diag,
						external_id);
#endif
					if (fldatabasesaveas && db_format_adapter_is_active()) {
						(**hnode).fldontsave = true; /* skip unreadable legacy external during migration */
						return false; /* continue traversal */
					}
					HASH_PACK_FAIL("hashpackexternal");
				}

			lpi->flmustsave = lpi->flmustsave || flnewdbaddress;

			rec.data.longvalue = host_to_disk_int32 (data_index);
			break;
		}

		case novaluetype:
		case booleanvaluetype:
		case charvaluetype:
		case intvaluetype:
		case tokenvaluetype:
		case pointvaluetype:
		case longvaluetype:
		case ostypevaluetype:
		case enumvaluetype:
		case fixedvaluetype:
		case singlevaluetype:
		case directionvaluetype:
		case datevaluetype:
		diskvalue_from_value_legacy (&val, &rec.data);
			break;

		default:
			langerror (cantpackerror);
			HASH_PACK_FAIL("langerror(default)");
	}

	if (!writehandlestream (&lpi->s1, &rec, sizeof (rec)))
		HASH_PACK_FAIL("writehandlestream(record)");

	languntraperrors (savecallback, saverefcon, false);

	return (false); /*keep going, kind of backwards*/

	error:
	
	disposehandlestream (&lpi->s1); 
	
	disposehandlestream (&lpi->s2); 
	
	languntraperrors (savecallback, saverefcon, true);
	
#if defined(FRONTIER_HEADLESS)
	if (hashpack_fail_file != NULL) {
		fprintf(stderr, "[headless] hashpackvisit failed name='%.*s' valuetype=%d reason=%s at %s:%d\n",
			(int)bsname[0], (char *)&bsname[1],
			(int)val.valuetype,
			hashpack_fail_reason ? hashpack_fail_reason : "unknown",
			hashpack_fail_file,
			hashpack_fail_line);
	}
#endif

hashreporterror (hashpackerror, bsname, bspackerror);

#undef HASH_PACK_FAIL
return (true); /*stop now, this is the error return*/
}

#if defined(FRONTIER_TESTS)
/* Test entry points to exercise v7 pack/unpack. */
void langhash_test_value_to_disk_v7(const tyvaluerecord *val, langhash_test_disksymbolrecord_v7 *rec_out) {
    tydisksymbolrecord_v7 rec_local;
    clearbytes(&rec_local, sizeof(rec_local));
    rec_local.valuetype = val->valuetype;
    rec_local.version = 0;
    rec_local._pad = 0;
    diskvalue_from_value_v7(val, &rec_local.data);
    memcpy(rec_out, &rec_local, sizeof(rec_local));
}

void langhash_test_value_from_disk_v7(const langhash_test_disksymbolrecord_v7 *rec_in, tyvaluerecord *val) {
    tydisksymbolrecord_v7 rec_local;
    memcpy(&rec_local, rec_in, sizeof(rec_local));
    initvalue(val, (tyvaluetype) rec_local.valuetype);
    diskvalue_to_value_v7(&rec_local.data, val);
}
#endif


/* Modern pack visitor (v7+; BE64 numerics) */
static boolean hashpackvisit_v7 (bigstring bsname, hdlhashnode hnode, tyvaluerecord val, ptrvoid refcon) {

	/*
	Matches legacy logic but writes modern scalar payloads (64-bit ints/doubles, Mac-epoch date).
	Non-scalar storage (string/binary/etc.) still uses 32-bit indices into the string handle.
	*/

	typackinforecord *lpi = (typackinforecord *) refcon;
	unsigned char recbuf[sizeof(tydisksymbolrecord_v7)];
	clearbytes(recbuf, sizeof(recbuf));
	uint8_t rec_version = 0;
#if defined(FRONTIER_HEADLESS)
	const char *hashpack_fail_file = NULL;
	const char *hashpack_fail_reason = "unknown";
	int hashpack_fail_line = 0;
#define HASH_PACK_FAIL(reason) do { hashpack_fail_file = __FILE__; hashpack_fail_line = __LINE__; hashpack_fail_reason = (reason); goto error; } while (0)
#else
#define HASH_PACK_FAIL(reason) goto error
#endif

#if defined(FRONTIER_HEADLESS)
	if (hnode != nil) { /* tests may call with nil hnode */
		if (!langhash_prepare_wordprocessor_value(bsname, hnode, &(**hnode).val))
			return true;
		val = (**hnode).val;
	}
#endif

	bigstring bsvalue;
	Handle hpacked;
	langerrormessagecallback savecallback;
	ptrvoid saverefcon;
	bigstring bspackerror;
	boolean fl;
	int32_t name_index = 0;
	int32_t data_index = 0;

	if ((**hnode).fldontsave && !flexternalmemorypack) /*keep traversing the table*/
		return (false);

	langtraperrors (bspackerror, &savecallback, &saverefcon);

	if (!hashpackstring (&lpi->s2, bsname, &name_index))
		HASH_PACK_FAIL("hashpackstring(name)");

	/* Manually populate packed record buffer (big-endian, fixed layout).
	   See planning/phase3/big_endian_portability_audit.md for BE64 guidance. */
	int32_t ix_be = host_to_disk_int32 (name_index);
	memcpy(recbuf + 0, &ix_be, sizeof(int32_t));
	recbuf[4] = (uint8_t) val.valuetype;
	recbuf[5] = rec_version; /* version */
	recbuf[6] = 0; /* pad */
	recbuf[7] = 0; /* pad */

#if defined(FRONTIER_HEADLESS)
	if (val.valuetype == listvaluetype) {
		const Handle hlist = (Handle) val.data.listvalue;
		const long hsize = (hlist == nil) ? -1L : gethandlesize(hlist);
		fprintf(stderr, "[headless] hashpackvisit_v7 list encounter path=%s name='%.*s' hlist=%p hdata=%p valid=%d size=%ld fldiskval=%d\n",
		        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
		        bsname[0], bsname + 1,
		        (void *) hlist,
		        hlist == nil ? NULL : *hlist,
		        (hlist == nil) ? 0 : validhandle(hlist),
		        hsize,
		        val.fldiskval);
	}
#endif

#if defined(FRONTIER_HEADLESS)
	fprintf(stderr, "[headless] hashpackvisit_v7 path=%s name='%.*s' valuetype=%d\n",
	        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
	        bsname[0], bsname + 1,
	        val.valuetype);
	if (val.valuetype == listvaluetype) {
		const Handle hlist = (Handle) val.data.listvalue;
		const long hsize = (hlist == nil) ? -1L : gethandlesize(hlist);
		fprintf(stderr, "[headless]   list debug hlist=%p hdata=%p valid=%d size=%ld fldiskval=%d\n",
		        (void *) hlist,
		        hlist == nil ? NULL : *hlist,
		        (hlist == nil) ? 0 : validhandle(hlist),
		        hsize,
		        val.fldiskval);
	}
	fflush(stderr);
#endif

	switch (val.valuetype) {
		case oldstringvaluetype: {
			copyheapstring ((hdlstring) val.data.stringvalue, bsvalue);

			data_index = 0;
			if (!hashpackstring (&lpi->s2, bsvalue, &data_index))
				HASH_PACK_FAIL("hashpackstring(oldstring)");

			{
				int64_t tmp = host_to_disk_int64 ((int64_t) data_index);
				memcpy(recbuf + 8, &tmp, sizeof(int64_t));
			}
			break;
		}

		case addressvaluetype: {
			disablelangerror ();

			fl = getaddresspath (val, bsvalue);

			enablelangerror ();

			if (!fl) /* on error, bsvalue should at least be item's name; don't break the save */
				;

			data_index = 0;
			if (!hashpackstring (&lpi->s2, bsvalue, &data_index))
				HASH_PACK_FAIL("hashpackstring(address)");

			{
				int64_t tmp = host_to_disk_int64 ((int64_t) data_index);
				memcpy(recbuf + 8, &tmp, sizeof(int64_t));
			}
			break;
		}

	#ifdef oldMACVERSION	
		case filespecvaluetype: { /*need to save as a (minimal) alias*/

			register hdlfilespec x = val.data.filespecvalue;
			tyfilespec fs = **x;
			AliasHandle halias = nil;

			disablelangerror ();

			if (filespectoalias (&fs, true, &halias)) {

				rec.version = 1; /*all versions were zero until now*/

				x = (hdlfilespec) halias;
			}
			enablelangerror ();

			data_index = 0;
			if (!hashpackbinary (&lpi->s2, (Handle) x, &data_index))
				HASH_PACK_FAIL("hashpackbinary(filespec->alias)");

			{
				int64_t tmp = host_to_disk_int64 ((int64_t) data_index);
				memcpy(recbuf + 8, &tmp, sizeof(int64_t));
			}

			disposehandle ((Handle) halias); /*3.0.2*/

			break;
		}
	#endif

		case rectvaluetype: {
			diskrect rdisk;

			recttodiskrect (*val.data.rectvalue, &rdisk);

			data_index = 0;
			if (!hashpackdata (&lpi->s2, &rdisk, sizeof (rdisk), &data_index))
				HASH_PACK_FAIL("hashpackdata(rect)");

			{
				int64_t tmp = host_to_disk_int64 ((int64_t) data_index);
				memcpy(recbuf + 8, &tmp, sizeof(int64_t));
			}
			break;
		}

		case rgbvaluetype: {
			diskrgb rgbdisk;

			rgbtodiskrgb (*val.data.rgbvalue, &rgbdisk);

			data_index = 0;
			if (!hashpackdata (&lpi->s2, &rgbdisk, sizeof (rgbdisk), &data_index))
				HASH_PACK_FAIL("hashpackdata(rgb)");

			{
				int64_t tmp = host_to_disk_int64 ((int64_t) data_index);
				memcpy(recbuf + 8, &tmp, sizeof(int64_t));
			}
			break;
		}

		case doublevaluetype: {
			double x = **val.data.doublevalue;
			{
				uint64_t bits = host_to_disk_double_bits (x);
				memcpy(recbuf + 8, &bits, sizeof(uint64_t));
			}
			break;
		}

		case stringvaluetype:
		case passwordvaluetype:
		case patternvaluetype:
		case objspecvaluetype:
		case binaryvaluetype:
		case filespecvaluetype:
		case aliasvaluetype:
		case listvaluetype:
		case recordvaluetype:
		case codevaluetype: {
			/* Keep existing packing routines; store index as 64-bit */
			switch (val.valuetype) {
				case listvaluetype:
				case recordvaluetype:
					if (!oppacklist (val.data.listvalue, &hpacked))
						HASH_PACK_FAIL("oppacklist");
					rec_version = 2;
					break;
				case filespecvaluetype:
				case aliasvaluetype:
					if (!langpackfileval (&val, &hpacked))
						HASH_PACK_FAIL("langpackfileval");
					rec_version = 2;
					break;
				case codevaluetype:
					if (!langpacktree (val.data.codevalue, &hpacked))
						HASH_PACK_FAIL("langpacktree");
					break;
				default:
					hpacked = nil;
					break;
			}

			if (val.valuetype == stringvaluetype || val.valuetype == passwordvaluetype || val.valuetype == patternvaluetype || val.valuetype == objspecvaluetype || val.valuetype == binaryvaluetype) {
				data_index = 0;
				if (!hashpackscalar (&lpi->s2, hnode, &data_index, lpi->use_64bit))
					HASH_PACK_FAIL("hashpackscalar");
			}
			else if (val.valuetype == listvaluetype || val.valuetype == recordvaluetype || val.valuetype == filespecvaluetype || val.valuetype == aliasvaluetype || val.valuetype == codevaluetype) {
				data_index = 0;
				if (!hashpackbinary (&lpi->s2, hpacked, &data_index))
					HASH_PACK_FAIL("hashpackbinary(var)");
				if (hpacked != nil)
					disposehandle (hpacked);
			}

			{
				int64_t tmp = host_to_disk_int64 ((int64_t) data_index);
				memcpy(recbuf + 8, &tmp, sizeof(int64_t));
			}
			break;
		}

		case externalvaluetype: {
			boolean flnewdbaddress = false;

			hdlexternalvariable hv = (hdlexternalvariable) val.data.externalvalue;
			if (hv != nil && !(**hv).flinmemory) {
				const boolean skip_free_check =
					fldatabasesaveas && (db_format_adapter_force_repack() || db_format_adapter_is_active());

				if (!skip_free_check) {
					dbaddress adr = (dbaddress) (**hv).variabledata;
					Handle htmp = nil;
					boolean okref = false;
					db_context context;
					db_context_init(&context);

					if (adr != nildbaddress && adr != 0)
						okref = dbrefhandle_context(&context, adr, &htmp);

					if (htmp != nil)
						disposehandle(htmp);

					if (!okref) {
						(**hnode).fldontsave = true; /* skip this node when packing */
						val.fldiskval = false;
						(**hv).flinmemory = true;
						(**hv).variabledata = 0;
						(**hv).oldaddress = nildbaddress;
						return false;
					}
				}
			}

			data_index = 0;
			if (!hashpackexternal (&lpi->s2, (hdlexternalvariable) val.data.externalvalue, &data_index, &flnewdbaddress, lpi->context))
				HASH_PACK_FAIL("hashpackexternal");

			lpi->flmustsave = lpi->flmustsave || flnewdbaddress;
			{
				int64_t tmp = host_to_disk_int64 ((int64_t) data_index);
				memcpy(recbuf + 8, &tmp, sizeof(int64_t));
			}
			break;
		}

		case novaluetype:
		case booleanvaluetype:
		case charvaluetype:
		case intvaluetype:
		case tokenvaluetype:
		case pointvaluetype:
		case longvaluetype:
		case ostypevaluetype:
		case enumvaluetype:
		case fixedvaluetype:
		case singlevaluetype:
		case directionvaluetype:
		case datevaluetype:
			tydiskvaluedata_v7 rec_data_local;
			clearbytes(&rec_data_local, sizeof(rec_data_local));
			diskvalue_from_value_v7 (&val, &rec_data_local);
			memcpy(recbuf + 8, &rec_data_local, sizeof(rec_data_local));
			break;

	default:
		langerror (cantpackerror);
		HASH_PACK_FAIL("langerror(default)");
}

	/* Stamp final version byte before writing the record. */
	recbuf[5] = rec_version;

	if (!writehandlestream (&lpi->s1, recbuf, sizeof (recbuf)))
		HASH_PACK_FAIL("writehandlestream(record)");

	languntraperrors (savecallback, saverefcon, false);

	return (false); /*keep going, kind of backwards*/

error:
	disposehandlestream (&lpi->s1); 
	disposehandlestream (&lpi->s2); 
	languntraperrors (savecallback, saverefcon, true);

#if defined(FRONTIER_HEADLESS)
	if (hashpack_fail_file != NULL) {
		fprintf(stderr, "[headless] hashpackvisit_v7 failed name='%.*s' valuetype=%d reason=%s at %s:%d\n",
			(int)bsname[0], (char *)&bsname[1],
			(int)val.valuetype,
			hashpack_fail_reason ? hashpack_fail_reason : "unknown",
			hashpack_fail_file,
			hashpack_fail_line);
	}
#endif

	hashreporterror (hashpackerror, bsname, bspackerror);
	
#undef HASH_PACK_FAIL
	return (true); /*stop now, this is the error return*/
}


boolean hashpacktable_internal (const db_context *ctx, hdlhashtable htable, boolean flmemory, Handle *hpackedtable, boolean *flmustsave) {

	/*
	traverse the current symbol table, creating two packages of information
	that can be unpacked back into an in-memory hash table.

	the first, hrecords, is an array of disksymbolrecords.  each record can
	have one or two indexes to strings in the hstrings package.

	then merge the two handles returning one packet for the caller to save.

	10/6/91 dmb: mergehandles now consumes both source handles

	2/2/93 dmb: check result of pushpackstack

	3/30/93 dmb:
	*/

	register boolean fl = false;
	typackinforecord packrec;
	boolean use_64bit;
	Handle h1, h2;

	/* Check database format mode: use context if provided, else global state */
	if (ctx != NULL) {
		use_64bit = ctx->mode.use_64bit_format;
	} else {
		use_64bit = db_format_mode_current().use_64bit_format;
	}

#if defined(FRONTIER_HEADLESS)
	db_format_mode current_mode = db_format_mode_current();
	fprintf(stderr, "[headless] hashpacktable_internal use_64bit=%d (ctx=%p ctx_mode=%d current_mode: use_64bit=%d adapter_repack=%d)\n",
	        (int) use_64bit, (void *) ctx, ctx ? (int) ctx->mode.use_64bit_format : -1,
	        (int) current_mode.use_64bit_format, (int) current_mode.adapter_repack);
#endif
	if (use_64bit) {
		/* v7 mode: Write v0x05 with 64-bit timestamps */
		tydisktablerecord header;
		clearbytes (&header, sizeof (header));

		header.version = host_to_disk_int16((int16_t)tablediskversion);
		header.sortorder = host_to_disk_int16((int16_t) (**htable).sortorder);
		header._pad = 0; /* padding for 8-byte alignment */

		/* Write 64-bit timestamps in big-endian format */
		db_format_write_be64(&header.timecreated, (uint64_t) (**htable).timecreated);
		db_format_write_be64(&header.timelastsave, (uint64_t) (**htable).timelastsave);

#if defined(FRONTIER_HEADLESS)
		log_trace(LOG_COMP_HASH, "hashpacktable writing v7 header version=%d use64=1", tablediskversion);
#endif

		#ifdef xmlfeatures
			if ((**htable).flxml)
				header.flags = host_to_disk_int32(flxml);
			else
				header.flags = 0;
		#endif

		clearbytes (&packrec, sizeof (packrec));
		packrec.flmustsave = *flmustsave;
		packrec.use_64bit = true;
		packrec.context = ctx;  /* Phase 1: Thread context through pack operations */
		openhandlestream (nil, &packrec.s1);
		openhandlestream (nil, &packrec.s2);

		if (!writehandlestream (&packrec.s1, &header, sizeof (header)))
			goto exit;
	}
	else {
		/* v6 mode: Write v0x04 with 32-bit timestamps for backward compatibility */
		tydisktablerecord_v4 header_v4;
		clearbytes (&header_v4, sizeof (header_v4));

		header_v4.version = host_to_disk_int16((int16_t)0x04);
		header_v4.sortorder = host_to_disk_int16((int16_t) (**htable).sortorder);

		/* Truncate 64-bit timestamps to 32-bit for v6 compatibility */
		header_v4.timecreated = (uint32_t) host_to_disk_int32((int32_t) (**htable).timecreated);
		header_v4.timelastsave = (uint32_t) host_to_disk_int32((int32_t) (**htable).timelastsave);

		#ifdef xmlfeatures
			if ((**htable).flxml)
				header_v4.flags = host_to_disk_int32(flxml);
			else
				header_v4.flags = 0;
		#endif

		clearbytes (&packrec, sizeof (packrec));
		packrec.flmustsave = *flmustsave;
		packrec.use_64bit = false;
		packrec.context = ctx;  /* Phase 1: Thread context through pack operations */
		openhandlestream (nil, &packrec.s1);
		openhandlestream (nil, &packrec.s2);

		if (!writehandlestream (&packrec.s1, &header_v4, sizeof (header_v4)))
			goto exit;

#if TABLE_HEADER_RESERVED_BYTES > 0
		/* v0x04 also has reserved bytes */
		unsigned char reserved[TABLE_HEADER_RESERVED_BYTES] = {0};
		if (!writehandlestream(&packrec.s1, reserved, sizeof(reserved)))
			goto exit;
#endif
	}

#if TABLE_HEADER_RESERVED_BYTES > 0
	/* Write reserved bytes for v0x05 */
	if (use_64bit) {
		unsigned char reserved[TABLE_HEADER_RESERVED_BYTES] = {0};
		if (!writehandlestream(&packrec.s1, reserved, sizeof(reserved)))
			goto exit;
	}
#endif
	
	flexternalmemorypack = flmemory;
	
	if (flexternalmemorypack)
		hexternalpackdatabase = tablegetdatabase (htable);
	
	hashsortedinversesearch (htable, &hashpackvisit, &packrec);
	
	if (packrec.s1.data == nil) /*an error while packing*/
		goto exit;

	h1 = closehandlestream (&packrec.s1);

	h2 = closehandlestream (&packrec.s2);
	
	fl = mergehandles (h1, h2, hpackedtable);
	
	*flmustsave = packrec.flmustsave;

	exit:

	return (fl);
	} /*hashpacktable_internal*/


boolean hashpacktable (hdlhashtable htable, boolean flmemory, Handle *hpackedtable, boolean *flmustsave) {
	/* Wrapper for backward compatibility - uses global mode state */
	return hashpacktable_internal (NULL, htable, flmemory, hpackedtable, flmustsave);
}


boolean hashunpacktable_internal (const db_context *ctx, Handle hpackedtable, boolean flmemory, hdlhashtable htable) {

	/*
	unpack a hashtable packed by hashpacktable.  first explode the packed handle
	into two handles.
	
	return true if everything worked.
	
	we dispose of the packed table as soon as we're finished with it and both
	of the exploded handles.
	
	9/30/91 dmb: changed all hashassign calls to hashinsert; we're starting with 
	a fresh table, so duplicate names should only exist if they were saved that 
	way, in which case we want to preserve them.
	
	3/2/92 dmb: added backward compat code for change in double type (1.0 used 
	"universal" extended doubles; now we use SANE extended
	
	8/14/92 dmb: added special case for nil objspecs
	
	4/8/93 dmb: added support for code values
	
	2.1b2 dmb: filespecs are now saved as aliases
	
	2.1b9 dmb: if filespec can't be resolved from stored alias, change 
	valuetype to alias. also, this operation no longer attempts to mount 
	volumes
	
	5.0d1 dmb: tables now have a header when packed
	
	5.0.2b6 dmb: use new sethashtable to remove stack depth limit
	
	5.1.4 dmb: use new hashreporterror

	2002-11-11 AR: Added assert to make sure the C compiler chose the
	proper byte alignment for the tydisktablerecord struct. If it did not,
	we would end up corrupting any database files we saved.
	
	2006-04-20 sethdill & aradke: convert rgb values to native byte order
	*/
	
	boolean fl = false;
	Handle hrecords, hstrings;
	bigstring bsname, bsvalue;
	hdlhashnode hlastnode = nil;
	boolean flsorted = true; // 6.10.97 dmb: no longer do any auto-sorting here
	tydisktablerecord header;
	tydisktablerecord_v4 header_v4;
	long ix = 0;
	long ixstrings;
	Handle hpacked;
	boolean fldirty;
	langerrormessagecallback savecallback = nil;
	ptrvoid saverefcon = nil;
	bigstring bsunpackerror;
	hdlhashtable prevhashtable = nil;
#if defined(FRONTIER_HEADLESS)
	long debug_record_index = 0;
	fprintf(stderr, "[headless] hashunpacktable enter htable=%p flmemory=%d\n",
	        (void *) htable, (int) flmemory);
#endif

	/* v0x04 and earlier use 16-byte header, v0x05 uses 32-byte header */
	assert (sizeof(tydisktablerecord_v4) == 16L);
	assert (sizeof(tydisktablerecord) == 32L);
	
	if (!unmergehandles (hpackedtable, &hrecords, &hstrings)) /*consumes hpackedtable*/
		return (false);

#if defined(FRONTIER_HEADLESS)
	if (!hashunpack_log_init) {
		hashunpack_log_init = true;
		const char *logpath = getenv("FRONTIER_HASHUNPACK_LOG");
		if (is_safe_log_path(logpath)) {
			hashunpack_log = fopen(logpath, "w");
			if (hashunpack_log == NULL) {
				log_error(LOG_COMP_HASH, "hashunpacktable: failed to open log %s: %s", logpath, strerror(errno));
			} else {
				atexit(close_hashunpack_log);
			}
		} else if (logpath && *logpath) {
			fprintf(stderr, "[headless] hashunpacktable: unsafe log path ignored: %s\n", logpath);
		}
	}

	fprintf(stderr, "[headless] hashunpacktable split records=%ld strings=%ld\n",
	        hrecords ? gethandlesize (hrecords) : 0L,
	        hstrings ? gethandlesize (hstrings) : 0L);
	if (hrecords && gethandlesize (hrecords) >= (long) sizeof (tydisktablerecord)) {
		unsigned char *recbytes = (unsigned char *) *hrecords;
		fprintf(stderr, "[headless] hashunpacktable records bytes:");
		long dump = gethandlesize (hrecords);
		if (dump > 32)
			dump = 32;
		for (long i = 0; i < dump; ++i)
			fprintf(stderr, " %02x", recbytes[i]);
		fprintf(stderr, "\n");
	}
#endif
	
	fldirty = (**htable).fldirty; //start with current state

	/*see if this is a 5.0 table, with a header*/

	/* Peek at version to determine which structure to read.
	   If the packed data is too small or version is out of range, treat as no header. */
	int16_t version_peek = 0;
	boolean header_present = false;
	const long total_bytes = gethandlesize(hrecords);
	const int16_t max_supported_version = 10; /* future headroom */
	/* Peek at the first two bytes; only treat as a header when the version is in-range
	   and the packed records area is large enough to hold the corresponding header. */
	if (total_bytes >= (long) sizeof (tydisktablerecord_v4)) {
		long ix_peek = ix;
		if (loadfromhandle (hrecords, &ix_peek, sizeof(int16_t), &version_peek)) {
			version_peek = disk_to_host_int16(version_peek);
			if (version_peek >= 1 && version_peek <= max_supported_version)
				header_present = true;
		}
	}

	if (header_present && version_peek >= 0x05 && total_bytes >= (long) sizeof (tydisktablerecord)) {
		/* v0x05+: Modern format with 64-bit timestamps */
		/* Note: loadfromhandle() performs raw byte copy without byte swapping */
		loadfromhandle (hrecords, &ix, sizeof (tydisktablerecord), &header);
		header.version = disk_to_host_int16(header.version);
	}
	else if (header_present && version_peek >= 0x01) {
		/* v0x04 and earlier: Legacy format with 32-bit timestamps */
		/* Note: loadfromhandle() performs raw byte copy without byte swapping */
		loadfromhandle (hrecords, &ix, sizeof (tydisktablerecord_v4), &header_v4);
		/* Convert v4 header to v5 format for processing */
		header.version = disk_to_host_int16(header_v4.version);
		header.sortorder = header_v4.sortorder; /* will be byte-swapped below */
		header._pad = 0;
		/* Widen 32-bit timestamps to 64-bit */
		header.timecreated = (uint64_t) disk_to_host_int32((int32_t) header_v4.timecreated);
		header.timelastsave = (uint64_t) disk_to_host_int32((int32_t) header_v4.timelastsave);
		header.flags = header_v4.flags; /* will be byte-swapped below */
	}
	else {
		/* No valid header present (version 0 reserved for headerless legacy tables); fall back to legacy no-header behavior. */
		header.version = 0;
		header.flags = 0;
		header.sortorder = 0;
		header.timecreated = 0;
		header.timelastsave = 0;
	}

#if TABLE_HEADER_RESERVED_BYTES > 0
	if (header.version >= TABLE_HEADER_RESERVED_VERSION) {
		long needed = (long)TABLE_HEADER_RESERVED_BYTES;
		long total_bytes = gethandlesize(hrecords);
		if (total_bytes < ix + needed)
			goto L1;
		ix += needed;
	}
#endif

#if defined(FRONTIER_HEADLESS)
	fprintf(stderr, "[headless] hashunpacktable header version=%d sort=%d flags=0x%08x\n",
	        header.version,
	        disk_to_host_int16(header.sortorder),
	        disk_to_host_int32(header.flags));
#endif
	
	if (header.version > 0) { // a header has been written

		(**htable).sortorder = (short) disk_to_host_int16(header.sortorder);

		/* Handle timestamps based on version */
		if (header.version >= 0x05) {
			/* v0x05+: 64-bit timestamps, read big-endian format */
			/* Note: Using db_format_read_be64() instead of conditionallonglongswap() */
			/* for consistency with modern db_format code (both are functionally equivalent) */
			(**htable).timecreated = db_format_read_be64((const unsigned char *)&header.timecreated);
			(**htable).timelastsave = db_format_read_be64((const unsigned char *)&header.timelastsave);
		}
		else {
			/* v0x04 and earlier: 32-bit timestamps widened to 64-bit */
			/* Already converted during header read above */
			(**htable).timecreated = header.timecreated;
			(**htable).timelastsave = header.timelastsave;
		}

		if (header.version == 2) //5.0.1: forgot to initialize flags
			header.flags = 0;

		flsorted = true;
		}
	else {
		header.version = 0;

		header.flags = 0;

		(**htable).timecreated = (**htable).timelastsave = timenow (); //5.0.1

		ix = 0;
		}
	
	fl = false; /*default return value*/
	
	header.flags = disk_to_host_int32(header.flags);
	
	#ifdef xmlfeatures
		(**htable).flxml = (header.flags & flxml) != 0;
	#endif

	prevhashtable = sethashtable (htable); // pushhashtable (htable);
	
	++flunpackingtable;
	
	langtraperrors (bsunpackerror, &savecallback, &saverefcon); // hook errors so we can embellish

	/* Determine reader mode: use context if provided, else global state, then fall back to table header version.
	 * This ensures v7 databases always use v7 reader, even for tables with old header versions.
	 * CRITICAL FIX (Issue #123): Must check db_format_mode, not just header.version */
	boolean use_64bit_mode;
	if (ctx != NULL) {
		use_64bit_mode = ctx->mode.use_64bit_format;
	} else {
		use_64bit_mode = db_format_mode_current().use_64bit_format;
	}
	boolean v7_records = use_64bit_mode || (header.version >= tablediskversion);

#if defined(FRONTIER_HEADLESS)
	db_format_mode current_mode = db_format_mode_current();
	fprintf(stderr, "[headless] hashunpacktable_internal name='%.*s' use64=%d (ctx=%p ctx_mode=%d use_64bit_mode=%d current=%d || header.version=%d>=%d)\n",
	        (int) bsname[0], (char *) &bsname[1],
	        v7_records ? 1 : 0,
	        (void *) ctx, ctx ? (int) ctx->mode.use_64bit_format : -1,
	        use_64bit_mode ? 1 : 0,
	        current_mode.use_64bit_format ? 1 : 0,
	        header.version,
	        tablediskversion);
#endif

	long ixrecord = 0;
	while (true) {
			tydisksymbolrecord rec;
	/* Buffer mirrors the manual BE64 layout written in hashpackvisit_v7. */
	unsigned char recbuf[sizeof(tydisksymbolrecord_v7)];
			tyvaluerecord val;
			long remaining;
			boolean v7_rec = v7_records;
			int32_t name_index = 0;
			int64_t data_index64 = 0;
			tydiskvaluedata_v7 rec_data_v7;
			clearbytes(&rec_data_v7, sizeof(rec_data_v7));

			assert (sizeof (tydisksymbolrecord) == sizeof (tyOLD42disksymbolrecord));

			remaining = gethandlesize (hrecords) - ix;
			if (v7_records) {
				if (remaining < (long) sizeof (tydisksymbolrecord_v7)) /*out of records*/
					break;
				if (!loadfromhandle (hrecords, &ix, sizeof (recbuf), recbuf)) /*unexpected failure*/
					break;

				int32_t ix_be = 0;
				memcpy(&ix_be, recbuf + 0, sizeof(int32_t));
				name_index = disk_to_host_int32 (ix_be);
				rec.valuetype = recbuf[4];
				rec.version = recbuf[5];

				int64_t data_be = 0;
				memcpy(&data_be, recbuf + 8, sizeof(int64_t));
				data_index64 = (int64_t) disk_to_host_int64(data_be);
				memcpy(&rec_data_v7, recbuf + 8, sizeof(rec_data_v7));
				rec.data.longvalue = (int32_t) data_index64; /*for legacy debug printing paths*/
			}
			else {
				if (remaining < (long) sizeof (rec)) /*out of records*/
					break;
				if (!loadfromhandle (hrecords, &ix, sizeof (rec), &rec)) /*unexpected failure*/
					break;

				name_index = disk_to_host_int32(rec.ixkey);
				data_index64 = (int64_t) disk_to_host_int32 (rec.data.longvalue);
			}

//			disktomemshort (rec.valuetype);
//			disktomemshort (rec.version);

			if (!v7_rec && header.version < 2) // shift down from old bitfield position
				rec.version >>= 4;

			/* string/binary offsets remain 32-bit but are stored in a widened slot on disk */
			ixstrings = (long) data_index64;

			/* Inspect raw bytes before converting to a Pascal string. */
#if defined(FRONTIER_HEADLESS)
			{
				long hsize = gethandlesize(hstrings);
				if (name_index < 0 || name_index >= hsize) {
					fprintf(stderr, "[headless] hashunpacktable name ix OOB ix=%d hsize=%ld\n",
					        (int) name_index, hsize);
				} else {
					const unsigned char *s = (const unsigned char *)(*hstrings + name_index);
					unsigned int slen = s[0];
					long avail = hsize - name_index - 1;
					if ((long) slen > avail)
						slen = (unsigned int) (avail < 0 ? 0 : avail);
					fprintf(stderr, "[headless] hashunpacktable name bytes ix=%d len=%u: ",
					        (int) name_index, slen);
					for (unsigned int i = 0; i < slen && i < 32; ++i)
						fprintf(stderr, "%02x ", s[1 + i]);
					fprintf(stderr, "\n");
				}
			}
#endif

			hashunpackstring (hstrings, bsname, name_index);

#if defined(FRONTIER_HEADLESS)
			if (hashunpack_log != NULL) {
				unsigned int slen = (unsigned int) bsname[0];
				fprintf(hashunpack_log,
				        "rec=%ld name_ix=%d len=%u name=\"%.*s\" modern=%d data_ix=%lld\n",
				        ixrecord,
				        (int) name_index,
				        slen,
				        (int) slen,
				        (char *) (bsname + 1),
				        (int) v7_rec,
				        (long long) data_index64);
				fprintf(hashunpack_log, "  raw:");
				for (unsigned int i = 0; i < slen && i < 32; ++i)
					fprintf(hashunpack_log, " %02x", (unsigned char) bsname[1 + i]);
				fprintf(hashunpack_log, "\n");
				fflush(hashunpack_log);
			}

			fprintf(stderr, "[headless] hashunpacktable record ix=%ld name='%.*s' valuetype=%d version=%u use64=%d\n",
			        ixrecord,
			        bsname[0], bsname + 1,
			        (int) rec.valuetype,
			        (unsigned int) rec.version,
			        (int) v7_rec);
#endif

#if defined(FRONTIER_HEADLESS)
			if (debug_record_index++ < 10) {
				fprintf(stderr, "[headless] hashunpacktable record ixkey=%d type=%d version=%u data=0x%08llx name='%.*s'\n",
				        (int) name_index,
				        (int) rec.valuetype,
				        (unsigned int) rec.version,
				        (unsigned long long) data_index64,
				        (int) bsname[0],
				        (char *) &bsname[1]);
			}
#endif

			if (isemptystring (bsname)) /*skip junk*/
				continue;

			initvalue (&val, (tyvaluetype) rec.valuetype);
		if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_HASH)) {
			log_trace(LOG_COMP_HASH, "[unpack] name=%.*s type=%d version=%u raw_ix=0x%08llx ix=%ld",
				(int)bsname[0], bsname+1, val.valuetype, rec.version,
				(unsigned long long) data_index64, ixstrings);
		}

			switch (val.valuetype) {
				case oldstringvaluetype:
					hashunpackstring (hstrings, bsvalue, ixstrings);

					if (!newheapvalue (bsvalue + 1, (long) stringlength (bsvalue), stringvaluetype, &val))
						goto L1;

					exemptfromtmpstack (&val);

					break;

				case addressvaluetype:
					hashunpackstring (hstrings, bsvalue, ixstrings);

					if (!hashinsertaddress (bsname, bsvalue))
						goto L1;

					setemptystring (bsname); /*exception -- we've already inserted it*/

					break;

				case olddoublevaluetype: {
					Handle hextended;

					if (!hashunpackbinary (hstrings, &hextended, ixstrings))
						goto L1;

					pullfromhandle (hextended, 2, 2, nil); /*universal -> SANE*/

					val.valuetype = doublevaluetype;

					val.data.binaryvalue = hextended;

					break;
				}

	#ifdef oldMACVERSION
				case filespecvaluetype: { /*need to save as a (minimal) alias*/
					tyfilespec fs;
					boolean flresolved;
					Handle hbinary;

					if (!hashunpackbinary (hstrings, &hbinary, ixstrings))
						goto L1;

					if (rec.version > 0) { /*filespec is stored as an alias*/

						disablelangerror ();

						flresolved = aliastofilespec ((AliasHandle) hbinary, &fs);

						enablelangerror ();

						if (flresolved) {

							if (!sethandlecontents (&fs, filespecsize (fs), hbinary))
								goto L1;
						}
						else
							val.valuetype = aliasvaluetype;
					}

					val.data.binaryvalue = hbinary;

					break;
				}
	#endif

				case rectvaluetype: {
					diskrect **rdisk;
					Rect r;
					 
					if (!hashunpackbinary (hstrings, (Handle *) &rdisk, ixstrings))
						goto L1;

					diskrecttorect (*rdisk, &r);

					disposehandle ((Handle) rdisk);

					if (!newheapvalue (&r, sizeof (r), rectvaluetype, &val))
						goto L1;

					exemptfromtmpstack (&val);

					break;
				}

				case rgbvaluetype: { /* 2006-04-20 sethdill & aradke */
					diskrgb **rgbdisk;
					RGBColor rgb;
					 
					if (!hashunpackbinary (hstrings, (Handle *) &rgbdisk, ixstrings))
						goto L1;

					diskrgbtorgb (*rgbdisk, &rgb);

					disposehandle ((Handle) rgbdisk);

					if (!newheapvalue (&rgb, sizeof (rgb), rgbvaluetype, &val))
						goto L1;

					exemptfromtmpstack (&val);

					break;
				}

	#if noextended
				case doublevaluetype: {
					double x;
					extended80 **x80;
				 
					if (!hashunpackbinary (hstrings, (Handle *) &x80, ixstrings))
						goto L1;

					x = x80tod (*x80);
				 
					disposehandle ((Handle) x80);	// 1/22/97 dmb: this was a leak!

					if (!setdoublevalue (x, &val))
						goto L1;

					exemptfromtmpstack (&val);

					break;
				}
	#else
				case doublevaluetype:
	#endif

	#if oldWIN95VERSION
				case filespecvaluetype: // unpack normally
				case aliasvaluetype:    // *** needs xplat format
	#endif
				case stringvaluetype:
				case passwordvaluetype:
				case patternvaluetype:
				case binaryvaluetype: {
					if (!hashunpackscalar (hstrings, &val, ixstrings, v7_rec))
						goto L1;

					break;
				}

				case listvaluetype:
                case recordvaluetype:
#if !defined(FRONTIER_HEADLESS)
                    if (rec.version < 2) {
                        AEDesc aelist;

						if (!hashunpackscalar (hstrings, &val, ixstrings, v7_rec))
							goto L1;

						if (val.fldiskval) { //yikes! we have to resolve before converting
							Handle hbinary;

							if (!dbrefhandle (val.data.diskvalue, &hbinary))
								goto L1;

							dbpushreleasestack (val.data.diskvalue, val.valuetype);

							val.data.binaryvalue = hbinary;

							val.fldiskval = false;
						}

						{
						DescType typecode = typeAEList;

						if (val.valuetype == recordvaluetype)
							typecode = typeAERecord;

						newdescwithhandle (&aelist, typecode, val.data.binaryvalue);
						}

						if (!langipcconvertaelist (&aelist, &val))
							goto L1;

						AEDisposeDesc (&aelist);

                        exemptfromtmpstack (&val);
                    }
                    else {
                        if (!hashunpackbinary (hstrings, &hpacked, ixstrings))
                            goto L1;

                        if (!opunpacklist (hpacked, &val.data.listvalue))
                            goto L1;
                    }
#else
                    {
                        const char *ctx = (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>";
                        fprintf(stderr, "[headless] list unpack start path=%s ix=0x%08lx version=%u\n",
                                ctx, (unsigned long) ixstrings, (unsigned int) rec.version);
                    }

                    if (!hashunpackbinary (hstrings, &hpacked, ixstrings)) {
                        fprintf(stderr, "[headless] list hashunpackbinary failed ix=0x%08lx\n",
                                (unsigned long) ixstrings);
                        goto L1;
                    }

                    if (hpacked != nil) {
                        size_t dump = (size_t) gethandlesize(hpacked);
                        if (dump > 32) dump = 32;
                        if (dump > 0) {
                            unsigned char *bytes = (unsigned char *) *hpacked;
                            fprintf(stderr, "[headless] list packed bytes:");
                            for (size_t i = 0; i < dump; ++i)
                                fprintf(stderr, " %02x", bytes[i]);
                            fprintf(stderr, "\n");
                        }
                    }

                    if (!opunpacklist (hpacked, &val.data.listvalue)) {
                        fprintf(stderr, "[headless] opunpacklist returned false path=%s\n",
                                (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>");
                        goto L1;
                    }
#endif

                    break;

				case filespecvaluetype:
				case aliasvaluetype:

					if (rec.version < 2) {
						tyfilespec fs;
						boolean flresolved;
						Handle hbinary;

						if (!hashunpackbinary (hstrings, &hbinary, ixstrings))
							goto L1;

						if (rec.version > 0) { /*filespec is stored as an alias*/

							disablelangerror ();

							flresolved = aliastofilespec ((AliasHandle) hbinary, &fs);

							enablelangerror ();

							if (flresolved) {

								if (!sethandlecontents (&fs, filespecsize (fs), hbinary))
									goto L1;
							}
							else
								val.valuetype = aliasvaluetype;
						}

						val.data.binaryvalue = hbinary;

						break;
					}

					if (!hashunpackbinary (hstrings, &hpacked, ixstrings))
						goto L1;

					if (!langunpackfileval (hpacked, &val))
						goto L1;

					break;

				case objspecvaluetype: {
					Handle hobjspec;

					if (!hashunpackbinary (hstrings, &hobjspec, ixstrings))
						goto L1;

					if (gethandlesize (hobjspec) == 0) {

						disposehandle (hobjspec);

						hobjspec = nil;
					}

					val.data.objspecvalue = hobjspec;

					break;
				}

				case codevaluetype:
					if (!hashunpackbinary (hstrings, &hpacked, ixstrings))
						goto L1;

					if (!langunpacktree (hpacked, &val.data.codevalue))
						goto L1;

					break;

				case externalvaluetype: {
					hdlexternalhandle h;

					if (!hashunpackexternal (hstrings, flmemory, &h, ixstrings))
						goto L1;

					val.data.externalvalue = (Handle) h;

#if defined(FRONTIER_HEADLESS)
					if (h != nil) {
						dbaddress tableadr = (dbaddress) (**(hdlexternalvariable) h).variabledata;
						fprintf(stderr, "[headless] hashunpacktable external variabledata=0x%016llx\n",
						        (unsigned long long) tableadr);
					} else {
						fprintf(stderr, "[headless] hashunpacktable external variable nil\n");
					}
#endif

					break;
				}

				case booleanvaluetype:
					if (v7_rec) {
						diskvalue_to_value_v7 (&rec_data_v7, &val);
					} else if (header.version < 2) {
						int16_t rawbool = disk_to_host_int16 (rec.data.intvalue);
						val.data.flvalue = (rawbool != 0);
						val.data.chvalue = (unsigned char) (rawbool != 0);
					} else {
						diskvalue_to_value_legacy (&rec.data, &val);
					}

					break;

				case novaluetype:
				case charvaluetype:
				case intvaluetype:
				case tokenvaluetype:
				case pointvaluetype:
				case directionvaluetype:
				case longvaluetype:
				case ostypevaluetype:
				case enumvaluetype:
				case fixedvaluetype:
				case singlevaluetype:
				case datevaluetype:
					if (v7_rec)
						diskvalue_to_value_v7 (&rec_data_v7, &val);
					else
						diskvalue_to_value_legacy (&rec.data, &val);

					break;

				default:
					if (v7_rec)
						diskvalue_to_value_v7 (&rec_data_v7, &val);
					else
						diskvalue_to_value_legacy (&rec.data, &val);

					break;
			}

		if (!isemptystring (bsname)) { /*needs to be inserted*/
			boolean ok = hashinsert (bsname, val);
			if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_HASH)) {
				if (!ok) {
					log_trace(LOG_COMP_HASH, "[unpack] hashinsert failed name=%.*s type=%d", (int)bsname[0], bsname+1, val.valuetype);
				}
			}
			if (!ok)
				goto L1;
			}
		
		++ixrecord;
		
		if (hlastnode == nil)
			(**htable).hfirstsort = hnewnode;
		else
			(**hlastnode).sortedlink = hnewnode;
		
		hlastnode = hnewnode;
		} /*for*/
		
	fl = true; /*loop terminated, we will return true*/

	L1:

	if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_HASH)) {
		log_trace(LOG_COMP_HASH, "[unpack] L1 triggered name=%.*s", (int)bsname[0], bsname+1);
	}
	languntraperrors (savecallback, saverefcon, !fl);
	
	sethashtable (prevhashtable);
	
	--flunpackingtable;
	
	disposehandle (hrecords);
	
	disposehandle (hstrings);
	
	(**htable).fldirty = fldirty;
	
	if (!flsorted)
		hashresort (htable, nil);
	
	if (!fl)
		hashreporterror (hashunpackerror, bsname, bsunpackerror);
	
	return (fl);
	} /*hashunpacktable_internal*/


boolean hashunpacktable (Handle hpackedtable, boolean flmemory, hdlhashtable htable) {
	/* Wrapper for backward compatibility - uses global mode state */
	return hashunpacktable_internal (NULL, hpackedtable, flmemory, htable);
}


boolean hashcountitems (hdlhashtable htable, long *ctitems) {
	
	/*
	return the number of items in the indicated hash table.
	*/
	
	register hdlhashnode nomad = (**htable).hfirstsort;
	register long ct = 0;
	
	while (nomad != nil) {
		
		ct++;
		
		nomad = (**nomad).sortedlink;
		} /*while*/
		
	*ctitems = ct;
	
	return (true);
	} /*hashcountitems*/


boolean hashsortedsearch (hdlhashtable htable, const bigstring bsname, long *item) {
	
	/*
	search the sorted linked list attached to the indicated table, and return the
	index of the node having the indicated name.
	
	return false if there is no node with the name.
	
	2/6/91 dmb: handle array references
	*/
	
	register hdlhashnode nomad = (**htable).hfirstsort;
	register long ct = 0;
	
	/*
	short arrayindex;
	
	if (hashstringtoarrayindex (bsname, &arrayindex)) {
		
		*item = arrayindex - 1;
		
		return (true);
		}
	*/
	
	while (nomad != nil) {
		
		if (equalidentifiers ((**nomad).hashkey, bsname)) {
		
			*item = ct;
			
			return (true);
			}
		
		ct++;
		
		nomad = (**nomad).sortedlink;
		} /*while*/
		
	return (false);
	} /*hashsortedsearch*/
	
	
boolean hashgetnthnode (hdlhashtable htable, long n, hdlhashnode *hnode) {
	
	/*
	n is 0-based.  we return a handle to the node for the indicated item number.
	
	9/12/90 DW: add defensive driving -- don't crash when there are zero items in 
	the table and the caller is asking for info about item #0.
	
	4/18/91 dmb: fixed loop's nil test; used to crash when n was just out of range.
	*/
	
	register hdlhashnode nomad = (**htable).hfirstsort;
	register long ct = n;
	
	*hnode = nil;
	
	if (nomad == nil) /*defensive driving*/
		return (false);
	
	while (--ct >= 0) {
		
		nomad = (**nomad).sortedlink;
		
		if (nomad == nil) /*there aren't that many items in the table*/
			return (false);
		} /*for*/
	
	*hnode = nomad;
	
	return (true);
	} /*hashgetnthnode*/
	
	
boolean hashgetsortedindex (hdlhashtable htable, hdlhashnode hnode, long *idx) {
	
	/*
	traverse the sorted list for the indicated table, looking for the indicated node.
	
	if we find it, set *idx to its index, and return true.
	
	the index is 0-based.
	*/
	
	register hdlhashnode nomad = (**htable).hfirstsort;
	register long ct = 0;
	
	while (nomad != nil) {
		
		if (nomad == hnode) { /*found it*/
			
			*idx = ct;
			
			return (true);
			}
			
		nomad = (**nomad).sortedlink;
		
		ct++;
		} /*while*/
		
	return (false); /*not found*/
	} /*hashgetsortedindex*/


boolean hashgetiteminfo (hdlhashtable htable, long item, bigstring bsname, tyvaluerecord *val) {
	
	/*
	the item number is 0-based.  we return the name and value information for the
	indicated item number, returning false if there aren't that many items.
	
	4/3/92 dmb: must check for unresolved addresses here
	
	3/19/93 dmb: if bsname is nil, don't set it
	
	6/7/96 dmb: if val is nil, don't set it either. (For ODBEngine, but useful elsewhere)
	
	5.0a23 dmb: don't resolve the value if caller doesn't need it.
	*/
	
	hdlhashnode hnode;
	
	if (!hashgetnthnode (htable, item, &hnode))
		return (false);
	
	if (bsname != nil)
		gethashkey (hnode, bsname);
	
	if (val != nil) {
		
		if (!hashresolvevalue (htable, hnode))
			return (false);
		
		*val = (**hnode).val;
		}
	
	return (true);
	} /*hashgetiteminfo*/
	

#if !odbengine
boolean hashgetvaluestring (tyvaluerecord val, bigstring bs) {
	
	/*
	a special entrypoint for creating a string representation of a value, 
	something worth displaying.  you shouldn't put up an error dialog for any 
	of these coercions, and it's ok not to replicate all the info in the coercion.
	
	5/21/91 dmb: copy valuerecord before coercing, or we trash caller's value
	
	12/22/92 dmb: if an error occurs during string coercion, must clear temps
	
	2.1b2 dmb: deparse string values
	
	2.1b4 dmb: don't deparse quotes, just non-printing characters (pass chnul)

	5.0.1 dmb: deparse filespec and alias values
	*/
	
	disablelangerror ();
	
	switch (val.valuetype) {
		
		case novaluetype:
			langgetmiscstring (nilstring, bs);
			
			break;
		
		case charvaluetype:
			setstringwithchar (val.data.chvalue, bs);
			
			langdeparsestring (bs, chnul);
			
			break;
		
		case booleanvaluetype:
		case intvaluetype:
		case longvaluetype:
		case directionvaluetype:
		case datevaluetype:
		case ostypevaluetype:
		case pointvaluetype:
		case rectvaluetype:
		case rgbvaluetype:
		case patternvaluetype:
		case fixedvaluetype:
		case singlevaluetype:
		case doublevaluetype:
		case objspecvaluetype:
		case enumvaluetype:
		case listvaluetype:
		case recordvaluetype:
		
			if (copyvaluerecord (val, &val) && coercetostring (&val)) {
				
				pullstringvalue (&val, bs);
				
				releaseheaptmp ((Handle) val.data.stringvalue);
				
				break;
				}
			
			cleartmpstack (); /*clean up on error*/
			
			langgetmiscstring (errorstring, bs);
			
			break;
		
		case filespecvaluetype:
		case aliasvaluetype:
		
			if (copyvaluerecord (val, &val) && coercetostring (&val)) {
				
				pullstringvalue (&val, bs);
				
				releaseheaptmp ((Handle) val.data.stringvalue);

				langdeparsestring (bs, chnul);
				
				break;
				}
			
			cleartmpstack (); /*clean up on error*/
			
			langgetmiscstring (errorstring, bs);
			
			break;

		case addressvaluetype:
			getaddresspath (val, bs);
			
			if (!isemptystring (bs))
				insertchar ('@', bs);
			
			break;
		
		case stringvaluetype:
			pullstringvalue (&val, bs);
			
			langdeparsestring (bs, chnul);
			
			break;
		
		case binaryvaluetype: {
			register Handle h = val.data.binaryvalue;
			long cthex = gethandlesize (h) - sizeof (OSType);
			
			if (cthex == 0)
				langgetmiscstring (nilstring, bs);
			else
				bytestohexstring (*h + sizeof (OSType), cthex, bs);
			
			/*
			OSType typeid;
			bigstring bstype, bsdata;
			
			typeid = **(OSType **) h;
			
			switch (typeid) {
				
				case 'TEXT':
				case 's255':
					texttostring (*h + sizeof (OSType), gethandlesize (h) - sizeof (OSType), bs);
					
					break;
				
				default:
					bytestohexstring (*h + sizeof (OSType), gethandlesize (h) - sizeof (OSType), bs);
					
					break;
				}
			*/
			
			break;
			}
		
		case externalvaluetype:
			langexternalgetdisplaystring ((hdlexternalhandle) val.data.externalvalue, bs);
			
			break;
		
		
		case codevaluetype:
			parsenumberstring (langmiscstringlist, treesizestring, langcounttreenodes (val.data.codevalue), bs);
			
			break;
		
		case tokenvaluetype:
			langgetmiscstring (tokennumberstring, bs);
			
			pushint (val.data.tokenvalue, bs);
			
			break;
		
		
		default:
			langgetmiscstring (unknownstring, bs);
		} /*switch*/
	
	enablelangerror ();
	
	return (true);
	} /*hashgetvaluestring*/
#endif

boolean hashgettypestring (tyvaluerecord val, bigstring bstype) {
	
	switch (val.valuetype) {
		
		case novaluetype:
			langgetmiscstring (nonestring, bstype);
			
			break;
		
		case intvaluetype: case longvaluetype:
			langgetmiscstring (numberstring, bstype);
			
			break;
		
		/*
		case tokenvaluetype:
			copystring ((ptrstring) "\pverb", bstype);
			
			break;
		
		case codevaluetype:
			copystring ((ptrstring) "\phandler", bstype);
			
			break;
		*/
		
		case externalvaluetype:
			langexternaltypestring ((hdlexternalhandle) val.data.externalvalue, bstype);
			
			break;
		
		default:
			langgettypestring (val.valuetype, bstype);
			
			break;
		} /*switch*/
	
	return (true);
	} /*hashgettypestring*/


boolean hashgetsizestring (const tyvaluerecord *val, bigstring bssize) {
	
	long size = 0;
	
	/*
	the size string tells you how much storage is allocated for the value.  if it
	returns non-empty, you might want to display it along with the type string.
	
	we only return non-empty size strings for types where size is interesting.  not
	for chars, ints, longs, booleans, dates, etc.
	*/
	
	switch ((*val).valuetype) {
		
		case stringvaluetype:
		case listvaluetype:
		case recordvaluetype:
			if (langgetvalsize (*val, &size))
				numbertostring (size, bssize);
			
			break;
		

		case binaryvaluetype: {
			register Handle h = (*val).data.binaryvalue;
			OSType typeid;
			
			typeid = getbinarytypeid (h);
			
			ostypetostring (typeid, bssize);
			
			break;
			}
		
		default:
			setemptystring (bssize); /*often this string stays empty*/
			
			break;
		} /*switch*/
	
	return (true);
	} /*hashgetsizestring*/


boolean hashvaltostrings (tyvaluerecord val, bigstring bstype, bigstring bsvalue, bigstring bssize) {
	
	/*
	give me a value record and I'll return three strings suitable for display.
	
	the first string indicates the type of the value, and the second is the value.
	
	the third string tells you how much storage is allocated for the value.  if it
	returns non-empty, you might want to display it along with the type string.
	
	we only return non-empty size strings for types where size is interesting.  not
	for chars, ints, longs, booleans, dates, etc.
	
	10/7/91 dmb: for performance, don't call getvalsize for addresses -- we can 
	do a quick calc with the value string, instead of regenerating the full path.
	*/
	
	hashgetvaluestring (val, bsvalue);
	
	hashgettypestring (val, bstype);
	
	hashgetsizestring (&val, bssize);
	
	return (true);
	} /*hashvaltostrings*/
