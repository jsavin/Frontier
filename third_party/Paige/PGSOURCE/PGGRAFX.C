/* This file contains machine-specific code to satisfy the set of portable
graphics commands that are prototyped in pgGrafx.h.

Copyright 1993 by DataPak Software, Inc.  All rights reserved. It is illegal
to remove this notice   */

/* 12/07/94 Corrected and improved by board member, TR Shaw, OITC. Improvements and
fixes are copyrighted 1994 OITC, Inc. */

#include "Paige.h"  

#ifdef MAC_PLATFORM
#pragma segment pgbasic4   
#include <qdoffscreen.h>
#endif

#include "pgTraps.h"
#include "pgRegion.h"
#include "pgUtils.h"
#include "machine.h"
#include "pgExceps.h"
#include "pgFiles.h"
#include "pgGrafx.h"             
#include "pgErrors.h"


#ifdef WINDOWS_PLATFORM
#include <math.h>
#include <string.h>
#endif

static void save_poly_points (poly_ref poly, co_ordinate_ptr first_pt, co_ordinate_ptr second_pt);
static void frame_poly (graf_device_ptr port, pg_poly_ptr poly_ptr,
      rectangle_ptr target_frame, short pen_size);
static void pack_poly_rec (pack_walk_ptr walker, pg_poly_ptr poly);
static void unpack_poly_rec (pack_walk_ptr walker, pg_poly_ptr poly);


static void draw_segment(pg_globals_ptr globals, pg_char type, pg_embed_ptr item, rectangle_ptr draw_bounds);
static void draw_line(pg_char style, co_ordinate_ptr start_pt, co_ordinate_ptr end_pt);


/* pgTextOut draws text for length bytes in current style to port device. */

PG_PASCAL (void) pgTextOut (graf_device_ptr port, co_ordinate_ptr top_left_pt,
      pg_char_ptr text, short length, short optional_point_size)
{
#ifdef MAC_PLATFORM
   FontInfo       info;
   GrafPtr           current_port, draw_port;
   short          cur_textsize;

   if (port->poly_save)
      return;
   
   GetPort(&current_port);
   draw_port = (GrafPtr)port->machine_var;
   SetPort(draw_port);
   
   cur_textsize = draw_port->txSize;

   if (optional_point_size)
      TextSize(optional_point_size);

   GetFontInfo(&info);
   MoveTo(top_left_pt->h, top_left_pt->v + info.ascent);
   DrawText(text, 0, length);
   
   draw_port->txSize = cur_textsize;

   SetPort(current_port);

#endif

}


/* pgTextWidth returns the text width for text of length bytes */

PG_PASCAL (short) pgTextWidth (graf_device_ptr port, pg_char_ptr text, short length)
{
   short       result;

#ifdef WINDOWS_PLATFORM
   HDC           hdc = pgGetPlatformDevice(port);

   result = GetTextWidth(hdc, (PGSTR)text, length);
   pgReleasePlatformDevice(port);
#endif

#ifdef MAC_PLATFORM
   result = TextWidth((Ptr)text, 0, length);
#endif
   return  result;
}


/* pgLineDraw draws a line from start_pt to end_pt of pen_width/pen_height
pen size.  Only straight lines are drawn. */

PG_PASCAL (void) pgLineDraw (graf_device_ptr port, co_ordinate_ptr start_pt,
      co_ordinate_ptr end_pt, long color, short pen_size)
{
   short       use_pensize;
   
   if (port->poly_save) {
      
      save_poly_points(port->poly_save, start_pt, end_pt);
      return;
   }

   if (!(use_pensize = pen_size))
      use_pensize = 1;

#ifdef MAC_PLATFORM
   {
      CGrafPtr    current_port;
      RGBColor    old_color, new_color;
      Pattern        pat;

      GetPort((GrafPtr *)&current_port);

      PenNormal();
      
      old_color = current_port->rgbFgColor;
      pgLongToRGB(color, (color_value_ptr)&new_color);
      RGBForeColor(&new_color);
      
      if (color & COLORPAT_DOTTED) {
         
         pat.pat[0] = pat.pat[2] = pat.pat[4] = pat.pat[6] = 0x55;
         pat.pat[1] = pat.pat[3] = pat.pat[5] = pat.pat[7] = 0xAA;
         PenPat(&pat);
      }

      PenSize(use_pensize, use_pensize);
      MoveTo(pgLongToShort(start_pt->h), pgLongToShort(start_pt->v));
      LineTo(pgLongToShort(end_pt->h), pgLongToShort(end_pt->v));
      
      RGBForeColor(&old_color);
   }

#endif


#ifdef WINDOWS_PLATFORM
   {
      HDC            hdc = (HDC)port->machine_ref;
      HBRUSH         brush, old_brush;
      HPEN           pen, old_pen;
      RECT        r;
      
      if (end_pt->h > start_pt->h) {  // horizontal

         r.top = r.bottom = (int)start_pt->v;
         r.left = (int)start_pt->h;
         r.right = (int)end_pt->h;
         
         if (r.left > r.right) {

            r.right = (int)start_pt->h;
            r.left = (int)end_pt->h;
         }
         
      }
      else { // vertical
         
         r.top = (int)start_pt->v;
         r.bottom = (int)end_pt->v;
         
         if (r.top > r.bottom) {

            r.bottom = (int)start_pt->v;
            r.top = (int)end_pt->v;
         }
         
         r.left = r.right = (int)start_pt->h;
      }
      
      r.bottom += use_pensize;
      r.right += use_pensize;

      brush = CreateSolidBrush((COLORREF)color);
      pen = CreatePen(PS_SOLID, 1, (COLORREF)color);
      old_brush = (HBRUSH) SelectObject(hdc, brush);
      old_pen = (HPEN) SelectObject(hdc, pen);
      Rectangle(hdc, r.left, r.top, r.right, r.bottom);
      //FrameRect(hdc, &r, brush); // PDA
      SelectObject(hdc, old_brush);
      SelectObject(hdc, old_pen);
      DeleteObject(brush);
      DeleteObject(pen);
   }
#endif
}


/* pgFrameObject frames an object in the current pen color. The object can
be any of the op code enums from rect to arc (for arc, the angle is based on
a would-be oval of frame).  */

