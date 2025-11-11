/* HTML definitions table,  used by HTML Import & Export. This file contains all the
tags and tag attribute keywords that we support (in some fashion) for HTML. */

#include "CPUDEFS.H"
#include "pghtmdef.h"
#include "machine.h"


pg_char PG_FAR control_commands[] = {
	"base basefont body br dir dl head hr html li listing menu ol p title ul wbr \0"
};
pg_char PG_FAR control_attributes[] = {
	"align alink background bgcolor color href leftmargin link noshade size start text topmargin type vlink width \0"
};


pg_char PG_FAR html_style_commands[] = {
	"address b big blink blockquote center cite code dd dfn dt em font h1 h2 h3 h4 h5 h6 i nobr pre s small strike strong sub sup tt u var \0"
};
pg_char PG_FAR style_attributes[] = {
	"align color face size \0"
};

pg_char PG_FAR table_data_commands[] = {
	"table td th tr \0"
};

pg_char PG_FAR data_commands[] = {
	"a img \0"
};
pg_char PG_FAR data_attributes[] = {
	"align alt bgcolor border cellpadding cellspacing colspan height hspace href ismap lowsrc name src valign vspace width \0"
};
pg_char PG_FAR data_values[] = {
	"absbottom absmiddle baseline center left middle right top \0"
};

pg_char PG_FAR data_color_values[] = {
	"aliceblue antiquewhite aqua aquamarine azure beige bisque black blanchedalmond blue blueviolet brown burlywood cadetblue chartreuse chocolate coral cornflowerblue cornsilk crimson cyan darkblue darkcyan darkgoldenrod darkgray darkgreen darkkhaki darkmagenta darkolivegreen darkorange darkorchid darkred darksalmon darkseagreen darkslateblue darkslategray darkturquoise darkviolet deeppink deepskyblue dimgray dodgerblue firebrick floralwhite forestgreen fuchsia gainsboro ghostwhite gold goldenrod gray green greenyellow honeydew hotpink indianred indigo ivory khaki lavender lavenderblush lawngreen lemonchiffon lightblue lightcoral lightcyan lightgoldenrodyellow lightgreen lightgrey lightpink lightsalmon lightseagreen lightskyblue lightslategray lightsteelblue lightyellow lime limegreen linen magenta maroon mediumaquamarine mediumblue mediumorchid mediumpurple mediumseagreen mediumslateblue mediumspringgreen mediumturquoise mediumvioletred midnightblue mintcream mistyrose moccasin navajowhite navy oldlace olive olivedrab orange orangered orchid palegoldenrod palegreen paleturquoise palevioletred papayawhip peachpuff peru pink plum powderblue purple red rosybrown royalblue saddlebrown salmon sandybrown seagreen seashell sienna silver skyblue slateblue slategray snow springgreen steelblue tan teal thistle tomato turquoise violet wheat white whitesmoke yellow yellowgreen \0"
};

pg_char PG_FAR data_char_values[] = {
	"AElig Aacute Acirc Agrave Aring Atilde Auml Ccedil ETH Eacute Ecirc Egrave Euml Ntilde Oacute Ocirc Ograve Oslash Otilde Ouml THORN Uacute Ucirc Ugrave Uuml Yacute aacute acute acirc aelig agrave aring atilde auml ccedil cedil cent copy curren deg divide eacute ecirc egrave emdash eth euml frac12 frac14 frac34 iacute icirc iexcl igrave iquest iuml laquo Iacute Icirc Igrave Iuml macr micro middot nbsp not ntilde oacute ocirc ograve ordf ordm oslash otilde ouml para plusmn pound raquo reg sect shy szlig thorn trade uacute ucirc ugrave uml uuml yacute yen yuml \0"
};

pg_char PG_FAR data_char_values2[] = {
	"amp gt lt quot \0"
};

pg_char PG_FAR data_chars[] = {
	"®çåËÌ€‚*ƒæéè„îïñ¯Í…*òóô†*‡«‰¾ˆŒ‹Š*¢©Û¡ÖŽÑ*‘***’”Á“À•Çêëíìøµ*ÊÂ–—™˜»¼¿›š¦±£È¨¤Ð§*ªœž¬Ÿ*´Ø\0"
};

