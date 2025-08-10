/*	$Id$    */

/*
 * Frontier - A web browser for the Mac
 * Copyright 1994-2002, UserLand Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// Phase 0.4: Compiler Compatibility
// Include system headers first, then compatibility layer, then Frontier headers

// 1. System Objective-C frameworks (include first to avoid conflicts)
#import <Foundation/Foundation.h>
#import <AppKit/NSApplication.h>

// 2. System C frameworks (include before compatibility layer)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>

// 3. Compatibility layer (defines missing types only)
#include "frontier_compat.h"

// 4. Frontier headers (after all system types are defined)
#include "frontier.h"
#include "standard.h"

#include "threads.h"
#include "shell.h"
#include "shellprivate.h"
#include "frontierstart.h"

int main (int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplicationLoad();
        
        boolean fl;
        
        if (!shellinit ())
            return (1);
        
        grabthreadglobals ();
        
        fl = frontierstart ();
        
        releasethreadglobals ();
        
        if (fl)
            shellmaineventloop ();
    }
	
	return (0);
	} /*mainstart*/