PG_PASCAL (void) pgFrameObject (graf_device_ptr port, rectangle_ptr frame,
      long modifier, short object_verb, short pen_size)
{
   Rect        r;
#ifdef MAC_PLATFORM
   GrafPtr        current_port;
#endif
#ifdef WINDOWS_PLATFORM
   HDC            hdc = (HDC)port->machine_ref;
   HBRUSH         brush, old_brush;
#endif
   short       use_pensize, low_modifier, hi_modifier;

   if (port->poly_save) {
      co_ordinate    first_pt, second_pt;
      
      if (object_verb == object_arc)
         return;
      
      first_pt = frame->top_left;
      second_pt.v = first_pt.v;
      second_pt.h = frame->bot_right.h - pen_size;
      save_poly_points(port->poly_save, &first_pt, &second_pt);  // top-left to top-right
      
      first_pt = second_pt;
      second_pt.v = frame->bot_right.v - pen_size;
      save_poly_points(port->poly_save, &first_pt, &second_pt);  // top-right to bottom-right

      first_pt = second_pt;
      second_pt.h = frame->top_left.h;
      save_poly_points(port->poly_save, &first_pt, &second_pt);  // bottom-right to bottom-left
      
      first_pt = second_pt;
      second_pt = frame->top_left;
      save_poly_points(port->poly_save, &first_pt, &second_pt);  // back to top-left
   
      return;
   }

   low_modifier = pgLoWord(modifier);
   hi_modifier = pgHiWord(modifier);

#ifdef MAC_PLATFORM
   GetPort(&current_port);
   SetPort((GrafPtr)port->machine_var);
#endif

   RectangleToRect(frame, NULL, &r);
   
   if (!(use_pensize = pen_size))
      use_pensize = 1;

#ifdef MAC_PLATFORM
   
   PenSize(use_pensize, use_pensize);

   switch (object_verb) {

      case object_rect:
         FrameRect(&r);
         break;
         
      case object_oval:
         FrameOval(&r);
         break;
         
      case object_roundrect:
         FrameRoundRect(&r, low_modifier, low_modifier);
         break;
      
      case object_arc:
         FrameArc(&r, low_modifier, hi_modifier);
         break;
   }
#endif

#ifdef WINDOWS_PLATFORM

      hdc = (HDC)port->machine_ref;
      brush = CreateSolidBrush((COLORREF)0);
      old_brush = (HBRUSH) SelectObject(hdc, brush);

   switch (object_verb) {

      case object_rect:
         FrameRect(hdc, &r, brush);
         break;
         
      case object_oval:
         break;
         
      case object_roundrect:
         break;
      
      case object_arc:
         break;
   }
   
   SelectObject(hdc, old_brush);
   DeleteObject(brush);
#endif

#ifdef MAC_PLATFORM
   SetPort((GrafPtr)current_port);
#endif
}


/* pgFrameRect frames a rectangle in a specified color. */

PG_PASCAL (void) pgFrameRect (graf_device_ptr port, rectangle_ptr frame,
      long color, short pen_size)
{
   Rect        r;
#ifdef MAC_PLATFORM
   GrafPtr         current_port, draw_port;
   CGrafPtr		   color_port = NULL;
   RGBColor		   old_color, new_color;
#endif
#ifdef WINDOWS_PLATFORM
   HDC            hdc = (HDC)port->machine_ref;
   HBRUSH         brush, old_brush;
#endif
   short       use_pensize;


#ifdef MAC_PLATFORM
   GetPort(&current_port);
   draw_port = (GrafPtr)port->machine_var;
   SetPort(draw_port);

   if (draw_port->portBits.rowBytes & 0xC000) {
   
   		color_port = (CGrafPtr)draw_port;
   		old_color = color_port->rgbFgColor;
   }
   
   PenNormal();
   pgLongToRGB(color, (color_value_ptr)&new_color);
   RGBForeColor(&new_color);
#endif

   RectangleToRect(frame, NULL, &r);
   
   if (!(use_pensize = pen_size))
      use_pensize = 1;

#ifdef MAC_PLATFORM
   
   PenSize(use_pensize, use_pensize);
   FrameRect(&r);
   
   if (color_port)
   		RGBForeColor(&old_color);

   SetPort(current_port);

#endif

#ifdef WINDOWS_PLATFORM

   hdc = (HDC)port->machine_ref;
   brush = CreateSolidBrush((COLORREF)color);
   old_brush = (HBRUSH) SelectObject(hdc, brush);
   FrameRect(hdc, &r, brush);
   SelectObject(hdc, old_brush);
   DeleteObject(brush);
#endif

}

/* pgPaintObject fills an object with the current pen color. The object can
be any of the op code enums from rect to arc (for arc, the angle is based on
a would-be oval of frame).  */

PG_PASCAL (void) pgPaintObject (graf_device_ptr port, rectangle_ptr frame,
      long modifier, short object_verb, long color)
{
   Rect        r;
   short       low_modifier, hi_modifier;
#ifdef MAC_PLATFORM
   CGrafPtr    current_port;
   RGBColor    new_color, old_color;
#endif
#ifdef WINDOWS_COMPILE
   HDC               hdc;
   HBRUSH            brush;
#endif

   if (port->poly_save)
      return;

   low_modifier = pgLoWord(modifier);
   hi_modifier = pgHiWord(modifier);

#ifdef MAC_PLATFORM
   GetPort((GrafPtr *)&current_port);
#endif

   RectangleToRect(frame, NULL, &r);
   
#ifdef MAC_PLATFORM

   PenNormal();
   
   old_color = current_port->rgbFgColor;
   pgLongToRGB(color, (color_value_ptr)&new_color);
   RGBForeColor(&new_color);

   switch (object_verb) {

      case object_rect:
         PaintRect(&r);
         break;
         
      case object_oval:
         PaintOval(&r);
         break;
         
      case object_roundrect:
         PaintRoundRect(&r, low_modifier, hi_modifier);
         break;

      case object_arc:
         PaintArc(&r, low_modifier, hi_modifier);
         break;
   }

   RGBForeColor(&old_color);

#endif

#ifdef WINDOWS_PLATFORM

   hdc = (HDC)port->machine_ref;
   brush = CreateSolidBrush((COLORREF)(color & 0x00FFFFFF));

   switch (object_verb) {

      case object_rect:
         FillRect(hdc, (LPRECT)&r, brush);
         break;
         
      case object_oval:
         break;
         
      case object_roundrect:
         break;

      case object_arc:
         break;
   }
   
   DeleteObject(brush);
#endif
}


