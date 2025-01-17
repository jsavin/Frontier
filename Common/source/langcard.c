
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

#include <uisharing.h>
#include <uisinternal.h>

#include "launch.h"
#include "memory.h"
#include "ops.h"
#include "strings.h"
#include "frontierwindows.h"
#include "shell.h"
#include "scrap.h"
#include "shellprivate.h"
#include "shellhooks.h"
#include "shellmenu.h"
#include "lang.h"
#include "langinternal.h"
#include "langipc.h"
#include "process.h"





boolean langruncard (hdltreenode hparam1, boolean flmodal, tyvaluerecord *vreturned) {
	
	return (false);
	} /*langruncard*/

boolean langismodalcard (hdltreenode hparam1, tyvaluerecord *vreturned) {
	
	return (false);
	} /*langismodalcard*/





