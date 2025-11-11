/* This file handles a high-level "frame" which can be an image, subref, embed, etc.  */

#include "Paige.h"
#include "pgExceps.h"
#include "pgFrame.h"
#include "machine.h"
#include "pgUtils.h"
#include "pgText.h"
#include "pgShapes.h"
#include "pgEdit.h"
#include "pgSelect.h"
#include "pgErrors.h"
#include "pgEmbed.h"
#include "pgGrafx.h"
#include "pgBasics.h"
#include "pgOSUtl.h"
#include "pgDefstl.h"

static void draw_selection_rect (paige_rec_ptr pg, long color, rectangle_ptr target, pg_frame_ptr frame);
static void make_display_rects (paige_rec_ptr pg, pg_frame_ptr frame, rectangle_ptr target,
		rectangle_ptr bounds, rectangle_ptr wrap);
static void align_frame (paige_rec_ptr pg, pg_frame_ptr frame);
static void low_level_update (pg_ref pg, short draw_mode);


/* pgInsertFrame inserts a new frame based on the values in *frame. */

PG_PASCAL(void) pgInsertFrame (pg_ref pg, pg_frame_ptr frame, embed_callback embed_callback, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	pg_frame_ptr		the_frame;
	memory_ref			frame_ref;
	long				attached_to_par;
	short				use_draw_mode;

	pg_rec = UseMemory(pg);
	frame_ref = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(pg_frame), 1, 0);
	the_frame = UseMemory(frame_ref);
	*the_frame = *frame;
	align_frame(pg_rec, the_frame);
	attached_to_par = frame->flags & FRAME_ATTACHED_TO_PAR;

	if (!the_frame->callback)
		the_frame->callback = (long)pgStandardFrameCallback;
	
	if (the_frame->type == frame_embed) {
		style_info			new_style;
		style_info_ptr		appended_style;
		pg_embed_ptr		embed_ptr;
		pg_short_t			style_item;

		embed_ptr = UseMemory((memory_ref)the_frame->data);
		embed_ptr->used_ctr += 1;
		UnuseMemory((memory_ref)the_frame->data);
		
		new_style = pg_rec->globals->def_style;

		new_style.embed_object = (long)the_frame->data;
		
		if ((new_style.embed_entry = (long)embed_callback) == 0)
			new_style.embed_entry = (long)pgDefaultEmbedCallback;
		
		new_style.class_bits |= EMBED_FRAME_BIT;
		style_item = pgAddStyleInfo(pg_rec, NULL, internal_clone_reason, &new_style);
		appended_style = UseMemoryRecord(pg_rec->t_formats, (long)style_item, 0, TRUE);
		appended_style->used_ctr += 1;
		UnuseMemory(pg_rec->t_formats);
	}

	UnuseMemory(frame_ref);
	pg_rec->flags2 |= HAS_PG_FRAMES_BIT;
	
	if (attached_to_par)
		use_draw_mode = draw_none;
	else
		use_draw_mode = draw_mode;

	pgInsertExclusion(pg, &the_frame->wrap, 0, (long)frame_ref, draw_none);
	
	if (attached_to_par) {
		
		the_frame = UseMemory(frame_ref);
		the_frame->position = pgAttachParExclusion(pg, frame->position, 1, draw_none);
		UnuseMemory(frame_ref);
	}
	
	low_level_update(pg, draw_mode);
	UnuseMemory(pg);
}

/* pgStandardFrameCallback is the standard callback proc for frames. */