pg_char PG_FAR data_chars2[] = {
	"&>< \0"
};

long PG_FAR data_colors[140] = {

	0xF0F8FF, 0xFAEBD7, 0x00FFFF, 0x7FFFD4, 0xF0FFFF, 0xF5F5DC, 0xFFE4C4, 0x000000,
	0xFFEBCD, 0x0000FF, 0x8A2BE2, 0xA52A2A, 0xDEB887, 0x5F9EA0, 0x7FFF00, 0xD2691E,
	0xFF7F50, 0x6495ED, 0xFFF8DC, 0xDC143C, 0x00FFFF, 0x00008B, 0x008B8B, 0xB8860B,
	0xA9A9A9, 0x006400, 0xBDB76B, 0x8B008B, 0x556B2F, 0xFF8C00, 0x9932CC, 0x8B0000,
	0xE9967A, 0x8FBC8F, 0x483D8B, 0x2F4F4F, 0x00CED1, 0x9400D3, 0xFF1493, 0x00BFFF,
	0x696969, 0x1E90FF, 0xB22222, 0xFFFAF0, 0x228B22, 0xFF00FF, 0xDCDCDC, 0xF8F8FF,
	0xFFD700, 0xDAA520, 0x808080, 0x008000, 0xADFF2F, 0xF0FFF0, 0xFF69B4, 0xCD5C5C,
	0x4B0082, 0xFFFFF0, 0xF0E68C, 0xE6E6FA, 0xFFF0F5, 0x7CFC00, 0xFFFACD, 0xADD8E6,
	0xF08080, 0xE0FFFF, 0xFAFAD2, 0x90EE90, 0xD3D3D3, 0xFFB6C1, 0xFFA07A, 0x20B2AA,
	0x87CEFA, 0x778899, 0xB0C4DE, 0xFFFFE0, 0x00FF00, 0x32CD32, 0xFAF0E6, 0xFF00FF,
	0x800000, 0x66CDAA, 0x0000CD, 0xBA55D3, 0x9370DB, 0x3CB371, 0x7B68EE, 0x00FA9A,
	0x48D1CC, 0xC71585, 0x191970, 0xF5FFFA, 0xFFE4E1, 0xFFE4B5, 0xFFDEAD, 0x000080,
	0xFDF5E6, 0x808000, 0x6B8E23, 0xFFA500, 0xFF4500, 0xDA70D6, 0xEEE8AA,
	0x98FB98, 0xAFEEEE, 0xDB7093, 0xFFEFD5, 0xFFDAB9, 0xCD853F, 0xFFC0CB, 0xDDA0DD,
	0xB0E0E6, 0x800080, 0xFF0000, 0xBC8F8F, 0x4169E1, 0x8B4513, 0xFA8072, 0xF4A460,
	0x2E8B57, 0xFFF5EE, 0xA0522D, 0xC0C0C0, 0x87CEEB, 0x6A5ACD, 0x708090, 0xFFFAFA,
	0x00FF7F, 0x4682B4, 0xD2B48C, 0x008080, 0xD8BFD8, 0xFF6347, 0x40E0D0, 0xEE82EE,
	0xF5DEB3, 0xFFFFFF, 0xF5F5F5, 0xFFFF00, 0x9ACD32
};

pg_char PG_FAR errorlog_tags[] = { // tags that are not supported but can affect content
	"applet form frame \0"
};


// Stylesheet names:

pg_char PG_FAR body_style[] = {"HTML Body\0"};
pg_char PG_FAR pre_tag_style[] = {"HTML Pre-tag\0"};
pg_char PG_FAR teletype_style[] = {"HTML Teletype\0"};
pg_char PG_FAR blockquote_style[] = {"HTML Blockquote\0"};
pg_char PG_FAR address_style[] = {"HTML Address\0"};
pg_char PG_FAR cite_style[] = {"HTML Cite\0"};

