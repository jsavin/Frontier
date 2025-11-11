/* Import-Export module. Copyright 1995-96 by DataPak Software, Inc.  This software is a
part of the total PAIGE library.

This source file contains all the member functions for the RTF export class (derived
from the PaigeExportFilter class. */


#include "Paige.h"
#include "pgTraps.h"
#include "pgExceps.h"
#include "machine.h"
#include "pgUtils.h"
#include "pgTxrCPP.h"
#include "defprocs.h"
#include "pgErrors.h"
#include "pgDefStl.h"
#include "pgRTFDef.h"
#include "pgEmbed.h"
#include "pgdeftbl.h"

/* PaigeRTFExportFilter constructor. This sets up the various members common to all filters.
No error checking or file verification occurs just yet. */

static void translate_color (color_value_ptr color, short PG_FAR *redcolor,
         short PG_FAR *greencolor, short PG_FAR *bluecolor);
static long locate_color_in_table (color_value_ptr color, memory_ref color_table);
static short locate_font_id (paige_rec_ptr pg, style_info_ptr style);
static pg_boolean plain_styles (style_info_ptr style);
static pg_boolean long_to_color (long color, color_value_ptr out_color);


PaigeRTFExportFilter::PaigeRTFExportFilter ()
{
   short       str_index;

   file_type = pg_rtf_type;   // Default type for this class
   feature_bits = EXPORT_TEXT_FEATURE | EXPORT_TEXT_FORMATS_FEATURE
               | EXPORT_PAR_FORMATS_FEATURE | EXPORT_PAGE_INFO_FEATURE
               | EXPORT_EMBEDDED_OBJECTS_FEATURE
               | EXPORT_UNICODE_FEATURE
               | EXPORT_SERIAL_SETUP;

   pgFillBlock((void PG_FAR *)file_kind, KIND_STR_SIZE * sizeof(pg_char), 0);
   pgBlockMove((void PG_FAR *)FILETYPE_RTF, (void PG_FAR *)file_kind, FILETYPE_STR_SIZE * sizeof(pg_char));

   pgFillBlock(def_stylename, FONT_SIZE, 0);
   
   for (str_index = 0; str_index < (FONT_SIZE - 1); ++str_index)
      if ((def_stylename[str_index] = def_stylesheet_name[str_index]) == 0)
         break;
}


/* PaigeImportFilter destructor. This removes common items. */

PaigeRTFExportFilter::~PaigeRTFExportFilter ()
{
	if (style_stack)
		DisposeFailedMemory(style_stack);
	if (font_table)
		DisposeFailedMemory(font_table);
	if (color_table)
		DisposeFailedMemory(color_table);
}


/* pgPrepareImport sets up the necessary extra buffers to begin importing. */

pg_error PaigeRTFExportFilter::pgPrepareExport (void)
{
   pgm_globals_ptr      mem_globals;
   style_info        mask;
   format_stack_ptr  def_state;
   short          os_keyword;
   short          rtf_version = RTF_VERSION;
   pg_error       result = NO_ERROR;

   if (export_pg_rec->version < MINIMUM_RTF_VERSION)
      return      BAD_PAIGE_VERSION_ERR;
   
   last_text_par = doing_row = FALSE;
   mem_globals = paige_globals->mem_globals;
   count_last_cr = total_out;

   PG_TRY (mem_globals) {

   #ifdef UNICODE
      if (export_bits & EXPORT_UNICODE_FLAG)
         pgWriteChar((pg_char)PG_BOM);
   #endif
      style_stack = ::MemoryAlloc(paige_globals->mem_globals, sizeof(format_stack), 0, 2);
      font_table = ::MemoryAlloc(paige_globals->mem_globals, sizeof(font_info), 0, 4);
      color_table = ::MemoryAlloc(paige_globals->mem_globals, sizeof(color_value), 0, 4);
   
      def_style = translator.format;
      def_par = translator.par_format;
      def_font = translator.font;

      PushStyleStack();
      WriteCommand(destination_commands, rtf_signature_command, &rtf_version, FALSE);

      if (file_os != MACINTOSH_OS) {
      
         def_font.family_id = (short)def_style.font_index;
         os_keyword = non_mac_os_command;
      }
      else
         os_keyword = mac_platform_command;
   
      WriteCommand(destination_commands, os_keyword, NULL, FALSE);
      WriteCommand(destination_commands, def_font_command, &def_font.family_id, TRUE);
      
      OutputFontTable();
      OutputColorTable();
      OutputStylesheets();
      OutputPageInfo();
      pgFailError(mem_globals, OutputHeaders());
      pgFailError(mem_globals, OutputFooters());
      
// Output the default style and par,  then push the stack for first text:
      
      WriteCommand(paragraph_commands, par_default_command, NULL, TRUE);

      pgFillBlock((void PG_FAR *)&mask, sizeof(style_info), 0);
      
// Fix for 2.0F

      mask.font_index = -2;

      OutputStyleInfo(&def_style, &mask);
      
      def_state_level = GetMemorySize(style_stack);
      def_state = (format_stack_ptr)UseMemoryRecord(style_stack, def_state_level - 1, 0, TRUE);
      pgBlockMove(&def_style, &def_state->style, sizeof(style_info));
      pgBlockMove(&def_font, &def_state->font, sizeof(font_info));

      UnuseMemory(style_stack);
   }

   PG_CATCH {
      
      if (result == NO_ERROR)
         result = paige_globals->mem_globals->last_error;
   }

   PG_ENDTRY;
   
   last_text_par = TRUE;

   return      result;
}