PG_PASCAL (long) pgStandardFrameCallback (paige_rec_ptr pg, pg_frame_ptr frame,
			rectangle_ptr target, co_ordinate_ptr offset_extra, short verb)
{
	pg_embed_ptr		embed_ptr;
	generic_var			old_rgn;
	rectangle			bounds, border_bounds, erase_bounds, vis_bounds, check_bounds;
	long				fill_color;

	switch (verb) {

		case FRAME_DRAW_VERB:
			make_display_rects(pg, frame, target, &border_bounds, &erase_bounds);
			pgShapeBounds(pg->vis_area, &vis_bounds);
			check_bounds = erase_bounds;
			pgInsetRect(&check_bounds, -8, -8);

			if (!pgSectRect(&check_bounds, &vis_bounds, NULL))
				break;

			if (frame->type == frame_embed) {

				embed_ptr = UseMemory((memory_ref)frame->data);
				
				bounds.top_left.h = bounds.top_left.v = 0;
				bounds.bot_right.h = embed_ptr->width;
				bounds.bot_right.v = embed_ptr->height;
				
				pgOffsetRect(&bounds, border_bounds.top_left.h, border_bounds.top_left.v);

				if (frame->flags & FRAME_NO_BACKGROUND)
					fill_color = pgRGBToLong(&pg->bk_color);
				else
					fill_color = frame->shading;

				old_rgn = (generic_var)pgSetClipRect(&pg->port, &erase_bounds, &vis_bounds);
				pgPaintObject(&pg->port, &erase_bounds, 0, object_rect, fill_color);
				
				pgDefaultEmbedCallback(pg, embed_ptr, embed_ptr->type & EMBED_TYPE_MASK,
							EMBED_DRAW, 0, (long)&bounds, 0);

				if (frame->border_info) {
					
					pgSectRect(&border_bounds, &erase_bounds, &border_bounds);
					pgDrawGeneralBorders(pg, frame->border_info, 0, fill_color, &border_bounds, TRUE);
				}
				
				pgRestoreClipRgn(&pg->port, (generic_var)old_rgn);

				UnuseMemory((memory_ref)frame->data);
				
				if (frame->flags & FRAME_SELECTED)
					draw_selection_rect(pg, 0x00400000, target, frame);
				
				frame->flags &= (~FRAME_WAS_SELECTED);
			}
			break;
		
		case FRAME_DRAW_SELECT:
			if (frame->flags & FRAME_SELECTED)
				draw_selection_rect(pg, 0x00400000, target, frame);
			else
			if (frame->flags & FRAME_WAS_SELECTED)
				pgStandardFrameCallback(pg, frame, target, offset_extra, FRAME_DRAW_VERB);
			break;

		case FRAME_DESTROY_VERB:
			if (frame->type == frame_embed) {
				style_info_ptr		styles;
				long				used_ctr, num_styles;
				
				embed_ptr = UseMemory((memory_ref)frame->data);
				embed_ptr->used_ctr -= 1;
				used_ctr = embed_ptr->used_ctr;
				UnuseMemory((memory_ref)frame->data);
				
				if (used_ctr <= 0)
					pgEmbedDispose((memory_ref)frame->data);
				
				styles = UseMemory(pg->t_formats);
				num_styles = GetMemorySize(pg->t_formats);
				
				while (num_styles) {
					
					if (styles->embed_object == (memory_ref)frame->data) {
						
						if ((styles->used_ctr -= 1) <= 0) {
							
							styles->used_ctr = 0;
							styles->embed_object = 0;
							styles->embed_entry = 0;
							styles->procs = pg->globals->def_style.procs;
						}

						break;
					}
					
					++styles;
					--num_styles;
				}
				
				UnuseMemory(pg->t_formats);
			}
			break;
	}

	return	0;
}

/* pgGetFrame returns a frame record. */

extern PG_PASCAL (void) pgGetFrame (pg_ref pg, pg_short_t frame_num, pg_frame_ptr frame)
{
	paige_rec_ptr	pg_rec;
	rectangle		real_exclusion;
	long			diff_h, diff_v;
	memory_ref		frame_ref;
	
	pg_rec = UseMemory(pg);
	GetMemoryRecord(pg_rec->exclusions, (long)(frame_num - 1), &frame_ref);
	pgBlockMove(UseMemory(frame_ref), frame, sizeof(pg_frame));
	
	GetMemoryRecord(pg_rec->exclude_area, (long)frame_num, &real_exclusion);
	diff_h = real_exclusion.top_left.h - frame->wrap.top_left.h;
	diff_v = real_exclusion.top_left.v - frame->wrap.top_left.v;
	pgOffsetRect(&frame->bounds, diff_h, diff_v);
	pgOffsetRect(&frame->wrap, diff_h, diff_v);

	UnuseMemory(frame_ref);
	UnuseMemory(pg);
}

/* pgSetFrame sets new info from the frame record. */

