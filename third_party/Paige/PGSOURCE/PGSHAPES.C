/* This file handles all the (complex?) "shape" stuff.  No machine-specific
code is in here.  */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgbasic2
#endif

#include "machine.h"
#include "pgShapes.h"
#include "pgDefstl.h"
#include "pgUtils.h"
#include "pgText.h"
#include "pgEdit.h"
#include "pgSelect.h"


struct shape_walk {
	shape_ref			mem_ref;		/* Memory ref of shape */
	pg_short_t			num_rects;		/* Number of rectangles */
	rectangle_ptr		rects;			/* Pointer to shape struct */
	rectangle_ptr		bounds;			/* Pointer to bounds */
};
typedef struct shape_walk shape_walk;
typedef shape_walk PG_FAR *shape_walk_ptr;

typedef enum {
	use_top,			/* Defines for any_touching_rect verb */
	use_left,
	use_bottom,
	use_right
} any_touching_rect_verb;


static void setup_shape (shape_ref the_shape, shape_walk_ptr shape_stuff);
static rectangle_ptr any_partial_rect_match (shape_walk_ptr shape_walker,
			rectangle_ptr rect, pg_short_t PG_FAR *mask_rec);
static short any_secting_rect (rectangle_ptr rect, shape_walk_ptr shape_stuff,
		rectangle_ptr result_rect, pg_short_t PG_FAR *start_rect);
static short any_secting_or_touching_rect (rectangle_ptr rect, shape_walk_ptr shape_stuff,
		rectangle_ptr result_rect, pg_short_t PG_FAR *start_rect);
static rectangle_ptr any_touching_rect (rectangle_ptr rect, shape_walk_ptr shape_stuff,
		pg_short_t PG_FAR *found_rect, short side_to_use);
static short compare_rect_sides (rectangle_ptr src, rectangle_ptr dest,
		short side_to_use);
static void combine_new_rect (shape_walk_ptr shape_stuff, rectangle_ptr new_rect);
static void subtract_all_rects (rectangle_ptr sub_rect, shape_walk_ptr target,
			shape_walk_ptr output);
static pg_short_t rect_still_subtracts (shape_walk_ptr input, shape_walk_ptr output,
		rectangle_ptr parts, short PG_FAR *part_qty);
static rectangle_ptr re_init_walk (shape_walk_ptr walker, short re_use);
static short subtract_rect (rectangle_ptr src, rectangle_ptr target, rectangle_ptr parts);
static void cleanup_shape (shape_walk_ptr shape_to_clean);
static pg_short_t merge_two_rects (shape_walk_ptr merge_walk);
static void dump_select_pair (select_pair_ptr the_pair, memory_ref selections);
static size_t rect_will_nuke (rectangle_ptr src_rect, shape_ref the_shape);
static rectangle_ptr overlapping_exclude (memory_ref exclude_rects,
		rectangle_ptr sect_rect, long minimum_width);
static rectangle_ptr rects_wrong_order (memory_ref rect_list, pg_short_t qty);
static pg_short_t sect_rect_parts (rectangle_ptr src_rect, rectangle_ptr sect_rect,
		rectangle_ptr output_list);
static pg_short_t add_secting_sides (rectangle_ptr src_rect, rectangle_ptr sect_rect,
		rectangle_ptr output_list, pg_short_t r_qty);
static pg_boolean rect_fits_within (rectangle_ptr outside, rectangle_ptr inside);
static pg_short_t rects_sect_internally (shape_walk_ptr walker, rectangle_ptr sect_rect);
static pg_boolean omit_par_exclusion (paige_rec_ptr pg, long line_offset, pg_short_t index);


/* pgRectToShape (defined in Paige.h) returns a new shape composed of a single
rectangle. If the rectangle is NULL, an empty shape is created.   */

PG_PASCAL (shape_ref) pgRectToShape (pgm_globals_ptr mem_globals, const rectangle_ptr rect)
{
	shape_ref			ref;

	ref = MemoryAlloc(mem_globals, sizeof(rectangle), SIMPLE_SHAPE_QTY, 8);
	pgSetShapeRect(ref, rect);
	
	return 	ref;
}


/* pgSetShapeRect changes an existing shape to a specified rectangle (or,
if rect is NULL, the shape is set to an empty shape). */

PG_PASCAL (void) pgSetShapeRect (shape_ref the_shape, const rectangle_ptr rect)
{
	rectangle_ptr		new_shape;
	short				ctr;

	SetMemorySize(the_shape, SIMPLE_SHAPE_QTY);
	new_shape = (rectangle_ptr) UseMemory(the_shape);

	if (!rect)
		pgFillBlock(new_shape, sizeof(rectangle) * 2, 0);
	else
		for (ctr = SIMPLE_SHAPE_QTY; ctr; --ctr, ++new_shape)
			pgBlockMove(rect, new_shape, sizeof(rectangle));

	UnuseMemory(the_shape);
}


/* pgResetBounds walks through a shape a rebuilds the bounds (first) rect. */

PG_PASCAL (void) pgResetBounds (shape_ref the_shape)
{
	register rectangle_ptr		bounds, the_parts;
	register pg_short_t			qty;
	
	if ((qty = (pg_short_t)GetMemorySize(the_shape)) < SIMPLE_SHAPE_QTY) {
		
		SetMemorySize(the_shape, SIMPLE_SHAPE_QTY);
		bounds = (rectangle_ptr) UseMemory(the_shape);
		pgFillBlock(bounds, sizeof(rectangle) * SIMPLE_SHAPE_QTY, 0);
	}
	else {
		
		--qty;

		bounds = the_parts = (rectangle_ptr) UseMemory(the_shape);
		pgFillBlock(bounds, sizeof(rectangle), 0);
	
		while (qty) {
			
			++the_parts;
			pgUnionRect(the_parts, bounds, bounds);
			--qty;
		}
	}

	UnuseMemory(the_shape);
}



/* This function disposes a shape_ref. It should be called instead of the
memory mgr stuff since we might elect to add internal allocations */

PG_PASCAL (void) pgDisposeShape (shape_ref the_shape)
{
	DisposeMemory(the_shape);
}


/* pgEmptyShape returns TRUE if the shape has no dimension.  */

PG_PASCAL (pg_boolean) pgEmptyShape (shape_ref the_shape)
{
	pg_short_t					rect_qty;
	rectangle					bounds;
	
	if ((rect_qty = (pg_short_t)GetMemorySize(the_shape)) < SIMPLE_SHAPE_QTY)
		return	TRUE;
	
	GetMemoryRecord(the_shape, BOUNDS_OFFSET, &bounds);
	
	return pgEmptyRect(&bounds);
}


/* pgEqualShapes returns TRUE if both shapes are completely identical (if both
have equal rectangles).  */

PG_PASCAL (pg_boolean) pgEqualShapes (shape_ref shape1, shape_ref shape2)
{
	pg_boolean		result;
	size_t			qty1, qty2;
	
	qty1 = GetMemorySize(shape1);
	qty2 = GetMemorySize(shape2);
	
	if (qty1 != qty2)
		return	FALSE;
	
	result = pgEqualStruct(UseMemory(shape1), UseMemory(shape2), qty1 * sizeof(rectangle));
	
	UnuseMemory(shape1);
	UnuseMemory(shape2);
	
	return	result;
}



/* This function returns the bounds rect to the_shape.  */

PG_PASCAL (void) pgShapeBounds (shape_ref the_shape, rectangle_ptr bounds)
{
	GetMemoryRecord(the_shape, 0, bounds);
}


/* pgAddRectToShape adds a single rectangle to the shape (following all rectangle
union rules for shapes) 	*/

