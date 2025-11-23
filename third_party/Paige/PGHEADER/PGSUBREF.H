/* This header file defines some of the sub pg_ref functions and structs. */

#ifndef PGSUBREF_H
#define PGSUBREF_H

#include "Paige.h"
#include "pgEmbed.h"
#include "pgFiles.h"

/* Sub-structure "clone" record (used mainly in embed_refs): */

typedef struct {

	long				t_length;			/* Total length of text */
	block_ref			t_blocks;			/* Text block array */
	style_ref			t_style_run;		/* Run for styles */
	style_ref			par_style_run;		/* Run for paragraph styles */
	memory_ref			hyperlinks;			/* Hyperlink run */
	memory_ref			target_hyperlinks;	/* Target hyperlink run */
	long				mem_id;				/* Memory ID used for this subreference */
	pg_short_t			num_selects;		/* Number of selections */
	pg_short_t			insert_style;		/* Style to insert next */
	select_ref			select;				/* Selection points */
	long				flags;				/* Private flags that get swapped in paige_rec */
	pg_subref			active_subset;		/* Last subref selected */
	char_widths			width_table;		/* Subref's personal charwidth table */

// The following do not get saved in Push, Pop:
	
	pg_subref			myself;				/* The original subref (self) */
	rectangle			subset_bounds;		/* Bounding area of subset */
	long				empty_width;		/* Width when empty */
	long				minimum_width;		/* Smallest width allowed */
	long				generated_extra;	/* (Used in matrix or table cells only) */
	size_t				home_position;		/* Place the source character bounds is inserted */
	long				subref_flags;		/* Flags (similar to embed_ref flags) */
	short				descent;			/* Descent for the text line */
	short				ascent;				/* Ascent for the text line */
	short				left_extra;			/* Extra amount to left */
	short				right_extra;		/* Extra amount to right */
	short				top_extra;			/* Extra amount on top */
	short				bot_extra;			/* Extra amount on bottom */
	short				tighten_ascent;		/* Tighten up total ascent by this much */
	short				tighten_descent;	/* Tighten up total descent by this much */
	long				actual_text_width;	/* Original text width (before matrix sets in) */
	long				alignment_flags;	/* For left, right, etc. alignment to previous */
	long				modifier;			/* Data modifiers */
	long				refcon;				/* App-defined space */
	long				callback_refcon;	/* Callback-specific space */
	pg_subref			home_sub;			/* Subref where we live, if any */
	short				nested_shift_v;		/* Vertical shift from nested level(s) */
	short				reserved1;			// Used by DSI -- type of EQ
	long				reserved2;			// Ditto -- fence data
	double				reserved3;			// Ditto -- RESERVED EQ Value
	short				reserved4;			// Ditto -- max alt refs built
	short				reserved5;			// Ditto -- number of sig digits
	long				reserved6;			// mine
	long				alt_index;			/* Index to current alternate subref */
	
} paige_sub_rec, PG_FAR *paige_sub_ptr;

#define SUBREF_CHAR		0x07				// Subref character, embedded				

// Nested level variable point size factors (relative to 12 point):

#define NESTED_POINT_LEVEL1			12
#define NESTED_POINT_LEVEL2			10
#define NESTED_POINT_LEVEL3			9

// General flags:

#define VERTICAL_SHIFT_UP		0x00010000		// Vertically aligned but messed up by LOWORD amount
#define VERTICAL_SHIFT_DOWN		0x00020000		// Vertically aligned but messed down by LOWORD amount
#define NTH_ELEMENT_BASELINE	0x00040000		// The nth element (LOWORD) defines baseline
#define NTH_ELEMENT_CENTERED	0x00080000		// Same as above but composite subrefs will be centered vertically
#define SUBREF_IS_TABLE			0x00100000		// The subref is a table (version 2.12)
#define SUBREF_NOT_EDITABLE		0x00400000		// Subref type not editable
#define VERTICAL_CENTER_FLAG	0x00800000		// Center the embed_ref vertically to baseline
#define HORIZONTAL_CENTER_FLAG	0x01000000		// Align horizontally centered
#define HORIZONTAL_RIGHT_FLAG	0x02000000		// Align horizontally right
#define CLICK_REDRAWS_TEXT		0x08000000		// Click within ref redraws all
#define SUBREF_MATRIXALIGN		0x10000000		// Subrefs within align as matrix
#define SUBREF_DRAWN			0x20000000		// Subref has been drawn and not re-built
#define MATRIX_PHASE_FLAG		0x40000000		// Matrix out of phase (used on READ)

