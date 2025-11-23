/* This file contains bi-directional (and eventually vertical) utilities.
Nothing in this source file should be machine-specific.
Added July 25 96, multilingual language support. */

#include "Paige.h"
#include "defprocs.h"
#include "machine.h"
#include "pgUtils.h"
#include "pgText.h"
#include "pgDefStl.h"
#include "pgSelect.h"
#include "pgShapes.h"
#include "pgScript.h"
#include "pgSubRef.h"
#include "pgTraps.h"
#include "pgTables.h"

static void sides_of_run (point_start_ptr first_start, pg_short_t num_starts,
		long PG_FAR *left_side, long PG_FAR *right_side);
static pg_boolean fix_rect (rectangle_ptr rect, rectangle_ptr max_rect);
static pg_boolean find_char_in_table (pg_short_t PG_FAR *table, pg_short_t the_char);

#ifdef UNICODE

static pg_short_t kanji_nonbreak_after[21] = {
	0x24, 0x28, 0x5B, 0x5C, 0x7B, 0xFF62, 0x2018, 0x201C,
	0x3008, 0x300A, 0x300C, 0x300E, 0x3010, 0x3014,
	0xFF04, 0xFF08, 0xFF3B, 0xFF5B, 0xFFE1, 0xFFE5,
	0
};

static pg_short_t kanji_nonbreak_before[81] = {
	0x21, 0x25, 0x29, 0x2C, 0x2E, 0x3F, 0x5D, 0x7D,
	0xFF61, 0xFF62, 0xFF63, 0xFF64, 0xFF65, 0xFF67, 0xFF69, 0xFF6A,
	0xFF6B, 0xFF6C, 0xFF6D, 0xFF6E, 0xFF6F, 0xFF70, 0xFF9E, 0xFF9F,
	0x3001, 0x3002, 0xFF0C, 0xFF0E, 0x30FB, 0xFF1A,
	0xFF1B, 0xFF1F, 0xFF01, 0x309B, 0x309C, 0x30FD,
	0x30FE, 0x309D, 0x309E, 0x3005, 0x30FC, 0x2019,
	0x201D, 0xFF09, 0x3015, 0xFF3D, 0xFF5D, 0x3009,
	0x300B, 0x300D, 0x300F, 0x3011, 0x2032,
	0x2033, 0x2103, 0xFFE0, 0xFF05, 0x2030, 0x3041,
	0x3042, 0x3045, 0x3047, 0x3049, 0x3063, 0x3083,
	0x3085, 0x3087, 0x308E, 0x30A1, 0x30A3, 0x30A5,
	0x30A7, 0x30A9, 0x30C3, 0x30E3, 0x30E5, 0x30E7,
	0x30EE, 0x30F5, 0x30F6,
	0
};

static pg_short_t tchinese_nonbreak_after[27] = {
	0x28, 0x5B, 0x7B, 0xFF08, 0xFE35, 0xFF5B, 0xFE37,
	0xFF3B, 0xFF39, 0x3010, 0xFE3B, 0x300A, 0xFE3D,
	0x3008, 0xFF3E, 0x300C, 0xFE41, 0x300E, 0xFE43,
	0xFE59, 0xFE5B, 0xFE5D, 0xFF40, 0x201C, 0x301D,
	0x2035, 0
};


static pg_short_t tchinese_nonbreak_before[60] = {
	0x21, 0x29, 0x2C, 0x2E, 0x3A, 0x3B, 0x3F, 0x5D, 0x7D,
	0xFF0C, 0x3001, 0x3002, 0xFF0E, 0x2022, 0xFF1B, 0xFF1A,
	0xFF1F, 0xFF01, 0xFE30, 0x2026, 0x2025, 0xFE50, 0xFE51,
	0xFE52, 0x00B7, 0xFE54, 0xFE55, 0xFE56, 0xFE57, 0xFF5C,
	0x2013, 0xFE31, 0x2014, 0xFE33, 0xFE34, 0xFE4F,
	0xFF09, 0xFE36, 0xFF5D, 0xFE38, 0xFF3D, 0xFE3A, 0x3011,
	0xFE3C, 0x300B, 0xFE3E, 0x3009, 0xFE40, 0x300D, 0xFE42,
	0x300F, 0xFE44, 0xFE5A, 0xFE5C, 0xFE5E, 0xFF07, 0xFF02,
	0x301E, 0x2032, 0
};


static pg_short_t schinese_nonbreak_after[27] = {
	0x28, 0x5B, 0x7B, 0x2018, 0x201C, 0x3014, 0x3008,
	0x300A, 0x300C, 0x300E, 0x3016, 0x3010, 0xFF08,
	0xFF0E, 0xFF10, 0xFF11, 0xFF12, 0xFF13, 0xFF14,
	0xFF15, 0xFF16, 0xFF17, 0xFF18, 0xFF19, 0xFF3B,
	0xFF5B, 0
};


static pg_short_t schinese_nonbreak_before[45] = {
	0x21, 0x29, 0x2C, 0x2E, 0x3A, 0x3B, 0x3F, 0x5D, 0x7D,
	0x3001, 0x3002, 0x2022, 0x02C9, 0x02C7, 0x00A8, 0x3003,
	0x3005, 0x2015, 0xFF5E, 0x2016, 0x2026, 0x2019, 0x201D,
	0x3015, 0x3009, 0x300B, 0x300D, 0x300F, 0x3017, 0x3011,
	0x2236, 0xFF01, 0xFF02, 0xFF07, 0xFF09, 0xFF0C, 0xFF0E,
	0xFF1A, 0xFF1B, 0xFF1F, 0xFF3D, 0xFF40, 0xFF5C, 0xFF5D,
	0
};


