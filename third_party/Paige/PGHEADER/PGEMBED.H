/* This is the PAIGE "insert anything" extension. This provides a slightly
higher-level interface to an otherwise complicated implementation of inserting
something that is not a real "text" item.  Copyright 1994-1995 by DataPak
Software, Inc.  All rights reserved. Software by "Gar."  

Fixes and additions by Board Member TRS/OITC (oitc@iu.net). Additions, etc. by "Tom" 
*/

#ifndef PGEMBED_H
#define PGEMBED_H

#include "Paige.h"
#include "pgGrafx.h"

#define CURRENT_EMBED_VERSION	5

//Format revision variables
#define EMBED_REVISION_1		2
#define EMBED_REVISION_2		3
#define EMBED_REVISION_3		4

#define EMBED_DEFAULT_SIZE		(sizeof(pg_char) * 2)

#define DEF_EMBED_HEIGHT		32
#define DEF_EMBED_WIDTH			32

/* Possible embedded types: */

enum {
	embed_rectangle,			// Plain rectangle (all platforms)
	embed_oval,					// Plain oval (all platforms)
	embed_roundrectangle,		// Round rectangle (all platforms)
	embed_arc,					// Arc angle (all platforms)
	embed_control,				// Control (such as a button), MAC ONLY this release
	embed_bitmap,				// Bit image (all platforms)
	embed_polygon,				// List of line co-ordinates (all platforms)
	embed_ole,					// OLE object (supported by APP only!)
	embed_mac_pict,				// Macintosh PicHandle (Mac only)
	embed_meta_file,			// Windows meta file (Windows only)
	embed_qt_component,			// QuickTime graphics component (PAIGE 3.0+)
	embed_alternate_char,		// Alternate character (all platforms)
	embed_user_data,			// Embed user-defined data (any length)
	embed_dynamic_string,		// Dynamically changing string or char
	embed_user_box,				// User-based box
	embed_os2_meta_file,		// OS/2 meta file (OS/2 only)
	embed_dibitmap,				// Windows Device-Independent Bitmap (Windows only)
	embed_wbitmap,				// Windows Device-Dependent Bitmap (Windows only)
	embed_gif_pict,				// Gif picture (all platforms)
	embed_jpeg_pict,			// JPEG picture (all platforms)
	embed_mac_vm_pict,			// Macintosh PicHandle as a memory_ref (Mac only)
	embed_qt_movie,				// Macintosh Quicktime movie (Mac only)
	embed_qt_flat_movie,		// Quicktime movie (Mac & Windows only)
	embed_time,					// Time - data contains time or zero for current; draw_flags contain display flags 
	embed_date,					// Date - data contains time or zero for current; draw_flags contain display flags 
	embed_cur_page_num,			// Page number - if user_refcon & 1 current page
	embed_cur_sect_num,			// Section number - if user_refcon & 1 current section
	embed_annotation,			// Annotation
	embed_bookmark_start,		// Bookmark start
	embed_bookmark_end,			// Bookmark end
	embed_footnote_ref,			// Footnote/Endnote reference
	embed_footnote_anchor,		// Footnote anchor
	embed_pg_link,				// A pg_ref (linked to a file)
	embed_url_image,			// Image (pathname or URL is the "data")
	embed_embed,				// An embeded emebded
	embed_horiz_rule,			// A horizontal rule
	embed_unsupported_object,	// Unsupported object (but we keep data around)
	embed_last_known,			// Last known standard embedded + 1
	embed_useritem = 1024		// Type we don't know about
};

#define LAST_KNOWN_EMBEDDED		embed_last_known - 1
#define EMBEDDED_SUBSET			256		// Special DSI subset

/* Command messages for callback function: */