/* pgFramePoly draws a frame around the polygon whose target rect is target_frame.
The target frame can be larger or smaller than width or height of original
polygon, hence the graphic can be expanded/shrunk accordingly. */

PG_PASCAL (void) pgFramePoly (graf_device_ptr port, poly_ref poly,
      rectangle_ptr target_frame, short pen_size)
{
   frame_poly(port, (pg_poly_ptr)UseMemory(poly), target_frame, pen_size);
   UnuseMemory(poly);
}


/* pgFillPoly fills the polygon with the current pen color, whose target rect
is target_frame. The target frame can be larger or smaller than width or height
of original polygon, hence the graphic can be expanded/shrunk accordingly. */

PG_PASCAL (void) pgFillPoly (graf_device_ptr port, poly_ref poly,
      rectangle_ptr target_frame)
{
   pg_poly_ptr          poly_ptr;
   
   poly_ptr = (pg_poly_ptr) UseMemory(poly);

#ifdef MAC_PLATFORM
   {
      PolyHandle           poly_h;
      GrafPtr           current_port;

      GetPort(&current_port);
      SetPort((GrafPtr)port->machine_var);

      poly_h = OpenPoly();
      pgFramePoly(port, poly, target_frame, 1);
      ClosePoly();
      
      PaintPoly(poly_h);
      KillPoly(poly_h);
      
      SetPort(current_port);
   }
   
#endif

   UnuseMemory(poly);
}


/* pgCopyBits blits the bit image in bitmap to the drawing device port.
If source_rect is NULL the whole image is copied. The target_rect, however,
must not be NULL. The draw_mode can be any of the "bits_n" draw modes. */

PG_PASCAL (void) pgCopyBits (graf_device_ptr port, pg_bitmap_ptr bitmap,
      rectangle_ptr source_rect, rectangle_ptr target_rect, short draw_mode)
{
   Rect           target_r, source_r;
   long           transfer_mode;
#ifdef MAC_PLATFORM
   RgnHandle       clip_rgn;
   GrafPtr        bits_port;
   
   if (port->poly_save)
      return;

   clip_rgn = pgCreateRgn();
   RectangleToRect(target_rect, NULL, &target_r);
   
   if (source_rect)
      RectangleToRect(source_rect, NULL, &source_r);
   else
      SetRect(&source_r, 0, 0, bitmap->width, bitmap->height);
   
   bits_port = (GrafPtr)port->machine_var;

   pgRectToRgn(clip_rgn, &target_r);
   pgSectRgn(bits_port->visRgn, clip_rgn, clip_rgn);
   pgSectRgn(bits_port->clipRgn, clip_rgn, clip_rgn);

   switch (draw_mode) {
      
      case bits_copy:
         transfer_mode = srcCopy;
         break;
         
      case bits_or:
         transfer_mode = srcOr;
         break;

      case bits_xor:
         transfer_mode = srcXor;
         break;

      default:
         transfer_mode = srcCopy;
         break;
   }
   
   if (bitmap->pixel_size == 1) {
      BitMap        bits;

      SetRect(&bits.bounds, 0, 0, bitmap->width, bitmap->height);
      bits.rowBytes = bitmap->row_bytes;
      bits.baseAddr = (Ptr)bitmap->bits;
      CopyBits(&bits, &bits_port->portBits, &source_r, &target_r, transfer_mode, clip_rgn);
   }
   else {
      //е TRS/OITC
      Rect        src_rect;
      char *         bits_p;
      GWorldPtr      off_g_world = NULL;
      PixMapHandle   pix_map_h;
      unsigned short row_bytes;
#ifdef THINK_C
      //Or any old compiler using old Mac Headers
      char        mode;
#else
      SInt8       mode;
#endif   
      SetRect(&src_rect, 0, 0, bitmap->width, bitmap->height);
      if( !NewGWorld( &off_g_world, bitmap->pixel_size, &src_rect, pgColorValues2CTab(bitmap->palette), NULL, keepLocal ) )
      {
         SysEnvRec   the_world;
         
         SysEnvirons(curSysEnvVers, &the_world);
         if (the_world.systemVersion >= SYSTEM_7_VAL)
            pix_map_h = GetGWorldPixMap( off_g_world );     /*  7.0 only */
         else pix_map_h = off_g_world->portPixMap;       /* -----pre-7.0 */
         (**pix_map_h).hRes = bitmap->h_res;
         (**pix_map_h).vRes = bitmap->v_res;
         PortChanged((GrafPtr)off_g_world);
         LockPixels( pix_map_h );

         /* get baseAddr good in 32-bit mode */
         bits_p = (char *) GetPixBaseAddr( pix_map_h );  
         row_bytes = (**pix_map_h).rowBytes & 0x3fff;
   
         /* Go to 32-bit addressing mode to access pixels */
         mode = true32b;
         SwapMMUMode( &mode );
         
         /* Copy the properly oriented bytes */
         if (bitmap->row_bytes == (**pix_map_h).rowBytes)
         {
            pgBlockMove(&bitmap->bits[0], bits_p, bitmap->row_bytes * bitmap->height);
         }
         else
         {
            long  i;
            char *   src_bits_p;
            
            src_bits_p = (char *)&bitmap->bits[0];
            for (i = 0; i < bitmap->height; ++i)
            {
               pgBlockMove(src_bits_p, bits_p, bitmap->row_bytes * bitmap->height);
               src_bits_p += bitmap->row_bytes;
               bits_p += row_bytes;
            }
         }
         
         /* Back to old addressing mode. */
         SwapMMUMode( &mode );                              
   
         /* Copy the bits. */
         CopyBits( (BitMap *)*pix_map_h, &bits_port->portBits, &source_r, &target_r, transfer_mode, clip_rgn);
         UnlockPixels( pix_map_h );
   
         DisposeGWorld( off_g_world );
      }
      //ееHow do we report errors?
      //else pgFailNIL(GetGlobalsFromRef(palette), off_g_world);
   }
   
   pgDisposeRgn(clip_rgn);

#endif

#ifdef WINDOWS_PLATFORM
   HDC               bitmap_dc, target_dc;
   HBITMAP           source_bitmap;
   short          source_width, source_height, target_width, target_height;

   if (port->poly_save)
      return;

    RectangleToRect(target_rect, NULL, &target_r);
    source_r.left = source_r.top = 0;
    
    if (source_rect)
       RectangleToRect(source_rect, NULL, &source_r);
    else  {
    
      source_r.right = bitmap->width;
      source_r.bottom = bitmap->height;
    }

    switch (draw_mode) {
      
     case bits_copy:
         transfer_mode = SRCCOPY;
         break;
         
     case bits_or:
         transfer_mode = SRCPAINT;
         break;

     case bits_xor:
         transfer_mode = SRCINVERT;
         break;

     default:
        transfer_mode = SRCCOPY;
        break;
   }

   target_dc = pgGetPlatformDevice(port);
   bitmap_dc = CreateCompatibleDC(target_dc);
   source_bitmap = CreateBitmap(bitmap->width, bitmap->height, 1,
         bitmap->pixel_size, (LPSTR)bitmap->bits);
  
   source_width = (short)(source_r.right - source_r.left);
   source_height = (short)(source_r.bottom - source_r.top);
   target_width = (short)(target_r.right - target_r.left);
   target_height = (short)(target_r.bottom - target_r.top);
   
   if ((source_width != target_width) || (source_height != target_height))
      StretchBlt(target_dc, target_r.left, target_r.top,
          source_width, source_height, bitmap_dc, 0, 0,
          target_width, target_height, transfer_mode);
   else
       BitBlt(target_dc, target_r.left, target_r.top,
          source_r.right, source_r.bottom, bitmap_dc, 0, 0, transfer_mode);

   pgReleasePlatformDevice(port);

    DeleteDC(bitmap_dc);
    DeleteObject(source_bitmap);
#endif
}


