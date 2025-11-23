/* This header file goes with pgParagraph.h     */

#ifndef PGDEFPAR_H
#define PGDEFPAR_H

#include "Paige.h"

#define MINIMUM_TAB_SPACE			1		/* Lowest I can go for a tab   */

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (long) pgGetLeftTabBase (paige_rec_ptr pg, long line_left);
extern PG_PASCAL (long) pgGetJExtra (point_start_ptr line_start);
extern PG_PASCAL (pg_boolean) pgTiedParagraphRange (paige_rec_ptr pg, select_pair_ptr range);
extern PG_PASCAL (void) pgScaleParInfo (paige_rec_ptr pg, par_info_ptr par, short numerator, short denominator);
extern PG_PASCAL (void) pgAdjustBorders (par_info_ptr par_style, long PG_FAR *left_extra, long PG_FAR *right_extra);
extern PG_PASCAL (pg_boolean) pgGetBorderInfo (paige_rec_ptr pg, text_block_ptr block,
			point_start_ptr line_start, rectangle_ptr border_box, rectangle_ptr border_mask,
			long PG_FAR *shading, long PG_FAR *border_bits, long PG_FAR *border_top_extra,
			long PG_FAR *border_bottom_extra);
extern PG_PASCAL (void) pgHalfPenSize (long PG_FAR *pensize);

#ifdef PG_DEBUG
extern PG_PASCAL (void) pgCheckRunIntegrity (paige_rec_ptr pg);
#endif

#ifdef __cplusplus
	}
#endif

#endif
