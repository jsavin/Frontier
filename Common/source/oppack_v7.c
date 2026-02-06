
/*	$Id$    */

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

/*
 * 2025-12-05: V7 outline packer for v4 portable format (64-bit timestamps, NO font/UI fields).
 * This file handles WRITING v7 database outline payloads with the portable header.
 * Legacy v6 databases use oppack_legacy.c for reading old v2/v3 format.
 * See planning/phase3/carbon_migration/outline_script_payload.md for format details.
 */
/* 2025-12-08 Codex: Use memcpy for outline timestamps/ctsaves to tolerate packed legacy alignment. */

#include "frontier.h"
#include "standard.h"

#include <stdint.h>
#include <string.h>
/* 2025-11-14 Codex: Preserve fixed legacy header layout on 64-bit builds. */
/* 2025-11-23 Codex: Write outline header sizes with BE helpers for v7 portability. */

#include "memory.h"
#include "font.h"
#include "quickdraw.h"
#include "strings.h"
#include "ops.h"
#include "op.h"
#include "opinternal.h"
#include "oppack_legacy.h" /* For dispatching to legacy v2/v3 unpackers during migration */
#include "db_format.h" /* 2025-11-23 Codex: explicit BE writes for outline headers */
#include "byteorder.h"	/* 2006-04-08 aradke: endianness conversion macros */
#include "logging.h"
#if defined(FRONTIER_HEADLESS)
extern const char *langhash_materialize_current_path;
#endif

#pragma pack(2)
typedef struct tylinetableitem {
	
	int16_t flags;
	
	int32_t lenrefcon; /*number of bytes that follow -- the refcon information*/
	} tylinetableitem, *ptrlinetable, **hdllinetable;
#pragma options align=reset

_Static_assert (sizeof (tylinetableitem) == 6, "tylinetableitem size drift");

typedef enum tylinetableitemflags {

#ifndef SWAP_BYTE_ORDER
	flexpanded = 0x8000, /*was the line expanded when the outline was closed?*/
	flopenwindow = 0x4000, /*was the linked window open when its owner was closed?*/
	appbit0 = 0x2000, /*application-defined bit*/
	appbit1 = 0x1000, /*application-defined bit*/
	fllocked = 0x0800, /*is the line locked?*/
	flcomment = 0x0400, /*is this line a comment?*/
	flbreakpoint = 0x0200 /*is a breakpoint set on this line?*/
#else
	flexpanded = 0x0080, /*was the line expanded when the outline was closed?*/
	flopenwindow = 0x0040, /*was the linked window open when its owner was closed?*/
	appbit0 = 0x0020, /*application-defined bit*/
	appbit1 = 0x0010, /*application-defined bit*/
	fllocked = 0x0008, /*is the line locked?*/
	flcomment = 0x0004, /*is this line a comment?*/
	flbreakpoint = 0x0002 /*is a breakpoint set on this line?*/
#endif
	} tylinetableitemflags;

#define macplatform 'mac '
#define winplatform 'win '

	#define thisplatform macplatform
	#define diskchcomment			((byte) 0xab)	/* '�' */
	#define diskchendcomment		((byte) 0xbb)
	#define diskchopencurlyquote	((byte) 0x93)
	#define diskchclosecurlyquote	((byte) 0x94)
	#define diskchtrademark			((byte) 0x99)
	#define diskchnotequals			((byte) 0xad)	/* '�' */
	#define diskchdivide			((byte) 0xf7)	/* '�' */

#define opversionnumber 4  /* v4 = portable header format */

#define hibyte(x) (x & 0xff00)

/*
 * V4 Portable Header Format (v7/portable)
 * Drops all QuickDraw/UI fields (fonts, colors, scroll positions, window rects).
 * Keeps only runtime-relevant metadata.
 * See planning/phase3/carbon_migration/outline_script_payload.md
 */
#pragma pack(2)
typedef struct typortablediskheader {

	int16_t versionnumber; /* 4 for portable format */

	int32_t sizelinetable; /* bytes in linetable section */

	int32_t sizetext; /* bytes in text portion */

	int16_t lnumcursor; /* cursor line number (low word) */

	int16_t lnumcursor_hiword; /* cursor line number (high word) */

	unsigned char _pad[4]; /* padding for 8-byte alignment of timecreated */

	int64_t timecreated; /* seconds since Mac epoch (1904-01-01) */

	int64_t timelastsave; /* seconds since Mac epoch (1904-01-01) */

	int32_t ctsaves; /* number of times saved */

	int16_t fltextmode; /* stored as byte (non-zero = true) */

	int32_t outlinesignature; /* caller-defined cookie */

	OSType platform; /* 'mac ' or 'win ' for character mapping */

	/* Reserved expansion area: 1020 bytes instead of 1024 because:
	 * Total header = 1068 bytes (spec requirement)
	 * Fields above = 48 bytes:
	 *   versionnumber(2) + sizelinetable(4) + sizetext(4) + lnumcursor(2) +
	 *   lnumcursor_hiword(2) + _pad[4](4) + timecreated(8) + timelastsave(8) +
	 *   ctsaves(4) + fltextmode(2) + outlinesignature(4) + platform(4) = 48
	 * Reserved = 1068 - 48 = 1020 bytes
	 * The _Static_assert below verifies total struct size is exactly 1068 bytes.
	 */
	byte reserved[1020]; /* zeroed expansion area for future metadata */

	} typortablediskheader;