#define ANY_VERTICAL_FLAGS		(VERTICAL_SHIFT_UP | VERTICAL_SHIFT_UP | VERTICAL_CENTER_FLAG)
#define ANY_NTH_ALIGN			(NTH_ELEMENT_BASELINE | NTH_ELEMENT_CENTERED)
#define ELEMENT_MASK			0x00000FFF		// Element mask for nth-ref align.

/* *NTH_REF_IS_BASELINE contains a subref element in loword. */

// Alignment flags:

#define SUBREF_ALIGNTOP				0x00000001		// Align top with previous top
#define SUBREF_ALIGNTOPRIGHT		0x00000002		// Align bottom to previous top
#define SUBREF_ALIGNMIDDLE			0x00000004		// Align to center of previous
#define CELL_ALIGN_RIGHT			0x00000008		// Align right inside cell
#define CELL_ALIGN_DECIMAL			0x00000010		// Align decimal inside cell
#define SUBREF_SUBSCRIPT_ALIGN		0x00000020		// Align as subscript (relative to left object)
#define SUBREF_HASTOPALIGN			0x00010000		// Another ref is aligned to this top

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (pg_subref) pgNewSubRef (pgm_globals_ptr mem_globals);
extern PG_PASCAL (void) pgInsertSubRef (pg_ref pg, pg_subref subref, long position,
		pg_boolean set_focus, short stylesheet_option, pg_char_ptr initial_text,
		long initial_text_size, short draw_mode);
extern PG_PASCAL (void) pgDisposeSubRef (pg_subref subref);
extern PG_PASCAL (void) pgUseSubRef (paige_rec_ptr pg, pg_subref subref, long flags, rectangle_ptr pin_to,
		draw_points_ptr draw_position);
extern PG_PASCAL (pg_subref) pgUnuseSubRef (paige_rec_ptr pg);
extern PG_PASCAL (memory_ref) pgGetSubrefState (paige_rec_ptr pg, pg_boolean clear_hilites, pg_boolean clear_refs);
extern PG_PASCAL (void) pgRestoreSubRefs (paige_rec_ptr pg, memory_ref old_subrefs);
extern PG_PASCAL (pg_boolean) pgClickSelectSubRef (pg_ref pg, co_ordinate_ptr mouse_point);
extern PG_PASCAL (pg_boolean) pgInsertIntoSubRef (paige_rec_ptr pg, pg_char_ptr data, size_t length,
		size_t position, short insert_mode, short modifiers, short draw_mode);
extern PG_PASCAL (void) pgUpdateSubRefBounds (paige_rec_ptr pg, text_block_ptr block);
extern PG_PASCAL (void) pgDecrementSubsetStyles (paige_rec_ptr pg, pg_short_t bad_index,
		pg_short_t max_styles, pg_boolean do_par);
extern PG_PASCAL (void) pgSetSubRefText (pg_ref pg, pg_subref ref, long index,
		pg_char_ptr the_text, short text_size, short stylesheet_option, short draw_mode);
extern PG_PASCAL (memory_ref) pgGetSubRefText (pg_ref pg, pg_subref ref, long index);
extern PG_PASCAL (void) pgInvalSubBlocks (paige_rec_ptr pg, paige_sub_ptr sub_ptr);
extern PG_PASCAL (void) pgCopySubfields (paige_rec_ptr pg, paige_sub_ptr subrec_ptr,
		pg_boolean move_to_subref);
