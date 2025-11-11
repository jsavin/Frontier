
/* HTML DEFINITIONS for the HTML filter */

#ifndef PGHTMDEF_H
#define PGHTMDEF_H
#endif

#include "Paige.h"
#include "pgTxr.h"

#define HTML_VERSION			3

/* Command table types: */

#define DO_NOTHING_COMMAND		-1

#define CONTROL_COMMAND			1
#define STYLE_COMMAND			2
#define DATA_COMMAND			3
#define TABLE_DATA_COMMAND		4

#define DEF_POINT_LEVEL			2
#define DEFLIST_INDENT_VALUE	36
#define LINE_BREAK_EXTRA		12

#ifdef WINDOWS_PLATFORM

#define PRETAG_DEF_FONT (pg_char_ptr)"Courier New\0"
#define PRETAG_DEF_POINTSIZE 10
#define CODETAG_DEF_FONT (pg_char_ptr)"Courier New\0"
#define CODETAG_DEF_POINTSIZE 9

#endif

#ifdef MAC_PLATFORM

#define PRETAG_DEF_FONT (pg_char_ptr)"Monaco\0"
#define PRETAG_DEF_POINTSIZE 10
#define CODETAG_DEF_FONT (pg_char_ptr)"Monaco\0"
#define CODETAG_DEF_POINTSIZE 9

#endif

#ifdef __cplusplus
extern "C" {
#endif

extern pg_char PG_FAR control_commands[];

enum {
	base_url_command = 1,
	basefont_command,
	body_command,
	linebreak_command,
	directory_command,
	div_command,	//Jeff
	deflist_command,
	heading_command,
	linerule_command,
	html_command,
	listing_command,
	code_listing_command,
	menu_command,
	ordered_list_command,
	parbreak_command,
	style_command,
	title_command,
	unordered_list_command,
	wordbreak_command,
	x_tab_command,          // Kusuma
	x_signature_command     // boo
};

extern pg_char PG_FAR control_attributes[];

enum {
	align_attribute = 1,
	current_link_color_attribute,
	bk_image_attribute,
	bk_color_attribute,
	class_attribute,
	def_text_color_attribute,
	URL_attribute,
	leftmargin_attribute,
	link_color_attribute,
	noshade_attribute,
	size_attribute,
	start_number_attribute,
	text_color_attribute,
	topmargin_attribute,
	number_type_attribute,
	seen_link_color_attribute,
	width__attribute
};

extern pg_char PG_FAR html_style_commands[];

enum {
	address_style_command = 1,
	bold_command,
	larger_size_command,
	blink_command,
	blockquote_style_command,
	center_command,
	cite_style_command,
	code_style_command,
	deflist_indented_command,
	code_def_style_command,
	deflist_item_command,
	emphasis_style_command,
	font_command,
	heading1_command,
	heading2_command,
	heading3_command,
	heading4_command,
	heading5_command,
	heading6_command,
	italic_command,
	nobreak_command,
	asis_command,
	strikeout_command,
	smaller_size_command,
	strikeout2_command,
	strong_command,
	subscript_command,
	superscript_command,
	teletype_style_command,
	underline_command,
	code_var_style_command
};

extern pg_char PG_FAR style_attributes[];

enum {
	style_align_attribute = 1,
	fontcolor_attribute,
	fontface_attribute,
	fontsize_attribute
};

extern pg_char PG_FAR table_data_commands[];

enum {
	table_begin_command = 1,
	table_dcell_command,
	table_hcell_command,
	table_row_command
};

extern pg_char PG_FAR data_commands[];

enum {
	hyperlink_command = 1,
	embed_command,    // BOG: netscape "embed" tag
	image_command,
	object_command,   // BOG: html 4.x "object" tag
	param_command     // BOG: html 4.x "param" tag
};

extern pg_char PG_FAR data_attributes[];

enum {
	data_align_attribute = 1,
	alt_image_attribute,
	bk_table_attribute,
	border_attribute,
	cellpad_attribute,
	cellspace_attribute,
	object_classid_attribute,
	column_span_attribute,
	eudora_attribute,   // BOG: support for the "eudora" attribute 
	image_height_attribute,
	image_h_extra_attribute,
	link_url_attribute,
	object_id_attribute,
	ismap_attribute,
	alt_image_url_attribute,
	link_name_attribute,
	image_url_attribute,
	object_title_attribute,
	vertical_align_attribute,
	param_value_attribute,
	param_valuetype_attribute,
	image_v_extra_attribute,
	image_width_attribute
};

extern pg_char PG_FAR data_values[];

enum {
	absbottom_value = 1,
	absmiddle_value,
	baseline_value,
	center_value,
	left_value,
	middle_value,
	right_value,
	top_value
};

extern pg_char PG_FAR errorlog_tags[];

enum {
	uns_applet = 1,
	uns_form,
	uns_frame
};

extern pg_char PG_FAR data_color_values[];
extern long PG_FAR data_colors[140];

extern pg_char PG_FAR data_char_values[];
extern pg_char PG_FAR data_char_values2[];
extern pg_char PG_FAR data_chars[];
extern pg_char PG_FAR data_chars2[];

#define DATA2_QUOTE_ELEMENT		4

extern pg_char PG_FAR body_style[];
extern pg_char PG_FAR pre_tag_style[];
extern pg_char PG_FAR teletype_style[];
extern pg_char PG_FAR blockquote_style[];
extern pg_char PG_FAR address_style[];
extern pg_char PG_FAR cite_style[];
extern pg_char PG_FAR heading1_style[];
extern pg_char PG_FAR heading2_style[];
extern pg_char PG_FAR heading3_style[];
extern pg_char PG_FAR heading4_style[];
extern pg_char PG_FAR heading5_style[];
extern pg_char PG_FAR heading6_style[];
extern pg_char PG_FAR code_style[];
extern pg_char PG_FAR code_def_style[];
extern pg_char PG_FAR code_var_style[];


extern PG_PASCAL (void) CreateHTMLStyles (pg_ref pg, pg_char_ptr def_font, long PG_FAR *style_array);

#ifdef __cplusplus
	}
#endif

