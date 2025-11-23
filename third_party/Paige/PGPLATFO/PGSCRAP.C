#include "Paige.h"
#include "defprocs.h"
#include "pgtraps.h"
#include "machine.h"
#include "pgutils.h"
#include "pgEmbed.h"
#include "pgscrap.h"

#if defined(UNIX_PLATFORM)

PG_PASCAL (pg_ref) pgGetScrap (pg_globals_ptr globals, pg_os_type native_format, embed_callback def_embed_callback)
{
    (void)globals;
    (void)native_format;
    (void)def_embed_callback;
    return MEM_NULL;
}

PG_PASCAL (pg_boolean) pgScrapAvail (pg_os_type native_format)
{
    (void)native_format;
    return FALSE;
}

PG_PASCAL (void) pgPutScrap (pg_ref the_scrap, pg_os_type native_format, short scrap_type)
{
    (void)the_scrap;
    (void)native_format;
    (void)scrap_type;
}

#else


/* This source file handles machine-specific clipboard operations. Copyright 1996, DataPak
Software, Inc.  Software by Gar. */

#include "Paige.h"
#include "defprocs.h"
#include "pgtraps.h"
#include "machine.h"
#include "pgutils.h"
#include "pgEmbed.h"
#include "pgscrap.h"

static pg_handle get_machine_scrap (pg_globals_ptr globals, pg_os_type native_format,
      short PG_FAR *scrap_type, long PG_FAR *embed_type, size_t PG_FAR *raw_data_size,
      pg_boolean load_real_data);
static pg_boolean put_machine_scrap (pg_ref the_scrap, int scrap_type, pg_os_type native_format, pg_boolean clear_scrap);

#ifdef UNICODE
static pg_handle convert_scrap_to_unicode (pg_handle the_scrap, size_t PG_FAR *bytesize);
static void convert_unicode_to_scrap (memory_ref the_scrap, long charsize);
static HGLOBAL process_cf_unicode (HGLOBAL data, size_t PG_FAR *text_size);
#endif

#ifdef WINDOWS_PLATFORM
static HANDLE bitmap_to_metafile (pg_globals_ptr globals, HBITMAP bitmap, RECT *bounds);
static short pixel_convert (HDC hdc, short map_mode, short value);
static short pixels_to_himetric (HDC hdc, short value);
static HGLOBAL duplicate_data (HGLOBAL data);
static HGLOBAL process_cf_text (HGLOBAL data, size_t PG_FAR *text_size);
static HGLOBAL process_text_out (memory_ref ref, pg_boolean convert_to_bits8);

#endif

#define NUM_SCRAP_KEYS	11

static pg_file_key PG_FAR scrap_save_keys[NUM_SCRAP_KEYS] = {

	paige_key,
	text_block_key,
	text_key,
	style_run_key,
	par_run_key,
	style_info_key,
	par_info_key,
	font_info_key,
	named_styles_key,
	hyperlink_key,
	hyperlink_target_key
};


/* pgGetScrap returns a pg_ref built from the external clipboard. If none available
we return MEM_NULL. The def_embed_callback param is used for embed_ref type scrap items
(the callback is placed into the newly created embed_ref).  */