static pg_short_t korean_nonbreak_after[18] = {
	0x28, 0x5B, 0x5C, 0x7B, 0x2018, 0x201C, 0x3014,
	0x3008, 0x300A, 0x300C, 0x300E, 0x3010, 0xFF04,
	0xFF08, 0xFF3B, 0xFFE6, 0xFF5B, 0
};


static pg_short_t korean_nonbreak_before[36] = {
	0x21, 0x25, 0x29, 0x2C, 0x2E, 0x3A, 0x3B, 0x3F,
	0x5D, 0x7D, 0x3001, 0x2019, 0x201D, 0x3015, 0x3009,
	0x300B, 0x300D, 0x300F, 0x3011, 0x00B0, 0x2032, 0x2033,
	0x2103, 0xFFE0, 0xFF01, 0xFF05, 0xFF09, 0xFF0C, 0xFF0E,
	0xFF1A, 0xFF1B, 0xFF1F, 0xFFE6, 0xFF3D, 0xFF5D, 0
};

#else

static pg_short_t kanji_nonbreak_after[21] = {
	0x24, 0x28, 0x5B, 0x5C, 0x7B, 0xA2, 0x8165, 0x8167,
	0x8169, 0x816B, 0x816D, 0x816F, 0x8171, 0x8173,
	0x8175, 0x8177, 0x8179, 0x818F, 0x8190, 0x8192,
	0
};

#ifdef MAC_PLATFORM

static PG_FAR pg_short_t kanji_nonbreak_before[80] = {
	0x21, 0x25, 0x29, 0x2C, 0x2E, 0x3F, 0x5D, 0x7D,
	0xC1, 0xA2, 0xA3, 0xDB, 0xB4, 0xA4, 0xA9, 0xBB,
	0xC7, 0xC2, 0xA8, 0xE1, 0xA1, 0xA7,
	0x8141, 0x8142, 0x8143, 0x8144, 0x8145, 0x8146,
	0x8147, 0x8148, 0x8149, 0x814A, 0x814B, 0x8152,
	0x8153, 0x8154, 0x8155, 0x8158, 0x815B, 0x8166,
	0x8168, 0x816A, 0x816C, 0x816E, 0x8170, 0x8172,
	0x8174, 0x8176, 0x8178, 0x817A, 0x818B, 0x818C,
	0x818D, 0x818E, 0x8191, 0x8193, 0x81F1, 0x829F,
	0x82A1, 0x82A3, 0x82A5, 0x82A7, 0x82C1, 0x82E1,
	0x82E3, 0x82E5, 0x82EC, 0x8340, 0x8342, 0x8344,
	0x8346, 0x8348, 0x8362, 0x8383, 0x8385, 0x8387,
	0x838E, 0x8395, 0x8396,
	0
};

#else

static pg_short_t kanji_nonbreak_before[82] = {
	0x21, 0x25, 0x29, 0x2C, 0x2E, 0x3F, 0x5D, 0x7D,
	0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA7, 0xA9, 0xAA,
	0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xDE, 0xDF,
	0x8141, 0x8142, 0x8143, 0x8144, 0x8145, 0x8146,
	0x8147, 0x8148, 0x8149, 0x814A, 0x814B, 0x8152,
	0x8153, 0x8154, 0x8155, 0x8158, 0x815B, 0x8166,
	0x8168, 0x816A, 0x816C, 0x816E, 0x8170, 0x8172,
	0x8174, 0x8176, 0x8178, 0x817A, 0x818B, 0x818C,
	0x818D, 0x818E, 0x8191, 0x8193, 0x81F1, 0x829F,
	0x82A1, 0x82A3, 0x82A5, 0x82A7, 0x82C1, 0x82E1,
	0x82E3, 0x82E5, 0x82EC, 0x8340, 0x8342, 0x8344,
	0x8346, 0x8348, 0x8362, 0x8383, 0x8385, 0x8387,
	0x838E, 0x8395, 0x8396,
	0
};

#endif
// endif MAC_PLATFORM vs. WINDOWS

static pg_short_t tchinese_nonbreak_after[27] = {
	0x28, 0x5B, 0x7B, 0xA15D, 0xA15F, 0xA161, 0xA163,
	0xA165, 0xA167, 0xA169, 0xA16B, 0xA16D, 0xA16F,
	0xA171, 0xA173, 0xA175, 0xA177, 0xA179, 0xA17B,
	0xA17D, 0xA1A1, 0xA1A3, 0xA1A5, 0xA1A7, 0xA1A9,
	0xA1AB, 0
};


