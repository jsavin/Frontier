/* Header file for pgEdit */

#ifndef PGEDIT_H
#define PGEDIT_H

#include "Paige.h"
#include "pgText.h"

#define DRAW_TO_END			-1
#define CLR_SOME_LINE_GOOD	(~SOME_LINES_GOOD)			/* Clears "SOME_LINES_GOOD"  */
#define MAX_KEY_CHECK		256								/* When to check keys for block overflow */
#define CLR_DOC_TERMINATED	(~DOC_TERMINATED_BIT)		/* Clears doc termination */
#define DISPLAY_SLOP_EXTRA	24

typedef enum {
	sp_erase_all_pages = -32
}special_draw_verb;


#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (long) pgUpdateText (paige_rec_ptr pg, graf_device_ptr draw_port,
		size_t beginning_offset, size_t ending_offset, shape_ref vis_rgn,
		co_ordinate_ptr offset_extra, short display_verb, short hilite_mode);
extern PG_PASCAL (void) pgDoDisplay (pg_ref pg, select_pair_ptr range, const graf_device_ptr target_device,
		shape_ref vis_target, shape_ref wrap_target, const co_ordinate_ptr offset_extra,
		short draw_mode);
extern PG_PASCAL (long) pgInsertRawData (paige_rec_ptr pg, pg_char_ptr data, size_t length,
			size_t offset, smart_update_ptr update_info, short insert_mode);
extern PG_PASCAL (long) pgDeleteRawData (paige_rec_ptr pg, size_t delete_from, size_t PG_FAR *delete_to,
		smart_update_ptr update_info);
extern PG_PASCAL (graf_device_ptr) pgSetupGrafDevice (paige_rec_ptr pg, graf_device_ptr device,
		shape_ref intersect_shape, short clip_verb);
extern PG_PASCAL (long) pgCurrentInsertion (paige_rec_ptr pg);
extern PG_PASCAL (void) pgDrawPages (paige_rec_ptr pg, graf_device_ptr device,
		co_ordinate_ptr offset_extra, shape_ref vis_rgn, short draw_mode_used);
extern PG_PASCAL (pg_short_t) pgStripTrailingBlanks (paige_rec_ptr pg, point_start_ptr related_start,
		style_walk_ptr styles, pg_char_ptr text, pg_short_t text_offset,
		pg_short_t text_length, long j_extra);
extern PG_PASCAL (void) pgDisplaySubset (paige_rec_ptr pg, rectangle_ptr draw_bounds,
			co_ordinate_ptr vis_offset);
extern PG_PASCAL (void) pgInvalidateLine (paige_rec_ptr pg, long offset, short draw_mode);
extern PG_PASCAL (void) pgSetTerminatorFlag (paige_rec_ptr pg);


#ifdef __cplusplus
	}
#endif

#endif

