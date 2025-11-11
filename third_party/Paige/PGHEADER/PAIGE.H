/* Paige Technology API file, copyright 1993-1995 by DataPak Software, Inc.
	Stronger typing added by T. R. Shaw, OITC, Inc.
	
"Paige" is a code name for DataPak's text processing technology. It is
illegal to remove this notice.

CROSS PLATFORM INSTRUCTIONS:

(1) Modify the file "CPUDefs.h" to satisfy the various compiler
demands of your platform.

(2) To add/change machine-specific code, you need to replace
"machine.c" and "mmemory.c" as these files contain every piece of
code necessary to run on a specific platform.

All other code (hopefully) is universal and does not need to be changed. */

/* Updated by TR Shaw, OITC, Inc. 3 Jan 1994 for compatible linkages */
/* Updated by TR Shaw/J Smith, OITC, Inc. 9 JUL 1995 for Window 32, NT, 95 & RTF. */
/* Updated by Gar to merge 1.2 fixes with TR Shaw enancements */

#ifndef PAIGE_H
#define PAIGE_H

#include "pgMemMgr.h"
#include "Userdefs.h"

/* Fixed tab stop array size in paragraph info records: */

#define TAB_ARRAY_SIZE	32

/* Possible bits passed to pgNew: */

#define NO_WRAP_BIT 		0x00000001L	/* Wraps only on <CR> or <LF> */
#define NO_LF_BIT			0x00000002L	/* <LF> char ignored */
#define NO_DEFAULT_LEADING	0x00000004L	/* Do not add default leading from OS font info */
#define NO_EDIT_BIT			0x00000008L	/* No editing (display only) */
#define EXTERNAL_SCROLL_BIT	0x00000010L	/* Scrolling is controlled by application */
#define COUNT_LINES_BIT	 	0x00000020L	/* Keep track of # of lines */
#define NO_HIDDEN_TEXT_BIT	0x00000040L	/* Do not display hidden text */
#define SHOW_INVIS_CHAR_BIT	0x00000080L	/* Show invisible character(s) active */
#define EX_DIMENSION_BIT	0x00000100L	/* Exclude area is included as width/height */
#define NO_WINDOW_VIS_BIT	0x00000200L	/* Do not respect window's clipped area */
#define TEXT_DIRTY_BIT	 	0x00000400L	/* Text format changed but not re-drawn */
#define SMART_QUOTES_BIT	0x00000800L	/* Do "smart quotes" */
#define NO_SMART_CUT_BIT	0x00001000L	/* Do not do "smart cut/paste" */
#define NO_SOFT_HYPHEN_BIT	0x00002000L	/* Ignore soft hyphens */
#define NO_DUAL_CARET_BIT	0x00004000L	/* Do not show dual carets */

/* Additional attributes used internally by Paige: */

#define NO_LINE_SAVE_BIT	0x00008000L	/* Don't save point_start run on Write */
#define DEACT_BIT			0x00010000L	/* Paige is deactivated (won't hilite) */
#define PERM_DEACT_BIT		0x00020000L	/* App wants no highlighting */
#define CARET_BIT			0x00040000L	/* Caret is currently showing */
#define SCALE_VIS_BIT		0x00080000L	/* Include vis area when scaling - MUST be set if scrolling by setting the origin (without changing the window's origin)*/
#define BITMAP_ERASE_BIT	0x00100000L	/* Erase screen with offscreen bits */
#define PRINT_MODE_BIT		0x00200000L	/* pg_ref is "printing" */
#define MERGE_MODE_BIT		0x00400000L	/* pg_ref is in "merge" mode */
#define NO_DEVICE_BIT		0x00800000L	/* pg_ref has been made with no device */
#define DOC_TERMINATED_BIT	0x01000000L	/* Doc ends with paragraph break ("CR") */
#define DOC_BOUNDS_DIRTY	0x02000000L	/* Document bounding rect is not valid */
#define CARET_CREATED_BIT	0x08000000L	/* Windows version, caret is alive */
#define TABS_ARE_WIDTHS_BIT 0x10000000L	/* TAB chars are just default tab width */
#define INIT_IO_BIT			0x20000000L	/* pgReadDoc in progress when this bit set */
#define LINE_EDITOR_BIT		0x40000000L	/* Document becomes basic line editor if set */
#define INVALID_CURSOR_BIT	0x80000000L	/* Cursor is not valid */

/* Additional Paige attributes used to save various information in flags2: */

#define HAS_TITLE_PAGE		0x00000001L	/* Document has title page */
#define PAGE_AFTER_TITLE	0x00000002L	/* Page after title page is special (ie back of page) */
#define LEFT_EVEN_PAGES		0x00000004L	/* Left hand pages are even (Paige page number) */
#define RIGHT_EVEN_PAGES	0x00000008L	/* Right hand pages are even (Paige page number) */
#define HIDE_IMPORT_DELETE	0x00000010L	/* Deletions will be automatically hidden on import (RTF) */
#define DONT_HANGING_INDENT	0x00000020L	/* Don't add automatic tab stop for hanging indent (RTF) */
#define WRAP_TRAILING_SPACE	0x00000040L	/* Wrap trailing spaces onto the next line (RTF) */
#define IMAGE_COLORS_BLACK	0x00000080L	/* Image all colors as black (RTF) */
#define STYLESHEET_CHANGE	0x00000100L	/* Set while stylesheets being added or changed */
#define KEEP_READ_STYLES	0x00000200L	/* Keep existing style_infos for pgReadDoc() */
#define KEEP_READ_PARS		0x00000400L	/* Keep existing par_infos for pgReadDoc() */
#define KEEP_READ_FONTS		0x00000800L	/* Keep existing font_infos for pgReadDoc() */
#define NO_STYLE_CLEANUP	0x00001000L	/* Do not delete style_infos during cleanup (** internal use **) */
#define CHECK_PAGE_OVERFLOW	0x00002000L	/* Constantly check text overflow bottom of page area */
#define UNICODE_SAVED		0x00004000L	/* Text is saved as unicode */
#define UNICODE_TEXT		0x00008000L	/* Text is active as unicode */
#define BLINK_ON_DEACTIVE	0x00010000L	/* Text is will blink even on deactive paige docs */
#define IDLE_VIA_GBL_IDLE	0x00020000L	/* pgIdles are handled via pgGlobalIdle */
#define MOUSE_DRAG_STATE	0x00040000L	/* Mouse is in between mouse_down and mouse_up */
#define NO_HAUTOSCROLL		0x00080000L	/* Do not autoscroll horizontally */
#define NO_VAUTOSCROLL		0x00100000L	/* Do not autoscroll vertically */
#define APPLY_ALL_PAR_INFOS	0x00200000L	/* Apply every par_info in pgCopy and pgPaste (used by Undo) */
#define PAR_EXCLUSION_CHANGE 0x00400000L /* Paragraph exclusion changed */
#define BITMAP_SCROLL_CHANGE 0x00800000L	/* Scroll changed and "smooth" bitmap draw */
#define SAVE_AS_UNICODE		0x00800000L	/* Save file as UNICODE even if non-UNICODE version. */
#define SHOW_HT_TARGETS		0x01000000L	/* Show hypertext target style(s) */
#define NO_CARET_BIT		0x02000000L	/* Do not draw caret under any circumstances */
#define WHOLE_KERNING_BIT	0x04000000L	/* Whole kerning only if set. */
#define NO_READVIS_BIT		0x08000000L	/* Do not read vis area shape from file */
#define HAS_PG_FRAMES_BIT	0x10000000L	/* This pg_ref uses 4.0 frames (DSI RESERVED) */
#define IS_MASTER_BIT		0x20000000L	/* This pg_ref is a "master" (shared by others) */

#ifdef UNICODE
#define INITIAL_TEXT_FLAGS	UNICODE_TEXT
#define TEXT_FILE_FLAGS		UNICODE_SAVED
#define BOM_HEADER			1
#else
#define INITIAL_TEXT_FLAGS	0
#define TEXT_FILE_FLAGS		0
#define BOM_HEADER			0
#define pgInsertBytes	pgInsert
#endif

/* "Shared structures" flags (version 2.0): */

#define SHARED_FORMATS		0x00000001L	/* Style, font and par infos shared */
#define SHARED_GRAF_DEVICE	0x00000002L	/* Common graphics context */
#define SHARED_VIS_AREA		0x00000004L	/* Shared vis area */
#define SHARED_PAGE_AREA	0x00000008L	/* Shared page area */
#define SHARED_EXCLUDE_AREA	0x00000010L	/* Shared exclusion area */

/* Paige types, pg_type */

#define MAIN_DOCUMENT		0			// The main document (RTF enhancement) 

/* Values returned from char_info and/or pgCharType: */

#define BLANK_BIT			0x00000001L	/* Character is blank */
#define WORD_BREAK_BIT		0x00000002L	/* Word breaking char */
#define WORD_SEL_BIT		0x00000004L	/* Word select char */
#define SOFT_HYPHEN_BIT		0x00000008L	/* Soft hyphen char */
#define INCLUDE_BREAK_BIT	0x00000010L	/* Word break but include with word */
#define INCLUDE_SEL_BIT		0x00000020L	/* Select break but include with word */
#define CTL_BIT				0x00000040L	/* Char is a control code */
#define INVIS_ACTION_BIT	0x00000080L	/* Char is not a display char, but arrow, bs, etc. */
#define PAR_SEL_BIT			0x00000100L	/* Char breaks a paragraph */
#define LINE_SEL_BIT		0x00000200L	/* Char breaks a line (soft CR) */
#define TAB_BIT				0x00000400L	/* Char performs a TAB */
#define FIRST_HALF_BIT		0x00000800L	/* 1st half of a multi-byte char */
#define LAST_HALF_BIT		0x00001000L	/* Last half of a multi-byte char */
#define MIDDLE_CHAR_BIT		0x00002000L	/* Middle of a multi-byte char run */
#define CONTAINER_BRK_BIT	0x00004000L	/* Break-container bit */
#define PAGE_BRK_BIT		0x00008000L	/* Break-repeating-shape bit */
#define NON_BREAKAFTER_BIT	0x00010000L	/* Can not end on a line */
#define NON_BREAKBEFORE_BIT	0x00020000L	/* Can not begin a new line */
#define NUMBER_BIT			0x00040000L	/* Char is numeric */
#define DECIMAL_CHAR_BIT	0x00080000L	/* Char is decimal (for decimal tab)  */
#define UPPER_CASE_BIT		0x00100000L	/* Char is UPPER CASE */
#define LOWER_CASE_BIT		0x00200000L	/* Char is lower case */
#define SYMBOL_BIT			0x00400000L	/* Char is a symbol */
#define EUROPEAN_BIT		0x00800000L	/* Char is ASCII-European */
#define NON_ROMAN_BIT		0x01000000L	/* Char is not Roman script */
#define NON_TEXT_BIT		0x02000000L	/* Char is not really text */
#define FLAT_QUOTE_BIT		0x04000000L	/* Char is a "flat" quote */
#define SINGLE_QUOTE_BIT	0x08000000L	/* Quote char is single quote */
#define LEFT_QUOTE_BIT		0x10000000L	/* Char is a left quote */
#define RIGHT_QUOTE_BIT		0x20000000L	/* Char is a right quote */
#define PUNCT_NORMAL_BIT	0x40000000L	/* Char is normal punctuation */
#define OTHER_PUNCT_BIT		0x80000000L	/* Char is other punctuation in multibyte */

/* Convenient char_info macro for any quote char in globals: */

#define QUOTE_BITS (FLAT_QUOTE_BIT | SINGLE_QUOTE_BIT | LEFT_QUOTE_BIT | RIGHT_QUOTE_BIT)
#define NON_MULTIBYTE_BITS (~(FIRST_HALF_BIT | LAST_HALF_BIT))
#define WORDBREAK_PROC_BITS (WORD_BREAK_BIT | WORD_SEL_BIT | NON_BREAKAFTER_BIT | NON_BREAKBEFORE_BIT)
#define PG_SUBSET_BIT (INVIS_ACTION_BIT | CTL_BIT)

/* Possible bits passed to pgDragSelect: */

#define EXTEND_MOD_BIT		0x0001	/* Extend the selection */
#define WORD_MOD_BIT		0x0002	/* Select whole words only */
#define PAR_MOD_BIT			0x0004	/* Select whole paragraphs only */
#define LINE_MOD_BIT		0x0008	/* Highlight whole lines */
#define VERTICAL_MOD_BIT	0x0010	/* Allow vertical selection */
#define DIS_MOD_BIT			0x0020	/* Enable discontiguous selection */
#define STYLE_MOD_BIT		0x0040	/* Select whole style range */
#define WORD_CTL_MOD_BIT	0x0080	/* Select "words" delineated with controls */
#define NO_HALF_CHARS_BIT	0x0100	/* Click does not go left/right on half chars */
#define CONTROL_MOD_BIT		0x0200	/* Word advance for arrows, Home-End to doc top and bottom */
#define OPTION_MOD_BIT		0x0400	/* Option key held down */
#define ALT_MOD_BIT			0x0800	/* Alt key (Windows) or command key (Mac) */
#define PIVOT_EXTEND_BIT	0x4000	/* Extend-selection keeps anchor point */
#define ARROW_ACTIVE_BIT	0x8000	/* Arrow key is actively down */

/* Possible bits within a point start: */