PG_PASCAL (pg_ref) pgGetScrap (pg_globals_ptr globals, pg_os_type native_format, embed_callback def_embed_callback)
{
   pg_handle         raw_data;
   paige_rec_ptr     pg_rec;
   style_info_ptr    def_style;
   pg_char_ptr       text_ptr;
   memory_ref        converted_data = MEM_NULL;
   pg_ref            result = MEM_NULL;
   embed_ref         embed;
   pg_fixed          vert_pos;
   long              embed_type;
   size_t            datasize, position;
   short             scrap_type;
   
   raw_data = get_machine_scrap(globals, native_format, &scrap_type, &embed_type, &datasize, TRUE);
   
   if (scrap_type) {

      result = pgNewShell(globals);
	  pgSetAuthor(result, (long)-scrap_type);

      switch (scrap_type) {

         case pg_native_scrap:
            position = 0;

            pgInitEmbedProcs(globals, def_embed_callback, NULL);

            converted_data = HandleToMemory(globals->mem_globals, raw_data, 1);
            pgReadDoc(result, &position, NULL, 0, pgScrapMemoryRead, (file_ref)converted_data);
            break;

         case pg_text_scrap:
         #ifdef UNICODE
            raw_data = convert_scrap_to_unicode (raw_data, &datasize);
         #endif
         // no break
       case pg_unicode_scrap:

         #ifdef MAC_PLATFORM
            HLock(raw_data);
            text_ptr = (pg_char_ptr)*raw_data;
         #endif
         
         #ifdef WINDOWS_PLATFORM
            text_ptr = (pg_char_ptr) GlobalLock(raw_data);
         #endif
         
            pgInsert(result, text_ptr, datasize, 0, data_insert_mode, 0, draw_none);
         
         #ifdef MAC_PLATFORM
            DisposeHandle(raw_data);
         #endif
         #ifdef WINDOWS_PLATFORM
            GlobalUnlock(raw_data);
            GlobalFree(raw_data);
         #endif
         
            break;

         case pg_embed_scrap:
            
            pg_rec = (paige_rec_ptr) UseMemory(result);
            def_style = (style_info_ptr) UseMemory(pg_rec->t_formats);
            vert_pos = def_style->descent;
            vert_pos <<= 16;
            UnuseMemory(pg_rec->t_formats);
            UnuseMemory(result);

            switch (embed_type) {
               
               case embed_mac_pict:

                  embed = pgNewEmbedRef(globals->mem_globals, embed_mac_pict,
                        (void PG_FAR *)raw_data, 0, 0, -vert_pos, 0, FALSE);
                  break;

               case embed_meta_file:
         #ifdef WINDOWS_PLATFORM
               {
                  METAFILEPICT PG_FAR     *src_meta;
                  metafile_struct         meta;
                  HDC                 hdc;
 
                  pgFillBlock(&meta, sizeof(metafile_struct), 0);

                  src_meta = (METAFILEPICT *) GlobalLock(raw_data);
                  meta.metafile = PG_LONGWORD(size_t)src_meta->hMF;
                  meta.mapping_mode = src_meta->mm;
              meta.x_ext = (short)src_meta->xExt;
              meta.y_ext = (short)src_meta->yExt;

          // Convert width and height to screen units
               
                 hdc = (HDC)globals->machine_const;
                  meta.bounds.bot_right.h = pixel_convert(hdc, (short)src_meta->mm, (short)src_meta->xExt);
                  meta.bounds.bot_right.v = pixel_convert(hdc, (short)src_meta->mm, (short)src_meta->yExt);
                  GlobalUnlock(raw_data);
                  GlobalFree(raw_data);

                  embed = pgNewEmbedRef(globals->mem_globals, embed_meta_file,
                        (void PG_FAR *)&meta, 0, 0, -vert_pos, 0, FALSE);
                  
               }
         #endif
                  break;
                  
               case embed_ole:
                  break;

            }
            
            pgInsertEmbedRef(result, embed, 0, 0, def_embed_callback, 0, draw_none);
      }
   }

   DisposeNonNilMemory(converted_data);

   return   result;
}


/* pgScrapAvail returns TRUE if there is something we can work with from the scrap. */

PG_PASCAL (pg_boolean) pgScrapAvail (pg_os_type native_format)
{
   short       the_type;
   
    get_machine_scrap(NULL, native_format, &the_type, NULL, NULL, FALSE);
   
    return  (pg_boolean)(the_type != 0);
}

/* pgPutScrap sends the contents of the_scrap to the external clipboard. If scrap_type
is pg_void_scrap, all type(s) within the pg_ref are placed into the scrap, otherwise
only the specific type is placed. */