PG_PASCAL (void) pgAddRectToShape (shape_ref the_shape, const rectangle_ptr rect)
{
	shape_walk				shape_walker;
	
	if (pgEmptyShape(the_shape))
		pgSetShapeRect(the_shape, rect);
	else
	if (!pgEmptyRect(rect)) {
		
		setup_shape(the_shape, &shape_walker);
		combine_new_rect(&shape_walker, rect);

		UnuseMemory(the_shape);
	}
}


/* pgMergeRectToShape is similar to pgAddRectToShape except it only adds the
part(s) of rect that are different than any part of the shape. Basically,
the difference of rect's intersections are added. */

PG_PASCAL (void) pgMergeRectToShape (shape_ref the_shape, const rectangle_ptr rect)
{
	register rectangle_ptr		src_rects;
	register pg_short_t			r_qty, part_qty, part_ctr;
	rectangle					sect_rect;
	rectangle					parts_list[6];
	shape_ref					temp_shape;
	shape_walk					output_shape;
	pg_short_t					internal_sect;

	if (pgEmptyRect(rect))
		return;

	if (!pgSectOrTouchRectInShape(the_shape, rect, NULL)) {
	
		pgAddRectToShape(the_shape, rect);
		setup_shape(the_shape, &output_shape);
		cleanup_shape(&output_shape);
		UnuseMemory(the_shape);
	}
	else {
		
		temp_shape = MemoryDuplicate(the_shape);
		pgSetShapeRect(temp_shape, NULL);

		setup_shape(temp_shape, &output_shape);
		sect_rect = *rect;

		src_rects = (rectangle_ptr) UseMemory(the_shape);
		r_qty = (pg_short_t)GetMemorySize(the_shape) - 1;
		
		while (r_qty) {
			
			++src_rects;

			part_qty = sect_rect_parts(src_rects, &sect_rect, parts_list);
			
			for (part_ctr = 0; part_ctr < part_qty; ++part_ctr)
				if (!pgEmptyRect(&parts_list[part_ctr]))
					combine_new_rect(&output_shape, &parts_list[part_ctr]);

			--r_qty;
		}

		cleanup_shape(&output_shape);

		UnuseMemory(the_shape);
		
		while (internal_sect = rects_sect_internally(&output_shape, &sect_rect)) {
			
			src_rects = output_shape.rects;
			src_rects += (internal_sect - 1);
			
			part_qty = sect_rect_parts(src_rects, &sect_rect, parts_list);

			UnuseMemory(temp_shape);
			DeleteMemory(temp_shape, internal_sect, 1);
			re_init_walk(&output_shape, TRUE);

			for (part_ctr = 0; part_ctr < part_qty; ++part_ctr)
				if (!pgEmptyRect(&parts_list[part_ctr]))
					combine_new_rect(&output_shape, &parts_list[part_ctr]);

			cleanup_shape(&output_shape);
		}
	
		UnuseMemory(temp_shape);
		
		MemoryCopy(temp_shape, the_shape);
		DisposeMemory(temp_shape);
	}
}


/* pgSectShape checks for intersection(s) between shape1 and shape2. If any exist,
the function returns TRUE and, if result_shape is non-NULL, it gets set to the
shape intersection (even if no intesection exists, in which case result_shape
is set to an empty shape).     */

PG_PASCAL (pg_boolean) pgSectShape (shape_ref shape1, shape_ref shape2,
		shape_ref result_shape)
{
	shape_walk				src_walk, target_walk, new_walk;
	register rectangle_ptr	src_rects;
	register pg_short_t		qty;
	pg_short_t				next_scan;
	pg_boolean				result;
	
	if (result_shape)
		pgSetShapeRect(result_shape, NULL);

	if (pgEmptyShape(shape1) || pgEmptyShape(shape2))
		return	FALSE;

	setup_shape(shape1, &src_walk);
	setup_shape(shape2, &target_walk);
	
	if (result = pgSectRect(src_walk.bounds, target_walk.bounds, NULL)) {

		src_rects = src_walk.rects;
		qty = src_walk.num_rects;
		result = FALSE;
	
		if (result_shape) {
			rectangle		target_rect;

			setup_shape(result_shape, &new_walk);
	
			while (qty) {
				
				next_scan = 0;
	
				while (any_secting_rect(src_rects, &target_walk, &target_rect, &next_scan)) {
					
					result = TRUE;
					combine_new_rect(&new_walk, &target_rect);
				}
				
				++src_rects;
				--qty;
			}
			
			UnuseMemory(result_shape);
		}
		else {
		
			while (qty) {
				
				next_scan = 0;
				
				if (result = any_secting_rect(src_rects, &target_walk, NULL, &next_scan))
					break;
				++src_rects;
				--qty;
			}
		}
	}

	UnuseMemory(shape1);
	UnuseMemory(shape2);
	
	return	result;
}


/* pgSectRectInShape returns "TRUE" if rect intersects with any part of the_shape
(actuall, the function result is the rectangle index that intersects, the first
rectangle being 1). If sect_rect is non-NULL it is set to the intersection. */

PG_PASCAL (pg_short_t) pgSectRectInShape (shape_ref the_shape, rectangle_ptr rect,
		rectangle_ptr sect_rect)
{
	shape_walk			walker;
	pg_short_t			result;

	setup_shape(the_shape, &walker);
	result = 0;
	
	if (!any_secting_rect(rect, &walker, sect_rect, &result))
		result = 0;
	
	UnuseMemory(the_shape);
	
	return	result;
}


/* pgSectOrTouchRectInShape is identical to pgSectRectInShape except rects that
are also touching opposite sides are considered. */

PG_PASCAL (pg_short_t) pgSectOrTouchRectInShape (shape_ref the_shape,
		const rectangle_ptr rect, const rectangle_ptr sect_rect)
{
	shape_walk			walker;
	pg_short_t			result;

	setup_shape(the_shape, &walker);
	result = 0;

	if (!any_secting_or_touching_rect(rect, &walker, sect_rect, &result))
		result = 0;

	UnuseMemory(the_shape);
	
	return	result;
}


/* pgDiffShape sets result_shape to the difference between shape1 and shape2.
By "difference" is meant the following: result_shape initially gets set to
shape2, then all intersections of shape1/shape2 are removed from result_shape.
Note that result_shape must not be the same reference as shape as shape2 or shape1. */

