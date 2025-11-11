
/* This file provides support for machine-specific code (but on its own has
no machine-specific functions except when mapped through macros). */

/* Fixed fixed rounding, 28 Dec 94 OITC (oitc@iu.net) */
/* String conversion routines, 7 Feb 95 OITC (oitc@iu.net) */
/* I/O handling, Jun 95 OITC (oitc@iu.net) */

#include "Paige.h"
#include "defprocs.h"
#include "machine.h"
#include "pgRegion.h"
#include "pgSelect.h"
#include "pgUtils.h"
#include "pgTxtWid.h"
#include "pgText.h"
#include "pgOSUtl.h"
#include "pgIO.h"
#include "pgErrors.h"
#include "pgSubref.h"
#include "pgTraps.h"

#ifndef DEF_POINT_SIZE
#define DEF_POINT_SIZE 12
#endif
#ifndef DEF_CHAR_WIDTH
#define DEF_CHAR_WIDTH 12
#endif

static long find_next_word (paige_rec_ptr pg, style_walk_ptr walker,
		pg_char_ptr text, long global_offset, long local_offset, long end_offset,
		pg_short_t PG_FAR *begin, pg_short_t PG_FAR *end, long PG_FAR *terminator_flags);
static void intersect_page_modify (paige_rec_ptr pg, rectangle_ptr page_bounds,
			pg_scale_ptr scaler, co_ordinate_ptr offset, long page_num, pg_region rgn);
static long compute_subref_level (paige_rec_ptr pg);


/* same_case_length returns the length of bytes in data that are of the same
upper/lower case. The *info_flags gets set to charInfo type of the "run".
The other params are required for a sensible call to CharInfo. */

PG_PASCAL (long) pgSameCaseLength (paige_rec_ptr pg, style_walk_ptr walker,
		pg_char_ptr data, long global_offset, long length,
		long PG_FAR *info_flags)
{
	register long   offset_ctr, max_count;
	long		    true_flags;

	true_flags = walker->cur_style->procs.char_info(pg, walker, data, NO_BLOCK_OFFSET,
				0, length, 0, UPPER_CASE_BIT);
	*info_flags = true_flags;

	max_count = length;

	for (offset_ctr = 0; offset_ctr < max_count; ++offset_ctr)
		if (walker->cur_style->procs.char_info(pg, walker, data, NO_BLOCK_OFFSET,
				0, length, offset_ctr, UPPER_CASE_BIT) != true_flags)
			break;

	return  offset_ctr;
}


/* pgFixFontName forces the name in the font to be a pascal string but
zero-terminated. Hence for platforms that require cstrings the name is
addressed as &name[1]. */

PG_PASCAL (void) pgFixFontName (font_info_ptr font)
{
	register short		  length;

	if (font->environs & NAME_IS_CSTR) {
		
		length = (short)pgCStrLength((pg_c_string_ptr)font->name);
		if (length) {
			
			if (length >= (FONT_SIZE - 1))
				length = FONT_SIZE - 2;

			pgBlockMove(&font->name[length], &font->name[length + 1], -(long)(length * sizeof(pg_char)));
		}
		
		font->name[0] = (pg_char)length;
		font->name[length + 1] = 0;

		font->environs &= (~NAME_IS_CSTR);
	}

	if (font->environs & NAME_ALT_IS_CSTR) {

		length = (short)pgCStrLength((pg_c_string_ptr)font->alternate_name);
		if (length) {
			
			if (length >= (FONT_SIZE - 1))
				length = FONT_SIZE - 2;

			pgBlockMove(&font->alternate_name[length], &font->alternate_name[length + 1], -(long)(length * sizeof(pg_char)));
		}
		
		font->alternate_name[0] = (pg_char)length;
		font->alternate_name[length + 1] = 0;

		font->environs &= (~NAME_ALT_IS_CSTR);
	}
}

/* pgInitDefaultStyle must initialize a style record to the default settings.
You should leave the font index field alone. NOTE: Function pointers in
the style record may not be valid when this call is made.  NOTE 2: Upon entry,
the entire record has been filled with zeros. NOTE 3: The font provided is
for reference only and has already been initialized from pgInitDefaultFont
(above). The pgInitDefaultStyle is always called after pgInitDefaultFont. */

PG_PASCAL (void) pgInitDefaultStyle (const pg_globals_ptr globals, style_info_ptr style, font_info_ptr def_font)
{
#ifdef MAC_PLATFORM
#pragma unused (globals, def_font)
#endif
	style->bk_color.red = style->bk_color.blue = style->bk_color.green = 0xFFFF;
	style->point = DEF_POINT_SIZE << 16;
	style->char_width = DEF_CHAR_WIDTH << 16;
	style->machine_var = 0;
}


/* pgInitDefaultPar sets up any defaults for par_info */

PG_PASCAL (void) pgInitDefaultPar (const pg_globals_ptr globals, par_info_ptr def_par)
{
#ifdef MAC_PLATFORM
#pragma unused(globals)
#endif

	def_par->def_tab_space = globals->def_tab_space;
}


/* pgBitmapModifyProc is the default modify-bitmap-proc, which does nothing
by default.  This gets called twice, once before drawing to the bitmap
(post_call == FALSE) and once after drawing (post_call == TRUE).  The text_offset
param is the next text location (which will be the start of the line if post_call
is FALSE and the end of line of post_call is TRUE).  NOTE: The bits_rect is the
actual screen (eventual) target, not the local bitmap rect. To get the actual
bitmap rect offset by screen_offset. */

PG_PASCAL (void) pgBitmapModifyProc (paige_rec_ptr pg, graf_device_ptr bits_port,
		pg_boolean post_call, rectangle_ptr bits_rect, co_ordinate_ptr screen_offset,
		long text_position)
{
	if (pg->bk_image) {
		pg_url_image_ptr		image;
		
		image = (pg_url_image_ptr) UseMemory(pg->bk_image);
		
		if (!image->image_data && !image->loader_result)
			pg->procs.background_image(pg, image, (generic_var)pg->port.machine_ref,
									load_background_image, NULL, NULL, screen_offset);
		
		if (image->image_data) {
		
		}
		
		UnuseMemory(pg->bk_image);
	}
}