#define EMBED_INIT				0x0000			/* Called after item read from file */
#define EMBED_DRAW				0x0001			/* Draw embed item */
#define EMBED_MEASURE			0x0002			/* Measure width of item */
#define EMBED_VMEASURE			0x0003			/* Measure vertical height of item */
#define EMBED_SWAP				0x0004			/* Swap out string data */
#define EMBED_CURSOR			0x0005			/* Mouse is over an embed_item */
#define EMBED_ACTIVATE			0x0006			/* Item is active */
#define EMBED_DEACTIVATE		0x0007			/* Item is deactive */
#define EMBED_MOUSEDOWN			0x0008			/* Click in item */
#define EMBED_MOUSEMOVE			0x0009			/* Moused moved after mousedown */
#define EMBED_MOUSEUP			0x000A			/* Mouse up after mousedown */
#define EMBED_DOUBLECLICK		0x000B			/* Item received double click */
#define EMBED_DESTROY			0x000C			/* Embed_ref being destroyed */
#define EMBED_COPY				0x000D			/* Embed_ref beging copied (only for NOT_SHARED_FLAG) */
#define EMBED_WRITE_DATA		0x000E			/* Called for writing custom data */
#define EMBED_READ_DATA			0x000F			/* Called for reading custom data */
#define EMBED_CLIPBOARD_READ	0x0011			/* Called after initializing an embed from the clipboard */
#define EMBED_CLIPBOARD_SAVE	0x0012			/* Called before saving the data to clipboard */
#define EMBED_LOAD_IMAGE		0x0013			/* Load image command */
#define EMBED_UNLOAD_IMAGE		0x0014			/* Unload the image (called by mem mgr) */
#define EMBED_PREPARE_IMAGE		0x0015			/* Prepare image for future loading */
#define EMBED_SAVE_IMAGE		0x0016			/* Save image to a file, return pathname */

/* Flags to modify the embed type behavior: */

#define USE_TEXT_HEIGHT			0x00010000		// Use text default for height
#define INCLUDE_EXTRA_SPACES	0x00020000		// Include spaces in text stream
#define KEPT_AROUND				0x00040000		// "Keep around" was declared

/* Flags that app can provide: */

#define NOT_SHARED_FLAG			0x00080000		// Embed_ref not shared in Copy, Paste
#define EMBED_CONTROL_FLAG		0x00100000		// Embed_ref acts like a control
#define SCRAP_READ_FLAG			0x00200000		// Reading the clipboard to get embed
#define ALIGN_BASELINE_FLAG		0x00400000		// Align bottom of object to baseline
#define ALIGN_CBASELINE_FLAG	0x00800000		// Align to vertical center of line
#define DRAW_TILED_FLAG			0x01000000		// Image draws "titled" (background images only)
#define REQUIRES_ACTIVATE_FLAG	0x02000000		// Click within ref redraws all
#define GROUP_TEXT_FLAG			0x04000000		// Text is grouped
#define REPRESENTS_OTHER_FLAG	0x08000000		// This ref represents an unsupported type
#define NO_FORCED_IDENTITY		0x10000000		// Do not force unique ID
#define ALIGN_BBASELINE_FLAG	0x20000000		// Align bottom of baseline
#define CLIP_EMBED_FLAG			0x40000000		// Clip drawing bounds when drawn
#define EMBED_EMOTICON_FLAG		0x80000000		// Embed is an emoticon

typedef memory_ref				embed_ref;
typedef struct pg_embed_rec 	pg_embed_rec;
typedef pg_embed_rec PG_FAR *	pg_embed_ptr;


typedef PG_FN_PASCAL (size_t, embed_callback) (paige_rec_ptr pg, pg_embed_ptr embed_ptr,
		long embed_type, short command, long user_refcon,
		void *param1, void *param2);

#define BOOKMARK_SIZE		44
#define ALT_SIZE			BOOKMARK_SIZE   // in-line data size option must be >= sizeof(pg_pic_embed) (28)


/* The universal "non-text" embedding record. You will pass a memory_ref containing
this structure to insert non-text items into the text stream, and you will also
get this record passed back to you in the callback functions: */


struct pg_pic_embed {
	pg_boolean		pict_transparent;		/* Picture scale (TRUE - transparent) */
	pg_boolean		scale_pict;				/* Picture scaled (TRUE - scale to frame) */
	pg_fixed		vert_scale;				/* Vertical scale */
	pg_fixed		horiz_scale;			/* Horizontal scale */
	rectangle		pict_frame;				/* Actual picture frame where this draws */
	long			mapping_mode;			/* Mapping mode (for Windows metafiles and OLE) */
	short			twips_width;			/* (Windows) original width in TWIPS or clipboard xExt */
	short			twips_height;			/* (Windows) original height in TWIPS or clipboard yExt */
	short			twips_gwidth;			/* (Windows) original picwgoal, RTF */
	short			twips_gheight;			/* (Windows) original puchgoal, RTF */
	short			meta_ext_x;				/* Original metafile xExt */
	short			meta_ext_y;				/* Original metafile yExt */
};