static pg_short_t tchinese_nonbreak_before[61] = {
	0x21, 0x29, 0x2C, 0x2E, 0x3A, 0x3B, 0x3F, 0x5D, 0x7D,
	0xA141, 0xA142, 0xA143, 0xA144, 0xA145, 0xA146, 0xA147,
	0xA148, 0xA149, 0xA14A, 0xA14B, 0xA14C, 0xA14D, 0xA14E,
	0xA14F, 0xA150, 0xA151, 0xA152, 0xA153, 0xA154, 0xA155,
	0xA156, 0xA157, 0xA158, 0xA159, 0xA15A, 0xA15B, 0xA15C,
	0xA15E, 0xA160, 0xA162, 0xA164, 0xA166, 0xA168, 0xA16A,
	0xA16C, 0xA16E, 0xA170, 0xA172, 0xA174, 0xA176, 0xA178,
	0xA17A, 0xA17C, 0xA17E, 0xA1A2, 0xA1A4, 0xA1A6, 0xA1A8,
	0xA1AA, 0xA1AC, 0
};


static pg_short_t schinese_nonbreak_after[27] = {
	0x28, 0x5B, 0x7B, 0xA1AE, 0xA1B0, 0xA1B2, 0xA1B4,
	0xA1B6, 0xA1B8, 0xA1BA, 0xA1BC, 0xA1BE, 0xA3A8,
	0xA3AE, 0xA3B0, 0xA3B1, 0xA3B2, 0xA3B3, 0xA3B4,
	0xA3B5, 0xA3B6, 0xA3B7, 0xA3B8, 0xA3B9, 0xA3DB,
	0xA3FB, 0
};


static pg_short_t schinese_nonbreak_before[45] = {
	0x21, 0x29, 0x2C, 0x2E, 0x3A, 0x3B, 0x3F, 0x5D, 0x7D,
	0xA1A2, 0xA1A3, 0xA1A4, 0xA1A5, 0xA1A6, 0xA1A7, 0xA1A8,
	0xA1A9, 0xA1AA, 0xA1AB, 0xA1AC, 0xA1AD, 0xA1AF, 0xA1B1,
	0xA1B3, 0xA1B5, 0xA1B7, 0xA1B9, 0xA1BB, 0xA1BD, 0xA1BF,
	0xA1C3, 0xA3A1, 0xA3A2, 0xA3A7, 0xA3A9, 0xA3AC, 0xA3AE,
	0xA3BA, 0xA3BB, 0xA3BF, 0xA3DD, 0xA3E0, 0xA3FC, 0xA3FD,
	0
};


static pg_short_t korean_nonbreak_after[18] = {
	0x28, 0x5B, 0x5C, 0x7B, 0xA1AE, 0xA1B0, 0xA1B2,
	0xA1B4, 0xA1B6, 0xA1B8, 0xA1BA, 0xA1BC, 0xA3A4,
	0xA3A8, 0xA3DB, 0xA3DC, 0xA3FB, 0
};


static pg_short_t korean_nonbreak_before[36] = {
	0x21, 0x25, 0x29, 0x2C, 0x2E, 0x3A, 0x3B, 0x3F,
	0x5D, 0x7D, 0xA1A2, 0xA1AF, 0xA1B1, 0xA1B3, 0xA1B5,
	0xA1B7, 0xA1B9, 0xA1BB, 0xA1BD, 0xA1C6, 0xA1C7, 0xA1C8,
	0xA1C9, 0xA1CB, 0xA3A1, 0xA3A5, 0xA3A9, 0xA3AC, 0xA3AE,
	0xA3BA, 0xA3BB, 0xA3BF, 0xA3DC, 0xA3DD, 0xA3FD, 0
};

#endif
// endif UNICODE

/* pgBreakInfoProc is the standard char info for word breaks. */

PG_PASCAL (long) pgBreakInfoProc (paige_rec_ptr pg, pg_char_ptr the_char, short charsize,
		style_info_ptr style, font_info_ptr font, long current_settings)
{
	pg_short_t				test_char;
	long					flags = current_settings;

	test_char = (pg_short_t)*the_char;

#ifndef UNICODE
	if (charsize == 2) {
		
		test_char <<= 8;
		test_char |= the_char[1];
	}
#endif

	switch (font->char_type & SCRIPT_CODE_MASK) {

		case SHIFTJIS_CHARSET:
			if (find_char_in_table(kanji_nonbreak_before, test_char))
				flags |= NON_BREAKBEFORE_BIT;
			else
			if (find_char_in_table(kanji_nonbreak_after, test_char))
				flags |= NON_BREAKAFTER_BIT;
			
			break;

		case HANGEUL_CHARSET:
			if (find_char_in_table(korean_nonbreak_before, test_char))
				flags |= NON_BREAKBEFORE_BIT;
			else
			if (find_char_in_table(korean_nonbreak_after, test_char))
				flags |= NON_BREAKAFTER_BIT;
			
			break;

		case GB2312_CHARSET:
			if (find_char_in_table(schinese_nonbreak_before, test_char))
				flags |= NON_BREAKBEFORE_BIT;
			else
			if (find_char_in_table(schinese_nonbreak_after, test_char))
				flags |= NON_BREAKAFTER_BIT;
			
			break;

		case CHINESEBIG5_CHARSET:
			if (find_char_in_table(tchinese_nonbreak_before, test_char))
				flags |= NON_BREAKBEFORE_BIT;
			else
			if (find_char_in_table(tchinese_nonbreak_after, test_char))
				flags |= NON_BREAKAFTER_BIT;
			
			break;
	}

	return		flags;
}