/* pgOpenPoly begins a poly-save memory_ref in the graf_device, hence causing
all subsequent drawings to be recorded as a "polygon".  */

PG_PASCAL (void) pgOpenPoly (pgm_globals_ptr mem_globals, rectangle_ptr frame,
      graf_device_ptr drawing_device)
{
   pg_poly_ptr       new_poly;

   drawing_device->poly_save = MemoryAllocClear(mem_globals, 1,
      sizeof(pg_poly_rec) - sizeof(co_ordinate), 32);
   new_poly = (pg_poly_ptr) UseMemory(drawing_device->poly_save);
   
   new_poly->height = (short)(frame->bot_right.v - frame->top_left.v);
   new_poly->width = (short)(frame->bot_right.h - frame->top_left.h);
   UnuseMemory(drawing_device->poly_save);
}


/* pgClosePoly removes the poly-save reference from drawing_device and returns
a memory_ref filled with its data. */

PG_PASCAL (poly_ref) pgClosePoly (graf_device_ptr drawing_device)
{
   poly_ref       result;
   
   result = drawing_device->poly_save;
   drawing_device->poly_save = MEM_NULL;
   
   return   result;
}


/* pgPolyBounds returns the width and height for poly. Either width or height
can be NULL if you do not want that value.  */

PG_PASCAL (void) pgPolyBounds (poly_ref poly, long PG_FAR *width, long PG_FAR *height)
{
   pg_poly_ptr       poly_ptr;
   
   poly_ptr = (pg_poly_ptr) UseMemory(poly);
   
   if (width)
      *width = (long)poly_ptr->width;
   if (height)
      *height = (long)poly_ptr->height;
   
   UnuseMemory(poly);
}


/* pgOpenPicture begins a picture-save memory_ref in the graf_device, hence causing
all subsequent drawings to be recorded as a "picture".  */

//е TRS/OITC

PG_PASCAL (void) pgOpenPicture (pgm_globals_ptr mem_globals, rectangle_ptr frame,
      graf_device_ptr drawing_device)
{
   picture_header_ptr   new_pict;

   drawing_device->pict_save = MemoryAllocClear(mem_globals, 1,
      sizeof(picture_header), 32);
   new_pict = (picture_header_ptr) UseMemory(drawing_device->pict_save);
   
   new_pict->height = (short)(frame->bot_right.v - frame->top_left.v);
   new_pict->width = (short)(frame->bot_right.h - frame->top_left.h);
   UnuseMemory(drawing_device->pict_save);
}


/* pgClosePicture removes the picture-save reference from drawing_device and returns
a memory_ref filled with its data. */

//е TRS/OITC

PG_PASCAL (picture_ref) pgClosePicture (graf_device_ptr drawing_device)
{
   picture_ref       result;
   
   result = drawing_device->pict_save;
   drawing_device->pict_save = MEM_NULL;
   
   return   result;
}


/* pgPictureBounds returns the width and height for picture. Either width or height
can be NULL if you do not want that value.  */

PG_PASCAL (void) pgPictureBounds (picture_ref picture, long PG_FAR *width, long PG_FAR *height)
{
   picture_header_ptr         pict_ptr;
   
   pict_ptr = (picture_header_ptr) UseMemory(picture);
   
   if (width)
      *width = (long)pict_ptr->width;
   if (height)
      *height = (long)pict_ptr->height;
   
   UnuseMemory(picture);
}


/* pgDrawPicture cranks out all the objects in picture to the drawing_device.
The picture is pinned to the top-left of pict_frame. If scale_h is TRUE
the picture is scaled to fit into pict_frame horizontally, if scale_v is TRUE
the picture is scaled to fit into pict_frame vetrically. If clip_to_frame is
TRUE the drawing is clipped inside the frame area. */