PG_PASCAL (void) pgDiffShape (shape_ref shape1, shape_ref shape2,
		shape_ref result_shape)
{
	shape_walk				src_walk, target_walk, result_walk;
	memory_ref				temp_shape;
	rectangle				src_bounds, target_bounds;
	register rectangle_ptr	r_list;
	register pg_short_t		qty;
	rectangle				parts[4];
	pg_short_t				pass_2_rec;
	size_t					dead_rect_index;
	short					part_qty;
	short					complex_result;

	if (pgEmptyShape(shape1) || pgEmptyShape(shape2)) {
	
		MemoryCopy(shape2, result_shape);
		
		return;
	}

	GetMemoryRecord(shape1, 0, &src_bounds);
	GetMemoryRecord(shape2, 0, &target_bounds);
	
	if (!pgSectRect(&src_bounds, &target_bounds, NULL)) {
		
		MemoryCopy(shape2, result_shape);
		
		return;
	}

/* Phase 1:  Produce a temporary shape that contains all the rectangles remaining
from shape2 that won't get completely elmininated by subtracting shape1:  */

	temp_shape = MemoryDuplicate(shape2);
	setup_shape(shape1, &src_walk);

	for (qty = src_walk.num_rects, r_list = src_walk.rects;
			qty;  ++r_list, --qty)
		if (dead_rect_index = rect_will_nuke(r_list, temp_shape))
			DeleteMemory(temp_shape, dead_rect_index, 1);

	pgResetBounds(temp_shape);
	pgSetShapeRect(result_shape, NULL);

	setup_shape(temp_shape, &target_walk);
	setup_shape(result_shape, &result_walk);
	
	if (!pgEmptyShape(temp_shape)) {

		for (qty = src_walk.num_rects, r_list = src_walk.rects;
				qty;  ++r_list, --qty)
			subtract_all_rects(r_list, &target_walk, &result_walk);
		
	/* Phase 2:  Some rects still might intersect so make as many additional
		passes as necessary:   */
		
		complex_result = FALSE;
		
		while (pass_2_rec = rect_still_subtracts(&src_walk, &result_walk,
				parts, &part_qty)) {
			
			complex_result = TRUE;
	
			if (part_qty < 0) {
				
				if (result_walk.num_rects == 1)
					pgFillBlock(result_walk.bounds, sizeof(rectangle) * 2, 0);
				else {
					UnuseMemory(result_shape);
					DeleteMemory(result_shape, pass_2_rec, 1);
	
					r_list = re_init_walk(&result_walk, TRUE);
				}
			}
			else {
	
				pgBlockMove(parts, &result_walk.bounds[pass_2_rec], sizeof(rectangle));
				
				if (--part_qty) {
					
					r_list = (rectangle_ptr) InsertMemory(result_shape, pass_2_rec + 1, part_qty);
					pgBlockMove(&parts[1], r_list, part_qty * sizeof(rectangle));
					
					r_list = re_init_walk(&result_walk, FALSE);
				}
			}
		}
	
		if (complex_result)
			if (!pgEmptyShape(result_shape))
				cleanup_shape(&result_walk);		
	}

	UnuseMemory(shape1);
	UnuseAndDispose(temp_shape);
	UnuseMemory(result_shape);
}


/* pgOffsetShape moves the whole shape h and v distance (can be negative)  */

PG_PASCAL (void) pgOffsetShape (shape_ref the_shape, long h, long v)
{
	register rectangle_ptr	r_list;
	register pg_short_t		qty;
	
	if (h || v) {
		
		for (r_list = (rectangle_ptr) UseMemory(the_shape), qty = (pg_short_t)GetMemorySize(the_shape); qty;
				++r_list, --qty)
			pgOffsetRect(r_list, h, v);
		
		UnuseMemory(the_shape);
	}
}

/* pgInsetShape insets the outward edges of the_shape by h and v amounts. */

PG_PASCAL (void) pgInsetShape (shape_ref the_shape, long h, long v)
{
	shape_walk					walker;
	register rectangle_ptr		r_ptr;
	register pg_short_t			qty;
	pg_short_t					dummy_offset;

	setup_shape(the_shape, &walker);
	r_ptr = walker.rects;
	
	for (qty = walker.num_rects; qty; ++r_ptr, --qty) {
		
		if (!any_touching_rect(r_ptr, &walker, &dummy_offset, use_top))
			r_ptr->top_left.v += v;
		if (!any_touching_rect(r_ptr, &walker, &dummy_offset, use_left))
			r_ptr->top_left.h += h;
		if (!any_touching_rect(r_ptr, &walker, &dummy_offset, use_bottom))
			r_ptr->bot_right.v -= v;
		if (!any_touching_rect(r_ptr, &walker, &dummy_offset, use_right))
			r_ptr->bot_right.h -= h;
	}

	UnuseMemory(the_shape);
}


/* pgRectInShape returns TRUE if the given rect, offset by offset_extra,
intersects any part of the shape. Offset_extra can be NULL. And, if
sect_rect is non_NULL the located intersection is filled in.   */

PG_PASCAL (pg_boolean) pgRectInShape (shape_ref the_shape, const rectangle_ptr rect,
		const co_ordinate_ptr offset_extra, rectangle_ptr sect_rect)
{
	rectangle				test_rect;
	register rectangle_ptr	rect_ptr;
	register pg_short_t		qty;
	short					result;

	test_rect = *rect;
	if (offset_extra)
		pgOffsetRect(&test_rect, offset_extra->h, offset_extra->v);
	
	for (rect_ptr = (rectangle_ptr) UseMemory(the_shape), qty = (pg_short_t)GetMemorySize(the_shape) - 1;
			qty; --qty) {
		
		++rect_ptr;

		if (result = pgSectRect(rect_ptr, &test_rect, sect_rect))
			break;
	}
	
	UnuseMemory(the_shape);
	
	return	result;
}



/* pgPtInShape returns "TRUE" if point is within any part of the shape (actually,
what is returned is the rectangle number beginning with #1). 
The shape is temporarily offset with offset_extra if offset_extra is non_NULL.
The shape is also temporarily inset by inset_extra if non-NULL.
Note, for convenience the_shape can be NULL, which of course returns FALSE. */

PG_PASCAL (pg_short_t) pgPtInShape (shape_ref the_shape, const co_ordinate_ptr point,
		const co_ordinate_ptr offset_extra, const co_ordinate_ptr inset_extra,
		const pg_scale_ptr scaling)
{
	co_ordinate				the_pt;
	shape_walk				shape_walker;
	register rectangle_ptr	rect_ptr;
	register pg_short_t		qty, r_num;
	pg_short_t				result;

	if (!the_shape)
		return	FALSE;

	the_pt = *point;
	if (offset_extra) {
		the_pt.h -= offset_extra->h;
		the_pt.v -= offset_extra->v;
	}
	
	setup_shape(the_shape, &shape_walker);
	rect_ptr = shape_walker.rects;
	
	for (result = FALSE, r_num = 1, qty = shape_walker.num_rects; qty;
					++rect_ptr, ++r_num, --qty)
		if (result = pgPtInRectInset(&the_pt, rect_ptr, inset_extra, scaling)) {
			
			result = r_num;
			break;
		}
	
	UnuseMemory(the_shape);
	
	return	result;
}


/* pgMaxRectInShape gets called frequently by pgTextBlock when figuring out
non-rectangular word-wraps. The assumption made by calling this function is two
or more rectangles are overlapping vertically, but not necessarily with the
same horizontal positions or widths. Hence, the purpose of pgMaxRectInShape is
to determine the largest possible width that can be obtained given a top
position and minimum height. On entry, rect's top position should be set to the
desired top and rect's bottom should be the minimum acceptable; rect's left and
right should be the prefered locations (but might get changed by this function).
Also on entry, *r_num should contain the relative rectangle to begin the
computaion -- of which rect must be an intersection of in some way.  The
minimum_h parameter is the minimum resulting rect width that can be accepted.
On return, rect gets set to the maximum left and right positions while still
mantaining the minimum top/bottom, and minimum_h width, and *r_num will be
updated to the ending rectangle of a possible union of several rectangles.
If the height is not possible at all, either because there is no vertical space
or the resulting width in rect has gone below minimum_h, FALSE is returned.
11/2/93 -- for sake of "repeater" feature it is necessary to pass offset_extra,
which is the amount the source rect has been offset in a repeat. */