PG_PASCAL (void) pgPutScrap (pg_ref the_scrap, pg_os_type native_format, short scrap_type)
{
   paige_rec_ptr     pg_rec;
   select_pair       full_range;
   pg_boolean        keep_scrap = FALSE;

   if (scrap_type == (short)pg_void_scrap || scrap_type == (short)pg_native_scrap)
      keep_scrap |= put_machine_scrap(the_scrap, (int)pg_native_scrap, native_format, (pg_boolean)!keep_scrap);
   
   pg_rec = (paige_rec_ptr) UseMemory(the_scrap);
   full_range.begin = 0;
   full_range.end = pg_rec->t_length;
   
   if (pg_rec->t_length == 2 && pgNumEmbeds(the_scrap, &full_range) == 1) {
      
      if (scrap_type == (short)pg_void_scrap || scrap_type == (short)pg_embed_scrap)
         keep_scrap |= put_machine_scrap(the_scrap, (int)pg_embed_scrap, native_format, (pg_boolean)!keep_scrap);
   }
   else {
      
      if (scrap_type == (short)pg_void_scrap || scrap_type == (short)pg_text_scrap || scrap_type == (short)pg_unicode_scrap) {
       
#ifdef UNICODE
         keep_scrap |= put_machine_scrap(the_scrap, (int)pg_text_scrap, native_format, (pg_boolean)!keep_scrap);      
       keep_scrap |= put_machine_scrap(the_scrap, (int)pg_unicode_scrap, native_format, (pg_boolean)!keep_scrap);     
#else
         keep_scrap |= put_machine_scrap(the_scrap, (int)pg_text_scrap, native_format, (pg_boolean)!keep_scrap);      
#endif
     }
   }

   UnuseMemory(the_scrap);
}



/********************************* LOCAL FUNCTIONS *******************************/


/* get_machine_scrap returns the raw data, in a HANDLE, of the recognized scrap type. It first
checks PAIGE type, then graphics, then text. If load_real_data is FALSE then we merely check
for the existence of the type but return a dummy handle ("TRUE") if it exists. */