/* Given a point_start record (which is typically the beginning of a text line)
pgDirectionRun returns the number of starts, up to and including the last start
for the line, that contain the same text direction. This function is used for
possible mixed, bi-directional text such as English and Hebrew. If back_direction
is TRUE the scan is done backwards (in which case the function result is how
many point_starts backwards the run is in).  Note, if num_starts is zero then
only the start.flags are checked to determine boundaries. */

PG_PASCAL (pg_short_t) pgDirectionRun (point_start_ptr first_start, long num_starts,
		pg_boolean back_direction)
{
	register point_start_ptr	starts;
	register pg_short_t			result, direction_flag;	
	register long				ctr;

	starts = first_start;
	direction_flag = starts->flags & RIGHT_DIRECTION_BIT;
	result = 0;
	if (!(ctr = num_starts))
		ctr = GOD_AWFUL_HUGE;

	if (back_direction) {

		while ( (starts->flags & RIGHT_DIRECTION_BIT) == direction_flag ) {
			
			++result;
			--ctr;

			if ( (starts->flags & NEW_LINE_BIT) || (!ctr) )
				break;
			
			--starts;
		}
	}
	else {

		while ( (starts->flags & RIGHT_DIRECTION_BIT) == direction_flag ) {
			
			++result;
			--ctr;

			if ( (starts->flags & LINE_BREAK_BIT) || (!ctr) )
				break;
			
			++starts;
		}
	}

	return	result;
}

/* pgHasRightLeftRun returns TRUE if any right-to-left run(s) exist at point start
line beginning at first_start for num_starts (or, if num_starts is
zero to end of line). */

PG_PASCAL (pg_boolean) pgHasRightLeftRun (point_start_ptr first_start,
		long num_starts)
{
	register point_start_ptr		starts;
	register long					start_ctr;
	
	if (!(start_ctr = num_starts))
		start_ctr = GOD_AWFUL_HUGE;
	
	for (starts = first_start; start_ctr; ++starts, --start_ctr) {
		
		if (starts->flags & RIGHT_DIRECTION_BIT)
			return	TRUE;
		
		if (starts->flags & LINE_BREAK_BIT)
			break;
	}
	
	return	FALSE;
}



/* pgInsertHiliteRect gets called by the highlight-region builder. Its purpose
is to begin with a single highlight rectangle area for a single line, then
build multiple rectangles from that, if necessary, due to mixed directions.
The parameters are as follows: block is the text block in question; rgn is the
shape to add the rectangle(s); first_select is the starting selection or NULL
(indicating first select is start of line) and last_select is the ending
selection or NULL (indicating end of line); top is the rectangle's top vertical
position.  */