typedef struct pg_pic_embed 	pg_pic_embed;

struct pg_horiz_line {
	short			line_size;				/* Line size */
	short			line_shading;			/* Line shading (0 - standard, 1 - none, etc. other */
	short			line_alignment;			/* Alignment (use Justification modes) */
	short			line_width_type;		/* Width type */
	long			line_width;				/* Width (either abslolute in pixels or %) */
};
typedef struct pg_horiz_line 	pg_horiz_line;

enum {
	absolute_time,
	dynamic_time
};
#define GMT_DATE_TIME		0x8000
#define DAYLIGHT_DATE_TIME	0x4000

enum {
	short_date,
	long_date,
	abbrev_date
};

struct pg_date {
	pg_time_t		time;					/* Date/Time in paige seconds */
	short			time_type;				/* Date/Time format - absolute_time, dynamic_time */
	short			format;					/* Date format - short_date, long_date, abbrev_date */
};
typedef struct pg_date 	pg_date;


enum {
	short_time,
	long_time,
	short_24_time,
	long_12_time
};

struct pg_time {
	pg_time_t		time;					/* Date/Time in paige seconds */
	short			reserved;				/* reserved */
	short			format;					/* Time format - short_time, long_time, short_24_time, long_12_time */
};
typedef struct pg_time 	pg_time;


struct pg_embed_rec {
	short				version;			/* Version of embedded */
	short				reserved;			/* reserved */
	long				type;				/* Type of item embedded */
	long				width;				/* Drawing width, in pixels */
	long				minimum_width;		/* Minimum width */
	long				height;				/* Drawing height, in pixels */
	long				descent;			/* Distance bottom is below text line bottom */
	long				draw_flags;			/* Drawing attributes (see table below) */
	long				modifier;			/* Extra data for certain objects */
	long				empty_width;		/* Width of item when empty (applies to dynamic types). */
	long				alignment_flags;	/* Alignment (subrefs only) */
	short				top_extra;			/* Extra "whitespace" at the top */
	short				bot_extra;			/* Extra "whitespace" at the bottom */
	short				left_extra;			/* Extra "whitespace" at the left edge */
	short				right_extra;		/* Extra "whitespace" at the right edge */
	void PG_FAR			*data;				/* The item's data, if appropriate */
	memory_ref			embed_represent;	/* Optional embed that represents unsupported type */
	memory_ref			rtf_text_data;		/* Original preamble text from unsupported RTF import */
	union {
		pg_pic_embed	pict_data;			/* Special picture data */
		pg_horiz_line	line_info;			/* Special line data */
		pg_date			date_info;			/* Date info */
		pg_time			time_info;			/* Time info */
		pg_char			alt_data[ALT_SIZE + BOM_HEADER];	/* Alternate data (for ALT_SIZE or less chars) */
		pg_char			book_data[BOOKMARK_SIZE];/* Bookmark data */
	} uu;
	long				border_info;		/* Border control (revised for 3.01) */
	long				border_color;		/* The border color */
	long				shading;			/* Background color shading */
	style_info_ptr		style;				/* The style associated to this item */
	long				user_refcon;		/* What app put with this embed */
	long				user_data;			/* App can also use this field */
	long				style_refcon;		/* Refcon saved in styles */
	long				lowlevel_index;		/* Used by low level functions */
	long				used_ctr;			/* Count of shared access (maintained internally) */
};


// Draw flags
#define DATE_SHORT				0x00000000		// short date
#define DATE_ABBREV				0x00000001		// abbrev date
#define DATE_LONG				0x00000002		// long date
#define DATE_JULIAN				0x00000003		// julian date
#define TIME_SHORT				0x00000004		// short time hh:mm
#define TIME_LONG				0x00000005		// long time
#define DATE_TIME_GMT			0x10000000		// time or date in GMT

#define PAGE_NUM_DEFAULT		0x00000000		// Page-number format is the default
#define PAGE_NUM_DECIMAL		0x00000001		// Page-number format is decimal
#define PAGE_NUM_UP_ROMAN		0x00000002		// Page-number format is uppercase roman numeral
#define PAGE_NUM_LOW_ROMAN		0x00000003		// Page-number format is lowercase roman numeral
#define PAGE_NUM_UP_LETTER		0x00000004		// Page-number format is uppercase letter
#define PAGE_NUM_LOW_LETTER		0x00000005		// Page-number format is lowercase letter
#define PAGE_NUM_FMT_MASK		0x00000005		// Page-number format mask
#define PAGE_NUM_HEAD_MASK		0x00FF0000		// heading level is used to prefix a heading number to the page number
#define PAGE_NUM_SEP_MASK		0xFF000000		// heading level separator character


