/* This file includes basic Paige stuff that always needs to be linked in. You
cannot omit this file from your app!   */

/* Modified Dec 22 1994 for the use of volatile in PG_TRY-PG_CATCH by TR Shaw, OITC */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgbasic3
#endif

#include "pgExceps.h"
#include "pgText.h"
#include "pgDefStl.h"
#include "pgDefPar.h"
#include "pgSelect.h"
#include "pgEdit.h"
#include "pgShapes.h"
#include "pgUtils.h"
#include "machine.h"
#include "defprocs.h"
#include "pgBasics.h"
#include "pgTxtWid.h"
#include "pgSubref.h"
#include "pgLists.h"
#include "pgHText.h"
#include "pgTables.h"
#include "pgGrafx.h"
#include "pgFrame.h"

/* Values for borders. */

enum {
	drawing_top,
	drawing_left,
	drawing_right,
	drawing_bottom
};


static shape_ref copy_shape_param (pg_globals_ptr globals, shape_ref shape_param);
static size_t free_cached_blocks (paige_rec_ptr pg, memory_ref ref_mask, size_t needed_free);
void set_max_run (paige_rec_ptr pg_rec, memory_ref the_run);
static void draw_border_line (paige_rec_ptr pg, co_ordinate_ptr begin_pt,
		co_ordinate_ptr end_pt, long source_border_info, long border_info,
		long pen_size, pg_short_t cell_info, long border_color, long cell_bk_color, short verb);

#define BORDER_LIGHT_SCALE		13
#define BORDER_MEDIUM_SCALE		-27
#define BORDER_DARK_SCALE		-54

/* DEFAULT "MERGE TEXT" PROC. By default, this simply returns FALSE which
creates a non-effect for merging.  */

PG_PASCAL (pg_boolean) pgMergeProc (paige_rec_ptr pg, style_info_ptr style, pg_char_ptr text_data,
		pg_short_t length, text_ref merged_data, long ref_con)
{
#ifdef MAC_PLATFORM
#pragma unused (pg, style, text_data, length, merged_data, ref_con)
#endif
	return	FALSE;
}


/* DEFAULT "UNDO" PROC which gives the host program a chance to modify and/or
perform actions for custom undo's. BY default this does nothing (since standard
undo's are handled in pgClipBd.c). */

PG_PASCAL (void) pgEnhanceUndo (paige_rec_ptr pg, pg_undo_ptr undo_rec,
		void PG_FAR *insert_ref, short action_to_take)
{
#ifdef MAC_PLATFORM
#pragma unused (pg, undo_rec, insert_ref, action_to_take)
#endif
}

/* pgLineGlitterProc is the default "draw after line is drawn,"  The app can use this to place
ornaments on the line. The default function draws borders if they are set. */

PG_PASCAL (void) pgLineGlitterProc (paige_rec_ptr pg, style_walk_ptr walker,
		long line_number, long par_number, text_block_ptr block, point_start_ptr first_line,
		point_start_ptr last_line, point_start_ptr previous_first, point_start_ptr previous_last,
		co_ordinate_ptr offset_extra, rectangle_ptr vis_rect, short call_verb)
{
	par_info_ptr		par;
	long				border_shading, border_info;
	rectangle			line_rect, borders;

	if (call_verb == glitter_post_bitmap_draw)
		return;

	par = walker->cur_par_style;
	
	if (!par->table.border_info && !par->table.table_columns && !par->table.border_shading)
		return;

	if (pgGetBorderInfo(pg, block, first_line, &line_rect, &borders, &border_shading, &border_info, NULL, NULL)) {
		pgOffsetRect(&line_rect, offset_extra->h, offset_extra->v);

		if (call_verb == glitter_pre_draw) {
			graf_device_ptr			target_device;

	        if (pg->globals->offscreen_enable == OFFSCREEN_SUCCESSFUL)
	        	target_device = &pg->globals->offscreen_port;
	        else
	        	target_device = &pg->port;

			if (border_shading)
				pgPaintObject(target_device, &line_rect, 0, object_rect, border_shading);
		}
		else
		if (border_info) {
			co_ordinate		top_right, bot_left, bot_right;
			long			added_extra;
			
			top_right = bot_left = line_rect.bot_right;
			top_right.v = line_rect.top_left.v;
			bot_left.h = line_rect.top_left.h;
			bot_right = line_rect.bot_right;

			if (borders.bot_right.v & PG_BORDER_RULE) {
				long		half_value;
				
				half_value = (bot_right.v - line_rect.top_left.v) / 2;
				bot_right.v -= half_value;
				bot_left.v -= half_value;
			}

			if (borders.top_left.v)
				draw_border_line(pg, &line_rect.top_left, &top_right, border_info,
							borders.top_left.v, 0, first_line->cell_num,
							par->table.top_border_color, border_shading, drawing_top);

			if (borders.bot_right.v)
				draw_border_line(pg, &bot_left, &bot_right, border_info,
							borders.bot_right.v, 0, first_line->cell_num,
							par->table.bottom_border_color, border_shading, drawing_bottom);
			
			if (borders.top_left.h) {
				
				added_extra = 0;
				
				if (first_line->cell_num)
					if (!(first_line->cell_num & CELL_BOT_BIT))
						added_extra = borders.bot_right.v;
				
				bot_left.v += added_extra;
				draw_border_line(pg, &line_rect.top_left, &bot_left, border_info,
							borders.top_left.h, 0, first_line->cell_num,
							par->table.left_border_color, border_shading, drawing_left);
				bot_left.v -= added_extra;
			}
			
			if (borders.bot_right.h)
				draw_border_line(pg, &top_right, &line_rect.bot_right, border_info,
							borders.bot_right.h, 0, first_line->cell_num,
							par->table.right_border_color, border_shading, drawing_right);
		}
	}
}


/* pgAdjustBorders adjust the left and right sides per the border info. The left_extra and
right_extra params are adjusted. */

PG_PASCAL (void) pgAdjustBorders (par_info_ptr par_style, long PG_FAR *left_extra, long PG_FAR *right_extra)
{
	if (par_style->table.border_info) {
		long			pensize, border_info;

		if (par_style->table.border_info & PG_BORDER_LEFT) {
			
			*left_extra += par_style->table.border_spacing;

			pensize = (par_style->table.border_info & PG_BORDER_PENSIZE);

			if (par_style->table.border_info & PG_BORDER_DOUBLE)
				pensize += (pensize + 1);

			*left_extra += pensize;
		}

		if (par_style->table.border_info & PG_BORDER_RIGHT) {
			long		original;
			
			*right_extra += par_style->table.border_spacing;
			border_info = (par_style->table.border_info & PG_BORDER_RIGHT) >> 8;
			
			pensize = original = border_info & PG_BORDER_PENSIZE;

			if (border_info & PG_BORDER_DOUBLE)
				pensize += (original + 1);

			if (border_info & PG_BORDER_SHADOW)
				pensize += original;

			*right_extra += pensize;
		}
	}
}


/* pgCacheFree is the function (for 2.0) which frees text_blocks that are cached but not
changed. This occurs before memory_refs are purged. */

PG_PASCAL (size_t) pgCacheFree (pgm_globals_ptr mem_globals, memory_ref dont_free, size_t desired_free)
{
	pg_ref PG_FAR	*paige_refs;
	paige_rec_ptr	pg_rec;
	size_t			num_refs;
	size_t			freed_memory = 0;

	if (mem_globals->freemem_info) {
		
		num_refs = GetMemorySize(mem_globals->freemem_info);
		paige_refs = (pg_ref PG_FAR*) UseMemory(mem_globals->freemem_info);
		
		while (num_refs) {
			
			if (*paige_refs && (*paige_refs != dont_free)) {
				
				pg_rec = (paige_rec_ptr) UseMemory(*paige_refs);
				
				if (pg_rec->cache_file)
					freed_memory += free_cached_blocks(pg_rec, dont_free, desired_free - freed_memory);

				UnuseMemory(*paige_refs);
				
				if (freed_memory >= desired_free)
					break;
			}

			++paige_refs;
			--num_refs;
		}
		
		UnuseMemory(mem_globals->freemem_info);
	}

	return		freed_memory;
}


/* pgExtendProc gets called after a new pg_ref has been created, and also
when it has been disposed. The default function does nothing. */

PG_PASCAL (void) pgExtendProc (void PG_FAR *param, short verb)
{

}



/* pgWaitProc is the standard "wait-process" function, which does nothing.  */

PG_PASCAL (void) pgWaitProc (paige_rec_ptr pg, short wait_verb, size_t progress_ctr,
		size_t completion_ctr)
{
#ifdef MAC_PLATFORM
#pragma unused (pg, wait_verb, progress_ctr, completion_ctr)
#endif
}


/* pgDrawScrollProc is the default pre and post-draw for scrolling. It does nothing
by default. When it gets called the clipping is set to the scrolled area. */

PG_PASCAL (void) pgDrawScrollProc (paige_rec_ptr pg, shape_ref update_rgn,
		co_ordinate_ptr scroll_pos, pg_boolean post_call)
{
#ifdef MAC_PLATFORM
#pragma unused (pg, update_rgn, scroll_pos, post_call)
#endif
}


/* pgModifyContainerProc is the default container-modify function, which does
nothing by default. */