PG_PASCAL (short) pgMaxRectInShape (shape_ref the_shape, rectangle_ptr rect,
		long minimum_h, long PG_FAR *r_num, co_ordinate_ptr offset_extra)
{
	shape_walk				walker;
	register rectangle_ptr	r_ptr, src_rect;
	register pg_short_t		r_ctr, r_qty;
	register long			accumulated_v, minimum_v;

	setup_shape(the_shape, &walker);
	r_ptr = walker.rects;
	r_ctr = (pg_short_t)*r_num;
	r_ptr += r_ctr;

	src_rect = rect;
	pgOffsetRect(src_rect, -offset_extra->h, -offset_extra->v);

	minimum_v = src_rect->bot_right.v - src_rect->top_left.v;
	accumulated_v = r_ptr->bot_right.v - src_rect->top_left.v;
	src_rect->top_left.h = r_ptr->top_left.h;
	src_rect->bot_right.h = r_ptr->bot_right.h;

	r_qty = walker.num_rects - 1;
	
	while (r_ctr < r_qty) {
		
		if (accumulated_v >= minimum_v)
			break;
		
		if (!compare_rect_sides(r_ptr, &r_ptr[1], use_bottom))
			break;
		
		++r_ctr;
		++r_ptr;
		
		if (r_ptr->top_left.h > src_rect->top_left.h)
			src_rect->top_left.h = r_ptr->top_left.h;
		if (r_ptr->bot_right.h < src_rect->bot_right.h)
			src_rect->bot_right.h = r_ptr->bot_right.h;

		accumulated_v += (r_ptr->bot_right.v - r_ptr->top_left.v);
		
		if ((src_rect->bot_right.h - src_rect->top_left.h) < minimum_h)
			break;
	}
	
	*r_num = r_ctr;

	UnuseMemory(the_shape);

	pgOffsetRect(src_rect, offset_extra->h, offset_extra->v);

	return	((accumulated_v >= minimum_v)
			&& ((src_rect->bot_right.h - src_rect->top_left.h) >= minimum_h));
}


/* pgExcludeRectInShape is called mainly by text calculating functions and is
used to determine "sub" rectangles within a single rectangle due to the
exclusion shape (exclude_shape) overlapping portions of it. On entry, pg contains
the exclusion shape (where to exclude text);  rect is the maximum line
rectangle.  On return, result_ref will be set to one or more rectangles that
define areas to "avoid."  Also, if necessary, left and/or right sides of rect will
be changed to accommodate the intersections  Note that at least one rectangle
will be set in result_ref, even if no exclusions exist (in which case the single
"exclude" rectangle will be beyond the boundaries of rect). The minimum_width
parameter defines the smallest non-exclusion area that can be produced. This
function will return the number of intersections found (which implies a "FALSE"
if none found). Also, lowest_exclude gets set to the smallest bottom in the
series of exclude rects (this value is required to hop over a completely
excluded area).
ADDITION 5-27-96 -- Now we need to check for paragraph exclusions. */

PG_PASCAL (pg_short_t) pgExcludeRectInShape (paige_rec_ptr pg, rectangle_ptr rect,
		long minimum_width, long PG_FAR *lowest_exclude, long line_offset, memory_ref result_ref)
{
	shape_walk				x_walker;
	shape_ref				exclude_shape;
	register rectangle_ptr	output_ptr, input;
	rectangle_ptr			overlap_ptr;
	rectangle				sect_rect, src_bounds;
	co_ordinate				no_exclude_values;
	long					output_qty;
	pg_short_t				sect_index;
	
	exclude_shape = pg->exclude_area;
	
	if (pgEmptyShape(exclude_shape))
		return	0;

	GetMemoryRecord(exclude_shape, 0, &src_bounds);
	if (!pgSectRect(&src_bounds, rect, NULL))
		return	0;

	input = rect;
	
	output_qty = 1;
	SetMemorySize(result_ref, output_qty);

	no_exclude_values.h = input->bot_right.h + 1;
	no_exclude_values.v = BOTTOMLESS_VALUE - input->top_left.v;
	
	output_ptr = (rectangle_ptr) UseMemory(result_ref);
	output_ptr->top_left = output_ptr->bot_right = no_exclude_values;
	*lowest_exclude = no_exclude_values.v;

	setup_shape(exclude_shape, &x_walker);
	sect_index = 0;
	
	while (any_secting_rect(input, &x_walker, &sect_rect, &sect_index)) {
		
		if (!omit_par_exclusion(pg, line_offset, sect_index)) {
		
			overlap_ptr = x_walker.rects + (sect_index - 1);
			
			if (overlap_ptr->bot_right.v < *lowest_exclude)
				*lowest_exclude = overlap_ptr->bot_right.v;
	
			if ((sect_rect.top_left.h - input->top_left.h) < minimum_width)
				input->top_left.h = sect_rect.bot_right.h;
	
			if ((input->bot_right.h - sect_rect.bot_right.h) < minimum_width)
				input->bot_right.h = sect_rect.top_left.h;
			
			if (overlap_ptr = overlapping_exclude(result_ref, &sect_rect,
					minimum_width))
				pgUnionRect(&sect_rect, overlap_ptr, overlap_ptr);
			else {
			
				pgBlockMove(&sect_rect, output_ptr, sizeof(rectangle));
				
				output_ptr = (rectangle_ptr) AppendMemory(result_ref, 1, FALSE);
				output_ptr->top_left = output_ptr->bot_right = no_exclude_values;
				
				++output_qty;
			}
		}
	}

	UnuseMemory(exclude_shape);
	UnuseMemory(result_ref);

	while (output_ptr = rects_wrong_order(result_ref, (pg_short_t)(output_qty - 1))) {
		rectangle			swap;
		
		swap = *output_ptr;
		*output_ptr = output_ptr[1];
		output_ptr[1] = swap;
		
		UnuseMemory(result_ref);
	}

	return	(pg_short_t)(output_qty - 1);
}


/* pgShapeDimension returns a set of bits defining how complex a shape is. If
all bits are clear, the shape is the simplest form (a single rectangle). Otherwise,
bits are set per the #defines in this header file.    */

PG_PASCAL (short) pgShapeDimension (shape_ref the_shape)
{
	shape_walk				shape_walker;
	register rectangle_ptr	rect_ptr, next_rect_ptr;
	register pg_short_t		qty;
	register long			first_width;
	short					dimension;
	
	dimension = 0;

	if (GetMemorySize(the_shape) > SIMPLE_SHAPE_QTY) {
		
		dimension = MULTIPLE_RECT_DIMENSION;

		setup_shape(the_shape, &shape_walker);
		rect_ptr = next_rect_ptr = shape_walker.rects;
		++next_rect_ptr;
		first_width = rect_ptr->bot_right.h - rect_ptr->top_left.h;

		for (qty = shape_walker.num_rects - 1; qty; ++rect_ptr, ++next_rect_ptr, --qty) {
			
			if ((next_rect_ptr->bot_right.h - next_rect_ptr->top_left.h) != first_width)
				dimension |= MUTIPLE_WIDTH_DIMENSION;
			
			if (next_rect_ptr->top_left.v < rect_ptr->bot_right.v) {
				
				dimension |= NOT_STACKED_DIMENSION;
				if (pgSectRect(rect_ptr, next_rect_ptr, NULL))
					dimension |= OVERLAP_DIMENSION;
			}

			if (!(dimension & UNION_DIMENSION))
				if (compare_rect_sides(rect_ptr, next_rect_ptr, use_bottom)
					|| compare_rect_sides(rect_ptr, next_rect_ptr, use_right))
					dimension |= UNION_DIMENSION;

			if (dimension == ALL_DIMENSIONS)
				break;
		}
	
		UnuseMemory(the_shape);
	}
	
	return	dimension;
}


/* pgShapeToSelections fills in a memory_ref of select_pair entries that would
be affected by the_shape. In other words, the select_pair result(s) will be all
the text offset pairs that intersect the shape. If is_scrolled is TRUE the
shape provided is "scrolled" (on screen as-is),  */