_Static_assert (sizeof (typortablediskheader) == 1068, "typortablediskheader must be 1068 bytes");


typedef struct tyoppackinfo {

	handlestream *packstream;
	
	boolean flpackcomments;
	} tyoppackinfo, *ptroppackinfo;
#pragma options align=reset


static boolean outtablevisit (hdlheadrecord hnode, ptrvoid refcon) {
	
	/*
	4.1b7 dmb: we should never dispose of the packhandle.
	oppack takes care of that.
	
	5.1.3 dmb: use pushhandle to add refcon data; don't lock refcon while 
	enlarging packhandle
	*/

	register hdlheadrecord hn = hnode;
	register Handle hrefcon = (**hn).hrefcon;
	register long lenrefcon;
	ptroppackinfo packinfo = (ptroppackinfo) refcon;
	tylinetableitem item;
	
	clearbytes (&item, sizeof (item));
	
	item.flags |= flexpanded * (**hn).flexpanded;
	
	item.flags |= flopenwindow * (**hn).tmpbit;
	
	item.flags |= appbit0 * (**hn).appbit0;
	
	item.flags |= appbit1 * (**hn).appbit1;
	
	item.flags |= fllocked * (**hn).fllocked;
	
	item.flags |= flcomment * (**hn).flcomment;
	
	item.flags |= flbreakpoint * (**hn).flbreakpoint;
	
	lenrefcon = gethandlesize (hrefcon); /*0 if it's nil*/

	item.lenrefcon = conditionallongswap (lenrefcon);

	(**hnode).tmpbit = false;
	
	if (!writehandlestream ((*packinfo).packstream, &item, sizeof (item))) {
	
		/*4.1b7 dmb: our caller manages this - disposehandle (packhandle);*/
		
		return (false);
		}
	
	if (lenrefcon > 0) { /*something stored in the refcon field*/
		
		if (!writehandlestreamhandle ((*packinfo).packstream, hrefcon)) {
			
			/*4.1b7 dmb: our caller manages this - disposehandle (packhandle);*/
			
			return (false);
			}
		}
		
	return (true);
	} /*outtablevisit*/


static boolean opoutlinetotable (hdlheadrecord hnode, handlestream *packstream, long *ctbytes) {
	
	/*
	traverse the current outline, producing a table of information 
	with one element for each line in the outline.  append that table
	to the end of the indicated handle and return the number of bytes
	added to the handle.
	
	return false if there was a memory error.
	*/
	
	register long origsize;
	tyoppackinfo packinfo;
	
	*ctbytes = 0;
	
	origsize = (*packstream).eof;
	
	packinfo.packstream = packstream;
	
	if (!opsiblingvisiter (hnode, false, &outtablevisit, &packinfo))
		return (false);
		
	*ctbytes = (*packstream).eof - origsize;
	
	return (true);
	} /*opoutlinetotable*/


static boolean pushdiskchar (byte ch, handlestream *deststream) {

	/*
	insert the character at the end of a pascal string.
	map cross-platform disk characters to machine-specific chars

	5.0b11 dmb: we're now only called when mapping from the other
	platform the diskch constants are the other flatform's
	*/

	switch (ch) {
		case diskchcomment:
			ch = chcomment;
			break;
		
		case diskchendcomment:
			ch = chendcomment;
			break;
		
		case diskchopencurlyquote:
			ch = chopencurlyquote;
			break;

		case diskchclosecurlyquote:
			ch = chclosecurlyquote;
			break;
		
		case diskchtrademark:
			ch = chtrademark;
			break;

		case diskchdivide:
			ch = chdivide;
			break;
		

		default:
			break;
		}
	
	return (writehandlestreamchar (deststream, ch));
	} /*pushdiskchar*/




static boolean outtextvisit (hdlheadrecord hnode, ptroppackinfo packinfo) {
	
	/*
	2.1b3 dmb: account for the fact that the leading tabs and trailing 
	return may overflow our 255-character limit
	
	2.1b5 dmb: added option to skip comment text
	
	4.1b7 dmb: we should never dispose of the packhandle.
	oppack takes care of that.

	5.0b11 dmb: don't map character when packing. can lose info, screw 
	clipboard
	*/
	
	register short level = (**hnode).headlevel;
	bigstring bs;
	
	filledstring (chtab, level, bs);
	
	if (!writehandlestreamstring ((*packinfo).packstream, bs)) //push the tabs by themself
		goto error;
	
	if ((*packinfo).flpackcomments || !opnestedincomment (hnode)) {
		
		if (!writehandlestreamhandle ((*packinfo).packstream, (**hnode).headstring)) //push the head text
			goto error;
		}
	
	if (!writehandlestreamchar ((*packinfo).packstream, chreturn)) //push the cr terminator
		goto error;

	return (true);
	
	error: {
		
		/*4.1b7 dmb: our caller manages this - disposehandle (packhandle);*/
		
		return (false);
		}
	} /*outtextvisit*/
	

static boolean opoutlinetotext (hdlheadrecord hnode, handlestream *textstream, long *ctbytes) {
	
	/*
	convert the outline into a block of tab-indented text, each
	line ended by a carriage return.  suitable for saving to disk
	because we convert the whole outline, all level-0 heads in
	the current outline.
	
	push the resulting text at the end of the indicated handle and
	return in ctbytes the number of bytes added to the handle.
	
	return false if there was a memory allocation error.
	*/
	
	register long origbytes;
	tyoppackinfo packinfo;
	
	origbytes = (*textstream).eof;
	
	*ctbytes = 0;
	
	packinfo.packstream = textstream;
	
	packinfo.flpackcomments = true;
	
	if (!opsiblingvisiter (hnode, false, (opvisitcallback) &outtextvisit, &packinfo))
		return (false);
	
	*ctbytes = (*textstream).eof - origbytes;
	
	return (true);
	} /*opoutlinetotext*/