/* pgImportDone disposes all structs created for pgPrepareImport(). */

pg_error PaigeRTFExportFilter::pgExportDone (void)
{
   PG_TRY(paige_globals->mem_globals) {
   
      while (GetMemorySize(style_stack))
         PopStyleStack();
   
      pgFlushOutputBuffer();
   }
   
   PG_CATCH {
      
      return   paige_globals->mem_globals->last_error;
   }
   
   PG_ENDTRY;

   return   io_proc(NULL, io_set_eof, &filepos, &filepos, filemap);
}


/* pgReadNextBlock is the major entry point that reads the next block of text, sets up paragraph
and text formats and returns something for the base class to insert into the pg_ref. */

pg_boolean PaigeRTFExportFilter::pgWriteNextBlock (void)
{
   pgm_globals_ptr      mem_globals = paige_globals->mem_globals;
   format_stack      stack_style;
   pg_char_ptr       text;
   pg_short_t        next_char;
   long          	 num_chars;

   PG_TRY(mem_globals) {
      
      if (translator.par_format_changed || translator.format_changed || translator.font_changed) {
         long        current_state_level;

         current_state_level = GetMemorySize(style_stack);
         
         if (current_state_level > def_state_level)
            PopStyleStack();
         
         current_state_level = GetMemorySize(style_stack) - 1;
         GetMemoryRecord(style_stack, current_state_level, &stack_style);
         
         if (!pgEqualStruct(&stack_style.style, &translator.format, SIGNIFICANT_STYLE_SIZE)
            || !pgEqualStruct(&translator.par_format, &def_par, SIGNIFICANT_PAR_STYLE_SIZE))
            PushStyleStack();
         
         if (translator.par_format_changed) {
         
            WriteCommand(paragraph_commands, par_default_command, NULL, TRUE);
            doing_row = FALSE;
            OutputParInfo(&translator.par_format, &def_par);
         }

         OutputStyleInfo(&translator.format, &stack_style.style);
         
         pgFailError(mem_globals, OutputCustomParams());
      }

      if (translator.data_type == export_embed_type) {
      
         pgFailError(mem_globals, OutputEmbed());
      }
      else {

         text = (pg_char_ptr)UseMemory(translator.data);
         num_chars = GetMemorySize(translator.data);
         
         while (num_chars) {

         if (last_text_par && translator.par_format.table.table_columns)
            OutputRowInfo(&translator.par_format);

            next_char = (pg_short_t)*text++;
   
            if (next_char < (pg_short_t)' ') {
               
               last_text_par = FALSE;

               if (next_char == paige_globals->tab_char) {
                  
                  if (translator.par_format.table.table_columns) {
                     
                     EnterTable();
                     WriteCommand(table_commands, table_cell, NULL, TRUE);
                  }
                  else
                     WriteCommand(special_char_commands, tab_char_command, NULL, TRUE);
               }
               else
               if (next_char == paige_globals->line_wrap_char) {
                  
                  if (translator.par_format.table.table_columns) {

                     EnterTable();
                     WriteCommand(table_commands, table_cell, NULL, TRUE);
                     WriteCommand(table_commands, table_row_end, NULL, FALSE);
                  }
                  else
                     WriteCommand(special_char_commands, cr_command, NULL, FALSE);
                  
                  OutputCR(TRUE);
                  last_text_par = TRUE;
                  doing_row = FALSE;
               }
               else
               if (next_char == 0x0A && translator.par_format.table.table_columns) {
                  
                  WriteCommand(special_char_commands, cr_command, NULL, FALSE);
                  OutputCR(TRUE);
               }

               if (next_char == paige_globals->ff_char)
                  WriteCommand(special_char_commands, page_break_command, NULL, TRUE);
            }
            else
            if (next_char == RTF_COMMAND_CHAR
               || next_char == RTF_GROUPBEGIN_CHAR
               || next_char == RTF_GROUPEND_CHAR
               || next_char == RTF_COLON) {
               
               pgWriteChar(RTF_COMMAND_CHAR);
               pgWriteChar((pg_char)next_char);
            }
            else {
      
      // Normal text:
               
               if (translator.par_format.table.table_columns)
                  EnterTable();

               if (last_char_out == (pg_short_t)command_char_out)
                  pgWriteChar((pg_char)' ');

               if (next_char >= 0x80) {
                  
                  pgWriteChar(RTF_COMMAND_CHAR);
                  pgWriteChar(0x27);
                  pgWriteHexByte((pg_char)next_char);

                  last_char_out = ' ';
               }
               else
                  pgWriteChar((pg_char)next_char);
            }

            --num_chars;
         }
         
         UnuseMemory(translator.data);
      }
   }

   PG_CATCH {
      
      UnuseFailedMemory(translator.data);
      UnuseFailedMemory(style_stack);

      return   FALSE;
   }

   PG_ENDTRY;

   return   TRUE;
}


/* WriteCommand sends an RTF keyword to the file. The keyword token is the table_entry-th
element of rtf_table.  If parameter is non-NULL the numeric parameter is appended.
If delimeter TRUE, a space char is output, otherwise no delimeter is given. */