PG_PASCAL (void) pgInsertHiliteRect (paige_rec_ptr pg, text_block_ptr block,
		shape_ref rgn, t_select_ptr first_select, t_select_ptr last_select,
		long top, rectangle_ptr page_wrap_rect, long actual_begin_select,
		long actual_end_select)
{
	register point_start_ptr		starts, secondary_starts;
	rectangle_ptr					wrap_rect;
	t_select						begin_select, end_select;
	par_info_ptr					par_of_select;
	pg_short_t						run_qty, starts_qty;
	rectangle						rect, sub_rect;
	long							left_side, right_side;
	long							begin_line_offset, end_line_offset;
	long							extreme_left, extreme_right;
	long							doing_table;
	short							prime_dir, justification;
	
	if (first_select)
		doing_table = first_select->flags & SELECT_IN_TABLE;
	else
		doing_table = last_select->flags & SELECT_IN_TABLE;

	if (doing_table || pg->active_subset) {
		paige_sub_ptr		sub_ptr;
		
		if (doing_table) {
			
			if (first_select)
				begin_line_offset = first_select->offset;
			else
				begin_line_offset = last_select->offset;

			pgGetTableBounds(pg, begin_line_offset, &sub_rect);
		}
		else {
		
			sub_ptr = (paige_sub_ptr) UseMemory(pg->active_subset);
			sub_ptr += sub_ptr->alt_index;
			sub_rect = sub_ptr->subset_bounds;
			UnuseMemory(pg->active_subset);
		}

		wrap_rect = &sub_rect;
	}
	else
		wrap_rect = page_wrap_rect;

	rect.top_left.v = top;
	starts = secondary_starts = (point_start_ptr) UseMemory(block->lines);

	if (first_select) {
	
		begin_select = *first_select;
		starts += begin_select.line;
	}
	else {
		
		begin_select = *last_select;
		starts += begin_select.line;
		
		while (!(starts->flags & NEW_LINE_BIT)) {
			
			--begin_select.line;
			--starts;
		}
		
		begin_select.offset = starts->offset;
		begin_select.offset += block->begin;
		
		run_qty = pgDirectionRun(starts, 0, FALSE);
		sides_of_run(starts, run_qty, &left_side, &right_side);
		
		if (starts->flags & RIGHT_DIRECTION_BIT)
			begin_select.primary_caret = right_side - starts->bounds.top_left.h;
		else
			begin_select.primary_caret = left_side - starts->bounds.top_left.h;
	}
	
	if (last_select) {
	
		end_select = *last_select;
		secondary_starts += end_select.line;
	}
	else {

		end_select = *first_select;
		secondary_starts += end_select.line;
		
		while (!(secondary_starts->flags & LINE_BREAK_BIT)) {
			
			++end_select.line;
			++secondary_starts;
		}
		
		end_select.offset = secondary_starts[1].offset;
		end_select.offset += block->begin;

		run_qty = pgDirectionRun(secondary_starts, 0, TRUE);
		secondary_starts -= (run_qty - 1);
		sides_of_run(secondary_starts, run_qty, &left_side, &right_side);
		secondary_starts += (run_qty - 1);
		
		if (secondary_starts->flags & RIGHT_DIRECTION_BIT)
			end_select.primary_caret = left_side - secondary_starts->bounds.top_left.h;
		else
			end_select.primary_caret = right_side - secondary_starts->bounds.top_left.h;
	}

	// PAJ, we're not really selecting anything, lets not look stupid!
	if (end_select.offset == begin_select.offset)
	{
		UnuseMemory(block->lines);
		return;
	}

	if ((block->flags & BLOCK_HAS_TABLE) || doing_table) {
		rectangle_ptr		max_rect_ptr = NULL;
		rectangle			whole_line_bounds;
		
		run_qty = end_select.line - begin_select.line + 1;
		pgFillBlock(&whole_line_bounds, sizeof(rectangle), 0);
		
		if (doing_table)
			max_rect_ptr = wrap_rect;

		for (starts_qty = run_qty; starts_qty; ++starts, --starts_qty) {
		
			pgUnionRect(&starts->bounds, &whole_line_bounds, &whole_line_bounds);
			
			rect = starts->bounds;
			
			if ((starts_qty == run_qty) && first_select)
				rect.top_left.h += begin_select.primary_caret;
			
			if ((starts_qty == 1) && last_select) {
				
				if (doing_table && end_select.offset >= (pg->t_length - 1))
					end_select.primary_caret = starts->bounds.bot_right.h - starts->bounds.top_left.h;

				rect.bot_right.h = starts->bounds.top_left.h + end_select.primary_caret;
			}
			
			if (fix_rect(&rect, max_rect_ptr))
				pgAddRectToShape(rgn, &rect);
		}
		
		if (!last_select) {
			
			whole_line_bounds.bot_right.h = wrap_rect->bot_right.h;
			whole_line_bounds.top_left.h = secondary_starts->bounds.bot_right.h;
			
			if (fix_rect(&whole_line_bounds, max_rect_ptr))
				pgAddRectToShape(rgn, &whole_line_bounds);
		}
	}
	else {
	
		par_of_select = pgFindParStyle(pg, begin_select.offset);
		prime_dir = pgPrimaryTextDirection(pg->globals, par_of_select);
		justification = par_of_select->justification;
		
		UnuseMemory(pg->par_formats);
	
		rect.bot_right.v = starts->bounds.bot_right.v;
	
		if (!last_select) {
			
			run_qty = pgDirectionRun(secondary_starts, 0, TRUE);
			secondary_starts -= (run_qty - 1);
			sides_of_run(secondary_starts, run_qty, &left_side, &right_side);
			secondary_starts += (run_qty - 1);
			
			extreme_left = wrap_rect->top_left.h;
			extreme_right = wrap_rect->bot_right.h;

			if (secondary_starts->flags & NO_LINEFEED_BIT) {
				point_start_ptr		next_block_starts;
				long				next_start_end;
				
				pgPaginateBlock(pg, &block[1], NULL, FALSE);
				next_block_starts = (point_start_ptr) UseMemory(block[1].lines);
				
				while (!(next_block_starts->flags & LINE_BREAK_BIT))
					++next_block_starts;

				next_start_end = (long)next_block_starts[1].offset;
				UnuseMemory(block[1].lines);
				
				next_start_end += block[1].begin;
				
				if (actual_end_select < next_start_end) {
				
					extreme_left = left_side;
					extreme_right = right_side;
				}
			}
			
			if (prime_dir == right_left_direction) {
				
				rect.top_left.h = extreme_left;
				rect.bot_right.h = left_side;
			}
			else {
				rect.top_left.h = right_side;
				rect.bot_right.h = extreme_right;
			}

			if (fix_rect(&rect, NULL))
				pgAddRectToShape(rgn, &rect);
		}
		
		starts_qty = end_select.line - begin_select.line + 1;
		run_qty = pgDirectionRun(starts, 0, FALSE);
			
		begin_line_offset = starts->offset;
		end_line_offset = secondary_starts[1].offset;
		begin_line_offset += block->begin;
		end_line_offset += block->begin;
	
		if (pg->doc_info.attributes & PG_LIST_MODE_BIT) {
			
			if ((starts->flags & NEW_LINE_BIT)
				&& (begin_select.offset != end_select.offset)) {
				rectangle		vis_rect;
				
				pgShapeBounds(pg->vis_area, &vis_rect);

				if (prime_dir == right_left_direction) {
				
					rect.top_left.h = right_side;
					rect.bot_right.h = vis_rect.bot_right.h;
				}
				else {
					rect.top_left.h = vis_rect.top_left.h;
					rect.bot_right.h = left_side;
				}
			
				if (fix_rect(&rect, NULL))
					pgAddRectToShape(rgn, &rect);
			}
		}
		else
		if (!first_select) {
			
			sides_of_run(starts, 0, &left_side, &right_side);

			if (prime_dir == right_left_direction) {
				
				rect.top_left.h = right_side;
				rect.bot_right.h = wrap_rect->bot_right.h;
			}
			else {
				rect.top_left.h = wrap_rect->top_left.h;
				rect.bot_right.h = left_side;
			}
			
			if (fix_rect(&rect, NULL))
				pgAddRectToShape(rgn, &rect);
		}
			
		if ( (begin_select.offset == begin_line_offset)
			&& (end_select.offset == end_line_offset)
			&& (starts->flags & NEW_LINE_BIT)
			&& (secondary_starts->flags & LINE_BREAK_BIT) ) { /* whole line selected */
			
			sides_of_run(starts, 0, &rect.top_left.h, &rect.bot_right.h);
			rect.top_left.h = wrap_rect->top_left.h;		// PAJ

			if (fix_rect(&rect, NULL))
				pgAddRectToShape(rgn, &rect);
		}
		else {
		
			rect.top_left.h = starts->bounds.top_left.h + begin_select.primary_caret;
		
			if ( (starts_qty == 1) || (starts_qty <= run_qty)) {
				
				rect.bot_right.h = secondary_starts->bounds.top_left.h + end_select.primary_caret;
				
				if (fix_rect(&rect, NULL))
					pgAddRectToShape(rgn, &rect);
			}
			else {
		
				if (starts->flags & RIGHT_DIRECTION_BIT) {
					
					run_qty = pgDirectionRun(starts, 0, TRUE);
					starts -= (run_qty - 1);
					sides_of_run(starts, run_qty, &rect.bot_right.h, NULL);
					starts += (run_qty - 1);
				}
				else {
					
					run_qty = pgDirectionRun(starts, 0, FALSE);
					sides_of_run(starts, run_qty, NULL, &rect.bot_right.h);
				}
				
				if (fix_rect(&rect, NULL))
					pgAddRectToShape(rgn, &rect);
				
				++starts;
				--starts_qty;
		
				run_qty = pgDirectionRun(starts, 0, FALSE);
				
				if (run_qty < starts_qty) {   /* There is a middle direction */
					
					sides_of_run(starts, run_qty, &rect.top_left.h, &rect.bot_right.h);
					
					if (fix_rect(&rect, NULL))
						pgAddRectToShape(rgn, &rect);
				}
				
				rect.bot_right.h = secondary_starts->bounds.top_left.h + end_select.primary_caret;
		
				if (secondary_starts->flags & RIGHT_DIRECTION_BIT) {
					
					run_qty = pgDirectionRun(secondary_starts, 0, FALSE);
					sides_of_run(secondary_starts, run_qty, NULL, &rect.top_left.h);
				}
				else {
		
					run_qty = pgDirectionRun(secondary_starts, 0, TRUE);
					secondary_starts -= (run_qty - 1);
					sides_of_run(secondary_starts, run_qty, &rect.top_left.h, NULL);
				}
				
				if (fix_rect(&rect, NULL))
					pgAddRectToShape(rgn, &rect);
			}
		}
	}

	UnuseMemory(block->lines);
}