#define LINE_BREAK_BIT		0x8000	/* Line ends here */
#define PAR_BREAK_BIT		0x4000	/* Paragraph ends here */
#define SOFT_PAR_BIT		0x2000	/* Soft CR ends line */
#define RIGHT_DIRECTION_BIT	0x1000	/* Text in this start is right to left */
#define LINE_GOOD_BIT		0x0800	/* This line requires no recalc */
#define NEW_LINE_BIT		0x0400	/* New line starts here */
#define NEW_PAR_BIT			0x0200	/* New paragraph starts here */
#define WORD_HYPHEN_BIT		0x0100	/* Draw a hyphen after this text */
#define TAB_BREAK_BIT		0x0080	/* Tab char terminates this line */
#define HAS_WORDS_BIT		0x0040	/* One or more word separaters exist */
#define CUSTOM_CHARS_BIT	0x0020	/* Style(s) are known only to app
									   (so don't play games with display) */
#define SOFT_BREAK_BIT		0x0010	/* Start breaks on soft hyphen */
#define BREAK_CONTAINER_BIT	0x0008	/* Line breaks for next container */
#define BREAK_PAGE_BIT		0x0004	/* Line broke for whole repeater shape */
#define LINE_HIDDEN_BIT		0x0002	/* Line is invisible (hidden text) */
#define NO_LINEFEED_BIT		0x0001	/* Line does not advance vertically */

#define TERMINATOR_BITS		0xFFFF	/* Flagged only as terminator record */

#define HARD_BREAK_BITS (PAR_BREAK_BIT | SOFT_PAR_BIT | BREAK_CONTAINER_BIT | BREAK_PAGE_BIT)

/* Possible result bits returned from pgPtInView:  */

#define WITHIN_VIS_AREA		0x0001	/* Point is within vis region of Paige */
#define WITHIN_WRAP_AREA	0x0002	/* Point is within wrap shape of Paige */
#define WITHIN_EXCLUDE_AREA	0x0004	/* Point is within exclusion shape of Paige */
#define WITHIN_TEXT			0x0008	/* Point is in text area  */
#define WITHIN_REPEAT_AREA	0x0010	/* Point is in repeating gap of page area */
#define WITHIN_LEFT_AREA	0x0020	/* Point is left of document */
#define WITHIN_RIGHT_AREA	0x0040	/* Point is right of document */
#define WITHIN_TOP_AREA		0x0080	/* Point is above top of document */
#define WITHIN_BOTTOM_AREA	0x0100	/* Point is below bottom of document */
#define WITHIN_LINK_SOURCE	0x0200	/* Point is inside a source link */
#define WITHIN_LINK_TARGET	0x0400	/* Point is inside a target link */
#define WITHIN_TABLE		0x0800	/* Point is inside a table */

/* Possible value(s) set in class_bits for style_info: */

#define CANNOT_HILITE_BIT	0x00000001L	/* Do not include highlight region in this style */
#define CANNOT_BREAK		0x00000002L	/* Non-breaking style (words can't break) */
#define STYLE_IS_CONTROL	0x00000004L	/* Style is "control" type item */
#define GROUP_CHARS_BIT	 	0x00000008L	/* All char(s) in style considered one word */
#define STYLE_MERGED_BIT	0x00000010L	/* Style has been merged in "merged mode" */
#define STYLE_IS_CUSTOM		0x00000020L	/* Style is known only to app */
#define HILITE_RESTRICT_BIT	0x00000040L	/* Cannot select outside of of style */
#define CANNOT_WRAP_BIT		0x00000080L	/* Cannot wrap bit (used for tables, etc.) */
#define IS_NOT_TEXT_BIT		0x00000100L	/* Data is not text at all */
#define REQUIRES_COPY_BIT	0x00000200L	/* Text copy requires copy_proc callback */
#define NO_SMART_DRAW_BIT	0x00000400L	/* Tells PAIGE not to second-guess line drawing */
#define ACTIVATE_ENABLE_BIT	0x00000800L	/* Causes activate_proc to be called */
#define CANT_UNDERLINE_BIT	0x00001000L	/* The OS does not support underline */
#define CANT_TRANS_BIT		0x00002000L	/* Text cant transliterate upper/lower etc. */
#define RIGHTLEFT_BIT		0x00004000L	/* Text direction is right to left */
#define VERTICAL_TEXT_BIT	0x00008000L	/* Vertical text */
#define TEXT_LOCKED			0x00010000L	/* Text can be copied but not changed */
#define NO_EXTRA_SUPER_SUB	0x00020000L	/* Don't add extra space to line height for showing raised(superscript)/lowered(subscript) characters (RTF) */
#define EMBED_SUBSET_BIT	0x00040000L	/* Style is embed_ref pg subset */
#define NO_SAVEDOC_BIT		0x00080000L	/* Do not save this style_info */
#define EMBED_INITED_BIT	0x00100000L	/* Embed ref style and has been initialized. */
#define EMBED_READ_BIT		0x00200000L	/* Used by EmbedReadHandler */
#define EMBED_APPLIED_BIT	0x00400000L	/* Embed_ref is applied to existing chars */
#define EMBED_FRAME_BIT		0x00800000L	/* Style is for a frame embed_ref. */

#define NON_TEXT_BITS (STYLE_IS_CUSTOM | IS_NOT_TEXT_BIT)

/* Version 1.2 special tab-stop flags (embedded in "type"). */

#define TAB_TEXT_HIDDEN_BIT 0x4000	/* Text within tab mark is hidden */
#define TAB_TRUNCATES_BIT	0x2000	/* Text cannot go beyond next tab */
#define TAB_TYPE_MASK		0x00FF	/* Mask for the tab_stop.type */

/* pgPtToStyleInfo() conversion_info bits: */

#define NO_HALFCHARS		0x0001	/* Whole char only */
#define NO_BYTE_ALIGN		0x0002	/* No multibyte alignment (return style AS-IS) */

/* Some default font codes: */

#define WINDOW_FONT			0		/* Font used in windows, etc. */
#define DEF_FIXED_FONT		1		/* Default fixed-spaced font */
#define DEF_PROP_FONT		2		/* Default variable spaced font */
#define TIMES_ROMAN			3		/* Times font */
#define HELVETICA_FONT		4		/* Helvetica font */
#define GOTHIC_FONT			5		/* Gothic font */
#define COURIER_FONT		6		/* Courier font */
#define SYMBOL_FONT			7		/* Symbol font */


/* Font environment settings: */

#define NAME_IS_CSTR		1		/* Font name is a cstring */
#define FONT_NOT_AVAIL		2		/* Font name doesn't exist in system */
#define FONT_GOOD			4		/* Font name will NOT be checked if this bit set */
#define FONT_BEST_GUESS		8		/* Font has been changed to best-guess equiv. */
#define FONT_PG_DEFAULT		0x10	/* Default font was used as substitute */
#define FONT_USES_ALTERNATE	0x20	/* The alternate font name has been selected */
#define NAME_ALT_IS_CSTR	0x40	/* Font alternate name is a cstring */
#define FONT_NOT_SAVED		0x80	/* Font is not saved in pgSaveDoc() */
#define FONTS_TEXT_UNKNOWN	0x100	/* Font's text may not be rendered correctly as it is untranslatable from original platform (not this one) */

/* Misc. #defines */

#define SIMPLE_SHAPE_QTY	2		/* Number of rects for a simple shape */
#define CURRENT_POSITION	-1		/* Insert at current positions */
#define TAB_BOUNDS_RELATIVE	-1		/* Tab base is relative to wrap bound's left */
#define TAB_WRAP_RELATIVE	-2		/* Tab base is relative to current wrap edge */
#define SET_MASK_BITS		-1		/* Mask word for change masks */
#define PG_FUTURE			8		/* Number of long's reserved for future */
#define PLAIN_SCALE			1		/* Widths are scaled directly in measure proc */
#define COMPENSATE_SCALE	2		/* Errors in scaling are compensated in measure proc */
#define XOR_SUPERIMPOSE		-1		/* Superimpose style XORs if style element is this. */
#define REVISION_MASK		0xF000	/* revision_var "deleted"/"revised" flag mask. (RTF) */
#define AUTHOR_MASK			0x0FFF	/* revision_var author index mask. (RTF) */
#define TEXT_DELETED		0x1000	/* revision_var "deleted" text flag. (RTF) */
#define TEXT_REVISED		0x2000	/* revision_var "revised" text flag. (RTF) */


/* CharInfo / pgCharType convenient mask_bits: */

#define SHORT_FORM_INFO		0x0000FFFF	/* All short form items */
#define LONG_FORM_INFO		(EUROPEAN_BIT | SYMBOL_BIT | PUNCT_NORMAL_BIT | OTHER_PUNCT_BIT | NUMBER_BIT | UPPER_CASE_BIT)

#define ALL_INFO_BITS		0xFFFFFFFF	/* All bits */
#define MULTICHAR_BITS		(FIRST_HALF_BIT | LAST_HALF_BIT | MIDDLE_CHAR_BIT)  /* All multi-char bits */

/* text_block flags */

#define NEEDS_CALC			0x0001		/* One or more lines need re-calc */
#define NEEDS_PAGINATE		0x0002		/* Needs re-paginatation */
#define SOME_LINES_GOOD		0x0004		/* One or more lines are probably OK */
#define SOME_LINES_BAD		0x0008		/* Flags means we already tagged bad lines */
#define BROKE_BLOCK			0x0010		/* Terminator char deleted */
#define ALL_TEXT_HIDDEN		0x0020		/* All text in block is hidden! */
#define BOUNDS_GUESSED		0x0040		/* Best guess only has been made for bounds rect */
#define LINES_PURGED		0x0080		/* Lines purged, but block OK */
#define BELOW_CONTAINERS	0x0100		/* Lines below last container (so they are fake) */
#define NO_CR_BREAK			0x0400		/* Does not break on a CR */
#define SWITCHED_DIRECTIONS 0x0800		/* System text direction has switched! */
#define BLOCK_HAS_TABLE 	0x1000		/* One or more tables exist in this block */
#define JUMPED_4_EXCLUSIONS	0x2000		/* One or more lines hop across exclusions */
#define NEEDS_PARNUMS		0x4000		/* Requires paragraph "line" computation (line editor) */

#define NULL_RUN			0xFFFF		/* Void "style item" for runs */
#define DEFAULT_STYLE_ITEM	0x0000		/* Default style item index */

/* font->machine_var[n] indexes:  */

#define PG_OUT_PRECISION	0			/* font's out precision */
#define PG_CLIP_PRECISION	1			/* font's clip precision */
#define PG_QUALITY			2			/* font's quality */
#define PG_CHARSET			3			/* font's character set */
#define PG_PREV_LANG		4			/* font's previous language */
#define PG_LANGINFO			5			/* language information bits */

/* Border codes (for 2.15) */

#define PG_BORDER_LEFT		0x000000FFL		/* Left border */
#define PG_BORDER_RIGHT		0x0000FF00L		/* Right border */
#define PG_BORDER_TOP		0x00FF0000L		/* Top border */
#define PG_BORDER_BOTTOM	0xFF000000L		/* Bottom border */

#define PG_BORDER_LEFTLINE	0x00000001L		/* Left border */
#define PG_BORDER_RIGHTLINE	0x00000100L		/* Right border */
#define PG_BORDER_TOPLINE	0x00010000L		/* Top border */
#define PG_BORDER_BOTTOMLINE 0x01000000L		/* Bottom border */

#define PG_BORDER_3D		0x00000008L		/* A 3D-effect border (e.g. html tables) */
#define PG_BORDER_ALL3D		0x08080808L		/* All sides 3D */
#define PG_BORDER_DOTTED	0x00000010L		/* Border = dotted line */
#define PG_BORDER_ALLDOTTED	0x10101010L		/* All sides dotted */
#define PG_BORDER_SHADOW	0x00000020L		/* Shadow effect */
#define PG_BORDER_DOUBLE	0x00000040L		/* Double line(s) */
#define PG_BORDER_ALLDOUBLE	0x40404040L		/* All sides double */
#define PG_BORDER_RULE		0x00000080L			/* Single line rule (vertically centered if no text in line) */
#define PG_BORDER_PENSIZE	0x00000007L		/* Pensize (1 - 8) */
#define PG_BORDER_LINERULE	0x82000000L		/* A single line rule */
#define PG_BORDER_ALLSIDES	0x01010101L		/* Border on all sides */
#define PG_BORDER_SHADOWBOX	0x21012101L		/* Shadow box */

/* Verbs for pgInsert: */

enum {
	key_insert_mode,				/* Typing insertion */
	key_buffer_mode,				/* Typing-buffer insertion */
	data_insert_mode,				/* Raw data insertion */
	recursive_insert_mode			/* Inserting buffered keys recursively */
};

/* Draw verbs (affects display or refresh): */

enum {
	draw_none,						/* Do not draw at all */
	best_way,						/* Use most effecient method(s) */
	direct_copy,					/* Directly to screen, overwrite */
	direct_or,						/* Directly to screen, "OR" */
	direct_xor,						/* Directly to screen, "XOR" */
	bits_copy,						/* Copy offscreen */
	bits_or,						/* Copy offscreen in "OR" mode */
	bits_xor,						/* Copy offscreen in "XOR" mode */
	bits_emulate_copy,				/* Offscreen emulation, copy */
	bits_emulate_or,				/* Offscreen emulation, or */
	bits_emulate_xor,				/* Offscreen emulation, xor */
	grayish_bits_copy,				/* Gray-disabled bits copy */
	grayish_or						/* In gray-disabled */
};


/* "Mouse" tracking verbs: */

enum {
	mouse_down,						/* First time click */
	mouse_moved,					/* Mouse moved */
	mouse_up						/* Mouse up */
};

/* Caret position setting verbs: */

enum {
	home_caret,
	doc_bottom_caret,
	begin_line_caret,
	end_line_caret,
	next_word_caret,
	previous_word_caret
};

#define EXTEND_CARET_FLAG	0x8000	/* Used with caret verbs above to extend selection */
#define CARET_VERB_MASK		0x0FFF	/* Used to clear bit(s) to get caret verb */

/* Style index definitions: */

enum {
	bold_var,
	italic_var,
	underline_var,
	outline_var,
	shadow_var,
	condense_var,
	extend_var,
	dbl_underline_var,
	word_underline_var,
	dotted_underline_var,
	hidden_text_var,
	strikeout_var,
	superscript_var,
	subscript_var,
	rotation_var,
	all_caps_var,
	all_lower_var,
	small_caps_var,
	overline_var,
	boxed_var,
	relative_point_var,
	super_impose_var,
	revision_var,			// RTF
	nested_subset_var,		// For by DSI only, do not use
	blink_var,				// HTML
	index_var,				// Document index
	TOC_var,				// Table of contents variable
	dsi_custom_var = 27,	/* Used for DSI internally only */
	custom_var = 28
};

/* Index values for HTML character styles (version 3.0+) */

enum {
	body_stylesheet,
	pre_tag_stylesheet,
	teletype_stylesheet,
	blockquote_stylesheet,
	address_stylesheet,
	cite_stylesheet,
	code_stylesheet,
	code_def_stylesheet,
	code_var_stylesheet,
	heading1_stylesheet,
	heading2_stylesheet,
	heading3_stylesheet,
	heading4_stylesheet,
	heading5_stylesheet,
	heading6_stylesheet,
	NUM_HTML_STYLES
};

/* Additional HTML paragraph formats: */

enum {
	html_par_normal,
	html_ordered_list,
	html_unordered_list,
	html_definition_list,
	html_list,
	html_directory,
	html_menu
};

/* The graf_device verb values: */

enum {
	set_pg_device,
	unset_pg_device
};

/* Action state values (v2.12+) */

enum {
	pg_state_idle,
	pg_state_save,
	pg_state_export,
	pg_state_read,
	pg_state_import
};


#define SMALL_CAPS_DEFAULT  -1

/* Style variations (when defines as "bits"): */

#define X_PLAIN_TEXT					0x00000000L
#define X_BOLD_BIT						0x00000001L
#define	X_ITALIC_BIT					0x00000002L
#define X_UNDERLINE_BIT					0x00000004L
#define X_OUTLINE_BIT					0x00000008L
#define X_SHADOW_BIT					0x00000010L
#define X_CONDENSE_BIT					0x00000020L
#define X_EXTEND_BIT					0x00000040L
#define X_DBL_UNDERLINE_BIT				0x00000080L
#define X_WORD_UNDERLINE_BIT			0x00000100L
#define X_DOTTED_UNDERLINE_BIT			0x00000200L
#define X_HIDDEN_TEXT_BIT				0x00000400L
#define X_STRIKEOUT_BIT					0x00000800L
#define X_SUPERSCRIPT_BIT				0x00001000L
#define X_SUBSCRIPT_BIT					0x00002000L
#define X_ROTATION_BIT					0x00004000L
#define X_ALL_CAPS_BIT					0x00008000L
#define X_ALL_LOWER_BIT					0x00010000L
#define X_SMALL_CAPS_BIT				0x00020000L
#define X_OVERLINE_BIT					0x00040000L
#define X_BOXED_BIT						0x00080000L
#define X_RELATIVE_POINT_BIT			0x00100000L
#define X_SUPERIMPOSE_BIT				0x00200000L
#define X_INDEX_BIT						0x02000000L
#define X_TOC_BIT						0x04000000L

#define X_ALL_STYLES					0xFFFFFFFF

/* Super/subscript action verbs: */

enum {
	baseline_relative,			/* Draw from line's baseline */
	percent_of_style			/* Draw relative to percent of baseline */
};


/* Justification modes */

enum {
	justify_left,				/* Left justify */
	justify_center,				/* Center justify */
	justify_right,				/* Right justify */
	justify_full,				/* Full justify */
	force_left,					/* Force left (regardless of writing dir.) */
	force_right,				/* Force right (regardless of writing dir.) */
	
/* The following are used for HTML attributes */

	justify_middle,
	justify_absbottom,
	justify_absmiddle,
	justify_baseline,
	justify_top
};


/* Writing direction modes */

enum {
	right_left_direction = -1,	/* Right-to-left */
	system_direction,			/* Direction defined by OS */
	left_right_direction,		/* Left-to-right */
	up_down_left_direction,		/* Vertical, from top-left */
	up_down_right_direction		/* Vertical, from top-right */
};


/* Tab type definitions: */

enum {
	no_tab,						/* none (used to delete and/or report "none") */
	left_tab,					/* Left tab */
	center_tab,					/* Center tab */
	right_tab,					/* Right tab */
	decimal_tab					/* Decimal tab */
};



/* Action verbs for "get data": */

enum {
	all_data,					/* Return all data */
	all_text_chars,				/* All text that is writing script */
	all_roman,					/* All Roman ASCII chars */
	all_visible_data,			/* Return all visible data */
	all_visible_text_chars,		/* All visible text that is writing script */
	all_visible_roman			/* All visible Roman ASCII chars */
};



/* Undo action verbs: */

enum {
	undo_none,					/* Null undo ("can't undo") */
	undo_typing,				/* Undo pg_char key insertion (except backspace) */
	undo_backspace,				/* Undo backspace key */
	undo_delete,				/* Undo clear/cut/delete */
	undo_fwd_delete,			/* Undo forward delete */
	undo_paste,					/* Undo paste/insert */
	undo_format,				/* Undo text style or par format or font */
	undo_insert,				/* Undo some other form of insertion */
	undo_page_change,			/* Undo page area change */
	undo_vis_change,			/* Undo vis area change */
	undo_exclude_change,		/* Undo exclusion area change */
	undo_doc_info,				/* Undo setDocInfo change */
	undo_embed_insert,			/* Undo embed_rec insertion */
	undo_app_insert				/* Undo insert with position parameter */
};

#define UNDO_CUSTOM	(undo_doc_info + 128)

/* Action verb for enhance_undo_proc (used for custom undo's): */

enum {
	enhance_prepared_undo,
	enhance_performed_undo
};


/* Scroll action verbs: */

enum {
	scroll_none,				/* Do not scroll */
	scroll_unit,				/* Scroll one unit */
	scroll_page,				/* Scroll one page unit */
	scroll_home,				/* Scroll to top */
	scroll_end					/* Scroll to end */
};


/* Cursor state action verbs: */

enum {
	debug_cursor = -1,			/* To debug cursor problems (debug mode only) */
	dont_draw_cursor,			/* Do nothing */
	toggle_cursor,				/* Toggle cursor based on timer */
	toggle_cursor_idle,			/* In between toggle_cursor time */
	show_cursor,				/* Show cursor */
	hide_cursor,				/* Hide cursor */
	deactivate_cursor,			/* Cursor is no longer active */
	activate_cursor,			/* Cursor is active */
	update_cursor,				/* Redraw cursor per current state */
	restore_cursor,				/* Turn cursor back on (mainly used in Windows) */
	compute_cursor				/* Just compute cursor pos in pg_rec->stable_caret */
};


/* Verbs for text measure function: */

enum {
	measure_width_locs,
	measure_draw_locs,
	measure_textout
};

/* Verbs for enhancing containers hook: */

enum {
	clip_container_verb,
	unclip_container_verb,
	erase_rect_verb,
	will_draw_verb,
	will_delete_verb
};


/* Verbs passed as "reason_verb" for dup_style_proc, delete_style_proc, etc.: */

enum {
	prepare_undo_text_reason,
	prepare_undo_style_reason,
	prepare_undo_typing_reason,
	undo_delete_reason,
	undo_style_reason,
	copy_reason,
	paste_reason,
	for_next_insert_reason,
	new_stylesheet_reason,
	internal_clone_reason,
	not_used_reason,
	pgdispose_reason,
	disposeundo_reason,
	delete_text_reason,
	pg_new_reason
};

/* Verbs for "line glitter procs" call_verb: */

enum {
	glitter_bitmap_draw,
	glitter_post_bitmap_draw,
	glitter_normal_draw,
	glitter_pre_draw
};

/* Background drawing verbs: */

enum {
	load_background_image,
	draw_background_image
};


/* File handler key */

typedef short pg_file_key, PG_FAR *pg_file_key_ptr;


/* Default key handler verbs */

enum {
	pg_eof_key = -2,			/* Logical end-of-file key */
	pg_signature = -1,			/* File signature (not a "real" key) */
	paige_key,					/* Paige record info */
	text_block_key,				/* Text block record */
	text_key,					/* Text bytes */
	line_key,					/* Point_start info */
	style_run_key,				/* Style runs */
	par_run_key,				/* Paragraph runs */
	style_info_key,				/* Style_info records */
	par_info_key,				/* Par_info data */
	font_info_key,				/* Font_info records */
	vis_shape_key,				/* Paige's vis area */
	page_shape_key,				/* Paige's wrap area */
	exclude_shape_key,			/* Paige's exclude area */
	selections_key,				/* List of selections */
	extra_struct_key,			/* Extra stuff from app */
	applied_range_key,			/* Special applied range for copies */
	doc_info_key,				/* Document info */
	containers_key,				/* Container ref_cons key */
	exclusions_key,				/* Exclusion ref_cons key */
	aux_pg_key,					/* A new pg_ref begins (nested in original) */
	globals_key,				/* Critical Paige global values */
	embedded_item_key,			/* Embedded text item (from pgEmbed module) */
	source_refcon_key,			/* for source code users */
	exception_key,				/* Error handler */
	named_styles_key,			/* Named styles */
	format_init_key,			/* Init style or font, etc. key */
	table_info_key,				/* Table info */
	par_exclusions_key,			/* Paragraph exclusion run */
	PLATFORM_SPECIFIC_KEY		/* Start if platform-specific keys */
};

/* Macintosh-specific keys (we discourage the use of these after version 1.1 --
see notes on "pgGrafx" for portable graphic objects): */

enum {
	mac_pict_key = PLATFORM_SPECIFIC_KEY,	/* Mac Pictures */
	mac_control_key,						/* Mac Control */
	mac_sound,								/* Sound record */
	mac_quicktime,							/* QuickTime pic */
	mac_print_key,							/* Mac print record */
	mac_rgb_key,							/* RGBColor */
	mac_code_rsrc,							/* Mac code resource */
	mac_quickdraw,							/* Quickdraw object */
	mac_custom_object						/* Custom object (code rsrc included */
};

/* Extended file key definitions: */

enum {
	character_set_key = PLATFORM_SPECIFIC_KEY + 255,	/* Character set of doc */
	portable_font_key,									/* Font record (portable) */
	pg_picture_key,										/* PAIGE portable picture */
	ruler_format_key,									/* - reserved for Aurbach - */
	layout_object_key,									/* - reserved for Aurbach - */
	extension_key,										/* Extension Manager's key */	//ее TRS/OITC
	author_key,											/* Author references key */	//ее TRS/OITC
	bookmark_key,										/* Bookmark references key */	//ее TRS/OITC
	annotations_key,									/* Annotations references key */	//ее TRS/OITC
	document_ref_key,									/* Document file references key */	//ее TRS/OITC
	document_info_key,									/* Document information references key */	//ее TRS/OITC
	html_info_key,										/* HTML/SGML information references key */	//ее TRS/OITC
	spcl_idle_list_key,									/* For special idle list */
	section_info_key,									/* For sections */
	hyperlink_key,							/* For hypertext links */
	hyperlink_target_key,					/* For hypertext target links */
	url_image_key							/* A URL image (goes with embeds) */
};


#define CUSTOM_HANDLER_KEY	(PLATFORM_SPECIFIC_KEY + 1024)	// App can use this for keys

/* Format init verbs for format_init_key: */

enum {
	init_start_verb,
	init_style_verb,
	init_font_verb,
	init_par_verb,
	init_end_verb
};

/* I/O Verbs  */

enum {
	io_data_direct,				/* Read or write data directly to pointer */
	io_data_indirect,			/* Read or write data into/from a memory_ref */
	io_get_eof,					/* Return file size */
	io_set_fpos,				/* Set file position */
	io_set_eof,					/* Set end of file */
	io_file_unit				/* Return file unit */
};

/* Extend proc verbs:  */

enum {
	pg_std_procs,				/* Standard procs have been initialized */
	pg_new,						/* pgNew has been called */
	pg_dispose					/* About to do a pgDispose */
};


/* Text block feed action verbs: */

enum {
	use_block,					/* Will be using block */
	unuse_block,				/* No longer using block */
	break_block					/* Break this block */
};

/* Highlight state verbs: */

enum {
	no_change_verb,				/* State stays the same */
	activate_verb,				/* Set to active mode */
	deactivate_verb				/* Set to deactive mode */
};


/* "Wait function" verbs:  */

enum {
	paginate_wait,				/* Long pagination */
	copy_wait,					/* Long copy */
	insert_wait,				/* Long paste (insert) */
	save_wait,					/* Save file wait */
	open_wait					/* Open file wait */
};

/* Line initialize verbs: */

enum {
	init_measure_verb,
	new_line_verb,
	done_measure_verb
};

/* Post-(after) pagination action taken verbs: */

enum {
	paginated_line_shift,			/* Only shifted line locations vertically */
	paginated_empty_block,			/* Built an empty block */
	paginated_hidden_block,		 	/* Built block whose text is all invisible. */
	paginated_fake_block,			/* Built dummy block -- sits below last container */
	paginated_partial_block,		/* Rebuilt lines only partially */
	paginated_full_block			/* Rebuilt everything */
};


/* app_init_proc verbs */

#define EMBED_REF_INIT		1


/**********************  Typed Memory Ref's ***********************/

/* These are typedef'd for compiler checking only */

typedef memory_ref	pg_ref;		/* Reference ID for a Paige object */
typedef pg_ref		pg_subref;	/* A pg_ref clone (most members shared with main doc) */
typedef pg_subref PG_FAR *pg_subref_ptr;	/* Pointer to a list of subrefs */
typedef memory_ref	shape_ref;	/* Memory ref containing a "shape" */
typedef memory_ref	text_ref;	/* Memory ref containing text data */
typedef memory_ref	line_ref;	/* Memory ref containing line info */
typedef memory_ref  tab_ref;	/* Memory ref contain tab records */
typedef memory_ref  select_ref;	/* Selection points (longs) */
typedef memory_ref	file_ref;	/* File reference (contains machine stuff) */
typedef memory_ref	undo_ref;	/* For Undo support */
typedef memory_ref	pg_list_ref;/* For list of paige refs */

/* Additional typedef'd memory_refs (that app won't usually need to see): */

typedef memory_ref  block_ref;	/* For text_block arrays */
typedef memory_ref	format_ref;	/* For style record array */
typedef memory_ref	par_ref;	/* Paragraph record array */
typedef memory_ref	font_ref;	/* Font table */
typedef memory_ref	style_ref;	/* Style run array */
typedef memory_ref	pgHyphenInfoRef;	/* Hyphenation extension */



/**********************  Paige data structures ********************/


/* Coordinate or "Point" */

struct co_ordinate {
	long		v;				/* vertical position */
	long		h;				/* horizontal position */
};

typedef struct co_ordinate co_ordinate;

typedef co_ordinate PG_FAR *co_ordinate_ptr; /* "FAR" pointer for coordinate */

/* Rectangle */

struct rectangle {
	co_ordinate	top_left;		/* Top-left of rect */
	co_ordinate	bot_right;		/* Bottom-right of rect */
};

typedef struct rectangle rectangle;

typedef rectangle PG_FAR *rectangle_ptr;
typedef rectangle 		 shape;			/* Also a "shape" for practical purposes */
typedef rectangle_ptr	 shape_ptr;

/* Shape section */

struct shape_section {
	long					r_num;		/* Rectangle number (index) */
	long					v_position;	/* Vertical position (from top) */
};

typedef struct shape_section shape_section;
typedef shape_section PG_FAR *shape_section_ptr;


/* Color variable: */

struct color_value {
	unsigned short	red;		/* Red composite */
	unsigned short	green;		/* Green composite */
	unsigned short	blue;		/* Blue composite */
	pg_short_t		alpha;		/* Optional value (machine dependent) */
};

typedef struct color_value color_value;
typedef color_value PG_FAR *color_value_ptr;


/* Line info record (becomes array of screen positions): */

struct point_start {
	pg_short_t	offset;			/* Position into text */
	short		extra;			/* Tab record if &0xC000 == 0 */
	short		baseline;		/* Distance from bottom to draw */
	pg_short_t	flags;			/* Various attributes flags */
	pg_short_t	cell_height;	/* EXTRA distance to bottom of cell (if this is a table) */
	pg_short_t	cell_num;		/* The column number if in a table */
	long		r_num;			/* Wrap rectangle record where this sits */
	rectangle	bounds;			/* Point(s) that enclose text piece exactly */
};

typedef struct point_start point_start;
typedef point_start PG_FAR *point_start_ptr;

#define CELL_NUM_MASK	0x00FF	/* Mask for the cell number in a point_start */
#define CELL_TOP_BIT	0x8000	/* The point_start is in the top line of a column */
#define CELL_BOT_BIT	0x4000	/* The point_start is in the bottom line of a column */
#define CELL_FIRST_BIT	0x2000	/* The point_start is in the top row of the table */
#define CELL_LAST_BIT	0x1000	/* The point_start is in the last row of the table */
#define CELL_LEFT_BIT	0x0800	/* The point_start is in the leftmost column */
#define CELL_RIGHT_BIT	0x0400	/* The point_start is in the rightmost column */

struct text_block {
	long		begin;			/* Relative offset beginning */
	long		end;			/* Relative offset ending */
	rectangle	bounds;			/* Entire area this includes */
	text_ref	text;			/* Actual text data */
	line_ref	lines;			/* Point_start run for lines */
	pg_short_t	flags;			/* Used internally by Paige */
	pg_short_t	num_lines;		/* Number of lines */
	pg_short_t	num_pars;		/* Number of paragraphs */
	pg_short_t	cache_flags;	/* Flags used in disk caching feature v2.0 */
	long		cache_begin;	/* File begin offset (disk caching feature v2.0) */
	long		file_os;		/* Which file OS we came from (used for cache-read) */
	long		first_line_num;	/* First line number */
	long		first_par_num;	/* First par number */
	point_start	end_start;		/* Copy of ending point_start in block */
	memory_ref	subref_list;	/* Reserved for DSI extensions */
	tb_append_t	user_var;		/* Can be used for anything */
};

typedef struct text_block text_block;
typedef text_block PG_FAR *text_block_ptr; /* "FAR" pointer for text_block */

/* From/To drawing point: */

struct draw_points {
	co_ordinate		from;			/* Draw from */
	co_ordinate		to;				/* Draw to */
	long			real_offset;	/* Actual offset into all text */
	long			ascent;			/* Distance to top of line from baseline */
	long			descent;		/* Distance to bottom of line from baseline */
	long			line_offset;	/* Offset where line begins */
	long			compensate_h;	/* Amount of x-axis compensation (due to device mapping) */
	point_start_ptr	starts;			/* Pointer to current point_start record */
	shape_ref		bitmap_exclude;	/* If bitmap draw, shape of exclusion rects */
	co_ordinate		bitmap_offset;	/* If bitmap, offset to "real" screen draw */
	co_ordinate		vis_offset;		/* Total amount point_starts were offset */
	text_block_ptr	block;			/* Current text block */
};

typedef struct draw_points draw_points;
typedef draw_points PG_FAR *draw_points_ptr;

/* Text style run record */

struct style_run {
	size_t		offset;			/* Absolute text offset */
	pg_short_t	style_item;		/* Index to style records */
};

typedef struct style_run style_run;
typedef style_run PG_FAR *style_run_ptr;	/* "FAR" pointer for style run */


/* Font table record: */

#define FONT_SIZE		64			/* Size of font name */

/* IMPORTANT SOURCE CODE NOTE: See function "pgSetMaskFromFlds" in pgDefStl.c
source code, which depends on the exact sequence of fields. Hence if fields are
moved around or added in style_info this source needs to be modified. */

struct font_info {
	pg_char	 	name[FONT_SIZE + BOM_HEADER];			/* "Name" of font */
	pg_char		alternate_name[FONT_SIZE + BOM_HEADER];	/* Alternate font if first name not found */
	short		environs;					/* Machine-specific attributes */
	short		typeface;					/* Typography class (low byte = char map index) */
	short		family_id;					/* Font ID code */
	short		alternate_id;				/* Alternate ID code if bad font */
	short		char_type;					/* Type of character set ("script" for Mac) */
	short		code_page;					/* Code Page for Windows(MS) */ 	
	long		platform;					/* The platform this font originated */
	long		language;					/* Language */
	long		machine_var[PG_FUTURE];		/* Used for machine-specific purposes */
	font_append_t user_var;					/* Can be whatever */
};

typedef struct font_info font_info;
typedef font_info PG_FAR *font_info_ptr;	/* "FAR" pointer for font_info */

/* The following defines short and long members in font_info.
Do not change unless font_info changes! */

#define FONTINFO_SHORTS		6
#define FONTINFO_LONGS		11

/* Named stylesheet record: */

struct named_stylesheet {
	pg_char		name[FONT_SIZE + BOM_HEADER];
	short		stylesheet_id;
	short		par_stylesheet_id;
};

typedef struct named_stylesheet named_stylesheet;
typedef named_stylesheet PG_FAR *named_stylesheet_ptr;


/* Tab record: */
/* (For tables, tab_type = alignment, leader = cell borders, position = column width and ref_con = column shading. */

struct tab_stop {
	long		tab_type;		/* Type of tab */
	long		position;		/* Tab position */
	long		leader;			/* Low word = tab leader, high word = reserved for DSI */
	long		ref_con;		/* Can be used for special purposes */
};

typedef struct tab_stop tab_stop;
typedef tab_stop PG_FAR *tab_stop_ptr;	/* "FAR" pointer for tab record */

/* Pair defining a selection range:  */

struct select_pair {
	size_t		begin;				/* Beginning offset of some text portion */
	size_t		end;				/* Ending offset of some text portion */
};

typedef struct select_pair select_pair;
typedef select_pair PG_FAR *select_pair_ptr;


/* Array of records in keyboard_ref: */

struct pg_kb_data {
	pg_char			text_byte[4];		/* Byte backspaced (extra for pad & long align) */
	pg_short_t		style_item;			/* Style (locally indexed) */
	pg_short_t		par_item;			/* Paragraph (locally indexed) */
};

typedef struct pg_kb_data pg_kb_data;
typedef pg_kb_data PG_FAR *pg_kb_ptr;


/* Text selection point record (defines "caret" or visual selection points): */

struct t_select {
	size_t				offset;				/* Absolute text offset */
	select_pair			word_offsets;		/* Original word offsets if applicable */
	co_ordinate			original_pt;		/* Original point of selection */
	pg_short_t			line;				/* Point start number */
	pg_short_t			flags;				/* Contains internal attributes */
	long				control_offset;		/* Offset for purposes of tracking control */
	long				primary_caret;		/* Offset from left for primary direction caret */
	long				secondary_caret;	/* Offset from left for opposing direction caret */
};

typedef struct t_select t_select;
typedef t_select PG_FAR *t_select_ptr;

/* Record to save character positions, for performance: */

struct char_widths {
	memory_ref			positions;			/* Ref that holds positions */
	memory_ref			types;				/* Ref that holds char_info stuff */
	long				offset;				/* The offset into the text_block for these locs */
	long				end;
	long				mem_id;				/* Memory ID that "owns" this set of charwidths */
	long				used_ctr;			/* Nested usage of these char widths */
};

typedef struct char_widths char_widths;

/* Scaling factor record */

struct pg_scale_factor {
	co_ordinate		origin;			/* Relative origin */
	pg_fixed		scale;			/* Scaling (hiword / loword fraction) */
};

typedef struct pg_scale_factor pg_scale_factor;
typedef pg_scale_factor PG_FAR *pg_scale_ptr;

/* Portable "graphics port" or window: */

struct pg_clip_info {
	co_ordinate		clip_origin;	/* Origin the clipping is based on */
	co_ordinate		scroll_pos;		/* Scroll position clipping was based */
	rectangle		vis_bounds;		/* Visual bounds clipping was based */
	rectangle		page_bounds;	/* Visual page bounds clipping was based on */
	long			scale;			/* Scale value clip was based on */
	long			change_flags;	/* Misc. changes that can affect clip region */
};

typedef struct pg_clip_info pg_clip_info;

struct graf_device {
	generic_var		machine_var;	/* Machine-dependent item */
	generic_var		palette;		/* Machine-specific palette (to be selected by PAIGE) */
	color_value 	bk_color;		/* Color used for erasing (Windows only) */
	co_ordinate 	origin;			/* Offset for which things draw (default is 0, 0) */
	pg_region		clip_rgn;		/* The current clip region */
	pg_region		scroll_rgn;		/* If non-null, clip to this (from scrolling) */
	pg_clip_info	clip_info;		/* Info in which last clipping region was based. */
	long			palette_select; /* Used for machine-specific palette code */
	size_t			previous_items[6]; /* Previously selected items (machine-specific) */
	size_t			machine_ref;	/* Additional ref for machine */
	size_t			machine_ref2;	/* More info for machine */
	size_t			machine_ref3;	/* More info for machine */
	size_t			machine_ref4;	/* More info for machine */
	long			caret_info;		/* Used for machine-dependent caret stuff */
	memory_ref		graf_stack;		/* Use to "push" previous stuff */
	memory_ref		poly_save;		/* Used to save polygon or picture drawings */
	memory_ref		pict_save;		/* Used to save portable pictures */
	long			resolution;		/* Device resolution (HiWord h LoWord v) */
	generic_var		print_port;		/* Printer port (if we are to match up) */
	pg_scale_factor	scale;			/* Automatic scaling factor, if any */
	long			access_ctr;		/* Number of times called to set this port */
	port_append_t	user_var;		/* Can be used for whatever */
};

typedef struct graf_device graf_device;
typedef graf_device PG_FAR *graf_device_ptr;	/* "FAR" pointer for graf_device */

#define USE_NO_DEVICE		(generic_var) -1  /* pgNew is with no device */

/* Document info record */

#define MAX_COLS			10

struct pg_col_info {
	pg_fixed	width;				/* Column width or 0 if default width (RTF) */
	pg_fixed	gap;				/* Column gap or 0 if default gap (1/2 inch) (RTF) */
};

typedef struct pg_col_info pg_col_info;
typedef pg_col_info PG_FAR *pg_col_ptr;


struct pg_doc_info {
	long		attributes;			/* Various attributes (see below) */
	long		section_attributes;	/* Various section attributes (see below) (RTF) */
	long		begin;				/* Beginning of main text */
	long		end;				/* End of main text */
	short		exclusion_inset;	/* Amount to inset exclusion area for clipping */
	short		scroll_inset;		/* Amount to inset vis area when scrolling */
	short		caret_width_extra;	/* Extra width of the caret (default is 0) */
	pg_short_t	num_pages;			/* Number of "real" pages */
	long		repeat_slop;		/* Minimum remaining before repeat */
	short		minimum_widow;		/* Minimum widow (lines at end of page) */
	short		minimum_orphan;		/* Minimum orphan (lines at top of page) */
	co_ordinate	repeat_offset;		/* Amount of "gap" for repeater shapes */
	rectangle	print_target;		/* App can use as printed page size (RTF) */
	rectangle	margins;			/* Page margins (RTF) */
	rectangle	offsets;			/* Additional offsets of doc, 4 sides */
	long		max_chars_per_line;	/* Optional max characters per line, or zero */
	long		gutter_color;		/* Color for gutter area */
	long		page_borders;		/* Borders for page area */
	short		start_page_num;		/* Starting page number (RTF) */
	short		restart_page_num;	/* Restarting page number (RTF) */
	short		page_num_prefix;	/* Page number prefix index (RTF) */
	pg_char		page_num_sep;		/* Page number separator (RTF) */
	pg_char		page_num_format;	/* Page number format (RTF) */
	co_ordinate	page_num_loc;		/* Location of the page number relative to top and right margin (RTF) */
	pg_fixed	hyph_hot_zone;		/* Amount of space at the right margin in which words are hyphenated (RTF) */
	short		consecutive_hyph;	/* Maximum number of consecutive lines that will be allowed to end in a hyphen. 0 means no limit (RTF) */
	short		start_line_num;		/* Starting line number (RTF) */
	short		restart_line_num;	/* Restarting line number (RTF) */
	short		line_increment;		/* Line number increment (RTF) */
	pg_fixed	line_num_loc;		/* Location of the line number relative to left margin (RTF) */
	pg_fixed	line_num_width;		/* Location of the line number relative to left margin (RTF) */
	pg_char		rev_format;			/* How revised text will be displayed: 0 for no properties shown; 1 for bold; 2 for italic; 3 for underline (the default); 4 for double underline. (RTF) */
	pg_char		rev_bar;			/* How vertical lines mark altered text, based on the argument: 0 for no marking; 1 for left margin; 2 for right margin; 3 for outside (the default: left on left pages, right on right pages). (RTF) */
	short		num_of_cols;		/* Optional number of columns, or zero (RTF) */
	pg_fixed	gutter;				/* Gutter (RTF) */
	pg_fixed	dflt_gap;			/* Default column gap (RTF) */
	pg_col_info	col_info[MAX_COLS];	/* Column information (RTF) */
	memory_ref	title;				/* document's title (RTF) */
	memory_ref	subject;			/* document's subject (RTF) */
	memory_ref	author;				/* document's author (RTF) */
	memory_ref	wp_operator;		/* document's operator (RTF) */
	memory_ref	keywords;			/* document's keywords (RTF) */
	memory_ref	comment;			/* document's comment (RTF) */
	long		version;			/* document's document version (RTF) */
	memory_ref	doccomm;			/* document's document comment (RTF) */
	long		vern;				/* document's internal version number (RTF) */
	pg_time_t	creationtime;		/* document's document creation time (Paige style ie. seconds since 00:00 01 JAN 1900) (RTF) */
	pg_time_t	revisiontime;		/* document's document revision time (Paige style ie. seconds since 00:00 01 JAN 1900) (RTF) */
	pg_time_t	printtime;			/* document's document print time (Paige style ie. seconds since 00:00 01 JAN 1900) (RTF) */
	pg_time_t	backuptime;			/* document's document backup time (Paige style ie. seconds since 00:00 01 JAN 1900) (RTF) */
	pg_time_t	edittime;			/* document's document edit time in min (RTF) */
	long		id;					/* document's Id (RTF) */
	long		future[PG_FUTURE];	/* Reserved for future */
	long		ref_con;			/* App can store whatever */
};

typedef struct pg_doc_info pg_doc_info;
typedef pg_doc_info PG_FAR *pg_doc_ptr;


/* rev_format */

enum {
	no_rev_properties = 0,			/* How revised text will be displayed: 0 for no properties shown (RTF) */
	rev_bold_property = 1,			/* .1 for bold (RTF) */
	rev_italic_property = 2,		/* .2 for italic (RTF) */
	rev_underline_property = 3,		/* .3 for underline (the default) (RTF) */
	rev_dbl_underline_property = 4,	/* .4 for double underline (RTF) */
	rev_paige_property = 5			/* .5 for Paige special formatting (RTF) */
};


/* rev_bar */

enum {
	no_rev_marking = 0,				/* How vertical lines mark altered text, based on the argument: 0 for no marking (RTF) */
	left_margin_rev_bar = 1,		/* .1 for left margin (RTF) */
	right_margin_rev_bar = 2,		/* .2 for right margin (RTF) */
	outside_margin_rev_bar = 3,		/* .3 for outside (the default: left on left pages, right on right pages) (RTF) */
	paige_margin_rev_bar = 4		/* .4 for Paige special formatting (RTF) */
};

#define NUM_FMT_MASK			0x0000000FL	/* Number format mask (RTF) */
#define NUM_DECIMAL				0x00000000L	/* Number format is decimal (1, 2, 3) (RTF) */
#define NUM_UPR_ROMAN			0x00000001L	/* Number format is uppercase roman numeral (I, II, III) (RTF) */
#define NUM_LWR_ROMAN			0x00000002L	/* Number format is lowercase roman numeral (i, ii, iii) (RTF) */
#define NUM_UPR_LETTER			0x00000003L	/* Number format is uppercase letter (A, B, C) (RTF) */
#define NUM_LWR_LETTER			0x00000004L	/* Number format is lowercase letter (a, b, c) (RTF) */
#define NUM_CARDINAL			0x00000005L	/* Number format is cardinal numbering (One, Two, Three) (RTF) */
#define NUM_ORDINAL				0x00000006L	/* Number format is ordinal numbering (1st, 2nd, 3rd) (RTF) */
#define NUM_ORDINAL_TEXT		0x00000007L	/* Number format is ordinal text numbering (First, Second, Third) (RTF) */

/* attributes */

#define V_REPEAT_BIT			0x00000001L	/* Shape repeats vertically */
#define H_REPEAT_BIT			0x00000002L	/* Shape repeats horizontally */
#define BOTTOM_FIXED_BIT		0x00000004L  /* Shape's bottom does not grow */
#define NO_CONTAINER_JMP_BIT	0x00000010L	/* Can't jump containers */
#define MAX_SCROLL_ON_SHAPE		0x00000020L	/* Maximum scroll is on shape only */
#define NO_CLIP_PAGE_AREA		0x00000040L	/* Page area does NOT clip text */
#define PG_LIST_MODE_BIT		0x00000080L	/*** FOR DSI ONLY ***/
#define PG_FORMS_MODE_BIT		0x00000100L	/*** FOR DSI ONLY ***/
#define PAINT_BACKGROUND_BIT	0x00000200L	/*** (not used in 1.3) ***/
#define WINDOW_CURSOR_BIT		0x00000400L	/* Keep cursor in window view */
#define PRINT_MATCH_BIT			0x00000800L	/* Screen must match printer exactly */
#define LANDSCAPE_BIT			0x00001000L	/* Document should be landscaped (RTF) */
#define MIRRORED_MARGINS_BIT	0x00002000L	/* Document has mirrored margins (RTF) */
#define FACING_PAGE_BIT			0x00004000L	/* Document has facing pages (RTF) */
#define MAC_FRAC_WIDTH_BIT		0x00008000L	/* Document has fractional character widths (RTF) */
#define SAVE_AS_RTF_BIT			0x00010000L	/* Document should be saved as rtf (RTF) */
#define LINE_BETWEEN_COLS_BIT	0x00020000L	/* Document should have lines between columns (RTF) */
#define HARD_SUPPRESS_BIT		0x00040000L	/* Document suppresses space before paragraph property after hard page or column break if this bit set (RTF) */
#define LEADING_SUPPRESS_BIT	0x00080000L	/* Document suppresses extra line spacing at top and bottom of page if this bit set (RTF) */
#define REV_PROTECT_BIT			0x00100000L	/* Document is protected for revisions. The user can edit the document, but revision marking cannot be disabled if this bit set (RTF) */
#define REV_ON_BIT				0x00200000L	/* Revision marking is on for the document if this bit set (RTF) */
#define IS_STATIONARY			0x00400000L	/* Document is stationary if this bit set (RTF) */
#define	MAKE_BACKUP				0x00800000L	/* Backup copy is made automatically when the document is saved. (RTF) */
#define OLD_RTF_MAIL_MERGE_BIT	0x01000000L	/* Treat old-style escaped quotation marks (\ ") as current style ("") in mail merge data documents (RTF) */
#define NO_BALANCE_COLUMNS_BIT	0x02000000L	/* Don't balance columns (RTF) */
#define PRINT_COLORS_BLACK_BIT	0x04000000L	/* Print all colors as black (RTF) */
#define NO_SUBSET_REDRAW_BIT	0x08000000L	/* (reserved for DSI, do not use) */
#define COLOR_VIS_BIT			0x10000000L	/* Page color covers whole vis area */
#define NO_CLIP_REGIONS			0x20000000L	/* Do not ever set a clipping region */
#define USE_MARGINS_BIT			0x40000000L	/* Use margins as defined in doc_info. */
#define NO_ERASE_SCROLL_BIT		0x80000000L	/* Do not erase the scroll region */

/* section_attributes */

#define INCL_ENDNOTES_BIT		0x00000001L	/* Section should include endnotes (RTF) */
#define LINE_NUMBER_RESTART_BIT	0x00000002L	/* Section should restarts line numbers (RTF) */
#define LINE_PAGE_RESTART_BIT	0x00000004L	/* Section should restarts line numbers on each page (RTF) */
#define LINE_NUMBERS_CONT_BIT	0x00000008L	/* Section should continue line numbers (RTF) */
#define SECT_BREAK_MASK			0x000000F0L	/* Section break mask (RTF) */
#define SECT_PAGE_BREAK_FLAG	0x00		/* Section break starts a new page (RTF) */
#define SECT_EPAGE_BREAK_FLAG	0x10		/* Section break starts at an even page (RTF) */
#define SECT_OPAGE_BREAK_FLAG	0x20		/* Section break starts at an odd page (RTF) */
#define SECT_COL_BREAK_FLAG		0x30		/* Section break starts a new column (RTF) */
#define NO_SECT_BREAK_FLAG		0x40		/* No section break (RTF) */
#define HYPHEN_CAP_BIT			0x00000100L	/* Section should hyphenate capitalized words (RTF) */
#define PAGE_NUMBER_RESTART_BIT	0x00000200L	/* Section should restarts page numbers (RTF) */
#define PAGE_NUMBER_CONT_BIT	0x00000400L	/* Section should continue page numbers (RTF) */


/* Structure used by post-paginate low level hook: */

struct smart_update {
	long			flags;					/* Flags for special purposes */
	long			suggest_begin;			/* Beginning display suggestion */
	long			suggest_end;			/* Ending display suggestion */
	pg_short_t		num_display_lines;		/* Number of lines in display */
	pg_short_t		num_display_starts;		/* Number of starts in display */
};

typedef struct smart_update smart_update;
typedef smart_update PG_FAR *smart_update_ptr;


/************************* Function Pointer Definitions **********************/

typedef void PG_FAR *pg_proc;

//Forward Declarations
typedef struct paige_rec paige_rec;
typedef paige_rec PG_FAR *paige_rec_ptr;	/* "FAR" pointer for paige_rec */

typedef struct style_info style_info;
typedef style_info PG_FAR *style_info_ptr;	/* "FAR" pointer for style record */

typedef struct par_info par_info;
typedef par_info PG_FAR *par_info_ptr;		/* "FAR" pointer for par_info */

typedef struct style_walk style_walk;
typedef style_walk PG_FAR *style_walk_ptr;

typedef struct pg_globals pg_globals;
typedef pg_globals PG_FAR *pg_globals_ptr;


/* Special generic pointer for info run structures: */

typedef long PG_FAR *pg_info_ptr;	/* Pointer to app-defined record for run */


/* Internal struct used for Undo records: */

struct pg_undo {
	short			verb;				/* Type of undo (for app's reference) */
	short			real_verb;			/* Internal action verb (could be different) */
	pg_ref			data;				/* Data (different for each verb) */
	pg_globals_ptr	globals;			/* Pointer to PAIGE globals */
	memory_ref		keyboard_ref;		/* Used for backspace-key-undo */
	format_ref		keyboard_styles;	/* Styles possibly backspaced */
	format_ref		keyboard_pars;		/* Paragraphs possibly backspaced */
	memory_ref		applied_range;		/* Range to apply Undo */
	memory_ref		shape_data;			/* Data for shape undo */
	memory_ref		refcon_data;		/* Used to copy refcons for containers, etc. */
	memory_ref		doc_data;			/* Used for undo doc info */
	memory_ref		subset_data;		/* Used for tables, etc. */
	memory_ref		rsrv;				/* Reserved for DSI extensions */
	select_pair		alt_range;			/* Range for Undo Paste & other things */
	select_pair		keyboard_delete;	/* Delete range for backspace undo */
	long			ref_con;			/* App can set this */
};

typedef struct pg_undo pg_undo;
typedef pg_undo PG_FAR *pg_undo_ptr;


/* Special record passed to the line measure proc: */

struct pg_measure {
	pg_short_t				previous_flags; /* Ending flags at last line's end */
	pg_short_t				prv_prv_flags;	/* Previous flags before above */
	pg_short_t				wrap_dimension; /* Bits defining how complex wrap is */
	pg_boolean				repeating;		/* TRUE if shape is a repeater */
	rectangle				extra_indents;	/* Any extra indents for hooks to alter */
	long					line_text_size;	/* Total use of text in line */
	long					max_text_size;	/* Maximum text line can use */
	style_walk_ptr			styles;			/* Pointer to the style_walk record */
	text_block_ptr			block;			/* Current text block */
	point_start_ptr			starts;			/* Current point start ptr */
	pg_short_t				starts_ctr;		/* Number of starts remaining */
	pg_short_t				num_starts;		/* Number of start records generated */
	long PG_FAR				*char_locs;		/* Current character locations */
	short PG_FAR			*char_types;	/* Current character types */
	long PG_FAR				*positions;		/* Original character locations */
	short PG_FAR			*types;			/* Original character types */
	rectangle				fit_rect;		/* Rect in which line must fit */
	rectangle				actual_rect;	/* Resulting rect that encloses new line */
	rectangle				wrap_bounds;	/* Bounding rect for wrap area */
	line_ref				starts_ref;		/* Memory_ref of starts */
	memory_ref				tab_info;		/* Contans tab_width_info (see below) */
	memory_ref				exclude_ref;	/* Holds "exclusion" rectangles */
	rectangle_ptr			wrap_r_base;	/* Base for shape (first rect) */
	rectangle_ptr			wrap_r_begin;	/* Top wrap rectangle */
	rectangle_ptr			wrap_r_end;		/* End wrap rectangle (often the same) */
	long					r_num_begin;	/* Current wrap-target rectangle */
	long					r_num_end;		/* Ending rect of line (for complex wrap) */
	long					end_r;			/* Ending record index for wrap rects */
	long					subpage_qty;	/* Number of rects within a "page" (0 if non-repeating) */
	long					last_page;		/* Last page number given to modify_page hook */
	long					old_last_page;	/* Previous last page */
	long					wrap_r_save;	/* Saves old bottom (I replace with 0x7FFFFFFF) */
	co_ordinate				repeat_offset;	/* Amount to add to shape (repeater mode) */
	rectangle				prev_bounds;	/* Previous start's bounds */
	long					hook_refcon;	/* Custom hooks can use this */
	long					minimum_left;	/* Minimum left side */
	long					maximum_right;	/* Maximum right side */
	pg_boolean				quick_paginate;	/* Only moving lines, not full calc */
	pg_short_t				old_offset;		/* Ending offset from old line end */
	long					adjusted_height; /* used for line adjust proc */
	long					adjusted_descent; /* used for line adjust proc */
	long					is_table;		/* Non-zero if we are paginating a table */
	long					cell_alignment;	/* Alignment for individual cell. */
	long					top_table_r;	/* The top (initial) rect for table processing */
	long					bot_table_r;	/* Bottom-most rect for table processing */
	rectangle				cell_extra;		/* Extra insets around the cell */
	pg_short_t				cell_index;		/* Current cell index (to cell element) */
	pg_short_t				cell_qty;		/* Table cell quantity (when doing tables) */
	rectangle				cell_rect;		/* Bounds for current cell */
	rectangle				table_union;	/* Union of all cells while processing */
	rectangle				table_fit_rect;	/* Initial rectangle for table pagination */
	select_pair				table_top;		/* Beginning and ending of the top row of a table */
	select_pair				table_bottom;	/* Beginning and ending of the bottom row of a table */
};

typedef struct pg_measure pg_measure;
typedef pg_measure PG_FAR *pg_measure_ptr;

typedef struct pg_hyperlink 	pg_hyperlink;
typedef pg_hyperlink PG_FAR *	pg_hyperlink_ptr;

/* The image reference record for indirect "URL" image types: */

struct pg_url_image {
	pg_char			URL[512];				/* The pathname of the image */
	pg_char			alt_string[64];			/* Alternate text if image not found */
	generic_var		image_data;				/* Resulting image data (machine-specific) */
	pg_error		loader_result;			/* Error code if load failed. */
	short			source_width;			/* Source (original) width of picture */
	short			source_height;			/* Source (original) height of picture */
	short			source_type;			/* Type of image from source file */
	long			type_and_flags;			/* The target (converted) embed data type */
	long			machine_ref1;			/* Machine-specific */
	long			machine_ref2;			/* Machine-specific */
	long			used_ctr;				/* Number of indirect usages */
};

typedef struct pg_url_image pg_url_image;
typedef pg_url_image PG_FAR *pg_url_image_ptr;

#define SIGNIFICANT_IMAGE_SIZE  (sizeof(pg_url_image) - sizeof(long))


typedef PG_FN_PASCAL(void, line_measure_proc) (paige_rec_ptr pg, pg_measure_ptr measure);
typedef PG_FN_PASCAL(void, line_adjust_proc) (paige_rec_ptr pg, pg_measure_ptr measure,
		point_start_ptr starts, pg_short_t num_starts, rectangle_ptr line_fit,
		par_info_ptr par_format);
typedef PG_FN_PASCAL(pg_boolean, line_validate_proc) (paige_rec_ptr pg, pg_measure_ptr measure_info);
typedef PG_FN_PASCAL(long, line_parse_proc) (paige_rec_ptr pg, pg_measure_ptr measure_info,
		pg_char_ptr text, point_start_ptr line_start, long global_offset, long remaining_length);
typedef PG_FN_PASCAL(void, line_init_proc) (paige_rec_ptr pg, pg_measure_ptr measure_info,
		short init_verb);
typedef PG_FN_PASCAL(pg_boolean, hyphenate_proc) (paige_rec_ptr pg, text_block_ptr block,
		style_walk_ptr styles, pg_char_ptr block_text, long line_begin, long PG_FAR *line_end,
		long PG_FAR *positions, short PG_FAR *char_types, long PG_FAR *line_width_extra,
		pg_boolean zero_length_ok);
typedef PG_FN_PASCAL(void, click_examine_proc) (paige_rec_ptr pg, short click_verb,
		short modifiers, long refcon_return, t_select_ptr begin_select,
		t_select_ptr end_select);
typedef PG_FN_PASCAL(void, hilite_rgn_proc) (paige_rec_ptr pg, t_select_ptr selections,
		pg_short_t select_qty, shape_ref rgn);
typedef PG_FN_PASCAL(void, draw_hilite_proc) (paige_rec_ptr pg, shape_ref rgn);
typedef PG_FN_PASCAL(void, draw_cursor_proc) (paige_rec_ptr pg, t_select_ptr select,
		short verb);
typedef PG_FN_PASCAL(void, do_idle_proc) (paige_rec_ptr pg, short verb);
typedef PG_FN_PASCAL(void, text_load_proc) (paige_rec_ptr pg, text_block_ptr block);
typedef PG_FN_PASCAL(long, text_break_proc) (paige_rec_ptr pg, text_block_ptr block); 
typedef PG_FN_PASCAL(void, pt2_offset_proc) (paige_rec_ptr pg, co_ordinate_ptr point,
		short conversion_info, t_select_ptr selection);
typedef PG_FN_PASCAL(pg_boolean, par_boundary_proc) (paige_rec_ptr pg, select_pair_ptr boundary);
typedef PG_FN_PASCAL(void, change_container_proc) (paige_rec_ptr pg, pg_short_t container_num,
		rectangle_ptr container, pg_scale_ptr scaling_ptr, co_ordinate_ptr screen_extra,
		short verb, void PG_FAR *misc_info);
typedef PG_FN_PASCAL(void, smart_quotes_proc) (paige_rec_ptr pg, long insert_offset,
		long info_bits, pg_char_ptr char_to_insert, short PG_FAR *insert_length);
typedef PG_FN_PASCAL(void, post_paginate_proc) (paige_rec_ptr pg, text_block_ptr block,
		smart_update_ptr update_info, long lineshift_begin, long lineshift_end,
		short action_taken_verb);
typedef PG_FN_PASCAL(void, increment_text_proc) (paige_rec_ptr pg, long base_offset,
		long increment_amount);
typedef PG_FN_PASCAL(void, font_init_proc) (paige_rec_ptr pg, font_info_ptr info);
typedef PG_FN_PASCAL(void, style_init_proc) (paige_rec_ptr pg, style_info_ptr style,
		font_info_ptr font);
typedef PG_FN_PASCAL(void, install_font_proc) (paige_rec_ptr pg, style_info_ptr style,
		font_info_ptr font, style_info_ptr superimpose, short style_overlay, pg_boolean include_offscreen);
typedef PG_FN_PASCAL(void, special_char_proc) (paige_rec_ptr pg, style_walk_ptr walker, pg_char_ptr data,
		pg_short_t offset, pg_short_t length, draw_points_ptr draw_position,
		long extra, short draw_mode);
typedef PG_FN_PASCAL(void, auto_scroll_proc) (paige_rec_ptr pg, short h_verb, short v_verb,
		co_ordinate_ptr mouse_point, short draw_mode);
typedef PG_FN_PASCAL(void, scroll_adjust_proc) (paige_rec_ptr pg, long amount_h,
		long amount_v, short draw_mode);
typedef PG_FN_PASCAL(void, draw_scroll_proc) (paige_rec_ptr pg, shape_ref update_rgn,
		co_ordinate_ptr scroll_pos, pg_boolean post_call);
typedef PG_FN_PASCAL(void, bitmap_modify_proc) (paige_rec_ptr pg, graf_device_ptr bits_port,
		pg_boolean post_call, rectangle_ptr bits_rect, co_ordinate_ptr screen_offset,
		long text_position);
typedef PG_FN_PASCAL(void, wait_process_proc) (paige_rec_ptr pg, short wait_verb,
		size_t progress_ctr, size_t completion_ctr);
typedef PG_FN_PASCAL(void, draw_page_proc) (paige_rec_ptr pg, shape_ptr page_shape,
		pg_short_t r_qty, pg_short_t page_num, co_ordinate_ptr vis_offset,
		short draw_mode_used, short call_order);
typedef PG_FN_PASCAL(void, enhance_undo_proc) (paige_rec_ptr pg, pg_undo_ptr undo_rec,
		void PG_FAR *insert_ref, short action_to_take);
typedef PG_FN_PASCAL(void, measure_proc) (paige_rec_ptr pg, style_walk_ptr walker,
		pg_char_ptr data, size_t length, pg_short_t slop, long PG_FAR *positions,
		short PG_FAR *types, short measure_verb, size_t current_offset, pg_boolean scale_widths,
		short call_order);
typedef PG_FN_PASCAL(pg_boolean, merge_proc) (paige_rec_ptr pg, style_info_ptr style, pg_char_ptr text_data,
		pg_short_t length, text_ref merged_data, long ref_con);
typedef PG_FN_PASCAL(long, track_control_proc) (paige_rec_ptr pg, short verb,
		t_select_ptr first_select, t_select_ptr last_select, style_walk_ptr styles,
		pg_char_ptr associated_text, point_start_ptr bounds_info, short modifiers,
		long track_refcon);
typedef PG_FN_PASCAL(long, char_info_proc) (paige_rec_ptr pg, style_walk_ptr walker,
		pg_char_ptr data, size_t block_offset, size_t offset_begin, size_t offset_end, size_t char_offset,
		long mask_bits);
typedef PG_FN_PASCAL(void, text_draw_proc) (paige_rec_ptr pg, style_walk_ptr walker, pg_char_ptr data,
		pg_short_t offset, pg_short_t length, draw_points_ptr draw_position,
		long extra, short draw_mode);
typedef PG_FN_PASCAL(void, dup_style_proc) (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		short reason_verb, format_ref all_styles, style_info_ptr style);
typedef PG_FN_PASCAL(void, delete_style_proc) (paige_rec_ptr pg, pg_globals_ptr globals,
		short reason_verb, format_ref all_styles, style_info_ptr style);
typedef PG_FN_PASCAL(void, alter_style_proc) (paige_rec_ptr pg, style_info_ptr old_style,
		style_info_ptr new_style, style_info_ptr style_mask);
typedef PG_FN_PASCAL(void, save_style_proc) (paige_rec_ptr pg, style_info_ptr style_to_save);
typedef PG_FN_PASCAL(void, style_activate_proc) (paige_rec_ptr pg, style_info_ptr style,
		select_pair_ptr text_range, pg_char_ptr text, short front_back_state,
		short perm_state, pg_boolean show_hilite);
typedef PG_FN_PASCAL(void, copy_text_proc) (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		short reason_verb, style_info_ptr style, size_t style_position,
		size_t text_position, pg_char_ptr text, size_t length);
typedef PG_FN_PASCAL(void, delete_text_proc) (paige_rec_ptr pg, short reason_verb,
		style_info_ptr style, long style_position, long text_position, pg_char_ptr text, long length);
typedef PG_FN_PASCAL(pg_boolean, setup_insert_proc) (paige_rec_ptr pg, style_info_ptr to_be_inserted,
		long position);
typedef PG_FN_PASCAL(size_t, bytes_to_unicode_proc) (pg_bits8_ptr input_bytes, pg_short_t PG_FAR *output_chars,
		font_info_ptr font, size_t input_byte_size);
typedef PG_FN_PASCAL(size_t, unicode_to_bytes_proc) (pg_short_t PG_FAR *input_chars, pg_bits8_ptr output_bytes,
		font_info_ptr font, size_t input_char_size);
typedef PG_FN_PASCAL(void, line_glitter_proc) (paige_rec_ptr pg, style_walk_ptr walker,
		long line_number, long par_number, text_block_ptr block, point_start_ptr first_line,
		point_start_ptr last_line, point_start_ptr previous_first, point_start_ptr previous_last,
		co_ordinate_ptr offset_extra, rectangle_ptr vis_rect, short call_verb);
typedef PG_FN_PASCAL(long, tab_measure_proc) (paige_rec_ptr pg, style_walk_ptr walker, long cur_pos,
		long cur_text_pos, long line_left, long line_right, pg_char_ptr text, pg_short_t text_length,
		long PG_FAR *char_positions, pg_short_t PG_FAR *tab_rec_info);
typedef PG_FN_PASCAL(void, tab_draw_proc) (paige_rec_ptr pg, style_walk_ptr walker, tab_stop_ptr tab_ptr,
		draw_points_ptr draw_position);
typedef PG_FN_PASCAL(void, dup_par_proc) (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		short reason_verb, par_ref all_pars, par_info_ptr par_style);
typedef PG_FN_PASCAL(void, delete_par_proc) (paige_rec_ptr pg, short reason_verb,
		par_ref all_pars, par_info_ptr par_style);
typedef PG_FN_PASCAL(void, alter_par_proc) (paige_rec_ptr pg, par_info_ptr old_par,
		par_info_ptr new_par);
typedef PG_FN_PASCAL(void, set_device_proc) (paige_rec_ptr pg, short verb, graf_device_ptr device,
		color_value_ptr bk_color);
typedef PG_FN_PASCAL(void, page_modify_proc) (paige_rec_ptr pg, size_t page_num, rectangle_ptr margins);
typedef PG_FN_PASCAL(pg_error, file_io_proc) (void PG_FAR *data, short verb, size_t PG_FAR *position,
		size_t PG_FAR *data_size, file_ref filemap);
typedef PG_FN_PASCAL(long, wordbreak_info_proc) (paige_rec_ptr pg, pg_char_ptr the_char, short charsize,
		style_info_ptr style, font_info_ptr font, long current_settings);
typedef PG_FN_PASCAL(pg_word, charclass_info_proc) (paige_rec_ptr pg, pg_char_ptr the_char, short charsize,
		style_info_ptr style, font_info_ptr font);
typedef PG_FN_PASCAL(short, key_insert_query) (paige_rec_ptr pg, pg_char_ptr the_char, short charsize);
typedef PG_FN_PASCAL(pg_boolean, pg_handler_proc) (paige_rec_ptr pg, pg_file_key key,
		memory_ref key_data, long PG_FAR *element_info, void PG_FAR *aux_data,
		size_t PG_FAR *unpacked_size);
typedef PG_FN_PASCAL(void, extend_proc) (void PG_FAR *param, short verb);
typedef PG_FN_PASCAL(void, app_init_read) (paige_rec_ptr pg, memory_ref ref, short type);	//е TRS/OITC
typedef PG_FN_PASCAL(void, subset_glitter_proc) (paige_rec_ptr pg, draw_points_ptr draw_position,
		rectangle_ptr draw_bounds, co_ordinate_ptr offset_extra, style_walk_ptr walker);
typedef PG_FN_PASCAL (long, ht_callback) (paige_rec_ptr pg, pg_hyperlink_ptr hypertext,
		short command, short modifiers, long position, pg_char_ptr URL);
typedef PG_FN_PASCAL (void, bk_image_proc) (paige_rec_ptr pg, pg_url_image_ptr image,
		generic_var device, short verb, rectangle_ptr target, rectangle_ptr clip,
		co_ordinate_ptr actual_target_offset);


/***************************  Style records & Globals ************************/

/* Style procs for custom styles: */

struct pg_style_hooks {
	style_init_proc			init;				/* Initialize style_info */
	install_font_proc		install;			/* Set up "current" font & style */
	measure_proc			measure;			/* Measure char positions */
	merge_proc				merge;				/* Substitute other text */
	char_info_proc			char_info;			/* Return info about a char */
	text_draw_proc			draw;				/* Draw the character(s) draw characters */
	dup_style_proc			duplicate;			/* Style will get duplicated */
	delete_style_proc		delete_style;		/* Style will get deleted */
	alter_style_proc		alter_style;		/* Style will get altered */
	save_style_proc			save_style;			/* Style about to be written to disk */
	copy_text_proc			copy_text;			/* Text of style will be copied */
	delete_text_proc		delete_text;		/* Text of style will be deleted */
	setup_insert_proc		insert_proc;		/* Set up the style for a future insert */
	track_control_proc		track_ctl;			/* Mouse track for "control" type style*/
	style_activate_proc		activate_proc;		/* Activate/deactivate style */
	bytes_to_unicode_proc	bytes_to_unicode;	/* Convert ASCII to Unicode */
	unicode_to_bytes_proc	unicode_to_bytes;	/* Convert Unicode to ASCII */
};

typedef struct pg_style_hooks pg_style_hooks;


/* Paragraph procs for custom paragraphs: */

struct pg_par_hooks {
	line_measure_proc	line_proc;		/* Measure a line */
	line_glitter_proc	line_glitter; 	/* Draw ornaments, line */
	tab_measure_proc	tab_width;		/* Return the tab position */
	tab_draw_proc		tab_draw;		/* Draws leaders */
	dup_par_proc		duplicate;		/* Style will get duplicated */
	delete_par_proc		delete_par;		/* Style will get deleted */
	alter_par_proc		alter_par;		/* Style will get altered */
};

typedef struct pg_par_hooks pg_par_hooks;


/* Paige general hook record */

struct pg_hooks {
	line_init_proc			line_init;		/* Initialize line measure */
	line_adjust_proc		adjust_proc;	/* Adjust a line */
	line_validate_proc		validate_line;	/* Validate (or change) a line */
	line_parse_proc			parse_line;		/* Change length for parsing */
	hyphenate_proc			hyphenate;		/* Hyphenate word */
	hilite_rgn_proc			hilite_rgn;		/* Make highlight region */
	draw_hilite_proc		hilite_draw;	/* Draw (invert) highlight */
	text_load_proc			load_proc;		/* Load text for text_block */
	text_break_proc			break_proc;		/* Find break point for a text block */
	draw_cursor_proc		cursor_proc;	/* Draw a caret */
	do_idle_proc			idle_proc;		/* Do idle processing */
	pt2_offset_proc			offset_proc;	/* Find offset matching point */
	font_init_proc			font_proc;		/* Set up fields in font_info */
	special_char_proc		special_proc;	/* Special character draw proc */
	auto_scroll_proc		auto_scroll;	/* Called for auto-scrolling during drag */
	scroll_adjust_proc		adjust_scroll;	/* Adjust scrolling values */
	draw_scroll_proc		draw_scroll;	/* Called to draw during scroll */
	draw_page_proc			page_proc;		/* Called to draw "page" stuff */
	bitmap_modify_proc		bitmap_proc;	/* Modify offscreen draw proc */
	wait_process_proc		wait_proc;		/* Called for long crunches */
	enhance_undo_proc		undo_enhance;	/* Called for custom undo's */
	par_boundary_proc		boundary_proc;	/* Called to find paragraph boundary */
	change_container_proc	container_proc;	/* Called for pieces of shape for ornaments, etc. */		
	smart_quotes_proc		smart_quotes;	/* Called to handle smart quotes */
	post_paginate_proc		paginate_proc;	/* Called after a block paginates */
	increment_text_proc		text_increment; /* Called when text is inserted or deleted */
	click_examine_proc		click_proc;		/* Called to look at clicked item */
	set_device_proc			set_device;		/* Called before and after PAIGE uses a display device */
	page_modify_proc		page_modify; 	/* Called to let app modify each (repeating) page */
	wordbreak_info_proc		wordbreak_proc;	/* Called to let app decide on special-case chars for word breaks */
	charclass_info_proc		charclass_proc; /* Called to determine the character subset type */
	key_insert_query		insert_query;	/* Called to determine if insert chars should be buffered. */
	subset_glitter_proc		subset_glitter;	/* Called for drawing DSI subsets */
	bk_image_proc			background_image;		/* Called to draw background images */
};

typedef struct pg_hooks pg_hooks;



/* The text format struct */

#define MAX_STYLES			32		/* Maximum # of styles in style_info */
#define MAX_NORMAL_STYLES	15		/* Maximum non-small-caps or custom styles */
#define DEFAULT_FONT_INDEX	-1		/* Implies the default font index in style_info */


/* IMPORTANT SOURCE CODE NOTE: See function "pgSetMaskFromFlds" in pgDefStl.c
source code, which depends on the exact sequence of fields. Hence if fields are
moved around or added in style_info this source needs to be modified. */

struct style_info {
	short		font_index;			/* What font this style is in */
	short		styles[MAX_STYLES];	/* Stylization */
	short		char_bytes;			/* Number of bytes per character - 1 */
	short		max_chars;			/* Maximum chars before new style begins */
	short		ascent;			 	/* This style's ascent */
	short		descent;			/* This style's descent */
	short		leading;			/* Regular leading */
	short		shift_verb;			/* Superscript / Subscript verb */
	short		style_num;			/* Holds the RTF stylesheet number for stylesheets (RTF) */
	short		style_basedon;		/* Holds the RTF stylesheet number which this stylesheet is based on (RTF) */
	short		next_style;			/* Holds the RTF stylesheet number which is next (RTF) */
	short		rtf_reserved;		/* Reserved RTF */
	color_value	fg_color;			/* Foreground color */
	color_value	bk_color;			/* Background color (white = transparent) */
	long		class_bits;			/* Defines selection & behavior */	//е TRS/OITC
	long		style_sheet_id;		/* Used for style sheet features */
	long		named_style_index;	/* Index to named stylesheet */
	long		small_caps_index;	/* Style_info index for small caps point size */
	size_t		machine_var;		/* Machine-specific use (hold Windows font object) */
	size_t		machine_var2;		/* Machine-specific (used for Windows font object) */
	pg_fixed	char_width;			/* Character width (not used in Mac) */
	pg_fixed	point;				/* Point (if different than pitch) */
	pg_fixed	left_overhang;		/* This style's left overhang if any */
	pg_fixed	right_overhang;	 	/* This style's right overhang if any */
	pg_fixed	top_extra;			/* This style's top leading */
	pg_fixed	bot_extra;			/* This style's bottom leading */
	pg_fixed	space_extra;		/* Extra pixels for spaces ("fixed" value) */
	pg_fixed	char_extra;			/* Extra pixels for chars ("fixed" value) */
	long		user_id;			/* Can be used by app to identify custom styles */
	long		user_data;			/* Additional space for app if style is custom */
	long		user_data2;			/* Additional space for app */
	pg_time_t	time_stamp;			/* Time stamp (Paige style ie. seconds since 00:00 01 JAN 1900) (RTF) */
	long		key_equiv;			/* Holds the keyboard equivalent for stylesheets (RTF) */
	long		future[2];			/* Reserved for future expansion */
	long		html_style;			/* Applied style from HTML import, used also for export */
	void*		embed_entry;		/* App callback function for embed_refs */
	long		embed_style_refcon; /* Used by embed object extension */
	long		embed_refcon;		/* Used by embedded object extension */
	long		embed_id;			/* Used by embedded object extension */
	memory_ref	embed_object;		/* Used by embedded object extension */
	style_append_t user_var;		/* Can be used for anything */
	pg_style_hooks procs;			/* Contains functions on how to draw */
	long		maintenance;		/* (Used internally) */
	size_t		used_ctr;			/* (used internally) */
};

/* NOTE, the following defines the number of "short" and "long" members sequentially in style_info.
Do not change unless style_info changes! */

#define STYLEINFO_SHORTS		51
#define STYLEINFO_LONGS			28

/* The hypetext record (maintained as a "style") */

struct pg_hyperlink {
	select_pair		applied_range;					// Offset(s) of source
	pg_char			URL[FONT_SIZE + BOM_HEADER];	// Document address (set by app)
	memory_ref		alt_URL;						// URL (if > FONT_SIZE - 1)
	ht_callback		callback;						// Callback function
	short			active_style;					// Style to show targetted text (zero if none)
	short			state1_style;					// Primary state style (user has not gone here)
	short			state2_style;					// Secondary state style ("current" link)
	short			state3_style;					// Third state style (user has been here)
	long			unique_id;						// Unique ID used for searching
	long			target_id;						// Used by app to locate a target by ID
	long			type;							// Type of link (used for index, toc, etc.)
	long			refcon;							// App can keep whatever
};

/* Paragraph indent struct (for "ruler" support): */

struct pg_indents {
	pg_fixed	left_indent;	/* Left margin (indent) */
	pg_fixed	right_indent;	/* Right margin (indent) */
	pg_fixed	first_indent;	/* First-line indent */
};

typedef struct pg_indents pg_indents;
typedef pg_indents PG_FAR *pg_indents_ptr;

/* The table and border format: */

struct pg_table {
	long		table_columns;		/* Number of columns (tables) */
	long		table_column_width;	/* Width of columns (tables) */
	long		table_cell_height;	/* MINIMUM cell height */
	long		border_info;		/* Holds border information (or table outside border) */
	long		border_spacing;		/* Extra spacing (around the border) */
	long		border_shading;		/* R-G-B value for border background shading */
	long		cell_borders;		/* Default, grid, border format for inside cells */
	long		grid_borders;		/* NON-PRINTABLE borders for cells, default */
	long		unique_id;			/* Unique table ID */
	long		cell_h_extra;		/* Horizontal extra (around left and right) */
	long		left_border_color;	/* Color for left border */
	long		top_border_color;	/* Color for left border */
	long		right_border_color;	/* Color for left border */
	long		bottom_border_color; /* Color for left border */
};

typedef struct pg_table pg_table;
typedef pg_table PG_FAR *pg_table_ptr;


/* Paragraph format record: */

/* IMPORTANT SOURCE CODE NOTE: See function "pgSetMaskFromFlds" in pgDefStl.c
source code, which depends on the exact sequence of fields. Hence if fields are
moved around or added in style_info this source needs to be modified. */

/* Attention source code owners: If you need more potential tab stops than 32 per paragraph,
please change TAB_ARRAY_SIZE near top of this header and re-compile. */


struct par_info {
	short		justification;	/* How text is justified */
	short		direction;		/* Primary text direction */
	short		class_info;		/* Used to define para attributes */
	short		style_num;		/* Holds the RTF stylesheet number for stylesheets (RTF) */
	short		style_basedon;	/* Holds the RTF stylesheet number which this stylesheet is based on (RTF) */
	short		next_style;		/* Holds the RTF stylesheet number which is next (RTF) */
	short		outline_level;	/* Outline level (RTF enhancement) */
	pg_short_t	num_tabs;		/* Number of active tabs */
	tab_stop	tabs[TAB_ARRAY_SIZE];	/* Tab stop information */
	pg_table	table;			/* Border and table information */
	long		style_sheet_id;	/* Used for style sheet features */
	long		named_style_index;  /* Index to named stylesheet */
	pg_fixed	def_tab_space;	/* Default tab space */
	pg_indents	indents;		/* Indentations */
	pg_fixed	spacing;		/* Line spacing */
	pg_fixed	leading_extra;	/* Extra leading of lines */
	pg_fixed	leading_fixed;  /* Fixed leading (0 = auto) */
	pg_fixed	leading_variable; /* Variable leading (0 = auto) (RTF enhancement) */
	pg_fixed	top_extra;		/* Extra space at top */
	pg_fixed	bot_extra;		/* Extra space at bottom */
	pg_fixed	left_extra;		/* Extra space at left */
	pg_fixed	right_extra;	/* Extra space at right */
	long		user_id;		/* Can be used by app to identify custom par styles */
	long		user_data;		/* Additional space for app if par is custom */
	long		user_data2;		/* More space for app */
	long		partial_just;	/* Partial justify (future enhancement) */
	long		key_equiv;		/* Holds the keyboard equivalent for stylesheets (RTF) */
	long		html_style;		/* The HTML-specific style (used for exporting) */
	long		future[6]; 		/* Reserved for future enhancement */
	long		html_bullet;	/* Holds nested level for bullet(s) */
	long		html_numbers;	/* HTML-style numeric line numbering */
	par_append_t user_var;		/* Can be used for anything */
	pg_par_hooks procs;			/* Function pointers */
	long		maintenance;	/* (Used internally) */
	long		column_var;		/* (Used internally) */
	long		used_ctr;	 	/* (used internally) */
};


/* The following defines number of short and long members in par_info.
Do not change unless par_info changes! */

#define PARINFO_SHORTS				7
#define PARINFO_TABLE_LONGS			((TAB_ARRAY_SIZE * 4) + 14)
#define PARINFO_NONTABLE_LONGS		(23 + PAR_APPEND_T_SIZE)

/* Paragraph class_info definitions: */

#define KEEP_PARS_TOGETHER	0x0001	/* Keep all pars on same page if this bit set */
// RTF Paragaph flags
#define KEEP_WHOLE_PAR		0x0002	/* Keep par on same page if this bit set */
#define PAR_ON_NEXT_PAGE	0x0004	/* Page break before par if this bit set */
#define PAR_IGNORES_ORPHAN	0x0008	/* Par ignores window/orphan control if this bit set */
#define INCLUDE_PAR_EXTRA	0x0010	/* Par include extra paragraph spacing at top and bottom of page if this bit set */
#define INCLUDE_LINE_EXTRA	0x0020	/* Par include extra line spacing at top and bottom of page if this bit set */
#define NO_LINE_NUMBER		0x0040	/* Par ignores line numbering if this bit set */
#define SIDE_BY_SIDE_PAR	0x0080	/* Pars are side by side if this bit set */
#define HYPHEN_OFF_PAR		0x0100	/* No automatic hyphenation for this par if this bit set */
#define NO_SAVEDOC_PAR		0x0200	/* Do not save this par_info in pgSaveDoc() */
#define RECALC_TO_BEGINNING 0x0400	/* When figuring wrap, go to start of paragraph */
#define FORCE_BORDER_LINES	0x0800	/* If borders, always draw for every paragraph */
#define BULLETED_LINE		0x1000	/* Lines have bullets */

#define SPECIAL_PAR_BITS	(KEEP_PARS_TOGETHER | KEEP_WHOLE_PAR | RECALC_TO_BEGINNING)

#define WIDTH_QTY	3
#define CREATED_WIDTH_QTY	(WIDTH_QTY + 1)

/* Number flags used in paragraph info (low-word is numberic beginning or zero as default): */

#define PAR_ARABIC_NUMERALS		0x00010000L		/* Arabic numbers e.g. 1, 2, 3... */
#define PAR_CAPS_NUMERALS		0x00020000L		/* Capital letters e.g. A, B, C */
#define PAR_LOWER_NUMERALS		0x00040000L		/* Lower case letters e.g. a, b, c... */
#define PAR_CAPSROMAN_NUMERALS	0x00080000L		/* Roman numerals lower case */
#define PAR_LOWERROMAN_NUMERALS	0x000A0000L		/* Roman numerals upper case */
#define PAR_DECIMAL_NUMERALS	0x40000000L		/* Numerals terminate with '.' */

struct pg_globals {
	pgm_globals_ptr		mem_globals;			/* Globals for pgMemManager */
	long				max_offscreen;			/* Maximum memory for offscreen */
	long				max_block_size;			/* Max size of text block */
	long				minimum_line_width;		/* Minimum size line width */
	long				def_tab_space;			/* Default tab spacing for pgNew */
	pg_short_t			line_wrap_char;			/* <CR> char */
	pg_short_t			soft_line_char;			/* Soft <CR> char */
	pg_short_t			tab_char;				/* Tab character */
	pg_short_t			soft_hyphen_char;		/* Soft "-" char */
	pg_short_t			bs_char;				/* Backspace char */
	pg_short_t			ff_char;				/* Formfeed char (used for page breaks) */
	pg_short_t			container_brk_char;		/* Container break character */
	pg_short_t			left_arrow_char;		/* Left arrow */
	pg_short_t			right_arrow_char;		/* Right arrow */
	pg_short_t			up_arrow_char;			/* Up arrow */
	pg_short_t			down_arrow_char;		/* Down arrow */
	pg_short_t			fwd_delete_char;		/* Forward delete */
	pg_short_t			text_brk_char;			/* Alternate Textblock break */
	pg_short_t			null_char;				/* Key that does not insert but paginates */
	pg_char				hyphen_char[4];			/* "-" char */
	pg_char				decimal_char[4];		/* "." char (for decimal tabs) */
	pg_char				cr_invis_symbol[4];		/* Char to draw for <CR> invisibles */
	pg_char				lf_invis_symbol[4];		/* Char to draw for <LF> invisibles */
	pg_char				tab_invis_symbol[4];	/* Char to draw for TAB invisibles */
	pg_char				end_invis_symbol[4];	/* Char to draw for end-doc invisibles */
	pg_char				pbrk_invis_symbol[4];	/* Char to draw for page breaks */
	pg_char				cont_invis_symbol[4];	/* Container break symbol */
	pg_char				space_invis_symbol[4];	/* Char to draw for SPACE invisibles */
	pg_char				flat_single_quote[4];	/* Flat single quote */
	pg_char				flat_double_quote[4];	/* Flat double quote */
	pg_char				left_single_quote[4];	/* Left smart quote */
	pg_char				right_single_quote[4];	/* Right smart quote */
	pg_char				left_double_quote[4];	/* Left smart double quote */
	pg_char				right_double_quote[4];	/* Right smart double quote */
	pg_char		 		elipse_symbol[4];		/* Char to draw for "elipse" */
	pg_char				unknown_char[4];		/* Character to use for unsupported chars */
	size_t				invis_font;				/* Machine-specific invisible char font */
	void				*embed_callback_proc;	/* Used by embed_ref engine */
	font_info			def_font;				/* Default font for all pgNew's */
	style_info			def_style;				/* Default style for all pgNew's */
	par_info			def_par;				/* Default para for all pgNew's */
	color_value			def_bk_color;			/* Default background color */
	color_value			trans_color;			/* Transparent color (white = default) */
	pg_char				bullet_char[4];			/* Bullet character */
	pg_hooks			def_hooks;				/* Default general hooks */
	app_init_read		app_init_proc;			/* Used to init extension data in app */
	memory_ref			file_handlers;			/* Contains file I/O handlers */
	shape_ref			offscreen_exclusion;	/* Shape to hold possible offscreen exclusions */
	graf_device_ptr		current_port;			/* Current graphics device */
	graf_device			offscreen_port;			/* Used for offscreen drawing */
	rectangle			offscreen_target;		/*  "    "      "        "    */
	rectangle			original_target;		/*  "    "      "        "    */
	memory_ref			offscreen_buf;			/* Used for offscreen */
	short				offscreen_enable;		/* TRUE if in offscreen mode */
	short				color_enable;			/* Used to detect color screens */
	long				system_version;			/* Used to know what system */
	size_t				machine_const;			/* Can be used by your machine */
	long				bullet_size;			/* Size of HTML bullet chars */
	memory_ref			machine_specific;		/* Can hold whatever, MACHINE-DEPENDENT! */
	memory_ref			font_objects;			/* For Windows, built font objects */
	memory_ref			alpha_widths;			/* Used for ExtTextOut in Windows */
	char_widths 		width_tables[CREATED_WIDTH_QTY]; /* Array of width tables */
	pgHyphenInfoRef		hyphenInfoRef;			/* Hyphenation Information */
	extend_proc			pg_extend;				/* Used by add-ons */
	memory_ref			g_extn_mgr;				/* Used by the extensions manager */
	pg_list_ref			pg_list;				/* List of all active pg_refs */
};

/* Used for saving/reading critical globals: */

#define SHORT_GLOBAL_CHARS		14
#define LONG_GLOBAL_CHARS		(sizeof(pg_char) * 17) * 4

/* File handler record struct (an array of these exists in globals->file_handler_ref */

struct pg_handler {
	pg_file_key			key;				/* Parameter file key */
	pg_short_t			flags;			 	/* Internal maintenance */
	pg_handler_proc		read_handler;		/* Called to handle read-in data */
	pg_handler_proc		write_handler;		/* Called to handle write-in data */
	file_io_proc		read_data_proc;		/* Called to read the data */
	file_io_proc		write_data_proc; 	/* Called to write the data */
};

typedef struct pg_handler pg_handler;
typedef pg_handler PG_FAR *pg_handler_ptr;


/************************* Paige Object Structs ******************************/


/* The basic Paige record: */

struct paige_rec {
	pg_globals_ptr		globals;			/* Pointer to global vars */
	long				version;			/* Version this was created with */
	long				platform;			/* Platform ID created on */
	long				author;				/* Application signature */
	long				flags;				/* Attributes (set in pgNew) */
	long				flags2;				/* Extra attributes */ //ее TRS/OITC
	long				shared_flags;		/* Shared structure flags (version 2.0) */
	long				pg_type;			/* Paige type (RTF enhancement) */
	long				io_mask_bits;		/* Bit(s) to mask individual file keys */
	long				tab_compensate;		/* Amount to add or subtract for tab import, export */
	pg_ref				shared_pg;			/* Common pg_ref we are sharing with */
	pg_ref				doc_pg;				/* Main document reference (for subdocuments) (RTF) */
	long				t_length;			/* Total length of text */
	select_pair			first_word;			/* Boundaries of original word/par/line selection */
	select_pair			last_word;			/* Boundaries of previous word/par/line selection */
	long				tab_base;			/* Left edge used for measuring tabs */
	co_ordinate			scroll_pos;			/* Scrolled position (that affects display) */
	co_ordinate			previous_scroll;	/* Previous scroll position (in pixels) */
	co_ordinate			logical_scroll_pos;	/* The logical or "real" scrolled position */
	co_ordinate			stable_caret;		/* Co-ordinate of stable cursor */
	co_ordinate			relative_caret;		/* Used internally by scrolling functions */
	co_ordinate			window_origin;		/* For Windows caret when changing origins */
	rectangle			doc_bounds;			/* Physical max bounds of doc */
	long				doc_top;			/* Top vertical position */
	long				doc_bottom;			/* Bottom point (might be different than doc_height) */
	long				hilite_flags;		/* Method(s) for highlighting */
	long				hilite_anchor;		/* Drag-select anchor point for extend-select */
	pg_scale_factor	 	scale_factor;		/* Scaling factor */
	color_value			bk_color;			/* Default background color */
	long				resolution;			/* Fundamental resolution (HiWord h LoWord v) */
	pg_fixed			real_scaling;		/* Original "real" scaling factor */
	short				unit_h;				/* Scroll unit_h (see docs) */
	short				unit_v;				/* Scroll unit_v (see docs) */
	short				append_h;			/* Extra pixel append for h-scroll */
	short				append_v;			/* Extra pixel append for v-scroll */
	short				last_h;				/* Last h-scroll from pgGetScrollValues */
	short				last_v;				/* Last v-scroll from pgGetScrollValues */
	short				last_max_h;			/* Last max-h from pgGetScrollValues */
	short				last_max_v;			/* Last max-v from pgGetScrollValues */
	pg_short_t			insert_style;		/* Text style for insertion */
	pg_short_t			num_selects;		/* Number of selections */
	short				key_buffer_mode;	/* Display mode for buffered keys */
	short				autoscroll_mode;	/* Display mode used for autoscroll */
	short				action_state;		/* Indicates current action, if any */
	pg_short_t			def_style_index;	/* Default style index */
	long				def_named_index;	/* Default -named- style index */
	long				scroll_align_h;		/* Alignment for horizontal scroll */
	long				scroll_align_v;		/* Alignment for vertical scroll */
	shape_section		last_drawn_bottom;	/* Last bottom point drawn in wrap area */
	shape_section		last_drawn_top;		/* Last top point drawn in wrap area */
	graf_device			port;				/* Default graphics port to draw to */
	pg_ref				myself;				/* My own pg_ref */
	select_ref			select;				/* Selection points */
	shape_ref			vis_area;			/* Visual area */
	shape_ref			exclude_area;		/* Area to exclude text */
	shape_ref			wrap_area;			/* Intersection of page/exclude */
	shape_ref			hilite_rgn;			/* Area for highlight draw */
	shape_ref			temp_rgn;			/* Used to draw partial highlights */
	size_t				forced_break;		/* Used for temporary page break when paginating */
	co_ordinate			base_vis_origin;	/* Base (original) vis area top-left point */
	block_ref			t_blocks;			/* Text block array */
	block_ref			tlevel_blocks;		/* Top level blocks (when dealing with hiearchy subrefs) */
	format_ref			t_formats;			/* Text style records */
	par_ref				par_formats;		/* Paragraph style records */
	font_ref			fonts;				/* Font table */
	memory_ref			hyperlinks;			/* Holds hypertext link data (source)*/
	memory_ref			target_hyperlinks;	/* Holds hypertext link data (target, e.f. from elsewhere)*/
	style_ref			t_style_run;		/* Run for styles */
	style_ref			par_style_run;		/* Run for paragraph styles */
	style_ref			named_styles;		/* Named stylesheet list */
	style_ref			par_exclusions;		/* List of paragraph exclusion areas */
	pg_hooks			procs;				/* General hooks */
	char_widths			last_locs;			/* Last char widths computed */
	unsigned long		timer_info;			/* Used for caret toggles */
	long				last_key_check;		/* Last time key insert checked bad blocks */
	long				overflow_size;		/* Amount of bytes that overflow page bottom */
	pg_ref				merge_save;			/* List of "pushed" merged text or NULL */
	long				change_ctr;			/* Changes counter */
	long				mem_id;				/* Common ID all memory_refs were created */
	memory_ref			key_buffer;			/* Buffer's chars for pgInsert */
	memory_ref			applied_range;		/* Used for copy/pastes */
	memory_ref			exclusions;			/* List of exclusion refCons */
	pg_doc_info			doc_info;			/* Info about the page shape */
	memory_ref			containers;			/* List of container refCons */
	pg_subref			active_subset;		/* - reserved for DSI, do not use - */
	pg_subref			subset_focus;		/* - reserved for DSI, do not use - */
	long				text_direction;		/* Last known System text direction */
	long				lock_id;			/* Used for asynchronous apps */
	long				next_id;			/* Used to produce unique "ID" */
	long				section_gap;		/* Vertical gap for section */
	long				subref_level;		/* Level of editable subrefs (** DSI ONLY **) */
	long				notification;		/* Used by Windows custom control */
	long				current_section;	/* Current section (index into doc_info array) */
	pg_fixed			hdr_ftr_loc[2];		/* Amount of space from the top of the page for the header and footer (RTF) */
	pg_ref				headers[4];			/* Headers section [main, left, right, first] (RTF) */
	pg_ref				footers[4];			/* Footers section [main, left, right, first] (RTF) */
	pg_ref				list_columns;		/* For DSI listbox mode extension */
	memory_ref			bk_image;			/* Background image (or MEM_NULL) */
	memory_ref			sections;			/* Array of pg_doc_info that defines sections. */
	memory_ref			import_control;		/* Non-null when in "import" mode (set by pgBeginImport) */
	memory_ref			import_base;		/* Document-import base (contains platform-specific pathname) */
	memory_ref			export_base;		/* Document-import base (contains platform-specific pathname) */
	memory_ref			url_list_ref;		/* Used in file-open for image URL(s) */
	memory_ref			custom_control;		/* Used by Windows custom control */
	memory_ref			global_styles;		/* If non zero this is the Parent named stylesheet pg_ref. */
	memory_ref			extra_stuff;		/* Used by apps and add-ons */
	memory_ref			buf_special;		/* Used for drawing invis spaces, SMALL CAPS, etc. */
	memory_ref			subref_stack;		/* Preserves original pg_ref when subset selection made */
	memory_ref			extn_mgr;			/* Used by the extensions manager */ //ее TRS/OITC
	memory_ref			authors;			/* Contains list of authors */ //ее TRS/OITC
	memory_ref			bookmarks;			/* Contains list of bookmarks */ //ее TRS/OITC
	memory_ref			annotations;		/* Contains list of annotations */ //ее TRS/OITC
	memory_ref			document_refs;		/* Contains list of document references */ //ее TRS/OITC
	memory_ref			document_info;		/* Contains list of document information */ //ее TRS/OITC
	memory_ref			html_info;			/* Contains list of HTML/SGML information */ //ее TRS/OITC
	pg_list_ref			idle_list;			/* List of all active special idle records */
	file_ref			cache_file;			/* Open file ref for disk cache (2.0) */
	file_ref			cache_target_file;	/* File we are writing to (if save-doc) 2.0 */
	file_io_proc		cache_read_proc;	/* The file read function for disk cache (2.0) */
	pg_char_ptr			cross_table[2];		/* Character mapping table for cache-read (2.0) */
	long				user_refcon;		/* Can be used by app */
};


/************************** Low-level Structs & Utilities ******************/

/* Record to make it easier to advance through styles: */

struct style_walk {
	size_t				current_offset;	/* Current style offset position */
	style_info_ptr		cur_style;		/* Current text style */
	par_info_ptr		cur_par_style;	/* Current paragraph style */
	font_info_ptr		cur_font;		/* Current font record */
	style_run_ptr		next_style_run;	/* Next style run record */
	style_run_ptr		next_par_run;	/* Next paragraph run record */
	style_run_ptr		prev_style_run;	/* Previous style run */
	style_run_ptr		prev_par_run;	/* Previous paragraph style run */
	style_info_ptr		style_base;		/* used internally */
	par_info_ptr		par_base;		/* used internally */
	font_info_ptr		font_base;		/* used internally */
	pg_hyperlink_ptr	hyperlink_base;	/* used internally */
	pg_hyperlink_ptr	hyperlink;		/* Next hyperlink keyword range */
	pg_hyperlink_ptr	hyperlink_target_base;	/* used internally */
	pg_hyperlink_ptr	hyperlink_target;		/* Next hyperlink keyword range */
	style_info			superimpose;	/* Composite superimpose style */
	long				last_font;		/* used internally */
	long				t_length;		/* Text size from original pg_ref */
	long				flags2;			/* Flags2 from paige rec. */
	short				style_overlay;	/* If superimposed for any reason, the stylesheet overlay */
	short				rsrvd;

/* The following are used only for PG_DEBUG: */

	pg_globals_ptr		globals;		/* Used for checking things in debug mode */
	memory_ref			stylerun;		/* Used in debug mode */
	memory_ref			parrun;			/* Used in debug mode */
	long				max_styles;		/* Used to detect overrun in debug mode */
	long				max_pars;		/* Used to detect overrun in debug mode */
};

/* Inline macros for "GetDefault..." style, para, font: */

#define pgGetDefaultPar(pg, def_par)  pgGetParInfoRec(pg, 0, def_par)
#define pgGetDefaultFont(pg, def_font)  pgGetFontInfoRec(pg, 0, def_font)

#ifdef __cplusplus
extern	"C" {
#endif


extern PG_PASCAL (void) pgPrepareStyleWalk (paige_rec_ptr pg, size_t offset,
		style_walk_ptr walker, pg_boolean include_pars);
extern PG_PASCAL (pg_boolean) pgWalkNextStyle (style_walk_ptr walker);
extern PG_PASCAL (pg_boolean) pgWalkPreviousStyle (style_walk_ptr walker);
extern PG_PASCAL (pg_boolean) pgWalkStyle (style_walk_ptr walker, size_t amount);
extern PG_PASCAL (pg_boolean) pgSetWalkStyle (style_walk_ptr walker, size_t position);

/* Async Lock/Unlock */

extern PG_PASCAL (pg_boolean) pgSetLockID (pg_ref pg, long ID);
extern PG_PASCAL (long) pgGetLockID (pg_ref pg);


/**************************  Main Functions ***************************/


extern PG_PASCAL (void) pgInit (pg_globals_ptr globals, const pgm_globals_ptr mem_globals);
extern PG_PASCAL (void) pgShutdown (const pg_globals_ptr globals);
extern PG_PASCAL (pg_globals_ptr) pgGetGlobals (pg_ref pg);
extern PG_PASCAL (void) pgSetStandardProcs (pg_globals_ptr globals);
extern PG_PASCAL (pg_ref) pgNew (const pg_globals_ptr globals, const generic_var def_device,
		shape_ref vis_area, shape_ref page_area, shape_ref exclude_area,
		long attributes);
extern PG_PASCAL (pg_ref) pgNewShared (pg_ref shared_from, const generic_var def_device,
		shape_ref vis_area, shape_ref page_area, shape_ref exclude_area,
		long attributes);
extern PG_PASCAL (void) pgSetAuthor (pg_ref pg, long author);
extern PG_PASCAL (long) pgGetAuthor (pg_ref pg);
extern PG_PASCAL (long) pgGetType (pg_ref pg);
extern PG_PASCAL (void) pgSetType (pg_ref pg, long new_type);
extern PG_PASCAL (long) pgUniqueID (pg_ref pg);
extern PG_PASCAL (void) pgDispose (pg_ref pg);
extern PG_PASCAL (void) pgFailureDispose (pg_ref pg);
extern PG_PASCAL (pg_ref) pgNewShell (const pg_globals_ptr globals);
extern PG_PASCAL (pg_ref) pgDuplicate (pg_ref pg);
extern PG_PASCAL (void) pgShareRefs (pg_ref pg, pg_ref shared_from, long shared_flags);
extern PG_PASCAL (void) pgSetDefaultDevice (pg_ref pg, const graf_device_ptr device);
extern PG_PASCAL (void) pgGetDefaultDevice (pg_ref pg, graf_device_ptr device);
extern PG_PASCAL (void) pgInitDevice (const pg_globals_ptr globals, const generic_var the_port,
		size_t machine_ref, graf_device_ptr device);
extern PG_PASCAL (void) pgInitSameDevice (pg_ref pg, const graf_device_ptr device);
extern PG_PASCAL (void) pgCloseDevice (const pg_globals_ptr globals, const graf_device_ptr device);
extern PG_PASCAL (generic_var) pgSetDrawingDevice (pg_ref pg, const generic_var draw_device);
extern PG_PASCAL (generic_var) pgSetScaledDrawingDevice (pg_ref pg, const generic_var draw_device,
		pg_scale_ptr scale);
extern PG_PASCAL (void) pgReleaseDrawingDevice (pg_ref pg, const generic_var previous_device);
extern PG_PASCAL (void) pgSetDevicePalette (pg_ref pg, const generic_var palette);
extern PG_PASCAL (generic_var) pgGetDevicePalette (pg_ref pg);
extern PG_PASCAL (void) pgSetOrigin (pg_ref pg, const co_ordinate_ptr origin);
extern PG_PASCAL (void) pgGetOrigin (pg_ref pg, co_ordinate_ptr origin);
extern PG_PASCAL (void) pgSetAreas (pg_ref pg, shape_ref vis_area, shape_ref page_area,
		shape_ref exclude_area);
extern PG_PASCAL (void) pgGetAreas (pg_ref pg, shape_ref vis_area, shape_ref page_area,
		shape_ref exclude_area);
extern PG_PASCAL (void) pgAreaBounds (pg_ref pg, rectangle_ptr page_bounds,
		rectangle_ptr vis_bounds);
extern PG_PASCAL (void) pgSetAreaBounds (pg_ref pg, const rectangle_ptr page_bounds,
		const rectangle_ptr vis_bounds);
extern PG_PASCAL (shape_ref) pgGetPageArea (pg_ref pg);
extern PG_PASCAL (shape_ref) pgGetVisArea (pg_ref pg);
extern PG_PASCAL (shape_ref) pgGetExcludeArea (pg_ref pg);
extern PG_PASCAL (void) pgOffsetAreas (pg_ref pg, long h, long v, pg_boolean offset_page,
		pg_boolean offset_vis, pg_boolean offset_exclude);
extern PG_PASCAL (void) pgWindowOriginChanged (pg_ref pg, const co_ordinate_ptr original_origin,
		co_ordinate_ptr new_origin);
extern PG_PASCAL (long) pgGetAttributes (pg_ref pg);
extern PG_PASCAL (pg_boolean) pgSetAttributes (pg_ref pg, long attributes);
extern PG_PASCAL (long) pgGetAttributes2 (pg_ref pg);
extern PG_PASCAL (pg_boolean) pgSetAttributes2 (pg_ref pg, long attributes2);
extern PG_PASCAL (void) pgGrowVisArea (pg_ref pg, const co_ordinate_ptr top_left, const co_ordinate_ptr bot_right);
extern PG_PASCAL (void) pgSetHooks (pg_ref pg, const pg_hooks PG_FAR *hooks, pg_boolean inval_text);
extern PG_PASCAL (void) pgGetHooks (pg_ref pg, pg_hooks PG_FAR *hooks);
extern PG_PASCAL (void) pgSetExtraStruct (pg_ref pg, const void PG_FAR *extra_struct, long ref_id);
extern PG_PASCAL (void PG_FAR *) pgGetExtraStruct (pg_ref pg, long ref_id);
extern PG_PASCAL (long) pgExtraUniqueID (pg_ref pg);
extern PG_PASCAL (void) pgDisplay (pg_ref pg, const graf_device_ptr target_device, shape_ref vis_target,
		shape_ref wrap_target, const co_ordinate_ptr offset_extra, short draw_mode);
extern PG_PASCAL (void) pgTextboxDisplay (pg_ref pg, paige_rec_ptr target_pg, const rectangle_ptr target_box,
		const rectangle_ptr wrap_rect, short draw_mode);
extern PG_PASCAL (pg_boolean) pgInsert (pg_ref pg, const pg_char_ptr data, size_t length, size_t position,
		short insert_mode, short modifiers, short draw_mode);
extern PG_PASCAL (pg_boolean) pgInsertText (pg_ref pg, pg_char_ptr data, long length,
		font_info_ptr font, style_info_ptr style, par_info_ptr paragraph, short draw_mode);
extern PG_PASCAL (void) pgBeginImport (pg_ref pg, long import_position);
extern PG_PASCAL (void) pgEndImport (pg_ref pg, pg_boolean keep_selection, short draw_mode);
extern PG_PASCAL (pg_boolean) pgIdle (pg_ref pg);
extern PG_PASCAL (pg_boolean) pgGlobalIdle (pg_globals_ptr globals);
extern PG_PASCAL (pg_boolean) pgInsertPendingKeys (pg_ref pg);
extern PG_PASCAL (long) pgDragSelect (pg_ref pg, const co_ordinate_ptr location, short verb,
		short modifiers, long track_refcon, pg_boolean auto_scroll);
extern PG_PASCAL (void) pgSetHiliteStates (pg_ref pg, short front_back_state,
		short perm_state, pg_boolean should_draw);
extern PG_PASCAL (void) pgGetHiliteStates (pg_ref pg, short PG_FAR *front_back_state,
		short PG_FAR *perm_state);
extern PG_PASCAL (long) pgGetStyleInfo (pg_ref pg, const select_pair_ptr selection,
		pg_boolean set_any_match, style_info_ptr info, style_info_ptr mask);
extern PG_PASCAL (pg_boolean) pgFindStyleInfo (pg_ref pg, size_t PG_FAR *begin_position,
		size_t PG_FAR *end_position, style_info_ptr match_style, style_info_ptr mask,
		style_info_ptr AND_mask);
extern PG_PASCAL (void) pgSetStyleInfo (pg_ref pg, const select_pair_ptr selection,
		const style_info_ptr info, const style_info_ptr mask,  short draw_mode);
extern PG_PASCAL (long) pgGetStyleClassInfo (pg_ref pg, long position, select_pair_ptr style_range);
extern PG_PASCAL (void) pgSetStyleClassInfo (pg_ref pg, const select_pair_ptr range, long class_AND,
		long class_OR, short draw_mode);
extern PG_PASCAL (void) pgSetInsertionStyles (pg_ref pg, const style_info_ptr style, const font_info_ptr font);
extern PG_PASCAL (long) pgGetParInfo (pg_ref pg, const select_pair_ptr selection,
		pg_boolean set_any_match, par_info_ptr info, par_info_ptr mask);
extern PG_PASCAL (void) pgSetParInfo (pg_ref pg, const select_pair_ptr selection,
		const par_info_ptr info, const par_info_ptr mask, short draw_mode);
extern PG_PASCAL (long) pgGetParInfoEx (pg_ref pg, const select_pair_ptr selection,
		pg_boolean set_any_match, par_info_ptr info, par_info_ptr mask, pg_boolean tables_only);
extern PG_PASCAL (void) pgSetParInfoEx (pg_ref pg, const select_pair_ptr selection,
		const par_info_ptr info, const par_info_ptr mask, pg_boolean tables_only, short draw_mode);
extern PG_PASCAL (void) pgSetFontInfo (pg_ref pg, const select_pair_ptr selection,
		const font_info_ptr info, const font_info_ptr mask, short draw_mode);
extern PG_PASCAL (void) pgSetStyleAndFont (pg_ref pg, const select_pair_ptr selection,
		const style_info_ptr the_style, const style_info_ptr style_mask,
		const font_info_ptr font, const font_info_ptr font_mask, short draw_mode);
extern PG_PASCAL (long) pgGetFontInfo (pg_ref pg, const select_pair_ptr selection,
		pg_boolean set_any_match, font_info_ptr info, font_info_ptr mask);
extern PG_PASCAL (font_ref) pgGetFontTable (pg_ref pg);
extern PG_PASCAL (short) pgCrossFont (pg_ref src_pg, pg_ref new_pg, short src_index);
extern PG_PASCAL (void) pgInitStyleMask (style_info_ptr mask, short filler);
extern PG_PASCAL (void) pgInitParMask (par_info_ptr mask, short filler);
extern PG_PASCAL (void) pgInitFontMask (font_info_ptr mask, short filler);
extern PG_PASCAL (void) pgSetStyleProcs (pg_ref pg, const pg_style_hooks PG_FAR *procs,
		const style_info_ptr match_style, const style_info_ptr mask_style,
		const style_info_ptr AND_style, long user_data, long user_id,
		pg_boolean inval_text, short draw_mode);
extern PG_PASCAL (void) pgSetParProcs (pg_ref pg, const pg_par_hooks PG_FAR *procs,
		const par_info_ptr match_style, const par_info_ptr mask_style,
		const par_info_ptr AND_style, long user_data, long user_id,
		pg_boolean inval_text, short draw_mode);
extern PG_PASCAL (size_t) pgPtToStyleInfo (pg_ref pg, const co_ordinate_ptr point,
		short conversion_info, style_info_ptr style, select_pair_ptr range);
extern PG_PASCAL (void) pgGetStyleInfoRec (pg_ref pg, short style_item, style_info_ptr format);
extern PG_PASCAL (void) pgGetParInfoRec (pg_ref pg, short style_item, par_info_ptr format);
extern PG_PASCAL (void) pgGetFontInfoRec (pg_ref pg, short font_index, font_info_ptr info);
extern PG_PASCAL (void) pgSetTabBase (pg_ref pg, long tab_base);
extern PG_PASCAL (long) pgGetTabBase (pg_ref pg);
extern PG_PASCAL (void) pgSetTab (pg_ref pg, const select_pair_ptr selection, const tab_stop_ptr tab_ptr,
		short draw_mode);
extern PG_PASCAL (void) pgGetTabList (pg_ref pg, const select_pair_ptr selection, tab_ref tabs,
		memory_ref tab_mask, long PG_FAR *screen_offset);
extern PG_PASCAL (void) pgSetTabList (pg_ref pg, const select_pair_ptr selection, tab_ref tabs,
		memory_ref tab_mask, short draw_mode);
extern PG_PASCAL (void) pgSetSpecialTabFlags (pg_ref pg, const select_pair_ptr selection,
		memory_ref tab_flags, pg_boolean inval_text, short draw_mode);
extern PG_PASCAL (void) pgClearSpecialTabFlags (pg_ref pg, const select_pair_ptr selection,
		pg_boolean inval_text, short draw_mode);
extern PG_PASCAL (long) pgParMaxWidth (pg_ref pg, long position);
extern PG_PASCAL (void) pgGetIndents (pg_ref pg, const select_pair_ptr selection, pg_indents_ptr indents,
		pg_indents_ptr mask, long PG_FAR *left_screen_offset, long PG_FAR *right_screen_offset);
extern PG_PASCAL (void) pgSetIndents (pg_ref pg, const select_pair_ptr selection, const pg_indents_ptr indents,
		const pg_indents_ptr mask, short draw_mode);
extern PG_PASCAL (short) pgNewStyle (pg_ref pg, const style_info_ptr new_style, const font_info_ptr style_font);
extern PG_PASCAL (void) pgRemoveStyle (pg_ref pg, short style_id);
extern PG_PASCAL (short) pgNumStyles (pg_ref pg);
extern PG_PASCAL (short) pgMaxStylesheetID (pg_ref pg);
extern PG_PASCAL (pg_boolean) pgGetStyle (pg_ref pg, short style_id, style_info_ptr style);
extern PG_PASCAL (void) pgChangeStyle (pg_ref pg, short style_id, const style_info_ptr style,
		const font_info_ptr style_font, short draw_mode);
extern PG_PASCAL (void) pgSetStyleSheet (pg_ref pg, const select_pair_ptr selection,
		short style_id, short draw_mode);
extern PG_PASCAL (pg_boolean) pgGetStyleSheet (pg_ref pg, const select_pair_ptr selection, short PG_FAR *style_id);
extern PG_PASCAL (short) pgFindStyleSheet (pg_ref pg, const style_info_ptr compare_style,
		const style_info_ptr mask);
extern PG_PASCAL (short) pgGetIndStyleSheet (pg_ref pg, short index, style_info_ptr stylesheet);
extern PG_PASCAL (short) pgNewParStyle (pg_ref pg, const par_info_ptr new_style);
extern PG_PASCAL (void) pgRemoveParStyle (pg_ref pg, short style_id);
extern PG_PASCAL (short) pgNumParStyles (pg_ref pg);
extern PG_PASCAL (short) pgMaxParStylesheetID (pg_ref pg);
extern PG_PASCAL (pg_boolean) pgGetParStyle (pg_ref pg, short style_id, par_info_ptr style);
extern PG_PASCAL (void) pgChangeParStyle (pg_ref pg, short style_id, const par_info_ptr style,
		short draw_mode);
extern PG_PASCAL (void) pgSetParStyleSheet (pg_ref pg, const select_pair_ptr selection,
		short style_id, short draw_mode);
extern PG_PASCAL (pg_boolean) pgGetParStyleSheet (pg_ref pg, const select_pair_ptr selection, short PG_FAR *style_id);
extern PG_PASCAL (short) pgFindParStyleSheet (pg_ref pg, const par_info_ptr compare_style,
		const par_info_ptr mask);
extern PG_PASCAL (short) pgGetIndParStyleSheet (pg_ref pg, short index, par_info_ptr stylesheet);
extern PG_PASCAL (long) pgNewNamedStyle (pg_ref pg, pg_c_string_ptr stylename, const style_info_ptr style,
		const font_info_ptr font, par_info_ptr par);
extern PG_PASCAL (long) pgNewNamedStyleMerge (pg_ref pg, pg_c_string_ptr stylename, const style_info_ptr style,
		const font_info_ptr font, par_info_ptr par);
extern PG_PASCAL (void) pgMergeNamedStyles (pg_ref pg, pg_c_string_ptr source_style,
		pg_c_string_ptr target_style, short draw_mode);
extern PG_PASCAL (long) pgAddNamedStyle (pg_ref pg, pg_c_string_ptr stylename, const short style_id, const short par_id);
extern PG_PASCAL (void) pgApplyNamedStyle (pg_ref pg, select_pair_ptr selection, pg_c_string_ptr stylename,
		short draw_mode);
extern PG_PASCAL (void) pgApplyNamedStyleIndex (pg_ref pg, select_pair_ptr selection, long index, short draw_mode);
extern PG_PASCAL (long) pgNumNamedStyles (pg_ref pg);
extern PG_PASCAL (long) pgGetNamedStyleIndex (pg_ref pg, pg_c_string_ptr stylename);
extern PG_PASCAL (void) pgGetNamedStyleInfo (pg_ref pg, long index, style_info_ptr style,
		font_info_ptr font, par_info_ptr par);
extern PG_PASCAL (pg_boolean) pgGetNamedStyle (pg_ref pg, long named_style_index, named_stylesheet_ptr named_style);
extern PG_PASCAL (pg_boolean) pgGetAppliedNamedStyle (pg_ref pg, select_pair_ptr selection, pg_c_string_ptr stylename);
extern PG_PASCAL (void) pgDeleteNamedStyle (pg_ref pg, long named_style_index);
extern PG_PASCAL (void) pgRenameStyle (pg_ref pg, long named_style_index, pg_c_string_ptr style_name);
extern PG_PASCAL (void) pgSetDefaultNamedStyle (pg_ref pg, long def_index);
extern PG_PASCAL (long) pgGetDefaultNamedStyle (pg_ref pg);
extern PG_PASCAL (memory_ref) pgSetGlobalNamedStyles (pg_ref pg, pg_ref global_pg,
			pg_boolean remove_from_child, pg_boolean merge_duplicates);
extern PG_PASCAL (void) pgUnsetGlobalNamedStyles (pg_ref pg);
extern PG_PASCAL (pg_boolean) pgUpdateNamedStyleChild (pg_ref pg, pg_char_ptr stylename, pg_boolean deleted);
extern PG_PASCAL (long) pgFindSameNamedStyle (pg_ref pg, style_info_ptr style, font_info_ptr font,
			par_info_ptr par, long exclude_mask);
extern PG_PASCAL (pg_boolean) pgMergeText (pg_ref pg, const style_info_ptr matching_style,
		const style_info_ptr mask, const style_info_ptr AND_mask, long ref_con, short draw_mode);
extern PG_PASCAL (void) pgRestoreMerge (pg_ref pg, pg_boolean revert_original, short draw_mode);
extern PG_PASCAL (pg_ref) pgCut (pg_ref pg, const select_pair_ptr selection, short draw_mode);
extern PG_PASCAL (pg_ref) pgCopy (pg_ref pg, const select_pair_ptr selection);
extern PG_PASCAL (text_ref) pgCopyText (pg_ref pg, const select_pair_ptr selection, short data_type);
extern PG_PASCAL (void) pgPaste (pg_ref pg, pg_ref paste_ref, long position,
		short text_only, short draw_mode);
extern PG_PASCAL (void) pgDelete (pg_ref pg, const select_pair_ptr delete_range, short draw_mode);
extern PG_PASCAL (undo_ref) pgPrepareUndo (pg_ref pg, short verb, void PG_FAR *insert_ref);
extern PG_PASCAL (undo_ref) pgUndo (pg_ref pg, undo_ref ref, pg_boolean requires_redo,
		short draw_mode);
extern PG_PASCAL (short) pgUndoType (undo_ref ref);
extern PG_PASCAL (void) pgSetUndoRefCon (undo_ref ref, long refCon);
extern PG_PASCAL (long) pgGetUndoRefCon (undo_ref ref);
extern PG_PASCAL (void) pgDisposeUndo (undo_ref ref);
extern PG_PASCAL (pg_char_ptr) pgExamineText (pg_ref pg, long offset, text_ref *text, size_t PG_FAR *length);
extern PG_PASCAL (long) pgNumTextblocks (pg_ref pg);
extern PG_PASCAL (long) pgGetTextblock (pg_ref pg, long offset, text_block_ptr block,
		pg_boolean want_pagination);
extern PG_PASCAL (long) pgCharType (pg_ref pg, long offset, long mask_bits);
extern PG_PASCAL (pg_short_t) pgCharByte (pg_ref pg, size_t offset, pg_char_ptr char_bytes);
extern PG_PASCAL (long) pgTextSize (pg_ref pg);
extern PG_PASCAL (void) pgFindWord (pg_ref pg, size_t offset, size_t PG_FAR *first_byte,
		size_t PG_FAR *last_byte, pg_boolean left_side, pg_boolean smart_select);
extern PG_PASCAL (void) pgFindCtlWord (pg_ref pg, size_t offset, size_t PG_FAR *first_byte,
		size_t PG_FAR *last_byte, short left_side);
extern PG_PASCAL (void) pgFindPar (pg_ref pg, size_t offset, size_t PG_FAR *first_byte,
		size_t PG_FAR *last_byte);
extern PG_PASCAL (void) pgFindLine (pg_ref pg, size_t offset, size_t PG_FAR *first_byte,
		size_t PG_FAR *last_byte);
extern PG_PASCAL (long) pgFindCharType (pg_ref pg, long char_info, size_t PG_FAR *offset, pg_char_ptr the_byte);
extern PG_PASCAL (void) pgGetSelection (pg_ref pg, size_t PG_FAR *begin_sel, size_t PG_FAR *end_sel);
extern PG_PASCAL (void) pgSetSelection (pg_ref pg, size_t begin_sel, size_t end_sel,
		short modifiers, pg_boolean show_hilite);
extern PG_PASCAL (memory_ref) pgGetSelectionList (pg_ref pg, pg_boolean for_paragraph);
extern PG_PASCAL (void) pgSetSelectionList (pg_ref pg, memory_ref select_list,
		long extra_offset, pg_boolean show_hilite);
extern PG_PASCAL (void) pgExtendSelection (pg_ref pg, long amount_ext, short modifiers,
			pg_boolean show_hilite);
extern PG_PASCAL (pg_boolean) pgGetHiliteRgn (pg_ref pg, const select_pair_ptr range,
		memory_ref select_list, shape_ref rgn);
extern PG_PASCAL (pg_short_t) pgNumSelections (pg_ref pg);
extern PG_PASCAL (pg_boolean) pgCaretPosition (pg_ref pg, long offset, rectangle_ptr caret_rect);
extern PG_PASCAL (void) pgSetCaretPosition (pg_ref pg, pg_short_t position_verb, pg_boolean show_caret);
extern PG_PASCAL (void) pgSetCursorState (pg_ref pg, short cursor_state);
extern PG_PASCAL (short) pgGetCursorState (pg_ref pg);
extern PG_PASCAL (long) pgGetChangeCtr (pg_ref pg);
extern PG_PASCAL (void) pgSetChangeCtr (pg_ref pg, long ctr);
extern PG_PASCAL (short) pgTextRect (pg_ref pg, const select_pair_ptr range, pg_boolean want_scroll,
		pg_boolean want_scaled, rectangle_ptr rect);
extern PG_PASCAL (short) pgCharacterRect (pg_ref pg, long position, pg_boolean want_scrolled,
		pg_boolean want_scaled, rectangle_ptr rect);
extern PG_PASCAL (size_t) pgPtToChar (pg_ref pg, const co_ordinate_ptr point, const co_ordinate_ptr offset_extra);
extern PG_PASCAL (short) pgPtInView (pg_ref pg, const co_ordinate_ptr point,
		const co_ordinate_ptr offset_extra);
extern PG_PASCAL (long) pgTotalTextHeight (pg_ref pg, short paginate);
extern PG_PASCAL (void) pgMaxTextBounds (pg_ref pg, rectangle_ptr bounds, pg_boolean paginate);
extern PG_PASCAL (memory_ref) pgMaskedSelection (pg_ref pg, const select_pair_ptr range,
		long compare_styles, long mask_styles, pg_boolean exclude);
extern PG_PASCAL (void) pgPaginateNow (pg_ref pg, long paginate_to, pg_boolean use_best_guess);
extern PG_PASCAL (void) pgSelectToShape (pg_ref pg, memory_ref select_shape,
		pg_boolean show_hilite);
extern PG_PASCAL (void) pgSetScaling (pg_ref pg, const pg_scale_ptr scale_factor, short draw_mode);
extern PG_PASCAL (void) pgGetScaling (pg_ref pg, pg_scale_ptr scale_factor);
extern PG_PASCAL (pg_boolean) pgSetPrintDevice (pg_ref pg, generic_var device);
extern PG_PASCAL (generic_var) pgGetPrintDevice (pg_ref pg);
extern PG_PASCAL (long) pgPrintToPage (pg_ref pg, const graf_device_ptr target, long starting_position,
		const rectangle_ptr page_rect, short draw_mode);
extern PG_PASCAL (short) pgNumPages (pg_ref pg, const rectangle_ptr page_rect);
extern PG_PASCAL (long) pgFindPage (pg_ref pg, short page_num, const rectangle_ptr page_rect);
extern PG_PASCAL (void) pgInitStandardHandlers (pg_globals_ptr globals);
extern PG_PASCAL (void) pgSetHandler (pg_globals_ptr globals, pg_file_key key,
		pg_handler_proc read_handler, pg_handler_proc write_handler,
		file_io_proc read_data_proc, file_io_proc write_data_proc);
extern PG_PASCAL (pg_error) pgGetHandler (pg_globals_ptr globals, pg_handler_ptr handler);
extern PG_PASCAL (pg_error) pgRemoveHandler (pg_globals_ptr globals,
		pg_file_key key);
extern PG_PASCAL (pg_error) pgSaveDoc (pg_ref pg, size_t PG_FAR *file_position, const pg_file_key_ptr keys,
		pg_short_t num_keys, file_io_proc write_proc, file_ref filemap,
		long doc_element_info);
extern PG_PASCAL (pg_error) pgVerifyFile (file_ref filemap, file_io_proc io_proc, size_t position);
extern PG_PASCAL (pg_error) pgReadDoc (pg_ref pg, size_t PG_FAR *file_position, const pg_file_key_ptr keys,
		pg_short_t num_keys, file_io_proc read_proc, file_ref filemap);
extern PG_PASCAL (pg_error) pgCacheReadDoc (pg_ref pg, size_t PG_FAR *file_position, const pg_file_key_ptr keys,
		pg_short_t num_keys, file_io_proc read_proc, file_ref filemap);
extern PG_PASCAL (file_ref) pgGetCacheFileRef (pg_ref pg);
extern PG_PASCAL (pg_error) pgCacheSaveDoc (pg_ref pg, size_t PG_FAR *file_position, const pg_file_key_ptr keys,
		pg_short_t num_keys, file_io_proc write_proc, file_ref filemap,
		long doc_element_info);
extern PG_PASCAL (pg_error) pgTerminateFile (pg_ref pg, size_t PG_FAR *file_position,
		file_io_proc write_proc, file_ref filemap);
extern PG_PASCAL (void) pgOffsetShape (shape_ref the_shape, long h, long v);
extern PG_PASCAL (void) pgInsetShape (shape_ref the_shape, long h, long v);
extern PG_PASCAL (pg_short_t) pgPtInShape (shape_ref the_shape, const co_ordinate_ptr point,
		const co_ordinate_ptr offset_extra, const co_ordinate_ptr inset_extra,
		const pg_scale_ptr scaling);
extern PG_PASCAL (pg_boolean) pgRectInShape (shape_ref the_shape, const rectangle_ptr rect,
		const co_ordinate_ptr offset_extra, rectangle_ptr sect_rect);
extern PG_PASCAL (void) pgShapeToSelections (pg_ref pg, shape_ref the_shape, memory_ref selections);
extern PG_PASCAL (pg_boolean) pgEmptyShape (shape_ref the_shape);
extern PG_PASCAL (pg_boolean) pgEqualShapes (shape_ref shape1, shape_ref shape2);
extern PG_PASCAL (pg_short_t) pgSectRectInShape (shape_ref the_shape, rectangle_ptr rect,
		rectangle_ptr sect_rect);
extern PG_PASCAL (pg_short_t) pgSectOrTouchRectInShape (shape_ref the_shape,
		const rectangle_ptr rect, const rectangle_ptr sect_rect);
extern PG_PASCAL (void) pgShapeBounds (shape_ref the_shape, rectangle_ptr bounds);
extern PG_PASCAL (pg_short_t) pgNumRectsInShape (shape_ref the_shape);
extern PG_PASCAL (pg_boolean) pgSectShape (shape_ref shape1, shape_ref shape2,
		shape_ref result_shape);
extern PG_PASCAL (void) pgDiffShape (shape_ref shape1, shape_ref shape2,
		shape_ref result_shape);
extern PG_PASCAL (shape_ref) pgRectToShape (pgm_globals_ptr mem_globals, const rectangle_ptr rect);
extern PG_PASCAL (void) pgAddRectToShape (shape_ref the_shape, const rectangle_ptr rect);
extern PG_PASCAL (void) pgMergeRectToShape (shape_ref the_shape, const rectangle_ptr rect);
extern PG_PASCAL (void) pgSetShapeRect (shape_ref the_shape, const rectangle_ptr rect);
extern PG_PASCAL (void) pgEraseShape (pg_ref pg, shape_ref the_shape, const pg_scale_ptr scale_factor,
		const co_ordinate_ptr offset_extra, const rectangle_ptr vis_bounds);
extern PG_PASCAL (void) pgDisposeShape (shape_ref the_shape);
extern PG_PASCAL (void) pgInvalSelect (pg_ref pg, size_t select_from, size_t select_to);
extern PG_PASCAL (void) pgInvalShapes (pg_ref pg, pg_boolean inval_page, pg_boolean inval_exclusion);
extern PG_PASCAL (void) pgGetDocInfo (pg_ref pg, pg_doc_ptr doc_info);
extern PG_PASCAL (void) pgSetDocInfo (pg_ref pg, const pg_doc_ptr doc_info,
		pg_boolean inval_text, short draw_mode);
extern PG_PASCAL (void) pgSetPageColor (pg_ref pg, const color_value_ptr color);
extern PG_PASCAL (void) pgGetPageColor (pg_ref pg, color_value_ptr color);
extern PG_PASCAL (void) pgErasePageArea (pg_ref pg, shape_ref vis_area);
extern PG_PASCAL (long) pgGetEndingPage (pg_ref pg, rectangle_ptr page_rect);

/* Scrolling & alternate scrolling method support: */

extern PG_PASCAL (void) pgSetScrollParams (pg_ref pg, short unit_h, short unit_v,
		short append_h, short append_v);
extern PG_PASCAL (void) pgGetScrollParams (pg_ref pg, short PG_FAR *unit_h, short PG_FAR *unit_v,
		short PG_FAR *append_h, short PG_FAR *append_v);
extern PG_PASCAL (pg_boolean) pgGetScrollValues (pg_ref pg, short PG_FAR *h, short PG_FAR *v,
		short PG_FAR *max_h, short PG_FAR *max_v);
extern PG_PASCAL (void) pgSetScrollValues (pg_ref pg, short h, short v, short align_line,
		short draw_mode);
extern PG_PASCAL (void) pgScroll (pg_ref pg, short h_verb, short v_verb, short draw_mode);
extern PG_PASCAL (pg_boolean) pgAdjustScrollMax (pg_ref pg, short draw_mode);
extern PG_PASCAL (void) pgScrollUnitsToPixels (pg_ref pg, short h_verb, short v_verb,
		pg_boolean add_to_position, pg_boolean window_origin_changes,
		long PG_FAR *h_pixels, long PG_FAR *v_pixels);
extern PG_PASCAL (void) pgLastScrollAmount (pg_ref pg, long *h_pixels, long *v_pixels);
extern PG_PASCAL (void) pgDrawScrolledArea (pg_ref pg, long pixels_h, long pixels_v,
		const co_ordinate_ptr original_origin, const co_ordinate_ptr new_origin, short draw_mode);
extern PG_PASCAL (pg_boolean) pgScrollToView (pg_ref pg, long text_offset, short h_extra,
		short v_extra, pg_boolean align_line, short draw_mode);
extern PG_PASCAL (pg_region) pgScrollViewRect (pg_ref pg, long h_pixels, long v_pixels);
extern PG_PASCAL (void) pgScrollPosition (pg_ref pg, co_ordinate_ptr scroll_pos);
extern PG_PASCAL (void) pgScrollPixels (pg_ref pg, long h, long v, short draw_mode);
extern PG_PASCAL (void) pgSetScrollAlign (pg_ref pg, short align_h, short align_v);
extern PG_PASCAL (void) pgGetScrollAlign (pg_ref pg, short PG_FAR *align_h,
		short PG_FAR *align_v);


/* "Containers" Support: */

extern PG_PASCAL (pg_short_t) pgNumContainers (pg_ref pg);
extern PG_PASCAL (void) pgGetContainer (pg_ref pg, pg_short_t position,
		pg_boolean include_scroll, pg_boolean include_scale,
		rectangle_ptr container);
extern PG_PASCAL (long) pgGetContainerRefCon (pg_ref pg, pg_short_t position);
extern PG_PASCAL (void) pgSetContainerRefCon (pg_ref pg, pg_short_t position, long ref_con);
extern PG_PASCAL (pg_short_t) pgPtInContainer (pg_ref pg, const co_ordinate_ptr point,
		const co_ordinate_ptr inset_extra);
extern PG_PASCAL (void) pgInsertContainer (pg_ref pg, const rectangle_ptr container,
		pg_short_t position, long ref_con, short draw_mode);
extern PG_PASCAL (void) pgRemoveContainer (pg_ref pg, pg_short_t position,
		short draw_mode);
extern PG_PASCAL (void) pgReplaceContainer (pg_ref pg, const rectangle_ptr container,
		pg_short_t position, short draw_mode);
extern PG_PASCAL (void) pgSwapContainers (pg_ref pg, pg_short_t container1,
		pg_short_t container2, short draw_mode);
extern PG_PASCAL (pg_short_t) pgCharToContainer (pg_ref pg, long offset);
extern PG_PASCAL (size_t) pgContainerToChar (pg_ref pg, pg_short_t position);

/* "Exclusion Area" Support (for graphics, layout, etc. ) */

extern PG_PASCAL (pg_short_t) pgNumExclusions (pg_ref pg);
extern PG_PASCAL (void) pgGetExclusion (pg_ref pg, pg_short_t position,
		pg_boolean include_scroll, pg_boolean include_scale,
		rectangle_ptr exclusion);
extern PG_PASCAL (long) pgGetExclusionRefCon (pg_ref pg, pg_short_t position);
extern PG_PASCAL (void) pgSetExclusionRefCon (pg_ref pg, pg_short_t position, long ref_con);
extern PG_PASCAL (pg_short_t) pgPtInExclusion (pg_ref pg, const co_ordinate_ptr point,
		const co_ordinate_ptr inset_extra);
extern PG_PASCAL (void) pgInsertExclusion (pg_ref pg, const rectangle_ptr exclusion,
		pg_short_t position, long ref_con, short draw_mode);
extern PG_PASCAL (void) pgRemoveExclusion (pg_ref pg, pg_short_t position,
		short draw_mode);
extern PG_PASCAL (void) pgReplaceExclusion (pg_ref pg, const rectangle_ptr exclusion,
		pg_short_t position, short draw_mode);
extern PG_PASCAL (void) pgSwapExclusions (pg_ref pg, pg_short_t exclusion1,
		pg_short_t exclusion2, short draw_mode);
extern PG_PASCAL (void) pgInsertExclusionShape (pg_ref pg, pg_short_t position,
		shape_ref exclude_shape, short draw_mode);
extern PG_PASCAL (long) pgAttachParExclusion (pg_ref pg, long position, pg_short_t index, short draw_mode);
extern PG_PASCAL (long) pgGetAttachedPar (pg_ref pg, pg_short_t exclusion);


/* Misc. Utilities */

extern PG_PASCAL (void) pgScaleLong (long scale_factor, long origin, long PG_FAR *value);
extern PG_PASCAL (void) pgScalePt (const pg_scale_ptr scale_gactor, const co_ordinate_ptr amount_offset, co_ordinate_ptr pt);
extern PG_PASCAL (void) pgScaleRect (const pg_scale_ptr scale_factor, const co_ordinate_ptr amount_offset, rectangle_ptr rect);
extern PG_PASCAL (void) pgScaleRectToRect (const pg_scale_ptr scale_factor, const rectangle_ptr src_rect,
			rectangle_ptr target_rect, const co_ordinate_ptr offset_extra);

extern PG_PASCAL (long) pgGetPaigeRefID(pg_ref pg);

/* Note, "pgGetDefault..." for functions below are defined as macros */

extern PG_PASCAL (void) pgSetDefaultStyle (pg_ref pg, const style_info_ptr def_style, short draw_mode);
extern PG_PASCAL (void) pgSetDefaultPar (pg_ref pg, const par_info_ptr def_par, short draw_mode);
extern PG_PASCAL (void) pgSetDefaultFont (pg_ref pg, const font_info_ptr def_font, short draw_mode);
extern PG_PASCAL (pg_short_t) pgGetDefaultStyle (pg_ref pg, style_info_ptr def_style);

/* Line and paragraph count utilities */

extern PG_PASCAL (long) pgNumLines (pg_ref pg);
extern PG_PASCAL (long) pgOffsetToLineNum (pg_ref pg, long offset,
		pg_boolean line_end_has_precedence);
extern PG_PASCAL (void) pgLineNumToOffset (pg_ref pg, size_t line_num,
		size_t PG_FAR *begin_offset, size_t PG_FAR *end_offset);
extern PG_PASCAL (long) pgNumPars (pg_ref pg);
extern PG_PASCAL (long) pgOffsetToParNum (pg_ref pg, long offset);
extern PG_PASCAL (void) pgParNumToOffset  (pg_ref pg, size_t par_num,
		size_t PG_FAR *begin_offset, size_t PG_FAR *end_offset);
extern PG_PASCAL (void) pgLineNumToBounds (pg_ref pg, long line_num,
		pg_boolean want_scrolled, pg_boolean want_scaled,
		rectangle_ptr bounds);
extern PG_PASCAL (void) pgParNumToBounds (pg_ref pg, long par_num,
		pg_boolean want_scrolled, pg_boolean want_scaled, rectangle_ptr bounds);
extern PG_PASCAL (void) pgDisplayLineRange (pg_ref pg, long line_from, long line_to,
		const graf_device_ptr target_device, shape_ref vis_target,
		shape_ref wrap_target, const co_ordinate_ptr offset_extra, short draw_mode);


/* Version 2.x Unicode Support */

extern PG_PASCAL (pg_boolean) pgIsPaigeUnicode (void);
extern PG_PASCAL (size_t) pgBytesToUnicode (pg_bits8_ptr input_bytes, pg_short_t PG_FAR *output_chars,
		font_info_ptr font, size_t input_byte_size);
extern PG_PASCAL (size_t) pgUnicodeToBytes (pg_short_t PG_FAR *input_chars, pg_bits8_ptr output_bytes,
		font_info_ptr font, size_t input_char_size);
extern PG_PASCAL (size_t) pgUnicodeToUnicode (pg_short_t PG_FAR *the_chars, size_t num_chars, pg_boolean force_reverse);

#ifdef UNICODE
extern PG_PASCAL (pg_boolean) pgInsertBytes (pg_ref pg, const pg_bits8_ptr data, long length, long position,
		short insert_mode, short modifiers, short draw_mode);
#endif

/* Future version (possible) support */

extern PG_PASCAL (void) pgSetImportBase (pg_ref pg, memory_ref base);
extern PG_PASCAL (memory_ref) pgGetImportBase (pg_ref pg);
extern PG_PASCAL (void) pgSetExportBase (pg_ref pg, memory_ref base);
extern PG_PASCAL (memory_ref) pgGetExportBase (pg_ref pg);
extern PG_PASCAL (void) pgSetBackgroundImage (pg_ref pg, pg_url_image_ptr image, short draw_mode);
extern PG_PASCAL (void) pgGetBackgroundImage (pg_ref pg, pg_url_image_ptr image);

#ifdef __cplusplus
	}
#endif

#endif