void PaigeRTFExportFilter::WriteCommand (pg_char_ptr rtf_table, short table_entry,
                  short PG_FAR *parameter, pg_boolean delimeter)
{
   pg_char_ptr    table_ptr;
   short       element_ctr;
   
   OutputCR(FALSE);

   table_ptr = rtf_table;
   
   for (element_ctr = 1; element_ctr < table_entry; ++element_ctr) {
      
      for (;;)
         if (*table_ptr++ == ' ')
            break;
   }
   
   pgWriteChar(RTF_COMMAND_CHAR);
   
   while (*table_ptr > ' ') {
      
      pgWriteChar(*table_ptr);
      ++table_ptr;
   }

   if (parameter)
      pgWriteDecimal(*parameter);
   
   if (delimeter)
      pgWriteChar((pg_bits8)' ');
   else
      last_char_out = command_char_out;
}


/* OutputFontTable determines all the unique fonts in the doc, builds a font table
then sends the table to the file. */

void PaigeRTFExportFilter::OutputFontTable ()
{
   font_info_ptr     fonts;
   pg_char_ptr       fonttype;
   short          num_fonts, font_ctr;
   
   PushStyleStack();

   WriteCommand(destination_commands, font_table_command, NULL, FALSE);

   MemoryCopy(export_pg_rec->fonts, font_table);
   fonts = (font_info_ptr)UseMemory(font_table);
   num_fonts = (short)GetMemorySize(font_table);

   for (font_ctr = 0; font_ctr < num_fonts; ++fonts, ++font_ctr) {
   
      PushStyleStack();
      
      if (file_os != MACINTOSH_OS)
         fonts->family_id = font_ctr;

      WriteCommand(style_commands, font_command, &fonts->family_id, FALSE);
      
      fonttype = GetFontType(fonts);
      pgWriteString(fonttype, RTF_COMMAND_CHAR, (pg_char)' ');
      pgWriteString(&fonts->name[1], 0, TABLE_TERMINATOR);
      PopStyleStack();
   }

   UnuseMemory(font_table);
   
   PopStyleStack();
}


/* GetFontType scans the current font table and returns a pointer to the font type string. */

pg_char_ptr PaigeRTFExportFilter::GetFontType (font_info_ptr font)
{
   pg_char_ptr       table_ptr;
   short          index;

   table_ptr = (pg_char_ptr)font_type_table;

   while (*table_ptr <= font->name[1]) {
      
      if ((index = pgCompareFontTable(&font->name[1], table_ptr)) != 0) {
         
         table_ptr += index;
         return   table_ptr;
      }
      
      for (;;) {
         
         if (*table_ptr++ == 0)
            break;
      }
   }

   return   (pg_char_ptr)def_font_type;
}


/* OutputColorTable determines all the unique colors in the style run, builds a color table
then sends the table to the file. */

void PaigeRTFExportFilter::OutputColorTable ()
{
   style_info_ptr    styles;
   par_info_ptr      pars;
   long           num_styles;
   
   styles = (style_info_ptr)UseMemory(export_pg_rec->t_formats);
   num_styles = GetMemorySize(export_pg_rec->t_formats);
   
   PushStyleStack();

   WriteCommand(destination_commands, color_table_command, NULL, FALSE);

   while (num_styles) {
      
      OutputColorEntry(&styles->fg_color);

      ++styles;
      --num_styles;
   }
   
   UnuseMemory(export_pg_rec->t_formats);

   pars = (par_info_ptr)UseMemory(export_pg_rec->par_formats);
   num_styles = GetMemorySize(export_pg_rec->par_formats);
   
   while (num_styles) {
      
      if (pars->table.table_columns || pars->table.border_info) {
         color_value       color;
         pg_short_t        tab_index;

         if (long_to_color(pars->table.border_info & 0x00FFFFFF, &color))
            OutputColorEntry(&color);
         if (long_to_color(pars->table.border_shading & 0x00FFFFFF, &color))
            OutputColorEntry(&color);
         if (long_to_color(pars->table.cell_borders & 0x00FFFFFF, &color))
            OutputColorEntry(&color);
         
         for (tab_index = 0; tab_index < pars->num_tabs; ++tab_index) {

            if (long_to_color(pars->tabs[tab_index].leader & 0x00FFFFFF, &color))
               OutputColorEntry(&color);

            if (long_to_color(pars->tabs[tab_index].ref_con & 0x00FFFFFF, &color))
               OutputColorEntry(&color);
         }
      }

      ++pars;
      --num_styles;
   }
   
   UnuseMemory(export_pg_rec->par_formats);

   PopStyleStack();
}

/* OutputColorEntry sends a color to the color table if it does not exist. */

void PaigeRTFExportFilter::OutputColorEntry (color_value_ptr color)
{
   short          redcolor, bluecolor, greencolor;
   color_value_ptr      colors;

   if (!locate_color_in_table(color, color_table)) {
      
      colors = (color_value_ptr)AppendMemory(color_table, 1, TRUE);
      *colors = *color;
      
      translate_color(colors, &redcolor, &greencolor, &bluecolor);

      WriteCommand(style_commands, redcolor_command, &redcolor, FALSE);
      WriteCommand(style_commands, greencolor_command, &greencolor, FALSE);
      WriteCommand(style_commands, bluecolor_command, &bluecolor, FALSE);

      UnuseMemory(color_table);

      pgWriteChar(TABLE_TERMINATOR);
   }
}

/* OutputStylesheets sends all the named stylesheets to the output file. */