/* Measure-text struct given in callback function as param1: */

struct pg_embed_measure {
	style_walk_ptr			walker;			// Style information
	pg_char_ptr				text;			// "Text" pointer
	long					text_size;		// "Text" size, in bytes
	pg_short_t				slop;			// Extra amount for full-justify
	long PG_FAR				*positions;		// Width locations of "text" bytes
	short PG_FAR 			*types;			// Character types
	short					scale_verb;		// Whether or not to scale results
	short 					measure_verb;	// Measurement verb
	long					current_offset;	// Current offset to measure
	short					call_order;		// The call order
};

typedef struct pg_embed_measure pg_embed_measure;
typedef pg_embed_measure PG_FAR *pg_embed_measure_ptr;

/* Mouse movement(s) structure given in callback for mouse down, up and moved: */

struct pg_embed_click {
	t_select_ptr		first_select;		// Start of selection
	t_select_ptr		last_select;		// End of selection
	co_ordinate			point;				// Mouse point
	rectangle			bounds;				// Frame around the item
	short				modifiers;			// Modifier bits given in drag-select
};
typedef struct pg_embed_click pg_embed_click;
typedef pg_embed_click PG_FAR *pg_embed_click_ptr;


/* activate struct given in callback function as param1: */

struct pg_embed_activate {
	style_info_ptr 			style;			// Style information
	select_pair_ptr			text_range;		// Test range
	pg_char_ptr				text;			// "Text" pointer
	short					front_back_state;// "Normal" state
	short					perm_state;		// Permanent state
	pg_boolean				show_hilite;	// Show hilite
};
typedef struct pg_embed_activate pg_embed_activate;
typedef pg_embed_activate PG_FAR *pg_embed_activate_ptr;


/* The meta file struct (given to pgNewEmbedRef): */

struct metafile_struct {
	size_t				metafile;			/* Metafile data (HANDLE if Windows) */
	long				mapping_mode;		/* Mapping mode (Windows only) */
	short				x_ext;				/* Original x-extent */
	short				y_ext;				/* Original y-extent */
	rectangle			bounds;				/* Source bounding rect */
};
typedef struct metafile_struct metafile_struct;
typedef metafile_struct PG_FAR *metafile_ptr;


#define EMBED_TYPE_MASK	0x0000FFFF			/* Bits looked at in type field */
#define DUMMY_LEFT_EMBED '['				/* Char used to embed dummy text, left side */
#define DUMMY_RIGHT_EMBED ']'				/* Char used to embed dummy text, right side */

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (embed_ref) pgNewEmbedRef (pgm_globals_ptr mem_globals, long item_type,
		void PG_FAR *item_data, long modifier, long flags, pg_fixed vert_pos,
		long user_refcon, pg_boolean keep_around);
extern PG_PASCAL (void) pgEmbedDispose (embed_ref ref);
extern PG_PASCAL (pg_boolean) pgInsertEmbedRef (pg_ref pg, embed_ref ref, long position,
		short stylesheet_option, embed_callback callback, long callback_refcon,
		short draw_mode);
extern PG_PASCAL (void) pgSetEmbedRef (pg_ref pg, embed_ref ref, select_pair_ptr selection,
		short stylesheet_option, embed_callback callback, long callback_refcon,
		short draw_mode);
extern PG_PASCAL (embed_ref) pgFindNextEmbed (pg_ref pg, size_t PG_FAR *text_position,
		long match_refcon, long AND_refcon);
extern PG_PASCAL (embed_ref) pgGetExistingEmbed (pg_ref pg, long user_refcon);
extern PG_PASCAL (long) pgNumEmbeds (pg_ref pg, select_pair_ptr selection);
extern PG_PASCAL (embed_ref) pgGetIndEmbed (pg_ref pg, select_pair_ptr selection, size_t index,
		size_t PG_FAR *text_position, style_info_ptr associated_style);
