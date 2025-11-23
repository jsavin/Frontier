/* Header file that goes with pgStyles.c */

#ifndef PGDEFSTL_H
#define PGDEFSTL_H

#include "Paige.h"

#define ZERO_TEXT_PAD		2		/* What goes at end of zero-text run */
#define IS_STYLE_SHEET		2		/* "flag" that define style_info as stylesheet */
#define HAS_FORCED_WIDTHS	0x4000	/* Charwidths are forced from app */
#define MAINTENANCE_MASK	0xF000	/* Bits that we clear after adding style */

#define SIGNIFICANT_STYLE_SIZE	(sizeof(style_info) - sizeof(long) - sizeof(long))
#define SIGNIFICANT_PAR_STYLE_SIZE (sizeof(par_info) - sizeof(long) - sizeof(long) - sizeof(long))
#define SIGNIFICANT_FONT_SIZE sizeof(font_info)
#define FORMAT_STYLE_SIZE SIGNIFICANT_STYLE_SIZE - sizeof(pg_style_hooks)
#define PAR_STYLE_SIZE SIGNIFICANT_PAR_STYLE_SIZE - sizeof(pg_par_hooks)

#define COMPARE_TRUE	-1	/* Used in pgSetMaskFromFlds for mask set when == */
#define COMPARE_FALSE	0	/* Used in pgSetMaskFromFlds for mask set when != */

#define BINARY_TABLE_MINIMUM	32		/* Minimum quantity for binary search */

enum {
	style_compare,		/* Compare style verb */
	par_compare,		/* Compare par style verb */
	font_compare		/* Compare font verb */
};


struct change_info {
	paige_rec_ptr		pg;					/* paige rec associated */
	style_info_ptr		style_change;		/* Style record to change */
	style_info_ptr		style_mask;			/* Style mask */
	par_info_ptr		par_change;			/* Par info to change */
	par_info_ptr		par_mask;			/* Par mask */
	font_info_ptr		font_change;		/* Used by pgGetFontInfo */
	font_info_ptr		font_mask;			/*  "   "     "          */
	tab_stop_ptr		tab_change;			/* Tab stop record(s) to change */
	long PG_FAR			*tab_mask;			/* Mask for tab(s) change */
	long				num_tabs;			/* Number of tabs in tab(s) change */
	select_pair			change_range;		/* Lowest/highest changed offsets */
	void PG_FAR			*base;				/* Used for "get" procs  */
	short				changed;			/* TRUE if text really changed */
	short				any_match;			/* "set_any_match" param for get's */
	long				class_AND;			/* Used for get-class-info */
	long				class_OR;			/* Used for get-class-info */
	pg_boolean			tables;				/* TRUE if only table records affected */
	pg_boolean			non_tables;			/* TRUE if only non-table records affected */
};

typedef struct change_info change_info;
typedef change_info PG_FAR *change_info_ptr;


typedef pg_short_t (*style_change_proc) (change_info_ptr change, style_run_ptr style_to_change);
typedef void (*style_get_proc) (change_info_ptr report, style_run_ptr style_to_get,
			pg_short_t record_ctr);


/* The style_change_proc is used to alter a set of style_info or par_info record(s).
When called, the style_to_change param is the "old" style_info record. The change
parameter contains all the necessary items for the function to operate (such as
the style_info record to change and its mask). THIS FUNCTION MUST NOT ACTUALLY
ALTER THE STYLE_RUN, nor should it alter the used_ctr of any style records. Rather,
it inserts new style records if needed, and in any event always returns the new
style_item number.   */

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (style_info_ptr) pgFindTextStyle (paige_rec_ptr pg_rec, size_t offset);
extern PG_PASCAL (style_info_ptr) pgLocateStyleSheet (paige_rec_ptr pg, short style_id,
		pg_short_t PG_FAR *style_index);
extern PG_PASCAL (par_info_ptr) pgLocateParStyleSheet (paige_rec_ptr pg, short style_id);
extern PG_PASCAL (style_run_ptr) pgFindStyleRun (paige_rec_ptr pg_rec, size_t offset,
		size_t PG_FAR *rec_num);
extern PG_PASCAL (par_info_ptr) pgFindParStyle (paige_rec_ptr pg_rec, size_t offset);
extern PG_PASCAL (style_run_ptr) pgFindParStyleRun (paige_rec_ptr pg_rec, size_t offset,
		size_t PG_FAR *rec_num);