/* pgBkImageProc is the background imager.  We can handle Mac PICT and Windows metafiles,
otherwise the app needs to translate. */

PG_PASCAL (void) pgBkImageProc (paige_rec_ptr pg, pg_url_image_ptr image,
		generic_var device, short verb, rectangle_ptr target, rectangle_ptr clip,
		co_ordinate_ptr actual_target_offset)
{
	
}


/* pgSetDrawingDevice sets a temporary platform-specific device that will be used for
subsequent display. The previous device, if any, is returned from this function, and it
must be balanced with a later call to pgReleaseDrawingDevice below. */

PG_PASCAL (generic_var) pgSetDrawingDevice (pg_ref pg, const generic_var draw_device)
{
	paige_rec_ptr				pg_rec;
	generic_var					previous_device;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

#ifdef MAC_PLATFORM
	previous_device = (generic_var)pg_rec->port.machine_var;
	pg_rec->port.machine_var = draw_device;
#endif

#ifdef WINDOWS_PLATFORM
	previous_device = (generic_var)pg_rec->port.machine_ref;
	pg_rec->port.machine_ref = draw_device;
	++pg_rec->port.access_ctr;
#endif

	UnuseMemory(pg);
	
	return	previous_device;
}


/* pgSetScaledDrawingDevice does the same thing as pgSetDrawingDevice except PAIGE sets up the
device to scale. */

PG_PASCAL (generic_var) pgSetScaledDrawingDevice (pg_ref pg, const generic_var draw_device, pg_scale_ptr scale)
{
	paige_rec_ptr			pg_rec;
	generic_var				old_device, machine_var;
	pg_scale_factor			old_scale;
	size_t					machine_ref3;
	long					access_ctr;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	old_device = (generic_var)pg_rec->port.machine_ref;
	access_ctr = pg_rec->port.access_ctr;
	machine_ref3 = pg_rec->port.machine_ref3;
	machine_var = pg_rec->port.machine_var;
	old_scale = pg_rec->port.scale;

	pg_rec->port.machine_ref = MEM_NULL;
	pg_rec->port.machine_var = (generic_var)MEM_NULL;
	pg_rec->port.machine_ref3 = (long)draw_device;
	pg_rec->port.scale = *scale;
	pg_rec->port.access_ctr = 0;
	
	pgGetPlatformDevice(&pg_rec->port);
	
	pg_rec->port.access_ctr = access_ctr + 1;
	pg_rec->port.scale = old_scale;
	pg_rec->port.machine_var = machine_var;
	pg_rec->port.machine_ref3 = machine_ref3;

	UnuseMemory(pg);
	
	return			old_device;
}


/* pgReleaseDrawingDevice restores the old device in the pg_ref. */

PG_PASCAL (void) pgReleaseDrawingDevice (pg_ref pg, const generic_var previous_device)
{
	paige_rec_ptr				pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);

#ifdef MAC_PLATFORM
	pg_rec->port.machine_var = previous_device;
#endif

#ifdef WINDOWS_PLATFORM
	pg_rec->port.machine_ref = previous_device;
	
	if ((pg_rec->port.access_ctr -= 1) < 0)
		pg_rec->port.access_ctr = 0;
#endif

	UnuseMemory(pg);
}


/* pgGlobalStrCopy copies a symbol from globals (4 byte max) indicated at
src_str to target_str and returns the copied size, or if target_str is NULL
just the source size is returned. The max_target_size param indicates the
maximum size of bytes to copy. Note that src_str must point to the first
char of the global string, e.g. &str[1] and it is assumed to be zero terminated
or 3 chars. Also, if max_target_size is zero, no max size is assumed. */

PG_PASCAL (short) pgGlobalStrCopy (pg_char_ptr src_str, pg_char_ptr target_str, short max_target_size)
{
	register pg_char_ptr	    src_ptr, dest_ptr;
	register short			  	target_ctr, target_qty;
	
	if (max_target_size < 2) {
		
		*target_str = *src_str;
		return	1;
	}
	
	src_ptr = src_str;
	if (!(dest_ptr = target_str))
		dest_ptr = src_ptr;
	
	target_ctr = max_target_size;

	if ( (target_ctr > (3)) || !target_ctr)
		target_ctr = 3;
	
	for (target_qty = 0; target_ctr; ++target_qty, --target_ctr)
		if (!(dest_ptr[target_qty] = src_ptr[target_qty]))
			break;

	return  target_qty;
}


/* pgBytesToUnicode converts 8-bit ASCII to 16-bit Unicode. The destination buffer
must have room for the resulting output.  If the input is already
unicode we just copy the bytes to the destination.  If output_chars is null
then no translation is performed, rather a test for unicode or  not is returned.
Note, the input_byte_size is a BYTE count, not a character count. RELEASE NOTE: This
function works correctly even for non-unicode libraries. */

PG_PASCAL (size_t) pgBytesToUnicode (pg_bits8_ptr input_bytes, pg_short_t PG_FAR *output_chars,
		font_info_ptr font, size_t input_byte_size)
{
	size_t						result = 0;
	register pg_short_t PG_FAR	*output;
	register pg_bits8_ptr		input;
	pg_short_t PG_FAR			*test_ptr;
	size_t						index;
	int							is_unicode;

	if (!(result = input_byte_size))
		return	0;
	
	if (input_byte_size & 1)
		is_unicode = FALSE;
	else {
		
		test_ptr = (pg_short_t PG_FAR *)input_bytes;
		is_unicode = (int)(*test_ptr == PG_BOM || *test_ptr == PG_REVERSE_BOM);
	}

	if (is_unicode) {
		
		if (output_chars != NULL)
			pgBlockMove(input_bytes, output_chars, input_byte_size);

		result /= sizeof(pg_short_t);
	}
	else
	if (output_chars == NULL)
		result = FALSE;
	else {

		input = input_bytes;
		output = output_chars;
		input += input_byte_size;
		output += input_byte_size;
		
		for (index = input_byte_size; index; --index)
			*(--output) = (pg_short_t) *(--input);
	}

	return	result;
}