PG_PASCAL (void) pgModifyContainerProc (paige_rec_ptr pg, pg_short_t container_num,
		rectangle_ptr container, pg_scale_ptr scaling, co_ordinate_ptr screen_extra,
		short verb, void PG_FAR *misc_info)
{
#ifdef MAC_PLATFORM
#pragma unused (container_num, misc_info)
#endif

	if (verb == erase_rect_verb) {
		
		if (pgEmptyRect(container))
			return;

		if ((pg->flags & BITMAP_ERASE_BIT) && !pg->globals->offscreen_enable) {
			rectangle		rect_to_erase, vis_rect, scaled_vis_rect, actual_rect;
			co_ordinate		not_used, not_scrolled;
			
			pgScaleRectToRect(scaling, container, &rect_to_erase, screen_extra);
			pgShapeBounds(pg->vis_area, &vis_rect);
			
			if (screen_extra) {
				
				not_scrolled = *screen_extra;
				pgAddPt(&pg->scroll_pos, &not_scrolled);
			}
			else
				not_scrolled.h = not_scrolled.v = 0;
			
			pg->port.scale.scale = -pg->port.scale.scale;
			pgScaleRectToRect(&pg->port.scale, &vis_rect, &scaled_vis_rect, &not_scrolled);
			pg->port.scale.scale = -pg->port.scale.scale;

			if (pgSectRect(&rect_to_erase, &scaled_vis_rect, &rect_to_erase)) {
			
				pgPrepareOffscreen(pg, &rect_to_erase, &actual_rect, &not_used, 0, NULL, bits_copy);
	
				for (;;) {
					
					if (pgFinishOffscreen(pg, 0, NULL, &not_used, &actual_rect, bits_copy))
						break;
				}
			}
		}
		else
			pgEraseRect(pg->globals, container, scaling, screen_extra);
	}
}


/* pgDrawPageProc is the default "page" drawing for scrolling and full updates. The default
draws gutters and page borders. */

PG_PASCAL (void) pgDrawPageProc (paige_rec_ptr pg, shape_ptr page_shape,
		pg_short_t r_qty, pg_short_t page_num, co_ordinate_ptr vis_offset,
		short draw_mode_used, short call_order)
{
	rectangle_ptr			page_ptr;
	rectangle				vis_bounds, page_rect;
	co_ordinate				negative_extra;
	long					fixed_bottom;

	if (call_order != -1)
		return;
	
	pgDrawAllFrames(pg, FRAME_DRAW_VERB);

	if ((pg->doc_info.gutter_color || pg->doc_info.page_borders) && !(pg->flags & PRINT_MODE_BIT)) {
		shape_ref			vis_page_ref;
		pgm_globals_ptr		mem_globals;
		co_ordinate			top_vis, repeat_offset;
		rectangle			page_bounds;
		size_t				num_rects;
		long				page_height;
		
		negative_extra = pg->scroll_pos;
		pgNegatePt(&negative_extra);

		pgShapeBounds(pg->vis_area, &vis_bounds);
		pg->scale_factor.scale = -pg->scale_factor.scale;
		pgScaleRect(&pg->scale_factor, &negative_extra, &vis_bounds);
		pg->scale_factor.scale = -pg->scale_factor.scale;
		
		fixed_bottom = pg->doc_info.attributes & BOTTOM_FIXED_BIT;
		pgShapeBounds(pg->wrap_area, &page_bounds);

		if (!fixed_bottom) {
			rectangle		print_target;
			
			print_target = pg->doc_info.print_target;
			pgOffsetRect(&print_target, 0, page_bounds.top_left.v);
			page_bounds = pg->doc_bounds;
			
			if (page_bounds.bot_right.v < print_target.bot_right.v)
				page_bounds.bot_right.v = print_target.bot_right.v;
			if (page_bounds.bot_right.v < (vis_bounds.bot_right.v + 8))
				page_bounds.bot_right.v = vis_bounds.bot_right.v + 8;
		}
		
		top_vis = page_bounds.top_left;
		pgAddPt(&pg->scroll_pos, &top_vis);
		top_vis.v -= 1;

		pgPixelToPage(pg->myself, &top_vis, &repeat_offset, NULL, &page_height, NULL, FALSE);
		pgAddPt(&negative_extra, &repeat_offset);
		pgOffsetRect(&page_bounds, repeat_offset.h, repeat_offset.v);

		mem_globals = pg->globals->mem_globals;
		
		if (!fixed_bottom || !(pg->doc_info.attributes & V_REPEAT_BIT))
			vis_page_ref = pgRectToShape(mem_globals, &page_bounds);
		else {
			long			bottom_plus_slop;
			
			bottom_plus_slop = vis_bounds.bot_right.v + 16;

			vis_page_ref = pgRectToShape(mem_globals, NULL);
			
			while (page_bounds.top_left.v < vis_bounds.bot_right.v) {
				
				pgShapeBounds(pg->wrap_area, &page_rect);
				pgOffsetRect(&page_rect, repeat_offset.h, repeat_offset.v);
				pgAddRectToShape(vis_page_ref, &page_rect);
				
				page_bounds.top_left.v += page_height;
				repeat_offset.v += page_height;
			}
		}

		if (pg->doc_info.gutter_color) {
		
			if (pgEmptyShape(vis_page_ref))
				pgPaintObject(&pg->port, &vis_bounds, 0, object_rect, pg->doc_info.gutter_color & 0x00FFFFFF);
			else {
				shape_ref			gutter_shape;

				gutter_shape = pgRectToShape(mem_globals, NULL);
				pgDiffShape(vis_page_ref, pg->vis_area, gutter_shape);
				
				num_rects = GetMemorySize(gutter_shape) - 1;
				page_ptr = (rectangle_ptr) UseMemory(gutter_shape);
				
				while (num_rects) {

					++page_ptr;
					page_rect = *page_ptr;
					pgScaleRect(&pg->scale_factor, &negative_extra, &page_rect);				
					pgPaintObject(&pg->port, &page_rect, 0, object_rect, pg->doc_info.gutter_color & 0x00FFFFFF);

					--num_rects;
				}

				UnuseMemory(gutter_shape);
				pgDisposeShape(gutter_shape);
			}
		}
		
		if (pg->doc_info.page_borders && !pgEmptyShape(vis_page_ref)) {

				num_rects = GetMemorySize(vis_page_ref) - 1;
				page_ptr = (rectangle_ptr) UseMemory(vis_page_ref);

				while (num_rects) {

					++page_ptr;
					
					page_rect = *page_ptr;
					pgScaleRect(&pg->scale_factor, &negative_extra, &page_rect);				
					pgDrawGeneralBorders(pg, pg->doc_info.page_borders, 0, 0, &page_rect, FALSE);

					--num_rects;
				}
				
				UnuseMemory(vis_page_ref);
			}
			
		pgDisposeShape(vis_page_ref);
	}
}

/* pgDrawGeneralBorders draws general borders around bounds. BOUNDS ALREADY POSITIONED
AND SCALED. */

PG_PASCAL (void) pgDrawGeneralBorders (paige_rec_ptr pg, long border_info, long border_color,
				long shading, rectangle_ptr bounds, pg_boolean no_insets)
{
	co_ordinate		top_right, bot_left;
	rectangle		box;
	long			left_border, right_border, top_border, bottom_border;
	
	box = *bounds;

	left_border = border_info & PG_BORDER_LEFT;
	right_border = (border_info & PG_BORDER_RIGHT) >> 8;
	top_border = (border_info & PG_BORDER_TOP) >> 16;
	bottom_border = (border_info & PG_BORDER_BOTTOM) >> 24;
	
	if (no_insets) {
		long		pensize;
		
		pensize = right_border & PG_BORDER_PENSIZE;
		
		if (right_border & PG_BORDER_DOUBLE)
			pensize += (pensize + 1);
		else
		if (right_border & PG_BORDER_SHADOW)
			pensize += pensize;
		
		box.bot_right.h -= pensize;

		pensize = bottom_border & PG_BORDER_PENSIZE;
		
		if (bottom_border & PG_BORDER_DOUBLE)
			pensize += (pensize + 1);
		else
		if (bottom_border & PG_BORDER_SHADOW)
			pensize += pensize;

		box.bot_right.v -= pensize;
	}
	else {
	
		box.top_left.h -= (left_border & PG_BORDER_PENSIZE);
		box.top_left.v -= (top_border & PG_BORDER_PENSIZE);
		box.bot_right.h += (right_border & PG_BORDER_PENSIZE);
		box.bot_right.v += (bottom_border & PG_BORDER_PENSIZE);
	}

	top_right = box.top_left;
	top_right.h = box.bot_right.h;
	bot_left = box.bot_right;
	bot_left.h = box.top_left.h;

	if (top_border)
		draw_border_line(pg, &box.top_left, &top_right, border_info,
					top_border, 0, 0, border_color, shading, drawing_top);

	if (bottom_border)
		draw_border_line(pg, &bot_left, &box.bot_right, border_info,
					bottom_border, 0, 0, border_color, shading, drawing_bottom);

	if (left_border)
		draw_border_line(pg, &box.top_left, &bot_left, border_info,
					left_border, 0, 0, border_color, shading, drawing_left);

	if (right_border)
		draw_border_line(pg, &top_right, &box.bot_right, border_info,
					right_border, 0, 0, border_color, shading, drawing_right);
}


