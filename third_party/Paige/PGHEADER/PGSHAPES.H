/* Header file for pgShapes.c.
This file handles all the (complex?) "shape" stuff.  No machine-specific
code is in here.  */

#ifndef PGSHAPES_H
#define PGSHAPES_H

#include "Paige.h"

#define BOUNDS_OFFSET		0		/* Offset from start of shape to bounds */

/* Dimensional flag bits:  */

#define MULTIPLE_RECT_DIMENSION			0x0001		/* Shape has more than 1 rect */
#define MUTIPLE_WIDTH_DIMENSION			0x0002		/* Rects don't all have same width */
#define NOT_STACKED_DIMENSION			0x0004		/* All tops are not >= previous bottoms */
#define OVERLAP_DIMENSION				0x0008		/* One or more rects overlap */
#define UNION_DIMENSION					0x0010		/* One or more rects have connecting rects */

#define COMPLEX_DIMENSION	(MUTIPLE_WIDTH_DIMENSION | NOT_STACKED_DIMENSION | OVERLAP_DIMENSION | UNION_DIMENSION)
#define ALL_DIMENSIONS	(MULTIPLE_RECT_DIMENSION | MUTIPLE_WIDTH_DIMENSION | NOT_STACKED_DIMENSION | OVERLAP_DIMENSION | UNION_DIMENSION)

#define BOTTOMLESS_VALUE	0x7FFFFFFF		/* Virtually bottomless */

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (short) pgShapeDimension (shape_ref the_shape);
extern PG_PASCAL (void) pgResetBounds (shape_ref the_shape);
extern PG_PASCAL (short) pgMaxRectInShape (shape_ref the_shape, rectangle_ptr rect,
		long minimum_h, long PG_FAR *r_num, co_ordinate_ptr offset_extra);
extern PG_PASCAL (pg_short_t) pgExcludeRectInShape (paige_rec_ptr pg, rectangle_ptr rect,
		long minimum_width, long PG_FAR *lowest_exclude, long line_offset, memory_ref result_ref);
extern PG_PASCAL (pg_short_t) pgGetWrapRect (paige_rec_ptr pg, size_t r_num, co_ordinate_ptr offset_extra);
extern PG_PASCAL (pg_short_t) pgGetSectWrapRect (paige_rec_ptr pg, rectangle_ptr r_sect,
		pg_short_t PG_FAR *r_num, co_ordinate_ptr repeat_offset);
extern PG_PASCAL (void) pgScaleShape (paige_rec_ptr pg, shape_ref shape, short numerator, short denomintor);
extern PG_PASCAL (void) pgVirtualShapeBounds (paige_rec_ptr pg, shape_ref the_shape, rectangle_ptr bounds);
extern PG_PASCAL (void) pgSetVirtualShapeRect (paige_rec_ptr pg, shape_ref the_shape, const rectangle_ptr rect);

#define pgNewShape(mem_globals)		MemoryAlloc(mem_globals, sizeof(rectangle), SIMPLE_SHAPE_QTY, 8)
#define pgDisposeFailedShape(the_shape)	DisposeFailedMemory(the_shape)


#ifdef __cplusplus
	}
#endif


#endif