void PaigeRTFExportFilter::OutputStylesheets ()
{
   named_stylesheet_ptr styles;
   style_info           stylemask;
   par_info          parmask;
   short             num_named_styles, named_index;
   
   pgFillBlock(&stylemask, sizeof(style_info), 0);
   pgFillBlock(&parmask, sizeof(par_info), 0);

   styles = (named_stylesheet_ptr)UseMemory(export_pg_rec->named_styles);
   num_named_styles = (short)GetMemorySize(export_pg_rec->named_styles);
   
   PushStyleStack();

   WriteCommand(destination_commands, stylesheet_command, NULL, FALSE);

   if (num_named_styles == 0) {
      
      PushStyleStack();
      
      named_index = 0;
      WriteCommand(style_commands, style_command, &named_index, FALSE);
      OutputStyleInfo(&def_style, &stylemask);
      OutputParInfo(&def_par, &parmask);
      
      pgWriteString(def_stylename, (pg_bits8)' ', TABLE_TERMINATOR);
      PopStyleStack();
   }
   else {
      
      OutputNamedStyle(&styles[export_pg_rec->def_named_index], 0); // Output default

      for (named_index = 0; named_index < num_named_styles; ++named_index, ++styles)
         if (named_index != (long)export_pg_rec->def_named_index)
            OutputNamedStyle(styles, named_index + 1);
   }

   PopStyleStack();
}

/* OutputNamedStyle sends a single stylesheet out. */

void PaigeRTFExportFilter::OutputNamedStyle (named_stylesheet_ptr style, short named_index)
{
   style_info_ptr stylesheet;
   par_info_ptr   par_stylesheet;
   style_info     stylemask;
   par_info    parmask;

   PushStyleStack();

   WriteCommand(style_commands, style_command, &named_index, FALSE);
   pgFillBlock(&stylemask, sizeof(style_info), 0);
   pgFillBlock(&parmask, sizeof(par_info), 0);

   if ((stylesheet = pgLocateStyleSheet(export_pg_rec, style->stylesheet_id, NULL)) != NULL) {
   
      OutputStyleInfo(stylesheet, &stylemask);
      UnuseMemory(export_pg_rec->t_formats);
   }

   if ((par_stylesheet = pgLocateParStyleSheet(export_pg_rec, style->stylesheet_id)) != NULL) {
   
      OutputParInfo(par_stylesheet, &parmask);
      UnuseMemory(export_pg_rec->par_formats);
   }

   pgWriteString(style->name, (pg_bits8)' ', TABLE_TERMINATOR);
   PopStyleStack();
}

/* OutputPageInfo sends page and margin information. */

void PaigeRTFExportFilter::OutputPageInfo ()
{
   rectangle      margins;
   short       margin_value;

   margins = export_pg_rec->doc_info.margins;

   if (margins.top_left.h) {
      
      margin_value = PointConversion((short)margins.top_left.h, TRUE, TRUE);
      WriteCommand(document_commands, left_margin_command, &margin_value, TRUE);
   }

   if (margins.bot_right.h) {
      
      margin_value = PointConversion((short)margins.bot_right.h, TRUE, TRUE);
      WriteCommand(document_commands, right_margin_command, &margin_value, TRUE);
   }

   if (margins.top_left.v) {
      
      margin_value = PointConversion((short)margins.top_left.v, TRUE, TRUE);
      WriteCommand(document_commands, top_margin_command, &margin_value, TRUE);
   }

   if (margins.bot_right.v) {
      
      margin_value = PointConversion((short)margins.bot_right.v, TRUE, TRUE);
      WriteCommand(document_commands, bottom_margin_command, &margin_value, TRUE);
   }
}


/* OutputStyleInfo sends changed style_info attributes. */