extern PG_PASCAL (void) pgDrawPicture (graf_device_ptr drawing_device,
      picture_ref picture, rectangle_ptr pict_frame, pg_boolean scale_h,
      pg_boolean scale_v, pg_boolean clip_to_frame)
{
   pg_char_ptr                pict_byte_data;
   register picture_op_ptr       op_code_ptr;
   picture_op_ptr             op_code_data;
   picture_header_ptr            header;
   rectangle                  target_frame;
   co_ordinate                pen_size;
   generic_var                old_clip;
   long                    vertical_scale, horizontal_scale;
   short                   op_code_ctr, target_width, target_height;
   
   if (clip_to_frame)
      old_clip = pgSetClipRect(drawing_device, pict_frame, NULL);

   pict_byte_data = (pg_char_ptr) UseMemory(picture);
   header = (picture_header_ptr)pict_byte_data;
   pict_byte_data += sizeof(picture_header);
   pen_size.h = pen_size.v = 1;
   target_height = (short)(pict_frame->bot_right.v - pict_frame->top_left.v);
   target_width = (short)(pict_frame->bot_right.h - pict_frame->top_left.h);
   vertical_scale = (long)target_height;
   vertical_scale <<= 16;
   vertical_scale |= (long)header->height;
   horizontal_scale = (long)target_width;
   horizontal_scale <<= 16;
   horizontal_scale |= (long)header->width;

   for (op_code_ctr = 0; op_code_ctr < header->num_ops; ++op_code_ctr) {
      
      op_code_ptr = op_code_data = (picture_op_ptr)pict_byte_data;
      ++op_code_data;
      pict_byte_data += op_code_ptr->data_size; // Is set up for next.
      target_frame = op_code_ptr->frame;
      pgOffsetRect(&target_frame, pict_frame->top_left.h, pict_frame->top_left.v);
      
      if (scale_h) {
         
         if (!(op_code_ptr->opcode & SUPPRESS_SCALE_HORIZONTAL)) {
         
            if (op_code_ptr->opcode & USE_LOCAL_ORIGIN)
               pgScaleLong(horizontal_scale, target_frame.top_left.h, &target_frame.bot_right.h);
            else {
               pgScaleLong(horizontal_scale, pict_frame->top_left.h, &target_frame.top_left.h);
               pgScaleLong(horizontal_scale, pict_frame->top_left.h, &target_frame.bot_right.h);
            }
         }
         
         if (op_code_ptr->opcode & FORCE_RIGHT_POINTS)
            target_frame.bot_right.h = pict_frame->bot_right.h;
         if (op_code_ptr->opcode & FORCE_LEFT_POINTS)
            target_frame.top_left.h = pict_frame->top_left.h;
      }

      if (scale_v) {
         
         if (!(op_code_ptr->opcode & SUPPRESS_SCALE_VERTICAL)) {
         
            if (op_code_ptr->opcode & USE_LOCAL_ORIGIN)
               pgScaleLong(vertical_scale, target_frame.top_left.v, &target_frame.bot_right.v);
            else {
               pgScaleLong(vertical_scale, pict_frame->top_left.v, &target_frame.top_left.v);
               pgScaleLong(vertical_scale, pict_frame->top_left.v, &target_frame.bot_right.v);
            }
         }

         if (op_code_ptr->opcode & FORCE_BOT_POINTS)
            target_frame.bot_right.v = pict_frame->bot_right.v;
         if (op_code_ptr->opcode & FORCE_TOP_POINTS)
            target_frame.top_left.v = pict_frame->top_left.v;
      }

      switch (op_code_ptr->type) {

         case object_rect:
         case object_oval:
         case object_roundrect:
         case object_arc:
            if ((op_code_ptr->opcode & OP_CODE_MASK) == frame_opcode)
               pgFrameObject(drawing_device, &target_frame, op_code_ptr->modifier,
                  op_code_ptr->type, (short)pen_size.v);
            else
               pgPaintObject(drawing_device, &target_frame, op_code_ptr->modifier,
                  op_code_ptr->type, 0);
            break;

         case object_poly:
            frame_poly(drawing_device, (pg_poly_ptr)op_code_data, &target_frame, (short)pen_size.v);
            break;

         case object_line:
            pgLineDraw(drawing_device, &target_frame.top_left, &target_frame.bot_right, 0, (short)pen_size.v);
            break;

         case object_bitmap:
         case object_style:
         case object_font:
         case object_pen_color:
            break;
            
         case object_pensize:
            pen_size = target_frame.top_left;
            break;

         case object_text_color:
         case object_bk_color:
         case object_clip_rgn:
         case object_text:
            break;
      }
   }

   UnuseMemory(picture);
   
   if (clip_to_frame)
      pgRestoreClipRgn(drawing_device, old_clip);
}


/* pgSetClipRect changes the clip region in device to rect. The older (previous) region
is returned which can be restored using pgRestoreClipRgn().  */

PG_PASCAL (generic_var) pgSetClipRect (graf_device_ptr device,
		rectangle_ptr rect, rectangle_ptr intersect_with)
{
   generic_var          old_rgn = (generic_var)NULL;
   rectangle			intersect;

#ifdef MAC_PLATFORM
   GrafPtr              port, old_port;
   RgnHandle            cur_rgn, new_rgn;
   Rect              	clip_rect;
   
   GetPort(&old_port);
   port = (GrafPtr)device->machine_var;
   SetPort(port);
   
   if (intersect_with)
   	   pgSectRect(intersect_with, rect, &intersect);
   else
   	   intersect = *rect;

   RectangleToRect(&intersect, NULL, &clip_rect);

   new_rgn = pgCreateRgn();
   pgRectToRgn(new_rgn, &clip_rect);
   old_rgn = pgCreateRgn();
   GetClip(old_rgn);

   SetClip(new_rgn);
   SetPort(old_port);
   DisposeRgn(new_rgn);

#endif
#ifdef WINDOWS_PLATFORM
   HRGN            		new_rgn;
   RECT              	clip_rect;
   HDC					port;

   port = (HDC)device->machine_ref;
   
   if (intersect_with)
   	   pgSectRect(intersect_with, rect, &intersect);
   else
   	   intersect = *rect;

   RectangleToRect(&intersect, NULL, &clip_rect);

   new_rgn = pgCreateRgn();
   pgRectToRgn(new_rgn, &clip_rect);
   old_rgn = PG_LONGWORD(generic_var)SelectObject(port, new_rgn);

#endif
   return   old_rgn;
}

/* pgRestoreClipRgn sets the clip region generic_var which was returned
previosly from pgSetClipRect or pgSetClipRgn. This function DISPOSES rgn
(if it was a memory item).  */