PG_PASCAL (void) pgShapeToSelections (pg_ref pg, shape_ref the_shape, memory_ref selections)
{
	paige_rec_ptr		pg_rec;
	text_block_ptr		block;
	point_start_ptr		starts;
	rectangle			bounds_rect, sect_rect;
	select_pair			range;
	t_select			fake_select;
	pg_short_t			num_blocks;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	SetMemorySize(selections, 0);
	
	block = (text_block_ptr) UseMemory(pg_rec->t_blocks);
	num_blocks = (pg_short_t)GetMemorySize(pg_rec->t_blocks);
	
	pgPaginateBlock(pg_rec, block, NULL, FALSE);

	while (num_blocks) {
		
		if (pgRectInShape(the_shape, &block->bounds, NULL, NULL)) {
			
			pgPaginateBlock(pg_rec, block, NULL, TRUE);
			
			range.begin = range.end = block->begin;
			
			starts = (point_start_ptr) UseMemory(block->lines);
			
			while (starts->flags != TERMINATOR_BITS) {
				
				pgBlockMove(&starts->bounds, &bounds_rect, sizeof(rectangle));
				pgInsetRect(&sect_rect, 1, 1);

				if (pgRectInShape(the_shape, &bounds_rect, NULL, &sect_rect)) {
					
					++sect_rect.top_left.v;
					--sect_rect.bot_right.v;
					
					if (sect_rect.top_left.h == bounds_rect.top_left.h)
						fake_select.offset = block->begin + (long) starts->offset;
					else	
						pg_rec->procs.offset_proc(pg_rec, &sect_rect.top_left, FALSE,
								&fake_select);
					range.begin = fake_select.offset;
		
					if (sect_rect.bot_right.h == bounds_rect.bot_right.h)
						fake_select.offset = block->begin + (long) starts[1].offset;
					else
						pg_rec->procs.offset_proc(pg_rec, &sect_rect.bot_right, FALSE,
							&fake_select);
					range.end = fake_select.offset;

					dump_select_pair(&range, selections);
				}

				++starts;
			}
			
			UnuseMemory(block->lines);
		}
		
		++block;
		--num_blocks;
	}

	UnuseMemory(pg_rec->t_blocks);
	UnuseMemory(pg);
}


/* pgGetWrapRect computes the "real" wrap rectangle based on r_num and possible
repeater flags in pg->doc_info. Repeater rectangles are determined by r_num
which might be larger than number of rects in pg->page_area. The computation is
r_num / qty of rects, where quotient is number of repeats and remainder (modulo)
is is rectangle index.  In this function, *offset_extra gets set to the amount
of pixel offset to set the real rect and the function result is the actual
rectangle number.  */

PG_PASCAL (pg_short_t) pgGetWrapRect (paige_rec_ptr pg, size_t r_num, co_ordinate_ptr offset_extra)
{
	register co_ordinate_ptr		offset;
	register long					repeat_flags;
	co_ordinate						dummy_offset;
	rectangle						bounds;
	long							repeat_size_v, repeat_size_h;
	pg_short_t						result, r_qty, repeat_qty;

	if (!(offset = offset_extra))
		offset = &dummy_offset;

	offset->h = offset->v = 0;
	result = (pg_short_t)r_num;
	repeat_flags = pg->doc_info.attributes;

	if (repeat_flags & (V_REPEAT_BIT | H_REPEAT_BIT)) {
		
		r_qty = (pg_short_t)GetMemorySize(pg->wrap_area) - 1;

		result = (pg_short_t)(r_num % (long)r_qty);
		if (repeat_qty = (pg_short_t)(r_num / (long)r_qty)) {
			
			GetMemoryRecord(pg->wrap_area, 0, &bounds);
			repeat_size_v = bounds.bot_right.v - bounds.top_left.v + pg->doc_info.repeat_offset.v;
			repeat_size_h = bounds.bot_right.h - bounds.top_left.h + pg->doc_info.repeat_offset.h;
			
			if (repeat_flags & V_REPEAT_BIT)
				offset->v = repeat_size_v * repeat_qty;
			if (repeat_flags & H_REPEAT_BIT)
				offset->h = repeat_size_h * repeat_qty;
		}
	}
	
	return	result;
}


/* pgGetSectWrapRect returns an identical result as pgGetWrapRect (above)
except instead of a rectangle number, a rectangle is given whose intersection
with the first rect in pg->wrap_area determines the answer. Additionally, r_num
is determined (1-based) which is the "fake" rectangle in case of repeater shape. */
 
PG_PASCAL (pg_short_t) pgGetSectWrapRect (paige_rec_ptr pg, rectangle_ptr r_sect,
		pg_short_t PG_FAR *r_num, co_ordinate_ptr repeat_offset)
{
	pg_short_t						fake_r_num;

	if (!(pg->doc_info.attributes & (V_REPEAT_BIT | H_REPEAT_BIT))) {
		
		repeat_offset->h = repeat_offset->v = 0;

		if (fake_r_num = pgSectRectInShape(pg->wrap_area, r_sect, NULL))
			--fake_r_num;

		if (r_num)
			*r_num = fake_r_num;
		
		return	fake_r_num;
	}
	
	fake_r_num = pgPixelToPage(pg->myself, &r_sect->top_left, repeat_offset,
			NULL, NULL, NULL, FALSE);
	
	if (r_num)
		*r_num = fake_r_num;
	
	return	fake_r_num % (pg_short_t)((GetMemorySize(pg->wrap_area) - 1));
}


/* pgNumRectsInShape returns the number of rectangles in the_shape */

PG_PASCAL (pg_short_t) pgNumRectsInShape (shape_ref the_shape)
{
	return	(pg_short_t)(GetMemorySize(the_shape) - 1);
}


/* pgEraseShape fills the shape with the background color appropriate to the
machine. If scale_factor is non-NULL each part of the shape is scaled. If
offset_extra is non-NULL each part is offset by that much. If vis_bounds is
non-NULL only the parts that intersect with it are erased. */

PG_PASCAL (void) pgEraseShape (pg_ref pg, shape_ref the_shape, const pg_scale_ptr scale_factor,
		const co_ordinate_ptr offset_extra, const rectangle_ptr vis_bounds)
{
	register rectangle_ptr				r_ptr, sect_ptr;
	register pg_short_t					r_qty, r_ctr;
	paige_rec_ptr						pg_rec;
	rectangle							sect_rect;
	long								container_proc_refcon;
	
	if (pgEmptyShape(the_shape))
		return;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	r_ptr = (rectangle_ptr) UseMemory(the_shape);

	if (!(sect_ptr = vis_bounds))
		sect_ptr = r_ptr;
	
	r_qty = (pg_short_t)(GetMemorySize(the_shape) - 1);
	container_proc_refcon = 0;

	if (pgSectRect(r_ptr, sect_ptr, &sect_rect)) {

		pgSetupGrafDevice(pg_rec, NULL, MEM_NULL, clip_standard_verb);

		pg_rec->procs.container_proc(pg_rec, 1, &sect_rect, scale_factor,
				offset_extra, clip_container_verb, &container_proc_refcon);

		for (r_ctr = 1; r_ctr < r_qty; ++r_ctr)
			if (pgSectRect(&r_ptr[r_ctr], vis_bounds, &sect_rect)) {
	
			pg_rec->procs.container_proc(pg_rec, r_ctr, &sect_rect, scale_factor,
					offset_extra, erase_rect_verb, NULL);
		}

		pg_rec->procs.container_proc(pg_rec, 1, r_ptr, scale_factor,
					offset_extra, unclip_container_verb, &container_proc_refcon);

		pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
	}

	UnuseMemory(the_shape);
	UnuseMemory(pg);
}


/* pgScaleShape scales the whole shape, using the origin in pg, to numerator -over- denomintor. */

PG_PASCAL (void) pgScaleShape (paige_rec_ptr pg, shape_ref shape, short numerator, short denomintor)
{
	register rectangle_ptr	rects;
	pg_scale_factor			scale;
	size_t					num_rects;
	
	scale.scale = numerator;
	scale.scale <<= 16;
	scale.scale |= denomintor;
	scale.origin = pg->port.origin;

	num_rects = GetMemorySize(shape);
	
	for (rects = (rectangle_ptr) UseMemory(shape); num_rects; ++rects, --num_rects)
		pgScaleRect(&scale, NULL, rects);
	
	UnuseMemory(shape);
	
	pgResetBounds(shape);
}