extern PG_PASCAL (embed_ref) pgPtInEmbed (pg_ref pg, co_ordinate_ptr point, long PG_FAR *text_offset,
		style_info_ptr associated_style, pg_boolean do_callback);
extern PG_PASCAL (long) pgEmbedStyleToIndex (pg_ref pg, style_info_ptr embed_style);
extern PG_PASCAL (embed_ref) pgGetEmbedJustClicked (pg_ref pg, long drag_select_result);
extern PG_PASCAL (long) pgGetEmbedBounds (pg_ref pg, long index, select_pair_ptr index_range,
		rectangle_ptr bounds, short PG_FAR *vertical_pos, co_ordinate_ptr screen_extra);
extern PG_PASCAL (void) pgSetEmbedBounds (pg_ref pg, long index, select_pair_ptr index_range,
		rectangle_ptr bounds, short PG_FAR *vertical_pos, short draw_mode);
extern PG_PASCAL (embed_ref) pgFindSimilarEmbed (pg_ref pg, embed_ref ref, style_info_ptr embed_style);

/* File handler support: */

extern PG_PASCAL (pg_error) pgSaveAllEmbedRefs (pg_ref pg, file_io_proc io_proc, file_io_proc data_io_proc,
		size_t PG_FAR *file_position, file_ref filemap);
extern PG_PASCAL (pg_error) pgSaveEmbedRef (pg_ref pg, embed_ref ref, long element_info,
		file_io_proc io_proc, file_io_proc data_io_proc, size_t PG_FAR *file_position,
		file_ref filemap);
extern PG_PASCAL (void) pgInitEmbedProcs (pg_globals_ptr globals, embed_callback callback,
		app_init_read init_proc);
extern PG_PASCAL (pg_boolean) pgEmbedReadHandler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
		long PG_FAR *element_info, void PG_FAR *aux_data, size_t PG_FAR *unpacked_size);
extern PG_PASCAL (void) pgLoadImages (pg_ref pg, embed_callback image_callback, short wait_proc_verb);
extern PG_PASCAL (void) pgPackEmbedRef (pack_walk_ptr walker, memory_ref url_list, embed_ref ref, long fake_ref_id);
extern PG_PASCAL (memory_ref) pgUnpackEmbedRef (paige_rec_ptr pg, pack_walk_ptr walker, long PG_FAR *ref_id);

/* Default callback (when app passes NULL), also app can call this directly */

extern PG_PASCAL (size_t) pgDefaultEmbedCallback (paige_rec_ptr pg, pg_embed_ptr embed_ptr,
		long embed_type, short command, long user_refcon,
		void *param1, void *param2);
extern PG_PASCAL (void) pgEmbedStyleInit (paige_rec_ptr pg, style_info_ptr style,
			font_info_ptr font);

/* Low-level help: */

extern PG_PASCAL (void) pgDrawAlternateText (graf_device_ptr device, pg_embed_ptr item,
		rectangle_ptr draw_bounds, short optional_pointsize);

/* Import help: */

extern PG_PASCAL (pg_boolean) pgInsertEmbed (pg_ref pg, embed_ref ref,
		short stylesheet_option, embed_callback callback, long callback_refcon,
		font_info_ptr font, style_info_ptr style, short draw_mode);

extern PG_PASCAL (long) pgInitEmbedStyleInfo (paige_rec_ptr pg, long position, embed_ref ref,
	short stylesheet_option, embed_callback callback, long callback_refcon,
	style_info_ptr style, style_info_ptr mask, font_info_ptr font,
	font_info_ptr font_mask, pg_boolean will_be_inserted);

extern PG_PASCAL (memory_ref) pgNewImageRecord (pg_ref pg, pg_url_image_ptr image, embed_callback callback, long check_to);
extern PG_PASCAL (void) pgInvalEmbedRef (pg_ref pg, long position, pg_embed_ptr embed_ptr, short draw_mode);

// THE FOLLOWING IS NOT SUPPORTED UNLESS ANNOUNCED OTHERWISE:

extern PG_PASCAL (void) pgSetEmbedBorders (embed_ref ref, long border_info, long color, long shading,
		pg_boolean transparent);
extern PG_PASCAL (void) pgDrawEmbedBorders(paige_rec_ptr pg, pg_embed_ptr embed, rectangle_ptr draw_bounds);

#ifdef __cplusplus
	}
#endif

#endif
