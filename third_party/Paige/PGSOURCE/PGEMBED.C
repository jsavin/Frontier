/* This is the PAIGE "insert anything" extension. This provides a slightly
higher-level interface to an otherwise complicated implementation of inserting
something that is not a real "text" item.  Copyright 1994-1995 by DataPak
Software, Inc.  All rights reserved. Software by "Gar."  */

/* 12/07/94 Corrected, improved and expanded by board member, TR Shaw, OITC. Expansions
   improvements and fixes are copyrighted 1994 OITC, Inc. */

/* 3/5/95 Made so that changes in the future will still support previous version - TRS/OITC */

//MAC NOTE: Need to change ControlRef to ControlHandle for Universal Interfaces before 2.0a3  ETO #16 TRS/OITC

#include <stdint.h>
#include "Paige.h"
#include "pgTraps.h"
#include "defprocs.h"
#include "pgUtils.h"
#include "pgDefStl.h"
#include "pgSelect.h"
#include "pgText.h"
#include "pgBasics.h"
#include "machine.h"
#include "pgFiles.h"
#include "pgEmbed.h"
#include "pgExceps.h"
#include "pgOSUtl.h"
#include "pgEdit.h"
#include "pgFrame.h"

#ifdef USING_GIF
#include "pgGIF.h"
#endif

#ifdef THINK_C
#define ControlRef ControlHandle
#endif

#define DROP_BOX_SIZE         2
#define DOUBLE_OFFSET         2


#ifndef WINDOWS_PLATFORM
#define MAKELONG(low, high) ((long)(((unsigned short)(low)) | (((unsigned long)((unsigned short)(high))) << 16)))
#endif

#ifndef MAC_PLATFORM

struct Mac_Picture {
   short                picSize;
   Rect                 picFrame;
};
typedef struct Mac_Picture Mac_Picture;
typedef Mac_Picture PG_FAR *Mac_PicPtr;

#endif

/* Older border record (changed in 3.01) */

enum {
	border_none,
	border_box,
	border_top,
	border_bottom,
	border_left,
	border_right
};

enum {
	border_single,
	border_double,
	border_dotted,
	border_dashed,
	border_shadow,
	border_hairline
};

struct old_pg_border {
	pg_char		type;						/* border type */
	pg_char		style;						/* border style */
	short		pen_size;					/* border pen size */
	short		reserved;					/* reserved */
	color_value	color;						/* border color */
};
typedef struct old_pg_border 	old_pg_border;

#define MAC_CONTROL_STR    "Mac"
#define OLE_STR            "Obj"
#define MAC_PICTURE_STR    "Pict"
#define  MAC_QT_STR        "QT"
#define WIN_META_STR    "Meta"
#define OS2_META_STR    "Meta"
#define WIN_BMP_STR        "bmp"
#define WIN_DDBMP_STR      "ddbmp"
#define UNKNOWN_STR        "Unkn"

static void initialize_embed_data (pgm_globals_ptr mem_globals, embed_ref the_ref,
      void PG_FAR *data, pg_fixed vert_pos);
static pg_boolean next_style_same_ref (paige_rec_ptr pg, style_walk_ptr walker);
static pg_boolean previous_style_same_ref (paige_rec_ptr pg, style_walk_ptr walker);
static long extra_trailing_spaces (paige_rec_ptr pg, style_walk_ptr walker,
      pg_char_ptr text, long length);
static pg_boolean have_saved_ref (style_info_ptr stylebase, short current_index);
static void pack_pic_embed(pack_walk_ptr walker, pg_embed_ptr embed_ptr);
static void unpack_pic_embed(pack_walk_ptr walker, pg_embed_ptr embed_ptr);
static co_ordinate_ptr center_unknown_text(pg_embed_ptr item, graf_device_ptr port, pg_char_ptr text, short length, rectangle_ptr draw_bounds, co_ordinate_ptr top_left_pt);
static void set_string_data (pgm_globals_ptr mem_globals, pg_embed_ptr embed_ptr, long the_type,
      pg_char_ptr alt_char_ptr, short str_size);
static void get_visual_frame (paige_rec_ptr pg, pg_embed_ptr embed_ptr,
      select_pair_ptr range, rectangle_ptr bounds);
static void unpack_altsize_text (paige_rec_ptr pg, pack_walk_ptr walker, pg_char_ptr str, long size);
static pg_boolean compare_embed_data (pg_embed_ptr compare_to, pg_embed_ptr compare_from);

static void standard_embed_draw (paige_rec_ptr pg, pg_embed_ptr item,
      rectangle_ptr draw_bounds, draw_points_ptr draw_position, pg_boolean drawing_to_bitmap);
static void standard_embed_dispose (pg_embed_ptr item, long the_type);
static void standard_embed_copy (pg_embed_ptr item, long the_type);
static long standard_embed_click (paige_rec_ptr pg, pg_embed_ptr item,
      long item_type, pg_embed_click_ptr click_info);
static void custom_data_callback (paige_rec_ptr pg, pack_walk_ptr walker, pg_embed_ptr embed_ptr,
      embed_callback callback, memory_ref buffer_ref);
static pg_boolean embed_empty (pg_embed_ptr embed_ptr);
static void pack_url (pack_walk_ptr walker, memory_ref url_ref);
static void PG_FAR * pack_graphics (pack_walk_ptr walker, short the_type, void PG_FAR *data);
static generic_var unpack_graphics (pgm_globals_ptr mem_globals, pack_walk_ptr walker, long type);
static long find_url_ref (memory_ref listref, memory_ref url);
static void unpack_url (paige_rec_ptr pg, pack_walk_ptr walker);
static void convert_old_borders (pg_embed_ptr embed_ptr, old_pg_border PG_FAR *old_border);


STATIC_PASCAL (void) measure_embed (paige_rec_ptr pg, style_walk_ptr walker,
      pg_char_ptr data, size_t length, pg_short_t slop, long PG_FAR *positions,
      short PG_FAR *types, short measure_verb, size_t current_offset, short scale_widths,
      short call_order);
STATIC_PASCAL (long) track_embed_ctl (paige_rec_ptr pg, short verb,
      t_select_ptr first_select, t_select_ptr last_select, style_walk_ptr styles,
      pg_char_ptr associated_text, point_start_ptr bounds_info, short modifiers,
      long track_refcon);
STATIC_PASCAL (void) embed_draw (paige_rec_ptr pg, style_walk_ptr walker, pg_char_ptr data,
      pg_short_t offset, pg_short_t length, draw_points_ptr draw_position,
      long extra, short draw_mode);
STATIC_PASCAL (void) embed_copy (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
      short reason_verb, format_ref all_styles, style_info_ptr style);
STATIC_PASCAL (void) embed_delete (paige_rec_ptr pg, pg_globals_ptr globals,
      short reason_verb, format_ref all_styles, style_info_ptr style);
STATIC_PASCAL (pg_boolean) embed_setup_insert_init (paige_rec_ptr pg, style_info_ptr style,
      long position);
STATIC_PASCAL (pg_boolean) embed_setup_insert (paige_rec_ptr pg, style_info_ptr style,
      long position);
STATIC_PASCAL (long) embed_char_info (paige_rec_ptr pg, style_walk_ptr style_walker,
      pg_char_ptr data, size_t block_offset, size_t offset_begin, size_t offset_end, size_t char_offset, long mask_bits);
STATIC_PASCAL (void) embed_activate (paige_rec_ptr pg_rec, style_info_ptr style,
      select_pair_ptr text_range, pg_char_ptr text, short front_back_state,
      short perm_state, pg_boolean show_hilite);

#ifdef WINDOWS_PLATFORM
static void draw_meta_file (paige_rec_ptr pg, memory_ref the_data, HDC target_dc, pg_embed_ptr embed_ptr, RECT PG_FAR *target_rect);
#endif

#ifdef MAC_PLATFORM

#include <Controls.h>
#include <Windows.h>
static void detach_control_from_window (ControlHandle ctl);

#endif



/* pgNewEmbedRef creates a new embedding reference. It does not get inserted
into anything, but you can keep it around and insert it into a pg_ref (or
apply it to already-existing bytes in a pg_ref) later. If keep_around is TRUE
the object will not get disposed when embedded in the text stream, i.e. you
want to always have the object around in your app even if it is inserted into
text then deleted.
The modifier parameter value is intepreted differently for various embed types
as follows:  if type is character substitution, date-time swap or page numbering
type (or any other type that draws text), modifier is an alternate stylesheet
ID (or zero if none), which gets set when the item is drawn and measured. If
type is a rounded rectangle or arc, modifier is the angle amount.  Currently
all other types ignore the modifier.
The user_refcon value is placed in the embed_ref. Note, this value is different
than the "style user_refcon" since the style refcon is unique to each style,
given at the moment of pgInsertEmbed.  */


PG_PASCAL (embed_ref) pgNewEmbedRef (pgm_globals_ptr mem_globals, long item_type,
      void PG_FAR *item_data, long modifier, long flags, pg_fixed vert_pos,
      long user_refcon, pg_boolean keep_around)
{
   volatile embed_ref       result;
   pg_embed_ptr      		embed_ptr;
   long               		old_current_id;
   
   old_current_id = mem_globals->current_id;
   ++mem_globals->next_mem_id;
   mem_globals->current_id = mem_globals->next_mem_id;

   result = MemoryAllocClear(mem_globals, sizeof(pg_embed_rec), 1, 0);
   embed_ptr = (pg_embed_ptr) UseMemory(result);

   embed_ptr->version = CURRENT_EMBED_VERSION;
   embed_ptr->type = item_type;
   embed_ptr->type |= flags;
   embed_ptr->uu.pict_data.pict_transparent = TRUE;
   embed_ptr->shading = 0x00FFFFFF;

   if (keep_around) {
   
      embed_ptr->used_ctr = 1;
      embed_ptr->type |= KEPT_AROUND;
   }
   
   embed_ptr->user_refcon = user_refcon;
   embed_ptr->modifier = modifier;

   PG_TRY (mem_globals)
   {
      initialize_embed_data(mem_globals, result, item_data, vert_pos);
   }
   PG_CATCH
   {
      UnuseMemory(result);
      pgEmbedDispose (result);
      mem_globals->current_id = old_current_id;
      PG_RERAISE();
   }
   PG_ENDTRY

   UnuseMemory(result);
   mem_globals->current_id = old_current_id;

   return   result;
}


/* pgEmbedDispose disposes the embed_ref. This is only called if "keep_around"
was TRUE when the embed_ref was created. */

PG_PASCAL (void) pgEmbedDispose (embed_ref ref)
{
   pg_embed_ptr         embed_ptr;
   
   embed_ptr = (pg_embed_ptr) UseMemory(ref);
   pgDefaultEmbedCallback((paige_rec_ptr)NULL, embed_ptr, embed_ptr->type & EMBED_TYPE_MASK,
         EMBED_DESTROY, 0, 0, 0);

   UnuseAndDispose(ref);
}


/* pgGetExistingEmbed locates the embed_ref, if any, attached to character(s)
that contains user_refcon value (the original user value given in pgNewEmbedRef).  */

PG_PASCAL (embed_ref) pgGetExistingEmbed (pg_ref pg, long user_refcon)
{
   paige_rec_ptr              pg_rec;
   register style_info_ptr       styles;
   embed_ref                  result;
   long                    num_styles;

   pg_rec = (paige_rec_ptr) UseMemory(pg);
   result = MEM_NULL;
   num_styles = GetMemorySize(pg_rec->t_formats);
   
   for (styles = (style_info_ptr) UseMemory(pg_rec->t_formats); num_styles; ++styles, --num_styles)
      if (styles->embed_object && (styles->embed_refcon == user_refcon)) {
            result = styles->embed_object;
            break;
      }

   UnuseMemory(pg_rec->t_formats);
   UnuseMemory(pg);

   return   result;
}


/* pgFindNextEmbed searches all embed refs in the document, beginning with (and including)
*text_position. If AND_refcon is non-zero, its bits are AND'd with the style_refcon of each
one and compared to match_refcon. If AND_refcon is zero or the resulting comparison with
match_refcon is TRUE, the embed_ref is returned and *text_position is updated. If nonne
found, MEM_NULL is returned and *text_position will be end of text. */

PG_PASCAL (embed_ref) pgFindNextEmbed (pg_ref pg, size_t PG_FAR *text_position,
      long match_refcon, long AND_refcon)
{
   paige_rec_ptr              pg_rec;
   register style_info_ptr    stylebase;
   style_info_ptr             style;
   style_run_ptr              run;
   pg_embed_ptr               embed_ptr;
   embed_ref                  result;
   pg_short_t                 style_index;
   size_t                    first_run, num_styles;

   pg_rec = (paige_rec_ptr) UseMemory(pg);
   result = MEM_NULL;
   
   if (*text_position < 0)
         *text_position = pgFixOffset(pg_rec, *text_position);
   
   if (*text_position >= pg_rec->t_length) {
   
         UnuseMemory(pg);
         return   MEM_NULL;
   }

   num_styles = GetMemorySize(pg_rec->t_style_run) - 1;
   run = pgFindStyleRun(pg_rec, *text_position, &first_run);
   num_styles -= (long)first_run;
   stylebase = (style_info_ptr) UseMemory(pg_rec->t_formats);

   while (num_styles) {
      
      style_index = run->style_item;
      style = &stylebase[style_index];
      
      if (style->embed_object)
         if (!AND_refcon || ((style->embed_style_refcon & AND_refcon) == match_refcon)) {
            
            result = style->embed_object;
            embed_ptr = (pg_embed_ptr) UseMemory(result);
            embed_ptr->style_refcon = style->embed_style_refcon;
            embed_ptr->lowlevel_index = (long)style_index;
            UnuseMemory(result);

            break;
         }
      
      ++run;
      --num_styles;
   }
 
   if (text_position) {
   
      if (result == MEM_NULL)
         *text_position = pg_rec->t_length;
      else
         *text_position = run->offset;
   }

   UnuseMemory(pg_rec->t_formats);
   UnuseMemory(pg_rec->t_style_run);
   
   UnuseMemory(pg);

   return   result;
}



/* pgInsertEmbedRef inserts an embedded object into the text stream. This
objects must have been previously created with pgNewEmbedRef as above (or
created descretely if the creator knew what to do). The style_refcon is an
additional value that the app can set for this particular style. */

PG_PASCAL (pg_boolean) pgInsertEmbedRef (pg_ref pg, embed_ref ref, long position,
      short stylesheet_option, embed_callback callback, long callback_refcon,
      short draw_mode)
{
   paige_rec_ptr        pg_rec;
   style_info           new_style, mask;
   font_info            font, font_mask;
   pg_embed_ptr         embed_ptr;
   long                 the_type, type_and_flags, current_pos;
   pg_char              dummy_insert[2];
   pg_boolean           result;

   pg_rec = (paige_rec_ptr) UseMemory(pg);
   
   if (pg_rec->num_selects)
      pgDelete(pg, NULL, draw_none);
   
   embed_ptr = (pg_embed_ptr) UseMemory(ref);
   type_and_flags = embed_ptr->type;
   the_type = type_and_flags & EMBED_TYPE_MASK;

   UnuseMemory(ref);
   
   pgInitEmbedStyleInfo(pg_rec, position, ref, stylesheet_option, callback, callback_refcon,
         &new_style, &mask, &font, &font_mask, TRUE);

   if (type_and_flags & NO_FORCED_IDENTITY) {
         
         if (pgFindSimilarEmbed(pg, ref, &new_style)) {
            
            embed_ptr = (pg_embed_ptr) UseMemory((memory_ref)new_style.embed_object);
            embed_ptr->used_ctr += 1;
            UnuseMemory((memory_ref)new_style.embed_object);
         new_style.procs.insert_proc = embed_setup_insert_init;
            pgEmbedDispose(ref);
         }
   }

   pgSetFontInfo(pg, NULL, &font, &font_mask, draw_none);
   pgSetStyleInfo(pg, NULL, &new_style, &mask, draw_none);
   current_pos = pgCurrentInsertion(pg_rec);
   pgInvalTextMeasure(pg_rec,  current_pos, current_pos + 2);

   dummy_insert[0] = DUMMY_LEFT_EMBED;
   dummy_insert[1] = DUMMY_RIGHT_EMBED;

   result = (pgInsert(pg, dummy_insert, 2, CURRENT_POSITION, data_insert_mode, 0, draw_mode));
   
   if (type_and_flags & NO_FORCED_IDENTITY)
         pgSetupInsertStyle(pg_rec);

   UnuseMemory(pg);

   return   result;
}


/* pgSetEmbedRef should be used INSTEAD OF pgInsertEmbedRef when you want to
apply some embed info to bytes that already exist in the text. The selection
parameter indicates the byte range within pg to apply the embed_ref, or if
it is NULL then the current selection range is used. The callback_refcon can
be anything. Attributes will change the applied "style" in several ways (** for
this version, pass zero **).
NOTE, the applied reference must be for ONE LOGICAL CHARACTER ONLY. */

PG_PASCAL (void) pgSetEmbedRef (pg_ref pg, embed_ref ref, select_pair_ptr selection,
      short stylesheet_option, embed_callback callback, long callback_refcon,
      short draw_mode)
{
   paige_rec_ptr        pg_rec;
   select_pair          change_range;
   style_info           new_style, mask;
   long                 new_type;

   pg_rec = (paige_rec_ptr) UseMemory(pg);
   
   if (selection)
      change_range = *selection;
   else
      pgGetSelection(pg, &change_range.begin, &change_range.end);

   new_type = pgInitEmbedStyleInfo(pg_rec, change_range.begin, ref, stylesheet_option,
         callback, callback_refcon, &new_style, &mask, NULL, NULL, FALSE);
   
   UnuseMemory(pg);
   
   if (new_style.char_bytes = (short)(change_range.end - change_range.begin))
      --new_style.char_bytes;

   new_style.class_bits |= EMBED_APPLIED_BIT;

   pgSetStyleInfo(pg, &change_range, &new_style, &mask, draw_mode);
}

/* pgFindSimilarEmbed walks through all existing embeds and returns one that matches the type,
data contents and width, height values.  If a match is found that embed_ref is returned (and
if embed_style is non-zero it is returned as well). */