/* pgUnicodeToBytes converts 16 bit ASCII to 8 bit ASCII. Note that input_chars is a CHARACTER
count, not byte count. The function returns a byte count of valid chars. RELEASE NOTE: This
function works correctly even for non-unicode libraries. */

PG_PASCAL (size_t) pgUnicodeToBytes (pg_short_t PG_FAR *input_chars, pg_bits8_ptr output_bytes,
		font_info_ptr font, long input_char_size)
{
	register		pg_short_t PG_FAR	*input;
	register		pg_bits8_ptr		output;
	pg_short_t		input_char, bom;
	long			index, bytecount, input_size;
	
	input = input_chars;
	output = output_bytes;
	input_size = input_char_size;
	bom = *input_chars;

	if (bom == PG_BOM || bom == PG_REVERSE_BOM) {
		
		++input;
		--input_size;
	}

	for (index = bytecount = 0; index < input_size; ++index) {

		input_char = *input++;
		
		if (bom == PG_REVERSE_BOM) {
			pg_short_t		lobyte, hibyte;
			
			lobyte = hibyte = input_char;
			lobyte >>= 8;
			hibyte <<= 8;
			input_char = lobyte | hibyte;
		}

		*output++ = (pg_bits8)input_char;
		++bytecount;
	}

	return		bytecount;
}


/* pgUnicodeToUnicode accepts a string of unicode chars, removes a BOM, if any,
and reverses the byte order if required.  The function result is the final character count
(which will be num_chars if no BOM, or num_chars - 1 if a BOM).
Release note: This function works even if this lib is NOT Unicode-enabled. */

PG_PASCAL (size_t) pgUnicodeToUnicode (pg_short_t PG_FAR *the_chars, size_t num_chars, pg_boolean force_reverse)
{
	pg_short_t		byte_order_mark;
	register size_t	bytecount;
	size_t			char_count;

	if ((char_count = num_chars) > 0) {
	
		byte_order_mark = *the_chars;

		if (byte_order_mark == PG_BOM || byte_order_mark == PG_REVERSE_BOM) {
			
			if ((char_count -= 1) > 0) {
				
				bytecount = char_count * sizeof(pg_char);
				pgBlockMove(&the_chars[1], the_chars, bytecount);
				the_chars[char_count] = 0;
			}
		}
		
		if (force_reverse || byte_order_mark == PG_REVERSE_BOM) {
			register pg_bits8_ptr	bytes;
			pg_bits8				hold_byte;

			bytecount = char_count * 2;
			bytes = (pg_bits8_ptr)the_chars;

			while (bytecount) {
				
				hold_byte = bytes[1];
				bytes[1] = bytes[0];
				bytes[0] = hold_byte;

				bytes += 2;
				bytecount -= 2;
			}
		}
	}

	return	char_count;
}


/* pgStandardReadProc is the default file-read function. The I/O functions are
macros that need to be mapped to the machine (see pgMTraps.h).  */

PG_PASCAL (pg_error) pgStandardReadProc (void PG_FAR *data, short verb, long PG_FAR *position,
		long PG_FAR *data_size, file_ref filemap)
{
	pg_file_unit	f_ref;
	pg_error		error;
	pg_bits8_ptr	data_ptr;
	
	GetMemoryRecord(filemap, 0, (void PG_FAR *)&f_ref);
    error = NO_ERROR;
	
	if (verb == io_file_unit) {
		
		*((pg_file_unit PG_FAR *)data) = f_ref;
		return	NO_ERROR;
	}
	else
	if (verb == io_get_eof) {
		
		error = pgGetFileEOF(f_ref, (long PG_FAR *)data);	//ее TRS/OITC

		if (error)
			return  pgProcessError(error);
	}
	else {
		
		error = pgSetFilePos(f_ref, *position);	//ее TRS/OITC
		
		if (error)
			return  pgProcessError(error);
		
		if (verb != io_set_fpos) {
		
			if (verb == io_data_indirect) {
	
				SetMemorySize((memory_ref) data, *data_size);
				data_ptr = (pg_bits8_ptr) UseMemory((memory_ref) data);
			}
			else
				data_ptr = (pg_bits8_ptr) data;
	
			error = pgReadFileData(f_ref, *data_size, data_ptr);	//ее TRS/OITC
			
			if (verb == io_data_indirect)
				UnuseMemory((memory_ref) data);
	
			if (error)
				return  pgProcessError(error);
	
			*position += *data_size;
		}
	}

	return  NO_ERROR;
}


/* pgStandardWriteProc is the default file-write function. This is Mac-specific,
but simply change it to match your device.   */

PG_PASCAL (pg_error) pgStandardWriteProc (void PG_FAR *data, short verb, long PG_FAR *position,
		long PG_FAR *data_size, file_ref filemap)
{
	pg_file_unit	   		f_ref;
	pg_error				error;
	pg_bits8_ptr		    data_ptr;
	
	GetMemoryRecord(filemap, 0, (void PG_FAR *)&f_ref);
	error = NO_ERROR;
	
	if (verb == io_file_unit) {
	
		*((pg_file_unit PG_FAR *)data) = f_ref;
		return	NO_ERROR;
	}
	else
	if (verb == io_get_eof) {

		error = pgGetFileEOF(f_ref, (long PG_FAR *) data);	//ее TRS/OITC

		if (error)
			return  pgProcessError(error);
	}
	else
	if (verb == io_set_eof)
		pgSetFileEOF(f_ref, *position);
	else {
	
		error = pgSetFilePos(f_ref, *position);	//ее TRS/OITC
		
		if (error)
			return  pgProcessError(error);

		if (verb != io_set_fpos) {
		
			if (verb == io_data_indirect)
				data_ptr = (pg_bits8_ptr) UseMemory((memory_ref) data);
			else
				data_ptr = (pg_bits8_ptr) data;
	
			error = pgWriteFileData(f_ref, *data_size, data_ptr);	//ее TRS/OITC
			
			if (verb == io_data_indirect)
				UnuseMemory((memory_ref) data);
			
			if (error)
				return  pgProcessError(error);
			
			*position += *data_size;
		}
	}
 
	return  error;
}



