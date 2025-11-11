/* Import-Export module. Copyright 1995-96 by DataPak Software, Inc.  This software is a
part of the total PAIGE library.

This source file contains all the member functions for the native import/export class (derived
from the PaigeImportFilter and PaigeExportFilter classes. */


#include "Paige.h"
#include "defprocs.h"
#include "pgExceps.h"
#include "pgUtils.h"
#include "pgTxrCPP.h"
#include "pgErrors.h"
#include "pgEmbed.h"
#include "pgdeftbl.h"
#include "pgFiles.h"

/* PaigeImportFilter constructor. This sets up the various members common to all filters.
No error checking or file verification occurs just yet. */

PaigeNativeImportFilter::PaigeNativeImportFilter ()
{
   file_type = pg_paige_type; // Default type for this class
   feature_bits = IMPORT_TEXT_FEATURE | IMPORT_TEXT_FORMATS_FEATURE
               | IMPORT_PAR_FORMATS_FEATURE | IMPORT_PAGE_INFO_FEATURE
               | IMPORT_EMBEDDED_OBJECTS_FEATURE | IMPORT_PAGE_GRAPHICS_FEATURE
               | IMPORT_CACHE_CAPABILITY;
}

/* PaigeImportFilter destructor. This removes common items. */

PaigeNativeImportFilter::~PaigeNativeImportFilter ()
{

}


/* pgPrepareImport sets up the read handlers to omit anything caller does not want. */

pg_error PaigeNativeImportFilter::pgPrepareImport (void)
{
   import_pg_rec->io_mask_bits = (import_bits ^ IMPORT_EVERYTHING_FLAG);

   return   NO_ERROR;
}



/* pgVerifySignature returns TRUE if the file is a valid PAIGE file. */

pg_error PaigeNativeImportFilter::pgVerifySignature ()
{
   return   ::pgVerifyFile((file_ref)filemap, io_proc, file_begin);
}


/* pgReadNextBlock for PaigeNativeImportFilter actually calls pgReadDoc for the "import". If
the whole document is to be imported, then import_pg is the original pg_ref.  Or, if this
is an import into an existing pg_ref then we create a temporary pg_ref that will get pasted in. */

pg_boolean PaigeNativeImportFilter::pgReadNextBlock (void)
{
   pg_file_key_ptr            key_list = NULL;
   short                num_keys = 0;

   if (bytes_imported == 0) {  // first time through
      
        pgInitEmbedProcs(paige_globals, (embed_callback)import_pg_rec->globals->embed_callback_proc, NULL);
      
      if (import_bits & IMPORT_NO_VISAREA)
         import_pg_rec->flags2 |= NO_READVIS_BIT;

      if (import_bits & IMPORT_CACHE_FLAG)
         io_result = ::pgCacheReadDoc(import_pg, &file_begin, key_list, num_keys, io_proc, filemap);
      else
         io_result = ::pgReadDoc(import_pg, &file_begin, key_list, num_keys, io_proc, filemap);

      translator.bytes_transferred = 0;
      text_position = import_pg_rec->t_length;
      bytes_imported = import_pg_rec->t_length;
      
      {
         par_info_ptr   pars;
         style_info_ptr  styles;
         font_info_ptr  font;
         pg_short_t     font_index;
         long        qty, font_qty;
         
         if (GetMemorySize(import_pg_rec->par_formats) == 0) {
            
            SetMemorySize(import_pg_rec->par_formats, 1);
            pars = (par_info_ptr)UseMemory(import_pg_rec->par_formats);
            *pars = import_pg_rec->globals->def_par;
            UnuseMemory(import_pg_rec->par_formats);
         }

         if (GetMemorySize(import_pg_rec->t_formats) == 0) {
            
            SetMemorySize(import_pg_rec->t_formats, 1);
            styles = (style_info_ptr)UseMemory(import_pg_rec->t_formats);
            *styles = import_pg_rec->globals->def_style;
            UnuseMemory(import_pg_rec->t_formats);
         }
         
         file_os = pgGetOSConstant(import_pg_rec->platform);

      // Map the fonts if appropriate
         
         font = (font_info_ptr)UseMemory(import_pg_rec->fonts);
         
         for (qty = font_qty = GetMemorySize(import_pg_rec->fonts); qty; ++font, --qty) {
            
            if (file_os != CURRENT_OS)
               this->pgMapFont(font, file_os, CURRENT_OS);

            font->environs &= (~(FONT_GOOD | FONT_USES_ALTERNATE));
            import_pg_rec->procs.font_proc(import_pg_rec, font);
         }

          font = (font_info_ptr)UseMemoryRecord(import_pg_rec->fonts, 0, 0, FALSE);
         styles = (style_info_ptr)UseMemory(import_pg_rec->t_formats);
       
         for (qty = GetMemorySize(import_pg_rec->t_formats); qty; ++styles, --qty) {
            
            if ((font_index = styles->font_index) == DEFAULT_FONT_INDEX)
               font_index = 0;
              else
              if (font_index >= (pg_short_t)font_qty) {
                  
                  font_index = (pg_short_t)(font_qty - 1);
               styles->font_index = font_index;          
              }
      
            styles->procs.init(import_pg_rec, styles, &font[font_index]);
         }

         UnuseMemory(import_pg_rec->t_formats);
         UnuseMemory(import_pg_rec->fonts);
      }
   }

   import_pg_rec->platform = PAIGE_PLATFORM;

   return   FALSE;
}


