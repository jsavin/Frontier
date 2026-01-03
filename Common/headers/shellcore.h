
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

#ifndef shellcoreinclude
#define shellcoreinclude

/*
shellcore.h - Core shell types safe for both GUI and headless builds

This header contains shell-level types and definitions that are used by both
Mac GUI builds and headless builds. These types are part of the core runtime
and don't depend on Mac-specific GUI functionality.

For Mac GUI-specific shell functions and types, see shellprivate.h.

Extracted as part of ADR-005 (thread-local parameter state migration) to
establish clean separation between portable runtime core and GUI layer,
supporting the collaborative ODB foundation.
*/

#define ctglobals 32 /*we can remember globals up to ctglobals levels deep*/

#pragma pack(2)
typedef struct tyglobalsstack {

	short top;

	WindowPtr stack [ctglobals]; /* WindowPtr = void* in headless builds */
	} tyglobalsstack;
#pragma options align=reset

#endif /* shellcoreinclude */