boolean oppack (Handle *hpackedoutline) {
	
	/*
	create a packed, contiguous version of the current outline record.
	
	DW 3/25/90: if hpackedoutline comes in non-nil, we just append our
	stuff to the end of the handle, we don't allocate a new one.
	
	dmb 10/16/90: don't dispose of handle if we didn't allocate it
	
	dmb 2/8/91: flush edit buffer if in text mode
	
	dmb 3/1/91: deal with hoists
	
	4/9/93 dmb: resize the packedoutline handle really large and back again 
	before starting to avoid potentially many, many heap compactions while 
	expanding it for real
	
	4.1b7 dmb: fixed double dispose bug. if our caller allocated the handle
	we do not dispose it on error, as was originally intended.

	5.0b11 dmb: added platform logic. don't map characters when packing
	*/
	
	register hdloutlinerecord ho = op_get_outlinedata();
	register hdlheadrecord hsummit;
	register Handle h;
	register long ixheader;
	handlestream packstream;
	typortablediskheader header;
	boolean flallocated = false;
	boolean flpoppedhoists = false;
	boolean flerror = false;
	int64_t lnumcursor;
	long textbytes = 0;
	long linetablebytes = 0;
	
	h = *hpackedoutline; /*copy into register*/

	clearbytes (&header, sizeof (header)); /*assure all bits set to 0*/

#if defined(FRONTIER_HEADLESS)
	const long hosize = gethandlesize((Handle) ho);
	hdlheadrecord hcursor_early = (ho == NULL || *ho == NULL) ? NULL : (**ho).hbarcursor;
	const long hcursor_size_early = (hcursor_early == NULL) ? 0 : gethandlesize((Handle) hcursor_early);
	if (ho == nil || *ho == NULL || hosize <= 0 || !validhandle((Handle) ho)) {
		const char *ctx = (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>";
		log_error(LOG_COMP_OP, "oppack abort path=%s outlinedata=%p hdata=%p hsize=%ld valid=%d",
		        ctx,
		        (void *) ho,
		        ho == nil ? NULL : *ho,
		        hosize,
		        ho == nil ? 0 : validhandle((Handle) ho));
		return (false);
	}
	if (hcursor_early == NULL || hcursor_size_early != (long) sizeof (tyheadrecord)) {
		log_error(LOG_COMP_OP, "oppack abort early cursor mismatch path=%s ho=%p hodata=%p hcursor=%p hcursor_data=%p hcursor_size=%ld expected=%ld",
		        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
		        (void *) ho,
		        *ho,
		        (void *) hcursor_early,
		        (hcursor_early == NULL) ? NULL : *hcursor_early,
		        hcursor_size_early,
		        (long) sizeof (tyheadrecord));
		return (false);
	}
	log_trace(LOG_COMP_OP, "oppack enter path=%s ho=%p hdata=%p hsize=%ld",
	        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
	        (void *) ho,
	        ho == nil ? NULL : *ho,
	        hosize);
#endif

#if defined(FRONTIER_HEADLESS)
	if (ho == nil || *ho == nil || !validhandle((Handle) ho)) {
		log_error(LOG_COMP_OP, "oppack abort path=%s outlinedata=%p hdata=%p valid=%d",
		        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
		        (void *) ho,
		        ho == nil ? NULL : *ho,
		        ho == nil ? 0 : validhandle((Handle) ho));
		__builtin_trap();
	}
#endif
	
	if (h == nil) { /*the normal case, allocate a new handle*/
		
		if (!newgrowinghandle (5 * 1024, hpackedoutline)) //start with non-empty handle
			return (false);
		
		h = *hpackedoutline;
		
		flallocated = true;
		
		openhandlestream (h, &packstream);
		
		packstream.pos = packstream.eof = sizeof (header);
		}
	else {
		
		openhandlestream (h, &packstream);
		
		packstream.pos = packstream.eof;
		
			if (!writehandlestream (&packstream, &header, sizeof (header)))
				return (false);
			}
			
			ixheader = packstream.pos - sizeof (header); //we're pointing past header now

#if defined(FRONTIER_HEADLESS)
			log_debug(LOG_COMP_OP, "oppack debug stream-init path=%s hpacked=%p pos=%ld eof=%ld",
			        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
			        (void *) h,
			        (long) packstream.pos,
			        (long) packstream.eof);
#endif
			
#if defined(FRONTIER_HEADLESS)
			if (op_get_outlinedata() != ho || op_get_outlinedata() == NULL || *op_get_outlinedata() == NULL || !validhandle((Handle) op_get_outlinedata())) {
				log_error(LOG_COMP_OP, "oppack abort before hoist pop path=%s outlinedata=%p hdata=%p ho=%p hodata=%p valid=%d",
			        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
			        (void *) op_get_outlinedata(),
			        op_get_outlinedata() == NULL ? NULL : *op_get_outlinedata(),
			        (void *) ho,
			        ho == NULL ? NULL : *ho,
			        op_get_outlinedata() == NULL ? 0 : validhandle((Handle) op_get_outlinedata()));
			__builtin_trap();
		}
		log_debug(LOG_COMP_OP, "oppack debug pre-hoists path=%s outlinedata=%p hdata=%p ho=%p hodata=%p",
		        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
		        (void *) op_get_outlinedata(),
		        op_get_outlinedata() == NULL ? NULL : *op_get_outlinedata(),
		        (void *) ho,
		        ho == NULL ? NULL : *ho);
#endif

		flpoppedhoists = oppopallhoists ();

#if defined(FRONTIER_HEADLESS)
		log_debug(LOG_COMP_OP, "oppack debug post-hoists path=%s outlinedata=%p hdata=%p ho=%p hodata=%p flpopped=%d",
		        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
		        (void *) op_get_outlinedata(),
		        op_get_outlinedata() == NULL ? NULL : *op_get_outlinedata(),
		        (void *) ho,
		        ho == NULL ? NULL : *ho,
		        flpoppedhoists);
		if (ho == nil || *ho == nil || !validhandle((Handle) ho)) {
			log_error(LOG_COMP_OP, "oppack abort after hoist pop path=%s outlinedata=%p hdata=%p valid=%d",
			        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
			        (void *) ho,
			        ho == nil ? NULL : *ho,
		        ho == nil ? 0 : validhandle((Handle) ho));
		__builtin_trap();
	}
#endif

	/* V4 Portable Header - only runtime-relevant fields */
	header.versionnumber = conditionalshortswap(opversionnumber);

	header.platform = conditionallongswap (thisplatform);

#if defined(FRONTIER_HEADLESS)
	if (*ho == NULL) {
		log_error(LOG_COMP_OP, "oppack abort before header fill path=%s outlinedata=%p hdata=%p",
		        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
		        (void *) ho,
		        NULL);
		__builtin_trap();
	}
	/* 2025-12-07 Codex: sanity-check handle size matches widened tyoutlinerecord */
	const long hosize_checked = gethandlesize((Handle) ho);
	if (hosize_checked != (long) sizeof (tyoutlinerecord)) {
		log_error(LOG_COMP_OP, "oppack abort handle size mismatch path=%s ho=%p hdata=%p hsize=%ld expected=%ld",
		        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
		        (void *) ho,
		        *ho,
		        hosize_checked,
		        (long) sizeof (tyoutlinerecord));
		__builtin_trap();
	}
	hdlheadrecord hcursor = (**ho).hbarcursor;
	log_debug(LOG_COMP_OP, "oppack pre-opgetnodeline ho=%p hodata=%p hcursor=%p hcursor_data=%p hcursor_size=%ld",
	        (void *) ho,
	        *ho,
	        (void *) hcursor,
	        (hcursor == NULL) ? NULL : *hcursor,
	        (hcursor == NULL) ? -1L : gethandlesize((Handle) hcursor));
	fflush(stderr);
	const long hcursor_size = (hcursor == NULL) ? 0 : gethandlesize((Handle) hcursor);
	if (hcursor == NULL || !validhandle((Handle) hcursor) || *hcursor == NULL) {
		log_error(LOG_COMP_OP, "oppack abort cursor invalid path=%s hcursor=%p valid=%d data=%p ho=%p hodata=%p",
		        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
		        (void *) hcursor,
		        (hcursor == NULL) ? 0 : validhandle((Handle) hcursor),
		        (hcursor == NULL) ? NULL : *hcursor,
		        (void *) ho,
		        *ho);
		__builtin_trap();
	}
	if (hcursor_size != (long) sizeof (tyheadrecord)) {
		log_error(LOG_COMP_OP, "oppack abort cursor size mismatch path=%s hcursor=%p hsize=%ld expected=%ld",
		        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
		        (void *) hcursor,
		        hcursor_size,
		        (long) sizeof (tyheadrecord));
		__builtin_trap();
	}
#endif

	opgetnodeline ((**ho).hbarcursor, &lnumcursor);

#if defined(FRONTIER_HEADLESS)
	log_debug(LOG_COMP_OP, "oppack cursor line=%ld ho=%p hdata=%p hsize=%ld",
	        lnumcursor,
	        (void *) ho,
	        *ho,
	        gethandlesize((Handle) ho));
	fflush(stderr);
	if (*ho == NULL || !validhandle((Handle) ho)) {
		log_error(LOG_COMP_OP, "oppack abort after opgetnodeline path=%s outlinedata=%p hdata=%p valid=%d",
		        (langhash_materialize_current_path != NULL) ? langhash_materialize_current_path : "<nil>",
		        (void *) ho,
		        (ho == NULL) ? NULL : *ho,
		        (ho == NULL) ? 0 : validhandle((Handle) ho));
		__builtin_trap();
	}
#endif

	memlongtodiskwords (lnumcursor, header.lnumcursor, header.lnumcursor_hiword);

	header.fltextmode = (**ho).fltextmode;

	/* 64-bit timestamp fields may be only 2-byte aligned under the packed legacy layout. */
	{
		int64_t timecreated_local = 0;
		int64_t timelastsave_local = 0;
		int32_t ctsaves_local = 0;

		memcpy(&timecreated_local, &(**ho).timecreated, sizeof(timecreated_local));
		memcpy(&timelastsave_local, &(**ho).timelastsave, sizeof(timelastsave_local));
		memcpy(&ctsaves_local, &(**ho).ctsaves, sizeof(ctsaves_local));

		db_format_write_be64(&header.timecreated, (uint64_t) timecreated_local);
		db_format_write_be64(&header.timelastsave, (uint64_t) timelastsave_local);

		++ctsaves_local;
		memcpy(&(**ho).ctsaves, &ctsaves_local, sizeof(ctsaves_local));
		db_format_write_be32(&header.ctsaves, (uint32_t) ctsaves_local);
	}

	header.outlinesignature = conditionallongswap((**ho).outlinesignature);

	/* reserved[] is already zeroed by clearbytes() above */
	
	hsummit = (**ho).hsummit; /*copy into register*/
	
	opwriteeditbuffer (); /*if a headline is being edited, update text handle*/
	
	if (!opoutlinetotext (hsummit, &packstream, &textbytes)) {
		
		flerror = true;
		
		goto exit;
		}
	
	header.sizetext = (int32_t) textbytes;
	
	if (!opoutlinetotable (hsummit, &packstream, &linetablebytes)) {
	
		flerror = true;
		
		goto exit;
		}
	
	header.sizelinetable = (int32_t) linetablebytes;
	
	db_format_write_be32(&header.sizetext, (uint32_t) header.sizetext);
	db_format_write_be32(&header.sizelinetable, (uint32_t) header.sizelinetable);
	
	/*move the header into handle*/ {
		
		register ptrbyte p;
		
		p = BIGSTRING (*h);
		
		p += ixheader;
		
		moveleft (&header, p, sizeof (header));
		}
	
	exit:
	
	if (flpoppedhoists)
		oprestorehoists ();
	
	closehandlestream (&packstream);
	
	if (flerror) {
		
		if (flallocated)
			disposehandle (h);
		
		return (false);
		}
	
	return (true);
	} /*oppack*/


boolean oppackoutline (hdloutlinerecord houtline, Handle *hpackedoutline) {
	
	boolean fl;
	
	oppushoutline (houtline);
	
	fl = oppack (hpackedoutline);
	
	oppopoutline ();
	
	return (fl);
	} /*oppackoutline*/


static boolean intablevisit (hdlheadrecord hnode, ptrvoid refcon) {
	
	/*
	5.1.5b9 dmb: validate expanded bit in case structure changes with overflow
	*/
	
	register hdlheadrecord hn = hnode;
	ptroppackinfo packinfo = (ptroppackinfo) refcon;
	register long lenrefcon;
	Handle hrefcon;
	tylinetableitem item;
	hdlheadrecord hup, hleft;
	
	if (athandlestreameof ((*packinfo).packstream)) // ran out of line items
		return (true);
	
	readhandlestream ((*packinfo).packstream, &item, sizeof (item));
	
	hup = (**hn).headlinkup;
	
	if (hup == hn) { //first in list, can set expandedness
	
		hleft = (**hn).headlinkleft;
		
		if ((hleft != hn) && !(**hleft).flexpanded)
			(**hn).flexpanded = false;
		else
			(**hn).flexpanded = (item.flags & flexpanded) != 0;
		}
	else
		(**hn).flexpanded = (**hup).flexpanded;
	
	(**hn).fllocked = (item.flags & fllocked) != 0;
	
	(**hn).flcomment = (item.flags & flcomment) != 0;
	
	(**hn).flbreakpoint = (item.flags & flbreakpoint) != 0;
	
	(**hn).tmpbit = (item.flags & flopenwindow) != 0;
	
	(**hn).appbit0 = (item.flags & appbit0) != 0;
	
	(**hn).appbit1 = (item.flags & appbit1) != 0;
	
	lenrefcon = conditionallongswap(item.lenrefcon); /*copy into register*/
	
	if (lenrefcon > 0) { /*link in a refcon node*/
	
		if (!newclearhandle (lenrefcon, &hrefcon))
			return (false);
		
		readhandlestream ((*packinfo).packstream, *hrefcon, lenrefcon);

		(**hn).hrefcon = hrefcon;
		}
	
	return (true);
	} /*intablevisit*/


static boolean optabletooutline (handlestream *packstream, hdlheadrecord hsummit) { 

	/*
	apply values in table to hsummit outline
	*/
	
	tyoppackinfo packinfo;
	
	packinfo.packstream = packstream;
	
	return (opsiblingvisiter (hsummit, false, &intablevisit, &packinfo));
	} /*optabletooutline*/
	

static short spacesforlevel;

static short firstindent;

static boolean fltabindent;

static void clearindentvalues (boolean flmusttabindent) {
	
	spacesforlevel = 0;
	
	firstindent = -1;
	
	fltabindent = flmusttabindent;
	} /*clearindentvalues*/


static boolean opgetlinetext (boolean flmapchars, handlestream *textstream, short *level, Handle *htext) {

	/*
	extract a line of text from the handle at offset ix.  ix gets bumped to
	point at the beginning of the next line.
	
	the level is the number of ascii 9's at the head of the string.
	
	12/13/91 dmb: added support for space-indented text import
	
	3/11/92 dmb: fixed off-by-one error when filling lenbigstring w/out hitting a cr.
	
	2.1b8 dmb: ignore null characters
	
	5.0a2 dmb: when overflowing a headline, try to break after a word
	
	5.0a4 dmb: added *level+1 limit to constrain to correct structure, avoid hang
			   skip linefeeds after returns

	5.0b11 dmb: map characters when they're from the other platform
	*/
	
	handlestream s;
	long ct = 0;
	register ptrbyte p;
	boolean fl;
	
	if (athandlestreameof (textstream))
		return (false);
	
	if (!newhandle (lenbigstring, htext))
		return (false);
	
	openhandlestream (*htext, &s);
	
	s.eof = 0; //nothing has actually been written to the 255-byte buffer
	
	ct = skiphandlestreamchars (textstream, chtab);
	
	if (ct > 0)
		fltabindent = true;
	
	else {
	
		ct = skiphandlestreamchars (textstream, chspace);
		
		if (ct > 0) {
			
			if (spacesforlevel == 0) /*first run of spaces*/
				spacesforlevel = (short) ct;
			
			ct = divround (ct, spacesforlevel);
			}
		}
	
/*
	while (*p == chtab) { /%count the leading tab chars

		ct++;
		
		p++;
		
		ixtext++;
		
		if (ixtext >= sizetext) /%no text on the line
			return (false);
		
		fltabindent = true;
		}

	if (!fltabindent) {
		
		while (*p == chspace) { /%count the leading tab chars
			
			ct++;
			
			p++;
			
			ixtext++;
			
			if (ixtext >= sizetext) /%no text on the line
				return (false);
			}
		
		if (ct > 0) {
			
			if (spacesforlevel == 0) /%first run of spaces
				spacesforlevel = (short) ct;
			
			ct = divround (ct, spacesforlevel);
			}
		}
*/

	if (firstindent < 0) { /*first line of text*/
	
		firstindent = (short) ct;
		
		*level = 0;
		}
	else {
		*level = min (*level + 1, max (0, ct - firstindent));
		}
	
	while ((*textstream).pos < (*textstream).eof) { /*copy the text chars into the string*/
		
		p = (ptrbyte) (*(*textstream).data + (*textstream).pos);
		
		if (*p == chreturn) {
			
			(*textstream).pos++; /*skip over the return; we'll start a new headline*/
			
			if (*++p == chlinefeed)
				(*textstream).pos++;
			
			break;
			}
		
		if (*p != chnul) { /*ignore null characters*/
			
			if (flmapchars)
				fl = pushdiskchar (*p, &s);
			else
				fl = writehandlestreamchar (&s, *p);
			
			if (!fl) {
				
				disposehandle (*htext);
				
				return (false);
				}
			}
		
		(*textstream).pos++;
		} /*while*/
	
	closehandlestream (&s);
	
	return (true);
	} /*opgetlinetext*/


static boolean opunpacktexttooutline (long platform, handlestream *packstream, hdlheadrecord *hnode) {
	
	/*
	2/19/91 dmb: call opstart/endinternalchange around this routine to 
	avoid building bogus undo.
	
	8/22/91 dmb: no longer attempt to attach structure to current outline.  used to 
	cause crash if an error occurred; cleaner to leave it to caller.
	*/
	
	register hdlheadrecord h = nil;
	register short lastlevel = 0;
	register tydirection dir;
	Handle hlinetext;
	hdlheadrecord hnewnode;
	short level;
	boolean fl = false; /*guilty until proven innocent*/
	
	opstartinternalchange ();
	
	clearindentvalues (true);
	
	while (true) {
		
		if (!opgetlinetext (platform != thisplatform, packstream, &level, &hlinetext)) { /*consumed the text*/
			
			if (h != nil) /*success -- we got something*/
				fl = true;
			
			break;
			}
		
		if (h == nil) { /*first line*/
			
			if (!opnewstructure (hlinetext, hnode))
				break;
			
			h = *hnode;
			
			lastlevel = 0;
			}
		else {
			if (level > lastlevel) 
				dir = right;
		
			else {
				h = oprepeatedbump (left, lastlevel - level, h, true);
		
				dir = down;
				}
			
			if (!opdepositnewheadline (h, dir, hlinetext, &hnewnode)) {
				
				opdisposestructure (*hnode, false);
				
				break;
				}
			
			h = hnewnode;
			
			lastlevel = level;
			}
		} /*while*/
	
	opendinternalchange ();
	
	return (fl);
	} /*opunpacktexttooutline*/

	
static boolean opunpackversion4 (handlestream *packstream) {

	/*
	2025-12-05: V4 Portable Header unpacker - reads only runtime-relevant fields.
	Skips all QuickDraw/UI fields (fonts, colors, scroll positions, window rects).
	*/

	register hdloutlinerecord ho;
	handlestream stream;
	hdlheadrecord hsummit, hline1, hcursor;
	typortablediskheader header;
	int64_t lnumcursor;
	boolean fl;

	ho = op_get_outlinedata(); /*copy into register*/

	if (!readhandlestream (packstream, &header, sizeof (header)))
		return (false);

	/* V4 Portable Header - read only runtime-relevant fields */

	(**ho).fltextmode = (boolean) conditionalshortswap (header.fltextmode);

	/* Stored under packed alignment; avoid unaligned 64-bit writes. */
	{
		int64_t timecreated_local = (int64_t) db_format_read_be64((unsigned char *) &header.timecreated);
		int64_t timelastsave_local = (int64_t) db_format_read_be64((unsigned char *) &header.timelastsave);
		int32_t ctsaves_local = (int32_t) db_format_read_be32((unsigned char *) &header.ctsaves);

		memcpy(&(**ho).timecreated, &timecreated_local, sizeof(timecreated_local));
		memcpy(&(**ho).timelastsave, &timelastsave_local, sizeof(timelastsave_local));
		memcpy(&(**ho).ctsaves, &ctsaves_local, sizeof(ctsaves_local));
	}

	disktomemlong (header.platform);

	disktomemlong (header.sizetext);

	disktomemlong (header.sizelinetable);

	if (header.platform == 0)
		header.platform = macplatform;

	if (header.outlinesignature == 0)
		header.outlinesignature = conditionallongswap ('LAND');

	(**ho).outlinesignature = conditionallongswap (header.outlinesignature);

	/* Set default UI values (not stored in v4) */
	(**ho).fontnum = config.defaultfont;
	(**ho).fontsize = config.defaultsize;
	(**ho).fontstyle = 0;
	(**ho).linespacing = 0;  /* portable stub, ignored */
	(**ho).lineindent = 0;

	pushscratchport ();

	pushstyle ((**ho).fontnum, (**ho).fontsize, (**ho).fontstyle);
	
	stream = *packstream;
	
	stream.eof = stream.pos + header.sizetext;
	
	fl = opunpacktexttooutline (header.platform, &stream, &hsummit);
	
	popstyle ();
	
	popport ();
	
	if (!fl) {
#if defined(FRONTIER_TESTS)
		OP_HEADLESS_TRACE ("[headless] opunpackv2 textfail text=%ld linetable=%ld pos=%ld eof=%ld\n",
			(long) header.sizetext,
			(long) header.sizelinetable,
			(long) (*packstream).pos,
			(long) (*packstream).eof);
#endif
		return (false);
	}
	
	(*packstream).pos += header.sizetext;
	
	stream = *packstream;
	
	stream.eof = stream.pos + header.sizelinetable;
	
	opsetsummit (ho, hsummit);
	
	opsetexpandedbits (hsummit, true); /*all 1st level items are expanded*/
	
	if (!optabletooutline (&stream, hsummit)) {
#if defined(FRONTIER_TESTS)
		OP_HEADLESS_TRACE ("[headless] opunpackv2 tablefail bytes=%ld pos=%ld eof=%ld\n",
			(long) header.sizelinetable,
			(long) stream.pos,
			(long) stream.eof);
#endif
		return (false);
	}
	
	(*packstream).pos += header.sizelinetable;
	
	hline1 = oprepeatedbump (flatdown, (**ho).vertscrollinfo.cur, hsummit, true);
	
	(**ho).hline1 = hline1;
	
	lnumcursor = diskwordstomemlong (header.lnumcursor, header.lnumcursor_hiword);

	hcursor = oprepeatedbump (flatdown, lnumcursor, hsummit, true);
	
	(**ho).hbarcursor = hcursor;
	
	opsetctexpanded (ho); /*don't bother saving this on disk, we re-compute*/
	
	return (true);
	} /*opunpackversion2*/
	
	
boolean opunpack (Handle hpackedoutline, long *ixload, hdloutlinerecord *houtline) {

	/*
	9/25/91 dmb: added call to testheapspace to try to improve low-mem handling

	9/15/92 dmb: removed support for version1 format; it never shipped

	12/17/96 dmb: tolerate version numbers new than now, unless high byte changes
	*/

	handlestream packstream;
	hdloutlinerecord ho;
	short versionnumber;
	boolean fl;

	*houtline = nil;

	if (!newoutlinerecord (&ho))
		return (false);

	openhandlestream (hpackedoutline, &packstream);

	packstream.pos = *ixload;

	if (!readhandlestream (&packstream, &versionnumber, sizeof (versionnumber))) {

		shellerrormessage (BIGSTRING ("\x3d" "Can't unpack outline because unexpected data was encountered."));

		opdisposeoutline (ho, false);

		return (false);
		}

	packstream.pos = *ixload;

	disktomemshort (versionnumber);

	oppushoutline (ho);

	/* V7 oppack handles v4 portable format, dispatches v2/v3 to legacy */
	if (versionnumber == 4) {
		fl = opunpackversion4 (&packstream);
	}
	else if (versionnumber == 2 || versionnumber == 3) {
		/* Legacy v2/v3 format - dispatch to oppack_legacy.c */
		oppopoutline (); /* We pushed ho above, but legacy will create its own */
		opdisposeoutline (ho, false); /* Don't need this one */
		closehandlestream (&packstream);
		return opunpack_legacy (hpackedoutline, ixload, houtline);
	}
	else {
		/* Unknown format version */
		shellinternalerror (idbadopversionnumber, STR_bad_outline_version_number);
		fl = false;
	}
	
	oppopoutline ();
	
	if (fl) { /*so far so good -- let's make sure we're not leaving memory dangerously low*/
		
		fl = testheapspace (0x800); /*2K gives us some room to play with*/
		}
	
	*ixload = packstream.pos;
	
	closehandlestream (&packstream);
	
	if (!fl) {
		
		opdisposeoutline (ho, false);
		
		return (false);
		}
	
	*houtline = ho;
	
	return (true);
	} /*opunpack*/


boolean opunpackoutline (Handle hpackedoutline, hdloutlinerecord *houtline) {
	
	long ixload = 0;
	
	return (opunpack (hpackedoutline, &ixload, houtline));
	} /*opunpackoutline*/


boolean optextscraptooutline (hdloutlinerecord houtline, Handle htext, hdlheadrecord *hnode) {
#pragma unused (houtline)

	/*
	there seems to be an opportunity to factor code here.
	
	2.1b8 dmb: added houtline parameter to satisfy texttooutlinecallback 
	conventions. currently ignored here.
	
	5.0a4 dmb: added logic so empty lines don't dictate structure.
	*/
	
	register hdlheadrecord hlast = nil;
	register short lastlevel = 0;
	tydirection dir;
	hdlheadrecord htree = nil;
	hdlheadrecord hnewnode;
	short level;
	Handle hlinetext;
	handlestream textstream;
	boolean fl = false; /*guilty until proven innocent*/
	
	opstartinternalchange ();
	
	openhandlestream (htext, &textstream);
	
	clearindentvalues (false);
	
	while (true) {
		
		if (!opgetlinetext (false, &textstream, &level, &hlinetext)) { /*no more text*/
			
			fl = true; /*success*/
			
			break;
			}
			
		if (hlast == nil) { /*first line*/
			
			if (!opnewstructure (hlinetext, &htree))
				break;
			
			hlast = htree;
			
			lastlevel = 0;
			
			continue; /*finished with this one*/
			}
		
		 /* 2005-01-21 creedon, aradke, JES - empty lines DO dictate structure < http://sourceforge.net/tracker/index.php?func=detail&aid=1093595&group_id=120666&atid=687798 > */
		 /* if (gethandlesize (hlinetext) == 0) // 5.0a4 dmb: empty lines don't dictate structure
			level = lastlevel; */
		
		if (level > lastlevel) {
			
			dir = right;
			}
		else { /*surface 0 or more times*/
		
			hlast = oprepeatedbump (left, lastlevel - level, hlast, true);
	
			dir = down;
			}
		
		if (!opdepositnewheadline (hlast, dir, hlinetext, &hnewnode))
			break;
		
		hlast = hnewnode;
		
		lastlevel = level;
		} /*while*/
	
	closehandlestream (&textstream);
	
	opendinternalchange ();
	
	if (!fl) { /*didn't succeed -- toss accumulated structure and return false*/
		
		if (htree != nil)
			opdisposestructure (htree, false);
		
		return (false);
		}
	
	*hnode = htree;
	
	return (htree != nil);
	} /*optextscraptooutline*/


static short outscraplevel = 0; /*for communications while sending to scrap*/


static boolean outscrapvisit (hdlheadrecord hnode, ptrvoid refcon) {
	
	/*
	6/12/91 dmb: validate op_get_outlinedata().  currently, menubar scraps will only 
	export properly while a menubar outline is active.  the problem is that 
	the scrap is a standalone headline; it needs to have an outline record too.
	*/
	
	register hdlheadrecord h = hnode;
	ptroppackinfo packinfo = (ptroppackinfo) refcon;
	handlestream *s;
	
	if (!outtextvisit (h, packinfo))
		return (false);
	
	if ((**h).hrefcon != nil) {
		
		assert (op_get_outlinedata() != nil);
		
		if (op_get_outlinedata() == nil)
			return (false);
		
		s = (*packinfo).packstream;
		
		closehandlestream (s);
		
		if (!(*(**op_get_outlinedata()).textualizerefconcallback) (h, (*s).data))
			return (false);
		
		openhandlestream ((*s).data, s);
		
		(*s).pos = (*s).eof;
		}
	
	return (true);
	} /*outscrapvisit*/


boolean opoutlinetotextstream (hdloutlinerecord houtline, boolean flomitcomments, handlestream *s) {
	
	/*
	convert the outline into a block of tab-indented text, each
	line ended by a carriage return.  suitable for passing through 
	the clipboard to a text-based application.
	*/
	
	tyoppackinfo packinfo;
	boolean fl;
	
	outscraplevel = 0;
	
	packinfo.packstream = s;
	
	packinfo.flpackcomments = !flomitcomments;
	
	oppushoutline (houtline);
	
	opwriteeditbuffer (); /*if a headline is being edited, update text handle*/
	
	fl = opsiblingvisiter ((**houtline).hsummit, false, &outscrapvisit, &packinfo);
	
	oppopoutline ();
	
	return (fl);
	} /*opoutlinetotextstream*/


boolean opoutlinetotextscrap (hdloutlinerecord houtline, boolean flomitcomments, Handle htext) {
	
	/*
	stick the handle into a handlestream and pass it on through...
	*/
	
	handlestream packstream;
	boolean fl;
	
	openhandlestream (htext, &packstream);
	
	fl = opoutlinetotextstream (houtline, flomitcomments, &packstream);
	
	closehandlestream (&packstream);
	
	return (fl);
	} /*opoutlinetotextscrap*/


boolean opoutlinetonewtextscrap (hdloutlinerecord houtline, Handle *htext) {
	
	/*
	similar to opoutlinetotextscrap above, but allocated new handle
	*/
	
	if (!newgrowinghandle (0, htext))
		return (false);
	
	if (opoutlinetotextscrap (houtline, false, *htext))
		return (true);
	
	disposehandle (*htext);
	
	*htext = nil;
	
	return (false);
	} /*opoutlinetonewtextscrap*/


/*
boolean opsuboutlinetonewtextscrap (hdlheadrecord hnode, Handle *htext) {
	
	/%
	similar to opoutlinetotextscrap above, but allocated new handle and 
	only visits hnode and its subheads
	%/
	
	register hdlheadrecord h = hnode;
	
	if (!newemptyhandle (htext))
		return (false);
	
	outscraplevel = 0;
	
	packhandle = *htext;
	
	if (outscrapvisit (h) && oprecursivelyvisit (h, infinity, &outscrapvisit))
		return  (true);
	
	disposehandle (*htext);
	
	return (false);
	} /%opsuboutlinetonewtextscrap%/
*/