void PaigeRTFExportFilter::OutputStyleInfo (style_info_ptr style, style_info_ptr mask)
{
   short       variable;

   if (style->font_index != mask->font_index) {
      font_info         associated_font;
      
      GetMemoryRecord(font_table, (long)style->font_index, (void PG_FAR *)&associated_font);
      WriteCommand(style_commands, font_command, &associated_font.family_id, FALSE);
   }

   if (style->point != mask->point) {
      
      variable = PointConversion(pgHiWord(style->point), FALSE, FALSE);
      WriteCommand(style_commands, pointsize_command, &variable, FALSE);
   }

   if (plain_styles(style) && !plain_styles(mask))
      WriteCommand(style_commands, plain_command, NULL, FALSE);
   else {

      if (style->styles[bold_var])
         if (style->styles[bold_var] != mask->styles[bold_var])
            WriteCommand(style_commands, bold_command, NULL, FALSE);

      if (style->styles[all_caps_var])
         if (style->styles[all_caps_var] != mask->styles[all_caps_var])
            WriteCommand(style_commands, all_caps_command, NULL, FALSE);

      if (style->styles[subscript_var])
         if (style->styles[subscript_var] != mask->styles[subscript_var]) {
            
            variable = PointConversion(style->styles[subscript_var], TRUE, FALSE);
            WriteCommand(style_commands, subscript_command, &variable, FALSE);
         }

      if (style->styles[italic_var])
         if (style->styles[italic_var] != mask->styles[italic_var])
            WriteCommand(style_commands, italic_command, NULL, FALSE);

      if (style->styles[outline_var])
         if (style->styles[outline_var] != mask->styles[outline_var])
            WriteCommand(style_commands, outline_command, NULL, FALSE);

      if (style->styles[small_caps_var])
         if (style->styles[small_caps_var] != mask->styles[small_caps_var])
            WriteCommand(style_commands, small_caps_command, NULL, FALSE);

      if (style->styles[strikeout_var])
         if (style->styles[strikeout_var] != mask->styles[strikeout_var])
            WriteCommand(style_commands, strikeout_command, NULL, FALSE);

      if (style->styles[shadow_var])
         if (style->styles[shadow_var] != mask->styles[shadow_var])
            WriteCommand(style_commands, shadow_command, NULL, FALSE);

      if (style->styles[underline_var])
         if (style->styles[underline_var] != mask->styles[underline_var])
            WriteCommand(style_commands, underline_command, NULL, FALSE);

      if (style->styles[superscript_var])
         if (style->styles[superscript_var] != mask->styles[superscript_var]) {

            variable = PointConversion(style->styles[superscript_var], TRUE, FALSE);
            WriteCommand(style_commands, superscript_command, &variable, FALSE);
         }

      if (style->styles[word_underline_var])
         if (style->styles[word_underline_var] != mask->styles[word_underline_var])
            WriteCommand(style_commands, word_underline_command, NULL, FALSE);

      if (style->styles[dbl_underline_var])
         if (style->styles[dbl_underline_var] != mask->styles[dbl_underline_var])
            WriteCommand(style_commands, double_underline_command, NULL, FALSE);

      if (style->styles[dotted_underline_var])
         if (style->styles[dotted_underline_var] != mask->styles[dotted_underline_var])
            WriteCommand(style_commands, dotted_underline_command, NULL, FALSE);

      if (style->styles[hidden_text_var])
         if (style->styles[hidden_text_var] != mask->styles[hidden_text_var])
            WriteCommand(style_commands, invis_text_command, NULL, FALSE);
   }

   if (!pgEqualStruct((void PG_FAR *)&style->fg_color, (void PG_FAR *)&mask->fg_color, sizeof(color_value))) {
      
      variable = (short)locate_color_in_table(&style->fg_color, color_table);
      variable -= 1;
      WriteCommand(style_commands, fgcolor_command, &variable, FALSE);
   }

   if (style->char_extra && style->char_extra != mask->char_extra) {
      long     kerning_register;
      
      kerning_register = (long)style->char_extra;
      
      if (kerning_register < 0)
         kerning_register += 0x00400000;
      
      kerning_register >>= 14;
      
      variable = (short)kerning_register;
      WriteCommand(style_commands, expand_condense_command, &variable, FALSE);
   }
}


/* OutputParInfo sends changed par_info attributes */

void PaigeRTFExportFilter::OutputParInfo (par_info_ptr par, par_info_ptr mask)
{
   short    variable;
   
   if (par->table.border_info != mask->table.border_info) {
      long        border_info, left, right, top, bottom;
      
      border_info = par->table.border_info;
      left = border_info & PG_BORDER_LEFT;
      right = (border_info & PG_BORDER_RIGHT) >> 8;
      top = (border_info & PG_BORDER_TOP) >> 16;
      bottom = (border_info & PG_BORDER_BOTTOM) >> 24;

      if ((border_info & PG_BORDER_ALLSIDES) == PG_BORDER_ALLSIDES) {
         
         WriteCommand(paragraph_commands, box_border, NULL, FALSE);
         
         if (right & PG_BORDER_SHADOW)
            WriteCommand(paragraph_commands, shadow_border, NULL, FALSE);
      }
      else {
         
         if (left)
            WriteBorderCommand(left, paragraph_commands, left_border);

         if (top)
            WriteBorderCommand(top, paragraph_commands, (short)top);

         if (right)
            WriteBorderCommand(right, paragraph_commands, (short)right);

         if (bottom)
            WriteBorderCommand(bottom, paragraph_commands, (short)bottom);
      }
      
      if (par->table.border_spacing) {
         
         variable = PointConversion((short)par->table.border_spacing, TRUE, TRUE);
         WriteCommand(paragraph_commands, border_spacing, &variable, FALSE);
      }

      if (border_info & PG_BORDER_PENSIZE)
         WriteCommand(paragraph_commands, thick_border, NULL, TRUE);
      else
      if (border_info & PG_BORDER_DOUBLE)
         WriteCommand(paragraph_commands, double_border, NULL, TRUE);
      else
         WriteCommand(paragraph_commands, single_border, NULL, TRUE);

   }

   if (par->justification != mask->justification) {
      
      switch (par->justification) {

         case justify_left:
         case force_left:
            variable = left_justify_command;
            break;
            
         case justify_center:
            variable = center_justify_command;
            break;
            
         case justify_right:
         case force_right:
            variable = right_justify_command;
            break;

         case justify_full:
            variable = full_justify_command;
            break;
      }
      
      WriteCommand(paragraph_commands, variable, NULL, FALSE);
   }
   
   if (par->indents.left_indent != mask->indents.left_indent) {
      
      variable = PointConversion((short)par->indents.left_indent, TRUE, TRUE);
      WriteCommand(paragraph_commands, left_indent_command, &variable, FALSE);
   }

   if (par->indents.right_indent != mask->indents.right_indent) {
      
      variable = PointConversion((short)par->indents.right_indent, TRUE, TRUE);
      WriteCommand(paragraph_commands, right_indent_command, &variable, FALSE);
   }

   if (par->indents.first_indent != mask->indents.first_indent) {
      
      variable = PointConversion((short)par->indents.first_indent, TRUE, TRUE);
      WriteCommand(paragraph_commands, first_indent_command, &variable, FALSE);
   }

   if (par->top_extra != mask->top_extra) {

      variable = PointConversion((short)par->top_extra, TRUE, TRUE);
      WriteCommand(paragraph_commands, space_before_command, &variable, FALSE);
   }

   if (par->bot_extra != mask->bot_extra) {

      variable = PointConversion((short)par->bot_extra, TRUE, TRUE);
      WriteCommand(paragraph_commands, space_after_command, &variable, FALSE);
   }
   
   if (par->leading_fixed != mask->leading_fixed || par->spacing != mask->spacing) {
      
      if (par->spacing != 0) {
      
         variable = pgHiWord(pgMultiplyFixed(par->spacing, (pg_fixed)0x000E0000));
         variable = (variable * 2) * 10;
      }
      else
         variable = PointConversion((short)par->leading_fixed, TRUE, TRUE);

      WriteCommand(paragraph_commands, line_spacing_command, &variable, FALSE);
   }

   if (par->direction & PAR_ON_NEXT_PAGE)
      WriteCommand(paragraph_commands, page_break_before_command, NULL, FALSE);
   if (par->direction & KEEP_PARS_TOGETHER)
      WriteCommand(paragraph_commands, keep_together_command, NULL, FALSE);

// Handle tab stops, if any:

   if (par->num_tabs && !par->table.table_columns) {
      pg_short_t     tab_index;
      rectangle      vis_bounds, page_bounds;
      long        tab_compensate;

      for (tab_index = 0; tab_index < par->num_tabs; ++tab_index) {
         
         if (tab_index >= mask->num_tabs || par->tabs[tab_index].position != mask->tabs[tab_index].position) {
            
            switch (par->tabs[tab_index].tab_type) {

               case center_tab:
                  variable = center_tab_command;
                  break;
                  
               case right_tab:
                  variable = right_tab_command;
                  break;
                  
               case decimal_tab:
                  variable = decimal_tab_command;
                  break;
                  
               default:
                  variable = 0;
                  break;
            }

            if (variable)
               WriteCommand(paragraph_commands, variable, NULL, FALSE);
            
            if (par->tabs[tab_index].leader) {
               
               if (par->tabs[tab_index].leader == (long)'.')
                  variable = dot_leader_command;
               else
                  variable = dash_leader_command;
               
               WriteCommand(paragraph_commands, variable, NULL, FALSE);
            }
            
            pgAreaBounds(export_pg, &page_bounds, &vis_bounds);
            tab_compensate = -export_pg_rec->tab_compensate;

            if (export_pg_rec->tab_base != TAB_BOUNDS_RELATIVE)
               tab_compensate -= page_bounds.top_left.h;

            variable = PointConversion((short)(par->tabs[tab_index].position + tab_compensate), TRUE, TRUE);
            WriteCommand(paragraph_commands, set_tab_command, &variable, FALSE);
         }
      }
   }
}