static pg_handle get_machine_scrap (pg_globals_ptr globals, pg_os_type native_format,
      short PG_FAR *scrap_type, long PG_FAR *embed_type, size_t PG_FAR *raw_data_size,
      pg_boolean load_real_data)
{
   pg_handle        data = (pg_handle)NULL;
   long             the_embed_type = 0;
   size_t           datasize;
   short            the_type = pg_void_scrap;
   
#ifdef MAC_PLATFORM
   long        offset;

   if (load_real_data)
      data = NewHandle(0);

   if (GetScrap(data, native_format, &offset) >= 0)
      the_type = pg_native_scrap;
   else
   if (GetScrap(data, PG_PICT_OSTYPE, &offset) >= 0) {
   
      the_type = pg_embed_scrap;
      the_embed_type = embed_mac_pict;
   }
   else
   if (GetScrap(data, PG_TEXT_OSTYPE, &offset) >= 0)
      the_type = pg_text_scrap;
   
   if (data)
      datasize = GetHandleSize(data);
#endif

#ifdef WINDOWS_PLATFORM
   HANDLE               converted_meta;
   METAFILEPICT   PG_FAR  *metarecord;
   RECT                 bounds;
   pg_os_type           formats[5];
   pg_os_type           found_type;
   int               num_formats;

   formats[0] = native_format;

#ifdef UNICODE
   formats[1] = CF_UNICODETEXT;
   formats[2] = CF_TEXT;
   formats[3] = CF_METAFILEPICT;
   formats[4] = CF_BITMAP;
   num_formats = 5;
#else
   
   formats[1] = CF_TEXT;
   formats[2] = CF_METAFILEPICT;
   formats[3] = CF_BITMAP;
   num_formats = 4;
#endif
   
   if ((found_type = GetPriorityClipboardFormat(formats, num_formats)) > 0) {
      
      if (load_real_data) {
      
         data = GetClipboardData(found_type);
         datasize = GlobalSize(data);

         if (raw_data_size)
            *raw_data_size = datasize;
        }
        
      if (found_type == (pg_os_type)native_format) {
      
         the_type = pg_native_scrap;
         
         if (load_real_data)
            data = duplicate_data(data);
      }
      else {
         
         switch (found_type) {
            
            case CF_METAFILEPICT:
               the_type = pg_embed_scrap;
               the_embed_type = embed_meta_file;
               
               if (!load_real_data)
                     break;
                     
               data = duplicate_data(data);
               metarecord = (METAFILEPICT *) GlobalLock(data);
               metarecord->hMF = CopyMetaFile(metarecord->hMF, NULL);
          
               GlobalUnlock(data);
               break;

            case CF_BITMAP:
/* PDA:  Modified the mapping mode for the converted bitmap!  Extents in HIMETRIC.
               the_type = pg_embed_scrap;
               the_embed_type = embed_meta_file;
               
               if(!load_real_data)
                  break;

               converted_meta = bitmap_to_metafile(globals, (HBITMAP)data, &bounds);
               
               data = GlobalAlloc(GMEM_MOVEABLE, sizeof(METAFILEPICT));
               metarecord = GlobalLock(data);
               metarecord->mm = MM_TEXT;
               metarecord->xExt = bounds.right - bounds.left;
               metarecord->yExt = bounds.bottom - bounds.top;
               metarecord->hMF = converted_meta;
               GlobalUnlock(data);

               break;
*/
               the_type = pg_embed_scrap;
               the_embed_type = embed_meta_file;
               
               if (!load_real_data)
                  break;

               converted_meta = bitmap_to_metafile(globals, (HBITMAP)data, &bounds);
               
               data = GlobalAlloc(GMEM_MOVEABLE, sizeof(METAFILEPICT));
               metarecord = (METAFILEPICT *) GlobalLock(data);
               metarecord->mm = MM_ANISOTROPIC;
               { // PDA:  Begin
                  POINT wpt;
                  HDC dc = (HDC)globals->machine_const;
                  int savedc = SaveDC(dc);
                  wpt.x = bounds.right;
                  wpt.y = bounds.bottom;
                  SetMapMode(dc, MM_HIMETRIC);
                  DPtoLP(dc, &wpt, 1);
                  RestoreDC(dc, savedc);
                  metarecord->xExt = wpt.x;
                  metarecord->yExt = wpt.y;
               } // PDA: End
               metarecord->hMF = (HMETAFILE) converted_meta;
               GlobalUnlock(data);

               break;

            case CF_TEXT:
               the_type = pg_text_scrap;
               
               if (load_real_data)
                     data = process_cf_text(data, &datasize);
      
#ifdef UNICODE
         case CF_UNICODETEXT:
            the_type = pg_unicode_scrap;

            if (load_real_data)
               data = process_cf_unicode(data, &datasize);
               break;
#endif
         }
      }
   }

#endif
   
   if (scrap_type)
      *scrap_type = the_type;
   if (embed_type)
      *embed_type = the_embed_type;
   if (raw_data_size)
      *raw_data_size = datasize;

   return      data;
}

#ifdef UNICODE

/* convert_scrap_to_unicode converts raw text from the scrap to unicode chars.
Upon entry, the_scrap is a machine-specific handle containing the text
and bytesize is the byte count. When this function exits, bytesize is changed
to the actual character count that should be inserted into the pg_ref. */

static pg_handle convert_scrap_to_unicode (pg_handle the_scrap, size_t PG_FAR *bytesize)
{
   pg_handle         result = the_scrap;
   pg_bits8_ptr      text;

#ifdef MAC_PLATFORM
      HUnlock(result);
      SetHandleSize(result, *bytesize * 2);
      HLock(result);
      text = (pg_bits8_ptr)*result;
#endif

#ifdef WINDOWS_PLATFORM
      GlobalUnlock(result);
      result = GlobalReAlloc(result, *bytesize * 2, 0);
      text = (pg_bits8_ptr) GlobalLock(result);
#endif

   *bytesize = pgBytesToUnicode(text, (pg_short_t PG_FAR *)text, NULL, *bytesize);

#ifdef MAC_PLATFORM
   HUnlock(result);
#endif
#ifdef WINDOWS_PLATFORM
   GlobalUnlock(result);
#endif

   return      result;
}