/* pgSetAuthor sets the app signature */

PG_PASCAL (void) pgSetAuthor (pg_ref pg, long author)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pg_rec->author = author;
	UnuseMemory(pg);
}

/* pgGetAuthor returns the app signature */

PG_PASCAL (long) pgGetAuthor (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	long				author;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	author = pg_rec->author;
	UnuseMemory(pg);
	
	return	author;
}


/* pgSetLockID sets an app-chosen long ID for "locking." If the current
ID is non-zero and ID param is non-zero, function result is FALSE if they
don't match and the ID not set. Otherwise, ID is set and result is TRUE. */

PG_PASCAL (pg_boolean) pgSetLockID (pg_ref pg, long ID)
{
	paige_rec_ptr		pg_rec;
	long				cur_id;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	cur_id = pg_rec->lock_id;

	if ((!ID) || (!cur_id))
		pg_rec->lock_id = cur_id = ID;

	UnuseMemory(pg);
	
	return	((!ID) || (cur_id == ID));
}


/* pgGetLockID returns the current lock ID  */

PG_PASCAL (long) pgGetLockID (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	long				result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->lock_id;
	UnuseMemory(pg);
	
	return	result;
}


/* pgNew creates a new, empty Paige object and returns the pg_ref */

PG_PASCAL (pg_ref) pgNew (const pg_globals_ptr globals, const generic_var def_device,
		shape_ref vis_area, shape_ref page_area, shape_ref exclude_area,
		long attributes)
{
	register paige_rec_ptr		pg_rec;
	rectangle_ptr				vis_ptr;
	style_info_ptr				first_style;
	par_info_ptr				first_par;
	font_info_ptr				first_font;
	pg_hyperlink_ptr			first_link;
	
#ifdef PG_OITC
	pg_list_rec					list_rec;
#endif

	volatile pg_ref				pg = MEM_NULL;

	pg_rec = NULL;

	PG_TRY(globals->mem_globals) {

		++globals->mem_globals->next_mem_id;
		globals->mem_globals->current_id = globals->mem_globals->next_mem_id;
	
		pg = MemoryAllocClear(globals->mem_globals, sizeof(paige_rec), 1, 0);
		pg_rec = (paige_rec_ptr) UseMemory(pg);
		pg_rec->mem_id = globals->mem_globals->current_id;
	
		pg_rec->globals = globals;
		pg_rec->version = PAIGE_VERSION;
		pg_rec->platform = PAIGE_PLATFORM;
		pg_rec->pg_type = MAIN_DOCUMENT;
		pg_rec->doc_pg = pg;
		pg_rec->flags = attributes & (~NO_DEVICE_BIT);
		pg_rec->flags2 = INITIAL_TEXT_FLAGS;

		if (pg_rec->flags & LINE_EDITOR_BIT)
			pg_rec->flags |= (NO_WRAP_BIT | NO_LF_BIT);

		pg_rec->scroll_align_h = pg_rec->scroll_align_v = -1;
		pg_rec->doc_info.num_pages = 1;
		//pg_rec->doc_info.gutter_color = 0x00FFFFFF;
		pg_rec->text_direction = pgSystemDirection(globals);

		pg_rec->bk_color = globals->def_bk_color;
		pgBlockMove(&globals->def_hooks, &pg_rec->procs, sizeof(pg_hooks));
		
		pg_rec->autoscroll_mode = bits_emulate_or;

		if (!def_device)
			pgInitDefaultDevice(globals, &pg_rec->port);
		else {
			
			if (def_device == USE_NO_DEVICE) {
				
				pgBlockMove(&globals->offscreen_port, &pg_rec->port, sizeof(graf_device));
				pg_rec->flags |= NO_DEVICE_BIT;
				pg_rec->port.resolution = pgDeviceResolution(&pg_rec->port);
			}
			else {
			
				pgInitDevice(globals, def_device, 0, &pg_rec->port);
			}
		}

		pg_rec->resolution = pg_rec->port.resolution;
		pg_rec->myself = pg;

		pgScaleGrafDevice(pg_rec);

		pg_rec->select = MemoryAllocClear(globals->mem_globals, sizeof(t_select),
				MINIMUM_SELECT_MEMSIZE, 4);

		pg_rec->vis_area = copy_shape_param(globals, vis_area);
		pg_rec->wrap_area = copy_shape_param(globals, page_area);
		pg_rec->exclude_area = copy_shape_param(globals, exclude_area);
		
		vis_ptr = (rectangle_ptr) UseMemory(pg_rec->vis_area);
		pg_rec->base_vis_origin = vis_ptr->top_left;
		UnuseMemory(pg_rec->vis_area);

		pg_rec->t_blocks = MemoryAlloc(globals->mem_globals, sizeof(text_block), 1, 1);
		pg_rec->tlevel_blocks = pg_rec->t_blocks;
		pgInitTextblock(pg_rec, 0, 0, (text_block_ptr) UseMemory(pg_rec->t_blocks), FALSE);
		UnuseMemory(pg_rec->t_blocks);
	
		pg_rec->hilite_rgn = pgRectToShape(globals->mem_globals, NULL);
		pg_rec->temp_rgn = pgRectToShape(globals->mem_globals, NULL);
		pg_rec->t_formats = MemoryAllocClear(globals->mem_globals, sizeof(style_info), 1, 1);
	
		pg_rec->par_formats = MemoryAllocClear(globals->mem_globals, sizeof(par_info), 1, 1);
		pg_rec->fonts = MemoryAllocClear(globals->mem_globals, sizeof(font_info), 1, 1);
		pg_rec->t_style_run = MemoryAllocClear(globals->mem_globals, sizeof(style_run), 2, 4);
		pg_rec->par_style_run = MemoryAllocClear(globals->mem_globals, sizeof(style_run), 2, 4);
		pg_rec->par_exclusions = MemoryAllocClear(globals->mem_globals, sizeof(style_run), 0, 2);
		pg_rec->key_buffer = MemoryAlloc(globals->mem_globals, sizeof(pg_char), 0, 16);
		pg_rec->containers = MemoryAlloc(globals->mem_globals, sizeof(long), 0, 4);
		pg_rec->exclusions = MemoryAlloc(globals->mem_globals, sizeof(long), 0, 4);
		pg_rec->buf_special = MemoryAlloc(globals->mem_globals, sizeof(pg_char), 0, 24);
		pg_rec->named_styles = MemoryAlloc(globals->mem_globals, sizeof(named_stylesheet), 0, 0);
		pg_rec->hyperlinks = MemoryAllocClear(globals->mem_globals, sizeof(pg_hyperlink), 1, 2);
		pg_rec->target_hyperlinks = MemoryAllocClear(globals->mem_globals, sizeof(pg_hyperlink), 1, 2);
		pg_rec->subref_stack = MemoryAlloc(globals->mem_globals, sizeof(paige_sub_rec), 0, 2);

		first_font = (font_info_ptr) UseMemory(pg_rec->fonts);
		pgBlockMove(&globals->def_font, first_font, sizeof(font_info));
			
		first_style = (style_info_ptr) UseMemory(pg_rec->t_formats);
		pgBlockMove(&globals->def_style, first_style, sizeof(style_info));

		pg_rec->procs.font_proc(pg_rec, first_font);
		first_style->procs.init(pg_rec, first_style, first_font);
		first_style->procs.duplicate(MEM_NULL, pg_rec, pg_new_reason,
			pg_rec->t_formats, first_style);

		first_style->used_ctr = 2;	// Gar change to force default font to never delete
	
		UnuseMemory(pg_rec->fonts);
		UnuseMemory(pg_rec->t_formats);
		
		first_par = (par_info_ptr) UseMemory(pg_rec->par_formats);
		pgBlockMove(&globals->def_par, first_par, sizeof(par_info));
		
		first_par->procs.duplicate(MEM_NULL, pg_rec, pg_new_reason,
			pg_rec->par_formats, first_par);
	
		first_par->used_ctr = 2;
		UnuseMemory(pg_rec->par_formats);
		
		set_max_run(pg_rec, pg_rec->t_style_run);
		set_max_run(pg_rec, pg_rec->par_style_run);
	
		first_link = (pg_hyperlink_ptr) UseMemory(pg_rec->hyperlinks);
		first_link->applied_range.begin = first_link->applied_range.end = ZERO_TEXT_PAD;
		UnuseMemory(pg_rec->hyperlinks);

		first_link = (pg_hyperlink_ptr) UseMemory(pg_rec->target_hyperlinks);
		first_link->applied_range.begin = first_link->applied_range.end = ZERO_TEXT_PAD;
		UnuseMemory(pg_rec->target_hyperlinks);

		pgResetDocBounds(pg_rec);
	
		SetMemoryPurge(pg_rec->vis_area, NO_PURGING_STATUS, FALSE);
		SetMemoryPurge(pg_rec->select, NO_PURGING_STATUS, FALSE);
		SetMemoryPurge(pg_rec->hilite_rgn, NO_PURGING_STATUS, FALSE);
		SetMemoryPurge(pg_rec->t_blocks, NO_PURGING_STATUS, FALSE);
		SetMemoryPurge(pg, NO_PURGING_STATUS, FALSE);
		SetMemoryPurge(pg_rec->t_style_run, 0x00A0, FALSE);
		SetMemoryPurge(pg_rec->par_style_run, 0x00A0, FALSE);

		++globals->mem_globals->next_mem_id;
		globals->mem_globals->current_id = globals->mem_globals->next_mem_id;

		pg_rec->doc_info.attributes = NO_CLIP_PAGE_AREA;

#ifdef PG_OITC
		list_rec.pg = pg;
		list_rec.flags = 0;
		pgAppendToList(globals->pg_list, &list_rec);
#endif
	}
	
	PG_CATCH {
		
		pgFailureDispose(pg);
		return	MEM_NULL;
	}
	
	PG_ENDTRY;

	UnuseMemory(pg);
	
	globals->pg_extend((void PG_FAR *) pg, pg_new);

	return pg;
}