pg_char PG_FAR heading1_style[] = {"HTML Heading 1\0"};
pg_char PG_FAR heading2_style[] = {"HTML Heading 2\0"};
pg_char PG_FAR heading3_style[] = {"HTML Heading 3\0"};
pg_char PG_FAR heading4_style[] = {"HTML Heading 4\0"};
pg_char PG_FAR heading5_style[] = {"HTML Heading 5\0"};
pg_char PG_FAR heading6_style[] = {"HTML Heading 6\0"};

pg_char PG_FAR code_style[] = {"HTML Code\0"};
pg_char PG_FAR code_def_style[] = {"HTML Code Def\0"};
pg_char PG_FAR code_var_style[] = {"HTML Var\0"};


static long make_style (pg_ref import_pg, pg_char_ptr name, long html_element, long stylebits,
				short pointsize, pg_char_ptr fontname, par_info_ptr par);


/* CreateHTMLStyles creates default named stylesheets for the various HTML tags.
If style_array is non-NULL then it contains an array of NUM_HTML_STYLES. Upon entry,
each value of zero in that array is created, otherwise the stylesheet is NOT created
(the array element is assumed to be an existing named style index). On return
all the elements are filled in. */

PG_PASCAL (void) CreateHTMLStyles (pg_ref pg, pg_char_ptr def_font, long PG_FAR *style_array)
{
	par_info		par_heading;
	long			html_styles[NUM_HTML_STYLES];
	
	pgGetParInfoRec(pg, 0, &par_heading);
	par_heading.indents.left_indent = par_heading.indents.first_indent = 0;

	if (style_array)
		pgBlockMove(style_array, html_styles, sizeof(long) * NUM_HTML_STYLES);
	else
		pgFillBlock(html_styles, sizeof(long) * NUM_HTML_STYLES, 0);

	if (!html_styles[body_stylesheet])
		html_styles[body_stylesheet] = make_style(pg, body_style, body_stylesheet, X_PLAIN_TEXT, 12, def_font, &par_heading);
	
#ifdef WINDOWS_PLATFORM
	if (!html_styles[pre_tag_stylesheet])
		html_styles[pre_tag_stylesheet] = make_style(pg, pre_tag_style, pre_tag_stylesheet, X_PLAIN_TEXT, PRETAG_DEF_POINTSIZE, PRETAG_DEF_FONT, &par_heading);
	if (!html_styles[teletype_stylesheet])
		html_styles[teletype_stylesheet] = make_style(pg, teletype_style, teletype_stylesheet, X_PLAIN_TEXT, 12, (pg_char_ptr)"Courier New\0", &par_heading);
	if (!html_styles[code_stylesheet])
		html_styles[code_stylesheet] = make_style(pg, code_style, code_stylesheet, X_PLAIN_TEXT, CODETAG_DEF_POINTSIZE, CODETAG_DEF_FONT, &par_heading);
	
	if (!html_styles[code_def_stylesheet])
		html_styles[code_def_stylesheet] = make_style(pg, code_def_style, code_def_stylesheet, X_ITALIC_BIT, 9, (pg_char_ptr)"Courier New\0", &par_heading);
#else

	if (!html_styles[pre_tag_stylesheet])
		html_styles[pre_tag_stylesheet] = make_style(pg, pre_tag_style, pre_tag_stylesheet, X_PLAIN_TEXT, PRETAG_DEF_POINTSIZE, (pg_char_ptr)"Monaco\0", &par_heading);
	if (!html_styles[teletype_stylesheet])
		html_styles[teletype_stylesheet] = make_style(pg, teletype_style, teletype_stylesheet, X_PLAIN_TEXT, 12, (pg_char_ptr)"Monaco\0", &par_heading);
	if (!html_styles[code_stylesheet])
		html_styles[code_stylesheet] = make_style(pg, code_style, code_stylesheet, X_PLAIN_TEXT, CODETAG_DEF_POINTSIZE, (pg_char_ptr)"Monaco\0", &par_heading);
	if (!html_styles[code_def_stylesheet])
		html_styles[code_def_stylesheet] = make_style(pg, code_def_style, code_def_stylesheet, X_ITALIC_BIT, 9, CODETAG_DEF_FONT, &par_heading);
#endif

	if (!html_styles[code_var_stylesheet])
		html_styles[code_var_stylesheet] = make_style(pg, code_var_style, code_var_stylesheet, X_ITALIC_BIT, 12, def_font, &par_heading);

	if (!html_styles[blockquote_stylesheet])
		html_styles[blockquote_stylesheet] = make_style(pg, blockquote_style, blockquote_stylesheet, X_PLAIN_TEXT, 12, def_font, &par_heading);
	if (!html_styles[address_stylesheet])
		html_styles[address_stylesheet] = make_style(pg, address_style, address_stylesheet, X_ITALIC_BIT, 12, def_font, &par_heading);
	if (!html_styles[cite_stylesheet])
		html_styles[cite_stylesheet] = make_style(pg, cite_style, cite_stylesheet, X_ITALIC_BIT, 12, def_font, &par_heading);

	if (!html_styles[heading1_stylesheet]) {
		
		par_heading.outline_level += 1;
		html_styles[heading1_stylesheet] = make_style(pg, heading1_style, heading1_stylesheet, X_BOLD_BIT, 24, def_font, &par_heading);
	}
	
	if (!html_styles[heading2_stylesheet]) {
	
		par_heading.outline_level += 1;
		html_styles[heading2_stylesheet] = make_style(pg, heading2_style, heading2_stylesheet, X_BOLD_BIT, 18, def_font, &par_heading);
	}
	
	if (!html_styles[heading3_stylesheet]) {

		par_heading.outline_level += 1;
		html_styles[heading3_stylesheet] = make_style(pg, heading3_style, heading3_stylesheet, X_BOLD_BIT, 14, def_font, &par_heading);
	}
	
	if (!html_styles[heading4_stylesheet]) {

		par_heading.outline_level += 1;
		html_styles[heading4_stylesheet] = make_style(pg, heading4_style, heading4_stylesheet, X_BOLD_BIT, 12, def_font, &par_heading);
	}
	
	par_heading.outline_level = 0;

	if (!html_styles[heading5_stylesheet])
		html_styles[heading5_stylesheet] = make_style(pg, heading5_style, heading5_stylesheet, X_BOLD_BIT, 10, def_font, &par_heading);
	
	if (!html_styles[heading6_stylesheet])
		html_styles[heading6_stylesheet] = make_style(pg, heading6_style, heading6_stylesheet, X_BOLD_BIT, 9, def_font, &par_heading);

	if (style_array)
		pgBlockMove(html_styles, style_array, sizeof(long) * NUM_HTML_STYLES);
}