PG_PASCAL (void) pgRestoreClipRgn (graf_device_ptr device, generic_var rgn)
{
#ifdef MAC_PLATFORM
   GrafPtr           old_port;
   
   GetPort(&old_port);
   SetPort((GrafPtr)device->machine_var);
   SetClip((RgnHandle)rgn);
   SetPort(old_port);
   pgDisposeRgn((RgnHandle)rgn);
#endif
#ifdef WINDOWS_PLATFORM
   HRGN			old_rgn;
   
   old_rgn = (HRGN) SelectObject((HDC)device->machine_ref, (HRGN)rgn);
   DeleteObject(old_rgn);
#endif
}



PG_PASCAL (void) pgGetForeColor (graf_device_ptr device, color_value_ptr pg_color)
{
   pg_plat_color_value  os_color;

#ifdef MAC_PLATFORM
//CGrafPtr
   if (((CGrafPtr)(device->machine_var))->portVersion & 0xC000) 
   {
      os_color = ((CGrafPtr)(device->machine_var))->rgbFgColor;
   }
   else
   {
      os_color.red = os_color.green = os_color.blue = 0xFFFF;
   }
#endif

   pgOSToPgColor (&os_color, pg_color);
}


PG_PASCAL (void) pgGetBackColor (graf_device_ptr device, color_value_ptr pg_color)
{
   pg_plat_color_value  os_color;

#ifdef MAC_PLATFORM
   if (((CGrafPtr)(device->machine_var))->portVersion & 0xC000) 
   {
      os_color = ((CGrafPtr)(device->machine_var))->rgbBkColor;
   }
   else
   {
      os_color.red = os_color.green = os_color.blue = 0xFFFF;
   }
#endif

   pgOSToPgColor (&os_color, pg_color);
}


PG_PASCAL (void) pgSetForeColor (graf_device_ptr device, const color_value_ptr pg_color)
{
   pg_plat_color_value  os_color;

   pgColorToOS (pg_color, &os_color);

#ifdef MAC_PLATFORM
   if (((CGrafPtr)(device->machine_var))->portVersion & 0xC000) 
   {
      ((CGrafPtr)(device->machine_var))->rgbFgColor = os_color;
   }
   else
   {
      ((GrafPtr)(device->machine_var))->fgColor = blackColor;
   }
#endif
}


PG_PASCAL (void) pgSetBackColor (graf_device_ptr device, const color_value_ptr pg_color)
{
   pg_plat_color_value  os_color;

   pgColorToOS (pg_color, &os_color);

#ifdef MAC_PLATFORM
   if (((CGrafPtr)(device->machine_var))->portVersion & 0xC000) 
   {
      ((CGrafPtr)(device->machine_var))->rgbBkColor = os_color;
   }
   else
   {
      ((GrafPtr)(device->machine_var))->bkColor = whiteColor;
   }
#endif
}


/* pgPackPoly writes the data from poly in PAIGE packed file format. However,
poly can be MEM_NULL, in which case a zero-size record is STILL saved, resulting
in (intentional) MEM_NULL later when pgUnpackPoly is called. */

PG_PASCAL (void) pgPackPoly (pack_walk_ptr walker, poly_ref poly)
{
   pg_poly_ptr    poly_ptr;
   long        poly_size;
   
   if (poly)
      poly_size = GetMemorySize(poly);
   else
      poly_size = 0;
   
   pgPackNum(walker, long_data, poly_size);
   
   if (!poly_size)
      return;
   
   poly_ptr = (pg_poly_ptr) UseMemory(poly);
   pack_poly_rec(walker, poly_ptr);
   UnuseMemory(poly);
}


/* pgPackBitMap writes the data from bitmap in PAIGE packed file format. However,
palette can be MEM_NULL, in which case a zero-size record is STILL saved, resulting
in (intentional) MEM_NULL later when pgUnpackBitMap is called. */

//е TRS/OITC
PG_PASCAL (void) pgPackBitMap (pack_walk_ptr walker, bitmap_ref b_map)
{
   pg_bitmap_ptr  bmap_ptr;
   long        palette_size;
   
   bmap_ptr = (pg_bitmap_ptr) UseMemory(b_map);
   if (bmap_ptr->palette)
      palette_size = GetMemorySize(bmap_ptr->palette);
   else
      palette_size = 0;
   
   pgPackNum(walker, long_data, GetMemorySize((memory_ref)b_map));
   pgPackBytes(walker, (pg_bits8_ptr)bmap_ptr, GetByteSize((memory_ref)b_map));
   
   pgPackNum(walker, long_data, palette_size);

   if (palette_size)
   {
      pgPackBytes(walker, (pg_bits8_ptr) UseMemory(bmap_ptr->palette), palette_size);
      UnuseMemory((memory_ref)bmap_ptr->palette);
   }
   
   UnuseMemory((memory_ref)b_map);
}

/* pgUnpackPoly returns a newly created polygon from file data in walker,
or MEM_NULL if the original polygon was MEM_NULL. */

PG_PASCAL (poly_ref) pgUnpackPoly (pack_walk_ptr walker)
{
   pg_poly_ptr    poly_ptr;
   poly_ref    new_poly = MEM_NULL;
   long        poly_size;
   
   if (poly_size = pgUnpackNum(walker)) {
      
      new_poly = MemoryAllocClear(GetGlobalsFromRef(walker->data_ref), 1, poly_size, 0);
      poly_ptr = (pg_poly_ptr) UseMemory(new_poly);
      unpack_poly_rec(walker, poly_ptr);
      UnuseMemory(new_poly);
   }

   return   new_poly;
}


/* pgUnpackBitMap returns a newly created bitmap from file data in walker,
or MEM_NULL if the original bitmap was MEM_NULL. */

PG_PASCAL (picture_ref) pgUnpackBitMap (pack_walk_ptr walker)
{
   pg_bitmap_ptr  bmap_ptr;
   poly_ref    new_bmap = MEM_NULL;
      
   new_bmap = MemoryAlloc(GetGlobalsFromRef(walker->data_ref), 1, 0, 0);
   pgUnpackBytes(walker, new_bmap);

   bmap_ptr = (pg_bitmap_ptr) UseMemory(new_bmap);
   bmap_ptr->palette = MEM_NULL;

   bmap_ptr->palette = MemoryAlloc(GetGlobalsFromRef(walker->data_ref), 1, 0, 0);
   pgUnpackBytes(walker, bmap_ptr->palette);
   
   if (!GetMemorySize(bmap_ptr->palette)) {
      DisposeMemory(bmap_ptr->palette);
      bmap_ptr->palette = MEM_NULL;
   }
   
   UnuseMemory(bmap_ptr->palette);

   return   new_bmap;
}

