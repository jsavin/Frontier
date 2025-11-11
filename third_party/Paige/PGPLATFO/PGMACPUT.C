/* This file contains special extensions to pgMacUtils.  It is separate because
not everyone needs to use these and it is a fair amount of code. The original
functions are prototype'd in pgTraps.h.

Copyright 1993 by DataPak Software, Inc.  All rights reserved. It is illegal
to remove this notice 	*/

/* Copyright 1994 by OITC, Inc.  All rights reserved. 
	Fixed memory errors and added region support  	*/

/* Modified Dec 22 1994 for the use of volatile in PG_TRY-PG_CATCH by TR Shaw, OITC */

#include "CPUDefs.h"
#include "pgRegion.h"

#ifdef MAC_PLATFORM

#pragma segment pgmacutils

#ifndef THINK_C

#include <Quickdraw.h>
#include <Memory.h>
#include <fonts.h>
#include <Packages.h>
#include <OSUtils.h>
#include <ToolUtils.h>
#include <files.h>
#include <errors.h>
#include <script.h>
#include <gestalt.h>

#endif


#include "Paige.h"
#include "pgTraps.h"
#include "pgShapes.h"
#include "pgUtils.h"
#include "pgExceps.h"

#define IMAGE_HEIGHT		32		// Amount of column pixels to use for imaging


static short left_bit_position (pg_char the_byte);
static short right_bit_position (pg_char the_byte);
static short first_black_pixel (Ptr buffer_ptr, short row_size, short precision_mask,
			short right_side);


/* PictOutlineToShape sets shape_ref to match the outside edges of pict. The
accuracy parameter defines the nearest pixel to produce the image, 0 being the
most accurate.  The purpose of the accuracy paramater is to improve performance
and/or to save memory space when images to the nearest pixel are not important.
If accuracy is 1, for example, pixel positions 0 and 1, 2 and 3, etc. will be
considered the same position, hence for complex shapes about 1/2 the memory will
be required to produce the resulting shape.  If accuracy is 2, pixels 0, 1, and 2
will be considered the same, and so on. NOTE: accuracies larger than 7 will
produce an empty shape!  */

PG_PASCAL (void) PictOutlineToShape (PicHandle pict, shape_ref target_shape,
		short accuracy)
{
	register Ptr	bits_ptr;
	register short	line_ctr, left_side;
	GrafPtr			old_port, bits_port;
	BitMap			buffer;
	rectangle		out_rect;
	Rect			pic_frame, real_frame;
	short			and_mask, set_and, ctr;
	
	pgSetShapeRect(target_shape, NULL);		// Sets empty output shape

	if (accuracy > 7)
		return;

// Resolve accuracy:

	and_mask = -1;

	for (ctr = 0, set_and = 0xFFFE; ctr < accuracy; ++ctr, set_and <<= 1)
		and_mask &= set_and;
	
// Create an offscreen bitmap to map the image.

	pic_frame = (**pict).picFrame;
	real_frame = pic_frame;
	OffsetRect(&pic_frame, -pic_frame.left, -pic_frame.top);
	
	buffer.bounds = pic_frame;
	buffer.bounds.bottom = IMAGE_HEIGHT;
	buffer.rowBytes = (pic_frame.right / 8) + 1;
	if (buffer.rowBytes & 1)
		++buffer.rowBytes;
	
	buffer.bounds.right = buffer.rowBytes * 8;
	
	buffer.baseAddr = NewPtr(buffer.rowBytes * IMAGE_HEIGHT);
	
	GetPort(&old_port);
	bits_port = (GrafPtr) NewPtr(sizeof(GrafPort));
	OpenPort(bits_port);
	SetPort(bits_port);
	
	SetPortBits(&buffer);
	PortSize(buffer.bounds.right, buffer.bounds.bottom);
	ClipRect(&buffer.bounds);
	pgRectToRgn(bits_port->visRgn, &buffer.bounds);

// Now start drawing the image, IMAGE_HEIGHT sections at a time, and produce
// small rectangles to the output result.

	while (pic_frame.bottom > buffer.bounds.top) {
		
		EraseRect(&buffer.bounds);
		DrawPicture(pict, &pic_frame);
		
		bits_ptr = buffer.baseAddr;
		line_ctr = buffer.bounds.bottom;
		if (pic_frame.bottom < line_ctr)
			line_ctr = pic_frame.bottom;

		while (line_ctr) {
			
			left_side = first_black_pixel(bits_ptr, buffer.rowBytes, and_mask, FALSE);
			if (left_side < buffer.bounds.right) {
			
				out_rect.top_left.h = left_side;
				out_rect.bot_right.h = buffer.bounds.right
						- first_black_pixel(bits_ptr, buffer.rowBytes, and_mask, TRUE);
				// out_rect.bot_right.h += 1;

				if (out_rect.top_left.h < out_rect.bot_right.h) {
					
					out_rect.top_left.v = real_frame.top;
					out_rect.bot_right.v = out_rect.top_left.v + 1;
					out_rect.top_left.h += real_frame.left;
					out_rect.bot_right.h += real_frame.left;
					
					pgAddRectToShape(target_shape, &out_rect);
				}
			}

			bits_ptr += buffer.rowBytes;
			++real_frame.top;

			--line_ctr;
		}
		
		OffsetRect(&pic_frame, 0, -IMAGE_HEIGHT);
	}
	
	ClosePort(bits_port);
	SetPort(old_port);
	DisposePtr((Ptr) bits_port);
	DisposePtr(buffer.baseAddr);
}