/* pgNewShared creates a new pg_ref but uses shared_from common structures. The
remaining parameters are shared if they corresponding item is NULL. */

PG_PASCAL (pg_ref) pgNewShared (pg_ref shared_from, const generic_var def_device,
		shape_ref vis_area, shape_ref page_area, shape_ref exclude_area,
		long attributes)
{
	paige_rec_ptr	shared_rec;
	pg_ref			new_ref;
	generic_var		use_device;
	long			shared_flags;

	if (!def_device)
		use_device = USE_NO_DEVICE;
	/*Begin Ben Hack*/
	else
		use_device = def_device;
	/*End Ben Hack*/
	
	shared_rec = (paige_rec_ptr) UseMemory(shared_from);
	new_ref = pgNew(shared_rec->globals, use_device, vis_area, page_area, exclude_area, attributes);
	shared_flags = 0;
	
	if (!def_device)
		shared_flags |= SHARED_GRAF_DEVICE;
	if (!vis_area)
		shared_flags |= SHARED_VIS_AREA;
	if (!page_area)
		shared_flags |= SHARED_PAGE_AREA;
	if (!exclude_area)
		shared_flags |= SHARED_EXCLUDE_AREA;

	UnuseMemory(shared_from);
	
	pgShareRefs(new_ref, shared_from, shared_flags);
	
	return	new_ref;
}


/* pgShareRefs causes pg to share common structures from shared_from. The purpose of this is
to create one common set of formats and (optionally) the page and vis shapes. */

PG_PASCAL (void) pgShareRefs (pg_ref pg, pg_ref shared_from, long shared_flags)
{
	paige_rec_ptr	pg_rec, shared_rec;
	long			previous_shared_flags;
	style_info_ptr	shared_default;
	par_info_ptr	shared_par_default;

	shared_rec = (paige_rec_ptr) UseMemory(shared_from);
	shared_rec->flags2 |= IS_MASTER_BIT;

	shared_default = (style_info_ptr) UseMemory(shared_rec->t_formats);
	shared_default->used_ctr += 1;
	UnuseMemory(shared_rec->t_formats);

	shared_par_default = (par_info_ptr) UseMemory(shared_rec->par_formats);
	shared_par_default->used_ctr += 1;
	UnuseMemory(shared_rec->par_formats);

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	previous_shared_flags = pg_rec->shared_flags;
	pg_rec->shared_pg = shared_from;
	pg_rec->shared_flags = shared_flags | SHARED_FORMATS;

	DisposeMemory(pg_rec->t_formats);
	DisposeMemory(pg_rec->par_formats);
	DisposeMemory(pg_rec->fonts);
	DisposeMemory(pg_rec->named_styles);
	
	pg_rec->t_formats = shared_rec->t_formats;
	pg_rec->par_formats = shared_rec->par_formats;
	pg_rec->fonts = shared_rec->fonts;
	pg_rec->named_styles = shared_rec->named_styles;

	if (shared_flags & SHARED_GRAF_DEVICE) {

		if (!(pg_rec->flags & NO_DEVICE_BIT) && !(previous_shared_flags & SHARED_GRAF_DEVICE))
			pgCloseDevice(pg_rec->globals, &pg_rec->port);

		pgBlockMove(&shared_rec->port, &pg_rec, sizeof(graf_device));
		pg_rec->flags &= (~NO_DEVICE_BIT);
		pg_rec->flags |= (shared_rec->flags & NO_DEVICE_BIT);
	}

	if (shared_flags & SHARED_VIS_AREA) {
		
		pgDisposeShape(pg_rec->vis_area);
		pg_rec->vis_area = shared_rec->vis_area;
	}

	if (shared_flags & SHARED_PAGE_AREA) {
		
		pgDisposeShape(pg_rec->wrap_area);
		pg_rec->wrap_area = shared_rec->wrap_area;
	}

	if (shared_flags & SHARED_EXCLUDE_AREA) {
		
		if (pg_rec->exclude_area)
			pgDisposeShape(pg_rec->exclude_area);

		pg_rec->exclude_area = shared_rec->exclude_area;
	}

	UnuseMemory(shared_from);
	UnuseMemory(pg);
}


/* pgDispose -- Disposes a pg_ref (deallocates everything in it).  */

PG_PASCAL (void) pgDispose (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	pg_globals_ptr		globals;
	pgm_globals_ptr		mem_globals;
	memory_ref			old_sublist;
	long				memory_id, cache_index;
	short				i;
	
	globals = pgGetGlobals(pg);
	globals->pg_extend((void PG_FAR *) pg, pg_dispose);
	mem_globals = globals->mem_globals;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((cache_index = pgInCacheList(mem_globals, pg) > 0)) {
		
		DeleteMemory(mem_globals->freemem_info, cache_index - 1, 1);
		pg_rec->cache_file = MEM_NULL;
	}

	if (old_sublist = pgGetSubrefState(pg_rec, FALSE, TRUE))
		DisposeMemory(old_sublist);
		
	//Dispose of headers and footers
	for (i = 0; i < 4; i++) {
		pgFailureDispose(pg_rec->headers[i]);
		pgFailureDispose(pg_rec->footers[i]);
	}
	
	pgCallTextHook(pg_rec, NULL, pgdispose_reason, 0, pg_rec->t_length, call_for_delete, 0, 0, 0);

	pgDisposeFrames(pg_rec);

	if (!(pg_rec->shared_flags & SHARED_FORMATS))
		pgWillDeleteFormats(pg_rec, pg_rec->globals, pgdispose_reason, pg_rec->t_formats, pg_rec->par_formats);

	if (pg_rec->merge_save)
		pgDispose(pg_rec->merge_save);
	if (pg_rec->applied_range)
		DisposeMemory(pg_rec->applied_range);
	if (pg_rec->list_columns)
		DisposeMemory(pg_rec->list_columns);

	pgDisposeAllSubRefs(pg_rec->t_blocks);

	if (!(pg_rec->flags & NO_DEVICE_BIT) && !(pg_rec->shared_flags & SHARED_GRAF_DEVICE))
		pgCloseDevice(pg_rec->globals, &pg_rec->port);

	memory_id = pg_rec->mem_id;
	UnuseMemory(pg);

	DisposeAllMemory(mem_globals, memory_id);

#ifdef PG_OITC
	if ((memory_id = pgFindInList(globals->pg_list, NULL, pgListCompareLong, &pg)) != NOT_IN_LIST)
		pgRemoveFromList(globals->pg_list, memory_id, NULL);
#endif
}


/* pgFailureDispose is identical to pgDispose except it is called when an
exception handler causes a function to abort that might have created only a
partial pg_ref and/or in debug mode, allocations in a "used" state.  */

PG_PASCAL (void) pgFailureDispose (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	pg_globals_ptr		globals;
	pgm_globals_ptr		mem_globals;
	long				memory_id;
	short				i;

	if (!pg)
		return;

	if (globals = pgGetGlobals(pg))
		globals->pg_extend((void PG_FAR*) pg, pg_dispose);

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	//Dispose of headers and footers
	for (i = 0; i < 4; i++) {
		pgFailureDispose(pg_rec->headers[i]);
		pgFailureDispose(pg_rec->footers[i]);
	}
	
	if (pg_rec->t_blocks)
		pgCallTextHook(pg_rec, NULL, pgdispose_reason, 0, pg_rec->t_length, call_for_delete, 0, 0, 0);
	
	if (pg_rec->t_formats && pg_rec->t_style_run && pg_rec->par_formats
			&& pg_rec->par_style_run)
		pgWillDeleteFormats(pg_rec, pg_rec->globals, pgdispose_reason, pg_rec->t_formats, pg_rec->par_formats);
	
	if (pg_rec->merge_save)
		pgFailureDispose(pg_rec->merge_save);
	
	if (pg_rec->applied_range)
		DisposeFailedMemory(pg_rec->applied_range);

	if (!(pg_rec->flags & NO_DEVICE_BIT))
		pgCloseDevice(pg_rec->globals, &pg_rec->port);

	memory_id = pg_rec->mem_id;
	mem_globals = pg_rec->globals->mem_globals;
	UnuseMemory(pg);
	
	DisposeAllFailedMemory(mem_globals, memory_id);

#ifdef PG_OITC
	if ((memory_id = pgFindInList(globals->pg_list, NULL, pgListCompareLong, &pg)) != NOT_IN_LIST)
		pgRemoveFromList(globals->pg_list, memory_id, NULL);
#endif
}


/* pgNewShell creates a new pgRef but with no shapes, using only defaults for
everything else.  The usual purpose is to create the minimum pgRef to perform
a file "open" or some similar feature.  */

