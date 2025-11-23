/* Header file for text_block handling */

#ifndef PGTXTBLK_H
#define PGTXTBLK_H

#include "Paige.h"

/* smart_update_flags:  */

#define CAN_DRAW_1_LINE			1		/* Single line draw OK */
#define CAN_DRAW_PARTIAL_LINE	2		/* Can draw a single char (at end of line) */

#define J_EXTRA_MASK			0x1FFF	/* Mask for extra field in point_start */
#define EXTRA_IS_SUBREF			0x2000	/* Extra field = pg_subref record */
#define J_EXTRA_NEGATIVE		0x4000	/* Justify-extra is negative */
#define EXTRA_IS_TAB			0x8000	/* Extra field = tab record & J_EXTRA_MASK */
#define EXTRA_NOT_EXTRA			(EXTRA_IS_SUBREF | EXTRA_IS_TAB)

#define TEXT_PURGE_STATUS		32		/* Status for text purge  */
#define LINE_PURGE_STATUS		64		/* Status for line purging */

#define LOW_TEXT_PURGE_STATUS	8		/* Minimum status for text purge  */
#define LOW_LINE_PURGE_STATUS	16		/* Minimum status for line purging */

#define NO_BLOCK_OFFSET			-1		/* Used in char_info when block offset is unknown */

/* File cache flags (in text_block) */

#define CACHE_CHANGED_FLAG		0x0001	/* Text has changed, cannot unload */
#define CACHE_SAVED_FLAG		0x0002	/* Text has been saved to new file */
#define CACHE_UNICODE_FLAG		0x0004	/* Original text (from Read file) was unicode */
#define CACHE_LOCK_FLAG			0x0008	/* Do not purge block if this bit is set. */

/* Misc. flags: */

#define ANY_CALC			(NEEDS_CALC | NEEDS_PAGINATE)
#define CLEAR_CALC			(~(NEEDS_CALC | NEEDS_PAGINATE | BOUNDS_GUESSED | LINES_PURGED | SOME_LINES_BAD | SWITCHED_DIRECTIONS))		/* Clears recalc & paginate flags */
#define CLEAR_BROKE_FLAG	(~BROKE_BLOCK)	/* Clears BROKE_BLOCK flag */
#define CLEAR_INITIAL_FLAGS (~(ALL_TEXT_HIDDEN | LINES_NOT_HORIZONTAL | NO_CR_BREAK))
#define GOD_AWFUL_HUGE		0x7FFFFFF0
#define UNKNOWN_LINE_QTY	0xFFFF			/* When # of lines for smart display uncertain */

struct tab_width_info {
	long			offset;					/* Text position relative to start */
	long			width;					/* Effective width when measuring */
	long			max_width;				/* Max used for exclusions */
	long			exclude_right;			/* Right edge for exclusions */
	long			text_width;				/* Actual text width for exclusions */
	long			hard_width;				/* Hard character width */
	pg_short_t		extra;					/* Info from tab_width proc */
	short			type;					/* Recorded type (record used for other chars) */
};
typedef struct tab_width_info tab_width_info;
typedef tab_width_info PG_FAR *tab_width_ptr;

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (void) pgInitTextblock (paige_rec_ptr pg_rec, size_t offset_begin,
			text_ref the_text, text_block_ptr block, pg_boolean cache_text);
extern PG_PASCAL (text_block_ptr) pgFindTextBlock (paige_rec_ptr pg_rec, size_t offset,
			pg_short_t PG_FAR *block_num, pg_boolean want_build,
			pg_boolean will_access_text);
extern PG_PASCAL (void) pgPaginateBlock (paige_rec_ptr pg, text_block_ptr block,
			smart_update_ptr update_info, pg_boolean will_access_text);
extern PG_PASCAL (void) pgPaginateStarts (paige_rec_ptr pg, text_block_ptr block, size_t initial_offset,
		smart_update_ptr update_info);
extern PG_PASCAL (pg_short_t) pgLineBoundary (paige_rec_ptr pg, text_block_ptr block,
			pg_short_t PG_FAR *from_start, pg_short_t PG_FAR *line_begin_offset,
			pg_short_t *line_end_offset);
extern PG_PASCAL (long) pgRectFromSelection (paige_rec_ptr pg, text_block_ptr block,
		t_select_ptr selection, rectangle_ptr line_rect, short PG_FAR *baseline);
extern PG_PASCAL (short) pgFixBadBlocks (paige_rec_ptr pg);
extern PG_PASCAL (pg_char_ptr) pgTextFromOffset (paige_rec_ptr pg, size_t offset,
		text_ref PG_FAR *the_ref, size_t PG_FAR *max_length);
extern PG_PASCAL (void) pgBlockToUnicode (paige_rec_ptr pg, text_block_ptr block);
extern PG_PASCAL (void) pgUnicodeToBlock (paige_rec_ptr pg, text_block_ptr block);
extern PG_PASCAL (void) pgResetStartingLine (pg_measure_ptr line_stuff);
extern PG_PASCAL (void) pgCalcTableSpace (paige_rec_ptr pg, text_block_ptr block);
extern PG_PASCAL (void) RebuildTextblock (paige_rec_ptr pg, text_block_ptr block, smart_update_ptr update_info);
extern PG_PASCAL (void) pgBuildFakeBlock (paige_rec_ptr pg, pg_measure_ptr line_stuff);
extern PG_PASCAL (void) pgAdjustDirection (paige_rec_ptr pg, text_block_ptr block);
extern PG_PASCAL (void) pgLineUnionRect (point_start_ptr starts, rectangle_ptr target);

#ifdef __cplusplus
	}
#endif


#endif