/* RegionOutlineToShape sets shape_ref to match the outside edges of rgn. The
accuracy parameter defines the nearest pixel to produce the image, 0 being the
most accurate.  The purpose of the accuracy paramater is to improve performance
and/or to save memory space when images to the nearest pixel are not important.
If accuracy is 1, for example, pixel positions 0 and 1, 2 and 3, etc. will be
considered the same position, hence for complex shapes about 1/2 the memory will
be required to produce the resulting shape.  If accuracy is 2, pixels 0, 1, and 2
will be considered the same, and so on. NOTE: accuracies larger than 7 will
produce an empty shape!  */

//¥ TRS/OITC

PG_PASCAL (void) RegionOutlineToShape (RgnHandle rgn, shape_ref target_shape,
		short accuracy)
{
	register Ptr		bits_ptr;
	register short		line_ctr, left_side;
	GrafPtr				old_port, bits_port;
	BitMap				buffer;
	rectangle			out_rect;
	Rect				rgn_frame, real_frame;
	short				and_mask, set_and, ctr;
#ifndef PG_VOLATILE
	volatile memory_ref	offscrn_buffer = MEM_NULL;
	volatile memory_ref	offscrn_port = MEM_NULL;
#else
	memory_ref 			offscrn_buffer;
	memory_ref 			offscrn_port;
	
	PGVolatile(offscrn_buffer);
	PGVolatile(offscrn_port);

	offscrn_buffer = MEM_NULL;
	offscrn_port = MEM_NULL;
#endif
	
	pgSetShapeRect(target_shape, NULL);		// Sets empty output shape

	if (accuracy > 7)
		return;

// Resolve accuracy:

	and_mask = -1;

	for (ctr = 0, set_and = 0xFFFE; ctr < accuracy; ++ctr, set_and <<= 1)
		and_mask &= set_and;
	
// Create an offscreen bitmap to map the image.

	rgn_frame = (**rgn).rgnBBox;
	real_frame = rgn_frame;
	OffsetRect(&rgn_frame, -rgn_frame.left, -rgn_frame.top);
	OffsetRgn(rgn, -rgn_frame.left, -rgn_frame.top);
	
	buffer.bounds = rgn_frame;
	buffer.bounds.bottom = IMAGE_HEIGHT;
	buffer.rowBytes = (rgn_frame.right / 8) + 1;
	if (buffer.rowBytes & 1)
		++buffer.rowBytes;
	
	buffer.bounds.right = buffer.rowBytes * 8;
	
	PG_TRY(GetGlobalsFromRef(target_shape))
	{
		offscrn_buffer = MemoryAlloc (GetGlobalsFromRef(target_shape), sizeof(pg_bits8), buffer.rowBytes * IMAGE_HEIGHT, 0);		
		offscrn_port = MemoryAlloc (GetGlobalsFromRef(target_shape), sizeof(GrafPort), 1, 0);
	}
	PG_CATCH
	{
		DisposeNonNilMemory(offscrn_buffer);
		DisposeNonNilMemory(offscrn_port);
		PG_RERAISE();
	}
	PG_ENDTRY
	
	buffer.baseAddr = UseForLongTime(offscrn_buffer);
	bits_port = (GrafPtr) UseForLongTime(offscrn_port);

	GetPort(&old_port);
	
	OpenPort(bits_port);
	SetPort(bits_port);
	
	SetPortBits(&buffer);
	PortSize(buffer.bounds.right, buffer.bounds.bottom);
	ClipRect(&buffer.bounds);
	pgRectToRgn(bits_port->visRgn, &buffer.bounds);

// Now start drawing the image, IMAGE_HEIGHT sections at a time, and produce
// small rectangles to the output result.

	while (rgn_frame.bottom > buffer.bounds.top) {
		
		EraseRect(&buffer.bounds);
		PaintRgn(rgn);
		
		bits_ptr = buffer.baseAddr;
		line_ctr = buffer.bounds.bottom;
		if (rgn_frame.bottom < line_ctr)
			line_ctr = rgn_frame.bottom;

		while (line_ctr) {
			
			left_side = first_black_pixel(bits_ptr, buffer.rowBytes, and_mask, FALSE);
			if (left_side < buffer.bounds.right) {
			
				out_rect.top_left.h = left_side;
				out_rect.bot_right.h = buffer.bounds.right
						- first_black_pixel(bits_ptr, buffer.rowBytes, and_mask, TRUE);
				// out_rect.bot_right.h += 1;

				if (out_rect.top_left.h < out_rect.bot_right.h) {
					
					out_rect.top_left.v = real_frame.top;
					out_rect.bot_right.v = out_rect.top_left.v + 1;
					out_rect.top_left.h += real_frame.left;
					out_rect.bot_right.h += real_frame.left;
					
					pgAddRectToShape(target_shape, &out_rect);
				}
			}

			bits_ptr += buffer.rowBytes;
			++real_frame.top;

			--line_ctr;
		}
		
		OffsetRect(&rgn_frame, 0, -IMAGE_HEIGHT);
	}
	
	OffsetRgn(rgn, rgn_frame.left, rgn_frame.top);
	UnuseMemory(offscrn_buffer);
	UnuseMemory(offscrn_port);

	ClosePort(bits_port);
	SetPort(old_port);
}


