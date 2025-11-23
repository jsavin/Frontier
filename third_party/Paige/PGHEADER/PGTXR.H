/* Paige Technology, copyright 1993-1995 by DataPak Software, Inc.
This header file defines all the components for the built-in import-export mechanism,
including the generic Import and Export classes and default member functions for file
transfer. */

#ifndef PGTXR_H
#define PGTXR_H

#include "Paige.h"

#define MINIMUM_RTF_VERSION		0x00010015 /* Minimum PAIGE version for RTF */

/* Standard file types supported automatically: */

enum {
	pg_unknown_type,	// Unknown file type
	pg_text_type,		// Standard ASCII text
	pg_rtf_type,		// RTF format
	pg_paige_type,		// Standard PAIGE file type
	pg_html_type,		// HTML type
	pg_dsi_type			// Type reserved for DSI
};

typedef void PG_FAR * c_import_filter;
typedef void PG_FAR * c_export_filter;

/* Translation filter structure to and from a pg_ref */

struct pg_translator {
	memory_ref			data;				// Data transferred (read) or to-transfer (write)
	memory_ref			stylesheet_table;	// Contains list of possible stylesheets
	long				bytes_transferred;	// Number of bytes in buffer
	long				total_text_read;	// Total transferred to-moment
	long				cache_begin;		// Beginning file offset (if cache enabled)
	style_info			format;				// Style(s) and character format of text
	par_info			par_format;			// Paragraph format(s) of the text
	font_info			font;				// Font applied to this text
	pg_doc_info			doc_info;			// General document information
	pg_hyperlink		hyperlink;			// Hypertext link
	pg_hyperlink		hyperlink_target;	// Target hypertext link
	unsigned long		flags;				// Attributes of last transfer
	pg_boolean			format_changed;		// Set to TRUE if format is different than last txr
	pg_boolean			par_format_changed;	// Set to TRUE if par format different than last txr
	pg_boolean			font_changed;		// Set to TRUE if font different than last txr
	pg_boolean			doc_info_changed;	// Set to TRUE if document info changed since last txr
	pg_boolean			hyperlink_changed;	// Set to TRUE if a hypertext link gets added
	pg_boolean			hyperlink_target_changed;  // Set to true if hyperlink target changes
	long				par_format_verb;	// Verb that indicates how to apply par_format
	long				par_format_mark;	// Used with par_mark and verb
// The following items used only by export class:
	long				data_type;			// Type of data being exported (can by pict, etc.)
	long 				data_param1;		// Additional data for data type */
	long 				data_param2;		// Additional data for data type */
};

/* Verbs par_format_verb: */

enum {
	par_normal,				/* Apply normal (to current paragraph, or previous if last is CR, and on) */
	par_mark,				/* Apply from mark to end and beyond */
	par_forward				/* Apply only to next position and forward */
};

/* Data types for export: */

enum {
	export_text_type,
	export_embed_type
};

/* Character types out: */

enum {
	ascii_char_out,
	delimiter_char_out,
	command_char_out
};


typedef struct pg_translator pg_translator;
typedef pg_translator PG_FAR *pg_translator_ptr;

/* Stylesheet table record: */

struct style_table_entry {
	pg_char				name[FONT_SIZE];
	style_info			style;
	font_info			font;
	par_info			par;
	pg_boolean			has_styles;
	pg_boolean			has_par_styles;
	pg_boolean			has_font;
	short				style_id;
};

typedef struct style_table_entry style_table_entry;
typedef style_table_entry PG_FAR *style_entry_ptr;

struct style_table {
	short				style_id;
	short				rsrv;
	long				index;
};

typedef struct style_table style_table;
typedef style_table PG_FAR *style_table_ptr;

struct pg_errlog_rec {
	long			code;			/* The error code */
	long			refcon;			/* App can use this */
	pg_ref			pg;				/* Target pg_ref for which this was imported */
	select_pair		offsets;		/* Text range affected */
	pg_char			message[64];	/* Misc. message can be used here. */
};

typedef struct pg_errlog_rec pg_errlog_rec;
typedef pg_errlog_rec PG_FAR *pg_errlog_ptr;

/* Error log codes: */