/* convert_unicode_to_scrap converts the 16-bit chars in the_scrap to 8-bit ASCII text. */

static void convert_unicode_to_scrap (memory_ref the_scrap, long charsize)
{
   pg_short_t PG_FAR *characters;
   size_t               bytesize;

   characters = (pg_short_t *) UseMemory(the_scrap);
   bytesize = pgUnicodeToBytes(characters, (pg_bits8_ptr)characters, NULL, charsize);
   UnuseMemory(the_scrap);
   
   SetMemoryRecSize(the_scrap, 1, 0);
   SetMemorySize(the_scrap, bytesize);
}

#endif

/* put_machine_scrap places the contents of the_scrap into the external clipboard. The type
placed is scrap_type. Note, if embed type the_scrap is known to be ONLY an embed (no other
text). If something went to the scrap, TRUE is returned. */

static pg_boolean put_machine_scrap (pg_ref the_scrap, int scrap_type, pg_os_type native_format, pg_boolean clear_scrap)
{
   paige_rec_ptr     	pg_rec;
   memory_ref       	dataref;
   embed_ref         	embed;
   pgm_globals_ptr   	mem_globals;
   pg_embed_ptr      	embed_ptr;
   pg_handle         	raw_data = (pg_handle)NULL;
   pg_char_ptr       	text_ptr;
   select_pair       	full_range;
   pg_os_type        	os_type;
   long              	embed_type = 0;
   size_t          	 	position;

   pg_rec = (paige_rec_ptr) UseMemory(the_scrap);
   mem_globals = pg_rec->globals->mem_globals;
   full_range.begin = 0;
   full_range.end = pg_rec->t_length;

   switch (scrap_type) {

      case pg_native_scrap:
		       	 		
         dataref = MemoryAlloc(mem_globals, 1, 0, 128);
         position = 0;
         pgSaveDoc(the_scrap, &position, scrap_save_keys, NUM_SCRAP_KEYS, pgScrapMemoryWrite, dataref, 0);
         pgSaveAllEmbedRefs(the_scrap, pgScrapMemoryWrite, pgScrapMemoryWrite, &position, dataref);
         pgTerminateFile(the_scrap, &position, pgScrapMemoryWrite, dataref);

         raw_data = MemoryToHandle(dataref);
         os_type = native_format;
         break;

      case pg_text_scrap:
      case pg_unicode_scrap:
         dataref = pgCopyText(the_scrap, &full_range, all_data);
         
         if (dataref) {
         
            text_ptr = (pg_char_ptr) AppendMemory(dataref, 1, FALSE);
            *text_ptr = 0;
            UnuseMemory(dataref);

         #ifdef WINDOWS_PLATFORM
            raw_data = process_text_out(dataref, (pg_boolean)(scrap_type == pg_text_scrap));
            DisposeMemory(dataref);
         #endif
         
         #ifdef MAC_PLATFORM
            raw_data = MemoryToHandle(dataref);
         #endif
         
         if (scrap_type == pg_text_scrap)
            os_type = PG_TEXT_OSTYPE;
         else
            os_type = PG_UNICODE_OSTYPE;
         }

         break;

      case pg_embed_scrap:
         position = 0;
         
         if ((embed = pgFindNextEmbed(the_scrap, &position, 0, 0)) != MEM_NULL) {
            
            embed_ptr = (pg_embed_ptr) UseMemory(embed);
            embed_type = embed_ptr->type & EMBED_TYPE_MASK;
            
            if (embed_type == embed_mac_pict) {

      #ifdef MAC_PLATFORM
      
               raw_data = (pg_handle)embed_ptr->data;
               os_type = PG_PICT_OSTYPE;
      #endif
            }
            else
            if (embed_type == embed_meta_file) {
      
      #ifdef WINDOWS_PLATFORM
               METAFILEPICT   PG_FAR   *metarecord;
               HDC                  hdc;
               
               hdc = (HDC)pg_rec->globals->machine_const;
               raw_data = GlobalAlloc(GMEM_MOVEABLE, sizeof(METAFILEPICT));
               metarecord = (METAFILEPICT *) GlobalLock(raw_data);
               
               metarecord->mm = (int)embed_ptr->uu.pict_data.mapping_mode;
               
               if (!metarecord->mm)
                     metarecord->mm = MM_TEXT;
               
               if (metarecord->mm != MM_TEXT) {
               
                     if (embed_ptr->uu.pict_data.meta_ext_x == 0)
                        metarecord->xExt = (int)pixels_to_himetric(hdc, (short)embed_ptr->width);
                     else
                        metarecord->xExt = (int)embed_ptr->uu.pict_data.meta_ext_x;
               
                     if (embed_ptr->uu.pict_data.meta_ext_y == 0)
                        metarecord->yExt = (int)pixels_to_himetric(hdc, (short)embed_ptr->height);
                     else
                        metarecord->yExt = (int)embed_ptr->uu.pict_data.meta_ext_y;
               }
               else {
                    
                    metarecord->xExt = (int)embed_ptr->width;
                    metarecord->yExt = (int)embed_ptr->height;
               }
               
               metarecord->hMF = CopyMetaFile((HMETAFILE)embed_ptr->data, NULL);
               GlobalUnlock(raw_data);
               
               os_type = CF_METAFILEPICT;
      #endif

            }

            UnuseMemory(embed);
            
            if (raw_data)
               break;
         }

         break;
   }
   
   if (raw_data) {
   
#ifdef MAC_PLATFORM
        long         data_size;
 
      if (clear_scrap)
         ZeroScrap();
      
      data_size = GetHandleSize(raw_data);
      HLock(raw_data);
      PutScrap(data_size, os_type, (void *)*raw_data);
      
      if (embed_type != embed_mac_pict)
         DisposeHandle(raw_data);
#endif

#ifdef WINDOWS_PLATFORM
      
      if (clear_scrap)
         EmptyClipboard();
      
      SetClipboardData(os_type, raw_data);
#endif
   }

   UnuseMemory(the_scrap);
   
   return      (pg_boolean)(raw_data != (pg_handle)NULL);
}