/*  Internal Functions */

/* first_black_pixel scans a row from right to left (if right_side is TRUE)
or left-to-right (if right_side is FALSE) looking for the first non-white
pixel. The function result is the pixel position relative to the side
it started from. The precision_mask is the accuracy test (e.g., 0xFFFF, 0xFFFE, etc) */

static short first_black_pixel (Ptr buffer_ptr, short row_size, short precision_mask,
			short right_side)
{
	register Ptr				ptr;
	register short				row_ctr, modulo_extra, mod_test, mod_check;

	ptr = buffer_ptr;
	row_ctr = row_size;
	modulo_extra = 0;
	
	if (right_side) {
		
		ptr += row_ctr;
		
		while (row_ctr) {
			
			if (*(--ptr))
				break;
			
			--row_ctr;
		}
		
		if (*ptr)
			modulo_extra = right_bit_position((pg_char) *ptr);
	}
	else {

		while (row_ctr) {
			
			if (*ptr)
				break;
			++ptr;
			--row_ctr;
		}
		
		if (row_ctr)
			modulo_extra = left_bit_position((pg_char) *ptr);
	}

	if (precision_mask != -1)
		if (modulo_extra) {

		mod_test = ((~precision_mask) / 2) + 1;
		mod_check = modulo_extra & precision_mask;
		if (modulo_extra & mod_test)
			--mod_check;
		
		modulo_extra = mod_check;
	}

	return	((row_size - row_ctr) * 8) + modulo_extra;
}


// This returns the first pixel, from 0 to 7 left-to-right that is not 0.

static short left_bit_position (pg_char the_byte)
{
	register pg_char			test_byte;
	register short				byte_ctr;
	
	test_byte = the_byte;

	for (byte_ctr = 0; byte_ctr < 8; ++byte_ctr) {
		
		if (test_byte & 0x80)
			break;
		test_byte <<= 1;
	}
	
	return	byte_ctr;
}

// This returns the first pixel, from 0 to 7 right-to-left that is not 0.

static short right_bit_position (pg_char the_byte)
{
	register pg_char			test_byte;
	register short				byte_ctr;
	
	test_byte = the_byte;

	for (byte_ctr = 0; byte_ctr < 8; ++byte_ctr) {
		
		if (test_byte & 1)
			break;
		test_byte >>= 1;
	}
	
	return	byte_ctr;
}

#endif