/************************************** Local Functions *********************************/


/* make_style builds a named stylesheet returning the stylesheet index number. (If named style
already exists then that style is returned). */

static long make_style (pg_ref import_pg, pg_char_ptr name, long html_element, long stylebits,
				short pointsize, pg_char_ptr fontname, par_info_ptr par)
{
	long			index;
	
	if ((index = pgGetNamedStyleIndex(import_pg, name)) == 0) {
		paige_rec_ptr		pg_rec;
		pg_globals_ptr		paige_globals;
		style_info			info;
		font_info			font;
		font_info_ptr		use_font;
		
		pg_rec = UseMemory(import_pg);
		paige_globals = pg_rec->globals;
		UnuseMemory(import_pg);

		info = paige_globals->def_style;
		info.point = (long)pointsize;
		info.point <<= 16;
		
		QDStyleToPaige(stylebits, &info);
		
		info.html_style = html_element;

		if (fontname) {
			pg_short_t		font_size;
			
			pgFillBlock(&font, sizeof(font_info), 0);
			
			for (font_size = 0; font_size < (FONT_SIZE + 2); ++font_size)
				if ((font.name[font_size + 1] = fontname[font_size]) == 0)
					break;
			
			font.name[0] = (pg_char)font_size;
			use_font = &font;
		}
		else
			use_font = &paige_globals->def_font;

		index = pgNewNamedStyle(import_pg, name, &info, use_font, par);
	}
	
	return		index;
}