/* pgAdjustBiDirections adjusts any right-to-left style runs by moving the
bounding rects for each start if necessary. The first point_start is first_start
and number of starts is num_starts. If necessary the current paragraph format
can be examined in par_format. */

PG_PASCAL (void) pgAdjustBiDirections (point_start_ptr first_start,
		pg_short_t num_starts, par_info_ptr par_format)
{
	register point_start_ptr	starts;
	pg_short_t					run_qty, run_ctr, total_num_starts;

#ifdef MAC_PLATFORM
#pragma unused (par_format)
#endif

	if (pgHasRightLeftRun(first_start, num_starts)) {
	
		total_num_starts = num_starts;
		starts = first_start;
		
		while (total_num_starts) {
			
			run_qty = pgDirectionRun(starts, total_num_starts, FALSE);
	
			if ( (starts->flags & RIGHT_DIRECTION_BIT) && (run_qty > 1) ) {
				long			left_edge, bounds_width;
				
				left_edge = starts->bounds.top_left.h;
				run_ctr = run_qty;
				
				while (run_ctr) {
					
					--run_ctr;
					
					bounds_width = starts[run_ctr].bounds.bot_right.h - starts[run_ctr].bounds.top_left.h;
					starts[run_ctr].bounds.top_left.h = left_edge;
					left_edge = starts[run_ctr].bounds.bot_right.h = starts[run_ctr].bounds.top_left.h + bounds_width;
				}
			}
	
			total_num_starts -= run_qty;
			starts += run_qty;
		}
	}
}

/* pgAdjustRightLeftStarts is called when the primary text direction is
opposite to the way the point_start records have been formed. Usually this
means the primary direction is right to left and the line has just been
paginated, but it can also get called if system has reverted to left-right
and the line is right to left. The wanted_direction param indicates which
way it is to be reversed. The first_start param is always the first point_start
record for the line. */

PG_PASCAL (void) pgAdjustRightLeftStarts (paige_rec_ptr pg, rectangle_ptr wrap_bounds,
	short wanted_direction, point_start_ptr first_start, pg_short_t num_starts)
{
	register point_start_ptr	starts;
	register pg_short_t			total_num_starts, run_qty, run_index;
	long						h_offset;

#ifdef MAC_PLATFORM
#pragma unused (pg)
#endif

	starts = first_start;
	total_num_starts = num_starts;
	
	while (total_num_starts) {
		
		run_qty = pgDirectionRun(starts, 0, FALSE);
		h_offset = pgRightLeftOffset(wrap_bounds, starts, wanted_direction); 
		
		for (run_index = run_qty; run_index; ++starts, --run_index)
			pgOffsetRect(&starts->bounds, h_offset, 0);

		total_num_starts -= run_qty;
	}
}

