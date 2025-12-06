
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
 * 2025-12-05: Legacy outline packer header for v2/v3 format (32-bit timestamps).
 * These functions handle READING old v6 database outline/script payloads.
 * Modern v7 databases use oppack_modern.c with v4 portable header.
 * See planning/phase3/carbon_migration/outline_script_payload.md
 */

#ifndef oppacklegacyinclude
#define oppacklegacyinclude

#include "op.h"

/*
 * Legacy pack/unpack functions for v2/v3 outline format
 * Used by dispatch layer in opverbs.c to handle old v6 database payloads
 */

extern boolean oppack_legacy (Handle *hpackedoutline);

extern boolean oppackoutline_legacy (hdloutlinerecord houtline, Handle *hpackedoutline);

extern boolean opunpack_legacy (Handle hpackedoutline, long *ixload, hdloutlinerecord *houtline);

extern boolean opunpackoutline_legacy (Handle hpackedoutline, hdloutlinerecord *houtline);

#endif /* oppacklegacyinclude */