/* Initialize Paige's platform independant bit maps */

//е TRS/OITC

PG_PASCAL (void) pgInitBitMap(graf_device_ptr device, rectangle_ptr dest_rect, short pixel_size, memory_ref palette, long ref_con, short background, bitmap_ref bit_map)
{
   pg_bitmap_ptr  bitmap_p;
   long        row_size;
   short       width;
   short       height;
   pg_char        filler;
   
   width = (short)(dest_rect->bot_right.h - dest_rect->top_left.h);
   height = (short)(dest_rect->bot_right.v - dest_rect->top_left.v);

   // Set size for bitmap & clear
   row_size = ((pixel_size * width + 31) >> 4) * 4;
   SetMemorySize (bit_map, row_size * height + sizeof(pg_bitmap_rec) - 2);
   
   // Fill bit map header
   bitmap_p = (pg_bitmap_ptr)UseMemory(bit_map);
   pgFillBlock(bitmap_p, sizeof(pg_bitmap_rec), 0);
   
   bitmap_p->origin = *dest_rect;
   bitmap_p->width = width;
   bitmap_p->height = height;

   filler = 0;
   if (pixel_size <= 8)
   {
      short background_val, i;
      
      background_val = background & ((1 << pixel_size) - 1);
      
      for (i = 0; i < 8 / pixel_size; i++)
      {
         filler <<= pixel_size;
         filler |= background_val;
      }
   }
   pgFillBlock(&bitmap_p->bits[0], row_size * height, filler);
   
   bitmap_p->version = CUR_BIT_MAP_VERSION;
   bitmap_p->h_res = device->resolution << 16;
   bitmap_p->v_res = device->resolution & 0xFFFF0000;
   bitmap_p->pixel_size = pixel_size;
   bitmap_p->row_bytes = (short)row_size;
   bitmap_p->ref_con = ref_con;
   if (pixel_size > 8)
      bitmap_p->palette = MEM_NULL;
   else if (!palette)
#ifdef MAC_PLATFORM
      bitmap_p->palette = pgCTab2ColorValues(GetGlobalsFromRef(bit_map), GetCTable(64 + pixel_size));
#else
      bitmap_p->palette = MEM_NULL;
#endif
   else bitmap_p->palette = palette;
   
   UseMemory(bit_map);
}


/******************************* Macintosh Support Functions ***************************/

//е TRS/OITC

#ifdef MAC_PLATFORM

/* Converts a Paige palette to a Quickdraw color table */

PG_PASCAL (CTabHandle) pgColorValues2CTab(memory_ref palette)
{
   CTabHandle  cTab;
   long     size;
   long     i;
   
   if (!palette || (size = GetMemorySize(palette)) == 0)
      return NULL;
   
   cTab = (CTabHandle)NewHandleClear(sizeof(ColorTable) + sizeof(ColorSpec) * (--size));
   pgFailNIL(GetGlobalsFromRef(palette), cTab);
   
   (**cTab).ctSize = size;
   (**cTab).ctSeed = GetCTSeed();
   
   for (i = 0; i <= size; ++i)
   {
      color_value_ptr   color;
      
      color = (color_value_ptr)UseMemoryRecord (palette, i, 1, i == 0);
      
      (**cTab).ctTable[i].value = i;
      pgColorToOS(color, &(**cTab).ctTable[i].rgb);
   }
   CTabChanged(cTab);
   
   UnuseMemory(palette);
   
   return cTab;
   
}


/* Converts a Quickdraw color table to a Paige palette */

PG_PASCAL (memory_ref) pgCTab2ColorValues(pgm_globals_ptr mem_globals, CTabHandle cTab)
{
   memory_ref  palette;
   long     i;
   
   if (!cTab)
      return MEM_NULL;
   
   palette = MemoryAllocClear (mem_globals, sizeof(color_value), (**cTab).ctSize + 1, 0);
   
   for (i = 0; i <= (**cTab).ctSize; ++i)
   {
      color_value_ptr   color;
      
      color = (color_value_ptr)UseMemoryRecord (palette, (**cTab).ctTable[i].value, 1, i == 0);
      pgOSToPgColor(&(**cTab).ctTable[i].rgb, color);
   }
   
   UnuseMemory(palette);
   
   return palette;
   
}


/* Converts a Paige palette to a Quickdraw palette */

PG_PASCAL (PaletteHandle) pgColorValues2Palette(memory_ref palette)
{
   PaletteHandle  aPalette;
   long        size;
   long        i;
   
   if (!palette || (size = GetMemorySize(palette)) == 0)
      return NULL;
   
   aPalette = NewPalette(size, NULL, pmTolerant, 0x0000);
   pgFailNIL(GetGlobalsFromRef(palette), aPalette);
   
   for (i = 0; i < size; ++i)
   {
      color_value_ptr   color;
      RGBColor    rgb;
      
      color = (color_value_ptr)UseMemoryRecord (palette, i, 1, i == 0);
      
      pgColorToOS(color, &rgb);
      SetEntryColor(aPalette, i, &rgb);
   }  
   UnuseMemory(palette);
   
   return aPalette;
   
}


/* Converts a Quickdraw palette to a Paige palette */

PG_PASCAL (memory_ref) pgPalette2ColorValues(pgm_globals_ptr mem_globals, PaletteHandle aPalette)
{
   memory_ref  palette;
   long     i;
   
   if (!aPalette)
      return MEM_NULL;
   
   palette = MemoryAllocClear (mem_globals, sizeof(color_value), (**aPalette).pmEntries + 1, 0);
   
   for (i = 0; i <= (**aPalette).pmEntries; ++i)
   {
      color_value_ptr   color;
      RGBColor    rgb;
      
      SetEntryColor(aPalette, i, &rgb);
      color = (color_value_ptr)UseMemoryRecord (palette, i, 1, i == 0);
      pgOSToPgColor(&rgb, color);
   }
   
   UnuseMemory(palette);
   
   return palette;
   
}