/***************************  Local Functions  ************************/



/* This function initializes a shape struct so it is easier to work with. */

static void setup_shape (shape_ref the_shape, shape_walk_ptr shape_stuff)
{
	register shape_walk_ptr		shape_walker;
	
	shape_walker = shape_stuff;

	shape_walker->mem_ref = the_shape;
	shape_walker->num_rects = (pg_short_t)(GetMemorySize(the_shape) - 1);
	shape_walker->rects = shape_walker->bounds = (rectangle_ptr) UseMemory(the_shape);
	++shape_walker->rects;
}

/* This function scans through the rectangle list and, if a significant side
matches it returns the pointer to the match. (By "significant side" means that the
heights or depths match along with the horizontal or vertical edges). This ALSO
can mean one rectangle sitting inside of another (if one rect is completely
enclosed by another, it is considered merged with that rect). 
If mask_rec is non-NULL, it skips *mask_rec record during the scan.  */

static rectangle_ptr any_partial_rect_match (shape_walk_ptr shape_walker,
			rectangle_ptr rect, pg_short_t PG_FAR *mask_rec)
{
	register rectangle_ptr	scanner;
	register rectangle_ptr	matcher;
	register pg_short_t		counter, rec_num;
	pg_short_t				mask_out;
	
	if (mask_rec)
		mask_out = *mask_rec;
	else
		mask_out = 0;

	scanner = shape_walker->rects;
	rec_num = 1;

	if (mask_out != 1)
		if ((scanner->top_left.h == scanner->bot_right.h)
			|| (scanner->top_left.v == scanner->bot_right.v))
				return	scanner;

	for (counter = shape_walker->num_rects, matcher = rect; counter; ++scanner, --counter, ++rec_num) {
		
		if (mask_out != rec_num) {
			
			if ((matcher->top_left.h >= scanner->top_left.h)
				&& (matcher->bot_right.h <= scanner->bot_right.h)
				&& (matcher->top_left.v >= scanner->top_left.v)
				&& (matcher->bot_right.v <= scanner->bot_right.v))
					return	scanner;

		    if ((matcher->top_left.h == scanner->top_left.h)
			   && (matcher->bot_right.h == scanner->bot_right.h))
				if ((matcher->top_left.v == scanner->bot_right.v)
				   || (matcher->top_left.v == scanner->top_left.v)
				   || (matcher->bot_right.v == scanner->bot_right.v)
				   || (matcher->bot_right.v == scanner->top_left.v))
				   		return	scanner;
			
		    if ((matcher->top_left.v == scanner->top_left.v)
			   && (matcher->bot_right.v == scanner->bot_right.v))
				if ((matcher->top_left.h == scanner->bot_right.h)
				   || (matcher->top_left.h == scanner->top_left.h)
				   || (matcher->bot_right.h == scanner->bot_right.h)
				   || (matcher->bot_right.h == scanner->top_left.h))
				   		return	scanner;
		}
	}
	
	return	NULL;
}


/* This finds a rectangle in shape_stuff that intersects with rect and, if
any, returns TRUE and sets result_rect to the intersection if non-NULL.  Note
that only the first rect intersecting is dealt with. The first rect in the
list is *start_rect and start_rect is incremented to the next one to check.  */

static short any_secting_rect (rectangle_ptr rect, shape_walk_ptr shape_stuff,
		rectangle_ptr result_rect, pg_short_t PG_FAR *start_rect)
{
	register rectangle_ptr		src_rects;
	register pg_short_t			qty;
	
	if ((qty = shape_stuff->num_rects - *start_rect) <= 0)
		return	FALSE;

	for (src_rects = shape_stuff->rects + *start_rect; qty;  ++src_rects, --qty) {
		
		*start_rect += 1;

		if (pgSectRect(rect, src_rects, result_rect))
			return	TRUE;
	}
	
	return	FALSE;
}


/* any_secting_or_touching_rect is identical to any_secting_rect except rects
that touch left or right opposite sides are also considered. */

static short any_secting_or_touching_rect (rectangle_ptr rect, shape_walk_ptr shape_stuff,
		rectangle_ptr result_rect, pg_short_t PG_FAR *start_rect)
{
	register rectangle_ptr		src_rects;
	register pg_short_t			qty;
	
	if ((qty = shape_stuff->num_rects - *start_rect) <= 0)
		return	FALSE;

	for (src_rects = shape_stuff->rects + *start_rect; qty;  ++src_rects, --qty) {
		
		*start_rect += 1;

		if (pgSectOrTouchRect(rect, src_rects, result_rect))
			return	TRUE;
	}
	
	return	FALSE;
}


/* This function walks through the shape and locates any rectangle that exactly
touches against the designated side_to_use (side_to_use defines the source
rectangle's sides -- either use_top, use_bottom, use_left or use_right).  */

static rectangle_ptr any_touching_rect (rectangle_ptr rect, shape_walk_ptr shape_stuff,
		pg_short_t PG_FAR *found_rect, short side_to_use)
{
	register rectangle_ptr		source, target;
	register pg_short_t			qty, ctr;

	target = shape_stuff->rects;
	source = rect;

	for (qty = shape_stuff->num_rects, ctr = 0; qty; ++target, ++ctr, --qty)
		if (compare_rect_sides(source, target, side_to_use)) {
			
			*found_rect = ctr;
			
			return	target;
		}

	return	NULL;
}


/* This function compares left/right or top/bottom sides of the two rects,
returning TRUE if any match.  */

static short compare_rect_sides (rectangle_ptr src, rectangle_ptr dest,
		short side_to_use)
{
	switch (side_to_use) {
		
		case use_top:
			return (src->top_left.v == dest->bot_right.v);
			break;

		case use_left:
			return (src->top_left.h == dest->bot_right.h);

			break;
			
		case use_bottom:
			return (src->bot_right.v == dest->top_left.v);

			break;
			
		case use_right:
			return (src->bot_right.h == dest->top_left.h);

			break;
	}
	
	return	FALSE;
}



/* This function adds a new rect to the shape_stuff and updates shape_stuff
accordingly.    */

static void combine_new_rect (shape_walk_ptr shape_stuff, rectangle_ptr new_rect)
{
	rectangle_ptr			matching_rect;

	if (matching_rect = any_partial_rect_match(shape_stuff, new_rect, NULL))
		pgUnionRect(new_rect, matching_rect, matching_rect);
	else {

		pgBlockMove(new_rect, AppendMemory(shape_stuff->mem_ref, 1, FALSE),
					sizeof(rectangle));
		
		re_init_walk(shape_stuff, FALSE);
	}

	pgUnionRect(new_rect, shape_stuff->bounds, shape_stuff->bounds);
}


/* This function walks through target and subtracts sub_rect from each rect
and outputs the difference to output.  */

static void subtract_all_rects (rectangle_ptr sub_rect, shape_walk_ptr target,
			shape_walk_ptr output)
{
	rectangle				sub_parts[4];
	register rectangle_ptr	src_list;
	register pg_short_t		qty, add_ctr;
	short					sub_qty;

	qty = target->num_rects;
	src_list = target->rects;
	
	while (qty) {

		if ((sub_qty = subtract_rect(sub_rect, src_list, sub_parts)) > 0)
			for (add_ctr = 0; add_ctr < (pg_short_t)sub_qty; ++add_ctr)
				combine_new_rect(output, &sub_parts[add_ctr]);
		else
		if (sub_qty == 0)
			combine_new_rect(output, src_list);

		++src_list;
		--qty;
	}
}