extern PG_PASCAL (void) pgSetInsertionStyles (pg_ref pg, const style_info_ptr style, const font_info_ptr font);
extern PG_PASCAL (void) pgSetNextInsertIndex (paige_rec_ptr pg_rec);
extern PG_PASCAL (pg_short_t) pgGetInsertionStyle (paige_rec_ptr pg_rec);
extern PG_PASCAL (void) pgSetupInsertStyle(paige_rec_ptr pg_rec);
extern PG_PASCAL (void) pgCleanupStyleRuns (paige_rec_ptr pg, pg_boolean style_changed,
		pg_boolean par_changed);
extern PG_PASCAL (memory_ref) pgSetupOffsetRun (paige_rec_ptr pg, select_pair_ptr selection,
		pg_boolean for_paragraph, pg_boolean smart_select);
extern PG_PASCAL (pg_short_t) pgFindMatchingStyle (memory_ref ref_to_use, void PG_FAR *rec_to_match,
		pg_short_t start_rec, pg_short_t match_size);
extern PG_PASCAL (void) pgGetPadStyles (paige_rec_ptr pg, long pad_offset,
		pg_short_t PG_FAR *style_item, pg_short_t PG_FAR *par_item,
		pg_boolean increment_used);
extern PG_PASCAL (void) pgSetPadStyles (paige_rec_ptr pg, pg_short_t style_item,
		pg_short_t par_item, long pad_style_offset);
extern PG_PASCAL (short) pgAddNewFont (paige_rec_ptr pg, const font_info_ptr font);
extern PG_PASCAL (pg_short_t) pgAddStyleInfo (paige_rec_ptr pg, paige_rec_ptr src_option,
		short reason_verb, style_info_ptr style);
extern PG_PASCAL (pg_short_t) pgAddParInfo (paige_rec_ptr pg, paige_rec_ptr src_option,
		short reason_verb, par_info_ptr style);
extern PG_PASCAL (void) pgReturnStyleInfo (style_ref ref, change_info_ptr report, style_get_proc get_proc,
		long from_offset, long to_offset);
extern PG_PASCAL (void) pgChangeStyleRun (style_ref ref, change_info_ptr change, style_change_proc change_proc,
		long from_offset, long to_offset);
extern PG_PASCAL (pg_boolean) pgSetFldsFromMask (void PG_FAR *new_rec, void PG_FAR *fill, void PG_FAR *mask,
		pg_short_t rec_size);
extern PG_PASCAL (void) pgSetMaskFromFlds (void PG_FAR *rec1, void PG_FAR *rec2, void PG_FAR *mask,
		short rec_type, pg_boolean positive_match, pg_boolean exclude_tables);
extern PG_PASCAL (pg_boolean) pgStyleMatchesCriteria (void PG_FAR *target_style, void PG_FAR *match_style,
		void PG_FAR *mask, void PG_FAR *AND_mask, short rec_length);
extern PG_PASCAL (void) pgFinishStyleUpdate (paige_rec_ptr pg, change_info_ptr change_stuff,
		memory_ref ref_to_dispose, pg_boolean inval_text, short draw_mode);
extern PG_PASCAL (pg_boolean) pgIsPlatformFont (long platform);
extern PG_PASCAL (style_run_ptr) pgFindRunFromRef (style_ref ref, size_t offset, size_t PG_FAR *rec_num);
extern PG_PASCAL (void) pgFixAllStyleRuns (paige_rec_ptr pg, change_info_ptr change);
extern PG_PASCAL (void) pgStyleSuperImpose (paige_rec_ptr pg, style_info_ptr style,
		style_info_ptr new_style, font_info_ptr new_font, short use_stylesheet);
extern PG_PASCAL (void) pgDescrementRunItems (style_ref ref, pg_short_t index, pg_short_t max_styles);
extern PG_PASCAL (short) pgAddCrossFont (paige_rec_ptr source_pg, paige_rec_ptr target_pg,
		style_info_ptr style);
extern PG_PASCAL (void) pgScaleStyleInfo (paige_rec_ptr pg, style_info_ptr style, short numerator, short denominator);
extern PG_PASCAL (pg_boolean) pgEqualFontNames (font_info_ptr font1, font_info_ptr font2, pg_boolean use_alternate);
extern PG_PASCAL (long) pgAddSmallCapsStyle (paige_rec_ptr pg, paige_rec_ptr src_option,
		short reason_verb, style_info_ptr style);
extern PG_PASCAL (style_run_ptr) pgFindParExclusionRun (paige_rec_ptr pg, long offset,
		long PG_FAR *remaining_run);

#ifdef __cplusplus
	}
#endif


#endif