#endif


/* pgLongToRGB changes a Windows style colorref to a color value */

extern PG_PASCAL (void) pgLongToRGB (long colorref, color_value_ptr color)
{
   long     red, green, blue;
   
   if ((blue = (colorref >> 8) & 0xFF00) != 0)
      blue |= (blue >> 8);

   if ((green = colorref & 0xFF00) != 0)
      green |= (green >> 8);

   if ((red = (colorref << 8) & 0xFF00) != 0)
      red |= (red >> 8);
   
   color->red = (unsigned short)red;
   color->green = (unsigned short)green;
   color->blue = (unsigned short)blue;
}

/* pgRGBToLong returns a long from an RGB. */

PG_PASCAL (long) pgRGBToLong (color_value_ptr color)
{
   long     red, green, blue;
   
   red = (long)color->red;
   green = (long)color->green;
   blue = (long)color->blue;
   
   red &= 0x00FF00;
   green &= 0x00FF00;
   blue &= 0x00FF00;

   return   ((blue << 8) | (red >> 8) | green);
}

/* pgGetGrayScale returns a colorref (long color) which is a percent lighter or darker
than bk_color. If percent is positive the color is lighter, negative the color is darker.
If necessary the result is inverted (for example if "darker" is less than zero it is inverted
two's complement). */

PG_PASCAL (long) pgGetGrayScale (color_value_ptr bk_color, short percent)
{
   long     red, green, blue, extra;
   
   red = (long)bk_color->red >> 8;
   green = (long)bk_color->green >> 8;
   blue = (long)bk_color->blue >> 8;
   extra = 0;

   if (red < 32) {
   
      extra = (32 - red);
      red += extra;
      green += extra;
      blue += extra;
   }

   if (green < 32) {
   
      extra = (32 - green);
      red += extra;
      green += extra;
      blue += extra;
   }

   if (blue < 32) {
   
      extra = (32 - blue);
      red += extra;
      green += extra;
      blue += extra;
   }

   if (percent < 0) {
      
      red = (red * (-percent)) / 100;
      green = (green * (-percent)) / 100;
      blue = (blue * (-percent)) / 100;
   }
   else {

      red = (red * (percent + 100)) / 100;
      green = (green * (percent + 100)) / 100;
      blue = (blue * (percent + 100)) / 100;
   }
   
   if (red > 255)
      red = 255;
   if (green > 255)
      green = 255;
   if (blue > 255)
      blue = 255;
   
   return   ((blue << 16) | (green << 8) | red);
}


/******************************* Local Functions ***************************/


/* save_poly_points saves off first_pt and second_pt into the polygon. */

static void save_poly_points (poly_ref poly, co_ordinate_ptr first_pt, co_ordinate_ptr second_pt)
{
   pg_poly_ptr       poly_ptr;
   co_ordinate_ptr      new_points;

   poly_ptr = (pg_poly_ptr) UseMemory(poly);
   poly_ptr->num_points += 2;
   new_points = (co_ordinate_ptr) AppendMemory(poly, sizeof(co_ordinate) * 2, FALSE);
   new_points[0] = *first_pt;
   new_points[1] = *second_pt;
   
   UnuseMemory(poly);
}


/* frame_poly gets called by pgFramePoly to do the actual work. */

static void frame_poly (graf_device_ptr port, pg_poly_ptr poly_ptr,
      rectangle_ptr target_frame, short pen_size)
{
   register co_ordinate_ptr      points;
   co_ordinate                 draw_from, draw_to;
   pg_fixed                scale_h, scale_v;
   long                     target_width, target_height;
   long                     source_width, source_height;
   short                   num_points;

   if (poly_ptr->width && poly_ptr->height && poly_ptr->num_points > 1) {
   
      target_width = target_frame->bot_right.h - target_frame->top_left.h;
      target_height = target_frame->bot_right.v - target_frame->top_left.v;
      source_width = poly_ptr->width;
      source_height = poly_ptr->height;
   
      scale_h = scale_v = 0;
   
      if (source_width != target_width)
         scale_h = (pg_fixed)(target_width << 16) | source_width;
      if (source_height != target_height)
         scale_v = (pg_fixed)(target_height << 16) | source_height;
   
      points = poly_ptr->points;
      num_points = poly_ptr->num_points;
      
      if (num_points & 1)
         --num_points;
   
      while (num_points) {
         draw_from = *points++;
         draw_to = *points++;
   
         pgScaleLong(scale_h, 0, &draw_from.h);
         pgScaleLong(scale_v, 0, &draw_from.v);
         pgAddPt(&target_frame->top_left, &draw_from);
         
         pgScaleLong(scale_h, 0, &draw_to.h);
         pgScaleLong(scale_v, 0, &draw_to.v);
         pgAddPt(&target_frame->top_left, &draw_to);
         
         pgLineDraw(port, &draw_from, &draw_to, 0, pen_size);
         
         num_points -= 2;
      }
   }
}


/* unpack_poly_rec unpacks a previously saved poly record. The next data byte
will be the first field in polygon. */

static void unpack_poly_rec (pack_walk_ptr walker, pg_poly_ptr poly)
{
   short       index_ctr;
   
   poly->width = (short)pgUnpackNum(walker);
   poly->height = (short)pgUnpackNum(walker);
   poly->rsrv = (short)pgUnpackNum(walker);
   poly->num_points =(short) pgUnpackNum(walker);

   for (index_ctr = 0; index_ctr < poly->num_points; ++index_ctr)
      pgUnpackCoOrdinate(walker, &poly->points[index_ctr]);
}


/* pack_poly_rec sends a polygon record to the walker output. */

static void pack_poly_rec (pack_walk_ptr walker, pg_poly_ptr poly)
{
   short       index_ctr;
   
   pgPackNum(walker, short_data, (long)poly->width);
   pgPackNum(walker, short_data, (long)poly->height);
   pgPackNum(walker, short_data, (long)poly->rsrv);
   pgPackNum(walker, short_data, (long)poly->num_points);

   for (index_ctr = 0; index_ctr < poly->num_points; ++index_ctr)
      pgPackCoOrdinate(walker, &poly->points[index_ctr]);
}