PG_PASCAL (embed_ref) pgFindSimilarEmbed (pg_ref pg, embed_ref ref, style_info_ptr embed_style)
{
    paige_rec_ptr             pg_rec;
    pg_embed_ptr           embed_ptr, compare_to;
    register style_info_ptr      styles;
    long                num_styles, the_type, compare_type;
    pg_boolean             same_type, same_dimensions, same_data;
   embed_ref               result;
 
   pg_rec = (paige_rec_ptr) UseMemory(pg);
   num_styles = GetMemorySize(pg_rec->t_formats);
   result = MEM_NULL;
   compare_to = (pg_embed_ptr) UseMemory(ref);
   compare_type = compare_to->type & EMBED_TYPE_MASK;

   for (styles = (style_info_ptr) UseMemory(pg_rec->t_formats); num_styles; ++styles, --num_styles) {
      
      if (styles->embed_object) {
         
         embed_ptr = (pg_embed_ptr) UseMemory((memory_ref)styles->embed_object);
         
         the_type = embed_ptr->type & EMBED_TYPE_MASK;
         same_type = (pg_boolean)((the_type == compare_type)
                  && (embed_ptr->user_refcon == compare_to->user_refcon)
                  && (embed_ptr->modifier == compare_to->modifier
                  && styles->styles[super_impose_var] == embed_style->styles[super_impose_var]));
         
         if (same_type) {
            
            if (the_type == embed_alternate_char || the_type == embed_dynamic_string)
               same_dimensions = TRUE;
            else
               same_dimensions = (pg_boolean)((embed_ptr->width == compare_to->width)
                                       && (embed_ptr->height == compare_to->height));
            
            same_data = compare_embed_data(compare_to, embed_ptr);
         }
         else
            same_data = same_dimensions = FALSE;
         
         UnuseMemory((memory_ref)styles->embed_object);
         
         if (same_data && same_type && same_dimensions) {
            
            result = (embed_ref)styles->embed_object;
            
            if (embed_style)
               *embed_style = *styles;
            
            break;
         }
      }
   }
   
   UnuseMemory(pg_rec->t_formats);
   UnuseMemory(ref);

   return   result;
}


/* pgNumEmbeds returns the numger of embedded references in selection range
(or the current selection point or range if selection is NULL). */

PG_PASCAL (long) pgNumEmbeds (pg_ref pg, select_pair_ptr selection)
{
   paige_rec_ptr        pg_rec;
   register style_info_ptr style_base;
   select_pair          use_to_init;
   select_pair_ptr         selections;
   style_run_ptr        run;
   memory_ref           select_ref;
   long              result, num_selects;
   
   pg_rec = (paige_rec_ptr) UseMemory(pg);
   result = 0;
   
   if (selection)
      use_to_init = *selection;
   else
   if (pg_rec->num_selects)
         pgGetSelection(pg, &use_to_init.begin, &use_to_init.end);
   else {
      
      use_to_init.begin = 0;
      use_to_init.end = pg_rec->t_length;
   }

   if (select_ref = pgSetupOffsetRun(pg_rec, &use_to_init, FALSE, FALSE)) {
      
      num_selects = GetMemorySize(select_ref);
      selections = (select_pair_ptr) UseMemory(select_ref);
      style_base = (style_info_ptr) UseMemory(pg_rec->t_formats);

      while (num_selects) {
         
         run = pgFindStyleRun(pg_rec, selections->begin, NULL);
         
         while (run->offset < selections->end) {
            
            if (style_base[run->style_item].embed_object)
               ++result;
            
            ++run;
         }

         UnuseMemory(pg_rec->t_style_run);
         
         ++selections;
         --num_selects;
      }
      
      UnuseMemory(pg_rec->t_formats);
      UnuseAndDispose(select_ref);
   }
   else {
      style_info           single_style, mask;
      
      pgGetStyleInfo(pg, NULL, FALSE, &single_style, &mask);
      
      if (single_style.embed_object)
         ++result;
   }

   UnuseMemory(pg);
   
   return   result;
}


/* pgGetIndEmbed returns the nth embedded reference, 1-based, found within
the selection (or if selection is NULL, the current selection). If none are
found, MEM_NULL is returned. The text_position and associated_style params are optional: if
text_position is non-NULL, the first char position of the embed_ref is returned. If
associated_style is non-NULL, a copy of the associated style is returned. */

PG_PASCAL (embed_ref) pgGetIndEmbed (pg_ref pg, select_pair_ptr selection, size_t index,
      size_t PG_FAR *text_position, style_info_ptr associated_style)
{
   paige_rec_ptr        	pg_rec;
   register style_info_ptr  style_base;
   select_pair_ptr          selections;
   style_run_ptr        	run;
   memory_ref           	select_ref;
   embed_ref            	result;
   size_t              		num_selects, index_ctr;
   
   if (!index)
      return   MEM_NULL;

   pg_rec = (paige_rec_ptr) UseMemory(pg);
   result = MEM_NULL;
   index_ctr = 0;

   if (select_ref = pgSetupOffsetRun(pg_rec, selection, FALSE, FALSE)) {
      
      num_selects = GetMemorySize(select_ref);
      selections = (select_pair_ptr) UseMemory(select_ref);
      style_base = (style_info_ptr) UseMemory(pg_rec->t_formats);

      while (num_selects) {
         
         run = pgFindStyleRun(pg_rec, selections->begin, NULL);
         
         while (run->offset < selections->end) {
            
            if (style_base[run->style_item].embed_object) {
               
               ++index_ctr;
               
               if (index_ctr == index) {
                  
                  result = style_base[run->style_item].embed_object;
                  
                  if (text_position)
                     *text_position = run->offset;
                  if (associated_style)
                     pgBlockMove(&style_base[run->style_item], associated_style, sizeof(style_info));

                  break;
               }
            }
            
            ++run;
         }

         UnuseMemory(pg_rec->t_style_run);
         
         if (result)
            break;

         ++selections;
         --num_selects;
      }

      UnuseMemory(pg_rec->t_formats);
      UnuseAndDispose(select_ref);
   }
   else {
      style_info           single_style, mask;
      
      if (index == 1) {
      
         pgGetStyleInfo(pg, NULL, FALSE, &single_style, &mask);
         
         if (result = single_style.embed_object) {

            if (text_position)
               *text_position = pgCurrentInsertion(pg_rec);
            if (associated_style)
               pgBlockMove(&single_style, associated_style, sizeof(style_info));
         }
      }
   }

   UnuseMemory(pg);
   
   return   result;
}


/* pgPtInEmbed returns the embed_ref, if any, that contains point. If text_position is non-NULL,
the first textoffset is returned. If associated_style is non-NULL, the associated style_info
is returned. If do_callback is TRUE and a valid embed_ref
is located containing point, the callback function is given EMBED_CURSOR. */

PG_PASCAL (embed_ref) pgPtInEmbed (pg_ref pg, co_ordinate_ptr point, long PG_FAR *text_offset,
      style_info_ptr associated_style, pg_boolean do_callback)
{
   paige_rec_ptr        pg_rec;
   style_info           the_style;
   select_pair          the_range;
   pg_embed_ptr         embed_ptr;
   rectangle_ptr        bounds_ptr;
   rectangle            bounds;
   embed_callback       callback;
   embed_ref            ref, result;
   long              embed_type;

   result = MEM_NULL;

   pgPtToStyleInfo(pg, point, NO_BYTE_ALIGN | NO_HALFCHARS, &the_style, &the_range);
   
   if (ref = (embed_ref)the_style.embed_object) {
      
      result = ref;
      
      if (text_offset)
         *text_offset = the_range.begin;
      if (associated_style)
         pgBlockMove(&the_style, associated_style, sizeof(style_info));

      pg_rec = (paige_rec_ptr) UseMemory(pg);
      embed_ptr = (pg_embed_ptr) UseMemory(ref);
      
      get_visual_frame(pg_rec, embed_ptr, &the_range, &bounds);

      embed_type = embed_ptr->type & EMBED_TYPE_MASK;

      if (pgPtInRect(point, &bounds)) {
      
         if (do_callback) {
            
            callback = (embed_callback)the_style.embed_entry;
            bounds_ptr = &bounds;
            embed_ptr->style = &the_style;
            callback(pg_rec, embed_ptr, embed_type, EMBED_CURSOR,
                  the_style.embed_style_refcon, 
                  point, bounds_ptr);
            embed_ptr->style = NULL;
         }
      }
      else
         result = MEM_NULL;

      UnuseMemory(pg);
      UnuseMemory(ref);
   }

   return   result;
}


/* pgEmbedStyleToIndex returns the index number (1-based) of the embed_ref attached to
embed_style. If none, zero is returned. */

PG_PASCAL (long) pgEmbedStyleToIndex (pg_ref pg, style_info_ptr embed_style)
{
   paige_rec_ptr           pg_rec;
   register style_info_ptr    stylebase;
   style_info_ptr          style;
   style_run_ptr           run;
   long                 num_runs, index, embed_id;

   index = 0;

   if (embed_style->embed_object) {
      
      pg_rec = (paige_rec_ptr) UseMemory(pg);
      stylebase = (style_info_ptr) UseMemory(pg_rec->t_formats);
      run = (style_run_ptr) UseMemory(pg_rec->t_style_run);
      num_runs = GetMemorySize(pg_rec->t_style_run) - 1;
      embed_id = embed_style->embed_id;

      while (num_runs) {
         
         style = &stylebase[run->style_item];
         
         if (style->embed_object) {

            ++index;
   
            if (style->embed_id == embed_id)
               break;
         }

         ++run;
         --num_runs;
      }
      
      UnuseMemory(pg_rec->t_style_run);
      UnuseMemory(pg_rec->t_formats);
      UnuseMemory(pg);
   }
   
   return   index;
}



/* pgGetEmbedJustClicked is typically called after pgDragSelect returns a
non-zero number which, if track_refcon in pgDragSelect was originally zero,
implies that an embedded item was just clicked.  This function will find that
embed_ref and return it.  If not found for some reason, MEM_NULL is the result. */

PG_PASCAL (embed_ref) pgGetEmbedJustClicked (pg_ref pg, long drag_select_result)
{
   paige_rec_ptr              pg_rec;
   register style_info_ptr       styles;
   embed_ref                  result;
   long                    num_styles;
   
   pg_rec = (paige_rec_ptr) UseMemory(pg);
   result = MEM_NULL;

   num_styles = GetMemorySize(pg_rec->t_formats);
   for (styles = (style_info_ptr) UseMemory(pg_rec->t_formats); num_styles; ++styles, --num_styles)
      if (styles->embed_id == drag_select_result) {
         
         result = styles->embed_object;
         break;
      }
   
   UnuseMemory(pg_rec->t_formats);
   UnuseMemory(pg);
   
   return      result;
}


/* pgGetEmbedBounds returns the bounding box for embed_ref represented by index. The bounds
result is scaled (if scaling enabled) but not scrolled -- scrolled position, if any, is
given in screen_extra. The amount the rectangle is offset from the basline of the text is
given in vertical_pos. Note, any one of these pointers can be NULL. The function returns the
text position of the embed_ref. */

PG_PASCAL (long) pgGetEmbedBounds (pg_ref pg, long index, select_pair_ptr index_range,
      rectangle_ptr bounds, short PG_FAR *vertical_pos, co_ordinate_ptr screen_extra)
{
   paige_rec_ptr           pg_rec;
   pg_embed_ptr            embed_ptr;
   style_info              style;
   select_pair             style_range;
   embed_ref               ref;
   long                 result;

   result = -1;

   if (ref = pgGetIndEmbed(pg, index_range, index, &style_range.begin, &style)) {

      pg_rec = (paige_rec_ptr) UseMemory(pg);
      embed_ptr = (pg_embed_ptr) UseMemory(ref);
      result = style_range.begin;
      style_range.end = style_range.begin + (style.char_bytes + 1);
      
      if (bounds)
         get_visual_frame(pg_rec, embed_ptr, &style_range, bounds);
      if (vertical_pos)
         *vertical_pos = (short)embed_ptr->descent;
      if (screen_extra) {
         
         *screen_extra = pg_rec->scroll_pos;
         pgNegatePt(screen_extra);
         pgAddPt(&pg_rec->port.origin, screen_extra);
      }

      UnuseMemory(ref);
      UnuseMemory(pg);
   }
   
   return   result;
}


/* pgSetEmbedBounds changes the physical bounds of the embed_ref defined by index against
index_range. If bounds is non-NULL the embed_ref gets set to that width and height.
If vertical_pos is non-null the descent is set to that value. */

PG_PASCAL (void) pgSetEmbedBounds (pg_ref pg, long index, select_pair_ptr index_range,
      rectangle_ptr bounds, short PG_FAR *vertical_pos, short draw_mode)
{
   paige_rec_ptr           pg_rec;
   pg_embed_ptr            embed_ptr;
   style_info			   associated_style;
   size_t                    position, new_width, new_height, new_descent;
   embed_ref               ref;

   if (ref = pgGetIndEmbed(pg, index_range, index, &position, &associated_style)) {

     pg_rec = (paige_rec_ptr) UseMemory(pg);
     embed_ptr = (pg_embed_ptr) UseMemory(ref);
     
     if (bounds) {
     
        new_width = bounds->bot_right.h - bounds->top_left.h;
        new_height = bounds->bot_right.v - bounds->top_left.v;
     }
     else {
     
        new_width = embed_ptr->width;
        new_height = embed_ptr->height;
     }
     
     if (vertical_pos)
        new_descent = *vertical_pos;
     else
        new_descent = embed_ptr->descent;
	 
	 if (embed_ptr->type & ALIGN_CBASELINE_FLAG)
	 	new_descent = embed_ptr->height / 2;
	 else
	 if (embed_ptr->type & ALIGN_BBASELINE_FLAG)
	 	new_descent = embed_ptr->height - (long)associated_style.ascent;

     if (embed_ptr->width != new_width || embed_ptr->height != new_height
         || embed_ptr->descent != new_descent) {
         embed_ptr->width = new_width;
         embed_ptr->height = new_height;
         embed_ptr->descent = new_descent;
         
         pgInvalEmbedRef (pg, position, embed_ptr, draw_mode);
     }

     UnuseMemory(ref);
     UnuseMemory(pg);
   }
}

/* pgInvalEmbedRef invalidates the embed_ref, calculating new data if necessary. */

PG_PASCAL (void) pgInvalEmbedRef (pg_ref pg, long position, pg_embed_ptr embed_ptr, short draw_mode)
{
	 paige_rec_ptr	   pg_rec;
     style_run_ptr     run;
     style_info_ptr    style;
     
     pg_rec = (paige_rec_ptr) UseMemory(pg);
     pgInvalSelect(pg, position, position + 2);
     
     run = (style_run_ptr) UseMemory(pg_rec->t_style_run);
     
     while (run->offset <= position) {
        
        if (run->offset == position) {
           
           style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, (long)run->style_item, 0, TRUE);
           style->descent = (short)embed_ptr->descent;
           style->ascent = (short)(embed_ptr->height - embed_ptr->descent);
           UnuseMemory(pg_rec->t_formats);
           break;
        }
        
        ++run;
     }

     UnuseMemory(pg_rec->t_style_run);

     if (draw_mode) {
        short		use_draw_mode;
 
        if ((use_draw_mode = draw_mode) == best_way)
           use_draw_mode = bits_copy;

        pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, use_draw_mode, TRUE);
     }

     UnuseMemory(pg);
}


/* pgDrawAlternateText draws the alternate char-swap text in item to pin to
top-left of draw_bounds. If optional_pointsize is non-zero the size is
temporarily set to that point size. */

PG_PASCAL (void) pgDrawAlternateText (graf_device_ptr device, pg_embed_ptr item,
      rectangle_ptr draw_bounds, short optional_pointsize)
{
   pg_char_ptr       the_chars;
   pg_short_t        the_size;
   
   if (item->data)
      the_chars = (pg_char_ptr) UseMemory((memory_ref)item->data);
   else
      the_chars = (pg_char_ptr)&item->uu.alt_data;

   the_size = (pg_short_t)pgCStrLength((pg_c_string_ptr)the_chars);

   if (the_size)
      pgTextOut(device, &draw_bounds->top_left, the_chars, the_size, optional_pointsize);

   if (item->data)
      UnuseMemory((memory_ref)item->data);
}



/* pgSetEmbedBorders adjusts the offsets due to borders and sets the border information */

PG_PASCAL (void) pgSetEmbedBorders (embed_ref ref, long border_info, long color, long shading,
				pg_boolean transparent)
{
   pg_embed_ptr   embed;
   
   embed = (pg_embed_ptr) UseMemory(ref);
   embed->border_info = border_info;
   embed->border_color = color;
   embed->shading = shading;
   embed->uu.pict_data.pict_transparent = transparent;
   UnuseMemory(ref);
}
   

/* pgDrawEmbedBorders is also available by user of embededs.  It draws bounding boxes */
/* REVISED FOR VERSION 3.01, SEE "Tables and Borders" */

PG_PASCAL (void) pgDrawEmbedBorders(paige_rec_ptr pg, pg_embed_ptr embed, rectangle_ptr draw_bounds)
{
	if (embed->border_info)
		pgDrawGeneralBorders(pg, embed->border_info, embed->border_color, embed->shading,
					draw_bounds, TRUE);
}


/* pgDefaultEmbedCallback is the default callback function for embed_refs for
which application specified NULL callback. Or, if application does not know
how to handle the command it should call this directly. */