/* pgCheckDirectionChange tests the current System text direction and, if it
does not match the last known direction in pg, all text is invalidated to
re-adjust, the text_direction field is updated in pg and the function returns TRUE. */

PG_PASCAL (pg_boolean) pgCheckDirectionChange(paige_rec_ptr pg)
{
	register text_block_ptr		block;
	register t_select_ptr		selections;
	pg_short_t					block_qty;
	long						current_direction, pg_direction;
	
	current_direction = pgSystemDirection(pg->globals);
	pg_direction = pg->text_direction;
	
	if (pg_direction != right_left_direction)
		pg_direction = left_right_direction;

	if (pg_direction == current_direction)
		return	FALSE;
	
	pg->text_direction = current_direction;
	
	for (block_qty = GetMemorySize(pg->t_blocks), block = (text_block_ptr)UseMemory(pg->t_blocks);
			block_qty; ++block, --block_qty)
		block->flags |= SWITCHED_DIRECTIONS;
	
	UnuseMemory(pg->t_blocks);
	
	for (block_qty = GetMemorySize(pg->select), selections = (t_select_ptr)UseMemory(pg->select);
			block_qty; ++selections, --block_qty)
		selections->flags |= SELECTION_DIRTY;
	
	pg->stable_caret.h = pg->stable_caret.v = 0;

	UnuseMemory(pg->select);

	return	TRUE;
}


/* pgPrimaryTextDirection returns the writing direction of text based on
par format, or system direction as follows: if par is non-NULL, the "direction"
field is used. If par is NULL, pgSystemDirection() is returned. */

PG_PASCAL (short) pgPrimaryTextDirection (pg_globals_ptr globals, par_info_ptr par)
{
	short		direction;
	
	if (!par)
		direction = system_direction;
	else
		direction = par->direction;
	
	if (!direction)
		direction = pgSystemDirection(globals);

	return	direction;
}


/* pgSetSecondaryCaret sets the secondary_caret field in selection. This is done
by checking if the insertion point sits in between two direction runs. */

PG_PASCAL (void) pgSetSecondaryCaret (paige_rec_ptr pg, text_block_ptr block,
		t_select_ptr selection)
{
	register point_start_ptr		starts;
	par_info_ptr					par;
	rectangle						insert_bounds;
	long							local_offset, begin_start, end_start;
	short							start_direction, text_direction, adjacent_direction;
	pg_boolean						right_left;
	
	selection->secondary_caret = selection->primary_caret;
	
	if (!pg->t_length)
		return;

	par = pgFindParStyle(pg, selection->offset);
	text_direction = pgPrimaryTextDirection(pg->globals, par);
	
	right_left = FALSE;

	if (text_direction == right_left_direction) {
	
		text_direction = RIGHT_DIRECTION_BIT;
		right_left = TRUE;
	}
	else
		text_direction = 0;
	
	UnuseMemory(pg->par_formats);

	starts = (point_start_ptr) UseMemoryRecord(block->lines, selection->line, USE_ALL_RECS, TRUE);
	start_direction = starts->flags & RIGHT_DIRECTION_BIT;
	insert_bounds = starts->bounds;
	begin_start = starts->offset;
	end_start = starts[1].offset;
	local_offset = selection->offset - block->begin;
	
	adjacent_direction = start_direction;

	if (local_offset == begin_start) {
		
		if ( (starts->flags & NEW_LINE_BIT) || (!starts->offset) )
			adjacent_direction = text_direction;
		else {
			
			--starts;
			adjacent_direction = starts->flags & RIGHT_DIRECTION_BIT;
			++starts;
		}
	}
	else
	if (local_offset == end_start) {
		
		if (starts->flags & LINE_BREAK_BIT)
			adjacent_direction = text_direction;
		else
			adjacent_direction = starts[1].flags & RIGHT_DIRECTION_BIT;
	}

	if (start_direction != adjacent_direction) {  /* A dual caret situation exists */		
		long		other_caret, left_caret, right_caret;
		pg_short_t	run_qty;
		
		if (start_direction == text_direction) {
			
			if (local_offset == begin_start) {
				
				if (!(starts->flags & NEW_LINE_BIT))
					--starts;
				
				run_qty = pgDirectionRun(starts, 0, TRUE);
				starts -= (run_qty - 1);
				sides_of_run(starts, run_qty, &left_caret, &right_caret);
				
				if (right_left)
					other_caret = right_caret;
				else
					other_caret = left_caret;
			}
			else {

				if (!(starts->flags & LINE_BREAK_BIT))
					++starts;
				
				run_qty = pgDirectionRun(starts, 0, FALSE);			
				sides_of_run(starts, run_qty, &left_caret, &right_caret);
				
				if (right_left)
					other_caret = left_caret;
				else
					other_caret = right_caret;
			}
			
			selection->secondary_caret = other_caret - insert_bounds.top_left.h;
		}
		else {

			if (local_offset == end_start) {
				
				run_qty = pgDirectionRun(starts, 0, TRUE);
				starts -= (run_qty - 1);
				sides_of_run(starts, run_qty, &left_caret, &right_caret);
				
				if (right_left)
					other_caret = left_caret;
				else
					other_caret = right_caret;
			}
			else {

				run_qty = pgDirectionRun(starts, 0, FALSE);			
				sides_of_run(starts, run_qty, &left_caret, &right_caret);
				
				if (right_left)
					other_caret = right_caret;
				else
					other_caret = left_caret;
			}
			
			selection->primary_caret = other_caret - insert_bounds.top_left.h;
		}
	}

	UnuseMemory(block->lines);
}


