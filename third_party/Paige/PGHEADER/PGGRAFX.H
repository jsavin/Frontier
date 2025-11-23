/* This header contains miscellaneous graphics utilities for platform-independent
drawing. The functions are mapped to code in pgGrafx.c.  */

#ifndef PGGRAFX_H
#define PGGRAFX_H

#include "Paige.h"
#include "pgFiles.h"
#include "pgEmbed.h"

#ifdef MAC_PLATFORM
#include <Palettes.h>
#endif

/* Types that can be given to object drawing (or contained in pictures): */

enum {
	object_rect,
	object_oval,
	object_roundrect,
	object_arc,

/* The following are only in pict_ref data: */
	
	object_poly,
	object_line,
	object_bitmap,
	object_style,
	object_font,
	object_pen_color,
	object_pensize,
	object_text_color,
	object_bk_color,
	object_clip_rgn,
	object_text
};

/* Op codes given to pgDrawObject (also saved in pictures): */

enum {
	set_opcode,
	frame_opcode,
	paint_opcode,
	erase_opcode,
	copy_opcode,
	copy_or_opcode,
	copy_xor_opcode
};


enum {
	pat_black,
	pat_dk_gray,
	pat_gray,
	pat_lt_gray,
	pat_white
};

typedef memory_ref		poly_ref;			/* Contains a portable polygon */
typedef memory_ref		bitmap_ref;			/* Contains a portable bitmap */
typedef memory_ref		picture_ref;		/* Contains collection of objects */

/* Platform-independent polygon shape: */

struct pg_poly_rec {
	short			width;		// Width of polygon, in pixels
	short			height;		// Height of polygon, in pixels
	short			rsrv;		// (Here for future enhancement)
	short			num_points;	// Number of points that follow
	co_ordinate		points[1];	// One or more points for the drawing	 
};
typedef struct pg_poly_rec pg_poly_rec;
typedef pg_poly_rec PG_FAR *pg_poly_ptr;


struct picture_op_rec {
	short			type;		// Low-byte object type, High-byte attribute flags
	pg_short_t		opcode;		// Operation code
	long			modifier;	// Additional object type parameter
	rectangle		frame;		// Target drawing rect
	long			data_size;	// Size of data (including this header + data that follows)
};
typedef struct picture_op_rec picture_op_rec;
typedef picture_op_rec PG_FAR *picture_op_ptr;

/* High-byte op-code attributes: */

#define OP_CODE_MASK				0x00FF	// Reveals op code

#define SUPPRESS_SCALE_VERTICAL		0x8000	// Do not scale vertically
#define SUPPRESS_SCALE_HORIZONTAL	0x4000	// Do not scale horizontally
#define USE_LOCAL_ORIGIN			0x2000	// Use own origin for scaling
#define FORCE_RIGHT_POINTS			0x1000	// Force right to match target frame
#define FORCE_LEFT_POINTS			0x0800	// Force left to match target frame
#define FORCE_TOP_POINTS			0x0400	// Force top to match target frame
#define FORCE_BOT_POINTS			0x0200	// Force bottom to match target frame

/* Color, pattern variable: */

#define COLORPAT_DOTTED			0x01000000

/* Platform-independent picture (collection of drawing types): */

struct picture_header {
	short			width;		// Picture width
	short			height;		// Picture height
	pg_short_t		flags;		// Misc. properties
	short			num_ops;	// Number of op code records (OP CODES FOLLOW)
};
typedef struct picture_header picture_header;
typedef picture_header PG_FAR *picture_header_ptr;


/* Platform-independent bitmap image: */

#define CUR_BIT_MAP_VERSION	0x0100

struct pg_bitmap_rec {
	short			version;	// Version number
	short			pack_type;	// Future packing format
	pg_fixed		h_res;		// Horiz. resolution (ppi)
	pg_fixed		v_res;		// Vert. resolution (ppi)
	rectangle		origin;		// Bitmap origin rectangle
	short			width;		// Width of bit image, in pixels
	short			height;		// Height of bit image, in pixels
	short			pixel_size;	// Number of bits per pixel (possible values are 1, 2, 4, 8, 16, and 32 bits per pixel)
	short			row_bytes;	// Number of bytes in a row (row_bytes = ((pixel_size * width + 31) >> 4) * 4;)
	memory_ref		palette;	// Array of RGB color entries or MEM_NULL if B&W or direct pixels (ie 16 or 32)
	long			ref_con;	// Can be used by app
	pg_char			bits[2];	// Byte data follows 
};
typedef struct pg_bitmap_rec pg_bitmap_rec;
typedef pg_bitmap_rec PG_FAR *pg_bitmap_ptr;