PG_PASCAL (size_t) pgDefaultEmbedCallback (paige_rec_ptr pg, pg_embed_ptr embed_ptr,
      long embed_type, short command, long user_refcon,
      void *param1, void *param2)
{
   pg_embed_ptr     use_embed_ptr;
   size_t           result = 0;
   
   if (!embed_ptr)
         return   0;
   
   if (embed_ptr->embed_represent && (command != EMBED_DESTROY))
      use_embed_ptr = (pg_embed_ptr) UseMemory(embed_ptr->embed_represent);
   else
         use_embed_ptr = embed_ptr;

   switch (command) {

      case EMBED_DRAW:
         standard_embed_draw(pg, use_embed_ptr, (rectangle_ptr)param1, (draw_points_ptr)param2,
               (pg_boolean)(pg->globals->offscreen_enable == OFFSCREEN_SUCCESSFUL));
         break;

      case EMBED_MOUSEDOWN:
         result = standard_embed_click(pg, use_embed_ptr, embed_type,
               (pg_embed_click_ptr) param1);
         break;
         
      case EMBED_MOUSEMOVE:
      case EMBED_MOUSEUP:
      case EMBED_DOUBLECLICK:
         result = use_embed_ptr->style->embed_id;
         break;
     
     case EMBED_COPY:
       standard_embed_copy(use_embed_ptr, embed_type);
       break;

      case EMBED_DESTROY:
         standard_embed_dispose(use_embed_ptr, embed_type);
         
         if (use_embed_ptr->embed_represent) {
             pg_embed_ptr     represent_ptr;
             
             represent_ptr = (pg_embed_ptr) UseMemory(use_embed_ptr->embed_represent);
             standard_embed_dispose(represent_ptr, represent_ptr->type & EMBED_TYPE_MASK);
             UnuseAndDispose(use_embed_ptr->embed_represent);
             use_embed_ptr->embed_represent = MEM_NULL;
         }

         break;

      case EMBED_SWAP:
         if ((result = pgCStrLength((pg_c_string_ptr)param1)) > (size_t)param2)
            result = (size_t)param2;
         break;
      
      case EMBED_INIT:
         break;
      
      case EMBED_CURSOR:
         break;
      
      case EMBED_ACTIVATE:
         break;
      
      case EMBED_DEACTIVATE:
         break;
      
      case EMBED_WRITE_DATA:
         break;
      
      case EMBED_READ_DATA:
         break;
   }

   if (command != EMBED_DESTROY)
         if (embed_ptr->embed_represent)
         UnuseMemory(embed_ptr->embed_represent);

   return   result;
}



/* pgEmbedReadHandler is the standard handler function for reading back embed_refs
from a PAIGE file. */

PG_PASCAL (pg_boolean) pgEmbedReadHandler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
      long PG_FAR *element_info, void PG_FAR *aux_data, size_t PG_FAR *unpacked_size)
{
   pg_embed_ptr					embed_ptr;
   pgm_globals_ptr        		mem_globals;
   register style_info_ptr		styles;
   embed_ref       				ref;
   pack_walk  					walker;
   embed_callback  				callback;
   long PG_FAR 					*previous_embed;
   long                       	num_styles, ref_id, the_type;
   size_t                       called_ctr;
   long                       	file_version, old_current_id;

   file_version = pg->version;
   previous_embed = (long PG_FAR *)aux_data;

   pgSetupPacker(&walker, key_data, 0);
   mem_globals = pg->globals->mem_globals;
   old_current_id = mem_globals->current_id;

   if (key == url_image_key) {
      
      if (!pg->url_list_ref)
            pg->url_list_ref = MemoryAllocID(mem_globals, sizeof(memory_ref), 0, 32, pg->mem_id);
#if defined(FRONTIER_TESTS)
            pg_trace_handle_watch(pg->url_list_ref, "pg.url_list");
#endif
      
      unpack_url(pg, &walker);
      UnuseMemory(walker.data_ref);
      return   TRUE;
   }
   
   ref = pgUnpackEmbedRef(pg, &walker, &ref_id);
   embed_ptr = (pg_embed_ptr) UseMemory(ref);
   the_type = embed_ptr->type & EMBED_TYPE_MASK;

   if (!(callback = (embed_callback)pg->globals->embed_callback_proc))
      callback = pgDefaultEmbedCallback;

// Now replace all earlier occurrences with this embed_ref in the style_info array:

   num_styles = GetMemorySize(pg->t_formats);
   
   if (ref_id == -1) {
         pg_embed_ptr      previous_ptr;
         
         if (previous_embed) {
            
            if (*previous_embed) {
            
               previous_ptr = (pg_embed_ptr) UseMemory((memory_ref)*previous_embed);
               previous_ptr->embed_represent = ref;
               UnuseMemory((memory_ref)*previous_embed);
            }
         }
   }
   else {
   
      called_ctr = 0;

      for (styles = (style_info_ptr) UseMemory(pg->t_formats); num_styles; ++styles, --num_styles)
         if (ref_id == (long)styles->embed_object && !(styles->class_bits & EMBED_READ_BIT)) {
         
         ++embed_ptr->used_ctr;
         styles->embed_object = ref;
         styles->class_bits |= EMBED_READ_BIT;
         styles->procs.insert_proc = embed_setup_insert;
      
         if (!(styles->class_bits & EMBED_APPLIED_BIT) && the_type != embed_alternate_char
            && the_type != embed_dynamic_string
            && (styles->char_bytes == 1))
            styles->procs.char_info = embed_char_info;
         else
            styles->procs.char_info = pg->globals->def_style.procs.char_info;

         styles->procs.init = pgEmbedStyleInit;
         styles->procs.draw = embed_draw;
         styles->procs.measure = measure_embed;
         styles->procs.duplicate = embed_copy;
         styles->procs.delete_style = embed_delete;
         styles->procs.track_ctl = track_embed_ctl;
         styles->embed_entry = (void *)(uintptr_t)callback;

         embed_ptr->style = styles;
         embed_ptr->style_refcon = styles->embed_style_refcon;
         callback(pg, embed_ptr, the_type, EMBED_INIT, embed_ptr->style_refcon, (void*)called_ctr, (void*)styles);
         embed_ptr->style = NULL;

         ++called_ctr;
      }

       UnuseMemory(pg->t_formats);
       
       if (pg->flags2 & HAS_PG_FRAMES_BIT) {
       		memory_ref	PG_FAR		*framerefs;
       		pg_frame_ptr			frame;
			long					num_frames;
			
			num_frames = GetMemorySize(pg->exclusions);
			framerefs = (memory_ref *) UseMemory(pg->exclusions);
			
			while (num_frames) {
				
				frame = (pg_frame_ptr) UseMemory(*framerefs);
				
				if (ref_id == (long)frame->data)
					if (!(frame->flags & FRAME_READ_BIT)) {
					
					frame->data = (generic_var)ref;
					frame->flags |= FRAME_READ_BIT;
				}
				
				UnuseMemory(*framerefs);
				
				++framerefs;
				--num_frames;
			}
			
			UnuseMemory(pg->exclusions);
       }
   }

   embed_ptr->version = CURRENT_EMBED_VERSION;

   UnuseMemory(walker.data_ref);
   UnuseMemory(ref);

   if (pg->globals->app_init_proc)
      pg->globals->app_init_proc(pg, ref, EMBED_REF_INIT);

   mem_globals->current_id = old_current_id;
   
   if (previous_embed)
         *previous_embed = (long)ref;

   return      TRUE;
}


/* pgUnpackEmbedRef unpacks the embed_ref returning the new ref. */

PG_PASCAL (memory_ref) pgUnpackEmbedRef (paige_rec_ptr pg, pack_walk_ptr walker, long PG_FAR *ref_id)
{
   volatile pg_embed_ptr		embed_ptr;
   volatile pgm_globals_ptr     mem_globals;
   volatile embed_ref       	ref = MEM_NULL;
   embed_callback  				callback;
   pg_bits8_ptr    				custom_data;
   long                       	the_type, input_byte_size;
   long                       	custom_size;
   long                       	version, file_version;
   
   file_version = pg->version;
   mem_globals = pg->globals->mem_globals;
   
   if (!(callback = (embed_callback)pg->globals->embed_callback_proc))
      callback = pgDefaultEmbedCallback;

   *ref_id = pgUnpackNum(walker);      // Was original memory_ref value
   
   if (file_version > KEY_REVISION6)
      version = pgUnpackNum(walker);     // Get the version so we can be compatible in the future
   else version = 1;
   
   the_type = pgUnpackNum(walker);
   
   PG_TRY(mem_globals) {

	   ref = pgNewEmbedRef(mem_globals, the_type, NULL, 0, 0, 0, 0, FALSE);

	// Bug fix 8/16/95, any data struct within embed_ref must have different ID than pg_ref:

	   mem_globals->current_id = GetMemoryRefID(ref);
	   embed_ptr = (volatile pg_embed_ptr) UseMemory(ref);
	   embed_ptr->version = (short)version;      //Reestablish the original version

	   if (the_type & KEPT_AROUND)
	      embed_ptr->used_ctr = 1;

	   the_type &= EMBED_TYPE_MASK;
	   
	   embed_ptr->width = pgUnpackNum(walker);
	   embed_ptr->height = pgUnpackNum(walker);
	   
	   if (embed_ptr->width < 0)
	         embed_ptr->width = -embed_ptr->width;
	   if (embed_ptr->height < 0)
	         embed_ptr->height = -embed_ptr->height;

	   embed_ptr->descent = pgUnpackNum(walker);
	   embed_ptr->draw_flags = pgUnpackNum(walker);
	   embed_ptr->modifier = pgUnpackNum(walker);
	   embed_ptr->user_refcon = pgUnpackNum(walker);
	   embed_ptr->data = (void PG_FAR *)pgUnpackNum(walker);

	   if (file_version >= KEY_REVISION16)
	         embed_ptr->rtf_text_data = (memory_ref)pgUnpackNum(walker);

	   embed_ptr->top_extra = (short)pgUnpackNum(walker);
	   embed_ptr->bot_extra = (short)pgUnpackNum(walker);
	   embed_ptr->left_extra = (short)pgUnpackNum(walker);
	   embed_ptr->right_extra = (short)pgUnpackNum(walker);
	   embed_ptr->alignment_flags = pgUnpackNum(walker);

	   unpack_altsize_text(pg, walker, embed_ptr->uu.alt_data, ALT_SIZE);
	   
	   if (file_version >= KEY_REVISION30) {
	   	   
	   	   embed_ptr->border_info = pgUnpackNum(walker);
	   	   embed_ptr->border_color = pgUnpackNum(walker);
	   	   embed_ptr->shading = pgUnpackNum(walker);
	   }
	   else {
			old_pg_border		old_border;

	      pgUnpackPtrBytes(walker, (pg_bits8_ptr)&old_border);
	      convert_old_borders(embed_ptr, &old_border);
	   }
	   
	   if (embed_ptr->data) {
	      
	      switch (the_type) {

	         case embed_control:
	            break;
	            
	         case embed_bitmap:
	            unpack_pic_embed(walker, embed_ptr);
	            break;

	         case embed_polygon:
	            embed_ptr->data = (void PG_FAR *)pgUnpackPoly(walker);
	            break;
	         
	         case embed_mac_pict:
	            unpack_pic_embed(walker, embed_ptr);
	        	embed_ptr->data = (void PG_FAR *)unpack_graphics(mem_globals, walker, embed_mac_pict);
	         	break;
	      
	      case embed_meta_file:
	         unpack_pic_embed(walker, embed_ptr);
	         embed_ptr->data = (void PG_FAR *)unpack_graphics(mem_globals, walker, embed_meta_file);
	         break;
	            
	         case embed_qt_component:
	            break;

	         case embed_mac_vm_pict:
	         case embed_os2_meta_file:
	         case embed_dibitmap:
	         case embed_wbitmap:
	         case embed_gif_pict:
	         case embed_jpeg_pict:
	            unpack_pic_embed(walker, embed_ptr);
	            pgGetUnpackedPtr(walker, &input_byte_size);
	            embed_ptr->data = (void PG_FAR *)MemoryAlloc(mem_globals, 1, input_byte_size, 0);
	            pgUnpackBytes(walker, (memory_ref)embed_ptr->data);
	            break;
	         case embed_ole: // PDA:
	            embed_ptr->data = (void*)pgUnpackNum(walker);
	            break;
	         case embed_alternate_char:
	         case embed_dynamic_string:
	         case embed_user_data:
	         case embed_bookmark_start:
	         case embed_bookmark_end:
	         case embed_unsupported_object:
	            pgGetUnpackedPtr(walker, &input_byte_size);
	            embed_ptr->data = (void PG_FAR *)MemoryAlloc(mem_globals, 1, input_byte_size, 0);
	            pgUnpackBytes(walker, (memory_ref)embed_ptr->data);
	         
	         if (embed_ptr->rtf_text_data) {
	            
	            pgGetUnpackedPtr(walker, &input_byte_size);
	               embed_ptr->rtf_text_data = MemoryAlloc(mem_globals, sizeof(pg_char), input_byte_size / sizeof(pg_char), 0);
	               pgUnpackBytes(walker, embed_ptr->rtf_text_data);
	         }

	            break;

	         case embed_qt_movie:
	            unpack_pic_embed(walker, embed_ptr);
	#ifdef MAC_PLATFORM
	#endif
	            break;

	         case embed_qt_flat_movie:
	            unpack_pic_embed(walker, embed_ptr);
	            break;

	         case embed_time:
	           case embed_date:
	            embed_ptr->uu.date_info.time = pgUnpackNum(walker);
	            embed_ptr->uu.date_info.time_type = (short)pgUnpackNum(walker);
	            embed_ptr->uu.date_info.format = (short)pgUnpackNum(walker);
	            break;

	           case embed_cur_page_num:
	           case embed_cur_sect_num:
	            break;

	         case embed_horiz_rule:
	            embed_ptr->uu.line_info.line_size = (short)pgUnpackNum(walker);
	            embed_ptr->uu.line_info.line_shading = (short)pgUnpackNum(walker);
	            embed_ptr->uu.line_info.line_alignment = (short)pgUnpackNum(walker);
	            embed_ptr->uu.line_info.line_width_type = (short)pgUnpackNum(walker);
	            embed_ptr->uu.line_info.line_width = pgUnpackNum(walker);
	            break;
	         
	         case embed_url_image:
	         {
	            long     url_index;
	            
	            url_index = pgUnpackNum(walker);
	            
	            if (pg->url_list_ref && url_index > 0) {
	               memory_ref        url_ref;
	               pg_url_image_ptr  image_ptr;
	               
	               GetMemoryRecord(pg->url_list_ref, url_index - 1, &url_ref);
	               image_ptr = (pg_url_image_ptr) UseMemory(url_ref);
	               image_ptr->used_ctr += 1;
	               UnuseMemory(url_ref);
	               embed_ptr->data = (void PG_FAR *)url_ref;
	            }
	         }
	         break;
	      }
	   }

	   custom_data = pgGetUnpackedPtr(walker, &custom_size);
	   callback(pg, embed_ptr, the_type, EMBED_READ_DATA, embed_ptr->user_refcon,
	         (void*)custom_data, (void*)custom_size);

	   UnuseMemory(ref);
   }
   PG_CATCH {
   	   
   	   if (ref)
   	   	   UnuseFailedMemory(ref);
   	   
   	   pgFailure(mem_globals, NO_MEMORY_ERR, 0);
   }
   PG_ENDTRY;

   return	ref;
}


/* pgInitEmbedProcs sets up the I/O handlers for embed_refs. If init_proc is
non-null it will get called for each embed_ref that is successfully read from
the file. The callback parameter becomes the embed_ref user callback function. */

PG_PASCAL (void) pgInitEmbedProcs (pg_globals_ptr globals, embed_callback callback,
      app_init_read init_proc)
{
   pgSetHandler(globals, embedded_item_key, pgEmbedReadHandler, NULL, NULL, NULL);
   pgSetHandler(globals, url_image_key, pgEmbedReadHandler, NULL, NULL, NULL);

   globals->embed_callback_proc = (void *) (long)callback;
   globals->app_init_proc = init_proc;
}


/* pgSaveAllEmbedRefs saves all embed_refs found in the style_info records. Since
an embed_ref can be "shared" by multiple style runs, each unique embed_ref is
saved only once (which is also why we need a separate function to save these
refs descretely).  */

PG_PASCAL (pg_error) pgSaveAllEmbedRefs (pg_ref pg, file_io_proc io_proc, file_io_proc data_io_proc,
      size_t PG_FAR *file_position, file_ref filemap)
{
   paige_rec_ptr     pg_rec;
   embed_ref         ref;
   style_info_ptr    styles;
   memory_ref        key_data, custom_ref, represent_ref;
   memory_ref      	 url_list_ref;
   pack_walk         walker;
   embed_callback    callback;
   pg_embed_ptr      embed_ptr;
   long              fake_id, num_styles, index;
   file_io_proc      data_proc;
   pg_error          result;
   
   pg_rec = (paige_rec_ptr) UseMemory(pg);
   key_data = MemoryAlloc(pg_rec->globals->mem_globals, 1, 0, 32);
   custom_ref = MemoryAlloc(pg_rec->globals->mem_globals, 1, 0, 32);
   url_list_ref = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(memory_ref), 0, 32);

// First, build any URL type(s) within the list:

   num_styles = GetMemorySize(pg_rec->t_formats);
   styles = (style_info_ptr) UseMemory(pg_rec->t_formats);

   result = NO_ERROR;

   if (!(data_proc = data_io_proc))
      data_proc = io_proc;

   for (index = 0; index < num_styles; ++index) {
      
      if ((ref = styles[index].embed_object) != MEM_NULL) {

         embed_ptr = (pg_embed_ptr) UseMemory(ref);
         
         if ((embed_ptr->type & EMBED_TYPE_MASK) == embed_url_image)
			if (!find_url_ref(url_list_ref, (memory_ref)embed_ptr->data)) {
				pg_url_image_ptr     image;
				memory_ref           PG_FAR *url_list;
				memory_ref           url_ref;

				url_ref = (memory_ref)embed_ptr->data;
				url_list = (memory_ref *) AppendMemory(url_list_ref, 1, FALSE);
				*url_list = url_ref;
				UnuseMemory(url_list_ref);

				image = (pg_url_image_ptr) UseMemory(url_ref);
				callback = (embed_callback)styles[index].embed_entry;

				if (!image->image_data && !image->loader_result)
					callback(pg_rec, embed_ptr, embed_url_image, EMBED_LOAD_IMAGE, styles[index].embed_style_refcon,
				     	(void*)image, (void*)0);

				UnuseMemory(url_ref);

				SetMemorySize(key_data, 0);
				pgSetupPacker(&walker, key_data, 0);
				pack_url(&walker, url_ref);

				pgFinishPack(&walker);

				result = pgWriteKeyData(pg, url_image_key, UseMemory(key_data), walker.transfered,
				     0, io_proc, data_proc, file_position, filemap);
				UnuseMemory(key_data);

         }
 
         UnuseMemory(ref);
      }
   }

   for (index = 0; index < num_styles; ++index) {
      
      if (ref = styles[index].embed_object)
         if (!have_saved_ref(styles, (short)index)) {
         
         fake_id = 0;
 
         for (;;) {
         
            SetMemorySize(key_data, 0);
            pgSetupPacker(&walker, key_data, 0);
            pgPackEmbedRef(&walker, url_list_ref, ref, fake_id);
            callback = (embed_callback)styles[index].embed_entry;
            custom_data_callback(pg_rec, &walker, (pg_embed_ptr) UseMemory(ref), callback, custom_ref);
            UnuseMemory(ref);
            pgFinishPack(&walker);

            result = pgWriteKeyData(pg, embedded_item_key, UseMemory(key_data), walker.transfered,
                  0, io_proc, data_proc, file_position, filemap);
            UnuseMemory(key_data);
            
            if (result)
               break;

            embed_ptr = (pg_embed_ptr) UseMemory(ref);
            represent_ref = embed_ptr->embed_represent;
            UnuseMemory(ref);
            
            if (!represent_ref)
               break;
            
            ref = represent_ref;
            fake_id = -1;
         }

         if (result)
            break;
      }
   }
   
   DisposeMemory(url_list_ref);
   DisposeMemory(key_data);
   DisposeMemory(custom_ref);
   UnuseMemory(pg_rec->t_formats);
   UnuseMemory(pg);
   
   return      result;
}