/* WriteBorderCommand sends a border command to output. */

void PaigeRTFExportFilter::WriteBorderCommand (long border_info, pg_char_ptr command_table, short command)
{
   WriteCommand(command_table, command, NULL, FALSE);
   
   if (border_info & PG_BORDER_DOUBLE)
      WriteCommand(paragraph_commands, double_border, NULL, FALSE);

   if (border_info & PG_BORDER_SHADOW)
      WriteCommand(paragraph_commands, shadow_border, NULL, FALSE);

   if ((border_info & PG_BORDER_PENSIZE) > 1)
      WriteCommand(paragraph_commands, thick_border, NULL, FALSE);

   if (border_info & PG_BORDER_DOTTED)
      WriteCommand(paragraph_commands, dot_border, NULL, FALSE);
}


/* OutputHeaders in this version does nothing. */

pg_error PaigeRTFExportFilter::OutputHeaders ()
{
   return   NO_ERROR;
}


/* OutputFooters in this version does nothing. */

pg_error PaigeRTFExportFilter::OutputFooters ()
{
   return   NO_ERROR;
}


/* OutputEmbed sends an embed_ref type to the RTF stream. */

pg_error PaigeRTFExportFilter::OutputEmbed ()
{
   volatile embed_ref      embed = MEM_NULL;
   pg_embed_ptr   		   embed_ptr;
   pg_bits8_ptr   		   data_stream;
   long        			   embed_type, data_size, storage;
   pg_error    result = NO_ERROR;
   short       variable_h, variable_v;
   short       preamble_ctr = 0;

   PG_TRY(paige_globals->mem_globals) {

      embed = (embed_ref)translator.format.embed_object;

      if ((data_stream = pgPrepareEmbedData(embed, &data_size, &storage)) != NULL) {
         
         PushStyleStack();
         
         embed_ptr = (pg_embed_ptr)UseMemory(embed);
         embed_type = embed_ptr->type & 0x0000FFFF;
      
         switch (embed_type) {
            
            case embed_ole:
               WriteCommand(destination_commands, object_command, NULL, FALSE);

               if ((variable_h = embed_ptr->uu.pict_data.twips_width) == 0)
                  variable_h = (short)PointConversion((short)embed_ptr->width, TRUE, TRUE);                    

               WriteCommand(destination_commands, object_width_command, &variable_h, FALSE);

               if ((variable_v = embed_ptr->uu.pict_data.twips_height) == 0)
                  variable_v = (short)PointConversion((short)embed_ptr->height, TRUE, TRUE);                   

               WriteCommand(destination_commands, object_height_command, &variable_v, FALSE);

               break;

            case embed_mac_pict:
            case embed_meta_file:
               WriteCommand(destination_commands, pict_command, NULL, FALSE);
               
               if (embed_type == embed_mac_pict)
                  WriteCommand(destination_commands, mac_pict_command, NULL, FALSE);
               else {
                  
                  if ((variable_h = (short)embed_ptr->uu.pict_data.mapping_mode) == 0)
                     variable_h = 8;

                  WriteCommand(destination_commands, windows_metafile_command, &variable_h, FALSE);
               }
               
               variable_h = (short)embed_ptr->width;

               if (embed_type == embed_meta_file) {
                  
                  if ((variable_h = embed_ptr->uu.pict_data.twips_width) == 0)
                     variable_h = (short)PointConversion((short)embed_ptr->width, TRUE, TRUE);                    
               }

               WriteCommand(destination_commands, pict_width_command, &variable_h, FALSE);
               
               variable_v = (short)embed_ptr->height;

               if (embed_type == embed_meta_file) {

                  if ((variable_v = embed_ptr->uu.pict_data.twips_height) == 0)
                     variable_v = (short)PointConversion((short)embed_ptr->height, TRUE, TRUE);                   
               }

               WriteCommand(destination_commands, pict_height_command, &variable_v, FALSE);

               if (embed_type == embed_meta_file) {
                  
                  variable_h = (short)embed_ptr->width * 20;
                  variable_v = (short)embed_ptr->height * 20;
                  
                  if (embed_ptr->uu.pict_data.twips_gwidth)
                     variable_h = embed_ptr->uu.pict_data.twips_gwidth;
                  if (embed_ptr->uu.pict_data.twips_gheight)
                     variable_v = embed_ptr->uu.pict_data.twips_gheight;

                  WriteCommand(destination_commands, picframe_width_command, &variable_h, FALSE);
                  WriteCommand(destination_commands, picframe_height_command, &variable_v, FALSE);
               
                  
                  variable_h = pgHiWord(embed_ptr->uu.pict_data.horiz_scale);
                  variable_v = pgHiWord(embed_ptr->uu.pict_data.vert_scale);
                  
                  if (variable_h)
                     WriteCommand(destination_commands, pict_hscale_command, &variable_h, FALSE);
                  if (variable_v)
                     WriteCommand(destination_commands, pict_vscale_command, &variable_v, FALSE);
               }
               
               pgWriteChar((pg_bits8)' ');
               break;
            
            case embed_unsupported_object:
               WriteCommand(destination_commands, object_command, NULL, FALSE);
               
               if (embed_ptr->rtf_text_data) {
                  long        preamble_size;
                  pg_char_ptr    preamble_data;
                  
                  preamble_size = GetMemorySize(embed_ptr->rtf_text_data);
                  preamble_data = (pg_char_ptr)UseMemory(embed_ptr->rtf_text_data);
                  
                  while (preamble_size) {
                     
                     pgWriteChar((pg_bits8)*preamble_data);
                     
                     if (*preamble_data == RTF_GROUPBEGIN_CHAR)
                        ++preamble_ctr;
                     else
                     if (*preamble_data == RTF_GROUPEND_CHAR)
                        --preamble_ctr;
                     
                     ++preamble_data;
                     --preamble_size;
                  }
                  
                  UnuseMemory(embed_ptr->rtf_text_data);
               }

               break;
         }

         while (data_size) {
            
            pgWriteHexByte(*data_stream++);
            OutputCR(FALSE);
            --data_size;
         }

         PopStyleStack();
         
         if (embed_ptr->embed_represent) {

            PushStyleStack();
            WriteCommand(destination_commands, object_result_command, NULL, TRUE);
            PushStyleStack();

            translator.format.embed_object = (long)embed_ptr->embed_represent;
            OutputEmbed();
            translator.format.embed_object = (long)embed;

            PopStyleStack();
            PopStyleStack();

            while (preamble_ctr > 0) {
               
               pgWriteChar((pg_bits8)RTF_GROUPEND_CHAR);
               --preamble_ctr;
            }
         }

         UnuseMemory(embed);

         pgReleaseEmbedData(embed, storage);
      }
   }
   
   PG_CATCH {
      
      if (embed)
         UnuseFailedMemory(embed);
      
      result = paige_globals->mem_globals->last_error;
   }
   
   PG_ENDTRY;

   return      result;
}