PG_PASCAL (pg_ref) pgNewShell (const pg_globals_ptr globals)
{
	return	pgNew(globals, (generic_var)USE_NO_DEVICE, (shape_ref)MEM_NULL, (shape_ref)MEM_NULL, (shape_ref)MEM_NULL, 0);
}


/* pgDuplicate returns a new empty pgRef based on the attributes in pg. The
style and font are set to the defaults.  */

PG_PASCAL (pg_ref) pgDuplicate (pg_ref pg)
{
	paige_rec_ptr					pg_rec, target_rec;
	volatile pg_ref					result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);

	PG_TRY(pg_rec->globals->mem_globals) {

		result = pgNew(pg_rec->globals, (generic_var)USE_NO_DEVICE, pg_rec->vis_area, pg_rec->wrap_area, pg_rec->exclude_area,
				pg_rec->flags);
	}
	PG_CATCH
	{
		UnuseMemory(pg);
		PG_RERAISE();
	}
	PG_ENDTRY

	target_rec = (paige_rec_ptr) UseMemory(result);
	target_rec->doc_info = pg_rec->doc_info;
	target_rec->flags2 = pg_rec->flags2;

	UnuseMemory(pg);
	UnuseMemory(result);

	return	result;
}


/* pgSetDefaultStyle sets the default style of pg to def_style. */

PG_PASCAL (void) pgSetDefaultStyle (pg_ref pg, const style_info_ptr def_style, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	style_info_ptr			style;
	size_t					real_used_ctr;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, pg_rec->def_style_index, 0, TRUE);
	real_used_ctr = style->used_ctr;
	pgBlockMove(def_style, style, sizeof(style_info));
	style->used_ctr = real_used_ctr;
	UnuseMemory(pg_rec->t_formats);
	pgInvalSelect(pg, 0, pg_rec->t_length);

	if (draw_mode)
		pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, draw_mode, TRUE);

	UnuseMemory(pg);
}


/* pgGetDefaultStyle returns the default style for pg. */

PG_PASCAL (pg_short_t) pgGetDefaultStyle (pg_ref pg, style_info_ptr def_style)
{
	paige_rec_ptr			pg_rec;
	pg_short_t				index;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	index = pg_rec->def_style_index;
	GetMemoryRecord(pg_rec->t_formats, index, def_style);
	UnuseMemory(pg);
	
	return	index;
}


/* pgSetDefaultPar sets the default para style of pg to def_par. */

PG_PASCAL (void) pgSetDefaultPar (pg_ref pg, const par_info_ptr def_par, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	par_info_ptr			par;
	long					real_used_ctr;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	par = (par_info_ptr) UseMemory(pg_rec->par_formats);
	real_used_ctr = par->used_ctr;
	pgBlockMove(def_par, par, sizeof(par_info));
	par->used_ctr = real_used_ctr;
	UnuseMemory(pg_rec->par_formats);
	pgInvalSelect(pg, 0, pg_rec->t_length);

	if (draw_mode)
		pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, draw_mode, TRUE);

	UnuseMemory(pg);
}

/* pgSetDefaultFont sets the default font of pg to def_font. */

PG_PASCAL (void) pgSetDefaultFont (pg_ref pg, const font_info_ptr def_font, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	font_info_ptr			font;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	font = (font_info_ptr) UseMemory(pg_rec->fonts);
	pgBlockMove(def_font, font, sizeof(font_info));
	UnuseMemory(pg_rec->fonts);
	pgInvalSelect(pg, 0, pg_rec->t_length);

	if (draw_mode)
		pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, draw_mode, TRUE);

	UnuseMemory(pg);
}


/* pgTextSize returns the total bytesize of text */

PG_PASCAL (long) pgTextSize (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	long				t_size;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	t_size = pg_rec->t_length;
	UnuseMemory(pg);
	
	return	t_size;
}


/* pgGetAttributes returns the current flag settings in a paige object */

PG_PASCAL (long) pgGetAttributes (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	long				result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->flags;
	UnuseMemory(pg);
	
	return	result;
}


/* pgGetAttributes2 returns the current flags2 settings in a paige object */

PG_PASCAL (long) pgGetAttributes2 (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	long				result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->flags2;
	UnuseMemory(pg);
	
	return	result;
}



/* pgSetAttributes changes the attribute settings in a paige object. If
necessary the text is invalidated.  */

PG_PASCAL (pg_boolean) pgSetAttributes (pg_ref pg, long attributes)
{
	paige_rec_ptr		pg_rec;
	long				old_attributes, hide_text_change;
	pg_boolean			result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	old_attributes = pg_rec->flags;
	pg_rec->flags = attributes;
	
	if (attributes & LINE_EDITOR_BIT)
		pg_rec->flags |= (NO_WRAP_BIT | NO_LF_BIT);
	
	if (attributes & EXTERNAL_SCROLL_BIT)
		pg_rec->scroll_pos.h = pg_rec->scroll_pos.v = 0;

	hide_text_change = ((old_attributes & NO_HIDDEN_TEXT_BIT) != (pg_rec->flags & NO_HIDDEN_TEXT_BIT));

	if (((old_attributes & NO_WRAP_BIT) != (pg_rec->flags & NO_WRAP_BIT))
		|| hide_text_change) {
	
		pgInvalSelect(pg, 0, pg_rec->t_length);
		++pg_rec->change_ctr;
		
		if (hide_text_change)
			pgInvalCharLocs(pg_rec);

		if (hide_text_change && (pg_rec->flags & NO_HIDDEN_TEXT_BIT)) {
			pg_short_t			select_ctr;
			t_select_ptr		selections;
			style_info_ptr		select_style;
			pg_boolean			select_changed;
			size_t				offset_for_style;
			
			if (!(select_ctr = pg_rec->num_selects * 2))
				++select_ctr;

			selections = (t_select_ptr) UseMemory(pg_rec->select);
			select_changed = FALSE;

			while (select_ctr) {
				
				offset_for_style = selections->offset;
				
				if (offset_for_style && (!pg_rec->num_selects))
					--offset_for_style;
				
				select_style = pgFindTextStyle(pg_rec, offset_for_style);
				
				if (select_style->styles[hidden_text_var])
					select_changed |= pgAlignSelection(pg_rec, align_char_best, &selections->offset);

				UnuseMemory(pg_rec->t_formats);
				
				++selections;
				--select_ctr;
			}

			UnuseMemory(pg_rec->select);
			
			if (pg_rec->num_selects) {
				pg_short_t			rec_to_delete, delete_qty;
				
				while (delete_qty = pgFindEmptyHilite(pg_rec, &rec_to_delete))
					DeleteMemory(pg_rec->select, rec_to_delete, delete_qty);
			}

			if (!pg_rec->num_selects)
				pgSetNextInsertIndex(pg_rec);
		}
	}

	result = (old_attributes != pg_rec->flags);

	UnuseMemory(pg);
	
	return	result;
}


/* pgSetAttributes2 changes the attribute settings, flags2, in a paige object. */

PG_PASCAL (pg_boolean) pgSetAttributes2 (pg_ref pg, long attributes2)
{
	paige_rec_ptr		pg_rec;
	long				old_attributes;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	old_attributes = pg_rec->flags2;
	pg_rec->flags2 = attributes2;
	UnuseMemory(pg);
	
	return	(old_attributes != pg_rec->flags2);
}


/* pgGetDocInfo returns the current doc_info from pg */

PG_PASCAL (void) pgGetDocInfo (pg_ref pg, pg_doc_ptr doc_info)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgBlockMove(&pg_rec->doc_info, doc_info, sizeof(pg_doc_info));
	UnuseMemory(pg);
}


/* pgSetDocInfo changes the document info in pg, re-drawing/re-calcing if
necessary. */

PG_PASCAL (void) pgSetDocInfo (pg_ref pg, const pg_doc_ptr doc_info,
		pg_boolean inval_text, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	pg_boolean			page_info_changed;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	page_info_changed = ( (doc_info->attributes != pg_rec->doc_info.attributes)
		|| (doc_info->repeat_offset.h != pg_rec->doc_info.repeat_offset.h)
		|| (doc_info->repeat_offset.v != pg_rec->doc_info.repeat_offset.v)
		|| (doc_info->minimum_orphan != pg_rec->doc_info.minimum_orphan)
		|| (doc_info->minimum_widow != pg_rec->doc_info.minimum_widow));
	
	if (page_info_changed)
		pg_rec->port.clip_info.change_flags |= CLIP_PAGE_CHANGED;

	if (inval_text) {

		pgInvalSelect(pg, 0, pg_rec->t_length);

		if (page_info_changed)
			pg_rec->flags |= DOC_BOUNDS_DIRTY;
	}

	pgBlockMove(doc_info, &pg_rec->doc_info, sizeof(pg_doc_info));

	if (!(pg_rec->doc_info.attributes & BOTTOM_FIXED_BIT))
		pg_rec->doc_info.attributes |= NO_CLIP_PAGE_AREA;

	if (draw_mode)  {
		short	use_draw_mode;

		if ((use_draw_mode = draw_mode) == best_way)
				use_draw_mode = bits_copy;

		pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL,
			use_draw_mode, TRUE);
	}

	UnuseMemory(pg);
}