/* pgFlipLong reverses a long to wanted_direction based on base area */

PG_PASCAL (long) pgFlipLong (rectangle_ptr base, long value, short wanted_direction)
{
	if (wanted_direction == right_left_direction)
		return base->bot_right.h - (value - base->top_left.h);
	
	return	base->top_left.h + (base->bot_right.h - value);
}


/* pgFlipRect reverses a coordinate to right-left based on base area */

PG_PASCAL (void) pgFlipRect (rectangle_ptr base, rectangle_ptr rect, short wanted_direction)
{
	register rectangle_ptr	the_rect;
	long					temp_left;

	the_rect = rect;
	
	temp_left = pgFlipLong(base, the_rect->top_left.h, wanted_direction);
	the_rect->top_left.h = pgFlipLong(base, the_rect->bot_right.h, wanted_direction);
	the_rect->bot_right.h = temp_left;
}


/* pgRightLeftOffset returns the amount to offset the_start's bounds assuming
that the primary text direction is right-to-left. This is used to display text
and/or to position insertion points for right-to-left primary direction. Hence,
this function returns the amount of horizontal offset to add to the_start->bounds
for proper screen placement. The base param is the page rectangle base. */

PG_PASCAL (long) pgRightLeftOffset (rectangle_ptr base, point_start_ptr the_start,
		short wanted_direction)
{
	register point_start_ptr	starts;
	rectangle					flipped_bounds;
	long						left_base;
	pg_short_t					run_qty;

	starts = the_start;
	
	if (!(starts->flags & NEW_LINE_BIT))
		starts -= (pgDirectionRun(starts, 0, TRUE) - 1);
	
	run_qty = pgDirectionRun(starts, 0, FALSE);
	sides_of_run(starts, run_qty, &flipped_bounds.top_left.h,  &flipped_bounds.bot_right.h);
	left_base = flipped_bounds.top_left.h;
	pgFlipRect(base, &flipped_bounds, wanted_direction);
	
	return	flipped_bounds.top_left.h - left_base;
}


/******************************* Local Functions ****************************/


/* This function sets left and/or right side(s) of a direction run
beginning at first_start and continuing though num_starts. The purpose of this
is for cases where bounds rects have been switched around. If num_starts is
zero then the left/right edges are returned for first_start through end of
line regardless of direction(s).  */

static void sides_of_run (point_start_ptr first_start, pg_short_t num_starts,
		long PG_FAR *left_side, long PG_FAR *right_side)
{
	register point_start_ptr	starts;
	rectangle					first_bounds, last_bounds;

	first_bounds = first_start->bounds;
	
	if (!num_starts) {
		
		starts = first_start;
		
		for (;;) {
			
			pgUnionRect(&first_bounds, &starts->bounds, &first_bounds);
			
			if (starts->flags & LINE_BREAK_BIT)
				break;

			++starts;
		}
		
		if (left_side)
			*left_side = first_bounds.top_left.h;
		if (right_side)
			*right_side = first_bounds.bot_right.h;
	}
	else {
	
		last_bounds = first_start[num_starts - 1].bounds;
	
		if (left_side) {
			
			if (first_bounds.top_left.h < last_bounds.top_left.h)
				*left_side = first_bounds.top_left.h;
			else
				*left_side = last_bounds.top_left.h;
		}
		
		if (right_side) {
	
			if (first_bounds.bot_right.h < last_bounds.bot_right.h)
				*right_side = last_bounds.bot_right.h;
			else
				*right_side = first_bounds.bot_right.h;
		}
	}
}



/* This reverses a rectangle's left/right edges if backwards. Function returns
TRUE if the rect has horizontal dimension.  */

static pg_boolean fix_rect (rectangle_ptr rect, rectangle_ptr max_rect)
{
	long		swap;

	if (rect->top_left.h > rect->bot_right.h) {
		
		swap = rect->top_left.h;
		rect->top_left.h = rect->bot_right.h;
		rect->bot_right.h = swap;
	}

	if (max_rect) {
		
		if (rect->top_left.h < max_rect->top_left.h)
			rect->top_left.h = max_rect->top_left.h;
		if (rect->bot_right.h > max_rect->bot_right.h)
			rect->bot_right.h = max_rect->bot_right.h;
	}
	
	return	(rect->top_left.h < rect->bot_right.h);
}


static pg_boolean find_char_in_table (pg_short_t PG_FAR *table, pg_short_t the_char)
{
	register pg_short_t  PG_FAR		*table_ptr;
	
	table_ptr = table;
	
	while (*table_ptr)
		if (*table_ptr++ == the_char)
			return	TRUE;
	
	return	FALSE;
}