/* pgOSReadProc is the same as pgStandardReadProc except the filemap is not a memory_ref,
rather it is a pg_file_unit.  */

PG_PASCAL (pg_error) pgOSReadProc (void PG_FAR *data, short verb, long PG_FAR *position,
		long PG_FAR *data_size, file_ref filemap)
{
	pg_file_unit	f_ref;
	pg_error		error;
	pg_bits8_ptr	data_ptr;
	
	f_ref = (pg_file_unit)filemap;
    error = NO_ERROR;
	
	if (verb == io_file_unit) {

		*((pg_file_unit PG_FAR *)data) = f_ref;
		return	NO_ERROR;
	}
	else
	if (verb == io_get_eof) {
		
		error = pgGetFileEOF(f_ref, (long PG_FAR *)data);	//ее TRS/OITC

		if (error)
			return  pgProcessError(error);
	}
	else
	if (verb != io_set_eof) {
		
		error = pgSetFilePos(f_ref, *position);	//ее TRS/OITC
		
		if (error)
			return  pgProcessError(error);
		
		if (verb != io_set_fpos) {
		
			if (verb == io_data_indirect) {
	
				SetMemorySize((memory_ref) data, *data_size);
				data_ptr = (pg_bits8_ptr) UseMemory((memory_ref) data);
			}
			else
				data_ptr = (pg_bits8_ptr) data;
	
			error = pgReadFileData(f_ref, *data_size, data_ptr);	//ее TRS/OITC
			
			if (verb == io_data_indirect)
				UnuseMemory((memory_ref) data);
	
			if (error)
				return  pgProcessError(error);
	
			*position += *data_size;
		}
	}

	return  NO_ERROR;
}


/* pgOSWriteProc is the same as pgStandardWriteProc except the filemap is not a memory_ref,
rather it is a pg_file_unit.  */

PG_PASCAL (pg_error) pgOSWriteProc (void PG_FAR *data, short verb, long PG_FAR *position,
		long PG_FAR *data_size, file_ref filemap)
{
	pg_file_unit	   			f_ref;
	pg_error					error;
	pg_bits8_ptr		     	data_ptr;
	
	error = NO_ERROR;
	f_ref = (pg_file_unit)filemap;
	
	if (verb == io_file_unit) {

		*((pg_file_unit PG_FAR *)data) = f_ref;
		return	NO_ERROR;
	}
	else
	if (verb == io_get_eof) {

		error = pgGetFileEOF(f_ref, (long PG_FAR *) data);	//ее TRS/OITC

		if (error)
			return  pgProcessError(error);
	}
	else
	if (verb == io_set_eof)
		error = pgSetFileEOF(f_ref, *position);
	else {

		error = pgSetFilePos(f_ref, *position);	//ее TRS/OITC
		
		if (error)
			return  pgProcessError(error);

		if (verb != io_set_fpos) {
		
			if (verb == io_data_indirect)
				data_ptr = (pg_bits8_ptr) UseMemory((memory_ref) data);
			else
				data_ptr = (pg_bits8_ptr) data;
	
			error = pgWriteFileData(f_ref, *data_size, data_ptr);	//ее TRS/OITC
			
			if (verb == io_data_indirect)
				UnuseMemory((memory_ref) data);
			
			if (error)
				return  pgProcessError(error);
			
			*position += *data_size;
		}
	}
 
	return  error;
}


/* SpecialUnderline returns non-zero, with the bits set as #define at the top of
this file, if any special underline styles exist.  */

PG_PASCAL (short) SpecialUnderline (style_info_ptr style)
{
	register style_info_ptr	 draw_style;
	short					   result;
	
	result = 0;
	draw_style = style;
	
	if (draw_style->styles[dbl_underline_var])
		result |= DOUBLE_UNDERLINE;
	if (draw_style->styles[dotted_underline_var])
		result |= GRAY_UNDERLINE;
	if (draw_style->styles[word_underline_var])
		if (!(draw_style->class_bits & CANT_UNDERLINE_BIT))
			result |= WORD_UNDERLINE;
	
	if (draw_style->styles[overline_var])
		result |= OVERLINE_LINE;

	if (!result)
		if (draw_style->class_bits & CANT_UNDERLINE_BIT)
			if (draw_style->styles[underline_var] || draw_style->styles[word_underline_var])
				result |= FAKE_UNDERLINE;

	return  result;
}
 
 
/* PaigeToQDStyle returns a long whose bits are set per QuickDraw
styles based on a Paige style_info record.  */

PG_PASCAL (long) PaigeToQDStyle (const style_info_ptr the_style)
{
	short				   stl_index;
	long				    stl_set, stl_bits;

	stl_set = 0;

	for (stl_bits = 1, stl_index = 0; stl_index < MAX_STYLES; ++stl_index) {
		
		if (the_style->styles[stl_index])
			stl_set |= stl_bits;
			stl_bits = stl_bits << 1;
	}

	return  stl_set;
}


/* QDStyleToPaige sets the styles in the_styles according to the qd style
bits. */