/* pgSaveEmbedRef is called by apps that want to save an embed_ref that is
not saved in the normal way. In other words, an embed ref that is marked for
"keep_around" might not exist in the style_info record(s) and thus needs to
be saved descretely. */

PG_PASCAL (pg_error) pgSaveEmbedRef (pg_ref pg, embed_ref ref, long element_info,
      file_io_proc io_proc, file_io_proc data_io_proc, size_t PG_FAR *file_position,
      file_ref filemap) 
{
   paige_rec_ptr           pg_rec;
   memory_ref              key_data, use_ref, represent_ref;
   pack_walk               walker;
   file_io_proc            data_proc;
   pg_embed_ptr            embed_ptr;
   long                 fake_id;
   pg_error                result;

   pg_rec = (paige_rec_ptr) UseMemory(pg);
   key_data = MemoryAlloc(pg_rec->globals->mem_globals, 1, 0, 32);
   use_ref = ref;
   fake_id = 0;
   
   for (;;) {
   
      SetMemorySize(key_data, 0);
      pgSetupPacker(&walker, key_data, 0);
      pgPackEmbedRef(&walker, MEM_NULL, use_ref, fake_id);
      pgFinishPack(&walker);
      
      if (!(data_proc = data_io_proc))
         data_proc = io_proc;

      result = pgWriteKeyData(pg, embedded_item_key, UseMemory(walker.data_ref), walker.transfered,
            element_info, io_proc, data_proc, file_position, filemap);
         
         UnuseMemory(walker.data_ref);
         
         embed_ptr = (pg_embed_ptr) UseMemory(use_ref);
         represent_ref = embed_ptr->embed_represent;
         UnuseMemory(use_ref);
         
         if (!represent_ref)
               break;
            
         fake_id = -1;
         use_ref = represent_ref;
   }
   
   DisposeMemory(walker.data_ref);

   UnuseMemory(pg);

   return      result;
}


/* pgEmbedStyleInit is the style_info function to initialize the associated style
with an embedded ref. */

PG_PASCAL (void) pgEmbedStyleInit (paige_rec_ptr pg, style_info_ptr style, font_info_ptr font)
{
   pg_embed_ptr         embed_ptr;
   pg_boolean        	init_as_text;
   long                 object_ascent, object_descent;

   embed_ptr = (pg_embed_ptr) UseMemory(style->embed_object);
   init_as_text = (pg_boolean)((embed_ptr->type & USE_TEXT_HEIGHT) != 0);
   UnuseMemory(style->embed_object);
 
   if ((style->class_bits & EMBED_INITED_BIT) && !init_as_text)
      return;
   
   pg->globals->def_style.procs.init(pg, style, font);

   if (!init_as_text) {
      
      embed_ptr = (pg_embed_ptr) UseMemory(style->embed_object);
      object_ascent = embed_ptr->height - embed_ptr->descent + embed_ptr->top_extra;
      object_descent = embed_ptr->descent + embed_ptr->bot_extra;
   
      style->ascent += (short)(object_ascent - style->ascent);
      style->descent += (short)(object_descent - (style->descent + style->leading));
      
      if (embed_ptr->type & ALIGN_BASELINE_FLAG) {
           
           if (style->ascent < embed_ptr->height)
               style->ascent = (short)embed_ptr->height;
      }
	  else
	  if (embed_ptr->type & ALIGN_BBASELINE_FLAG) {

           if (style->descent < (embed_ptr->height - embed_ptr->descent))
               style->descent = (short)(embed_ptr->height - embed_ptr->descent);
	  }
	  
      UnuseMemory(style->embed_object);
   }
   
   style->class_bits |= EMBED_INITED_BIT;
}


/* pgInitEmbedStyleInfo places all the required function pointers in style & font
and initializes the "mask" fields.  The will_be_inserted param is TRUE if
the data does not yet exist (will be inserted), but FALSE if the data is
already in the text (makes a difference which "setup insert" hook we set).
The function returns the embed_ref type. */

PG_PASCAL (long) pgInitEmbedStyleInfo (paige_rec_ptr pg, long position, embed_ref ref,
   short stylesheet_option, embed_callback callback, long callback_refcon,
   style_info_ptr style, style_info_ptr mask, font_info_ptr font,
   font_info_ptr font_mask, pg_boolean will_be_inserted)
{
   pg_embed_ptr         new_embed;
   embed_callback       use_callback;
   pg_boolean           character_type;
   long                 new_type;
   short                use_stylesheet_option;

   if (!(use_callback = callback))
      use_callback = pgDefaultEmbedCallback;
   
   use_stylesheet_option = stylesheet_option;
   
   if (mask && !pg->import_control)
      pgGetStyleInfo(pg->myself, NULL, FALSE, style, mask);

   if (font) {
   
      pgGetFontInfo(pg->myself, NULL, FALSE, font, font_mask);
      
      if (font_mask)
         pgFillBlock(font_mask, sizeof(font_info), -1);
   }
   
   if (mask)
      pgFillBlock(&mask->procs, sizeof(pg_style_hooks), 0);
   
   if (!pg->import_control)
      pgSetSelection(pg->myself, position, position, 0, FALSE);

   new_embed = (pg_embed_ptr) UseMemory(ref);
   new_type = new_embed->type & EMBED_TYPE_MASK;
   
   if (will_be_inserted)
      style->procs.insert_proc = embed_setup_insert_init;
   else
      style->procs.insert_proc = embed_setup_insert;

   character_type = (pg_boolean)(new_type == embed_alternate_char || new_type == embed_dynamic_string);
   style->procs.char_info = pg->globals->def_style.procs.char_info;

   // QC - must clear embed inited bit so that we init the new embed style
   // even if it's being based on another embed style
   style->class_bits &= ~EMBED_INITED_BIT;
   // End QC

   if ((new_embed->type & EMBED_CONTROL_FLAG) || !character_type) {
   
      style->class_bits |= (STYLE_IS_CONTROL | CANT_TRANS_BIT);
      
      if (!character_type && will_be_inserted)
         style->procs.char_info = embed_char_info;
   }     
   else if (new_embed->type & GROUP_TEXT_FLAG) {
   
      style->class_bits |= (CANNOT_BREAK | GROUP_CHARS_BIT | CANT_TRANS_BIT);
   }
   
   if (new_embed->type & REQUIRES_ACTIVATE_FLAG)
   {
      style->procs.activate_proc = embed_activate;
      style->class_bits |= ACTIVATE_ENABLE_BIT;
   }

   style->procs.init = pgEmbedStyleInit;
   style->procs.draw = embed_draw;
   style->procs.measure = measure_embed;
   style->procs.duplicate = embed_copy;
   style->procs.delete_style = embed_delete;
   style->procs.track_ctl = track_embed_ctl;

   style->embed_entry = (void *)(uintptr_t)use_callback;
   style->embed_object = ref;
   style->embed_refcon = new_embed->user_refcon;
   style->embed_style_refcon = callback_refcon;
   
   style->char_bytes = 1;      // Insertions normally require two bytes

   style->embed_id = pgUniqueID(pg->myself);

   style->styles[super_impose_var] = use_stylesheet_option;

   use_callback(pg, new_embed, new_type, EMBED_VMEASURE, callback_refcon, (void*)style, 0);
   UnuseMemory(ref);

   if (mask) {
   
      mask->procs.init = (style_init_proc)SET_MASK_BITS;
      mask->procs.draw = (text_draw_proc)SET_MASK_BITS;
      mask->procs.insert_proc = (setup_insert_proc)SET_MASK_BITS;
      mask->procs.measure = (measure_proc)SET_MASK_BITS;
      mask->procs.duplicate = (dup_style_proc)SET_MASK_BITS;
      mask->procs.delete_style = (delete_style_proc)SET_MASK_BITS;
      mask->procs.track_ctl = (track_control_proc)SET_MASK_BITS;
       mask->procs.char_info = (char_info_proc)SET_MASK_BITS;
   
      mask->embed_entry = (void *) SET_MASK_BITS;
      mask->embed_object = (memory_ref)SET_MASK_BITS;
      mask->embed_refcon = SET_MASK_BITS;
      mask->embed_style_refcon = SET_MASK_BITS;
      mask->embed_id = SET_MASK_BITS;
      mask->char_bytes = SET_MASK_BITS;
      mask->class_bits = SET_MASK_BITS;
      mask->styles[super_impose_var] = SET_MASK_BITS;
   }

   return   new_type;
}

/* pgNewImageRecord returns a memory_ref containing image info. The result is a memory_ref
to use for the "data" portion of an embed_url_image type. This memory ref *might* be already
in use somewhere else, in which case the used_ctr is incremented and that ref is returned. */

PG_PASCAL (memory_ref) pgNewImageRecord (pg_ref pg, pg_url_image_ptr image, embed_callback callback, long check_to)
{
   paige_rec_ptr        pg_rec;
   style_info_ptr       styles;
   pg_url_image_ptr     result_image;
   long              num_styles, index;
   memory_ref           result = MEM_NULL;
   
   pg_rec = (paige_rec_ptr) UseMemory(pg);
   num_styles = GetMemorySize(pg_rec->t_formats);
   styles = (style_info_ptr) UseMemory(pg_rec->t_formats);
   
   if (check_to >= 0)
      num_styles = check_to;

   for (index = 0; index < num_styles; ++index, ++styles) {
      
      if (styles->embed_object) {
         pg_embed_ptr      embed_ptr;
         
         embed_ptr = (pg_embed_ptr) UseMemory(styles->embed_object);
         
         if ((embed_ptr->type & EMBED_TYPE_MASK) == embed_url_image && embed_ptr->data) {
            pg_url_image      image_record;
            
            GetMemoryRecord((memory_ref)embed_ptr->data, 0, &image_record);
            
            if (pgEqualStruct(image_record.URL, image->URL, sizeof(image_record.URL))) {
               
               result = (memory_ref)embed_ptr->data;
               UnuseMemory(styles->embed_object);
               
               break;
            }
         }
         
         UnuseMemory(styles->embed_object);
      }
   }

   UnuseMemory(pg_rec->t_formats);
   
   if (result == MEM_NULL) {
      embed_callback       use_callback;
      pg_embed_rec         temp_embed;
      
      result = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(pg_url_image), 1, 0);
      result_image = (pg_url_image_ptr) UseMemory(result);
      *result_image = *image;
      result_image->used_ctr = 0;
      
      if ((use_callback = callback) == NULL)
         use_callback = pgDefaultEmbedCallback;
      
      pgFillBlock(&temp_embed, sizeof(pg_embed_rec), 0);
      temp_embed.type = embed_url_image;
      temp_embed.data = (void PG_FAR *)result;
      temp_embed.version = CURRENT_EMBED_VERSION;
      use_callback(pg_rec, &temp_embed, embed_url_image, EMBED_PREPARE_IMAGE, 0, (void*)result_image, 0);

      UnuseMemory(result);
   }
   
   result_image = (pg_url_image_ptr) UseMemory(result);
   result_image->used_ctr += 1;
   UnuseMemory(result);
   UnuseMemory(pg);
   
   return      result;
}


/* pgLoadImages forces all images to load.  This avoid unnecessary delay when importing HTML
or equiv. files. The waitproc stuff is used for the waitproc getting called. */

PG_PASCAL (void) pgLoadImages (pg_ref pg, embed_callback image_callback, short wait_proc_verb)
{
   paige_rec_ptr        pg_rec;
   style_info_ptr       styles;
   pg_embed_ptr         embed_ptr;
   embed_callback       use_callback;
   pg_url_image_ptr     image;
   pg_frame_ptr			frame;
   memory_ref			PG_FAR *frame_refs;
   long              	num_styles, num_frames, index, count, count_progress;

   pg_rec = (paige_rec_ptr) UseMemory(pg);
   num_styles = GetMemorySize(pg_rec->t_formats);
   styles = (style_info_ptr) UseMemory(pg_rec->t_formats);

   for (index = count = 0; index < num_styles; ++index, ++styles) {
      
      if (styles->embed_object) {
         
         embed_ptr = (pg_embed_ptr) UseMemory((memory_ref)styles->embed_object);
         
         if ((embed_ptr->type & EMBED_TYPE_MASK) == embed_url_image)
            count += 1;
         
         UnuseMemory((memory_ref)styles->embed_object);
      }
   }
   
   if (pg_rec->flags2 & HAS_PG_FRAMES_BIT) {
   		
   		num_frames = GetMemorySize(pg_rec->exclusions);
   		frame_refs = (memory_ref *) UseMemory(pg_rec->exclusions);
   		
   		for (index = 0; index < num_frames; ++index, ++frame_refs) {
   			
   			frame = (pg_frame_ptr) UseMemory(*frame_refs);
   			
   			if (frame->type == frame_embed && frame->data) {
   				
   				embed_ptr = (pg_embed_ptr) UseMemory((memory_ref)frame->data);
   
	            if ((embed_ptr->type & EMBED_TYPE_MASK) == embed_url_image)
	            	count += 1;
         
        		UnuseMemory((memory_ref)frame->data);
   			}
   			
   			UnuseMemory(*frame_refs);
   		}
   		
   		UnuseMemory(pg_rec->exclusions);
   }
   else
		num_frames = 0;

   if (count > 0) {
   
      styles = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, 0, 0, FALSE);

      for (index = count_progress = 0; index < num_styles; ++index, ++styles) {

         pg_rec->procs.wait_proc(pg_rec, wait_proc_verb, count_progress, count);

         if (styles->embed_object) {
            
            embed_ptr = (pg_embed_ptr) UseMemory((memory_ref)styles->embed_object);
            
            if ((embed_ptr->type & EMBED_TYPE_MASK) == embed_url_image) {
               
               ++count_progress;
               image = (pg_url_image_ptr) UseMemory((memory_ref)embed_ptr->data);

               if ((use_callback = image_callback) == NULL)
                  use_callback = pgDefaultEmbedCallback;
               
               if (!image->image_data && !image->loader_result)
                  use_callback(pg_rec, embed_ptr, embed_url_image, EMBED_LOAD_IMAGE, embed_ptr->style_refcon,
                        (void*)image, 0);

               UnuseMemory((memory_ref)embed_ptr->data);
            }
            
            UnuseMemory((memory_ref)styles->embed_object);
         }
      }
      
      if (num_frames) {

	   		frame_refs = (memory_ref *) UseMemory(pg_rec->exclusions);
	   		
	   		for (index = 0; index < num_frames; ++index, ++frame_refs) {
	   			
	   			pg_rec->procs.wait_proc(pg_rec, wait_proc_verb, count_progress, count);

	   			frame = (pg_frame_ptr) UseMemory(*frame_refs);
	   			
	   			if (frame->type == frame_embed && frame->data) {
	   				
	   				embed_ptr = (pg_embed_ptr) UseMemory((memory_ref)frame->data);
	   
		            if ((embed_ptr->type & EMBED_TYPE_MASK) == embed_url_image) {
		            	
			               ++count_progress;
			               image = (pg_url_image_ptr) UseMemory((memory_ref)embed_ptr->data);

			               if ((use_callback = image_callback) == NULL)
			                  use_callback = pgDefaultEmbedCallback;
			               
			               if (!image->image_data && !image->loader_result)
			                  use_callback(pg_rec, embed_ptr, embed_url_image, EMBED_LOAD_IMAGE, embed_ptr->style_refcon,
			                        (void*)image, 0);

			               UnuseMemory((memory_ref)embed_ptr->data);
	         		}
	         		
	        		UnuseMemory((memory_ref)frame->data);
	   			}
	   			
	   			UnuseMemory(*frame_refs);
	   		}
	   		
	   		UnuseMemory(pg_rec->exclusions);
      }

      pg_rec->procs.wait_proc(pg_rec, wait_proc_verb, count, count);
   }

   UnuseMemory(pg_rec->t_formats);
   UnuseMemory(pg);
}


/* pgPackEmbedRef sends all the data in ref to the walker (to write it to a file) */