/* pgGetChangeCtr returns the change counter in pg */

PG_PASCAL (long) pgGetChangeCtr (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	long				result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->change_ctr;
	UnuseMemory(pg);
	
	return	result;
}


/* pgSetChangeCtr sets the change counter in pg */

PG_PASCAL (void) pgSetChangeCtr (pg_ref pg, long ctr)
{
	paige_rec_ptr		pg_rec;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pg_rec->change_ctr = ctr;
	UnuseMemory(pg);
}


/* pgSetDefaultDevice sets the default graphics device */

PG_PASCAL (void) pgSetDefaultDevice (pg_ref pg, const graf_device_ptr device)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (!(pg_rec->flags & NO_DEVICE_BIT))
		pgCloseDevice(pg_rec->globals, &pg_rec->port);
	
	pgBlockMove(device, &pg_rec->port, sizeof(graf_device));
	pg_rec->flags &= (~NO_DEVICE_BIT);
	pg_rec->scale_factor.scale = pg_rec->real_scaling;
	pgScaleGrafDevice(pg_rec);
	pg_rec->port.clip_info.change_flags |= CLIP_DEVICE_CHANGED;

	UnuseMemory(pg);
}

/* pgGetDefaultDevice returns the default graphics device.  */

PG_PASCAL (void) pgGetDefaultDevice (pg_ref pg, graf_device_ptr device)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgBlockMove(&pg_rec->port, device, sizeof(graf_device));
	UnuseMemory(pg);
}


/* pgInitSameDevice is identical to pgInitDevice except the same drawing port
is used from pg.  (This in effect "copies" the port in pg).  */

PG_PASCAL (void) pgInitSameDevice (pg_ref pg, const graf_device_ptr device)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgInitDevice(pg_rec->globals, pg_rec->port.machine_var, pg_rec->port.machine_ref, device);

	UnuseMemory(pg);
}


/* pgSetDevicePalette sets the machine-specific device palette for the pg_ref) */

PG_PASCAL (void) pgSetDevicePalette (pg_ref pg, const generic_var palette)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pg_rec->port.palette = palette;
	UnuseMemory(pg);
}

/* pgGetDevicePalette returns the palette device used by pg */

PG_PASCAL (generic_var) pgGetDevicePalette (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	generic_var			palette;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	palette = pg_rec->port.palette;
	UnuseMemory(pg);
	
	return	palette;
}


/* pgSetHooks replaces function pointers in pg with *hooks.  If inval_text is
TRUE the text is invalidated (so it re-calcs).  */

PG_PASCAL (void) pgSetHooks (pg_ref pg, const pg_hooks PG_FAR *hooks, pg_boolean inval_text)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgBlockMove(hooks, &pg_rec->procs, sizeof(pg_hooks));
	
	if (inval_text)
		pgInvalSelect(pg, 0, pg_rec->t_length);

	UnuseMemory(pg);
}


/* pgGetHooks returns the current function pointers in pg.  This is essential
before changing some of them and calling pgSetHooks.  */

PG_PASCAL (void) pgGetHooks (pg_ref pg, pg_hooks PG_FAR *hooks)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgBlockMove(&pg_rec->procs, hooks, sizeof(pg_hooks));

	UnuseMemory(pg);
}


/* pgSetOrigin changes pg's drawing origin (for all text and shapes). */

PG_PASCAL (void) pgSetOrigin (pg_ref pg, const co_ordinate_ptr origin)
{
	paige_rec_ptr		pg_rec;
	co_ordinate			old_origin;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	old_origin = pg_rec->port.origin;
	pg_rec->port.origin = *origin;
	
	UnuseMemory(pg);
}


/* pgGetOrigin returns pg's drawing origin (for all text and shapes). */

PG_PASCAL (void) pgGetOrigin (pg_ref pg, co_ordinate_ptr origin)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	*origin = pg_rec->port.origin;
	UnuseMemory(pg);
}


/* pgGetGlobals returns the globals pointer taken from a pg_ref. Typical use is
for situations where the app wants the globals but doesn't have access to it,
only a pg_ref is around.  */

PG_PASCAL (pg_globals_ptr) pgGetGlobals (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	pg_globals_ptr		result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->globals;
	UnuseMemory(pg);
	
	return	result;
}


/* pgSetArea sets one or more areas by copying the contents of the non-NULL
shapes.  */

PG_PASCAL (void) pgSetAreas (pg_ref pg, shape_ref vis_area, shape_ref page_area,
		shape_ref exclude_area)
{
	paige_rec_ptr			pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (vis_area) {
		rectangle_ptr		vis_ptr;

		MemoryCopy(vis_area, pg_rec->vis_area);
		vis_ptr = (rectangle_ptr) UseMemory(vis_area);
		pg_rec->base_vis_origin = vis_ptr->top_left;
		UnuseMemory(vis_area);
		
		pg_rec->port.clip_info.change_flags |= CLIP_VIS_CHANGED;
	}

	if (page_area) {
		pg_rec->port.clip_info.change_flags |= CLIP_PAGE_CHANGED;
		MemoryCopy(page_area, pg_rec->wrap_area);
	}
	if (exclude_area) {

		pg_rec->port.clip_info.change_flags |= CLIP_EXCLUDE_CHANGED;
		MemoryCopy(exclude_area, pg_rec->exclude_area);
	}
	
	if (page_area || exclude_area) {
		
		pgResetDocBounds(pg_rec);

		pgInvalSelect(pg, 0, pg_rec->t_length);
		++pg_rec->change_ctr;
	}
	
	UnuseMemory(pg);
}


/* pgGetAreas fills in each non-NULL shape with the respective areas.  */

PG_PASCAL (void) pgGetAreas (pg_ref pg, shape_ref vis_area, shape_ref page_area,
		shape_ref exclude_area)
{
	paige_rec_ptr			pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (vis_area)
		MemoryCopy(pg_rec->vis_area, vis_area);
	if (page_area)
		MemoryCopy(pg_rec->wrap_area, page_area);
	if (exclude_area)
		MemoryCopy(pg_rec->exclude_area, exclude_area);

	UnuseMemory(pg);
}


/* pgGetPageArea returns the actual wrap area ref */

PG_PASCAL (shape_ref) pgGetPageArea (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	shape_ref			result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->wrap_area;
	UnuseMemory(pg);
	
	return	result;
}

/* pgGetVisArea returns the actual vis area ref */

PG_PASCAL (shape_ref) pgGetVisArea (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	shape_ref			result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->vis_area;
	UnuseMemory(pg);
	
	return	result;
}


/* pgGetExcludeArea returns the actual exclude area ref */

PG_PASCAL (shape_ref) pgGetExcludeArea (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	shape_ref			result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->exclude_area;
	UnuseMemory(pg);

	return	result;
}



/* pgGrowVisArea enlarges the vis area by top_left amounts and/or
bot_right amounts. Either parameter can be null.  */

PG_PASCAL (void) pgGrowVisArea (pg_ref pg, const co_ordinate_ptr top_left, const co_ordinate_ptr bot_right)
{
	paige_rec_ptr			pg_rec;
	register rectangle_ptr	vis_ptr;
	register pg_short_t		num_rects;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	for (vis_ptr = (rectangle_ptr) UseMemory(pg_rec->vis_area), num_rects = (pg_short_t)GetMemorySize(pg_rec->vis_area);
			num_rects;  ++vis_ptr, --num_rects) {
		
		if (top_left)
			pgAddPt(top_left, &vis_ptr->top_left);
		
		if (bot_right)
			pgAddPt(bot_right, &vis_ptr->bot_right);
	}
	
	UnuseMemory(pg_rec->vis_area);

	pg_rec->port.clip_info.change_flags |= CLIP_VIS_CHANGED;

	UnuseMemory(pg);
}


/* pgOffsetAreas moves some or all of the pg shapes by h and v amounts. Each
shape moves if the parameter is TRUE. */

PG_PASCAL (void) pgOffsetAreas (pg_ref pg, long h, long v, pg_boolean offset_page,
		pg_boolean offset_vis, pg_boolean offset_exclude)
{
	paige_rec_ptr				pg_rec;
	register text_block_ptr		block;
	register point_start_ptr	starts;
	register size_t				num_blocks, num_lines;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (h || v) {
	
		if (offset_page) {
			
			pg_rec->port.clip_info.change_flags |= CLIP_PAGE_CHANGED;
			pgOffsetShape(pg_rec->wrap_area, h, v);
			pg_rec->doc_bounds.bot_right.v += v;
		}
		if (offset_vis) {
			
			pg_rec->port.clip_info.change_flags |= CLIP_VIS_CHANGED;
			pgOffsetShape(pg_rec->vis_area, h, v);
		}
		if (offset_exclude) {
			
			pg_rec->port.clip_info.change_flags |= CLIP_EXCLUDE_CHANGED;
			pgOffsetShape(pg_rec->exclude_area, h, v);
		}
		
		pgResetDocBounds(pg_rec);
	
		if ((offset_exclude || pgEmptyShape(pg_rec->exclude_area)) && offset_page) {
			
			block = (text_block_ptr) UseMemory(pg_rec->t_blocks);
			
			for (num_blocks = GetMemorySize(pg_rec->t_blocks); num_blocks;
					++block, --num_blocks) {
				
				pgOffsetRect(&block->bounds, h, v);
				
				if (block->flags & SOME_LINES_GOOD && !(block->flags & LINES_PURGED)) {
					
					starts = (point_start_ptr) UseMemory(block->lines);
					
					for (num_lines = GetMemorySize(block->lines); num_lines;
							++starts, --num_lines)
						pgOffsetRect(&starts->bounds, h, v);
					
					UnuseMemory(block->lines);
				}
				else
				if (!(block->flags & LINES_PURGED))
					block->flags |= NEEDS_CALC;

				pgOffsetRect(&block->end_start.bounds, h, v);
			}
			
			UnuseMemory(pg_rec->t_blocks);
		}
		else
			pgInvalSelect(pg, 0, pg_rec->t_length);
	}

	UnuseMemory(pg);
}



