
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

#include "frontier.h"
#include "standard.h"

#include "dialogs.h"
#include "error.h"
#include "strings.h"
#include "opinternal.h"
#include "claybrowserstruc.h"
#include "claybrowservalidate.h"
#include "langinternal.h" /* 2005-09-26 creedon */
#include "tablestructure.h" /* 2005-09-26 creedon */

#define collidewithequal 0x0001
#define collidewitholder 0x0002
#define collidewithnewer 0x0004

#define nocollisions 0

#define bilateralcollision (collidewitholder + collidewithnewer)

static long dragmodified;

//static char *pastefname;

typedef enum tyaction {
	
	validatemove,
	
	validatepaste
	} tyaction;


byte * actionstrings [] = {
	
	BIGSTRING ("\x06" "moving"),
	BIGSTRING ("\x07" "pasting")
	};


enum {
	ixsomeitems,
	ixan,
	ixanewer,
	ixanolder,
	ixitemnamed,
	ixalreadyexists
	};


byte * dialogstrings [] = {
	BIGSTRING ("\x40" "Some items in this location have the same names as items you're "),
	BIGSTRING ("\x02" "An"),
	BIGSTRING ("\x07" "A newer"),
	BIGSTRING ("\x08" "An older"),