/* OutputCustomParams gives an overriding class a chance to output additional parameters
to the current text style. */

pg_error PaigeRTFExportFilter::OutputCustomParams()
{
   return   NO_ERROR;
}


/* PushStyleStack saves the current style state into the style stack then
outputs a group beginning char. */

void PaigeRTFExportFilter::PushStyleStack (void)
{
   format_stack_ptr     stack;

   OutputCR(FALSE);

   pgWriteChar(RTF_GROUPBEGIN_CHAR);

   stack = (format_stack_ptr)AppendMemory(style_stack, 1, FALSE);
   pgBlockMove(&translator.format, &stack->style, sizeof(style_info));
   pgBlockMove(&translator.font, &stack->font, sizeof(font_info));
   
   UnuseMemory(style_stack);
}


/* PopStyleStack decrements the style-stack state by one and outputs a group ending char. */

void PaigeRTFExportFilter::PopStyleStack (void)
{
   long        stack_size;

   if ((stack_size = GetMemorySize(style_stack)) > 0) {
      
      SetMemorySize(style_stack, stack_size - 1);
      pgWriteChar(RTF_GROUPEND_CHAR);
   }
   
   OutputCR(FALSE);
}


/* OutputCR checks the counter since last return char writes one if it has hit
the max. Or, if unconditional then we do it anyway. */

