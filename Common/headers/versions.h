
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-present Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/*
	versions.h
	
	Defines version numbers for Frontier and Radio UserLand for all platforms.
	
	JES 12/04/2002
	
	2006-02-07 aradke: major overhaul
		about.c and shell.r don't contain any target-specific version info anymore.
		there is some target-specific info in the version resource in winland.rc left,
		but nothing that needs to be changed on a regular basic.
		to do: convert pascal to c strings? (see about.c)
*/ 


/* common strings for all targets */

#define	APP_COPYRIGHT_FROM	"2004"
#define	APP_COPYRIGHT_TILL	"2018"

/* 2011-08-03 tedchoward: these properties are identical for each app */
#define	APP_MAJOR_VERSION			10
#define	APP_MAJOR_VERSION_BCD		0x10	/* major version in BCD notation */

#define	APP_SUB_VERSION             2
#define	APP_MINOR_VERSION			0
#define	APP_SUBMINOR_VERSION_BCD	0x20	/* sub and minor version in BCD notation */

#define	APP_STAGE_CODE				0x20	/* dev = 0x20, alpha = 0x40, beta = 0x60, final = 0x80 */
#define	APP_REVISION_LEVEL			4      /* for non-final releases only */
#define	APP_BUILD_NUMBER			4      /* increment by one for every release, final or not */

#define	APP_VERSION_STRING			"10.2d4"



/* target-specific strings and version info */

#ifdef PIKE
#ifndef OPMLEDITOR

	/* version info for RADIO targets (formerly known as PIKE) */
	
	#define	APPNAME					"Radio"
	#define	APP_COPYRIGHT_HOLDER	"UserLand Software, Inc"
	
	#define	bs_APP_NAME				BIGSTRING ("\x05" "Radio")
	#define	bs_APP_SLOGAN			BIGSTRING ("\x2b" "The power of Web publishing on your desktop")
	#define	bs_APP_COPYRIGHT		BIGSTRING ("\x23" "© " APP_COPYRIGHT_FROM "-" APP_COPYRIGHT_TILL " UserLand Software, Inc.")
	#define	bs_APP_URL				BIGSTRING ("\x26" "http://frontierkernel.sourceforge.net/")

#else

	/* version info for OPMLEDITOR targets */
	
	#define	APPNAME					"OPML"
	
	#define	APP_COPYRIGHT_HOLDER	"Scripting News, Inc"
	
	#define	bs_APP_NAME				BIGSTRING ("\x04" "OPML")
	#define	bs_APP_SLOGAN			BIGSTRING ("\x25" "Powerful OPML editing on your desktop")
	#define	bs_APP_COPYRIGHT		BIGSTRING ("\x20" "© " APP_COPYRIGHT_FROM "-" APP_COPYRIGHT_TILL " Scripting News, Inc.")
	#define	bs_APP_URL				BIGSTRING ("\x18" "http://support.opml.org/")
		
#endif
#else

	/* version info for FRONTIER targets */
	
	#define	APPNAME					"Frontier"
	#define	APP_COPYRIGHT_HOLDER	"Frontier Kernel Project"
	
	#define	bs_APP_NAME				BIGSTRING ("\x08" "Frontier")
	#define	bs_APP_SLOGAN			BIGSTRING ("\x25" "Powerful cross-platform web scripting")
	#define	bs_APP_COPYRIGHT		BIGSTRING ("\x23" "© " APP_COPYRIGHT_FROM "-" APP_COPYRIGHT_TILL " Frontier Kernel Project")
	#define	bs_APP_URL				BIGSTRING ("\x26" "http://frontierkernel.sourceforge.net/")
		
#endif

#define	bs_APP_COPYRIGHT2	BIGSTRING ("\x22" "© 1992-2004 UserLand Software, Inc")
#define	APP_COPYRIGHT		APP_COPYRIGHT_FROM "-" APP_COPYRIGHT_TILL " " APP_COPYRIGHT_HOLDER

#define	APPNAME_SHORT		APPNAME	/* 2006-02-04 aradke */
#define	APPNAME_TM          APPNAME	/* 2005-01-12 aradke: app names no longer include trademark character */