PG_PASCAL (void) QDStyleToPaige (long qd_styles, style_info_ptr the_style)
{
	long				   stl_bits;
	short				   stl_index;

	for (stl_bits = 1, stl_index = 0; stl_index < MAX_STYLES; ++stl_index) {
		
		if (stl_bits & qd_styles)
			the_style->styles[stl_index] = -1;
		else
			the_style->styles[stl_index] = 0;

		stl_bits = stl_bits << 1;
	}
}


/* pgCountCtlChars returns the number if ctl_char occurrences in the text of block. */

PG_PASCAL (pg_short_t) pgCountCtlChars (text_block_ptr block, pg_short_t ctl_char)
{
	register pg_char_ptr			text;
	register size_t					byte_size;
	register pg_short_t				result;

	text = (pg_char_ptr) UseMemory(block->text);
	byte_size = GetMemorySize(block->text);
	
	result = 0;
	
	while (byte_size) {
		
		if (*text++ == ctl_char)
			++result;
		
		--byte_size;
	}

	UnuseMemory(block->text);

	return	result;
}



/* ShapeToRgn fills in a region to match a Paige shape. The resulting region
is offset h and v. Note that regions are limited to 32K each side so the
values are truncated as needed.
Added 7-20-95, if sect_rect is non-NULL the region is also intersected with it. */

PG_PASCAL (void) ShapeToRgn (shape_ref src_shape, long offset_h, long offset_v,
		pg_scale_factor PG_FAR *scale_factor, short inset_amount,
		rectangle_ptr sect_rect, pg_region rgn)
{
	register shape  	PG_FAR  *the_shape;
	rectangle	       	scale_r, src_rect;
	pg_scale_factor 	scale;
	register size_t  	r_qty;
	Rect		    	box;
	co_ordinate	     	adjust;

	adjust.h = offset_h;
	adjust.v = offset_v;
	
	if (scale_factor)
		pgBlockMove(scale_factor, &scale, sizeof(pg_scale_factor));
	else
		scale.scale = 0;

	r_qty = GetMemorySize(src_shape) - SIMPLE_SHAPE_QTY;
	the_shape = (shape *) UseMemory(src_shape);
	++the_shape;
	
	pgSectRect(the_shape, sect_rect, &src_rect);

	if (scale.scale) {
		
		pgScaleRectToRect(&scale, &src_rect, &scale_r, &adjust);
		RectangleToRect(&scale_r, NULL, &box);
	}
	else
		RectangleToRect(&src_rect, &adjust, &box);

	if (inset_amount) {
		
		box.left += inset_amount;
		box.top += inset_amount;
		box.right -= inset_amount;
		box.bottom -= inset_amount;
	}

	pgRectToRgn(rgn, &box);
	
	if (r_qty) {
		pg_region	       add_rgn;
		
		for (add_rgn = pgCreateRgn(); r_qty; --r_qty) {
			
			++the_shape;
			pgSectRect(the_shape, sect_rect, &src_rect);

			if (scale.scale) {
				
				pgScaleRectToRect(&scale, &src_rect, &scale_r, &adjust);
				RectangleToRect(&scale_r, NULL, &box);
			}
			else
				RectangleToRect(&src_rect, &adjust, &box);

			if (inset_amount) {
				
				box.left += inset_amount;
				box.top += inset_amount;
				box.right -= inset_amount;
				box.bottom -= inset_amount;
			}

			pgRectToRgn(add_rgn, &box);
			pgUnionRgn(add_rgn, rgn, rgn);
		}
		
		pgDisposeRgn(add_rgn);
	}

	UnuseMemory(src_shape);
}


/* pgBuildPageRegion builds the runtime region for the page_area. All things are
considered, i.e. the scroll position, origin and repeating shape(s). */

PG_PASCAL (void) pgBuildPageRegion (paige_rec_ptr pg, pg_scale_ptr scaler, pg_region rgn)
{
	pg_region			temp_rgn;
	rectangle			page_bounds, vis_bounds;
	co_ordinate			vis_extra, repeat_offset, page_offset;
	long				repeating, page_num, repeat_width, repeat_height;
	size_t				num_rects;

	vis_extra = pg->scroll_pos;
	pgNegatePt(&vis_extra);
	pgAddPt(&pg->port.origin, &vis_extra);

	if ((repeating = pg->doc_info.attributes & (V_REPEAT_BIT | H_REPEAT_BIT)) != 0) {

		pgShapeBounds(pg->vis_area, &vis_bounds);
		pgShapeBounds(pg->wrap_area, &page_bounds);
		page_offset = vis_bounds.top_left;
		pgSubPt(&vis_extra, &page_offset);
		num_rects = GetMemorySize(pg->wrap_area) - 1;

		page_num = (long)pgPixelToPage(pg->myself, &page_offset, &repeat_offset, &repeat_width, &repeat_height, NULL, FALSE);
		page_num += 1;
		pgAddPt(&repeat_offset, &vis_extra);
		ShapeToRgn(pg->wrap_area, vis_extra.h, vis_extra.v, scaler, 0, NULL, rgn);
		
		temp_rgn = pgCreateRgn();
		pgCopyRgn(rgn, temp_rgn);

		pgOffsetRect(&page_bounds, vis_extra.h, vis_extra.v);
		
		if (num_rects < 2)
			intersect_page_modify (pg, &page_bounds, scaler, &vis_extra, page_num, rgn);
		
	// Scale vis rect in reverse so we get enough clipping area:
	
		if (scaler->scale) {
			pg_scale_factor		vis_scaler;
			
			vis_scaler = *scaler;
			vis_scaler.scale = -vis_scaler.scale;
			pgScaleRect(&vis_scaler, NULL, &vis_bounds);
		}

		for (;;) {
			
			pgOffsetRect(&page_bounds, repeat_width, repeat_height);
			
			if (page_bounds.top_left.v > vis_bounds.bot_right.v
				|| page_bounds.top_left.h > vis_bounds.bot_right.h)
				break;
			
			page_num += 1;
			vis_extra.h += repeat_width;
			vis_extra.v += repeat_height;

			if (scaler->scale)
				ShapeToRgn(pg->wrap_area, vis_extra.h, vis_extra.v, scaler, 0, NULL, temp_rgn);
			else
				pgOffsetRgn(temp_rgn, (short)repeat_width, (short)repeat_height);

			if (num_rects < 2)
				intersect_page_modify (pg, &page_bounds, scaler, &vis_extra, page_num, temp_rgn);
			
			pgUnionRgn(rgn, temp_rgn, rgn);
		}
		
		pgDisposeRgn(temp_rgn);
	}
	else
		ShapeToRgn(pg->wrap_area, vis_extra.h, vis_extra.v, scaler, 0, NULL, rgn);
}