/* This function re-sets the pointers in a shape walk (because memory could have
relocated). If re_use is TRUE, a new UseMemory is performed.  For convenience,
the "rects" pointer is returned.  */

static rectangle_ptr re_init_walk (shape_walk_ptr walker, short re_use)
{
	register shape_walk_ptr		walk;
	
	walk = walker;
	
	walk->bounds = (rectangle_ptr) UseMemoryRecord(walk->mem_ref, 0, USE_ALL_RECS, re_use);
	walk->rects = (walk->bounds + 1);
	walk->num_rects = (pg_short_t)(GetMemorySize(walk->mem_ref) - 1);
	
	return	walk->rects;
}


/* This is used as the final phase of "pgDiffShape." After I have made the first
pass subtracting all source rects from target rects, I am left with a series of
rects that still might intersect the originals, hence, I need to make multiple
passes to break down the remaining rects. During this pass, rectangles often need
to get inserted in the middle; to help, this function returns the nth element
in the output list (beginning with 1) as the rectangle that still needs some
subtraction, with its broken down sub rects in "parts" and the part quantity in
*part_qty.  Note that *part_qty could be negative, which means the target rect
should be deleted.  */

static pg_short_t rect_still_subtracts (shape_walk_ptr input, shape_walk_ptr output,
		rectangle_ptr parts, short PG_FAR *part_qty)
{
	register rectangle_ptr		src, target;
	register pg_short_t			out_rec_num, in_qty, out_qty;

	if (pgEmptyShape(output->mem_ref))
		return	0;

	src = input->rects;
	in_qty = input->num_rects;
	
	while (in_qty) {
		
		out_qty = output->num_rects;
		target = output->rects;
		out_rec_num = 1;
		
		while (out_qty) {
			
			if (*part_qty = subtract_rect(src, target, parts))
				return	out_rec_num;
			
			++target;
			++out_rec_num;
			--out_qty;
		}
		
		++src;
		--in_qty;
	}
	
	return	0;		/* Getting here means nothing else subtracts.  */
}


/* This function subtracts src from target and outputs the resulting difference(s)
in parts. The number of output parts is returned, which can be -1 through 4. Note
that -1 means the subtraction would result in an empty rect, while 0 means no
subtraction took place at all.  MAKE SURE THE PARTS POINTER CAN BE FILLED WITH
4 RECTS (worst case). */

static short subtract_rect (rectangle_ptr src, rectangle_ptr target,
		rectangle_ptr parts)
{
	rectangle					sect;
	register rectangle_ptr		small_r, big_r, output;
	register short				output_ctr;

	if (pgCoversRect(src, target))
		return	-1;

	output_ctr = 0;

	if (pgSectRect(src, target, &sect)) {

		small_r = &sect;
		big_r = target;
		output = parts;
		
		if (small_r->top_left.v > big_r->top_left.v) {
			
			*output = *big_r;
			output->bot_right.v = small_r->top_left.v;
			
			++output;
			++output_ctr;
		}

		if (small_r->top_left.h > big_r->top_left.h) {
			
			output->top_left.v = small_r->top_left.v;
			output->top_left.h = big_r->top_left.h;
			output->bot_right.v = small_r->bot_right.v;
			output->bot_right.h = small_r->top_left.h;

			++output;
			++output_ctr;
		}

		if (small_r->bot_right.h < big_r->bot_right.h) {

			output->top_left.v = small_r->top_left.v;
			output->top_left.h = small_r->bot_right.h;
			output->bot_right.v = small_r->bot_right.v;
			output->bot_right.h = big_r->bot_right.h;

			++output;
			++output_ctr;
		}

		if (small_r->bot_right.v < big_r->bot_right.v) {

			*output = *big_r;
			output->top_left.v = small_r->bot_right.v;

			++output_ctr;
		}
	}
	
	return	output_ctr;
}

/* This function walks through the list of rects, cleans up touching or duplicate
rects and rebuilds its bounds. The shape is known to be non-empty.  */

static void cleanup_shape (shape_walk_ptr shape_to_clean)
{
	register rectangle_ptr		r_list, bounds;
	register pg_short_t			qty;
	rectangle					swap_rect;
	pg_short_t					delete_num;

	while (delete_num = merge_two_rects(shape_to_clean)) {
		
		UnuseMemory(shape_to_clean->mem_ref);
		DeleteMemory(shape_to_clean->mem_ref, delete_num, 1);
		re_init_walk(shape_to_clean, TRUE);
	}
	
	bounds = shape_to_clean->bounds;
	
	pgFillBlock(bounds, sizeof(rectangle), 0);

	for (r_list = shape_to_clean->rects, qty = shape_to_clean->num_rects;
			qty; ++r_list, --qty)
		pgUnionRect(r_list, bounds, bounds);

/* Now re-sort the vertical positions so rects read from top to bottom. */
	
	qty = shape_to_clean->num_rects - 1;
	r_list = shape_to_clean->rects;
	
	while (qty) {
		
		if (r_list->top_left.v > r_list[1].top_left.v) {
			
			swap_rect = *r_list;
			*r_list = r_list[1];
			r_list[1] = swap_rect;

			qty = shape_to_clean->num_rects - 1;
			r_list = shape_to_clean->rects;
		}
		else {
			
			--qty;
			++r_list;
		}
	}
}


/* This function is used in conjunction with cleanup_shape;  it finds a matching
pair of rects (that should be merged together) and, if so, merges the two
with pgUnionRect and returns the higher record number pair to be deleted.  */

static pg_short_t merge_two_rects (shape_walk_ptr merge_walk)
{
	register rectangle_ptr		r_list, match;
	pg_short_t					qty;
	
	if ((qty = merge_walk->num_rects) < 2)
		return	0;
	
	r_list = merge_walk->rects;

	for (r_list += qty; qty; --qty) {
		
		--r_list;

		if (match = any_partial_rect_match(merge_walk, r_list, &qty)) {
			
			pgUnionRect(r_list, match, match);
			
			return	qty;
		}
	}
	
	return	0;
}

/* This function puts in the_pair if (A) begin < end, (b) the previous ending
selection != begin, OR (c) no selections exist.  */

static void dump_select_pair (select_pair_ptr the_pair, memory_ref selections)
{
	select_pair_ptr		append_select;
	select_pair			last_select;
	pg_short_t			num_selects;

	if (the_pair->begin < the_pair->end) {
		
		if (num_selects = (pg_short_t)GetMemorySize(selections))
			GetMemoryRecord(selections, num_selects - 1, &last_select);
		else
			last_select.end = -1;
		
		if (the_pair->begin != last_select.end) {
			
			append_select = (select_pair_ptr) AppendMemory(selections, 1, FALSE);
			*append_select = *the_pair;
		}
		else {
			
			append_select = (select_pair_ptr) UseMemory(selections);
			append_select->end = the_pair->end;
		}
		
		UnuseMemory(selections);
	}
}


/* This is a function used by pgDiffShape, asking if any rect in the_shape
will get completely removed if src_rect is subtracted.  If so, the index
(beginning with 1) is returned.  */

static size_t rect_will_nuke (rectangle_ptr src_rect, shape_ref the_shape)
{
	register rectangle_ptr		r_ptr, r_list;
	register size_t				index, qty;

	if ((qty = GetMemorySize(the_shape)) >= SIMPLE_SHAPE_QTY) {
		
		r_list = (rectangle_ptr) UseMemory(the_shape);
		r_ptr = src_rect;

		index = 1;
		
		while (index < qty) {
			
			++r_list;
			
			if ((r_ptr->top_left.h <= r_list->top_left.h)
				&& (r_ptr->bot_right.h >= r_list->bot_right.h)
				&& (r_ptr->top_left.v <= r_list->top_left.v)
				&& (r_ptr->bot_right.v >= r_list->bot_right.v))  {
				
				UnuseMemory(the_shape);

				return	index;
			}
			
			++index;
		}
		
		UnuseMemory(the_shape);
	}
	
	return	0;		/* (The default */
}