extern PG_PASCAL (void) pgAllocateAlternate (paige_sub_ptr sub_ptr, long index);
extern PG_PASCAL (pg_subref) pgDuplicateRef (paige_rec_ptr source_pg, paige_rec_ptr target_pg,
			pg_subref subref, pg_subref home_ref);
extern PG_PASCAL (long) pgMatrixWidth (paige_rec_ptr pg, paige_sub_ptr home_sub_ptr, text_block_ptr block);
extern PG_PASCAL (void) pgInvalMatrix (paige_rec_ptr pg, text_block_ptr block,
			pg_boolean zero_widths);
extern PG_PASCAL (memory_ref) pgSetNestedFocus (paige_rec_ptr pg, pg_subref ref, pg_boolean redraw);
extern PG_PASCAL (void) pgClearSubrefCarets (paige_rec_ptr pg, pg_subref clear_this_also);
extern PG_PASCAL (long) pgFindSubrefPosition (text_block_ptr block, pg_short_t local_position);
extern PG_PASCAL (long) pgSubrefHomePosition (paige_rec_ptr pg, pg_subref subref);
extern PG_PASCAL (void) pgDrawSubRef (paige_rec_ptr pg, style_walk_ptr walker,
		draw_points_ptr draw_position, pg_short_t local_offset, co_ordinate_ptr vis_offset);
extern PG_PASCAL (void) pgSubsetGlitter (paige_rec_ptr pg, draw_points_ptr draw_position,
		rectangle_ptr draw_bounds, co_ordinate_ptr offset_extra, style_walk_ptr walker);
extern PG_PASCAL (pg_subref) pgGetNthSubref (paige_rec_ptr pg, long index);
extern PG_PASCAL (pg_subref) pgGetSubrefInSelect (paige_rec_ptr pg);
extern PG_PASCAL (void) pgDeleteSubRefs (text_block_ptr block, long local_offset, long length);
extern PG_PASCAL (void) pgDisposeAllSubRefs (block_ref blocks);
extern PG_PASCAL (void) pgDuplicateAllSubRefs (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
			block_ref blocks, pg_subref home_ref);
extern PG_PASCAL (void) pgCopySubRefs (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		select_pair_ptr source_range, long target_begin);
extern PG_PASCAL (pg_subref) pgGetSourceSubRef (paige_rec_ptr pg, long position);
extern PG_PASCAL (void) pgPackSubRefs (pack_walk_ptr walker, text_block_ptr block);
extern PG_PASCAL (long) pgUnpackSubRefs (paige_rec_ptr pg, pack_walk_ptr walker, text_block_ptr block,
			 pg_subref home_ref);
extern PG_PASCAL (void) pgApplyStylesToSubrefs (paige_rec_ptr pg, select_pair_ptr range,
			style_info_ptr style, style_info_ptr mask);
extern PG_PASCAL (long) pgRedrawSubsetLine (paige_rec_ptr pg_rec, pg_subref subref, long previous_redraw);
extern PG_PASCAL (void) pgInsertSubrefList (paige_rec_ptr pg_rec, long position, memory_ref list_ref);

extern PG_PASCAL (memory_ref) pgGetThisSubset (pg_measure_ptr line_info, long current_offset);
extern PG_PASCAL (long) pgFindNextSubset (short PG_FAR *char_types, long remaining);
extern PG_PASCAL (long PG_FAR*) pgGetSubrefCharLocs (paige_rec_ptr pg, text_block_ptr block,
		memory_ref PG_FAR *used_ref, memory_ref PG_FAR *types_ref);
extern PG_PASCAL (pg_boolean) pgActiveMatrix (paige_rec_ptr pg);
extern PG_PASCAL (pg_subref) pgPtInSubRef (paige_rec_ptr pg, co_ordinate_ptr mouse_point);
extern PG_PASCAL (pg_boolean) pgIsValidSubref (text_block_ptr block, long index);


#ifdef __cplusplus
	}
#endif

#endif