/* pgBuildExclusionRegion builds a region matching all the exclusion rects. This does not
get called unless the exclusion area is non-empty. This function returns if the region
intersects the vis_area at all. */

PG_PASCAL (pg_boolean) pgBuildExclusionRegion (paige_rec_ptr pg, pg_scale_ptr scaler, pg_region rgn)
{
	rectangle		vis_bounds, exclude_bounds;
	co_ordinate		vis_extra;

	vis_extra = pg->scroll_pos;
	pgNegatePt(&vis_extra);
	pgAddPt(&pg->port.origin, &vis_extra);
	pgShapeBounds(pg->vis_area, &vis_bounds);
	pgShapeBounds(pg->exclude_area, &exclude_bounds);
	pgOffsetRect(&exclude_bounds, vis_extra.h, vis_extra.v);

	if (scaler->scale) {
		pg_scale_factor		vis_scaler;
		
		vis_scaler = *scaler;
		vis_scaler.scale = -vis_scaler.scale;
		pgScaleRect(&vis_scaler, NULL, &vis_bounds);
	}

	if (pgSectRect(&exclude_bounds, &vis_bounds, NULL))
		ShapeToRgn(pg->exclude_area, vis_extra.h, vis_extra.v, scaler, 0, NULL, rgn);
	else
		pgSetEmptyRgn(rgn);
	
	return	(pg_boolean)!pgEmptyRgn(rgn);
}


/* pgVisRegionChanged returns TRUE if the vis_region is different than the last time
we set it up. If update_to_current is TRUE then all the info is brought up to date. */

PG_PASCAL (pg_boolean) pgVisRegionChanged (paige_rec_ptr pg, pg_boolean update_to_current)
{
	pg_boolean			result;
	rectangle_ptr		bounds;

	result = (pg_boolean)(pg->port.clip_info.change_flags != 0);

	if (!result) {
	
		result |= (pg_boolean)(pg->port.clip_info.clip_origin.h != pg->port.origin.h);
		result |= (pg_boolean)(pg->port.clip_info.clip_origin.v != pg->port.origin.v);
		result |= (pg_boolean)(pg->port.clip_info.scroll_pos.h != pg->scroll_pos.h);
		result |= (pg_boolean)(pg->port.clip_info.scroll_pos.v != pg->scroll_pos.v);
		result |= (pg_boolean)(pg->port.clip_info.scale != pg->port.scale.scale);

		if (!result) {

			if (!(pg->doc_info.attributes & NO_CLIP_PAGE_AREA)) {

				bounds = (rectangle_ptr) UseMemory(pg->wrap_area);
				result |= !pgEqualStruct(bounds, &pg->port.clip_info.page_bounds, sizeof(rectangle));
				UnuseMemory(pg->wrap_area);
			}
			
			if (!result) {
			
				bounds = (rectangle_ptr) UseMemory(pg->vis_area);
				result |= !pgEqualStruct(bounds, &pg->port.clip_info.vis_bounds, sizeof(rectangle));
				UnuseMemory(pg->vis_area);
			}
		}
	}

	if (update_to_current) {
	
		pg->port.clip_info.clip_origin = pg->port.origin;
		pg->port.clip_info.scroll_pos = pg->scroll_pos;
		pgShapeBounds(pg->vis_area, &pg->port.clip_info.vis_bounds);
		pgShapeBounds(pg->wrap_area, &pg->port.clip_info.page_bounds);
		pg->port.clip_info.scale = pg->port.scale.scale;
		pg->port.clip_info.change_flags = 0;
	}
	
	return		result;
}


/* ScaleRect scales a Mac Rect per the specified scaling factor.  */

PG_PASCAL (void) ScaleRect (pg_scale_ptr scale_factor, Rect PG_FAR *r)
{
	rectangle	       paige_r;
	
	RectToRectangle(r, &paige_r);
	pgScaleRect(scale_factor, NULL, &paige_r);
	RectangleToRect(&paige_r, NULL, r);
}


/* RectToRectangle converts a RECT to a Paige rectangle */

PG_PASCAL (void) RectToRectangle (Rect PG_FAR *r, rectangle_ptr pg_rect)
{
	pg_rect->top_left.h = r->left;
	pg_rect->top_left.v = r->top;
	pg_rect->bot_right.h = r->right;
	pg_rect->bot_right.v = r->bottom;
}


/* RectangleToRect converts a Paige rectangle to a RECT */

PG_PASCAL (void) RectangleToRect (rectangle_ptr pg_rect, co_ordinate_ptr offset,
		Rect PG_FAR *r)
{
	register long			   extra_h, extra_v;
	register Rect	   PG_FAR  *r_ptr;

	if (offset) {
		
		extra_h = offset->h;
		extra_v = offset->v;
	}
	else
		extra_h = extra_v = 0;

	r_ptr = r;

#ifdef WIN32_COMPILE
	r_ptr->left = pg_rect->top_left.h + extra_h;
	r_ptr->top = pg_rect->top_left.v + extra_v;
	r_ptr->right = pg_rect->bot_right.h + extra_h;
	r_ptr->bottom = pg_rect->bot_right.v + extra_v;
#else
	r_ptr->left = pgLongToShort(pg_rect->top_left.h + extra_h);
	r_ptr->top = pgLongToShort(pg_rect->top_left.v + extra_v);
	r_ptr->right = pgLongToShort(pg_rect->bot_right.h + extra_h);
	r_ptr->bottom = pgLongToShort(pg_rect->bot_right.v + extra_v);
#endif
}