PG_PASCAL (void) pgPackEmbedRef (pack_walk_ptr walker, memory_ref url_list, embed_ref ref, long fake_ref_id)
{
   pg_embed_ptr            embed_ptr;
   long                    ref_id;

   if (fake_ref_id)
         ref_id = fake_ref_id;
   else
      ref_id = (long)ref;

   pgPackNum(walker, long_data, ref_id);     // So I know where it goes on READ
   
   embed_ptr = (pg_embed_ptr) UseMemory(ref);

   pgPackNum(walker, long_data, CURRENT_EMBED_VERSION);
   pgPackNum(walker, long_data, embed_ptr->type);
   pgPackNum(walker, long_data, embed_ptr->width);
   pgPackNum(walker, long_data, embed_ptr->height);
   pgPackNum(walker, long_data, embed_ptr->descent);
   pgPackNum(walker, long_data, embed_ptr->draw_flags);
   pgPackNum(walker, long_data, embed_ptr->modifier);
   pgPackNum(walker, long_data, embed_ptr->user_refcon);
   pgPackNum(walker, long_data, (long)embed_ptr->data);
   pgPackNum(walker, long_data, (long)embed_ptr->rtf_text_data);

   pgPackNum(walker, short_data, (long)embed_ptr->top_extra);
   pgPackNum(walker, short_data, (long)embed_ptr->bot_extra);
   pgPackNum(walker, short_data, (long)embed_ptr->left_extra);
   pgPackNum(walker, short_data, (long)embed_ptr->right_extra);
   pgPackNum(walker, short_data, (long)embed_ptr->alignment_flags);

   pgPackUnicodeBytes(walker, (pg_bits8_ptr)embed_ptr->uu.alt_data, ALT_SIZE * sizeof(pg_char), FALSE);
   pgPackNum(walker, long_data, embed_ptr->border_info);
   pgPackNum(walker, long_data, embed_ptr->border_color);
   pgPackNum(walker, long_data, embed_ptr->shading);

   if (embed_ptr->data) {
      
      switch (embed_ptr->type & EMBED_TYPE_MASK) {

         case embed_control:
#ifdef MAC_PLATFORM
            pgPackNum(walker, long_data, GetControlReference((ControlRef)embed_ptr->data));
#endif
            break;

         case embed_bitmap:
            pack_pic_embed(walker, embed_ptr);
            pgPackBitMap (walker, (bitmap_ref)embed_ptr->data);
            break;
            
         case embed_polygon:
            pgPackPoly(walker, (poly_ref)embed_ptr->data);
            break;

         case embed_pg_link:
            break;

         case embed_mac_pict:
            pack_pic_embed(walker, embed_ptr);
            pack_graphics(walker, embed_mac_pict, embed_ptr->data);
            break;

         case embed_meta_file:
            pack_pic_embed(walker, embed_ptr);
            embed_ptr->data = pack_graphics(walker, embed_meta_file, embed_ptr->data);
            break;

         case embed_qt_component:
            break;
         
         case embed_ole: // PDA:
            pgPackNum(walker, long_data, (long)embed_ptr->data);
            break;
         case embed_mac_vm_pict:
         case embed_os2_meta_file:
         case embed_dibitmap:
         case embed_wbitmap:
         case embed_gif_pict:
         case embed_jpeg_pict:
            pack_pic_embed(walker, embed_ptr);
         case embed_alternate_char:
         case embed_dynamic_string:
         case embed_user_data:
         case embed_bookmark_start:
         case embed_bookmark_end:
         case embed_unsupported_object:
            pgPackBytes(walker, (pg_bits8_ptr) UseMemory((memory_ref)embed_ptr->data),
                  GetByteSize((memory_ref)embed_ptr->data));
            UnuseMemory((memory_ref)embed_ptr->data);
            
            if (embed_ptr->rtf_text_data) {
 
               pgPackBytes(walker, (pg_bits8_ptr) UseMemory(embed_ptr->rtf_text_data), GetByteSize(embed_ptr->rtf_text_data));
               UnuseMemory(embed_ptr->rtf_text_data);
            }

            break;

         case embed_qt_movie:
            pack_pic_embed(walker, embed_ptr);
#ifdef MAC_PLATFORM
#endif
            break;

         case embed_qt_flat_movie:
            pack_pic_embed(walker, embed_ptr);
            break;

         case embed_time:
           case embed_date:
            pgPackNum(walker, long_data, embed_ptr->uu.date_info.time);
            pgPackNum(walker, short_data, (long)embed_ptr->uu.date_info.time_type);
            pgPackNum(walker, short_data, (long)embed_ptr->uu.date_info.format);
            break;

           case embed_cur_page_num:
           case embed_cur_sect_num:
            break;
            
         case embed_horiz_rule:
            pgPackNum(walker, short_data, (long)embed_ptr->uu.line_info.line_size);
            pgPackNum(walker, short_data, (long)embed_ptr->uu.line_info.line_shading);
            pgPackNum(walker, short_data, (long)embed_ptr->uu.line_info.line_alignment);
            pgPackNum(walker, short_data, (long)embed_ptr->uu.line_info.line_width_type);
            pgPackNum(walker, long_data, embed_ptr->uu.line_info.line_width);
            break;
           
         case embed_url_image:
            pgPackNum(walker, long_data, find_url_ref(url_list, (memory_ref)embed_ptr->data));
            break;
      }
   }

   UnuseMemory(ref);
}


/******************************** Local functions ******************************/


/* measure_embed is the style_info function that PAIGE calls to measure char
width. */

STATIC_PASCAL (void) measure_embed (paige_rec_ptr pg, style_walk_ptr walker,
      pg_char_ptr data, size_t length, pg_short_t slop, long PG_FAR *positions,
      short PG_FAR *types, short measure_verb, size_t current_offset, short scale_widths,
      short call_order)
{
   register long PG_FAR     *char_locs;
   pg_embed_measure         measure_struct;
   pg_embed_measure_ptr     measure_ptr;
   embed_callback           callback;
   style_info_ptr           style;
   font_info_ptr            font;
   pg_embed_ptr             embed_ptr;
   size_t                   char_ctr, width_average, the_type;
   size_t                   ending_width, use_width, incrementing_width;

   if (types)
      pgFillBlock(types, length * sizeof(short), 0);
   
   measure_struct.walker = walker;
   measure_struct.text = data;
   measure_struct.text_size = length;
   measure_struct.slop = slop;
   measure_struct.positions = positions;
   measure_struct.types = types;
   measure_struct.measure_verb = measure_verb;
   measure_struct.current_offset = current_offset;
   measure_struct.call_order = call_order;
   measure_struct.scale_verb = scale_widths;
   measure_ptr = &measure_struct;

   style = walker->cur_style;
   font = walker->cur_font;
   embed_ptr = (pg_embed_ptr) UseMemory(style->embed_object);
   embed_ptr->style = style;
   embed_ptr->style_refcon = style->embed_style_refcon;
   callback = (embed_callback)style->embed_entry;

   the_type = embed_ptr->type & EMBED_TYPE_MASK;

   if (the_type == embed_alternate_char || the_type == embed_dynamic_string) {
      memory_ref        temp_ref;
      long PG_FAR       *temp_positions;
      pg_char_ptr       the_text;
      short             the_size;

      if (embed_ptr->data)
         the_text = (pg_char_ptr) UseMemory((memory_ref)embed_ptr->data);
      else
         the_text = (pg_char_ptr)&embed_ptr->uu.alt_data;

      the_size = (short)pgCStrLength((pg_c_string_ptr)the_text);

      if (the_type == embed_dynamic_string) {
         long     max_size;
         
         max_size = embed_ptr->modifier & 0x0000FFFF;
         
         the_size = (short)callback(pg, embed_ptr,
               the_type, EMBED_SWAP, embed_ptr->style_refcon, (void*)the_text, (void*)max_size);
      }

      if (the_size > length) {

         temp_ref = MemoryAlloc(pg->globals->mem_globals, sizeof(long), the_size + 1, 0);
         temp_positions = (long *) UseMemory(temp_ref);
         *temp_positions = *positions;
      }
      else {
         
         temp_ref = MEM_NULL;
         temp_positions = positions;
      }
      
      if (the_size) {
      
         pgMeasureProc(pg, walker, the_text, the_size, slop, temp_positions, NULL,
               measure_verb, current_offset, scale_widths, call_order);

         embed_ptr->width = temp_positions[the_size] - *temp_positions;
         
         if (embed_ptr->type & INCLUDE_EXTRA_SPACES)
            embed_ptr->width += extra_trailing_spaces(pg, walker, data, length);
      }
      else
         embed_ptr->width = 0;

      if (temp_ref)
         UnuseAndDispose(temp_ref);
      if (embed_ptr->data)
         UnuseMemory((memory_ref)embed_ptr->data);
   }

   if (measure_verb != measure_draw_locs)
      callback(pg, embed_ptr, the_type, EMBED_MEASURE, embed_ptr->style_refcon, (void*)measure_ptr, 0);

   if (embed_empty(embed_ptr))
      if (embed_ptr->width < embed_ptr->empty_width)
         embed_ptr->width = embed_ptr->empty_width;
   
   use_width = embed_ptr->width;

   if (embed_ptr->minimum_width)
      if (use_width < embed_ptr->minimum_width)
         use_width = embed_ptr->minimum_width;

   char_locs = positions;
   ending_width = use_width + embed_ptr->left_extra + embed_ptr->right_extra;
   
   // Multiply ending width times possible multiple occurrences of same ref:
   
   ending_width *= (length / (style->char_bytes + 1));

   width_average = ending_width / length;
   incrementing_width = *char_locs;
   ending_width += incrementing_width;

   for (char_ctr = 0; char_ctr < length; ++char_ctr, incrementing_width += width_average)
      char_locs[char_ctr] = incrementing_width;
   
   char_locs[char_ctr] = ending_width;

   embed_ptr->style = NULL;
   UnuseMemory(style->embed_object);
}

/* embed_activate is the embed bridge for the activate style type */

STATIC_PASCAL (void) embed_activate (paige_rec_ptr pg_rec, style_info_ptr style,
      select_pair_ptr text_range, pg_char_ptr text, short front_back_state,
      short perm_state, pg_boolean show_hilite)
{
   embed_callback             callback;
   pg_embed_ptr               embed_ptr;
   pg_embed_activate          activate_data;

   embed_ptr = (pg_embed_ptr) UseMemory(style->embed_object);
   embed_ptr->style = style;
   embed_ptr->style_refcon = style->embed_style_refcon;
   callback = (embed_callback)style->embed_entry;
   
   activate_data.style = style;
   activate_data.text_range = text_range;
   activate_data.text = text;
   activate_data.front_back_state = front_back_state;
   activate_data.perm_state = perm_state;
   activate_data.show_hilite = show_hilite;

   callback(pg_rec, embed_ptr, embed_ptr->type & EMBED_TYPE_MASK, (short)((pg_rec->flags & (PERM_DEACT_BIT | DEACT_BIT)) ? EMBED_DEACTIVATE : EMBED_ACTIVATE), embed_ptr->style_refcon, &activate_data, 0);
}


/* track_embed_ctl gets called by PAIGE to track a "control" type item. */

STATIC_PASCAL (long) track_embed_ctl (paige_rec_ptr pg, short verb,
      t_select_ptr first_select, t_select_ptr last_select, style_walk_ptr styles,
      pg_char_ptr associated_text, point_start_ptr bounds_info, short modifiers,
      long track_refcon)
{
   pg_embed_ptr            embed_ptr;
   style_info_ptr          embed_style;
   embed_callback          callback;
   pg_embed_click          click_struct;
   pg_embed_click_ptr      click_ptr;
   long                    track_result;
   t_select_ptr            hilite_select;
   select_pair             style_range;
   short                   command;

   embed_style = styles->cur_style;
   
   pgFillBlock(&click_struct, sizeof(pg_embed_click), 0);

   click_struct.first_select = first_select;
   click_struct.last_select = last_select;
   click_struct.modifiers = modifiers;
   click_ptr = &click_struct;

   embed_ptr = (pg_embed_ptr) UseMemory(embed_style->embed_object);
   style_range.begin = styles->prev_style_run->offset;
   style_range.end = styles->next_style_run->offset;

   if (style_range.end > pg->t_length)
      style_range.end = pg->t_length;
   
   get_visual_frame(pg, embed_ptr, &style_range, &click_struct.bounds);
   UnuseMemory(embed_style->embed_object);

   if (verb == mouse_down) {
      
      click_struct.point = first_select->original_pt;
      pgSubPt(&pg->scroll_pos, &click_struct.point);
      pgAddPt(&pg->port.origin, &click_struct.point);
     pgScalePt(&pg->port.scale, NULL, &click_struct.point);

      if (modifiers & (~WORD_MOD_BIT))
         return   0L;

      if (!pgPtInRect(&click_struct.point, &click_struct.bounds))
         return   0L;
   }
   else {
   
      click_struct.point = last_select->original_pt;
      pgSubPt(&pg->scroll_pos, &click_struct.point);
      pgAddPt(&pg->port.origin, &click_struct.point);
     pgScalePt(&pg->port.scale, NULL, &click_struct.point);
   }
   
   embed_ptr = (pg_embed_ptr) UseMemory(embed_style->embed_object);
   embed_ptr->style = embed_style;
   embed_ptr->style_refcon = embed_style->embed_style_refcon;
   callback = (embed_callback)embed_style->embed_entry;

   if (modifiers & WORD_MOD_BIT)
      command = EMBED_DOUBLECLICK;
   else
   if (verb == mouse_down)
      command = EMBED_MOUSEDOWN;
   else
   if (verb == mouse_moved)
      command = EMBED_MOUSEMOVE;
   else
      command = EMBED_MOUSEUP;
   
   track_result = callback(pg, embed_ptr,
         embed_ptr->type & EMBED_TYPE_MASK, command, embed_style->embed_style_refcon,
         (void*)click_ptr, 0);

   embed_ptr->style = NULL;
   UnuseMemory(embed_style->embed_object);
   
   hilite_select = (t_select_ptr) UseMemory(pg->select);

   if (track_result) {
   
      hilite_select->flags |= SELECT_ON_CTL;
      hilite_select[1].flags |= SELECT_ON_CTL;
   }
   else {
   
      hilite_select->flags &= (~SELECT_ON_CTL);
      hilite_select[1].flags &= (~SELECT_ON_CTL);
   }
   
   UnuseMemory(pg->select);

   return   track_result;
}



/* embed_draw gets called by PAIGE to draw text. In this case we draw our
embedded object. */

STATIC_PASCAL (void) embed_draw (paige_rec_ptr pg, style_walk_ptr walker, pg_char_ptr data,
      pg_short_t offset, pg_short_t length, draw_points_ptr draw_position,
      long extra, short draw_mode)
{
   style_info_ptr             style;
   font_info_ptr              font;
   rectangle                  draw_frame;
   rectangle_ptr              draw_frame_ptr;
   embed_callback             callback;
   long                       the_type, scaled_width, scaled_height, scaled_descent, height_diff;
   pg_embed_ptr               embed_ptr;
   
   style = walker->cur_style;
   font = walker->cur_font;   
   embed_ptr = (pg_embed_ptr) UseMemory(style->embed_object);
  
   the_type = embed_ptr->type & EMBED_TYPE_MASK;
   callback = (embed_callback)style->embed_entry;

   embed_ptr->style = style;
   embed_ptr->style_refcon = style->embed_style_refcon;

   scaled_width = embed_ptr->width;
   scaled_height = embed_ptr->height;
   scaled_descent = embed_ptr->descent;

   if (pg->scale_factor.scale) {
   
      pgScaleLong(pg->scale_factor.scale, 0, &scaled_width);
      pgScaleLong(pg->scale_factor.scale, 0, &scaled_height);
      pgScaleLong(pg->scale_factor.scale, 0, &scaled_descent);
   }

   draw_frame.top_left = draw_position->from;
   draw_frame.top_left.h += embed_ptr->left_extra;
   draw_frame.bot_right.h = draw_frame.top_left.h + scaled_width;
   
   draw_frame_ptr = &draw_frame;
   
   if (embed_ptr->type & USE_TEXT_HEIGHT) {
      
      draw_frame.top_left.v -= draw_position->ascent;
      draw_frame.bot_right.v = draw_position->from.v + draw_position->descent;
   }
   else
   if (embed_ptr->type & (ALIGN_CBASELINE_FLAG | ALIGN_BBASELINE_FLAG)) {

        height_diff = ((draw_position->ascent + draw_position->descent) - scaled_height);
        
        if (embed_ptr->type & ALIGN_CBASELINE_FLAG)
        	height_diff /= 2;
		else
			height_diff -= draw_position->ascent;
			
        draw_frame.top_left.v = draw_frame.bot_right.v = draw_position->from.v
                           - draw_position->ascent + height_diff;
        draw_frame.bot_right.v += scaled_height;
   }
   else {
   
      draw_frame.top_left.v -= (scaled_height - scaled_descent);
      draw_frame.bot_right.v = draw_frame.top_left.v + scaled_height;
   }

   if (the_type == embed_alternate_char || the_type == embed_dynamic_string) {
      pg_char_ptr       the_chars;
      short             the_size;
      
      if (embed_ptr->data)
         the_chars = (pg_char_ptr) UseMemory((memory_ref)embed_ptr->data);
      else
         the_chars = (pg_char_ptr)&embed_ptr->uu.alt_data;

      the_size = (short)pgCStrLength((pg_c_string_ptr)the_chars);

      if (the_type == embed_dynamic_string) {
         long     max_size;
         
         max_size = embed_ptr->modifier & 0x0000FFFF;
         the_size = (short)callback(pg, embed_ptr, embed_dynamic_string,
               EMBED_SWAP, embed_ptr->style_refcon, (void*)the_chars, (void*)max_size);
      }

     if (the_size) {
            pg_short_t        repeating_index, char_size;
            
            char_size = (pg_short_t)style->char_bytes + 1;
            draw_position->to.h = draw_position->from.h;
 
            for (repeating_index = 0; repeating_index < length; repeating_index += char_size) {
 
               draw_position->from.h = draw_position->to.h;
            pgDrawProc(pg, walker, the_chars, 0, the_size, draw_position, extra, draw_mode);
         }
     }

      if (embed_ptr->data)
         UnuseMemory((memory_ref)embed_ptr->data);
   }
   else {
         
        if (the_type == embed_url_image) {
            pg_url_image_ptr     image_ptr;
            memory_ref           image_ref;
            
            image_ref = (memory_ref)embed_ptr->data;
            image_ptr = (pg_url_image_ptr) UseMemory((memory_ref)image_ref);
         
         if (!image_ptr->image_data && !image_ptr->loader_result)
            callback(pg, embed_ptr, the_type, EMBED_LOAD_IMAGE, embed_ptr->style_refcon,
                     (void*)image_ptr, 0);

            UnuseMemory(image_ref);
     }

      callback(pg, embed_ptr, the_type, EMBED_DRAW,
            embed_ptr->style_refcon, draw_frame_ptr, (void*)draw_position);
   }

   embed_ptr->style = NULL;
   UnuseMemory(style->embed_object);
}


/* embed_copy gets called by PAIGE to duplicate any memory allocations within
the style given. In our case, we usually have a memory_ref sitting in the style. */