extern PG_PASCAL (void) pgSetFrame (pg_ref pg, pg_short_t frame_num, pg_frame_ptr frame,
			short draw_mode)
{
	paige_rec_ptr	pg_rec;
	memory_ref		frame_ref;
	pg_frame_ptr	the_frame;
	long			par_offset, old_attach, new_attach;
	long			old_align, new_align;

	pg_rec = UseMemory(pg);

	GetMemoryRecord(pg_rec->exclusions, (long)(frame_num - 1), &frame_ref);
	the_frame = UseMemory(frame_ref);
	old_attach = the_frame->flags & FRAME_ATTACHED_TO_PAR;
	new_attach = frame->flags & FRAME_ATTACHED_TO_PAR;
	old_align = the_frame->flags & FRAME_ANY_ALIGNMENT;
	new_align = frame->flags & FRAME_ANY_ALIGNMENT;

	if (old_align != new_align)
		align_frame(pg_rec, frame);

	pgBlockMove(frame, the_frame, sizeof(pg_frame));
	
	UnuseMemory(frame_ref);
	
// Remove attached par, if any:
	
	if (old_attach != new_attach) {
	
		if ((par_offset = pgGetAttachedPar(pg, frame_num)) >= 0)
			pgAttachParExclusion(pg, par_offset, 0, draw_none);
	}

	if (old_attach != new_attach && new_attach != 0) {
		co_ordinate		scroll;
		
		scroll = pg_rec->scroll_pos;
		pgNegatePt(&scroll);
		
		if ((par_offset = frame->position) == CURRENT_POSITION)
			par_offset = pgPtToChar(pg, &frame->wrap.top_left, &scroll);
		
		the_frame = UseMemory(frame_ref);
		the_frame->position = pgAttachParExclusion(pg, par_offset, frame_num, draw_none);
		UnuseMemory(frame_ref);
	}

	pgReplaceExclusion(pg, &frame->wrap, frame_num, draw_mode);

	UnuseMemory(pg);
}

/* pgDeleteFrames deletes the selected frame(s0. */

PG_PASCAL (void) pgDeleteFrames (pg_ref pg, short draw_mode)
{
	memory_ref		framelist;
	
	if (pgGetSelectedFrames(pg, &framelist) > 0) {
		pg_short_t		PG_FAR *list;
		long			num_frames;
		
		num_frames = GetMemorySize(framelist);
		list = UseMemory(framelist);
		
		while (num_frames) {
			
			pgRemoveExclusion(pg, *list, draw_none);

			++list;
			--num_frames;
		}
		
		UnuseAndDispose(framelist);
		low_level_update(pg, draw_mode);
	}
}


/* pgDisposeFrames disposes all the frame(s) in pg. */

PG_PASCAL (void) pgDisposeFrames (paige_rec_ptr pg)
{
	memory_ref	PG_FAR		*frames;
	pg_frame_ptr			frame;
	frame_callback			callback;
	rectangle_ptr			real_target;
	co_ordinate				scroll;
	long					num_frames;
	
	if (!(pg->flags2 & HAS_PG_FRAMES_BIT))
		return;
	
	num_frames = GetMemorySize(pg->exclusions);
	frames = UseMemory(pg->exclusions);
	real_target = UseMemory(pg->exclude_area);
	scroll = pg->scroll_pos;
	pgNegatePt(&scroll);

	while (num_frames) {
		
		frame = UseMemory(*frames);
		++real_target;
		callback = (frame_callback)frame->callback;
		callback(pg, frame, real_target, &scroll, FRAME_DESTROY_VERB);
		UnuseAndDispose(*frames);

		++frames;
		--num_frames;
	}
	
	UnuseMemory(pg->exclusions);
	UnuseMemory(pg->exclude_area);
}

/* pgSelectFrame selects the frame (shows inverted outline). DOES NOT DE-SELECT
ANY EXISTING FRAMES. */

PG_PASCAL (void) pgSelectFrame (pg_ref pg, pg_short_t frame_num, pg_boolean show_hilite)
{
	paige_rec_ptr			pg_rec;
	
	pg_rec = UseMemory(pg);

	if (pg_rec->flags2 & HAS_PG_FRAMES_BIT) {
		
		if (frame_num > 0 && frame_num <= (pg_short_t)GetMemorySize(pg_rec->exclusions)) {
			memory_ref		frame_ref;
			pg_frame_ptr	frame;
			long			old_flags;

			GetMemoryRecord(pg_rec->exclusions, (long)(frame_num - 1), &frame_ref);
			frame = UseMemory(frame_ref);
			old_flags = frame->flags;
			frame->flags |= FRAME_SELECTED;
			UnuseMemory(frame_ref);
			
			if (show_hilite && !(old_flags & FRAME_SELECTED))
				pgDrawAllFrames(pg_rec, FRAME_DRAW_SELECT);
		}
	}

	UnuseMemory(pg);
}

/* pgDeselectFrames de-selects all selected frames. */