#ifdef WINDOWS_PLATFORM

/* PDA:  replaced the bitmap_to_metafile function.
static HANDLE bitmap_to_metafile (pg_globals_ptr globals, HBITMAP bitmap, RECT *bounds)
{
   BITMAP         bitsInfo;
   HDC            dc, bitmapDC, metaDC;
   
   GetObject(bitmap, sizeof(BITMAP), &bitsInfo);
   bounds->top = bounds->left = 0;
   bounds->right = bitsInfo.bmWidth;
   bounds->bottom = bitsInfo.bmHeight;
   
   dc = (HDC)globals->machine_const;
   bitmapDC = CreateCompatibleDC(dc);
   SelectObject(bitmapDC, bitmap);
   
   metaDC = CreateMetaFile(NULL);
   BitBlt(metaDC, 0, 0, bitsInfo.bmWidth, bitsInfo.bmHeight, bitmapDC, 0, 0, SRCCOPY);
   DeleteDC(bitmapDC);

   return   (CloseMetaFile(metaDC));
}
*/
static HANDLE bitmap_to_metafile (pg_globals_ptr globals, HBITMAP bitmap, RECT *bounds)
{
   BITMAP         bitsInfo;
   HDC            dc, bitmapDC, metaDC;

   GetObject(bitmap, sizeof(BITMAP), &bitsInfo);
   bounds->top = bounds->left = 0;
   bounds->right = bitsInfo.bmWidth;
   bounds->bottom = bitsInfo.bmHeight;

   dc = (HDC)globals->machine_const;
   bitmapDC = CreateCompatibleDC(dc);
   SelectObject(bitmapDC, bitmap);
   
   metaDC = CreateMetaFile(NULL);
   // PDA:  Required to create a "Standard" clipboard metafile.
   SetMapMode(metaDC, MM_ANISOTROPIC);
   SetWindowExtEx(metaDC, bitsInfo.bmWidth, bitsInfo.bmHeight, NULL);
   // PDA:  Use StrectBlt not BitBlt!!!
   StretchBlt(metaDC, 0, 0, bitsInfo.bmWidth, bitsInfo.bmHeight,
              bitmapDC, 0, 0, bitsInfo.bmWidth, bitsInfo.bmHeight, SRCCOPY);
   DeleteDC(bitmapDC);

   return   (CloseMetaFile(metaDC));
}