void PaigeRTFExportFilter::OutputCR (pg_boolean unconditional)
{
   if (unconditional || ((total_out - count_last_cr) > (long)MAX_NO_CR)) {
      
      pgWriteChar((pg_bits8)paige_globals->line_wrap_char);
      
      if (file_os == WINDOWS_OS)
         pgWriteChar((pg_bits8)paige_globals->soft_line_char);

      count_last_cr = total_out;
   }
}


/* DecipointConvert converts the given value to decipoints. If convert_resolution is
TRUE then we first convert the value to points based on the pg_ref resolution. If x10 is
TRUE then the output is multiplied by 10, otherwise it is multiplied x 2. */

short PaigeRTFExportFilter::PointConversion (short value, pg_boolean convert_resolution,
         pg_boolean x10)
{
   short       result;
   
   result = value;
   
   if (convert_resolution)
      result = pgResolutionConvert(export_pg_rec, value);

   result *= 2;

   if (x10)
      result *= 10;

   return   result;
}

/* OutputRowInfo outputs the preamble to a row. */

void PaigeRTFExportFilter::OutputRowInfo (par_info_ptr par)
{
   long        border_info;
   long        column_index, last_position, shading;
   short       variable;

   WriteCommand(destination_commands, table_defaults, NULL, TRUE);
   last_position = 0;
   last_text_par = FALSE;

   for (column_index = 0; column_index < par->table.table_columns; ++column_index) {

      if (!(border_info = par->tabs[column_index].leader))
         border_info = par->table.cell_borders;

      if (border_info & PG_BORDER_LEFT) {
      
         WriteCommand(table_commands, cell_border_left, NULL, FALSE);
         WriteCommand(paragraph_commands, single_border, NULL, TRUE);
      }

      if (border_info & PG_BORDER_TOP) {
      
         WriteCommand(table_commands, cell_border_top, NULL, FALSE);
         WriteCommand(paragraph_commands, single_border, NULL, TRUE);
      }

      if (border_info & PG_BORDER_RIGHT) {
      
         WriteCommand(table_commands, cell_border_right, NULL, FALSE);
         WriteCommand(paragraph_commands, single_border, NULL, TRUE);
      }

      if (border_info & PG_BORDER_BOTTOM) {
      
         WriteCommand(table_commands, cell_border_bottom, NULL, FALSE);
         WriteCommand(paragraph_commands, single_border, NULL, TRUE);
      }
      
      if (!(shading = par->tabs[column_index].ref_con))
         shading = par->table.border_shading;

      if (shading) {
         color_value       color;
         
         long_to_color(shading, &color);
         variable = (short)locate_color_in_table(&color, color_table);
         variable -= 1;
         WriteCommand(paragraph_commands, border_colorfill, &variable, FALSE);
      }

      if (par->tabs[column_index].position)
         last_position += par->tabs[column_index].position;
      else
         last_position += par->column_var;

      variable = PointConversion((short)last_position, TRUE, TRUE);
      WriteCommand(table_commands, cell_setright, &variable, TRUE);
   }
}


/* EnterTable checks to see if we are in table mode and outputs "table enter" command if
necessary. */

void PaigeRTFExportFilter::EnterTable ()
{
   if (translator.par_format.table.table_columns && !doing_row) {
      
      WriteCommand(table_commands, enter_table, NULL, TRUE);
      doing_row = TRUE;
   }
}


/**************************************** Local Functions **************************************/


/* translate_color converts a color value RGB to 0-255 shorts. */

static void translate_color (color_value_ptr color, short PG_FAR *redcolor,
         short PG_FAR *greencolor, short PG_FAR *bluecolor)
{
   pg_short_t     red, green, blue;

   red = color->red >> 8;
   green = color->green >> 8;
   blue = color->blue >> 8;

   *redcolor = (short)red;
   *greencolor = (short)green;
   *bluecolor = (short)blue;
}


/* locate_color_in_table locates a color in the color table. If found, the nth element + 1
is returned, otherwise it is zero. */

static long locate_color_in_table (color_value_ptr color, memory_ref color_table)
{
   color_value_ptr      colors;
   long           num_colors, index;
   
   if ((num_colors = GetMemorySize(color_table)) > 0) {
      
      colors = (color_value_ptr)UseMemory(color_table);
      
      for (index = 1; index <= num_colors; ++index, ++colors) {
         
         if (color->red == colors->red && color->green == colors->green && color->blue == colors->blue) {
            
            UnuseMemory(color_table);
            
            return   index;
         }
      }
      
      UnuseMemory(color_table);
   }
   
   return   0;
}

/* plain_styles returns TRUE if all styles are turned off. */

static pg_boolean plain_styles (style_info_ptr style)
{
   short    index;
   
   for (index = 0; index < MAX_STYLES; ++index)
      if (style->styles[index])
         return   FALSE;
   
   return   TRUE;
}

/* long_to_color translates a single color to an RGB color. If pure black TRUE is returned. */

static pg_boolean long_to_color (long color, color_value_ptr out_color)
{
   if (!color) {
      
      out_color->red = out_color->blue = out_color->green = 0;
      return   TRUE;
   }
   
   out_color->red = (pg_short_t)((long)(color >> 8) & 0xFF00);
   out_color->green = (pg_short_t)(color & 0xFF00);
   out_color->blue = (pg_short_t)((color << 8) & 0xFF00);
   return TRUE;
}