PG_PASCAL (void) pgDeselectFrames (pg_ref pg, pg_boolean draw_hilite)
{
	paige_rec_ptr			pg_rec;
	memory_ref	PG_FAR		*frames;
	pg_frame_ptr			frame;
	pg_boolean				selection_changed = FALSE;
	long					num_frames;
	
	pg_rec = UseMemory(pg);

	if (pg_rec->flags2 & HAS_PG_FRAMES_BIT) {

		num_frames = GetMemorySize(pg_rec->exclusions);
		frames = UseMemory(pg_rec->exclusions);
		
		while (num_frames) {
			
			frame = UseMemory(*frames);
			
			if (frame->flags & FRAME_SELECTED) {
			
				selection_changed = TRUE;
				frame->flags &= (~FRAME_SELECTED);
				frame->flags |= FRAME_WAS_SELECTED;
			}

			UnuseMemory(*frames);

			++frames;
			--num_frames;
		}

		UnuseMemory(pg_rec->exclusions);
		
		if (selection_changed && draw_hilite)
			pgDrawAllFrames(pg_rec, FRAME_DRAW_SELECT);
	}
	
	UnuseMemory(pg);
}


/* pgGetSelectedFrames returns all selected frames (as a memory_ref containing array
of pg_short_t). */

PG_PASCAL (long) pgGetSelectedFrames (pg_ref pg, memory_ref PG_FAR *selectlist)
{
	memory_ref			    result = MEM_NULL;
	paige_rec_ptr			pg_rec;
	memory_ref	PG_FAR		*frames;
	pg_frame_ptr			frame;
	long					num_frames, frame_index, num_selected;
	
	pg_rec = UseMemory(pg);
	num_selected = 0;
	
	if (pg_rec->flags2 & HAS_PG_FRAMES_BIT) {
	
		num_frames = GetMemorySize(pg_rec->exclusions);
		frames = UseMemory(pg_rec->exclusions);
		
		for (frame_index = 1; frame_index <= num_frames; ++frame_index, ++frames) {
			
			frame = UseMemory(*frames);
			
			if (frame->flags & FRAME_SELECTED) {
			
				++num_selected;
				
				if (selectlist) {
				
					pg_short_t PG_FAR		*list;
					
					if (!result)
						result = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(pg_short_t), 0, 4);
					
					list = AppendMemory(result, 1, FALSE);
					*list = (pg_short_t)frame_index;
					UnuseMemory(result);
				}
			}
			
			UnuseMemory(*frames);
		}

		UnuseMemory(pg_rec->exclusions);
	}
	
	if (selectlist)
		*selectlist = result;

	UnuseMemory(pg);

	return		num_selected;
}


/* pgPackFrame sends frame data to the file walker. */

PG_PASCAL (void) pgPackFrame (paige_rec_ptr pg, pack_walk_ptr walker, memory_ref frameref)
{
	pg_frame_ptr			frame;
	long					name_size;
	
	frame = UseMemory(frameref);

	name_size = pgCStrLength(frame->name);
	pgPackNum(walker, short_data, name_size);
	
	if (name_size)
		pgPackBytes (walker, frame->name, name_size);

	pgPackNum(walker, long_data, frame->type);
	pgPackNum(walker, long_data, frame->flags);
	pgPackNum(walker, long_data, frame->border_info);
	pgPackNum(walker, long_data, frame->shading);
	pgPackRect(walker, &frame->bounds);
	pgPackRect(walker, &frame->wrap);
	pgPackNum(walker, long_data, (long)frame->data);
	
	UnuseMemory(frameref);
}

/* pgUnpackFrame unpacks a frame and returns the new frame in a memory_ref. */

PG_PASCAL (memory_ref) pgUnpackFrame (paige_rec_ptr pg, pack_walk_ptr walker)
{
	pg_frame_ptr		frame;
	memory_ref			ref;
	long				name_size;

	ref = MemoryAllocClear(pg->globals->mem_globals, sizeof(pg_frame), 1, 0);
	frame = UseMemory(ref);
	frame->callback = (long)pgStandardFrameCallback;
	
	name_size = pgUnpackNum(walker);
	
	if (name_size)
		pgUnpackPtrBytes(walker, frame->name);

	frame->type = pgUnpackNum(walker);
	frame->flags = pgUnpackNum(walker);
	frame->flags &= (~FRAME_READ_BIT);
	frame->border_info = pgUnpackNum(walker);
	frame->shading = pgUnpackNum(walker);
	pgUnpackRect(walker, &frame->bounds);
	pgUnpackRect(walker, &frame->wrap);
	frame->data = (generic_var)pgUnpackNum(walker); // (The 'real' data is unpacked later
	UnuseMemory(ref);

	return	ref;
}

/* pgDrawAllFrames draws the frames using the specified verb. */