PG_PASCAL (HMETAFILE) pgBitmapToMetafile(HBITMAP bitmap, LPRECT bounds)
{
   BITMAP         bitsInfo;
   HDC            dc, bitmapDC, metaDC;
   HWND           hwnd;

   GetObject(bitmap, sizeof(BITMAP), &bitsInfo);
   bounds->top = bounds->left = 0;
   bounds->right = bitsInfo.bmWidth;
   bounds->bottom = bitsInfo.bmHeight;

   hwnd = GetDesktopWindow();
   dc = GetDC(hwnd);
   bitmapDC = CreateCompatibleDC(dc);
   ReleaseDC(hwnd, dc);
   SelectObject(bitmapDC, bitmap);
   
   metaDC = CreateMetaFile(NULL);
   // PDA:  Required to create a "Standard" clipboard metafile.
   SetMapMode(metaDC, MM_ANISOTROPIC);
   SetWindowExtEx(metaDC, bitsInfo.bmWidth, bitsInfo.bmHeight, NULL);
   // PDA:  Use StrectBlt not BitBlt!!!
   StretchBlt(metaDC, 0, 0, bitsInfo.bmWidth, bitsInfo.bmHeight,
              bitmapDC, 0, 0, bitsInfo.bmWidth, bitsInfo.bmHeight, SRCCOPY);
   DeleteDC(bitmapDC);

   return   (CloseMetaFile(metaDC));
}

/* pixel_convert returns the pixel size from a value in a specific mapping mode. */

static short pixel_convert (HDC hdc, short map_mode, short value)
{
   pg_fixed       ratio, pixels;
   long           dpi;
   short          use_value;

   if (!value)
      return   0;
   if ((use_value = value) < 0)
      use_value = -value;                    

   dpi = (long)GetDeviceCaps(hdc, LOGPIXELSX);
   dpi <<= 16;

   if (map_mode == MM_ISOTROPIC || map_mode == MM_ANISOTROPIC) {
   
      ratio = pgFixedRatio(use_value, 2540);  // = inches
      
      pixels = pgMultiplyFixed(ratio, dpi);
      use_value = (short)HIWORD(pixels);
   }
   else
   if (map_mode == MM_TWIPS) {

      ratio = pgFixedRatio(use_value, 20);  // = inches
      pixels = pgMultiplyFixed(ratio, dpi);
      use_value = (short)HIWORD(pixels);
   }

   return   use_value;
}


// pixels_to_himetric return himetric values for the pixel value

static short pixels_to_himetric (HDC hdc, short value)
{
   pg_fixed       ratio, inches;
   short          dpi;
   short          use_value;

   if (!value)
      return   0;
   if ((use_value = value) < 0)
      use_value = -value;                    

   dpi = (short)GetDeviceCaps(hdc, LOGPIXELSX);

   inches = pgFixedRatio(value, dpi);  // = inches
   ratio = pgFixedRatio(2540, use_value);  // = himetric    
   ratio = pgMultiplyFixed(ratio, inches);
   use_value = (short)HIWORD(ratio);

   return   use_value;
}


/* duplicate_data makes a copy of a HANDLE. */

static HGLOBAL duplicate_data (HGLOBAL data)
{
   HGLOBAL  new_data;
   size_t   datasize;
   
   datasize = GlobalSize(data);
   new_data = GlobalAlloc(GMEM_MOVEABLE, datasize);
   pgBlockMove(GlobalLock(data), GlobalLock(new_data), datasize);
   GlobalUnlock(data);
   GlobalUnlock(new_data);
   
   return   new_data;
}