#define TABLE_IN_TABLE_ERROR	0x0001			/* Import has table within a table cell */
#define HEADER_FOOTER_ERROR		0x0002			/* Import has headers or footers */
#define FRAME_ERROR				0x0003			/* "FRAME" tag present */
#define FORMS_ERROR				0x0004			/* "FORM" tag present */
#define IMAGE_WRAP_ERROR		0x0005			/* Unsupported image or table wrap error */
#define IMAGE_CONVERT_ERROR		0x0006			/* Cannot convert image (bad type) */
#define IMAGE_BACKGROUND_ERROR	0x0007			/* Background image not supported */
#define APPLET_ERROR			0x0008			/* Applet tag (unsupported) */
#define UNSUPPORTED_SYMBOL		0x0009			/* Unsupported symbol name */
#define UNSUPPORTED_COLOR		0x000A			/* Unsupported color name */
#define HARD_CODED_CHAR_WARNING	0x000B			/* &#xx char hard coded (may not display) */
#define UNSUPPORTED_FONT		0x000C			/* Unsupported font (made best guess) */
#define IMAGE_NOTFOUND_ERROR	0x000D			/* Cannot find image file */
#define IMAGE_UNSUPPORT_ERROR	0x000E			/* Image type unsupported. */
#define IMAGE_MAP_ERROR			0x000F			/* Image map unsupported */

/* Special errors for HTML Export: */

#define POINT_MISMATCH_ERROR	0x0020			/* Pointsize might not be exact */
#define EMPTY_LINK_ERROR		0x0021			/* A source link has no URL */
#define SEARCH_LINK_ERROR		0x0022			/* Target link's search string not found */

/* Flag settings for the translator record: */

#define TRANSFER_NOT_TEXT		0x00000001L	/* Data transfer is not text */
#define TRANSFER_PAR_END		0x00000002L	/* Ending char ends paragraph */
#define TRANSFER_WAS_DIRECT		0x00000004L	/* Data transferred directly (no insert(s) required */


/* Import preparation flags: */

#define IMPORT_TEXT_FLAG				0x00000001L	/* Import raw text */
#define IMPORT_TEXT_FORMATS_FLAG		0x00000002L	/* Import text formats */
#define IMPORT_PAR_FORMATS_FLAG			0x00000004L	/* Import paragraph formats */
#define IMPORT_PAGE_INFO_FLAG			0x00000008L	/* Import page info (do not necessarily apply) */
#define IMPORT_CONTAINERS_FLAG			0x00000010L	/* Import container boxes */
#define IMPORT_HEADERS_FLAG				0x00000020L	/* Import headers */
#define IMPORT_FOOTERS_FLAG				0x00000040L	/* Import footers */
#define IMPORT_FOOTNOTES_FLAG			0x00000080L	/* Import footnotes */
#define IMPORT_EMBEDDED_OBJECTS_FLAG	0x00000100L	/* Import recognized embed_refs */
#define IMPORT_PAGE_GRAPHICS_FLAG		0x00000200L	/* Import page-anchored pictures */
#define IMPORT_STYLESHEETS_FLAG			0x00000400L	/* Import defined stylesheets */
#define IMPORT_HYPERTEXT_FLAG			0x00000800L	/* Import hypertext links (or index, toc if RTF) */
#define IMPORT_HIDDENTEXT_FLAG			0x00001000L	/* Import hidden text style */
#define IMPORT_NO_VISAREA				0x00002000L	/* Omit vis area import */
#define IMPORT_BKCOLOR_FLAG				0x00004000L	/* Import background color (usually with HTML) */
#define IMPORT_BROWSER_PAGE_FLAG		0x00008000L	/* Set doc to non-page break for HTML */
#define IMPORT_INDEX_FLAG				0x00010000L	/* Import index markers (RTF) */
#define IMPORT_TOC_FLAG					0x00020000L	/* Import TOC markers (RTF) */
// boggy-doggy was here!
#define IMPORT_TABLES_FLAG              0x00040000L /* Import tables (HTML fornow) */
#define IMPORT_HTTP_IMAGES_FLAG         0x00080000L /* Import non-local images (HTTP) */
#define IMPORT_SIG_FORMAT_FLAG          0x00100000L /* Import inline sig (HTML) style */
//
#define APPLY_PAGE_DIMENSIONS			0x02000000L	/* Apply paper and page sizes */
#define IMPORT_CACHE_FLAG				0x04000000L	/* Import cache (paging) method */
#define IMPORT_EVERYTHING_FLAG			0x00FFFFFFL	/* Import everything you can */

