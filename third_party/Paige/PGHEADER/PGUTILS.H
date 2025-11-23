/* Header file for pgUtils. We realize some of these functions are re-inventions
of the wheel since they commonly exist in some standard C libraries. However,
for true portability we cannot assume any of those libraries exist with this code. */

/* Updated by TR Shaw, OITC, Inc. 20 Apr for C/C++/Pascal linkage and for inlines */


#ifndef PGUTILS_H
#define PGUTILS_H


#include "Paige.h"

#ifdef C_LIBRARY
#include <stdlib.h>
#endif

#define MAX_INTEGER			16380
#define MAX_MINUS_INTEGER	-16380

#ifdef __cplusplus
extern	"C" {
#endif

enum {
	call_for_copy,
	call_for_delete,
	call_for_activate
};

#ifdef NO_C_INLINE
PG_PASCAL (long) pgMin (long value1, long value2);
PG_PASCAL (long) pgMax (long value1, long value2);
#else
#define pgMin(value1,value2)	((value1 < value2) ? value1 : value2)
#define pgMax(value1,value2)	((value1 > value2) ? value1 : value2)
#endif

extern PG_PASCAL (short) pgLongToShort (long value);
extern PG_PASCAL (void) pgOffsetRect (rectangle_ptr rect, long h, long v);
extern PG_PASCAL (void) pgUnionRect (rectangle_ptr r1, rectangle_ptr r2, rectangle_ptr r3);
extern PG_PASCAL (void) pgTrueUnionRect (rectangle_ptr r1, rectangle_ptr r2, rectangle_ptr r3);
extern PG_PASCAL (short) pgSectRect (rectangle_ptr r1, rectangle_ptr r2, rectangle_ptr r3);
extern PG_PASCAL (pg_boolean) pgSectOrTouchRect (rectangle_ptr r1, rectangle_ptr r2, rectangle_ptr r3);
extern PG_PASCAL (pg_boolean) pgEmptyRect (rectangle_ptr rect);
extern PG_PASCAL (pg_boolean) pgTrulyEmptyRect (rectangle_ptr rect);
extern PG_PASCAL (pg_boolean) pgPtInRect (co_ordinate_ptr point, rectangle_ptr rect);
extern PG_PASCAL (pg_boolean) pgPtInRectInset (co_ordinate_ptr point, rectangle_ptr rect,
			co_ordinate_ptr inset_extra, pg_scale_ptr scaling);
extern PG_PASCAL (void) pgLineBounds (point_start_ptr first_start, rectangle_ptr bounds);
extern PG_PASCAL (void) pgDistanceToRect (co_ordinate_ptr point, rectangle_ptr rect,
			co_ordinate_ptr distance);
extern PG_PASCAL (pg_boolean) pgEqualStruct (void PG_FAR *rec1, void PG_FAR *rec2, size_t length);
extern PG_PASCAL (pg_boolean) pgEqualStructMasked (void PG_FAR *rec1, void PG_FAR *rec2,
			void PG_FAR *mask, long length);
extern PG_PASCAL (pg_boolean) pgZeroStruct (void PG_FAR *rec, pg_short_t length);
extern PG_PASCAL (pg_boolean) pgCoversRect (rectangle_ptr r1, rectangle_ptr r2);
extern PG_PASCAL (void) pgInsetRect (rectangle_ptr rect, long h, long v);
extern PG_PASCAL (void) pgSwapLongs (long PG_FAR *lessor_val, long PG_FAR *greater_val);
extern PG_PASCAL (size_t) pgFixOffset (paige_rec_ptr pg, size_t wanted_offset);
extern PG_PASCAL (void) pgResetDocBounds (paige_rec_ptr pg);
extern PG_PASCAL (void) pgCallTextHook (paige_rec_ptr pg, paige_rec_ptr src_option,
		short reason_verb, long starting_offset, long length, short verb,
		short front_back_state, short perm_state, pg_boolean show_hilite);
extern PG_PASCAL (void) pgWillDeleteFormats (paige_rec_ptr pg, pg_globals_ptr globals,
		short reason_verb, format_ref text_formats, par_ref par_formats);
extern PG_PASCAL (void) pgComputeDocHeight (paige_rec_ptr pg, pg_boolean paginate);
extern PG_PASCAL (void) pgEraseContainerArea (paige_rec_ptr pg, pg_scale_ptr scale_factor,
		co_ordinate_ptr offset_extra, shape_section_ptr top_start,
		shape_section_ptr bottom_end);
extern PG_PASCAL (void) pgCallContainerProc (paige_rec_ptr pg, pg_short_t container_num,
		co_ordinate_ptr offset_extra, short verb, pg_scale_ptr scale_factor,
		void PG_FAR *extra_info);
extern PG_PASCAL (void) pgScrollParamValues (paige_rec_ptr pg, long PG_FAR *h_max, long PG_FAR *v_max,
		long PG_FAR *scroll_factor_h, long PG_FAR *scroll_factor_v,
		short PG_FAR *max_h_result, short PG_FAR *max_v_result);
extern PG_PASCAL (paige_rec_ptr) pgUseSharedPg (pg_ref PG_FAR *pg);
extern PG_PASCAL (void) pgScaleShort (long scale_factor, long origin, short PG_FAR *value);
extern PG_PASCAL (void) pgScaleDocument (paige_rec_ptr pg, short old_resolution,
		short new_resolution, pg_boolean scale_page_rect);
extern PG_PASCAL (void) pgScaleVirtualRect (paige_rec_ptr pg, rectangle_ptr rect, short old_res, short new_res);
extern PG_PASCAL (long) pgGetOSConstant (long platform);


#ifndef NO_C_INLINE
#define pgLoWord(value) ((short)value)
#define pgHiWord(value) (short)((long) value >> 16)
#define pgPushMemoryID(pg)	(pg)->globals->mem_globals->current_id = (pg)->mem_id
#define pgPopMemoryID(pg)	(pg)->globals->mem_globals->current_id = \
								(pg)->globals->mem_globals->next_mem_id
#define pgAddPt(pt1,pt2) {(pt2)->h += (pt1)->h; (pt2)->v += (pt1)->v;}
#define pgSubPt(pt1,pt2) {(pt2)->h -= (pt1)->h; (pt2)->v -= (pt1)->v;}
#define pgNegatePt(pt) {(pt)->h = -(pt)->h; (pt)->v = -(pt)->v;}

#else
extern PG_PASCAL (short) pgLoWord(long value);
extern PG_PASCAL (short) pgHiWord(long value);
extern PG_PASCAL (void) pgPushMemoryID (paige_rec_ptr pg);
extern PG_PASCAL (void) pgPopMemoryID (paige_rec_ptr pg);
extern PG_PASCAL (void) pgAddPt (co_ordinate_ptr pt1, co_ordinate_ptr pt2);
extern PG_PASCAL (void) pgSubPt (co_ordinate_ptr pt1, co_ordinate_ptr pt2);
extern PG_PASCAL (void) pgNegatePt (co_ordinate_ptr point);
#endif

#ifdef C_LIBRARY
#define pgAbsoluteValue(value)  labs(value)
#else
extern PG_PASCAL (long) pgAbsoluteValue (long value);
#endif

#ifdef __cplusplus
	}
#endif


#endif