/* process_cf_text processes text received from the clipboard. */

static HGLOBAL process_cf_text (HGLOBAL data, size_t PG_FAR *text_size)
{
   HGLOBAL      new_data;
   pg_bits8_ptr src, dest;
   size_t       datasize;
   
   datasize = GlobalSize(data);
   new_data = GlobalAlloc(GMEM_MOVEABLE, datasize * sizeof(pg_char));
   
   src = (pg_bits8_ptr) GlobalLock(data);
   dest = (pg_bits8_ptr) GlobalLock(new_data);
   datasize = 0;
   
   while (*src != 0) {
      
      if (*src != 0x0A) {
      
         *dest++ = *src++;
         ++datasize;
      }
      else
         ++src;
   }
   
   GlobalUnlock(data);
   GlobalUnlock(new_data);

#ifdef UNICODE
   dest = (pg_bits8_ptr) GlobalLock(new_data);
   datasize = pgBytesToUnicode(dest, (pg_short_t PG_FAR *)dest, NULL, datasize);
#endif
   
   *text_size = datasize;

   return   new_data;

}


#ifdef UNICODE

/* process_cf_unicode processes knonw unicode text received from the clipboard. */

static HGLOBAL process_cf_unicode (HGLOBAL data, size_t PG_FAR *text_size)
{
   HGLOBAL      new_data;
   pg_char_ptr  src, dest, unicode_ptr;
   size_t       datasize;
   
   datasize = GlobalSize(data);
   new_data = GlobalAlloc(GMEM_MOVEABLE, datasize * sizeof(pg_char));
   
   src = (pg_char_ptr) GlobalLock(data);
   dest = unicode_ptr = (pg_char_ptr) GlobalLock(new_data);
   datasize = 0;
   
   while (*src != 0) {
      
      if (*src != 0x0A) {
      
         *dest++ = *src++;
         ++datasize;
      }
      else
         ++src;
   }
   
   datasize = pgUnicodeToUnicode((pg_short_t*) unicode_ptr, datasize, FALSE);
   unicode_ptr[datasize] = 0;
   GlobalUnlock(data);
   GlobalUnlock(new_data);
   
   *text_size = datasize;

   return   new_data;

}
#endif


// process_text_out prepares raw text for the clipboard

static HGLOBAL process_text_out (memory_ref ref, pg_boolean convert_to_bits8)
{
   HGLOBAL        result;
   pg_char_ptr    text, first_text;
   size_t         src_text_size, cr_size;
   
   src_text_size = GetMemorySize(ref);
   text = first_text = (pg_char_ptr) UseMemory(ref);
   cr_size = 0;
   
   while (*text) {
      
      if (*text++ == 0x0D)
         if (*text != 0x0A)
            cr_size += 1;
   }
   
   result = GlobalAlloc(GMEM_MOVEABLE, (src_text_size + cr_size) * sizeof(pg_char));
   text = (pg_char_ptr) GlobalLock(result);
   
   src_text_size = 0;

   while (*first_text) {
      
      *text++ = *first_text;
      src_text_size += 1;

      if (*first_text == 0x0D)
         if (first_text[1] != 0x0A) {

            *text++ = 0x0A;
            src_text_size += 1;
      }

      ++first_text;
   }
   
   *text = 0;

   UnuseMemory(ref);
   GlobalUnlock(result);

#ifdef UNICODE
   if (convert_to_bits8) {
      pg_bits8_ptr      bytes;

      text = (pg_char_ptr) GlobalLock(result);
      src_text_size = pgUnicodeToBytes((pg_short_t*)text, (pg_bits8_ptr)text, NULL, src_text_size);
      bytes = (pg_bits8_ptr)text;
      bytes[src_text_size] = 0;
      GlobalUnlock(result);
   }
#endif
   return      result;
}

#endif

#endif