PG_PASCAL (void) pgDrawAllFrames (paige_rec_ptr pg, short verb)
{
	if (pg->flags2 & HAS_PG_FRAMES_BIT) {
		pg_frame_ptr		frame;
		frame_callback		callback;
		long PG_FAR			*frames;
		rectangle_ptr		real_target;
		co_ordinate			scroll_offset;
		long				num_exclusions;
		
		pgSetupGrafDevice(pg, &pg->port, MEM_NULL, clip_with_none_verb);

		num_exclusions = GetMemorySize(pg->exclusions);
		frames = UseMemory(pg->exclusions);
		real_target = UseMemory(pg->exclude_area);
		scroll_offset = pg->scroll_pos;
		pgNegatePt(&scroll_offset);

		while (num_exclusions) {
			
			frame = UseMemory((memory_ref)*frames);
			callback = (frame_callback)frame->callback;
			++real_target;
			callback(pg, frame, real_target, &scroll_offset, verb);
			UnuseMemory((memory_ref)*frames);
			
			++frames;
			--num_exclusions;
		}
		
		UnuseMemory(pg->exclusions);
		UnuseMemory(pg->exclude_area);
		pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);
	}
}


PG_PASCAL (void) pgGetFrameDisplay (pg_ref pg, pg_short_t frame_num, rectangle_ptr bounds,
			rectangle_ptr wrap)
{
	paige_rec_ptr		pg_rec;
	rectangle_ptr		target;
	pg_frame			frame;
	
	pgGetFrame(pg, frame_num, &frame);
	
	pg_rec = UseMemory(pg);
	target = UseMemoryRecord(pg_rec->exclude_area, (long)frame_num, 0, TRUE);
	make_display_rects(pg_rec, &frame, target, bounds, wrap);
	UnuseMemory(pg_rec->exclude_area);
	UnuseMemory(pg);
}


/**************************** Local Functions ***************************/

/* draw_selection_rect draws the frame outline. */

static void draw_selection_rect (paige_rec_ptr pg, long color, rectangle_ptr target, pg_frame_ptr frame)
{
	rectangle			bounds;
	
	make_display_rects(pg, frame, target, NULL, &bounds);
	pgFrameRect(&pg->port, &bounds, color, 2);
}


/* make_display_rects builds the rectangles to be in the correct draw positions. */

static void make_display_rects (paige_rec_ptr pg, pg_frame_ptr frame, rectangle_ptr target,
		rectangle_ptr bounds, rectangle_ptr wrap)
{
	long			offset_h, offset_v;
	
	offset_h = -pg->scroll_pos.h;
	offset_v = -pg->scroll_pos.v;
	
	offset_h += (target->top_left.h - frame->wrap.top_left.h);
	offset_v += (target->top_left.v - frame->wrap.top_left.v);
	
	if (bounds) {
		
		*bounds = frame->bounds;
		pgOffsetRect(bounds, offset_h, offset_v);
	}
	if (wrap) {
		*wrap = frame->wrap;
		pgOffsetRect(wrap, offset_h, offset_v);
	}
}

/* align_frame sets the data alignment per the flags in frame. */

static void align_frame (paige_rec_ptr pg, pg_frame_ptr frame)
{
	rectangle		page_bounds;
	long			distance, new_position, page_width, frame_width;
	
	if (!(frame->flags & FRAME_ANY_ALIGNMENT))
		return;

	pgShapeBounds(pg->wrap_area, &page_bounds);
	
	if (pg->doc_info.attributes & USE_MARGINS_BIT) {
	
		page_bounds.top_left.h += pg->doc_info.margins.top_left.h;
		page_bounds.bot_right.h -= pg->doc_info.margins.bot_right.h;
	}

	page_width = page_bounds.bot_right.h - page_bounds.top_left.h;
	frame_width = frame->wrap.bot_right.h - frame->wrap.top_left.h;
	
	if (frame->flags & FRAME_ALIGN_CENTER)
		distance = (page_width - frame_width) / 2;
	else
	if (frame->flags & FRAME_ALIGN_RIGHT)
		distance = page_width - frame_width;
	else
		distance = 0;
	
	new_position = page_bounds.top_left.h + distance;
	distance = new_position - frame->wrap.top_left.h;
	pgOffsetRect(&frame->wrap, distance, 0);
	pgOffsetRect(&frame->bounds, distance, 0);
}


/* low_level_update performs an update of the doc. */

static void low_level_update (pg_ref pg, short draw_mode)
{
	if (draw_mode) {
		paige_rec_ptr		pg_rec;
		short				use_draw_mode;
		
		if ((use_draw_mode = draw_mode) == best_way)
			use_draw_mode = bits_copy;

		pg_rec = UseMemory(pg);
		pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, use_draw_mode, TRUE);
		pgDrawAllFrames(pg_rec, FRAME_DRAW_VERB);
		UnuseMemory(pg);
	}
}