/* pgErasePageArea fills the page_area with the current background color
(defined in pg) if non-white, or pg port's background if white.
The appropriate port is set and clipped if vis_area is NULL, or the clip is
set to vis_area if non-NULL.  */

PG_PASCAL (void) pgErasePageArea (pg_ref pg, shape_ref vis_area)
{
	paige_rec_ptr		pg_rec;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgDrawPages(pg_rec, NULL, NULL, vis_area, sp_erase_all_pages);
	UnuseMemory(pg);
}


/* pgSetExtraStruct stores an arbitrary long or pointer into a paige object.
The ref_id is an index into the extra struct buffer. If necessary, the extra
stuff buffer will get extended if the ID is out of range.   */

PG_PASCAL (void) pgSetExtraStruct (pg_ref pg, const void PG_FAR *extra_struct,
		long ref_id)
{
	paige_rec_ptr		pg_rec;
	size_t PG_FAR		*extra_ptr;
	size_t				current_size;
	long				id;

	if ((id = ref_id + EXTRA_STRUCT_RSRV) < 0)
		return;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	pgInitExtraStruct(pg_rec);

	current_size = GetMemorySize(pg_rec->extra_stuff);
	
	if (id >= current_size)
		SetMemorySize(pg_rec->extra_stuff, id + 1);

	extra_ptr = (size_t PG_FAR*) UseMemoryRecord(pg_rec->extra_stuff, id, 0, TRUE);
	*extra_ptr = (size_t) extra_struct;
	
	UnuseMemory(pg_rec->extra_stuff);
	UnuseMemory(pg);
}


PG_PASCAL (void PG_FAR *) pgGetExtraStruct (pg_ref pg, long ref_id)
{
	paige_rec_ptr		pg_rec;
	void PG_FAR			*result;
	size_t PG_FAR		*extra_ptr;
	size_t				current_size;
	long				id;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = NULL;

	if (((id = ref_id + EXTRA_STRUCT_RSRV) >= 0) && pg_rec->extra_stuff) {
	
		current_size = GetMemorySize(pg_rec->extra_stuff);
		
		if (id < current_size) {
			
			extra_ptr = (size_t PG_FAR*) UseMemory(pg_rec->extra_stuff);
			result = (void PG_FAR *) extra_ptr[id];
			UnuseMemory(pg_rec->extra_stuff);
		}
	}

	UnuseMemory(pg);
	
	return	result;
}


/* pgInitExtraStruct is a low-level utility that initializes extra_struct. */

PG_PASCAL (void) pgInitExtraStruct (paige_rec_ptr pg)
{
	if (!pg->extra_stuff) {
		
		pgPushMemoryID(pg);

		pg->extra_stuff = MemoryAllocClear(pg->globals->mem_globals,
				sizeof(size_t), EXTRA_STRUCT_RSRV, 4);
		
		pgPopMemoryID(pg);
	}
}


/* pgExtraUniqueID returns a "unique" ID, which is really the first NULL entry
it finds in the extra_struct list, or if none found the appended record. */

PG_PASCAL (long) pgExtraUniqueID (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	long PG_FAR			*extra_ptr;
	size_t				extra_size;
	long				next_id;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	next_id = EXTRA_STRUCT_RSRV;

	if (pg_rec->extra_stuff) {
		
		extra_ptr = (long PG_FAR*) UseMemory(pg_rec->extra_stuff);
		extra_ptr += EXTRA_STRUCT_RSRV;
		extra_size = GetMemorySize(pg_rec->extra_stuff) - EXTRA_STRUCT_RSRV;

		while (extra_size) {
			
			if (!(*extra_ptr))
				break;
			
			++extra_ptr;
			++next_id;
			--extra_size;
		}
		
		UnuseMemory(pg_rec->extra_stuff);
	}

	UnuseMemory(pg);
	
	return	next_id;
}


/* pgSetScaling sets the scale_factor, re-drawing if necessary */

PG_PASCAL (void) pgSetScaling (pg_ref pg, const pg_scale_ptr scale_factor, short draw_mode)
{
	paige_rec_ptr			pg_rec;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgInvalidateHilite(pg_rec, (pg_boolean)(draw_mode != draw_none));
	pg_rec->port.clip_info.change_flags |= CLIP_DEVICE_CHANGED;
	pg_rec->scale_factor = *scale_factor;
	pg_rec->real_scaling = scale_factor->scale;
	
	if (pg_rec->scale_factor.scale) {
		long		scale_low, scale_hi;
		
		scale_low = pg_rec->scale_factor.scale & 0xFFFF;
		scale_hi = pg_rec->scale_factor.scale >> 16;
		
		if (scale_low == scale_hi)
			pg_rec->scale_factor.scale = 0;
	}
    
	pgScaleGrafDevice(pg_rec);
	
	if (draw_mode)
		pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL,
			draw_mode, TRUE);

	UnuseMemory(pg);
}


/* pgGetScaling returns the current scale_factor */

PG_PASCAL (void) pgGetScaling (pg_ref pg, pg_scale_ptr scale_factor)
{
	paige_rec_ptr			pg_rec;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	*scale_factor = pg_rec->scale_factor;
	scale_factor->scale = pg_rec->real_scaling;
	UnuseMemory(pg);
}


/* Returns document height   */

PG_PASCAL (long) pgTotalTextHeight (pg_ref pg, short paginate)
{
	paige_rec_ptr			pg_rec;
	memory_ref				subref_state;
	long					height;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	subref_state = pgGetSubrefState(pg_rec, FALSE, TRUE);

	if (paginate)
		pgComputeDocHeight(pg_rec, TRUE);

	height = pg_rec->doc_bounds.bot_right.v - pg_rec->doc_bounds.top_left.v;
	
	if (subref_state)
		pgRestoreSubRefs(pg_rec, subref_state);

	UnuseMemory(pg);
	
	return	height;
}


/* pgInCacheList locates pg within the cache list, if any, and returns the index + 1 if found. */

PG_PASCAL (long) pgInCacheList (pgm_globals_ptr mem_globals, pg_ref pg)
{
	pg_ref  PG_FAR			*refs;
	size_t					num_refs;
	long					index, result;
	
	if (!mem_globals->freemem_info)
		return	0;
	
	num_refs = GetMemorySize(mem_globals->freemem_info);
	refs = (pg_ref PG_FAR*) UseMemory(mem_globals->freemem_info);
	
	for (index = result = 0; index < num_refs; ++index)
		if (refs[index] == pg) {
			
			result = index + 1;
			break;
		}

	UnuseMemory(mem_globals->freemem_info);
	
	return		result;
}


/* pgSetImportBase establishes the import directory. */

PG_PASCAL (void) pgSetImportBase (pg_ref pg, memory_ref base)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (pg_rec->import_base)
		if (pg_rec->import_base != base)
			DisposeMemory(pg_rec->import_base);

	pg_rec->import_base = base;
	UnuseMemory(pg);
}


/* pgGetImportBase returns the current import directory. */

PG_PASCAL (memory_ref) pgGetImportBase (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	memory_ref			result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->import_base;
	UnuseMemory(pg);
	
	return	result;
}

/* pgSetExportBase establishes the export directory. */

PG_PASCAL (void) pgSetExportBase (pg_ref pg, memory_ref base)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (pg_rec->export_base)
		if (pg_rec->export_base != base)
			DisposeMemory(pg_rec->export_base);

	pg_rec->export_base = base;
	UnuseMemory(pg);
}

/* pgGetExportBase returns the current export directory. */

PG_PASCAL (memory_ref) pgGetExportBase (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	memory_ref			result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->export_base;
	UnuseMemory(pg);
	
	return	result;
}



/******************************* Local Functions **************************/


/* This gets called a few times by pgNew: if shape_param is NULL a new, empty
shape is returned, otherwise a copy is returned.  */

static shape_ref copy_shape_param (pg_globals_ptr globals, shape_ref shape_param)
{
	if (shape_param)
		return	MemoryDuplicate(shape_param);
	
	return	pgRectToShape(globals->mem_globals, NULL);
}


/* free_cached_blocks walks through pg and locates which text blocks, if
any, can be unloaded. This is for the 2.0 disk paging (cache) system. The
ref_mask memory_ref cannot be freed, however. If we unload >= needed_free,
we exit.  */

