/* This file deals with printing support functions  */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgprint
#endif

#include "machine.h"
#include "pgEdit.h"
#include "pgSelect.h"
#include "pgUtils.h"
#include "pgShapes.h"
#include "pgText.h"


static long offset_to_line_top (paige_rec_ptr pg, long offset);


/* pgSetPrintDevice takes the machine-specific device and forces the pg_ref to match
the screen to the eventual printed resolution. This is somewhat machine-specific. For
Macintosh the device is a THPrint record. For Windows device is a printer DC, and if
this function is called from Windows the DC must NOT be released.
The function returns TRUE if the pg_ref ought to be re-drawn due to printer changes. */

PG_PASCAL (pg_boolean) pgSetPrintDevice (pg_ref pg, generic_var device)
{
	paige_rec_ptr		pg_rec;
	pg_boolean			result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	result = (device != pg_rec->port.print_port);
	pg_rec->port.print_port = device;

	if (result) {
	
		pgInvalSelect(pg, 0, pg_rec->t_length);
		pgPrintDeviceChanged(pg_rec);
	}

	UnuseMemory(pg);

	return	result;
}


/* pgGetPrintDevice returns the "device" set earlier from pgMatchPrintDevice(). */

PG_PASCAL (generic_var) pgGetPrintDevice (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	generic_var			result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->port.print_port;
	UnuseMemory(pg);
	
	return	result;
}


/* pgPrintToPage draws pg to the target device, beginning at text offset starting_position
and maps all text to the page_rect. The function returns the next text position
(or zero if the end of the document was printed).  */

PG_PASCAL (long) pgPrintToPage (pg_ref pg, const graf_device_ptr target, long starting_position,
		const rectangle_ptr page_rect, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	graf_device_ptr			print_dev;
	rectangle_ptr			wrap_bounds;
	shape_ref				temp_new_vis;
	co_ordinate				offset_extra;
	long					next_position;
	short					use_draw_mode;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (!(print_dev = target))
		print_dev = &pg_rec->port;
	
	pgOpenPrinter(pg_rec, print_dev, starting_position, page_rect);

	if ((use_draw_mode = draw_mode) == best_way)
		use_draw_mode = direct_or;

	wrap_bounds = (rectangle_ptr) UseMemory(pg_rec->wrap_area);
	offset_extra = wrap_bounds->top_left;
	UnuseMemory(pg_rec->wrap_area);
	
	offset_extra.v += (offset_to_line_top(pg_rec, starting_position) - offset_extra.v);

	pgNegatePt(&offset_extra);
	pgAddPt(&pg_rec->scroll_pos, &offset_extra);
	pgAddPt(&page_rect->top_left, &offset_extra);

	temp_new_vis = pgRectToShape(pg_rec->globals->mem_globals, page_rect);
	pg_rec->flags |= PRINT_MODE_BIT;

	next_position = pgUpdateText(pg_rec, target, starting_position,
		pg_rec->t_length, temp_new_vis, &offset_extra, use_draw_mode, FALSE);
	pgDrawPages(pg_rec, target, &offset_extra, temp_new_vis, use_draw_mode);

	pg_rec->flags &= (~PRINT_MODE_BIT);

	if (next_position == pg_rec->t_length)
		next_position = 0;
	
	pgDisposeShape(temp_new_vis);

	pgClosePrinter(pg_rec, print_dev);

	UnuseMemory(pg);
	
	return	next_position;
}



/* pgNumPages returns the number of pages of page_rect size that would be
required to print the whole pg document, assuming these same parameters were
passed to pgPrintToPage above.  */

PG_PASCAL (short) pgNumPages (pg_ref pg, const rectangle_ptr page_rect)
{
	short		page_ctr;
	long		print_offset;
	
	print_offset = 0;
	page_ctr = 1;
	
	while (print_offset = pgPrintToPage(pg, NULL, print_offset, page_rect, draw_none))
		++page_ctr;
	
	return	page_ctr;
}


/* pgFindPage returns the text offset that should be passed to pgPrintPage to
print page_num (first page is ONE, not zero). It is assumed page_rect would
be passed to pgPrintToPage.  */

PG_PASCAL (long) pgFindPage (pg_ref pg, short page_num, const rectangle_ptr page_rect)
{
	short		page_ctr;
	long		print_offset;
	
	print_offset = 0;
	page_ctr = 1;
	
	if (page_num > 1)
		while (print_offset = pgPrintToPage(pg, NULL, print_offset, page_rect, draw_none)) {
		
			++page_ctr;
			
			if (page_ctr == page_num)
				break;
	}

	return	print_offset;
}


/********************************* Local Functions  ****************************/


/* This returns the vertical position of the line containing offset. If necessary, special
exclusions that caused the first line to move down are adjusted (so WYSIWYG printing is
achieved with exclusion areas). */

static long offset_to_line_top (paige_rec_ptr pg, long offset)
{
	text_block_ptr					block;
	register point_start_ptr		starts;
	point_start_ptr					first_starts;
	rectangle_ptr					wrap_rects;
	co_ordinate						repeat_offset;
	long							local_offset, top, previous_bottom;
	long							r_num;

	block = pgFindTextBlock(pg, offset, NULL, TRUE, TRUE);

	starts = (point_start_ptr) UseMemory(block->lines);
	first_starts = starts;

	local_offset = offset - block->begin;
	
	while (local_offset > (long)starts->offset)
		++starts;
	top = starts->bounds.top_left.v;

/* Now check to see if the current line has been shoved down due to an exclusion area: */

	while (!(starts->flags & NEW_LINE_BIT))
		--starts;

	wrap_rects = (rectangle_ptr) UseMemory(pg->wrap_area);
	r_num = starts->r_num;
	wrap_rects += (pgGetWrapRect(pg, r_num, &repeat_offset) + 1);
	previous_bottom = wrap_rects->top_left.v + repeat_offset.v;  // The default (if first line, for instance).

	if (starts->offset) {
		
		--starts;
		
		if (starts->r_num == r_num)
			previous_bottom = starts->bounds.bot_right.v;

		++starts;
	}
	else
	if (block->begin) {
		
		--block;

		if (block->end_start.r_num == r_num)
			previous_bottom = block->end_start.bounds.bot_right.v;

		++block;
	}

	top -= (starts->bounds.top_left.v - previous_bottom);

	UnuseMemory(pg->wrap_area);
	
	UnuseMemory(block->lines);
	UnuseMemory(pg->t_blocks);
	
	return	top;
}