/* Import capability bits: */

#define IMPORT_TEXT_FEATURE				0x00000001L	/* Can import raw text */
#define IMPORT_TEXT_FORMATS_FEATURE		0x00000002L	/* Can import text formats */
#define IMPORT_PAR_FORMATS_FEATURE		0x00000004L /* Can import paragraph formats */
#define IMPORT_PAGE_INFO_FEATURE		0x00000008L	/* Can import page dimensions */
#define IMPORT_CONTAINERS_FEATURE		0x00000010L	/* Can import containers */
#define IMPORT_HEADERS_FEATURE			0x00000020L	/* Can import headers */
#define IMPORT_FOOTERS_FEATURE			0x00000040L	/* Can import footers */
#define IMPORT_FOOTNOTES_FEATURE		0x00000080L	/* Can import footnotes */
#define IMPORT_EMBEDDED_OBJECTS_FEATURE	0x00000100L	/* Can import standard, supported embed_refs */
#define IMPORT_PAGE_GRAPHICS_FEATURE	0x00000200L	/* Can import graphics anchored to page */
#define IMPORT_HYPERTEXT_FEATURE		0x00000400L	/* Can import hypertext or index, toc. */
#define IMPORT_SERIAL_SETUP				0x01000000L	/* Setup serial byte read */
#define IMPORT_PARS_EXTERNAL			0x02000000L	/* Paragraphs are only set externally (not via importer) */
#define IMPORT_EXTEND_OUTPUT			0x04000000L	/* Extend output buffer as necessary */
#define IMPORT_NO_LEADING_SPACES		0x08000000L	/* Do not allow leading spaces after CR */
#define IMPORT_XFRAMES_FLAG				0x10000000L /* Import "frame" images (** reserved for DSI ** */
#define IMPORT_CACHE_CAPABILITY			0x40000000L	/* Can import with "cache" feature */
#define IMPORT_MERGED_STYLES_FLAG		0x80000000L	/* Force named PAR styles to match character */

/* Export preparation flags: */

#define EXPORT_TEXT_FLAG				0x00000001L	/* Export raw text */
#define EXPORT_TEXT_FORMATS_FLAG		0x00000002L	/* Export text formats */
#define EXPORT_PAR_FORMATS_FLAG			0x00000004L	/* Export paragraph formats */
#define EXPORT_PAGE_INFO_FLAG			0x00000008L	/* Export page info */
#define EXPORT_CONTAINERS_FLAG			0x00000010L	/* Export container boxes */
#define EXPORT_HEADERS_FLAG				0x00000020L	/* Export headers */
#define EXPORT_FOOTERS_FLAG				0x00000040L	/* Export footers */
#define EXPORT_FOOTNOTES_FLAG			0x00000080L	/* Export footnotes */
#define EXPORT_EMBEDDED_OBJECTS_FLAG	0x00000100L	/* Export recognized embed_refs */
#define EXPORT_PAGE_GRAPHICS_FLAG		0x00000200L	/* Export page-anchored pictures */
#define EXPORT_STYLESHEETS_FLAG			0x00000400L	/* Export defined stylesheets */
#define EXPORT_HYPERTEXT_FLAG			0x00000800L	/* Export hypertext links (or index, toc). */
#define INCLUDE_LF_WITH_CR				0x02000000L	/* Add LF with CR if not already */
#define EXPORT_CACHE_FLAG				0x04000000L	/* Export cached file */
#define EXPORT_UNICODE_FLAG				0x08000000L	/* Write text as UNICODE */

#define EXPORT_EVERYTHING_FLAG			0x00FFFFFFL	/* Export everything you can */

/* Export capability bits: */

