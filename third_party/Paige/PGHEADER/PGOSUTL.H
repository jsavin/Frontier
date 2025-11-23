#ifndef PGOSUTL_H
#define PGOSUTL_H

#include "Paige.h"
#include "pgTraps.h"

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (long) pgSameCaseLength (paige_rec_ptr pg, style_walk_ptr walker,
		pg_char_ptr data, long global_offset, long length,
		long PG_FAR *info_flags);
extern PG_PASCAL (memory_ref) pgConvertTextCaps (paige_rec_ptr pg, style_info_ptr cur_style,
		pg_char_ptr text, long length);
extern PG_PASCAL (short) pgComputePointSize (paige_rec_ptr pg, style_info_ptr style);
extern PG_PASCAL (void) pgDrawSpecialUnderline (paige_rec_ptr pg, Point from_pt,
		short distance, style_info_ptr style, short draw_bits);
extern PG_PASCAL (pg_short_t) pgCountCtlChars (text_block_ptr block, pg_short_t ctl_char);
extern PG_PASCAL (pg_boolean) pgVirtualScaling (paige_rec_ptr pg, pg_scale_ptr in_scaling,
		pg_scale_ptr out_scaling);
extern PG_PASCAL (void) pgBuildPageRegion (paige_rec_ptr pg, pg_scale_ptr scaler, pg_region rgn);
extern PG_PASCAL (pg_boolean) pgBuildExclusionRegion (paige_rec_ptr pg, pg_scale_ptr scaler, pg_region rgn);
extern PG_PASCAL (pg_boolean) pgVisRegionChanged (paige_rec_ptr pg, pg_boolean update_to_current);

#ifndef MAC_PLATFORM
#ifndef WINDOWS_EMULATE_COMPILE
extern PG_PASCAL (int) EmptyRgn (RgnHandle rgn);
extern PG_PASCAL (void) InsetRgn(RgnHandle rgn, short dh, short dv);
#endif
#endif

extern PG_PASCAL (pg_p_string_ptr) pgCStr2PStr(pg_p_string_ptr p_str, const pg_c_string_ptr c_str);
extern PG_PASCAL (pg_c_string_ptr) pgPStr2CStr(pg_c_string_ptr c_str, const pg_p_string_ptr p_str);

#ifdef NO_C_INLINE
extern PG_PASCAL (pg_time_t) pgGetPlatformTime(pg_time_t pg_secs);
extern PG_PASCAL (pg_time_t) pgGetPaigeTime(pg_time_t platform_secs);
#else
#define pgGetPlatformTime(pg_secs)		(pg_time_t)((pg_secs) - PLATFORM_DELTA_TIME)
#define pgGetPaigeTime(platform_secs)	(pg_time_t)((platform_secs) + PLATFORM_DELTA_TIME)
#endif

#ifndef C_LIBRARY
extern PG_PASCAL (long) pgCStrLength (const pg_c_string_ptr str);
#else
#if defined(UNIX_PLATFORM)
extern PG_PASCAL (long) pgCStrLength (const pg_c_string_ptr str);
#elif !defined(WINDOWS_PLATFORM)
#define pgCStrLength(s)	strlen(s)
#else
#define pgCStrLength(s)	lstrlen((LPCTSTR)s)
#endif
#endif

#ifdef __cplusplus
	}
#endif


#endif