/* pgImportDone for the native filter performs a pgPaste() if we had to create a temporary
holding ref. */

pg_error PaigeNativeImportFilter::pgImportDone (void)
{
   return   NO_ERROR;
}


/*******************************  Export  **************************/


PaigeNativeExportFilter::PaigeNativeExportFilter()
{
   file_type = pg_paige_type; // Default type for this class
   feature_bits = EXPORT_TEXT_FEATURE | EXPORT_TEXT_FORMATS_FEATURE
               | EXPORT_PAR_FORMATS_FEATURE | EXPORT_PAGE_INFO_FEATURE
               | EXPORT_EMBEDDED_OBJECTS_FEATURE | EXPORT_PAGE_GRAPHICS_FEATURE;

   file_version = PAIGE_VERSION;
   pgBlockMove((void PG_FAR *)FILETYPE_PAIGE, (void PG_FAR *)file_kind, FILETYPE_STR_SIZE * sizeof(pg_char));
}


PaigeNativeExportFilter::~PaigeNativeExportFilter()
{

}


/* pgPrepareExport sets up the pg_ref,  and it could be "selection only," meaning
a copy. */

pg_error PaigeNativeExportFilter::pgPrepareExport (void)
{
   copied_ref = MEM_NULL;
   
   if (export_range.begin != 0 || export_range.end != export_pg_rec->t_length) {
   
      PG_TRY (paige_globals->mem_globals) {
         
         copied_ref = pgCopy(export_pg, &export_range);
         pgFailZero(paige_globals->mem_globals, (generic_var)copied_ref);
      }
      
      PG_CATCH {
         
         return   NO_MEMORY_ERR;
      }
      
      PG_ENDTRY;
   }

   export_pg_rec->io_mask_bits = (export_bits ^ EXPORT_EVERYTHING_FLAG);

   return   NO_ERROR;
}


/* pgWriteNextBlock gets called to output blocks of text. In this case, however,
we just dump the whole file with pgSaveDoc() and return FALSE (done). */

pg_boolean PaigeNativeExportFilter::pgWriteNextBlock (void)
{
   pg_ref         ref_to_save;
   
   if ((ref_to_save = copied_ref) == MEM_NULL)
      ref_to_save = export_pg;
   
   io_result = pgSaveDoc(ref_to_save, &filepos, NULL, 0, io_proc, filemap, 0);
   
   if (io_result == NO_ERROR)
      if (feature_bits & EXPORT_EMBEDDED_OBJECTS_FLAG)
         io_result = pgSaveAllEmbedRefs(ref_to_save, io_proc, NULL, &filepos, filemap);

   if (io_result == NO_ERROR)
      pgTerminateFile (ref_to_save, &filepos, io_proc, filemap);
      
   return   FALSE;
}


/* pgExportDone is called after the file is complete. */

pg_error PaigeNativeExportFilter::pgExportDone (void)
{
   if (copied_ref)
      pgDispose(copied_ref);

   return   io_proc(NULL, io_set_eof, &filepos, &filepos, filemap);
}


/************************************* Local Functions *************************************/