#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (void) pgTextOut (graf_device_ptr port, co_ordinate_ptr top_left_pt,
		pg_char_ptr text, short length, short optional_point_size);
extern PG_PASCAL (short) pgTextWidth (graf_device_ptr port, pg_char_ptr text, short length);
extern PG_PASCAL (void) pgLineDraw (graf_device_ptr port, co_ordinate_ptr start_pt,
		co_ordinate_ptr end_pt, long color, short pen_size);
extern PG_PASCAL (void) pgFrameObject (graf_device_ptr port, rectangle_ptr frame,
		long modifier, short object_verb, short pen_size);
extern PG_PASCAL (void) pgFrameRect (graf_device_ptr port, rectangle_ptr frame,
		long color, short pen_size);
extern PG_PASCAL (void) pgPaintObject (graf_device_ptr port, rectangle_ptr frame,
		long modifier, short object_verb, long color);
extern PG_PASCAL (void) pgFramePoly (graf_device_ptr port, poly_ref poly,
		rectangle_ptr target_frame, short pen_size);
extern PG_PASCAL (void) pgFillPoly (graf_device_ptr port, poly_ref poly,
		rectangle_ptr target_frame);

/* Bitmap handling: */

extern PG_PASCAL (void) pgInitBitMap(graf_device_ptr device, rectangle_ptr dest_rect, short pixel_size, memory_ref palette, long ref_con, short background, bitmap_ref bit_map);
extern PG_PASCAL (void) pgCopyBits (graf_device_ptr port, pg_bitmap_ptr bitmap,
		rectangle_ptr source_rect, rectangle_ptr target_rect, short draw_mode);

/* Bitmap utilities: */

#ifdef MAC_PLATFORM
extern PG_PASCAL (CTabHandle) pgColorValues2CTab(memory_ref palette);
extern PG_PASCAL (memory_ref) pgCTab2ColorValues(pgm_globals_ptr mem_globals, CTabHandle cTab);
extern PG_PASCAL (PaletteHandle) pgColorValues2Palette(memory_ref palette);
extern PG_PASCAL (memory_ref) pgPalette2ColorValues(pgm_globals_ptr mem_globals, PaletteHandle palette);
#endif

/* Polygon and picture utilities: */

extern PG_PASCAL (void) pgOpenPoly (pgm_globals_ptr mem_globals, rectangle_ptr frame,
		graf_device_ptr drawing_device);
extern PG_PASCAL (poly_ref) pgClosePoly (graf_device_ptr drawing_device);
extern PG_PASCAL (void) pgPolyBounds (poly_ref poly, long PG_FAR *width, long PG_FAR *height);
extern PG_PASCAL (void) pgOpenPicture (pgm_globals_ptr mem_globals, rectangle_ptr frame,
		graf_device_ptr drawing_device);
extern PG_PASCAL (picture_ref) pgClosePicture (graf_device_ptr drawing_device);
extern PG_PASCAL (void) pgDrawPicture (graf_device_ptr drawing_device,
		picture_ref picture, rectangle_ptr pict_frame, pg_boolean scale_h,
		pg_boolean scale_v, pg_boolean clip_to_frame);
extern PG_PASCAL (void) pgPictureBounds (picture_ref picture, long PG_FAR *width, long PG_FAR *height);

/* File transfer: */

extern PG_PASCAL (void) pgPackPoly (pack_walk_ptr walker, poly_ref poly);
extern PG_PASCAL (void) pgPackBitMap (pack_walk_ptr walker, bitmap_ref b_map);
extern PG_PASCAL (poly_ref) pgUnpackPoly (pack_walk_ptr walker);
extern PG_PASCAL (bitmap_ref) pgUnpackBitMap (pack_walk_ptr walker);

/* Miscellaneous: */

extern PG_PASCAL (void) pgLongToRGB (long colorref, color_value_ptr color);
extern PG_PASCAL (long) pgRGBToLong (color_value_ptr color);
extern PG_PASCAL (long) pgGetGrayScale (color_value_ptr bk_color, short percent);
extern PG_PASCAL (generic_var) pgSetClipRect (graf_device_ptr device,
		rectangle_ptr rect, rectangle_ptr intersect_with);
extern PG_PASCAL (void) pgRestoreClipRgn (graf_device_ptr device, generic_var rgn);

extern PG_PASCAL (void) pgGetForeColor (graf_device_ptr device, color_value_ptr pg_color);
extern PG_PASCAL (void) pgGetBackColor (graf_device_ptr device, color_value_ptr pg_color);
extern PG_PASCAL (void) pgSetForeColor (graf_device_ptr device, const color_value_ptr pg_color);
extern PG_PASCAL (void) pgSetBackColor (graf_device_ptr device, const color_value_ptr pg_color);


#ifdef __cplusplus
	}
#endif

#endif