/* pgDrawWordUnderline takes what was just drawn and performs "word underline." The
text is already on the screen, so this needs to happen by measuring the char
widths to figure out where to draw. Params have changed as of 4-6-95 to allow "real"
measuring of char widths (due to small caps, all caps, etc.).  */

PG_PASCAL (void) pgDrawWordUnderline (paige_rec_ptr pg, style_walk_ptr walker,
		short draw_bits, pg_char_ptr text, pg_short_t offset, pg_short_t length,
		long extra, draw_points_ptr draw_position)
{
	memory_ref		      	char_locs;
	register long	  		PG_FAR *locs;
	register long	   		offset_ctr, end_offset;
	long					word_ending;
	pg_short_t		      	word_begin, word_end, word_loc_start, word_loc_end;
	Point			   		start_pt, draw_pt;

	start_pt.h = pgLongToShort(draw_position->from.h);
	start_pt.v = pgLongToShort(draw_position->to.v);
	
	char_locs = pgGetSpecialLocs(pg, draw_position->block, draw_position->starts,
			length, extra, COMPENSATE_SCALE);
	locs = (long *) UseMemory(char_locs);

	offset_ctr = offset;
	end_offset = offset_ctr + length;

	draw_pt = start_pt;
	
	while (offset_ctr < end_offset) {

		offset_ctr = find_next_word(pg, walker, text, draw_position->block->begin, offset_ctr,
			end_offset, &word_begin, &word_end, &word_ending);
		
		if (word_end != word_begin) {
			
			word_loc_start = (short)(word_begin - offset);
			word_loc_end = (short)(word_end - offset);
			
			if (word_ending & TAB_BIT)
				--word_loc_end;

			draw_pt.h = start_pt.h + (short)locs[word_loc_start];
			pgDrawSpecialUnderline(pg, draw_pt, (short)(locs[word_loc_end] - locs[word_loc_start]),
					walker->cur_style, draw_bits);
		}
	}

	UnuseMemory(char_locs);
	DisposeMemory(char_locs);
}


/* Function returns TRUE if given color is "transparent." For mac this is "white". */

PG_PASCAL (pg_boolean) pgTransColor (pg_globals_ptr globals, color_value_ptr color)
{
	return  pgEqualColor(color, &globals->trans_color);
}


/* pgEqualColor returns TRUE if the two colors are equal. */

PG_PASCAL (pg_boolean) pgEqualColor (color_value_ptr color1, color_value_ptr color2)
{
	return ((color1->red == color2->red)
			&& (color1->green == color2->green)
			&& (color1->blue == color2->blue));
}


#ifdef NO_OS_INLINE

/* pgRoundFixed rounds the fixed number to the nearest whole (but is still a
pg_fixed). For example, 0x00018000 will return as 0x00020000. */

//е TRS/OITC

PG_PASCAL (pg_fixed) pgRoundFixed (pg_fixed fix)
{
	return  (pg_fixed)((fix + 0x00008000) & 0xFFFF0000);
}

#endif


/* pgDeviceResolution returns the pixels per inch of the device in question.
The vertical resolution is returned in the high word and horizontal
resolution in the low word. */

PG_PASCAL (long) pgDeviceResolution (graf_device_ptr device)
{
	return		device->resolution;
}



/* pgComputePointSize returns the actual point size that should be set. This
can vary, for example, if style->styles[relative_point_var] is set. */

PG_PASCAL (short) pgComputePointSize (paige_rec_ptr pg, style_info_ptr style)
{
	pg_fixed		relative_ratio, the_point;
	short			relative_var;

	if ((the_point = style->point) == 0)
		the_point = 0x000C0000;
	
	if (style->styles[relative_point_var] || style->styles[nested_subset_var]) {

		relative_var = style->styles[relative_point_var];
		
		if (!relative_var && style->styles[nested_subset_var])
			relative_var = 12;

		if (style->styles[nested_subset_var]) {
			short				nested_factor, subref_level;
			
			if (subref_level = (short)compute_subref_level(pg)) {
			
				if (subref_level >= 3)
					nested_factor = NESTED_POINT_LEVEL3;
				else
				if (subref_level > 1)
					nested_factor = NESTED_POINT_LEVEL2;
				else
					nested_factor = NESTED_POINT_LEVEL1;
				
				if (nested_factor < relative_var)
					relative_var = nested_factor;
			}
		}

		relative_ratio = pgFixedRatio(relative_var, 12);
		the_point = pgMultiplyFixed(relative_ratio, the_point);
	}

	the_point >>= 16;

	return	(short)the_point;
}


/* pgConvertTextCaps converts text of length bytes to either ALL CAPS or
small caps depending on the given style. The conversion copies the text into
the pg->buf_special memory ref and returns that memory_ref.
Note that the style & point sizes, etc. are already set
so this function doesn't care. Also, the text is known to be in only ONE
style, i.e. the styles won't change within text[length]. If the text is not
converted MEM_NULL is returned. */

PG_PASCAL (memory_ref) pgConvertTextCaps (paige_rec_ptr pg, style_info_ptr cur_style,
		pg_char_ptr text, long length)
{
	register pg_char_ptr	    data;
	register style_info_ptr	 the_style;
	memory_ref				      result;

	the_style = cur_style;
	result = MEM_NULL;
	
	if (!(the_style->class_bits & CANT_TRANS_BIT))
		if (the_style->styles[all_caps_var] || the_style->styles[all_lower_var]
				|| the_style->styles[small_caps_var]) {
		
		result = pg->buf_special;
		SetMemorySize(result, length);
		data = (pg_char_ptr) UseMemory(result);
		pgTransLiterate(text, length, data, (pg_boolean)(the_style->styles[all_lower_var] == 0));
		UnuseMemory(result);
	}

	return  result;
}