#define EXPORT_TEXT_FEATURE				0x00000001L	/* Can Export raw text */
#define EXPORT_TEXT_FORMATS_FEATURE		0x00000002L	/* Can Export text formats */
#define EXPORT_PAR_FORMATS_FEATURE		0x00000004L	/* Can Export paragraph formats */
#define EXPORT_PAGE_INFO_FEATURE		0x00000008L	/* Can Export page dimensions */
#define EXPORT_CONTAINERS_FEATURE		0x00000010L	/* Can Export containers */
#define EXPORT_HEADERS_FEATURE			0x00000020L	/* Can Export headers */
#define EXPORT_FOOTERS_FEATURE			0x00000040L	/* Can Export footers */
#define EXPORT_FOOTNOTES_FEATURE		0x00000080L	/* Can Export footnotes */
#define EXPORT_EMBEDDED_OBJECTS_FEATURE	0x00000100L	/* Can Export standard, supported embed_refs */
#define EXPORT_PAGE_GRAPHICS_FEATURE	0x00000200L	/* Can Export graphics anchored to page */
#define EXPORT_HYPERTEXT_FEATURE		0x00000400L	/* Can Export hypertext (or index, toc, etc. */
#define EXPORT_CACHE_FEATURE			0x00100000L	/* Can Export cache method */
#define EXPORT_UNICODE_FEATURE			0x00200000L	/* Can export UNICODE */

#define EXPORT_SERIAL_SETUP				0x01000000L	/* Setup serial byte buffer */
#define EXPORT_STYLE2CR					0x10000000L /* Text should always include CR even if style changes */

/* Miscellaneous: */

#define MAX_TEXT_BUFFER					4096		/* Max buffer used by filter in serial mode */
#define TRANSLATOR_BUFFER_SIZE			4096		/* Default buffer size */
#define UNKNOWN_POSITION				-1			/* Used for unknown file position */
#define PICT_APPEND_SIZE				512			/* Size to append memory_ref when reading pict */
#define HIGH_CHARS_BASE					128
#define HIGH_CHARS_QTY					128
#define N_A								0x00	/* Char not supported */
#define MSWORD_TOPBOTTOM_MARGIN			72		/* Default top and bottom MS Word margins (point) */
#define MSWORD_LEFTRIGHT_MARGIN			90		/* Default left and right MS Word margins (point) */
#define KIND_STR_SIZE					4		/* Max size of filetype string */

/* RTF Character Constants: */

#define RTF_COMMAND_CHAR		0x5C
#define RTF_GROUPBEGIN_CHAR		0x7B
#define RTF_GROUPEND_CHAR		0x7D
#define RTF_HEX_CHAR			0x27
#define RTF_STAR_CHAR			0x2A
#define RTF_COLON				0x3A

#define TAG_COMMAND_CHAR		(pg_char)'<'
#define ENDTAG_COMMAND_CHAR		(pg_char)'>'
#define NTAG_COMMAND_CHAR		0x2F

#define TABLE_TERMINATOR		0x3B
#define MAX_COMMAND_BYTES		32		/* Maximum bytes for any command */

/* Special characters: */

#define NON_BREAKING_SPACE_CHAR	'~'
#define NON_REQUIRED_HYPHEN_CHAR	'-'
#define NON_BREAKING_HYPHEN	'_'
#define NEGATIVE_SIGN_CHAR '-'

/* Internal table tokens (used by me, not in RTF stream): */

#define TABLE_TERMINATOR_CHAR	0x7C

/* Misc. for HTML */

#define HTML_PARAM_MASK		0x3FFF
#define HTML_PARAM_NUMERIC	0x4000

/* Import mode record */

struct pg_import_rec {
	long				target_pos;		 /* Target import position once done. */
	long				t_length;		 /* Text length so far */
	long				cache_pos;	 	 /* File position for cache-read text */
	long				last_style_position; /* Text offset of last style change */
	pg_short_t			last_style_item; /* Last style_info item applied */
	pg_short_t			last_par_item;	 /* Last paragraph item applied */
	short				last_font_index; /* Last font item applied */
	short				last_was_embed;	 /* Last insert was an embed_ref */
	memory_ref			t_blocks;		 /* Text blocks solely for importing */
	memory_ref			t_style_run;	 /* Style run used for importing */
	memory_ref			par_style_run;	 /* Paragraph run used for importing */
	memory_ref			hyperlinks;		 /* The hypertext link run */
	memory_ref			hyperlinks_target; /* The hypertext target run */
	memory_ref			frames;			 /* The frames, if any, to be inserted when completed. */
	memory_ref			previous_import; /* Previous import control, if any (so we can nest) */
	long				par_format_verb; /* Verb that indicates how to apply par_format */
	long				par_format_mark; /* Used with par_mark and par_mark_only verb */
	long				file_os;		/* The OS source of import */
};

