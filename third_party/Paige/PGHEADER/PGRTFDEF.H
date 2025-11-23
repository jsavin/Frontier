
/* RTF DEFINITIONS for embedref RTF filter */

#ifndef PGRTFDEF_H
#define PGRTFDEF_H
#endif

#include "Paige.h"
#include "pgTxr.h"

#define RTF_VERSION				1
#define MAX_NO_CR				128	/* Maximum chars before CR is forced */
#define METAFILE_PARAM			8	/* Required wmetafile parameter */

/* Command table types: */

#define DO_NOTHING_COMMAND		-1

#define SPECIAL_CHAR_COMMAND	1
#define GROUP_AVOID_COMMAND		2
#define DESTINATION_COMMAND		3
#define DOCUMENT_COMMAND		4
#define PARAGRAPH_COMMAND		5
#define STYLE_COMMAND			6
#define TABLE_COMMAND			7
#define INFO_COMMAND			8

/* Nested "stack" for text formats: */

struct format_stack {
	style_info			style;
	font_info			font;
};

typedef struct format_stack format_stack;
typedef format_stack PG_FAR *format_stack_ptr;

#ifdef __cplusplus
extern "C" {
#endif

extern pg_char PG_FAR special_char_commands[];

enum {
	bullet_command = 1,
	left_doublequote_command,
	line_feed_command,
	left_singlequote_command,
	page_break_command,
	cr_command,
	right_doublequote_command,
	right_singlequote_command,
	end_section_command,
	tab_char_command
};

extern pg_char PG_FAR group_avoid_commands[];

extern pg_char PG_FAR destination_commands[];

enum {
	non_mac_os_command = 1,
	binary_data_command,
	color_table_command,
	def_font_command,
	font_table_command,
	mac_platform_command,
	mac_pict_command,
	object_data_command,
	object_command,
	object_height_command,
	object_width_command,
	pict_command,
	pict_height_command,
	picframe_height_command,
	pict_vscale_command,
	pict_hscale_command,
	pict_width_command,
	picframe_width_command,
	object_result_command,
	rtf_signature_command,
	style_basedon_command,
	stylesheet_command,
	table_defaults,
	windows_metafile_command
};


extern pg_char PG_FAR document_commands[];

enum {
	bottom_margin_command = 1,
	left_margin_command,
	right_margin_command,
	top_margin_command,
	paper_height_command,
	paper_width_command
};


extern pg_char PG_FAR paragraph_commands[];

enum {
	box_border = 1,
	bottom_border,
	border_colorfill,
	double_border,
	dot_border,
	left_border,
	right_border,
	single_border,
	shadow_border,
	top_border,
	thick_border,
	border_width,
	border_spacing,
	first_indent_command,
	keep_together_command,
	left_indent_command,
	page_break_before_command,
	par_default_command,
	center_justify_command,
	full_justify_command,
	left_justify_command,
	right_justify_command,
	right_indent_command,
	space_after_command,
	space_before_command,
	shading_command,
	line_spacing_command,
	dot_leader_command,
	dash_leader_command,
	center_tab_command,
	decimal_tab_command,
	right_tab_command,
	set_tab_command
};


extern pg_char PG_FAR style_commands[];

enum {
	bold_command = 1,
	bookmark_command,
	bluecolor_command,
	all_caps_command,
	charset_command,
	fgcolor_command,
	subscript_command,
	expand_condense_command,
	font_command,
	font_charset_command,
	pointsize_command,
	greencolor_command,
	italic_command,
	outline_command,
	plain_command,
	redcolor_command,
	style_command,
	small_caps_command,
	shadow_command,
	strikeout_command,
	sub_command,
	super_command,
	toc_command,
	toc_level_command,
	underline_command,
	dotted_underline_command,
	double_underline_command,
	word_underline_command,
	superscript_command,
	invis_text_command,
	index_style_command
};


extern pg_char PG_FAR table_commands[];

enum {
	table_cell = 1,		/* Data that follows is next cell */
	cell_setright,
	cell_border_bottom,
	cell_border_left,
	cell_border_right,
	cell_border_top,
	cell_first_merge,	/* First table in range of cells to be merged */
	cell_merge,			/* Contents of cell are merged with preceding cell */
	cell_shading,		/* Cell is shaded */
	enter_table,
	table_row_end,
	table_border_bottom,
	table_border_horizontal,
	table_border_left,
	table_border_right,
	table_border_vertical,
	table_border_top,
	table_spacing,		/* Half the space between cells in twips */
	table_header,		/* Data that follows is table header */
	table_keep_together,
	table_position_left,	/* Position table to left */
	table_center,
	table_left,
	table_right,
	table_height
};


extern pg_char PG_FAR info_commands[];
extern pg_char PG_FAR def_stylesheet_name[];

#ifdef __cplusplus
	}
#endif