/* This gets called mainly by pgExcludeRectInShape. The exclude_rects ref
contains 1 or more "exclude" rectangles. This function determines if any of
them come too close to sect_rect to add a new rect and, if so, returns the
pointer from exclude_rects (NOTE: The exclude_rects ref is already in a USED
state before the function is called). */
 
static rectangle_ptr overlapping_exclude (memory_ref exclude_rects,
		rectangle_ptr sect_rect, long minimum_width)
{
	register rectangle_ptr			exclude_ptr, sect_ptr;
	register size_t					exclude_qty;
	
	exclude_qty = GetMemorySize(exclude_rects) - 1;
	exclude_ptr = (rectangle_ptr) UseMemoryRecord(exclude_rects, 0, USE_ALL_RECS, FALSE);
	sect_ptr = sect_rect;
	
	while (exclude_qty) {
		
		if (pgAbsoluteValue(sect_ptr->top_left.h - exclude_ptr->top_left.h) < minimum_width)
			return	exclude_ptr;
		if (pgAbsoluteValue(sect_ptr->bot_right.h - exclude_ptr->bot_right.h) < minimum_width)
			return	exclude_ptr;
		if (pgAbsoluteValue(sect_ptr->top_left.h - exclude_ptr->bot_right.h) < minimum_width)
			return	exclude_ptr;
		if (pgAbsoluteValue(sect_ptr->bot_right.h - exclude_ptr->top_left.h) < minimum_width)
			return	exclude_ptr;
		
		++exclude_ptr;
		--exclude_qty;
	}
	
	return	NULL;
}


/* This gets called by pgExcludeRectInShape. The rect_list is NOT a shape but
a simple list of rectangles. If a pair or rects should be reversed (one is
"less" than the other in screen position), the USED pointer is returned to the
first of the pair, otherwise NULL is returned. */

static rectangle_ptr rects_wrong_order (memory_ref rect_list, pg_short_t qty)
{
	register rectangle_ptr		compare_r;
	register pg_short_t			ctr;

	if (qty < 2)
		return	NULL;
	
	compare_r = (rectangle_ptr) UseMemory(rect_list);

	for (ctr = qty - 1; ctr; ++compare_r, --ctr)
		if (compare_r->top_left.h > compare_r[1].top_left.h)
			return	compare_r;
	
	UnuseMemory(rect_list);
	
	return	NULL;
}


/* This function returns how many descrete rectangles are required to make if
src_rect and sect_rect were combined into a single shape. The function result
is the quantity, and output_list gets filled in with the list of new rects. */

static pg_short_t sect_rect_parts (rectangle_ptr src_rect, rectangle_ptr sect_rect,
		rectangle_ptr output_list)
{
	rectangle						sect_r;
	pg_short_t						out_qty;

	if (!pgSectOrTouchRect(src_rect, sect_rect, &sect_r)
		|| rect_fits_within(src_rect, sect_rect)) {

		pgBlockMove(src_rect, output_list, sizeof(rectangle));
		
		return	1;
	}

	if (rect_fits_within(sect_rect, src_rect)) {
		
		pgBlockMove(sect_rect, output_list, sizeof(rectangle));
		
		return	1;
	}

	out_qty = add_secting_sides(src_rect, &sect_r, output_list, 0);
	out_qty = add_secting_sides(sect_rect, &sect_r, output_list, out_qty);
	
	if (!pgEmptyRect(&sect_r)) {
	
		pgBlockMove(&sect_r, &output_list[out_qty], sizeof(rectangle));
		++out_qty;
	}

	return		out_qty;
}


/* add_secting_sides gets called by sect_rect_parts to add possibly four parts
to the rectangle list. The r_qty param is the count of output rectangles so far
and the function returns the new count. */

static pg_short_t add_secting_sides (rectangle_ptr src_rect, rectangle_ptr sect_rect,
		rectangle_ptr output_list, pg_short_t r_qty)
{
	register rectangle_ptr			comparison, output;
	register pg_short_t				qty;
	rectangle						source;

	qty = r_qty;
	source = *src_rect;
	comparison = sect_rect;
	output = output_list;
	output += qty;

	if (source.top_left.v < comparison->top_left.v) {
		
		pgBlockMove(&source, output, sizeof(rectangle));
		output->bot_right.v = comparison->top_left.v;
		
		source.top_left.v = comparison->top_left.v;

		++output;
		++qty;	
	}

	if (source.bot_right.v > comparison->bot_right.v) {

		pgBlockMove(&source, output, sizeof(rectangle));
		output->top_left.v = comparison->bot_right.v;
		
		source.bot_right.v = comparison->bot_right.v;

		++output;
		++qty;	
	}

	if (source.top_left.h < comparison->top_left.h) {
		
		pgBlockMove(&source, output, sizeof(rectangle));
		output->bot_right.h = comparison->top_left.h;
		
		++output;
		++qty;	
	}
	
	if (source.bot_right.h > comparison->bot_right.h) {

		pgBlockMove(&source, output, sizeof(rectangle));
		output->top_left.h = comparison->bot_right.h;
		
		++output;
		++qty;	
	}

	return	qty;
}


/* rect_fits_within returns TRUE if inside fits completely in outside. */

static pg_boolean rect_fits_within (rectangle_ptr outside, rectangle_ptr inside)
{
	register rectangle_ptr		out, in;
	
	out = outside;
	in = inside;
	
	if (in->top_left.v < out->top_left.v)
		return	FALSE;
	if (in->bot_right.v > out->bot_right.v)
		return	FALSE;
	if (in->top_left.h < out->top_left.h)
		return	FALSE;
	if (in->bot_right.h > out->bot_right.h)
		return	FALSE;
	
	return	TRUE;
}


/* rects_sect_internally gets called to see if there is a rect within walker
that intersects with some other part of the shape and, if so, returns the
rect number (1-based), or zero if none. The sect_rect param will get set to
the rectangle that intersects. */

static pg_short_t rects_sect_internally (shape_walk_ptr walker, rectangle_ptr sect_rect)
{
	register rectangle_ptr			r_list, sect_list;
	register pg_short_t				qty, r_num, ctr_to_end;
	
	qty = walker->num_rects;
	r_list = walker->rects;
	
	for (r_num = 1; r_num < qty; ++r_num, ++r_list) {
		
		sect_list = r_list;
		++sect_list;
		
		for (ctr_to_end = r_num + 1; ctr_to_end <= qty; ++sect_list, ++ctr_to_end)
			if (pgSectOrTouchRect(r_list, sect_list, NULL)) {
			
				*sect_rect = *sect_list;
				return	r_num;
			}
	}

	return	0;
}


/* omit_par_exclusion returns TRUE if the rectangle in question is attached to a paragraph
AFTER the line in question. */

static pg_boolean omit_par_exclusion (paige_rec_ptr pg, long line_offset, pg_short_t index)
{
	style_run_ptr		run;
	pg_boolean			result = FALSE;
	long				num_par_exclusions;
	
	if ((run = pgFindParExclusionRun(pg, line_offset, &num_par_exclusions)) != NULL) {
		
		if (num_par_exclusions)
			if (run->offset == line_offset) {
				
				++run;
				--num_par_exclusions;
		}

		while (num_par_exclusions) {
			
			if (run->style_item == index) {
				
				result = TRUE;
				break;
			}
			
			++run;
			--num_par_exclusions;
		}
		
		UnuseMemory(pg->par_exclusions);
	}
	
	return	result;
}