static size_t free_cached_blocks (paige_rec_ptr pg, memory_ref ref_mask, size_t needed_free)
{
	rectangle				vis_bounds;
	text_block_ptr			block;
	size_t					num_blocks;
	size_t					freed = 0;

	pgShapeBounds(pg->vis_area, &vis_bounds);
	pg->port.scale.scale = -pg->port.scale.scale;
	pgScaleRect(&pg->port.scale, NULL, &vis_bounds);
	pg->port.scale.scale = -pg->port.scale.scale;
	pgOffsetRect(&vis_bounds, pg->scroll_pos.h, pg->scroll_pos.v);
	pgOffsetRect(&vis_bounds, -pg->port.origin.h, -pg->port.origin.v);
	pgInsetRect(&vis_bounds, 32, 32);

	num_blocks = GetMemorySize(pg->t_blocks);
	block = (text_block_ptr) UseMemory(pg->t_blocks);
	
	while (num_blocks) {
		
		if (!(block->cache_flags & CACHE_LOCK_FLAG))
			if (block->text && block->text != ref_mask)
				if (!(block->cache_flags & CACHE_CHANGED_FLAG))
					if (!GetAccessCtr(block->text))
						if (!(GetMemoryPurge(block->text) & PURGED_FLAG)) {
					
						if ( (block->flags & (NEEDS_CALC | NEEDS_PAGINATE))
								|| !pgSectRect(&vis_bounds, &block->bounds, NULL)) {
						
						
							freed += (pgMemorySize(block->text));
	
							DisposeMemory(block->text);
							block->text = MEM_NULL;
						}
		}

		++block;
		--num_blocks;
	}

	UnuseMemory(pg->t_blocks);
	
	return		freed;
}

/* set_max_run sets the maximum size in an empty run. */

void set_max_run (paige_rec_ptr pg_rec, memory_ref the_run)
{
	style_run_ptr		run;

	run = (style_run_ptr) UseMemory(the_run);
	run[1].offset = ZERO_TEXT_PAD;
	UnuseMemory(the_run);
}


/* draw_border_line draws a line for a paragraph border. This code is platform-independent */

static void draw_border_line (paige_rec_ptr pg, co_ordinate_ptr begin_pt,
		co_ordinate_ptr end_pt, long source_border_info, long border_info,
		long pen_size, pg_short_t cell_info, long border_color, long cell_bk_color, short verb)
{
	co_ordinate		begin, end, adjust_begin, adjust_end;
	long			pensize, color, grayscale_light, grayscale_medium, grayscale_dark;
	short			ctr;

	begin = *begin_pt;
	end = *end_pt;
	
	color = grayscale_light = grayscale_medium = grayscale_dark = 0;

	if (border_info & (PG_BORDER_3D | PG_BORDER_RULE)) {
		color_value			bk_color;
		
		if (cell_bk_color)
			pgLongToRGB(cell_bk_color, &bk_color);
		else
			bk_color = pg->bk_color;

		grayscale_light = pgGetGrayScale(&bk_color, BORDER_LIGHT_SCALE);
		grayscale_medium = pgGetGrayScale(&bk_color, BORDER_MEDIUM_SCALE);
		grayscale_dark = pgGetGrayScale(&bk_color, BORDER_DARK_SCALE);
	}

	if ((pensize = pen_size) == 0)
		pensize = border_info & PG_BORDER_PENSIZE;

	//if (verb == drawing_top || verb == drawing_bottom)
	//	end.h -= pensize;
	//if (verb == drawing_left || verb == drawing_right)
	//	end.v -= pensize;

	if (border_info & PG_BORDER_DOUBLE)
		ctr = 2;
	else
		ctr = 1;
	
	if (border_info & PG_BORDER_RULE) {
		
		if (verb != drawing_bottom)
			return;			// Line rule not legal unless top or bottom

		color = grayscale_medium;
		pensize = pen_size / 2;
		
		if (!pensize)
			pensize = 1;
	}
	else
	if (border_info & PG_BORDER_3D) {
		long		middle_width;
		
		if (pensize < 3)
			color = 0;
		else {

			middle_width = pensize - 2;

// Special-case draw
			
			switch (verb) {
				
				case drawing_left:
					end.v -= 1;

					if (!(cell_info & CELL_LEFT_BIT) && (cell_info & CELL_LAST_BIT))
						end.v -= middle_width;
					
					pgLineDraw(pg->globals->current_port, &begin, &end, grayscale_light, 1);
					
					end.v = end_pt->v - 1;
					
					if (cell_info & CELL_BOT_BIT)
						end.v -= (middle_width + 1);
					
					if (cell_info & CELL_FIRST_BIT)
						begin.v += middle_width;
					else
					if (!(cell_info & CELL_TOP_BIT))
						begin.v -= 1;

					begin.h += middle_width;
					end.h += middle_width;
					pgLineDraw(pg->globals->current_port, &begin, &end, grayscale_dark, 1);
					break;

				case drawing_top:
					end.h -= 1;
					pgLineDraw(pg->globals->current_port, &begin, &end, grayscale_light, 1);
					begin.h += middle_width;
					
					if (cell_info & CELL_RIGHT_BIT)
						end.h -= middle_width;

					begin.v += middle_width;
					end.v += middle_width;
					pgLineDraw(pg->globals->current_port, &begin, &end, grayscale_dark, 1);
					break;
					
				case drawing_right:
					begin.h -= 1;
					end.h -= 1;
					end.v -= 1;
					pgLineDraw(pg->globals->current_port, &begin, &end, grayscale_dark, 1);
					
					begin.h -= middle_width;
					end.h -= middle_width;
					
					begin.v += middle_width;
					end.v -= middle_width;
					
					if (cell_info & CELL_BOT_BIT)
						end.v -= 1;

					pgLineDraw(pg->globals->current_port, &begin, &end, grayscale_light, 1);
					break;

				case drawing_bottom:
					begin.v -= 1;
					end.v -= 1;
					
					if (!(cell_info & CELL_LAST_BIT))
						begin.h += middle_width;

					if (cell_info & CELL_RIGHT_BIT)
						end.h -= 1;
					else
					if (cell_info & CELL_LAST_BIT)
						end.h += middle_width;
					
					pgLineDraw(pg->globals->current_port, &begin, &end, grayscale_dark, 1);
					
					end.h = end_pt->h;
					if (cell_info & CELL_RIGHT_BIT)
						end.h -= 1;
					
					if (!(cell_info & CELL_BOT_BIT))
						begin.h += middle_width;
					
					end.h -= middle_width;
					begin.v -= middle_width;
					end.v -= middle_width;
					pgLineDraw(pg->globals->current_port, &begin, &end, grayscale_light, 1);
					break;
			}

			return;
		}
	}
	else
		color = border_color;
	
	if (border_info & PG_BORDER_DOTTED)
		color |= COLORPAT_DOTTED;

	while (ctr) {
	
		pgLineDraw(pg->globals->current_port, &begin, &end, color, (short)pensize);
		
		if (border_info & PG_BORDER_RULE) {
			
			adjust_begin = begin;
			adjust_end = end;
			adjust_begin.v += pensize;
			adjust_end.v += pensize;
			
			pgLineDraw(pg->globals->current_port, &adjust_begin, &adjust_end, grayscale_light, (short)pensize);
			
			adjust_begin.v = begin.v;
			adjust_end.h = adjust_begin.h;
			pgLineDraw(pg->globals->current_port, &adjust_begin, &adjust_end, grayscale_medium, (short)pensize);
			adjust_end.v -= pensize;
			pgLineDraw(pg->globals->current_port, &adjust_begin, &adjust_end, grayscale_dark, (short)pensize);
		}
		else
		if (border_info & PG_BORDER_SHADOW) {

			if (verb == drawing_right || verb == drawing_bottom) {

				adjust_begin = begin;
				adjust_end = end;

				adjust_begin.h += pensize;
				adjust_end.h += pensize;

				if (verb == drawing_right) {
					
					adjust_begin.v += pensize;
					adjust_end.v += pensize;
				}
				else {
				
					adjust_begin.v += pensize;
					adjust_end.v += pensize;
				}

				pgLineDraw(pg->globals->current_port, &adjust_begin, &adjust_end, color, (short)pensize);
			}
		}
		
		if (ctr == 2) {
			
			if (verb == drawing_left) {
				
				begin.h += (pensize + 1);
				end.h += (pensize + 1);

				if (source_border_info & PG_BORDER_TOP)
					begin.v += (pensize + 1);
				
				if (source_border_info & PG_BORDER_BOTTOM)
					end.v -= (pensize + 1);
			}
			else
			if (verb == drawing_top) {

				begin.v += (pensize + 1);
				end.v += (pensize + 1);

				if (source_border_info & PG_BORDER_LEFT)
					begin.h += (pensize + 1);

				if (source_border_info & PG_BORDER_RIGHT)
					end.h -= (pensize + 1);
			}
			else
			if (verb == drawing_right) {

				begin.h -= (pensize + 1);
				end.h -= (pensize + 1);

				if (source_border_info & PG_BORDER_TOP)
					begin.v += (pensize + 1);

				if (source_border_info & PG_BORDER_BOTTOM)
					end.v -= (pensize + 1);
			}
			else
			if (verb == drawing_bottom) {

				begin.v -= (pensize + 1);
				end.v -= (pensize + 1);

				if (source_border_info & PG_BORDER_LEFT)
					begin.h += (pensize + 1);

				if (source_border_info & PG_BORDER_RIGHT)
					end.h -= (pensize + 1);
			}
		}

		--ctr;
	}
}