STATIC_PASCAL (void) embed_copy (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
      short reason_verb, format_ref all_styles, style_info_ptr style)
{
   paige_rec_ptr           pg_for_id;
   pg_embed_ptr            embed_ptr;
   pg_ref                  source_of_id;

   if (reason_verb == new_stylesheet_reason)
      return;

   embed_ptr = (pg_embed_ptr) UseMemory(style->embed_object);
   source_of_id = MEM_NULL;

   if (!(pg_for_id = target_pg))
      pg_for_id = src_pg;
   
   if (pg_for_id) {
      
      source_of_id = pg_for_id->myself;
      pg_for_id = pgUseSharedPg(&source_of_id);
      ++pg_for_id->next_id;
      style->embed_id = pg_for_id->next_id;
   }

   if ((embed_ptr->type & NOT_SHARED_FLAG) && ( (reason_verb == copy_reason) || (reason_verb == paste_reason))) {
      embed_callback    callback;
      memory_ref        the_copy;

      the_copy = MemoryDuplicate(style->embed_object);
      UnuseMemory(style->embed_object);
      style->embed_object = the_copy;
      embed_ptr = (pg_embed_ptr) UseMemory(style->embed_object);
      embed_ptr->used_ctr = 0;
      callback = (embed_callback) style->embed_entry;
      callback(pg_for_id, embed_ptr, embed_ptr->type & EMBED_TYPE_MASK, EMBED_COPY,
               style->embed_style_refcon, (void*)reason_verb, 0);
   }

   ++embed_ptr->used_ctr;

   UnuseMemory(style->embed_object);
   
   if (source_of_id)
      UnuseMemory(source_of_id);
}


/* embed_delete gets called by PAIGE when a style_info record is about to be
deleted. */

STATIC_PASCAL (void) embed_delete (paige_rec_ptr pg, pg_globals_ptr globals,
      short reason_verb, format_ref all_styles, style_info_ptr style)
{
   pg_embed_ptr            embed_ptr;
   embed_callback          callback;
   
   embed_ptr = (pg_embed_ptr) UseMemory(style->embed_object);
   
   if (embed_ptr->used_ctr)
      embed_ptr->used_ctr -= 1;

   if (!embed_ptr->used_ctr) {
      
      callback = (embed_callback)style->embed_entry;
      embed_ptr->style = style;
      callback(pg, embed_ptr, embed_ptr->type & EMBED_TYPE_MASK, EMBED_DESTROY,
               style->embed_style_refcon, (void*)reason_verb, 0);
      embed_ptr->style = NULL;
      UnuseAndDispose(style->embed_object);
   }
   else
      UnuseMemory(style->embed_object);
}



/* embed_setup_insert_init is the setup_insert proc for the FIRST INSERT,
which causes my own data to get inserted correctly. Then, within this function,
we setup for the "real" setup_insert proc before returning. */

STATIC_PASCAL (pg_boolean) embed_setup_insert_init (paige_rec_ptr pg, style_info_ptr style,
      long position)
{
   style->procs.insert_proc = embed_setup_insert;     // This gets called next time.
   return   TRUE;
}


/* embed_setup_insert gets called when one or more chars are about to be inserted,
in which case we do NOT want our embedded object to extend to additional chars. */

STATIC_PASCAL (pg_boolean) embed_setup_insert (paige_rec_ptr pg, style_info_ptr style,
      long position)
{
   style->embed_object = MEM_NULL;
   style->embed_refcon = 0;
   style->embed_id = 0;
   style->class_bits = 0;
   style->char_bytes = 0;
   style->styles[super_impose_var] = 0;
   style->procs = pg->globals->def_style.procs;

   return   TRUE;
}


/* embed_char_info returns info about word breaks (because we want to treat an
item as a "word". Note that we don't get called (hook was not set) for
character substitution.  */

STATIC_PASCAL (long) embed_char_info (paige_rec_ptr pg, style_walk_ptr style_walker,
      pg_char_ptr data, size_t block_offset, size_t offset_begin, size_t offset_end, size_t char_offset, long mask_bits)
{
   long        result;
   
   result = WORD_BREAK_BIT | WORD_SEL_BIT;
   
   if (data[char_offset] == DUMMY_RIGHT_EMBED)
      result |= (INCLUDE_BREAK_BIT | INCLUDE_SEL_BIT | LAST_HALF_BIT);
   else
      result |= FIRST_HALF_BIT;

   return   (result & mask_bits);
}


/* next_style_same_ref returns TRUE if the next style is an identical embed_ref. */

static pg_boolean next_style_same_ref (paige_rec_ptr pg, style_walk_ptr walker)
{
   pg_short_t        style_index;
   
   if (walker->next_style_run->offset >= pg->t_length)
      return   FALSE;
   
   style_index = walker->next_style_run->style_item;
   
   return   (walker->cur_style->embed_object == walker->style_base[style_index].embed_object);
}


/* previous_style_same_ref returns TRUE if the previous style has an identical
embed_ref. */

static pg_boolean previous_style_same_ref (paige_rec_ptr pg, style_walk_ptr walker)
{
   pg_short_t        style_index;
   
   if (!walker->prev_style_run->offset)
      return   FALSE;
   
   --walker->prev_style_run;
   style_index = walker->prev_style_run->style_item;
   ++walker->prev_style_run;

   return   (walker->cur_style->embed_object == walker->style_base[style_index].embed_object);
}


/* embed_empty returns TRUE if the embed data is knowingly empty. */

static pg_boolean embed_empty (pg_embed_ptr embed_ptr)
{
   long              length_test;

   switch (embed_ptr->type & EMBED_TYPE_MASK) {

      case embed_user_data:
         length_test = embed_ptr->modifier;
         break;

      case embed_bookmark_start:
      case embed_bookmark_end:
      case embed_alternate_char:
      case embed_dynamic_string:
      case embed_unsupported_object:
         if (embed_ptr->data) {
         
            length_test = pgCStrLength((pg_c_string_ptr)UseMemory((memory_ref)embed_ptr->data));
            UnuseMemory((memory_ref)embed_ptr->data);
         }
         else
            length_test = pgCStrLength((pg_c_string_ptr)embed_ptr->uu.alt_data);

         break;
      
      default:
         length_test = 1;
         break;
   }
   
   return   (length_test == 0);
}



/* standard_embed_draw is the default drawing proc. */

static void standard_embed_draw (paige_rec_ptr pg, pg_embed_ptr item,
      rectangle_ptr draw_bounds, draw_points_ptr draw_position, pg_boolean drawing_to_bitmap)
{
   memory_ref           the_data;
   graf_device_ptr      target_device;
   pg_url_image_ptr     image_ptr;
   generic_var			old_clip = (generic_var)0;
   long                 fill_color, low_modifier, hi_modifier, the_type;

   if (drawing_to_bitmap)
      target_device = &pg->globals->offscreen_port;
   else
      target_device = &pg->port;

   low_modifier = item->modifier & 0x0000FFFF;
   hi_modifier = item->modifier >> 16;

   the_data = (memory_ref)item->data;
   the_type = item->type;
   
   if ((the_type & EMBED_TYPE_MASK) == embed_url_image && the_data) {
         memory_ref           image_ref;
         
         image_ref = the_data;
         image_ptr = (pg_url_image_ptr) UseMemory((memory_ref)image_ref);
      
      if (image_ptr->image_data) {
      
         the_data = (memory_ref)image_ptr->image_data;
         the_type = image_ptr->type_and_flags;
      }
      
         UnuseMemory(image_ref);
   }

	if (item->type & CLIP_EMBED_FLAG) {
		rectangle		vis_rect;
		
		if (drawing_to_bitmap)
			old_clip = pgSetClipRect(target_device, draw_bounds, NULL);
		else {
		
			pgShapeBounds(pg->vis_area, &vis_rect);
			old_clip = pgSetClipRect(target_device, draw_bounds, &vis_rect);
		}
	}

	if (!item->shading || item->uu.pict_data.pict_transparent)
		fill_color = pgRGBToLong(&pg->bk_color);
	else
		fill_color = item->shading;

	pgPaintObject(target_device, draw_bounds, 0, object_rect, fill_color);

   switch (the_type & EMBED_TYPE_MASK) {

      case embed_oval:
         pgFrameObject(target_device, draw_bounds, 0, object_oval, (short)low_modifier);
         break;

      case embed_roundrectangle:
         pgFrameObject(target_device, draw_bounds, (short)hi_modifier, object_roundrect, (short)low_modifier);
         break;

      case embed_rectangle:
      case embed_user_box:
      case embed_url_image:
         pgFrameObject(target_device, draw_bounds, 0, object_rect, (short)low_modifier);

       if (the_data && (the_type & EMBED_TYPE_MASK) == embed_url_image) {
         rectangle      object;
         
         object = *draw_bounds;
         object.bot_right.h -= 1;
         object.bot_right.v -= 1;
         pgFrameObject(target_device, &object, 0, object_rect, (short)low_modifier);
		 
         pgInsetRect(&object, 1, 1);
         pgPaintObject(target_device, &object, 0, object_rect, 0x00A08080);
       }

         break;

      case embed_bitmap:
         pgCopyBits(target_device, (pg_bitmap_ptr)UseMemory(the_data),
               NULL, draw_bounds, bits_copy);
         UnuseMemory(the_data);
         break;
         
      case embed_polygon:
         pgFramePoly(target_device, the_data, draw_bounds, (short)low_modifier);
         break;

      case embed_control:
#ifdef MAC_PLATFORM
      {
         ControlHandle     ctl;
         
         ctl = (ControlHandle)the_data;
         SetRect(&(**ctl).contrlRect, draw_bounds->top_left.h,
               draw_bounds->top_left.v, draw_bounds->bot_right.h,
               draw_bounds->bot_right.v);
         (**ctl).contrlOwner = (WindowPtr)target_device->machine_var;
         Draw1Control(ctl);
      }
#else
      {
         pg_char_ptr unknown_text;
         co_ordinate centered_pt;
         short    length;
         
         pgPaintObject(target_device, draw_bounds, 0, object_rect, 0);
         unknown_text = (pg_char_ptr)MAC_CONTROL_STR;
         length = (short)pgCStrLength((pg_c_string_ptr)unknown_text);
         pgTextOut (target_device, center_unknown_text(item, target_device, unknown_text, length, draw_bounds, &centered_pt), unknown_text, length, 0);
      }
#endif
         break;

      case embed_ole:
     case embed_unsupported_object:
 
      {
         pg_char_ptr unknown_text;
         co_ordinate centered_pt;
         short    length;
         
         pgFrameObject(target_device, draw_bounds, 0, object_rect, 1);
         unknown_text = (pg_char_ptr)OLE_STR;
         length = (short)pgCStrLength((pg_c_string_ptr)unknown_text);
         pgTextOut (target_device, center_unknown_text(item, target_device, unknown_text, length, draw_bounds, &centered_pt), unknown_text, length, 0);
      }
         break;
      
      case embed_mac_pict:
#ifdef MAC_PLATFORM
      {
         PicHandle          pic;
         Rect           	frame;
         
         RectangleToRect(&item->uu.pict_data.pict_frame, NULL, &frame);
		 OffsetRect(&frame, (short)(draw_bounds->top_left.h - (long)frame.left),
		 			(short)(draw_bounds->top_left.v - (long)frame.top));
		 
         pic = (PicHandle)the_data;
         DrawPicture(pic, &frame);
      }
#else
      {
         pg_char_ptr unknown_text;
         co_ordinate centered_pt;
         short    length;
         
         pgFrameObject(target_device, draw_bounds, 0, object_rect, 1);
         unknown_text = (pg_char_ptr)MAC_PICTURE_STR;
         length = (short)pgCStrLength((pg_c_string_ptr)unknown_text);
         pgTextOut (target_device, center_unknown_text(item, target_device, unknown_text, length, draw_bounds, &centered_pt), unknown_text, length, 0);
      }
#endif
      break;
      
      case embed_mac_vm_pict:
#ifdef MAC_PLATFORM
      {
         PicHandle			pic;
         Rect				frame;
         pgm_globals_ptr	mem_globals;
         
         mem_globals = GetGlobalsFromRef(the_data);
         RectangleToRect(&item->uu.pict_data.pict_frame, NULL, &frame);
		 OffsetRect(&frame, (short)(draw_bounds->top_left.h - (long)frame.left),
		 			(short)(draw_bounds->top_left.v - (long)frame.top));

         pic = (PicHandle)MemoryToHandle(the_data);
         DrawPicture(pic, &frame);
         item->data = (void PG_FAR *) HandleToMemory (mem_globals, (Handle) pic, 1);
      }
#else
      {
         pg_char_ptr unknown_text;
         co_ordinate centered_pt;
         short    length;

         pgFrameObject(target_device, draw_bounds, 0, object_rect, 1);
         unknown_text = (pg_char_ptr)MAC_PICTURE_STR;
         length = (short)pgCStrLength((pg_c_string_ptr)unknown_text);
         pgTextOut (target_device, center_unknown_text(item, target_device, unknown_text, length, draw_bounds, &centered_pt), unknown_text, length, 0);
      }
#endif
      break;
      
      case embed_meta_file:
#ifdef WINDOWS_PLATFORM
        {
         RECT     	meta_rect;
         int		offset_h, offset_v;
         
         RectangleToRect(&item->uu.pict_data.pict_frame, NULL, &meta_rect);
         offset_h = (int)(draw_bounds->top_left.h - item->uu.pict_data.pict_frame.top_left.h);
         offset_v = (int)(draw_bounds->top_left.v - item->uu.pict_data.pict_frame.top_left.v);
         meta_rect.top += offset_v;
         meta_rect.bottom += offset_v;
         meta_rect.left += offset_h;
         meta_rect.right += offset_h;
 
         draw_meta_file(pg, the_data, (HDC)target_device->machine_ref, item, &meta_rect);
        }
#else
      {
         pg_char_ptr unknown_text;
         co_ordinate centered_pt;
         short    length;
         
         pgFrameObject(target_device, draw_bounds, 0, object_rect, 1);
         
         if (item->type & REPRESENTS_OTHER_FLAG)
            unknown_text = (pg_char_ptr)OLE_STR;
         else
            unknown_text = (pg_char_ptr)WIN_META_STR;

         length = pgCStrLength((pg_c_string_ptr)unknown_text);
         pgTextOut (target_device, center_unknown_text(item, target_device, unknown_text, length, draw_bounds, &centered_pt), unknown_text, length, 0);
      }
#endif
         break;

      case embed_os2_meta_file:
#ifndef OS2_PLATFORM
      {
         pg_char_ptr unknown_text;
         co_ordinate centered_pt;
         short    length;
         
         pgFrameObject(target_device, draw_bounds, 0, object_rect, 1);
         unknown_text = (pg_char_ptr)OS2_META_STR;
         length = (short)pgCStrLength((pg_c_string_ptr)unknown_text);
         pgTextOut (target_device, center_unknown_text(item, target_device, unknown_text, length, draw_bounds, &centered_pt), unknown_text, length, 0);
      }
#else
#endif
         break;

      case embed_dibitmap:
#ifndef WINDOWS_PLATFORM
      {
         pg_char_ptr unknown_text;
         co_ordinate centered_pt;
         short    length;
         
         pgPaintObject(target_device, draw_bounds, 0, object_rect, 0);
         unknown_text = (pg_char_ptr)WIN_BMP_STR;
         length = pgCStrLength((pg_c_string_ptr)unknown_text);
         pgTextOut (target_device, center_unknown_text(item, target_device, unknown_text, length, draw_bounds, &centered_pt), unknown_text, length, 0);
      }
#else
#endif
         break;

      case embed_wbitmap:
#ifndef WINDOWS_PLATFORM
      {
         pg_char_ptr unknown_text;
         co_ordinate centered_pt;
         short    length;
         
         pgPaintObject(target_device, draw_bounds, 0, object_rect, 0);
         unknown_text = (pg_char_ptr)WIN_DDBMP_STR;
         length = pgCStrLength((pg_c_string_ptr)unknown_text);
         pgTextOut (target_device, center_unknown_text(item, target_device, unknown_text, length, draw_bounds, &centered_pt), unknown_text, length, 0);
      }
#else
#endif
         break;

      case embed_pg_link:
         break;

      case embed_horiz_rule:
         break;
      
      case embed_user_data:
         break;

      default:
      {
         pg_char_ptr unknown_text;
         co_ordinate centered_pt;
         short    length;
         
         pgPaintObject(target_device, draw_bounds, 0, object_rect, 0);
         unknown_text = (pg_char_ptr)UNKNOWN_STR;
         length = (short)pgCStrLength((pg_c_string_ptr)unknown_text);
         pgTextOut (target_device, center_unknown_text(item, target_device, unknown_text, length, draw_bounds, &centered_pt), unknown_text, length, 0);
      }
         break;
   }

   pgDrawEmbedBorders(pg, item, draw_bounds);

	if (old_clip)
		pgRestoreClipRgn(target_device, old_clip);
}


/* standard_embed_dispose is the standard function to dispose an
embedded reference. */

static void standard_embed_dispose (pg_embed_ptr item, long the_type)
{
   memory_ref        the_data;
   long            use_type;
  
   the_data = (memory_ref)item->data;
   use_type = the_type;

   if (the_type == embed_url_image && the_data) {

         pg_url_image_ptr     image_ptr;
         
         image_ptr = (pg_url_image_ptr) UseMemory(the_data);
         image_ptr->used_ctr -= 1;
         
         if (image_ptr->used_ctr <= 0) {
   
            the_data = (memory_ref)image_ptr->image_data;
            use_type = image_ptr->type_and_flags & EMBED_TYPE_MASK;
            UnuseMemory((memory_ref)item->data);
            DisposeMemory((memory_ref)item->data);
         }
         else
            UnuseMemory(the_data);
   }

   switch (use_type) {

      case embed_control:
#ifdef MAC_PLATFORM
         DisposeControl((ControlRef)the_data);
         break;
#endif

      case embed_mac_pict:
#ifdef MAC_PLATFORM
#ifdef PG_DEBUG
		 if (ValidMemoryRef((memory_ref)the_data))
		 	  pgDebugProc(BAD_LINK_ERR, (memory_ref)the_data);
#endif
         DisposeHandle((Handle)the_data);
#else
         DisposeNonNilFailedMemory(the_data);
#endif
         break;
        
        case embed_meta_file:
#ifdef WINDOWS_PLATFORM
            
            DeleteMetaFile((HMETAFILE)the_data);
#else
         DisposeNonNilFailedMemory(the_data);
#endif
         break;
         
      case embed_bitmap:
         if (the_data)
         {
            pg_bitmap_ptr        bitmap_ptr;

            bitmap_ptr = (pg_bitmap_ptr) UseMemory(the_data);
            
            if (bitmap_ptr->palette)
               DisposeMemory(bitmap_ptr->palette);
   
            UnuseAndDispose(the_data);
         }
         break;

      case embed_qt_component:
       break;

      case embed_ole: // PDA:
      case embed_polygon:
      case embed_alternate_char:
      case embed_dynamic_string:
      case embed_user_data:
      case embed_mac_vm_pict:
      case embed_os2_meta_file:
      case embed_dibitmap:
      case embed_wbitmap:
      case embed_gif_pict:
      case embed_jpeg_pict:
      case embed_unsupported_object:
      default:
         DisposeNonNilFailedMemory(the_data);
            break;

      case embed_bookmark_start:
      case embed_bookmark_end:
      case embed_url_image:
            break;

      case embed_qt_movie:
      case embed_qt_flat_movie:
      //ее
            break;

      case embed_time:
      case embed_date:
      case embed_cur_page_num:
      case embed_cur_sect_num:
           break;
   }
   
   DisposeNonNilMemory(item->rtf_text_data);
}


