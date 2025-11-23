/* Header file for pgSelection.c  */


#ifndef PGSELECT_H
#define PGSELECT_H

#include "Paige.h"


/* t_select flags: */

#define SELECTION_DIRTY			0x0001		/* Exact line/caret is not valid */
#define END_LINE_FLAG			0x0002		/* Insertion is at line end */
#define VERTICAL_FLAG			0x0004		/* Selection is vertical */
#define VERTICAL_CTL_FLAG		0x0008		/* Selection is vertical controls */
#define WORD_FLAG				0x0010		/* Selection was in words */
#define SELECT_ON_CTL			0x0020		/* Selection was in a control */
#define SELECT_IN_TABLE			0x0040		/* Selection is in a table */

#define CLR_END_LINE_FLAG		(~END_LINE_FLAG)		/* Clears END_LINE_FLAG */
#define CLR_LINE_GOOD_BIT		(~LINE_GOOD_BIT)		/* Clears "LINE_GOOD" */

#define DEACT_BITS	(DEACT_BIT | PERM_DEACT_BIT)	/* Any deactivate flags */
#define CLR_INIT_SELECT			(VERTICAL_FLAG | VERTICAL_CTL_FLAG | WORD_FLAG | SELECT_ON_CTL)

#define MINIMUM_SELECT_MEMSIZE	2			/* Minimum SetMemorySize for pg->select */

#define MAX_HILITE_OFFSETS		4			/* Maximum mixed-direction hilites */

#define FIND_POINT_INSET		8			/* Amount inset for "slop" when point not found for select */
#define EXTRA_VIS				64			/* Extra vis for assured pagination */

enum {
	align_char_best,
	align_char_right,
	align_char_left
};

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (void) pgSetInsertSelect (paige_rec_ptr pg, long offset);
extern PG_PASCAL (void) pgCalcSelect (paige_rec_ptr pg, t_select_ptr selection);
extern PG_PASCAL (void) pgTurnOffHighlight (paige_rec_ptr pg, pg_boolean set_port);
extern PG_PASCAL (void) pgRemoveAllHilites (paige_rec_ptr pg, short will_draw_mode);
extern PG_PASCAL (void) pgDrawHighlight (paige_rec_ptr pg, short cursor_verb);
extern PG_PASCAL (pg_boolean) pgHighlightShowing (paige_rec_ptr pg);
extern PG_PASCAL (pg_short_t) pgPixelToPage (pg_ref pg, co_ordinate_ptr pixel,
			co_ordinate_ptr page_offset, long PG_FAR *p_width, long PG_FAR *p_height,
			long PG_FAR *max_bottom, pg_boolean include_scroll);
extern PG_PASCAL (pg_boolean) pgPtInPageArea (paige_rec_ptr pg, co_ordinate_ptr point,
		co_ordinate_ptr offset_extra, short PG_FAR *view_flags );
extern PG_PASCAL (void) pgBuildHiliteRgn (paige_rec_ptr pg, t_select_ptr selections,
		pg_short_t select_qty, shape_ref rgn);
extern PG_PASCAL (pg_boolean) pgAlignSelection (paige_rec_ptr pg, short align_verb, size_t PG_FAR *select_offset);
extern PG_PASCAL (pg_short_t) pgFindEmptyHilite (paige_rec_ptr pg, pg_short_t PG_FAR *rec);
extern PG_PASCAL (void) pgInvalTextMeasure (paige_rec_ptr pg_rec, long select_from,
			long select_to);
extern PG_PASCAL (void) pgConvertInputPoint (paige_rec_ptr pg, co_ordinate_ptr in_point,
		co_ordinate_ptr offset_extra, co_ordinate_ptr out_point);
extern PG_PASCAL (void) pgInvalidateHilite (paige_rec_ptr pg_rec, pg_boolean remove_caret);


#ifdef __cplusplus
	}
#endif

#endif