/* pgCStrLength returns the lenth of a cstring. */

//е TRS/OITC

#ifndef C_LIBRARY
PG_PASCAL (long) pgCStrLength (const pg_c_string_ptr str)
{
	register pg_c_string_ptr    str_ptr;
	register long		  		length;
		
	for (str_ptr = str, length = 0; *str_ptr++; ++length) ;
	
	return  length;
}

#endif


/* Converts a C string to a Pascal string */

//е TRS/OITC

PG_PASCAL (pg_p_string_ptr) pgCStr2PStr(pg_p_string_ptr p_str, const pg_c_string_ptr c_str)
{
	short index;

	index = (short)pgCStrLength(c_str);
	pgBlockMove (&c_str[index], &p_str[index + 1], -(long)(index * sizeof(pg_char)));
	p_str[0] = (unsigned char)index;
	return p_str;
}


/* Converts a Pascal string to a C string */

//е TRS/OITC

PG_PASCAL (pg_c_string_ptr) pgPStr2CStr(pg_c_string_ptr c_str, const pg_p_string_ptr p_str)
{
	short index;
	
	index = p_str[0];
	pgBlockMove (&p_str[1], c_str, index * sizeof(pg_char));
	c_str[index] = 0;
	return c_str;
}



#ifdef NO_C_INLINE

/* Converts a Paige style time to a platform style time (Paige style ie. seconds since 00:00 01 JAN 1900) */

PG_PASCAL (pg_time_t) pgGetPlatformTime(pg_time_t pg_secs)
{
	return pg_secs - PLATFORM_DELTA_TIME;
}


/* Converts a platform style time to a Paige style time (Paige style ie. seconds since 00:00 01 JAN 1900) */

PG_PASCAL (pg_time_t) pgGetPaigeTime(pg_time_t platform_secs)
{
	return platform_secs + PLATFORM_DELTA_TIME;
}

#endif



/************************ Local Functions ***********************/


static long compute_subref_level (paige_rec_ptr pg)
{
	pg_subref		last_subref, this_subref;
	paige_sub_ptr	sub_ptr;
	long			level = 0;
	
	last_subref = pg->active_subset;
	
	while (last_subref) {
		
		this_subref = last_subref;
		sub_ptr = (paige_sub_ptr) UseMemory(last_subref);

		if (!(sub_ptr->subref_flags & SUBREF_NOT_EDITABLE))
			++level;

		last_subref = sub_ptr->home_sub;
		UnuseMemory(this_subref);
	}
	
	return		level;
}


/* This is called by word underlining.  The next word, beginning from offset,
is figured out. Only whole words (never spaces) are considered. If no word is
found, begin and end will be equal.  The function returns the next offset that
should be passed to obtain the next word.  */

static long find_next_word (paige_rec_ptr pg, style_walk_ptr walker,
		pg_char_ptr text, long global_offset, long local_offset, long end_offset,
		pg_short_t PG_FAR *begin, pg_short_t PG_FAR *end, long PG_FAR *terminator_flags)
{
	register long			   offset_ctr;
	register long			   c_info;
	long					   offset_begin;

	offset_ctr = local_offset;
	offset_begin = offset_ctr;

	while (offset_ctr < end_offset) {

		c_info = walker->cur_style->procs.char_info(pg, walker, text,
				global_offset, offset_begin, end_offset, offset_ctr, INCLUDE_SEL_BIT | LAST_HALF_BIT | MIDDLE_CHAR_BIT);

		if ((c_info & INCLUDE_SEL_BIT) && (!(c_info & (LAST_HALF_BIT | MIDDLE_CHAR_BIT))))
			break;
		
		++walker->current_offset;
		++offset_ctr;
	}
	
	*begin = *end = (pg_short_t)offset_ctr;
	
	while (offset_ctr < end_offset) {

		c_info = walker->cur_style->procs.char_info(pg, walker, text,
				global_offset, offset_begin, end_offset, offset_ctr, TAB_BIT | BLANK_BIT | CTL_BIT | LAST_HALF_BIT | MIDDLE_CHAR_BIT | INCLUDE_SEL_BIT | WORD_SEL_BIT);
		
		*terminator_flags = c_info;
		
		if (c_info & (BLANK_BIT | CTL_BIT | TAB_BIT)) {
			
			++offset_ctr;
			break;
		}
		
		if (!(c_info & (LAST_HALF_BIT | MIDDLE_CHAR_BIT)))
			if (c_info & WORD_SEL_BIT)
				break;

		++offset_ctr;
		*end += 1;
	}
	
	return  offset_ctr;
}

/* intersect_page_modify calls the page_modify hook when computing the page region. */

static void intersect_page_modify (paige_rec_ptr pg, rectangle_ptr page_bounds,
			pg_scale_ptr scaler, co_ordinate_ptr offset, long page_num, pg_region rgn)
{
	rectangle			intersection, page_margins;

	pgFillBlock(&page_margins, sizeof(rectangle), 0);

	pg->procs.page_modify(pg, page_num, &page_margins);

	if (page_margins.top_left.h || page_margins.top_left.v
		|| page_margins.bot_right.h || page_margins.bot_right.v) {
		pg_region			temp_rgn;
		Rect				region_r;

		intersection = *page_bounds;
		intersection.top_left.v += page_margins.top_left.v;
		intersection.top_left.h += page_margins.top_left.h;
		intersection.bot_right.v -= page_margins.bot_right.v;
		intersection.bot_right.h -= page_margins.bot_right.h;

		temp_rgn = pgCreateRgn();

		pgScaleRect(scaler, offset, &intersection);
		RectangleToRect(&intersection, NULL, &region_r);
		pgRectToRgn(temp_rgn, &region_r);
		pgSectRgn(temp_rgn, rgn, rgn);
		
		pgDisposeRgn(temp_rgn);
	}
}