typedef struct pg_import_rec pg_import_rec;
typedef pg_import_rec PG_FAR *pg_import_ptr;


/* Attribute record (goes along with tags) */
#define kMaxAttributeSize 1024

struct tag_attribute {
	short   code;                       // Attribute code (from table)
	short   param_size;                 // Size of the string that follows
	pg_char param[kMaxAttributeSize];   // String value (if applicable)
};

struct out_tag_attribute {
	short       code;                       // Attribute code (from table)
	short       param_size;                 // Size of the string that follows
	pg_char_ptr table;                      // Table where the code comes from
	pg_char     param[kMaxAttributeSize];   // String value (if applicable)
};

typedef struct tag_attribute tag_attribute;
typedef tag_attribute PG_FAR *tag_attribute_ptr;
typedef struct out_tag_attribute out_tag_attribute;
typedef out_tag_attribute PG_FAR *out_tag_ptr;


#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (void) pgInitTranslatorRec (pg_globals_ptr globals, pg_translator_ptr translator_ptr);
extern PG_PASCAL (void) pgDisposeTranslatorRec (pg_translator_ptr translator_ptr);
extern PG_PASCAL (pg_filetype) pgDetermineFileType (pg_file_unit fileref, file_io_proc io_proc,
		long starting_position);
extern PG_PASCAL (pg_boolean) pgVerifyRTF (pg_file_unit fileref, file_io_proc io_proc,
		long starting_position);
extern PG_PASCAL (pg_boolean) pgVerifyHTML (pg_file_unit fileref, file_io_proc io_proc,
		long starting_position);
extern PG_PASCAL (void) pgInsertErrorLog (pg_ref pg, memory_ref error_log, short error_code, long begin,
			long end, long refcon, pg_char_ptr msg, pg_boolean no_dups);
extern PG_PASCAL (short) pgConvertResolution (paige_rec_ptr pg, short pointvalue);
extern PG_PASCAL (short) pgResolutionConvert (paige_rec_ptr pg, short value);
extern PG_PASCAL (pg_error) pgImportFileFromC (pg_ref pg, pg_filetype filetype, long feature_flags,
		long file_begin, pg_file_unit f_ref);
extern PG_PASCAL (pg_error) pgExportFileFromC (pg_ref pg, pg_filetype filetype, long feature_flags,
		long file_begin, select_pair_ptr output_range, pg_boolean use_selection, pg_file_unit f_ref);
extern PG_PASCAL (pg_short_t) pgSetFileExtension (pg_char_ptr filename, pg_char_ptr extension,
		short extension_size, short PG_FAR *non_extend_size);
extern PG_PASCAL (pg_error) pgExportIndex (pg_ref pg, long feature_flags, long index_type, pg_file_unit f_ref,
		pg_char_ptr source_filename, pg_char_ptr source_directory, pg_char_ptr page_str,
		short PG_FAR *def_styles, short PG_FAR *def_pars, short title_style, short title_par);
extern PG_PASCAL (void) pgMapCharacters (paige_rec_ptr pg, text_block_ptr block);
extern PG_PASCAL (pg_error) pgImportJPress (pg_ref pg, pg_filetype filetype, long feature_flags,
		long file_begin, pg_file_unit f_ref, memory_ref image_path_info);
extern PG_PASCAL (short) pgCompareFontTable (pg_char_ptr source_font, pg_char_ptr table_font);
extern PG_PASCAL (void) pgRemoveLastInsert (paige_rec_ptr pg_rec, pg_char match_char);
extern PG_PASCAL (void) pgMatchParStyles (paige_rec_ptr pg, long wait_progress, long wait_max);
extern PG_PASCAL (pg_short_t) pgTruncateFileName (pg_char_ptr string, pg_short_t string_size,
			pg_short_t max_size, pg_char file_sep);

#ifdef MAC_PLATFORM

PicHandle TranslateWMFToPicture(pg_bits8_ptr* DataBytes, Rect* frame);

#endif

#ifdef __cplusplus
	}
#endif

#endif
