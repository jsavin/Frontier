/* 
	Paige Header/Footer API file
	
	File:		pgHdrFtr.h

	by T. R. Shaw

	Copyright © 1994-1995 OITC, Inc.
	All rights reserved.
	
	pgHdrFtr.h - Header/footer routines for translation of RTF files to Paige.

	This software has been privately developed and falls within 
	DFARS 252.227-7013(c)(1) and associated regulations and it or its
	derivatives may not be used outside of the License agreement with 
	T. R. Shaw and OITC.  Copyright may not be removed.
	
	oitc@iu.net

	Revision History
	07/03/95	1.0b1	TRS - Initial release
*/


#ifndef PGHDRFTR_H
#define PGHDRFTR_H

#include "Paige.h"

#define MAX_SUB_DOC			0x00007FFF			// The document's maximum subdocument (RTF enhancement) 
#define SPCL_SECTION_MASK	0xFFFF0000			// Special section mask (RTF enhancement) 
#define HDR_FTR_FLAG		(long)0xC0000000	// Special header/footer flag (RTF enhancement) 
#define MAIN_HEADER			(long)0xC0000000	// The document's header (RTF enhancement) 
#define LEFT_HEADER			(long)0xC0000001	// The document's left header (RTF enhancement) 
#define RIGHT_HEADER		(long)0xC0000002	// The document's right header (RTF enhancement) 
#define FIRST_HEADER		(long)0xC0000003	// The document's first header (RTF enhancement) 
#define MAIN_FOOTER			(long)0xD0000000	// The document's footer (RTF enhancement) 
#define LEFT_FOOTER			(long)0xD0000001	// The document's left footer (RTF enhancement) 
#define RIGHT_FOOTER		(long)0xD0000002	// The document's right footer (RTF enhancement) 
#define FIRST_FOOTER		(long)0xD0000003	// The document's first footer (RTF enhancement) 

#define DEFAULT_HDR_FTR_SIZE	1440;		// Default header/footer size (approx 1 inch)

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (pg_ref) pgNewHeaderFooter(pg_ref pg, long section_type);
extern PG_PASCAL (pg_ref) pgGetHeaderFooter(pg_ref pg, long section_type, pg_boolean resolve);
extern PG_PASCAL (pg_fixed) pgGetHeaderFooterLoc(pg_ref pg, long section_type);
extern PG_PASCAL (pg_ref) pgSetHeaderFooter(pg_ref pg, long section_type, pg_ref new_ref);
extern PG_PASCAL (void) pgSetHeaderFooterLoc(pg_ref pg, long section_type, pg_fixed loc);
extern PG_PASCAL (void) pgPageModifyHdrFtr(paige_rec_ptr pg, long page_num, rectangle_ptr margins);
extern PG_PASCAL (pg_error) pgSaveHdrFtr (pg_ref pg, long PG_FAR *file_position, const pg_file_key_ptr keys,
		pg_short_t num_keys, file_io_proc write_proc, file_io_proc write_data_proc, 
		file_ref filemap, long doc_element_info);
extern PG_PASCAL (pg_error) pgReadHdrFtr (pg_ref pg, long PG_FAR *file_position, const pg_file_key_ptr keys,
		pg_short_t num_keys, file_io_proc read_proc, file_ref filemap);
extern PG_PASCAL (short) pgNumAugmentedPages (pg_ref pg);

#ifdef __cplusplus
	}
#endif

#endif