/* standard_embed_copy is the standard function to copy an
embedded reference. */

static void standard_embed_copy (pg_embed_ptr item, long the_type)
{
    switch (the_type) {

      case embed_control:
#ifdef MAC_PLATFORM
       HandToHand((Handle *)&item->data);
         break;
#endif

      case embed_mac_pict:
#ifdef MAC_PLATFORM
       HandToHand((Handle *)&item->data);
#else
         
#endif
         break;
        
        case embed_meta_file:
#ifdef WINDOWS_PLATFORM
         {
            long     meta;
            
            meta = (long)item->data;
            meta = PG_LONGWORD(long)CopyMetaFile((HMETAFILE)meta, NULL);
            item->data = (void PG_FAR *)meta;
         }
#else
         item->data = (void PG_FAR *)MemoryDuplicate((memory_ref)item->data);
#endif
         break;
         
      case embed_bitmap:
      case embed_ole: // PDA:
      case embed_polygon:
      case embed_alternate_char:
      case embed_dynamic_string:
      case embed_user_data:
      case embed_mac_vm_pict:
      case embed_os2_meta_file:
      case embed_dibitmap:
      case embed_wbitmap:
      case embed_gif_pict:
      case embed_jpeg_pict:
      case embed_unsupported_object:
      default:
          if (item->data)
             item->data = (void PG_FAR *)MemoryDuplicate((memory_ref)item->data);
 
         break;

      case embed_bookmark_start:
      case embed_bookmark_end:
            break;

      case embed_qt_movie:
      case embed_qt_flat_movie:
      //ее
            break;

      case embed_time:
        case embed_date:
        case embed_cur_page_num:
        case embed_cur_sect_num:
            break;
   }
   
   if (item->rtf_text_data)
         item->rtf_text_data = MemoryDuplicate(item->rtf_text_data);
}

/* standard_embed_click is the default click function. We only call this,
however, if the click was truly within the item AND for mouse-down only. */

static long standard_embed_click (paige_rec_ptr pg, pg_embed_ptr item,
      long item_type, pg_embed_click_ptr click_info)
{
   style_info_ptr    embed_style;
   t_select_ptr      selection;

   embed_style = item->style;
   selection = click_info->first_select;

   switch (item_type) {
            
      case embed_bookmark_start:
      case embed_bookmark_end:
//       pgSetSelection(pg->myself, selection->control_offset,
//             selection->control_offset + EMBED_DEFAULT_SIZE, 0, TRUE);

         return   embed_style->embed_id;
         
      case embed_control:
         return   embed_style->embed_id;
      
      default:
         pgSetSelection(pg->myself, selection->control_offset,
               selection->control_offset + 2, 0, TRUE);

         return   embed_style->embed_id;
   }

   return      0;
}


/* initialize_embed_data gets called when a new embedded object is created and
the given data is non-NULL. If vert_pos is non-zero, the descent value of the item is
computed as follows: if negative, the absolute value is determined and that many pixels
is used for the descent. If positive, the value is considered a percentage of the total height
(for example, if vert_pos is 0x000A0000 then the descent is height * 0.10). */

static void initialize_embed_data (pgm_globals_ptr mem_globals, embed_ref the_ref,
      void PG_FAR *data, pg_fixed vert_pos)
{
   rectangle         rect;
   memory_ref        data_ref;
   pg_embed_ptr      embed_ptr;
   pg_char_ptr       alt_char_ptr;
   pg_bitmap_ptr     bitmap_ptr;
   pg_poly_ptr       poly_ptr;
   long               data_size, the_type;
   
   embed_ptr = (pg_embed_ptr) UseMemory(the_ref);
   the_type = embed_ptr->type & EMBED_TYPE_MASK;
   if (the_type == embed_alternate_char || the_type == embed_dynamic_string)
      embed_ptr->type |= USE_TEXT_HEIGHT;
   
   if (data) {

      pgFillBlock(&rect, sizeof(rectangle), 0);
   
      switch (the_type) {
   
         case embed_oval:
         case embed_roundrectangle:
         case embed_rectangle:
         case embed_user_box:
            pgBlockMove(data, &rect, sizeof(rectangle));
            pgBlockMove(&rect, embed_ptr->uu.alt_data, sizeof(rectangle));
            
            if ((embed_ptr->type & EMBED_TYPE_MASK) == embed_roundrectangle)
               if (!embed_ptr->modifier)
                  embed_ptr->modifier = 16;
   
            break;
   
         case embed_control:
#ifdef MAC_PLATFORM
         {
            ControlHandle        ctl;
   
            embed_ptr->data = data;
            ctl = (ControlHandle)data;
            detach_control_from_window(ctl);
            RectToRectangle(&(**ctl).contrlRect, &rect);
         }
#endif

            break;
         
         case embed_ole: // PDA:
            break;
         case embed_user_data:
            if (embed_ptr->modifier) {
   
               embed_ptr->data = (void PG_FAR *)MemoryAlloc(mem_globals, 1, embed_ptr->modifier, 0);
               pgBlockMove(data, UseMemory((memory_ref)embed_ptr->data), embed_ptr->modifier);
               UnuseMemory((memory_ref)embed_ptr->data);
            }
            
            break;
  
         case embed_unsupported_object:
            embed_ptr->data = data;
         embed_ptr->modifier = GetMemorySize((memory_ref)data);
         break;

         case embed_bookmark_start:
         case embed_bookmark_end:
         case embed_alternate_char:
         case embed_dynamic_string:
            alt_char_ptr = (pg_char_ptr)data;
            data_size = pgCStrLength((pg_c_string_ptr)alt_char_ptr);
            set_string_data(mem_globals, embed_ptr, the_type, alt_char_ptr, (short)data_size);
            break;
   
         case embed_bitmap:
            bitmap_ptr = (pg_bitmap_ptr)data;
            rect.bot_right.h = bitmap_ptr->width;
            rect.bot_right.v = bitmap_ptr->height;
   
            data_size = (bitmap_ptr->row_bytes * bitmap_ptr->height)
                  + (sizeof(pg_bitmap_rec) - 2);
   
            data_ref = MemoryAlloc(mem_globals, 1, data_size, 0);
            pgBlockMove(data, UseMemory(data_ref), data_size);
            UnuseMemory(data_ref);
            
            embed_ptr->data = (void PG_FAR *)data_ref;
            break;
            
         case embed_polygon:
            poly_ptr = (pg_poly_ptr) UseMemory((memory_ref)data);
            rect.bot_right.h = poly_ptr->width;
            rect.bot_right.v = poly_ptr->height;
            UnuseMemory((memory_ref)data);
   
            embed_ptr->data = data;
            break;

         case embed_mac_pict:

#ifdef MAC_PLATFORM
         {
            PicHandle         pict;
   
            embed_ptr->data = data;
            pict = (PicHandle)data;
            RectToRectangle(&(**pict).picFrame, &rect);
         }
#else
         {
            embed_ptr->data = data;
            RectToRectangle(&((Mac_PicPtr)UseMemory((memory_ref)data))->picFrame, &rect);
            UnuseMemory((memory_ref)data);
         }
#endif
		    embed_ptr->uu.pict_data.pict_frame = rect;
            break;
      
         case embed_mac_vm_pict:
#ifdef MAC_PLATFORM
         {
            embed_ptr->data = data;
            RectToRectangle(&((PicPtr)UseMemory((memory_ref)data))->picFrame, &rect);
            UnuseMemory((memory_ref)data);
         }
#else
         {
            embed_ptr->data = data;
            RectToRectangle(&((Mac_PicPtr)UseMemory((memory_ref)data))->picFrame, &rect);
            UnuseMemory((memory_ref)data);
         }
#endif
		    embed_ptr->uu.pict_data.pict_frame = rect;
            break;

         case embed_meta_file:
           {
               metafile_ptr         metafile;

               metafile = (metafile_ptr)data;
               embed_ptr->data = (void PG_FAR *)metafile->metafile;
               rect = metafile->bounds;
               embed_ptr->modifier = rect.top_left.h << 16;
               embed_ptr->modifier |= rect.top_left.v;
               embed_ptr->uu.pict_data.mapping_mode = metafile->mapping_mode;
               embed_ptr->uu.pict_data.meta_ext_x = metafile->x_ext;
               embed_ptr->uu.pict_data.meta_ext_y = metafile->y_ext;
            
               if (pgEmptyRect(&rect)) {
                  
                  rect.bot_right.v =  DEF_EMBED_HEIGHT;
                  rect.bot_right.h = DEF_EMBED_WIDTH;
               }
           }
		   embed_ptr->uu.pict_data.pict_frame = rect;
           break;

          case embed_url_image:
          {
            pg_url_image_ptr     image_ptr;
            
            image_ptr = (pg_url_image_ptr) UseMemory((memory_ref)data);
            rect.bot_right.h = image_ptr->source_width;
            rect.bot_right.v = image_ptr->source_height;
            
          #ifdef MAC_PLATFORM
            image_ptr->type_and_flags |= embed_mac_pict;
          #endif
          
          #ifdef WINDOW_PLATFORM
            image_ptr->type_and_flags |= embed_meta_file;
          #endif
         
         UnuseMemory((memory_ref)data);
         embed_ptr->data = data;
          }
          embed_ptr->uu.pict_data.pict_frame = rect;
            break;
         
         case embed_os2_meta_file:
            break;
   
         case embed_dibitmap:
            break;
   
         case embed_wbitmap:
            break;
   
         case embed_gif_pict:
#ifdef USING_GIF
            embed_ptr->data = data;
            pgGetGIFRect((memory_ref)data, &rect);
            embed_ptr->uu.pict_data.pict_frame = rect;
#endif
            break;

         case embed_jpeg_pict:
            break;
         
         case embed_qt_movie:
#ifdef MAC_PLATFORM
#endif
#ifdef WINDOWS_PLATFORM
#endif
#ifdef UNIX_PLATFORM
#endif
#ifdef OS2_PLATFORM
#endif
            break;

         case embed_qt_flat_movie:
            break;

         case embed_time:
           case embed_date:
           case embed_cur_page_num:
           case embed_cur_sect_num:
            embed_ptr->type |= USE_TEXT_HEIGHT;
            embed_ptr->data = data;
            break;
         
         case embed_pg_link:
            break;
   
         case embed_useritem:
            embed_ptr->data = data;
            break;
   
      }
   
      if (!pgEmptyRect(&rect)) {
      
         pgOffsetRect(&rect, -rect.top_left.h, -rect.top_left.v);
         embed_ptr->width = rect.bot_right.h;
         embed_ptr->height = rect.bot_right.v;
         
         if ((long)vert_pos < 0)
            embed_ptr->descent = pgHiWord(pgRoundFixed(-vert_pos));
         else {
            pg_fixed    percent;
            
            percent = pgDivideFixed(vert_pos, 0x00640000);
            embed_ptr->descent = pgHiWord(pgRoundFixed(pgMultiplyFixed(embed_ptr->height << 16, percent)));
         }
         
         embed_ptr->left_extra = embed_ptr->right_extra = 1;
         embed_ptr->bot_extra = embed_ptr->top_extra = 1;
      }
   }
   
   UnuseMemory(the_ref);
}



/* set_string_data places the data for string type(s) into the embed ref. */

static void set_string_data (pgm_globals_ptr mem_globals, pg_embed_ptr embed_ptr, long the_type,
      pg_char_ptr alt_char_ptr, short str_size)
{
   short       data_size, max_size;

   data_size = max_size = (str_size + 1);
   
   if (the_type == embed_dynamic_string)
      max_size = (pgLoWord(embed_ptr->modifier)) + 1;

   if (max_size < ALT_SIZE) {

      if (embed_ptr->data)
      {
         DisposeMemory((memory_ref)embed_ptr->data);
         embed_ptr->data = MEM_NULL;
      }

      if (alt_char_ptr) {
      
         pgFillBlock(&embed_ptr->uu.alt_data, sizeof(pg_char), 0);
         pgBlockMove(alt_char_ptr, &embed_ptr->uu.alt_data, data_size * sizeof(pg_char));
      }
   }
   else {
      memory_ref        data_ref;

      data_ref = MemoryAllocClear(mem_globals, 1, max_size, 0);
      
      if (alt_char_ptr) {
      
         pgBlockMove(alt_char_ptr, UseMemory(data_ref), data_size);
         UnuseMemory(data_ref);
      }

      embed_ptr->data = (void PG_FAR *)data_ref;
   }
}


/* get_visual_frame returns the physical bounds of embed_ptr -- same frame that
is drawn on screen. */

static void get_visual_frame (paige_rec_ptr pg, pg_embed_ptr embed_ptr,
      select_pair_ptr range, rectangle_ptr bounds)
{
   text_block_ptr block;
   point_start_ptr   starts;
   co_ordinate    scroll_pos;
   t_select    associated_pt;
   short       baseline;

   associated_pt.offset = range->begin;
   associated_pt.flags = 0;
   pgCalcSelect(pg, &associated_pt);
   block = pgFindTextBlock(pg, associated_pt.offset, NULL, FALSE, FALSE);
   starts = (point_start_ptr) UseMemoryRecord(block->lines, (long)associated_pt.line, 0, TRUE);
   *bounds = starts->bounds;
   baseline = starts->baseline;
   UnuseMemory(block->lines);
   UnuseMemory(pg->t_blocks);
   
   scroll_pos = pg->scroll_pos;
   pgNegatePt(&scroll_pos);
   pgAddPt(&pg->port.origin, &scroll_pos);
   pgOffsetRect(bounds, scroll_pos.h, scroll_pos.v);

   if (!(embed_ptr->type & USE_TEXT_HEIGHT)) {
   
      bounds->bot_right.v -= baseline;
      bounds->bot_right.v += embed_ptr->descent;
      bounds->top_left.v = bounds->bot_right.v - embed_ptr->height;
   }

   bounds->top_left.h += embed_ptr->left_extra;
   bounds->top_left.v += embed_ptr->top_extra;
   bounds->bot_right.h -= embed_ptr->right_extra;
   bounds->bot_right.v -= embed_ptr->bot_extra;
   
   if (pg->port.scale.scale) {
      
      scroll_pos = pg->scroll_pos;
      pgNegatePt(&scroll_pos);
      pgScaleRect(&pg->port.scale, &scroll_pos, bounds);
   }
}




/* The following function returns the total width of all trailing spaces at
the end of the text. */

static long extra_trailing_spaces (paige_rec_ptr pg, style_walk_ptr walker,
      pg_char_ptr text, long length)
{
   long        index, actual_charbytes;
   
   index = length;
   actual_charbytes = walker->cur_style->char_bytes;
   walker->cur_style->char_bytes = 0;

   while (index) {
      
      // Note, I call standard proc because caller wants "real" text measured

      --index;

      if (!pgCharInfoProc(pg, walker, text, NO_BLOCK_OFFSET, 0, length, index, BLANK_BIT)) {
         
         ++index;
         break;
      }
   }
   
   walker->cur_style->char_bytes = (short)actual_charbytes;

   if (index < length)
      return (long)pgTextWidth(&pg->port, &text[index], (short)(length - index));
   
   return   0;
}


/* have_saved_ref if the same exact embed_ref exists before stylebase[index].
This function does not get called if there is no embed_ref in stylebase[index]. */

static pg_boolean have_saved_ref (style_info_ptr stylebase, short current_index)
{
   register style_info_ptr    styles;
   memory_ref              	  current_embed; 
   short                	  index;

   if (!current_index)
      return   FALSE;
   
   styles = stylebase;
   current_embed = styles[current_index].embed_object;

   for (index = 0; index < current_index; ++index)
      if (styles[index].embed_object == current_embed)
         return   TRUE;
   
   return   FALSE;
}

/* find_url_ref returns the index number of the url. Returns zero if none. */

static long find_url_ref (memory_ref listref, memory_ref url)
{
   long           num_entries, index;
   memory_ref PG_FAR *list;
   
   if (listref == MEM_NULL)
      return   0;

   list = (memory_ref *) UseMemory(listref);
   num_entries = GetMemorySize(listref);
   
   for (index = 0; index < num_entries; ++index)
      if (list[index] == url) {
      
      UnuseMemory(listref);
      
      return   (index + 1);
   }
   
   UnuseMemory(listref);
   
   return   0;
}  


/* custom_data_callback calls the app's callback function to setup data to save to a file. */

static void custom_data_callback (paige_rec_ptr pg, pack_walk_ptr walker, pg_embed_ptr embed_ptr,
      embed_callback callback, memory_ref buffer_ref)
{
   long        saved_size;

   SetMemorySize(buffer_ref, 0);
   callback(pg, embed_ptr, embed_ptr->type & EMBED_TYPE_MASK, EMBED_WRITE_DATA,
         embed_ptr->user_refcon, (void*)buffer_ref, 0);
   
   if (saved_size = GetMemorySize(buffer_ref)) {

      pgPackBytes(walker, (pg_bits8_ptr) UseMemory(buffer_ref), saved_size);
      UnuseMemory(buffer_ref);
   }  
}


/* compare_embed_data compares data together (but only alt_data). */

