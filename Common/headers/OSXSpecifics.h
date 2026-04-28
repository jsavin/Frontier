
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

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

/* OSXSpecifics.h
	
	This file and its header represent a one-stop shop for setting up OSX specific things.
	The goal is to only need to call the setup procedure once, and have the function pointers
	available for the rest of the time Frontier is running. I'll hone this down over time,
	but for right now, this is good enough.
	
*/

// BSD function prototypes
/*
int	execv( const char *path, char *const argv[] );
*/
typedef int (*execvFuncPtr)( const char*, char **const );


FILE *	 popen(const char *command, const char *type);
typedef FILE *(*BSDpopenFuncPtr)( const char*, const char* );

int	pclose( FILE *stream );
typedef int (*BSDpcloseFuncPtr)( FILE* );

typedef int (*BSDfreadFuncPtr)( void *, size_t, size_t, FILE * );
typedef UInt32 (*QuartzTextPtr)( UInt32 );

void useQDText(int i);


void	InvokeTool( char *toolName );

