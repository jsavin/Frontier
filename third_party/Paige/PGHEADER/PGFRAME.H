/* Header file for frames. */

#ifndef PGFRAME_H
#define PGFRAME_H

#include "Paige.h"
#include "pgFiles.h"
#include "pgEmbed.h"

/* Frame data types (standard) */

enum {
	frame_embed,
	frame_subref,
	frame_user
};

/* Callback verbs */

#define FRAME_DRAW_VERB		1
#define FRAME_DRAW_SELECT	2
#define FRAME_DESTROY_VERB	3
#define FRAME_MOVE_VERB		4
#define FRAME_RESIZE_VERB	5

/* Flags */

#define FRAME_ATTACHED_TO_PAR	0x00000001
#define FRAME_SELECTED			0x00000002
#define FRAME_WAS_SELECTED		0x00000004
#define FRAME_ALIGN_LEFT		0x00000080
#define FRAME_ALIGN_CENTER		0x00000100
#define FRAME_ALIGN_RIGHT		0x00000200
#define FRAME_NO_BACKGROUND		0x00000400
#define FRAME_READ_BIT			0x00004000

#define FRAME_ANY_ALIGNMENT		(FRAME_ALIGN_LEFT | FRAME_ALIGN_CENTER | FRAME_ALIGN_RIGHT)

struct pg_frame {
	pg_char			name[32]; /* Name of the frame */
	long			type;			/* Frame type */
	long			flags;			/* Special flags */
	long			position;		/* Position (if attached to par) */
	long			callback;		/* The callback function */
	long			border_info;	/* Borders */
	long			shading;		/* Shading */
	rectangle		bounds;			/* Bounding area (borders respect this) */
	rectangle		wrap; 			/* Bounding area for wrap purposes (can be different than bounds */
	generic_var		data;			/* The data component */
};

typedef struct pg_frame pg_frame;
typedef pg_frame PG_FAR *pg_frame_ptr;

typedef PG_FN_PASCAL(long, frame_callback) (paige_rec_ptr pg, pg_frame_ptr frame,
			rectangle_ptr target, co_ordinate_ptr offset_extra, short verb);


#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL(void) pgInsertFrame (pg_ref pg, pg_frame_ptr frame, embed_callback embed_callback, short draw_mode);
extern PG_PASCAL (void) pgGetFrame (pg_ref pg, pg_short_t frame_num, pg_frame_ptr frame);
extern PG_PASCAL (void) pgSetFrame (pg_ref pg, pg_short_t frame_num, pg_frame_ptr frame,
			short draw_mode);
extern PG_PASCAL (void) pgSelectFrame (pg_ref pg, pg_short_t frame_num, pg_boolean draw_hilite);
extern PG_PASCAL (void) pgDeselectFrames (pg_ref pg, pg_boolean draw_hilite);
extern PG_PASCAL (long) pgGetSelectedFrames (pg_ref pg, memory_ref PG_FAR *selectlist);
extern PG_PASCAL (void) pgDeleteFrames (pg_ref pg, short draw_mode);
extern PG_PASCAL (void) pgGetFrameDisplay (pg_ref pg, pg_short_t frame_num, rectangle_ptr bounds,
			rectangle_ptr wrap);
extern PG_PASCAL (void) pgDisposeFrames (paige_rec_ptr pg);
extern PG_PASCAL (long) pgStandardFrameCallback (paige_rec_ptr pg, pg_frame_ptr frame,
			rectangle_ptr target, co_ordinate_ptr offset_extra, short verb);
extern PG_PASCAL (void) pgPackFrame (paige_rec_ptr pg, pack_walk_ptr walker, memory_ref frameref);
extern PG_PASCAL (memory_ref) pgUnpackFrame (paige_rec_ptr pg, pack_walk_ptr walker);
extern PG_PASCAL (void) pgDrawAllFrames (paige_rec_ptr pg, short verb);

#ifdef __cplusplus
	}
#endif

#endif