static pg_boolean compare_embed_data (pg_embed_ptr compare_to, pg_embed_ptr compare_from)
{
   register long        data_size;
   register pg_char_ptr source_ptr, target_ptr;
   
   source_ptr = compare_from->uu.alt_data;
   target_ptr = compare_to->uu.alt_data;
   
   for (data_size = ALT_SIZE; data_size; --data_size)
      if (*source_ptr++ != *target_ptr++)
         return   FALSE;
   
   return   TRUE;
}


/* Orients as best as possible an unknown embedded type's text */

static co_ordinate_ptr center_unknown_text(pg_embed_ptr item, graf_device_ptr port, pg_char_ptr text, short length, rectangle_ptr draw_bounds, co_ordinate_ptr top_left_pt)
{
   short width;
   short ascent;
   
#ifdef MAC_PLATFORM
   FontInfo          f_info;

   GetFontInfo(&f_info);
   ascent = f_info.leading;
#endif

#ifdef WINDOWS_PLATFORM
    ascent = 0;
#endif

   width = pgTextWidth (port, text, length);

   top_left_pt->h = (draw_bounds->top_left.h + ((draw_bounds->bot_right.h - draw_bounds->top_left.h) >> 1)) - (width >> 1);
   top_left_pt->v = (draw_bounds->top_left.v + ((draw_bounds->bot_right.v - draw_bounds->top_left.v) >> 1)) - ascent;
   
   return top_left_pt;
}


/* Pack the extra picture information for output */

static void pack_pic_embed(pack_walk_ptr walker, pg_embed_ptr embed_ptr)
{
   pgPackNum(walker, short_data, (long)embed_ptr->uu.pict_data.pict_transparent);
   pgPackNum(walker, short_data, (long)embed_ptr->uu.pict_data.scale_pict);
   pgPackNum(walker, long_data, embed_ptr->uu.pict_data.vert_scale);
   pgPackNum(walker, long_data, embed_ptr->uu.pict_data.horiz_scale);
   pgPackRect(walker, &embed_ptr->uu.pict_data.pict_frame);
   pgPackNum(walker, long_data, embed_ptr->uu.pict_data.mapping_mode);
   pgPackNum(walker, short_data, embed_ptr->uu.pict_data.twips_width);
   pgPackNum(walker, short_data, embed_ptr->uu.pict_data.twips_height);
   pgPackNum(walker, short_data, embed_ptr->uu.pict_data.twips_gwidth);
   pgPackNum(walker, short_data, embed_ptr->uu.pict_data.twips_gheight);
   pgPackNum(walker, short_data, embed_ptr->uu.pict_data.meta_ext_x);
   pgPackNum(walker, short_data, embed_ptr->uu.pict_data.meta_ext_y);
}

/* Unpack the extra picture information for input */

static void unpack_pic_embed(pack_walk_ptr walker, pg_embed_ptr embed_ptr)
{
   if (embed_ptr->version > EMBED_REVISION_1)
   {
      embed_ptr->uu.pict_data.pict_transparent = (short)pgUnpackNum(walker);
      embed_ptr->uu.pict_data.scale_pict = (short)pgUnpackNum(walker);
      embed_ptr->uu.pict_data.vert_scale = pgUnpackNum(walker);
      embed_ptr->uu.pict_data.horiz_scale = pgUnpackNum(walker);
      pgUnpackRect(walker, &embed_ptr->uu.pict_data.pict_frame);
      
      if (embed_ptr->version > EMBED_REVISION_2) {

         embed_ptr->uu.pict_data.mapping_mode = pgUnpackNum(walker);
         embed_ptr->uu.pict_data.twips_width = (short) pgUnpackNum(walker);
         embed_ptr->uu.pict_data.twips_height = (short) pgUnpackNum(walker);
         embed_ptr->uu.pict_data.twips_gwidth = (short) pgUnpackNum(walker);
         embed_ptr->uu.pict_data.twips_gheight = (short) pgUnpackNum(walker);
      }
      
      if (embed_ptr->version > EMBED_REVISION_3) {
          
          embed_ptr->uu.pict_data.meta_ext_x = (short)pgUnpackNum(walker);
          embed_ptr->uu.pict_data.meta_ext_y = (short)pgUnpackNum(walker);
      }
   }
   
	if (pgEmptyRect(&embed_ptr->uu.pict_data.pict_frame)) {
		
		pgFillBlock(&embed_ptr->uu.pict_data.pict_frame, sizeof(rectangle), 0);
		embed_ptr->uu.pict_data.pict_frame.bot_right.h = embed_ptr->width;
		embed_ptr->uu.pict_data.pict_frame.bot_right.v = embed_ptr->height;
	}
}

/* pack_url saves a url record. */

static void pack_url (pack_walk_ptr walker, memory_ref url_ref)
{
   pg_url_image_ptr     image_ptr;
   long              url_string_size, alt_string_size;
   long              effective_type;

   image_ptr = (pg_url_image_ptr) UseMemory(url_ref);
   url_string_size = pgCStrLength(image_ptr->URL);
   alt_string_size = pgCStrLength(image_ptr->alt_string);

   pgPackNum(walker, short_data, (short)url_string_size);
   pgPackNum(walker, short_data, (short)alt_string_size);

   if (url_string_size)
       pgPackUnicodeBytes(walker, (pg_bits8_ptr)image_ptr->URL, url_string_size * sizeof(pg_char), FALSE);
   if (alt_string_size)
       pgPackUnicodeBytes(walker, (pg_bits8_ptr)image_ptr->alt_string, alt_string_size * sizeof(pg_char), FALSE);

   pgPackNum(walker, short_data, image_ptr->source_width);
   pgPackNum(walker, short_data, image_ptr->source_height);
   pgPackNum(walker, long_data, image_ptr->type_and_flags);
   pgPackNum(walker, short_data, image_ptr->source_type);
   pgPackNum(walker, long_data, image_ptr->machine_ref1);
   pgPackNum(walker, long_data, image_ptr->machine_ref2);

   effective_type = image_ptr->type_and_flags & EMBED_TYPE_MASK;
   
   if (effective_type == embed_mac_pict || effective_type == embed_meta_file)
      image_ptr->image_data = (generic_var)pack_graphics(walker, (short)effective_type, (void PG_FAR *)image_ptr->image_data);

   UnuseMemory(url_ref);
}

/* unpack_url does the reverse of the above. */

static void unpack_url (paige_rec_ptr pg, pack_walk_ptr walker)
{
   pg_url_image_ptr     image_ptr;
   pgm_globals_ptr         mem_globals;
   memory_ref           PG_FAR *list;
   memory_ref           url_ref;
   long              url_string_size, alt_string_size;
   long              effective_type;
   
   mem_globals = pg->globals->mem_globals;
   mem_globals->next_mem_id += 1;
   url_ref = MemoryAllocClearID(pg->globals->mem_globals, sizeof(pg_url_image), 1, 0, mem_globals->next_mem_id);
   
   list = (memory_ref *) AppendMemory(pg->url_list_ref, 1, FALSE);
   *list = url_ref;
   UnuseMemory(pg->url_list_ref);

   image_ptr = (pg_url_image_ptr) UseMemory(url_ref);
   url_string_size = pgUnpackNum(walker);
   alt_string_size = pgUnpackNum(walker);

   if (url_string_size)
      unpack_altsize_text(pg, walker, image_ptr->URL, url_string_size);
      
   if (alt_string_size)
      unpack_altsize_text(pg, walker, image_ptr->alt_string, alt_string_size);
   
   image_ptr->source_width = (short)pgUnpackNum(walker);
   image_ptr->source_height = (short)pgUnpackNum(walker);
   image_ptr->type_and_flags = pgUnpackNum(walker);
   image_ptr->source_type = (short)pgUnpackNum(walker);
   image_ptr->machine_ref1 = pgUnpackNum(walker);
   image_ptr->machine_ref2 = pgUnpackNum(walker);

   effective_type = image_ptr->type_and_flags & EMBED_TYPE_MASK;
   
   if (effective_type == embed_mac_pict || effective_type == embed_meta_file)
      image_ptr->image_data = (generic_var)unpack_graphics(mem_globals, walker, effective_type);

   UnuseMemory(url_ref);
}

/* convert_old_borders converts the older border struct to the new one. */

static void convert_old_borders (pg_embed_ptr embed_ptr, old_pg_border PG_FAR *old_border)
{
	long		pensize;

	if (!old_border->type)
		return;

	pensize = old_border->pen_size;
	
	if (pensize == 0)
		pensize = 1;
	else
	if (pensize > 7)
		pensize = 7;

    embed_ptr->border_color = pgRGBToLong(&old_border->color);

	switch (old_border->style) {

		case border_double:
			pensize |= PG_BORDER_DOUBLE;
			break;
			
		case border_dotted:
		case border_dashed:
			pensize |= PG_BORDER_DOTTED;
			break;

		case border_shadow:
			pensize |= PG_BORDER_SHADOW;
			break;
	}

	switch (old_border->type) {

		case border_box:
		 	 embed_ptr->border_info = (pensize << 24) | (pensize << 16) | (pensize << 8) | pensize;
		 	 break;

		case border_top:
		 	 embed_ptr->border_info = pensize << 16;
		 	 break;

		case border_bottom:
		 	 embed_ptr->border_info = pensize << 24;
		 	 break;

		case border_left:
		 	 embed_ptr->border_info = pensize;
		 	 break;

		case border_right:
		 	 embed_ptr->border_info = pensize << 8;
		 	 break;
	}
}


/* unpack_altsize_text reads an ALT_SIZE string and makes necessary Unicode conversions. */

static void unpack_altsize_text (paige_rec_ptr pg, pack_walk_ptr walker, pg_char_ptr str, long size)
{
#ifdef UNICODE

   pgUnpackPtrBytes(walker, (pg_bits8_ptr)str);
   
   if (pg->flags2 & UNICODE_SAVED)
      pgUnicodeToUnicode((pg_short_t PG_FAR *)str, size + 1, FALSE);
   else
      pgBytesToUnicode((pg_bits8_ptr)str, (pg_short_t*)str, NULL, size);
#else
   if (pg->flags2 & UNICODE_SAVED) {
      pg_char     temp_name[256];
      
      pgFillBlock(temp_name, (size * 2) + 2, 0);
      pgUnpackPtrBytes(walker, temp_name);
      
      pgUnicodeToUnicode((pg_short_t PG_FAR *)temp_name, size + 1, FALSE);
      pgUnicodeToBytes((pg_short_t PG_FAR *)temp_name, temp_name, NULL, size);
      pgBlockMove(temp_name, str, size);
   }
   else 
      pgUnpackPtrBytes(walker, str);
#endif
}


/************************** Windows-specific functions **************************/

#ifdef WINDOWS_PLATFORM

int CALLBACK metafile_enum_proc (HDC hdc, HANDLETABLE FAR *handles, METARECORD FAR *rec,
        int num_objects, long refcon);

static void PG_FAR * pack_graphics (pack_walk_ptr walker, short the_type, void PG_FAR *data)
{
    HANDLE        metabits;
    long          meta, metasize;
    void PG_FAR      *metaptr;
   void PG_FAR    *data_result;

   if (the_type == embed_meta_file) {
      
      data_result = data;
         meta = (long)data;          

#ifndef WIN16_COMPILE
            
       if (metasize = GetMetaFileBitsEx((HMETAFILE)meta, 0, NULL)) {
       
          metabits = GlobalAlloc(GMEM_MOVEABLE, metasize);
          metaptr = GlobalLock(metabits);
          GetMetaFileBitsEx((HMETAFILE)meta, metasize, (LPVOID)metaptr);
          pgPackBytes(walker, (pg_bits8_ptr)metaptr, metasize);
          GlobalUnlock(metabits);
          GlobalFree(metabits);
       }
#else    
       metabits = (HANDLE)GetMetaFileBits((HMETAFILE)meta);
       metasize = GlobalSize(metabits);
       metaptr = GlobalLock(metabits);
       pgPackBytes(walker, (pg_bits8_ptr)metaptr, metasize);
       GlobalUnlock(metabits);
       data_result = (void PG_FAR *)SetMetaFileBits(metabits);
#endif
   } else {
   
        pgPackBytes(walker, (pg_bits8_ptr) UseMemory((memory_ref)data), GetByteSize((memory_ref)data));
        UnuseMemory((memory_ref)data);
    }
    
    return     data_result;
}



static generic_var unpack_graphics (pgm_globals_ptr mem_globals, pack_walk_ptr walker, long type) {
   generic_var    result;
   long        input_byte_size;

   if (type == embed_meta_file) {
      
      pgGetUnpackedPtr(walker, &input_byte_size);

        if (input_byte_size) {
           HANDLE         metabits;
           void PG_FAR    *metaptr;
           
           if (metabits = GlobalAlloc(GMEM_MOVEABLE, input_byte_size)) {
              
              metaptr = GlobalLock(metabits);
              pgUnpackPtrBytes(walker, (pg_bits8_ptr)metaptr);

#ifndef WIN16_COMPILE
              result = (generic_var)SetMetaFileBitsEx((UINT)input_byte_size, (BYTE PG_FAR *)metaptr);
              GlobalUnlock(metabits);
              GlobalFree(metabits);
#else
              GlobalUnlock(metabits);
              result = (generic_var)(void PG_FAR*)SetMetaFileBits(metabits);
#endif
           }
       }
   }
   else {

      pgGetUnpackedPtr(walker, &input_byte_size);
        result = (generic_var)MemoryAlloc(mem_globals, 1, input_byte_size, 0);
        pgUnpackBytes(walker, (memory_ref)result);
   }
   
   return   result;
}

/* draw_meta_file plays a meta file into a pre-created bitmap, then stamps those bits to
the target DC. The purpose of this method is to render all meta files the same regardless
of mapping mode, viewport extend and origin. */


static void draw_meta_file (paige_rec_ptr pg, memory_ref the_data, HDC target_dc, pg_embed_ptr embed_ptr, RECT PG_FAR *target_rect)
{
   int map_mode;
   int dc_state = SaveDC(target_dc);

   if((map_mode = (int)embed_ptr->uu.pict_data.mapping_mode) == 0)
      map_mode = MM_TEXT;

   if(map_mode == MM_TEXT)
   {
      POINT       old_org;       
      
      SetWindowOrgEx(target_dc, -target_rect->left, -target_rect->top, &old_org);
      EnumMetaFile(target_dc, (HMETAFILE)(long)the_data,
                   (MFENUMPROC)metafile_enum_proc, 0);
      SetWindowOrgEx(target_dc, old_org.x, old_org.y, NULL);
   }
   else
   {
      rectangle r;
        RECT rect;
      pg_scale_factor scale = {{0, 0}, 0};

      SetMapMode(target_dc, map_mode);
      if(pg->flags & PRINT_MODE_BIT)
         scale.scale = ((long)HIWORD(pg->resolution)) +
                       ((long)pg->port.resolution << 16);
      else
         scale = pg->port.scale;

      RectToRectangle(target_rect, &r);
      pgScaleRect(&scale, NULL, &r);
      RectangleToRect(&r, NULL, &rect);

      SetViewportExtEx(target_dc, rect.right - rect.left, rect.bottom - rect.top, NULL);
      SetViewportOrgEx(target_dc, rect.left, rect.top, NULL);

      PlayMetaFile(target_dc, (HMETAFILE)(long)the_data);
   }
   
   RestoreDC(target_dc, dc_state);
}

int CALLBACK metafile_enum_proc (HDC hdc, HANDLETABLE FAR *handles, METARECORD FAR *rec,
        int num_objects, long refcon)
{
   switch (rec->rdFunction) {

      case META_SETWINDOWORG:
      case META_SETWINDOWEXT:
      case META_SETVIEWPORTORG:
      case META_SETVIEWPORTEXT:
      case META_OFFSETWINDOWORG:
      case META_SCALEWINDOWEXT:
      case META_OFFSETVIEWPORTORG:
      case META_SCALEVIEWPORTEXT:
      case META_SETMAPMODE:
         break;
      
      default:
          PlayMetaFileRecord(hdc, handles, rec, (UINT)num_objects);
          break;
    }
 
   return   TRUE;
}

#endif
// End of Windows-specif functions


/************************ Macintosh-specific functions **************************/

#ifdef MAC_PLATFORM

static void * pack_graphics (pack_walk_ptr walker, short the_type, void PG_FAR *data)
{
   if (the_type == embed_mac_pict) {
        Handle         the_pict;
        
        the_pict = (Handle)data;
        HLock(the_pict);
        pgPackBytes(walker, (pg_bits8_ptr)*the_pict, GetHandleSize(the_pict));
        HUnlock(the_pict);
    }
    else {
        pgPackBytes(walker, UseMemory((memory_ref)data),
              GetByteSize((memory_ref)data));
        UnuseMemory((memory_ref)data);
    }
    
    return  data;
}

static generic_var unpack_graphics (pgm_globals_ptr mem_globals, pack_walk_ptr walker, long type)
{
   generic_var    result;
   long        input_byte_size;

   result = (generic_var)0;
   
   if (type == embed_mac_pict) {
      
      pgGetUnpackedPtr(walker, &input_byte_size);
      
        if (input_byte_size) {
           Handle         the_pict;
           
          the_pict = NewHandle(input_byte_size);
          pgFailNIL(mem_globals, the_pict);
           
          result = (generic_var)the_pict;
          HLock(the_pict);
          pgUnpackPtrBytes(walker, (pg_bits8_ptr)*the_pict);
          HUnlock(the_pict);
        }
    }
   else {
        pgGetUnpackedPtr(walker, &input_byte_size);
        result = (generic_var)MemoryAlloc(mem_globals, 1, input_byte_size, 0);
        pgUnpackBytes(walker, (memory_ref)result);
   }
   
   return   result;
}


/* detach_control_from_window detaches the ControlHandle from the Window it
was created it. */

static void detach_control_from_window (ControlHandle ctl)
{
   WindowPeek        owner;
   ControlHandle     next_control, previous_control;
   
   if (!(owner = (WindowPeek) (**ctl).contrlOwner))
      return;

   next_control = owner->controlList;
   previous_control = NULL;

   while (next_control) {
      
      if (next_control == ctl) {
         
         if (previous_control)
            (**previous_control).nextControl = (**next_control).nextControl;
         else
            owner->controlList = (**next_control).nextControl;
         break;
      }
      
      previous_control = next_control;
      next_control = (**next_control).nextControl;
   }
}


#endif
// End of Mac-specific functions


